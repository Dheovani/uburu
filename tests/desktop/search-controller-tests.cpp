#include "search-controller.hpp"

#include "fixtures/test-fixtures.hpp"
#include "helpers/temporary-paths.hpp"

#include <QCoreApplication>
#include <QDesktopServices>
#include <QDir>
#include <QEventLoop>
#include <QObject>
#include <QSettings>
#include <QString>
#include <QUrl>
#include <QVariantMap>

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <atomic>
#include <chrono>
#include <filesystem>
#include <memory>
#include <string>
#include <thread>

namespace
{

  QCoreApplication& testApplication()
  {
    static int argc = 1;
    static std::array<char, 12> executableName{"uburu-tests"};
    static std::array<char*, 2> argv{executableName.data(), nullptr};
    static QCoreApplication application(argc, argv.data());

    application.setOrganizationName(QStringLiteral("UburuTests"));

    return application;
  }

  QString qtPath(const std::filesystem::path& path)
  {
    return QDir::fromNativeSeparators(QString::fromStdString(path.generic_string()));
  }

  void isolateSettings(const std::filesystem::path& path, QString applicationName)
  {
    auto& application = testApplication();
    application.setApplicationName(std::move(applicationName));
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, qtPath(path));
  }

  QVariantMap firstMap(const QVariantList& values)
  {
    REQUIRE(values.size() == 1);

    return values.front().toMap();
  }

  template <typename Predicate>
  bool waitUntil(Predicate predicate, std::chrono::milliseconds timeout = std::chrono::seconds(2))
  {
    const auto deadline = std::chrono::steady_clock::now() + timeout;

    while (std::chrono::steady_clock::now() < deadline) {
      testApplication().processEvents(QEventLoop::AllEvents, 10);

      if (predicate())
        return true;

      std::this_thread::sleep_for(std::chrono::milliseconds{1});
    }

    testApplication().processEvents(QEventLoop::AllEvents, 10);

    return predicate();
  }

  class BlockingSearchService final : public uburu::app::SearchService
  {
  public:
    [[nodiscard]]
    uburu::search::SearchSummary
    search(const uburu::SearchQuery&, uburu::search::ResultSink, std::stop_token stopToken = {}) const override
    {
      started = true;

      while (!stopToken.stop_requested())
        std::this_thread::sleep_for(std::chrono::milliseconds{1});

      observedStop = true;

      return uburu::search::SearchSummary{.cancelled = true};
    }

    [[nodiscard]]
    uburu::search::SearchSummary searchWithEvents(
      const uburu::SearchQuery&,
      const uburu::app::SearchEventSink&,
      uburu::app::SearchExecutionOptions = {},
      std::stop_token = {}) const override
    {
      return {};
    }

    mutable std::atomic_bool started{false};
    mutable std::atomic_bool observedStop{false};
  };

} // namespace

class UrlCapture final : public QObject
{
  Q_OBJECT

public:
  QUrl openedUrl;

public slots:
  void open(const QUrl& url)
  {
    openedUrl = url;
  }
};

TEST_CASE("search controller exposes selected scope and favorite state")
{
  uburu::tests::TemporaryDirectory settingsDirectory("uburu-controller-settings-test");
  uburu::tests::TemporaryDirectory scopeDirectory("uburu-controller-scope-test");
  isolateSettings(settingsDirectory.path(), QStringLiteral("selected-scope-test"));

  uburu::app::SearchController controller;
  const auto scopePath = qtPath(scopeDirectory.path());

  controller.selectDirectory(scopePath);
  CHECK(controller.directory() == scopePath);
  CHECK(controller.selectedDirectories() == QStringList{scopePath});
  CHECK(controller.recentDirectories().contains(scopePath));
  CHECK_FALSE(controller.currentDirectoryFavorite());

  controller.toggleCurrentDirectoryFavorite();
  CHECK(controller.currentDirectoryFavorite());
  CHECK(controller.favoriteDirectories().contains(scopePath));

  controller.toggleCurrentDirectoryFavorite();
  CHECK_FALSE(controller.currentDirectoryFavorite());
}

TEST_CASE("search controller replaces the active scope instead of accumulating roots")
{
  uburu::tests::TemporaryDirectory settingsDirectory("uburu-controller-replace-scope-settings-test");
  uburu::tests::TemporaryDirectory firstDirectory("uburu-controller-replace-scope-first-test");
  uburu::tests::TemporaryDirectory secondDirectory("uburu-controller-replace-scope-second-test");
  isolateSettings(settingsDirectory.path(), QStringLiteral("replace-scope-test"));

  const auto firstPath = qtPath(firstDirectory.path());
  const auto secondPath = qtPath(secondDirectory.path());

  uburu::app::SearchController controller;
  controller.selectDirectory(firstPath);
  controller.selectDirectory(secondPath);

  CHECK(controller.directory() == secondPath);
  CHECK(controller.selectedDirectories() == QStringList{secondPath});
  CHECK(controller.recentDirectories().contains(firstPath));
  CHECK(controller.recentDirectories().contains(secondPath));
}

TEST_CASE("search controller migrates saved multi-root scopes to the latest selection")
{
  uburu::tests::TemporaryDirectory settingsDirectory("uburu-controller-migrate-scope-settings-test");
  uburu::tests::TemporaryDirectory firstDirectory("uburu-controller-migrate-scope-first-test");
  uburu::tests::TemporaryDirectory secondDirectory("uburu-controller-migrate-scope-second-test");
  isolateSettings(settingsDirectory.path(), QStringLiteral("migrate-scope-test"));

  const auto firstPath = qtPath(firstDirectory.path());
  const auto secondPath = qtPath(secondDirectory.path());

  QSettings settings;
  settings.beginGroup(QStringLiteral("scope"));
  settings.setValue(QStringLiteral("selectedDirectories"), QStringList{firstPath, secondPath});
  settings.endGroup();

  uburu::app::SearchController controller;

  CHECK(controller.directory() == secondPath);
  CHECK(controller.selectedDirectories() == QStringList{secondPath});
}

TEST_CASE("desktop search flow does not scan previously selected scopes")
{
  uburu::tests::TemporaryDirectory settingsDirectory("uburu-controller-no-scope-leak-settings-test");
  uburu::tests::TemporaryDirectory firstDirectory("uburu-controller-no-scope-leak-first-test");
  uburu::tests::TemporaryDirectory secondDirectory("uburu-controller-no-scope-leak-second-test");
  isolateSettings(settingsDirectory.path(), QStringLiteral("no-scope-leak-test"));

  uburu::tests::writeFile(firstDirectory.path() / "outside.qml", "verdade fora do escopo\n");
  uburu::tests::writeFile(secondDirectory.path() / "inside.qml", "texto sem correspondencia\n");

  uburu::app::SearchController controller;
  controller.selectDirectory(qtPath(firstDirectory.path()));
  controller.selectDirectory(qtPath(secondDirectory.path()));
  controller.startSearch(
    QStringLiteral("verdade"), false, false, false, true, false, false, true, QStringLiteral("qml"));

  REQUIRE(waitUntil([&] { return !controller.running(); }));

  CHECK(controller.results()->rowCount() == 0);
  CHECK(controller.matchesFound() == 0);
}

TEST_CASE("search controller exposes included and excluded scope entries")
{
  uburu::tests::TemporaryDirectory settingsDirectory("uburu-controller-scope-settings-test");
  uburu::tests::TemporaryDirectory scopeDirectory("uburu-controller-scope-entries-test");
  isolateSettings(settingsDirectory.path(), QStringLiteral("scope-entries-test"));

  const auto includePath = scopeDirectory.path() / "include";
  const auto excludePath = scopeDirectory.path() / "exclude";
  std::filesystem::create_directories(includePath);
  std::filesystem::create_directories(excludePath);

  uburu::app::SearchController controller;
  const auto scopePath = qtPath(scopeDirectory.path());
  controller.selectDirectory(scopePath);
  controller.addIncludedDirectory(qtPath(includePath));
  controller.addExcludedDirectory(qtPath(excludePath));

  const auto included = firstMap(controller.includedDirectories());
  CHECK(included.value(QStringLiteral("scopeRoot")).toString() == scopePath);
  CHECK(included.value(QStringLiteral("relativePath")).toString() == QStringLiteral("include"));
  CHECK(included.value(QStringLiteral("absolutePath")).toString() == qtPath(includePath));

  const auto excluded = firstMap(controller.excludedDirectories());
  CHECK(excluded.value(QStringLiteral("scopeRoot")).toString() == scopePath);
  CHECK(excluded.value(QStringLiteral("relativePath")).toString() == QStringLiteral("exclude"));
  CHECK(excluded.value(QStringLiteral("absolutePath")).toString() == qtPath(excludePath));
}

TEST_CASE("desktop search flow selects a folder and finds a result")
{
  uburu::tests::TemporaryDirectory settingsDirectory("uburu-controller-e2e-search-settings-test");
  uburu::tests::TemporaryDirectory scopeDirectory("uburu-controller-e2e-search-test");
  isolateSettings(settingsDirectory.path(), QStringLiteral("e2e-search-test"));

  const auto filePath = scopeDirectory.path() / "notes" / "sample.txt";
  uburu::tests::writeFile(filePath, "first line\nneedle in the selected folder\n");

  uburu::app::SearchController controller;
  controller.selectDirectory(qtPath(scopeDirectory.path()));
  controller.startSearch(
    QStringLiteral("needle"), false, false, false, true, false, false, true, QStringLiteral("txt"));

  REQUIRE(waitUntil([&] { return !controller.running() && controller.results()->rowCount() == 1; }));

  const auto index = controller.results()->index(0, 0);
  REQUIRE(index.isValid());
  CHECK(controller.results()
          ->data(index, uburu::app::SearchResultModel::PathRole)
          .toString()
          .endsWith(QStringLiteral("sample.txt")));
  CHECK(controller.matchesFound() == 1);
}

TEST_CASE("desktop search flow can cancel a running search")
{
  uburu::tests::TemporaryDirectory settingsDirectory("uburu-controller-e2e-cancel-settings-test");
  uburu::tests::TemporaryDirectory scopeDirectory("uburu-controller-e2e-cancel-test");
  isolateSettings(settingsDirectory.path(), QStringLiteral("e2e-cancel-test"));

  auto service = std::make_shared<BlockingSearchService>();
  uburu::app::SearchController controller(service);
  controller.selectDirectory(qtPath(scopeDirectory.path()));
  controller.startSearch(QStringLiteral("needle"), false, false, false, true, false, false, true, QString{});

  REQUIRE(waitUntil([&] { return service->started.load() && controller.running(); }));

  controller.cancel();

  REQUIRE(waitUntil([&] { return !controller.running() && service->observedStop.load(); }));
  CHECK_FALSE(controller.cancelling());
}

TEST_CASE("desktop result action opens a selected file URL")
{
  uburu::tests::TemporaryDirectory settingsDirectory("uburu-controller-e2e-open-settings-test");
  uburu::tests::TemporaryFile file("uburu-controller-e2e-open-result.txt");
  isolateSettings(settingsDirectory.path(), QStringLiteral("e2e-open-test"));
  uburu::tests::writeFile(file.path(), "needle\n");

  UrlCapture capture;
  QDesktopServices::setUrlHandler(QStringLiteral("file"), &capture, "open");

  uburu::app::SearchController controller;
  controller.openFile(qtPath(file.path()));

  QDesktopServices::unsetUrlHandler(QStringLiteral("file"));

  CHECK(capture.openedUrl == QUrl::fromLocalFile(qtPath(file.path())));
}

TEST_CASE("desktop preview uses visible text for html files")
{
  uburu::tests::TemporaryDirectory settingsDirectory("uburu-controller-html-preview-settings-test");
  uburu::tests::TemporaryDirectory previewDirectory("uburu-controller-html-preview-test");
  const auto filePath = previewDirectory.path() / "preview.html";

  isolateSettings(settingsDirectory.path(), QStringLiteral("html-preview-test"));
  uburu::tests::writeFile(filePath, "<body><h1>Visible needle</h1><script>hiddenNeedle()</script></body>");

  uburu::app::SearchController controller;
  controller.loadPreview(qtPath(filePath), QStringLiteral("1:9"), {}, {});

  REQUIRE(waitUntil([&] { return !controller.previewLoading(); }));

  CHECK(controller.previewText().contains(QStringLiteral("Visible needle")));
  CHECK_FALSE(controller.previewText().contains(QStringLiteral("hiddenNeedle")));
  CHECK_FALSE(controller.previewText().contains(QStringLiteral("<script>")));
}

TEST_CASE("desktop preview uses cue text for subtitle files")
{
  uburu::tests::TemporaryDirectory settingsDirectory("uburu-controller-subtitle-preview-settings-test");
  uburu::tests::TemporaryDirectory previewDirectory("uburu-controller-subtitle-preview-test");
  const auto filePath = previewDirectory.path() / "preview.vtt";

  isolateSettings(settingsDirectory.path(), QStringLiteral("subtitle-preview-test"));
  uburu::tests::writeFile(filePath, "WEBVTT\n\n00:01.000 --> 00:02.000\nVisible subtitle needle\n");

  uburu::app::SearchController controller;
  controller.loadPreview(qtPath(filePath), QStringLiteral("1:18"), {}, {});

  REQUIRE(waitUntil([&] { return !controller.previewLoading(); }));

  CHECK(controller.previewText().contains(QStringLiteral("Visible subtitle needle")));
  CHECK_FALSE(controller.previewText().contains(QStringLiteral("WEBVTT")));
  CHECK_FALSE(controller.previewText().contains(QStringLiteral("-->")));
}

TEST_CASE("desktop preview uses visible text for rtf files")
{
  uburu::tests::TemporaryDirectory settingsDirectory("uburu-controller-rtf-preview-settings-test");
  uburu::tests::TemporaryDirectory previewDirectory("uburu-controller-rtf-preview-test");
  const auto filePath = previewDirectory.path() / "preview.rtf";

  isolateSettings(settingsDirectory.path(), QStringLiteral("rtf-preview-test"));
  uburu::tests::writeFile(filePath, "{\\rtf1 Visible rtf needle {\\pict hiddenNeedle}}");

  uburu::app::SearchController controller;
  controller.loadPreview(qtPath(filePath), QStringLiteral("1:13"), {}, {});

  REQUIRE(waitUntil([&] { return !controller.previewLoading(); }));

  CHECK(controller.previewText().contains(QStringLiteral("Visible rtf needle")));
  CHECK_FALSE(controller.previewText().contains(QStringLiteral("hiddenNeedle")));
  CHECK_FALSE(controller.previewText().contains(QStringLiteral("\\rtf1")));
}

TEST_CASE("desktop preview uses visible text for docx files")
{
  uburu::tests::TemporaryDirectory settingsDirectory("uburu-controller-docx-preview-settings-test");
  uburu::tests::TemporaryDirectory previewDirectory("uburu-controller-docx-preview-test");
  const auto filePath = previewDirectory.path() / "preview.docx";

  isolateSettings(settingsDirectory.path(), QStringLiteral("docx-preview-test"));
  uburu::tests::writeBytes(
    filePath,
    uburu::tests::fixtures::minimalDocxBytes(
      "<w:document xmlns:w=\"http://schemas.openxmlformats.org/wordprocessingml/2006/main\">"
      "<w:body><w:p><w:r><w:t>Visible docx needle</w:t></w:r></w:p></w:body></w:document>"));

  uburu::app::SearchController controller;
  controller.loadPreview(qtPath(filePath), QStringLiteral("1:14"), {}, {});

  REQUIRE(waitUntil([&] { return !controller.previewLoading(); }));

  CHECK(controller.previewText().contains(QStringLiteral("Visible docx needle")));
  CHECK_FALSE(controller.previewText().contains(QStringLiteral("document.xml")));
  CHECK_FALSE(controller.previewText().contains(QStringLiteral("<w:t>")));
}

TEST_CASE("search result model exposes result roles")
{
  auto& application = testApplication();
  application.setApplicationName(QStringLiteral("result-model-test"));

  uburu::app::SearchResultModel model;
  uburu::SearchResult result;
  result.path = "src/main.cpp";
  result.searchRoot = "C:/project";
  result.line = 12;
  result.column = 5;
  result.lineText = "needle";

  model.append(std::move(result));

  const auto index = model.index(0, 0);
  REQUIRE(index.isValid());
  CHECK(model.rowCount() == 1);
  CHECK(model.data(index, uburu::app::SearchResultModel::PathRole).toString() == QStringLiteral("src/main.cpp"));
  CHECK(model.data(index, uburu::app::SearchResultModel::LocationRole).toString() == QStringLiteral("12:5"));
  CHECK(model.data(index, uburu::app::SearchResultModel::PreviewRole).toString() == QStringLiteral("needle"));
  CHECK(model.data(index, uburu::app::SearchResultModel::FileGroupHeaderRole).toBool());
  CHECK(model.data(index, uburu::app::SearchResultModel::FileGroupLabelRole).toString() ==
        QStringLiteral("C:/project/src/main.cpp"));
}

#include "search-controller-tests.moc"
