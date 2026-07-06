#include "search-controller.hpp"

#include "helpers/temporary-paths.hpp"

#include <QCoreApplication>
#include <QDir>
#include <QSettings>
#include <QString>
#include <QVariantMap>

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <filesystem>
#include <memory>
#include <string>

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

} // namespace

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
