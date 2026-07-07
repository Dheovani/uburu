#include "search-controller.hpp"

#include "app/services/indexing-service.hpp"
#include "core/filesystem/recursive-file-scanner.hpp"
#include "core/git/git-cli-git-service.hpp"
#include "core/index/persistent-index-service.hpp"
#include "core/search/direct-search-engine.hpp"
#include "core/search/search-errors.hpp"
#include "core/storage/sqlite-storage-service.hpp"
#include "core/text/text-file-reader.hpp"

#ifdef UBURU_HAS_LIBGIT2
#include "core/git/libgit2-git-service.hpp"
#endif

#include <QClipboard>
#include <QDesktopServices>
#include <QDir>
#include <QFileInfo>
#include <QFutureWatcher>
#include <QGuiApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMetaObject>
#include <QProcess>
#include <QRegularExpression>
#include <QSettings>
#include <QStandardPaths>
#include <QStringList>
#include <QUrl>
#include <QVariantMap>
#include <QtConcurrentRun>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <exception>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace uburu::app
{
  namespace
  {

    constexpr int maximumRecentDirectories = 8;
    constexpr std::int64_t missingFirstResultTime = -1;
    constexpr std::size_t previewContextBeforeLines = 80;
    constexpr std::size_t previewContextAfterLines = 160;
    constexpr std::size_t maximumPreviewLinesWithoutLocation = 240;
    constexpr std::size_t maximumPreviewBytes = 256U * 1024U;
    constexpr int previewLineNumberWidth = 6;
    constexpr auto previewHtmlTextColor = "#e8f0ff";
    constexpr auto previewHtmlLineNumberColor = "#7f91b5";
    constexpr auto previewHtmlSelectedLineBackground = "#13233f";
    constexpr auto previewHtmlHighlightBackground = "#2f66ff";
    constexpr auto previewHtmlHighlightColor = "#ffffff";
    constexpr auto settingsScopeGroup = "scope";
    constexpr auto selectedDirectoriesKey = "selectedDirectories";
    constexpr auto includedDirectoriesKey = "includedDirectories";
    constexpr auto excludedDirectoriesKey = "excludedDirectories";
    constexpr auto recentDirectoriesKey = "recentDirectories";
    constexpr auto favoriteDirectoriesKey = "favoriteDirectories";
    constexpr int completeProgressPercentage = 100;

    std::vector<std::string> parseDocumentTypes(const QString& text);

    std::filesystem::path nativePath(const QString& path)
    {
#ifdef _WIN32
      return std::filesystem::path(path.toStdWString());
#else
      return std::filesystem::path(path.toUtf8().constData());
#endif
    }

    std::string pathToUtf8(const std::filesystem::path& path)
    {
      const auto text = path.generic_u8string();

      return {reinterpret_cast<const char*>(text.data()), text.size()};
    }

    std::filesystem::path absoluteResultPath(const SearchResult& result)
    {
      if (result.path.is_absolute())
        return result.path;

      if (!result.searchRoot.empty())
        return result.searchRoot / result.path;

      return result.path;
    }

    QVariantList highlightRanges(const std::vector<MatchSpan>& highlights)
    {
      QVariantList ranges;

      for (const auto& highlight : highlights) {
        QVariantMap range;
        range.insert(QStringLiteral("byteOffset"), static_cast<qulonglong>(highlight.byteOffset));
        range.insert(QStringLiteral("byteLength"), static_cast<qulonglong>(highlight.byteLength));
        ranges.push_back(std::move(range));
      }

      return ranges;
    }

    std::vector<MatchSpan> highlightRangesFromVariantList(const QVariantList& ranges)
    {
      std::vector<MatchSpan> highlights;
      highlights.reserve(static_cast<std::size_t>(ranges.size()));

      for (const auto& rangeValue : ranges) {
        const auto range = rangeValue.toMap();
        const auto byteOffset = range.value(QStringLiteral("byteOffset")).toULongLong();
        const auto byteLength = range.value(QStringLiteral("byteLength")).toULongLong();

        if (byteLength == 0)
          continue;

        highlights.push_back(MatchSpan{.column = 0,
                                       .byteOffset = static_cast<std::size_t>(byteOffset),
                                       .byteLength = static_cast<std::size_t>(byteLength)});
      }

      std::ranges::sort(highlights, {}, &MatchSpan::byteOffset);

      return highlights;
    }

    QString localDirectoryFromUrlOrPath(const QString& value)
    {
      const auto url = QUrl(value);

      if (url.isLocalFile())
        return url.toLocalFile();

      return value;
    }

    QString canonicalDirectoryPath(const QString& value)
    {
      if (value.isEmpty())
        return {};

      const QFileInfo info(localDirectoryFromUrlOrPath(value));

      if (info.exists())
        return info.canonicalFilePath();

      return info.absoluteFilePath();
    }

    std::filesystem::path localDataPath()
    {
      const auto location = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
      const auto fallbackLocation = QDir::current().absoluteFilePath(QStringLiteral(".uburu"));
      const auto directory = location.isEmpty() ? fallbackLocation : location;

      QDir().mkpath(directory);

      return nativePath(QDir(directory).filePath(QStringLiteral("uburu.sqlite3")));
    }

    bool isSameOrInsideDirectory(const QString& root, const QString& path)
    {
      const auto relativePath = QDir(root).relativeFilePath(path);

      return relativePath == QStringLiteral(".") ||
             (!relativePath.startsWith(QStringLiteral("..")) && !QDir::isAbsolutePath(relativePath));
    }

    QString relativeDirectoryPath(const QString& root, const QString& path)
    {
      return QDir::fromNativeSeparators(QDir(root).relativeFilePath(path));
    }

    std::filesystem::path normalizedAbsolutePath(const std::filesystem::path& path)
    {
      std::error_code error;
      auto normalizedPath = std::filesystem::weakly_canonical(path, error);

      if (!error)
        return normalizedPath;

      normalizedPath = std::filesystem::absolute(path, error);

      if (!error)
        return normalizedPath.lexically_normal();

      return path.lexically_normal();
    }

    bool pathIsSameOrInside(const std::filesystem::path& root, const std::filesystem::path& path)
    {
      const auto normalizedRoot = normalizedAbsolutePath(root);
      const auto normalizedPath = normalizedAbsolutePath(path);
      std::error_code error;
      const auto relativePath = std::filesystem::relative(normalizedPath, normalizedRoot, error);

      if (error)
        return normalizedRoot == normalizedPath;

      if (relativePath.empty() || relativePath == ".")
        return true;

      const auto firstPart = *relativePath.begin();

      return firstPart != "..";
    }

    QStringList withoutDirectory(QStringList directories, const QString& directory)
    {
      directories.removeAll(directory);

      return directories;
    }

    std::shared_ptr<git::GitService> makeGitService()
    {
#ifdef UBURU_HAS_LIBGIT2
      return std::make_shared<git::Libgit2GitService>();
#else
      return std::make_shared<git::GitCliGitService>();
#endif
    }

    SearchOptions indexingOptions(bool respectGitignore,
                                  bool includeHidden,
                                  bool includeBinary,
                                  bool includeSubdirectories,
                                  const QString& documentTypes)
    {
      SearchOptions options;
      options.respectGitignore = respectGitignore;
      options.includeHidden = includeHidden;
      options.includeBinary = includeBinary;
      options.includeSubdirectories = includeSubdirectories;
      options.maximumFileSize = std::numeric_limits<std::uintmax_t>::max();
      options.extensions = parseDocumentTypes(documentTypes);

      return options;
    }

    std::optional<WorktreeInfo> worktreeForRoot(const git::GitService& gitService, const std::filesystem::path& root)
    {
      const auto repositoryResult = gitService.discoverRepository(root);
      const auto* repository = std::get_if<RepositoryInfo>(&repositoryResult);

      if (repository == nullptr)
        return std::nullopt;

      const auto worktreesResult = gitService.listWorktrees(*repository);
      const auto* worktrees = std::get_if<std::vector<WorktreeInfo>>(&worktreesResult);

      if (worktrees != nullptr) {
        auto selectedWorktree = std::optional<WorktreeInfo>{};

        for (const auto& worktree : *worktrees) {
          if (!pathIsSameOrInside(worktree.root, root))
            continue;

          if (!selectedWorktree || worktree.root.native().size() > selectedWorktree->root.native().size())
            selectedWorktree = worktree;
        }

        if (selectedWorktree)
          return selectedWorktree;
      }

      if (!repository->worktreeRoot)
        return std::nullopt;

      return WorktreeInfo{.id = pathToUtf8(*repository->worktreeRoot),
                          .repositoryId = repository->id,
                          .root = *repository->worktreeRoot,
                          .gitDirectory = repository->commonGitDirectory,
                          .branch = repository->currentBranch,
                          .headOid = repository->headOid};
    }

    WorktreeInfo filesystemWorktreeForRoot(const std::filesystem::path& root)
    {
      const auto normalizedRoot = normalizedAbsolutePath(root);
      const auto rootId = "filesystem:" + pathToUtf8(normalizedRoot);

      return WorktreeInfo{.id = rootId,
                          .repositoryId = rootId,
                          .root = normalizedRoot,
                          .gitDirectory = normalizedRoot / ".git",
                          .headOid = "filesystem"};
    }

    RepositoryInfo repositoryForWorktree(const WorktreeInfo& worktree)
    {
      return RepositoryInfo{.id = worktree.repositoryId,
                            .commonGitDirectory = worktree.gitDirectory,
                            .worktreeRoot = worktree.root,
                            .currentBranch = worktree.branch,
                            .headOid = worktree.headOid};
    }

    std::vector<FileEntry> scanFilesForIndex(const filesystem::FileScanner& scanner,
                                             const std::filesystem::path& root,
                                             const SearchOptions& options,
                                             std::stop_token stopToken)
    {
      std::vector<FileEntry> files;

      scanner.scan(
        root,
        options,
        [&](FileEntry file) {
          if (stopToken.stop_requested())
            return false;

          files.push_back(std::move(file));

          return true;
        },
        stopToken);

      return files;
    }

    void addIndexSummary(index::IndexUpdateSummary& target, const index::IndexUpdateSummary& source)
    {
      target.indexed += source.indexed;
      target.reusedByCatalog += source.reusedByCatalog;
      target.reusedByBlob += source.reusedByBlob;
      target.reusedByHash += source.reusedByHash;
      target.removed += source.removed;
      target.failed += source.failed;
      target.skippedUnsupportedFormat += source.skippedUnsupportedFormat;
      target.skippedBinary += source.skippedBinary;
      target.skippedBySize += source.skippedBySize;
      target.skippedByFilter += source.skippedByFilter;
      target.skippedTemporaryLimitation += source.skippedTemporaryLimitation;
      target.cancelled = target.cancelled || source.cancelled;
    }

    std::size_t skippedIndexingFiles(const index::IndexUpdateSummary& summary)
    {
      return summary.skippedUnsupportedFormat + summary.skippedBinary + summary.skippedBySize +
             summary.skippedByFilter + summary.skippedTemporaryLimitation;
    }

    QString longestContainingRoot(const QStringList& roots, const QString& path)
    {
      QString selectedRoot;

      for (const auto& root : roots) {
        if (!isSameOrInsideDirectory(root, path))
          continue;

        if (root.size() > selectedRoot.size())
          selectedRoot = root;
      }

      return selectedRoot;
    }

    QVariantMap scopeDirectoryEntry(const QString& root, const QString& relativePath)
    {
      QVariantMap entry;
      entry.insert(QStringLiteral("scopeRoot"), root);
      entry.insert(QStringLiteral("relativePath"), relativePath);
      entry.insert(QStringLiteral("absolutePath"), QDir(root).absoluteFilePath(relativePath));

      return entry;
    }

    QHash<QString, QStringList> parseScopedDirectories(const QString& text, const QString& entriesKey)
    {
      QHash<QString, QStringList> entriesByRoot;
      const auto document = QJsonDocument::fromJson(text.toUtf8());

      if (!document.isArray())
        return entriesByRoot;

      for (const auto& rootValue : document.array()) {
        if (!rootValue.isObject())
          continue;

        const auto rootObject = rootValue.toObject();
        const auto root = rootObject.value(QStringLiteral("root")).toString();
        const auto entries = rootObject.value(entriesKey).toArray();

        if (root.isEmpty())
          continue;

        QStringList paths;

        for (const auto& pathValue : entries) {
          const auto path = pathValue.toString();

          if (!path.isEmpty())
            paths.push_back(path);
        }

        if (!paths.empty())
          entriesByRoot.insert(root, paths);
      }

      return entriesByRoot;
    }

    QString serializedScopedDirectories(const QHash<QString, QStringList>& entriesByRoot, const QString& entriesKey)
    {
      QJsonArray roots;
      const auto rootKeys = entriesByRoot.keys();

      for (const auto& root : rootKeys) {
        QJsonArray entries;

        for (const auto& path : entriesByRoot.value(root))
          entries.push_back(path);

        if (entries.empty())
          continue;

        QJsonObject rootObject;
        rootObject.insert(QStringLiteral("root"), root);
        rootObject.insert(entriesKey, entries);
        roots.push_back(rootObject);
      }

      return QString::fromUtf8(QJsonDocument(roots).toJson(QJsonDocument::Compact));
    }

    std::vector<std::filesystem::path> nativePaths(const QStringList& paths)
    {
      std::vector<std::filesystem::path> nativeValues;
      nativeValues.reserve(static_cast<std::size_t>(paths.size()));

      for (const auto& path : paths)
        nativeValues.push_back(nativePath(path));

      return nativeValues;
    }

    search::SearchSummary failedSearchSummary(QString context)
    {
      search::SearchSummary summary;
      summary.partialFailure = true;
      summary.errors.push_back(
        search::makeSearchError(search::SearchErrorCode::fileReadFailed, context.toUtf8().toStdString()));

      return summary;
    }

    QString completedSearchStatus(const search::SearchSummary& summary, QObject* receiver)
    {
      auto status =
        receiver->tr("%n ocorrência(s) encontrada(s) em %1 arquivo(s)", nullptr, static_cast<int>(summary.matches))
          .arg(summary.filesScanned);

      QStringList skipped;

      if (summary.metrics.ignoredFiles > 0)
        skipped.push_back(receiver->tr("%1 ignorado(s)").arg(summary.metrics.ignoredFiles));

      if (summary.metrics.hiddenFiles > 0)
        skipped.push_back(receiver->tr("%1 oculto(s)").arg(summary.metrics.hiddenFiles));

      if (summary.metrics.binaryFilesSkipped > 0)
        skipped.push_back(receiver->tr("%1 binário(s)").arg(summary.metrics.binaryFilesSkipped));

      if (!skipped.empty())
        status += receiver->tr(" — ignorados: %1").arg(skipped.join(receiver->tr(", ")));

      return status;
    }

    QString firstSearchErrorContext(const search::SearchSummary& summary)
    {
      if (summary.errors.empty())
        return {};

      return QString::fromUtf8(summary.errors.front().context);
    }

    QString failedSearchStatus(const search::SearchSummary& summary, QObject* receiver)
    {
      const auto context = firstSearchErrorContext(summary);

      return context.isEmpty() ? receiver->tr("Erro ao pesquisar") : receiver->tr("Erro ao pesquisar: %1").arg(context);
    }

    QString partialSearchStatus(const search::SearchSummary& summary, QObject* receiver)
    {
      auto status = completedSearchStatus(summary, receiver);
      const auto errorCount = static_cast<int>(summary.errors.size());
      const auto context = firstSearchErrorContext(summary);

      if (errorCount == 0)
        return status;

      const auto warning = context.isEmpty()
                             ? receiver->tr("%n erro(s) parcial(is)", nullptr, errorCount)
                             : receiver->tr("%n erro(s) parcial(is), primeiro: %1", nullptr, errorCount).arg(context);

      return receiver->tr("%1 — avisos: %2").arg(status, warning);
    }

    QString formatDuration(std::chrono::nanoseconds duration)
    {
      if (duration.count() <= 0)
        return QStringLiteral("—");

      const auto milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();

      if (milliseconds < 1000)
        return QStringLiteral("%1 ms").arg(milliseconds);

      const auto seconds = static_cast<double>(milliseconds) / 1000.0;

      return QStringLiteral("%1 s").arg(QString::number(seconds, 'f', 2));
    }

    std::optional<std::size_t> lineNumberFromLocation(const QString& location)
    {
      const auto line = location.section(QLatin1Char(':'), 0, 0).toULongLong();

      if (line == 0)
        return std::nullopt;

      return static_cast<std::size_t>(line);
    }

    bool openWithApplicationChooser(const QString& filePath)
    {
#ifdef Q_OS_WIN
      return QProcess::startDetached(
        QStringLiteral("rundll32.exe"),
        QStringList{QStringLiteral("shell32.dll,OpenAs_RunDLL"), QDir::toNativeSeparators(filePath)});
#else
      Q_UNUSED(filePath);

      return false;
#endif
    }

    QString formattedPreviewLine(const text::TextLine& line, bool selected)
    {
      return QStringLiteral("%1 %2  %3")
        .arg(selected ? QLatin1Char('>') : QLatin1Char(' '))
        .arg(static_cast<qulonglong>(line.lineNumber), previewLineNumberWidth)
        .arg(QString::fromStdString(line.text));
    }

    QString highlightedHtml(std::string_view text, const std::vector<MatchSpan>& highlights)
    {
      QString html;
      std::size_t cursor = 0;

      for (const auto& highlight : highlights) {
        if (highlight.byteOffset < cursor || highlight.byteOffset >= text.size())
          continue;

        const auto highlightEnd = std::min(text.size(), highlight.byteOffset + highlight.byteLength);

        if (highlightEnd <= highlight.byteOffset)
          continue;

        html += QString::fromUtf8(text.data() + cursor, static_cast<qsizetype>(highlight.byteOffset - cursor))
                  .toHtmlEscaped();
        html += QStringLiteral("<span style=\"background:%1;color:%2;border-radius:3px;\">%3</span>")
                  .arg(QString::fromLatin1(previewHtmlHighlightBackground),
                       QString::fromLatin1(previewHtmlHighlightColor),
                       QString::fromUtf8(text.data() + highlight.byteOffset,
                                         static_cast<qsizetype>(highlightEnd - highlight.byteOffset))
                         .toHtmlEscaped());
        cursor = highlightEnd;
      }

      html += QString::fromUtf8(text.data() + cursor, static_cast<qsizetype>(text.size() - cursor)).toHtmlEscaped();

      return html;
    }

    QString
    formattedPreviewHtmlLine(const text::TextLine& line, bool selected, const std::vector<MatchSpan>& highlights)
    {
      const auto lineNumber = QString::number(static_cast<qulonglong>(line.lineNumber))
                                .rightJustified(previewLineNumberWidth, QLatin1Char(' '));
      const auto lineText =
        selected ? highlightedHtml(line.text, highlights) : QString::fromStdString(line.text).toHtmlEscaped();
      const auto lineStyle =
        selected
          ? QStringLiteral(" style=\"background:%1;\"").arg(QString::fromLatin1(previewHtmlSelectedLineBackground))
          : QString{};

      return QStringLiteral("<div%1><span style=\"color:%2;\">%3</span>  %4</div>")
        .arg(lineStyle, QString::fromLatin1(previewHtmlLineNumberColor), lineNumber.toHtmlEscaped(), lineText);
    }

    PreviewLoadStatus previewStatusFromTextStatus(text::TextReadStatus status)
    {
      switch (status) {
      case text::TextReadStatus::completed:
        return PreviewLoadStatus::completed;
      case text::TextReadStatus::cancelled:
        return PreviewLoadStatus::cancelled;
      case text::TextReadStatus::openFailed:
        return PreviewLoadStatus::openFailed;
      case text::TextReadStatus::readFailed:
        return PreviewLoadStatus::readFailed;
      case text::TextReadStatus::binarySkipped:
        return PreviewLoadStatus::binarySkipped;
      case text::TextReadStatus::invalidEncoding:
        return PreviewLoadStatus::invalidEncoding;
      case text::TextReadStatus::lineTooLong:
        return PreviewLoadStatus::lineTooLong;
      }

      return PreviewLoadStatus::readFailed;
    }

    PreviewLoadResult loadPreviewText(const QString& path,
                                      const QString& location,
                                      const QString& fallbackPreview,
                                      std::vector<MatchSpan> highlights,
                                      std::stop_token stopToken)
    {
      PreviewLoadResult result{.filePath = path, .location = location, .fallbackPreview = fallbackPreview};
      const auto targetLine = lineNumberFromLocation(location);
      const auto firstLine = targetLine.has_value() && *targetLine > previewContextBeforeLines
                               ? *targetLine - previewContextBeforeLines
                               : std::size_t{1};
      const auto lastLine =
        targetLine.has_value() ? *targetLine + previewContextAfterLines : maximumPreviewLinesWithoutLocation;
      SearchOptions options;
      QStringList lines;
      QStringList htmlLines;
      std::size_t previewBytes = 0;
      const std::vector<MatchSpan> emptyHighlights;

      options.includeBinary = false;
      options.maximumFileSize = std::numeric_limits<std::uintmax_t>::max();

      const auto summary = text::readTextFileLines(
        nativePath(path),
        options,
        [&](const text::TextLine& line) {
          if (stopToken.stop_requested())
            return false;

          if (line.lineNumber < firstLine)
            return true;

          if (line.lineNumber > lastLine)
            return false;

          const auto selectedLine = targetLine.has_value() && *targetLine == line.lineNumber;
          const auto& lineHighlights = selectedLine ? highlights : emptyHighlights;
          lines.push_back(formattedPreviewLine(line, selectedLine));
          htmlLines.push_back(formattedPreviewHtmlLine(line, selectedLine, lineHighlights));
          previewBytes += line.text.size();

          if (previewBytes >= maximumPreviewBytes) {
            result.truncated = true;
            return false;
          }

          return true;
        },
        stopToken);

      result.status = previewStatusFromTextStatus(summary.status);

      if (result.status == PreviewLoadStatus::cancelled)
        return result;

      result.text = lines.join(QStringLiteral("\n"));

      if (!htmlLines.empty()) {
        result.html =
          QStringLiteral(
            "<html><body style=\"white-space:pre;font-family:Consolas,monospace;color:%1;\">%2</body></html>")
            .arg(QString::fromLatin1(previewHtmlTextColor), htmlLines.join(QString{}));
      }

      if (result.text.isEmpty() && !fallbackPreview.isEmpty())
        result.text = fallbackPreview;

      return result;
    }

    std::vector<std::string> parseDocumentTypes(const QString& text)
    {
      std::vector<std::string> extensions;
      const auto parts = text.split(QRegularExpression(QStringLiteral("[,;\\s]+")), Qt::SkipEmptyParts);

      extensions.reserve(static_cast<std::size_t>(parts.size()));

      for (auto part : parts) {
        part = part.trimmed();

        while (part.startsWith(QLatin1Char('.')))
          part.remove(0, 1);

        if (!part.isEmpty())
          extensions.push_back(part.toUtf8().toStdString());
      }

      return extensions;
    }

    std::shared_ptr<const SearchService> createDefaultSearchService()
    {
      return std::make_shared<DefaultSearchService>(
        std::make_shared<search::DirectSearchEngine>(std::make_shared<filesystem::RecursiveFileScanner>()));
    }

  } // namespace

  SearchResultModel::SearchResultModel(QObject* parent) : QAbstractListModel(parent) {}

  int SearchResultModel::rowCount(const QModelIndex& parent) const
  {
    return parent.isValid() ? 0 : static_cast<int>(results.size());
  }

  QVariant SearchResultModel::data(const QModelIndex& index, int role) const
  {
    if (!index.isValid() || index.row() < 0 || static_cast<std::size_t>(index.row()) >= results.size())
      return {};

    const auto row = static_cast<std::size_t>(index.row());
    const auto& result = results[row];

    if (role == PathRole)
      return QString::fromUtf8(pathToUtf8(result.path));

    if (role == AbsolutePathRole)
      return QString::fromUtf8(pathToUtf8(absoluteResultPath(result)));

    if (role == LocationRole)
      return QStringLiteral("%1:%2").arg(result.line).arg(result.column);

    if (role == PreviewRole)
      return QString::fromStdString(result.lineText);

    if (role == HighlightsRole)
      return highlightRanges(result.highlights);

    if (role == FileGroupHeaderRole)
      return row == 0 || absoluteResultPath(results[row - 1]) != absoluteResultPath(result);

    if (role == FileGroupLabelRole)
      return QString::fromUtf8(pathToUtf8(absoluteResultPath(result)));

    return {};
  }

  QHash<int, QByteArray> SearchResultModel::roleNames() const
  {
    return {{PathRole, "filePath"},
            {AbsolutePathRole, "absolutePath"},
            {LocationRole, "location"},
            {PreviewRole, "preview"},
            {HighlightsRole, "highlights"},
            {FileGroupHeaderRole, "fileGroupHeader"},
            {FileGroupLabelRole, "fileGroupLabel"}};
  }

  void SearchResultModel::clear()
  {
    beginResetModel();
    results.clear();
    endResetModel();
  }

  void SearchResultModel::append(SearchResult result)
  {
    const auto row = static_cast<int>(results.size());
    beginInsertRows({}, row, row);
    results.push_back(std::move(result));
    endInsertRows();
  }

  SearchController::SearchController(QObject* parent) : SearchController(createDefaultSearchService(), parent) {}

  SearchController::SearchController(std::shared_ptr<const SearchService> searchService, QObject* parent)
    : QObject(parent), statusValue(tr("Pronto")), timeToFirstResultValue(QStringLiteral("—")),
      searchDurationValue(QStringLiteral("—")), indexingStatusValue(tr("Indexação inativa")), resultsModel(this),
      searchService(std::move(searchService))
  {
    if (!this->searchService)
      throw std::invalid_argument("SearchController requires a search service");

    loadScopeHistory();
  }

  SearchController::~SearchController()
  {
    cancel();
    previewStopSource.request_stop();

    if (activeWatcher != nullptr)
      activeWatcher->waitForFinished();

    indexingStopSource.request_stop();

    if (activeIndexingWatcher != nullptr)
      activeIndexingWatcher->waitForFinished();

    if (activePreviewWatcher != nullptr)
      activePreviewWatcher->waitForFinished();
  }

  QString SearchController::directory() const
  {
    return directoryValue;
  }

  QString SearchController::status() const
  {
    return statusValue;
  }

  bool SearchController::running() const
  {
    return runningValue;
  }

  bool SearchController::cancelling() const
  {
    return cancellingValue;
  }

  QAbstractItemModel* SearchController::results()
  {
    return &resultsModel;
  }

  QStringList SearchController::selectedDirectories() const
  {
    return selectedDirectoryValues;
  }

  QVariantList SearchController::includedDirectories() const
  {
    QVariantList entries;

    for (const auto& root : selectedDirectoryValues)
      for (const auto& relativePath : includedDirectoryValues.value(root))
        entries.push_back(scopeDirectoryEntry(root, relativePath));

    return entries;
  }

  QVariantList SearchController::excludedDirectories() const
  {
    QVariantList entries;

    for (const auto& root : selectedDirectoryValues)
      for (const auto& relativePath : excludedDirectoryValues.value(root))
        entries.push_back(scopeDirectoryEntry(root, relativePath));

    return entries;
  }

  QStringList SearchController::recentDirectories() const
  {
    return recentDirectoryValues;
  }

  QStringList SearchController::favoriteDirectories() const
  {
    return favoriteDirectoryValues;
  }

  bool SearchController::currentDirectoryFavorite() const
  {
    return !directoryValue.isEmpty() && favoriteDirectoryValues.contains(directoryValue);
  }

  qulonglong SearchController::filesScanned() const
  {
    return filesScannedValue;
  }

  qulonglong SearchController::matchesFound() const
  {
    return matchesFoundValue;
  }

  QString SearchController::timeToFirstResult() const
  {
    return timeToFirstResultValue;
  }

  QString SearchController::searchDuration() const
  {
    return searchDurationValue;
  }

  QString SearchController::indexingStatus() const
  {
    return indexingStatusValue;
  }

  bool SearchController::indexingRunning() const
  {
    return indexingRunningValue;
  }

  int SearchController::indexingProgress() const
  {
    return indexingProgressValue;
  }

  QString SearchController::previewFilePath() const
  {
    return previewFilePathValue;
  }

  QString SearchController::previewLocation() const
  {
    return previewLocationValue;
  }

  QString SearchController::previewText() const
  {
    return previewTextValue;
  }

  QString SearchController::previewHtml() const
  {
    return previewHtmlValue;
  }

  bool SearchController::previewLoading() const
  {
    return previewLoadingValue;
  }

  bool SearchController::regexAvailable() const
  {
#ifdef UBURU_HAS_PCRE2
    return true;
#else
    return false;
#endif
  }

  void SearchController::selectDirectory(const QString& url)
  {
    const auto directory = canonicalDirectoryPath(url);

    if (directory.isEmpty())
      return;

    setDirectory(directory);
    addSelectedDirectory(directory);
    addRecentDirectory(directory);
    saveScopeHistory();
  }

  void SearchController::selectSavedDirectory(const QString& path)
  {
    selectDirectory(path);
  }

  void SearchController::removeSelectedDirectory(const QString& path)
  {
    const auto directory = canonicalDirectoryPath(path);

    if (directory.isEmpty())
      return;

    selectedDirectoryValues.removeAll(directory);
    includedDirectoryValues.remove(directory);
    excludedDirectoryValues.remove(directory);

    if (directoryValue == directory)
      directoryValue = selectedDirectoryValues.empty() ? QString{} : selectedDirectoryValues.front();

    saveScopeHistory();
    emit directoryChanged();
    emit scopeHistoryChanged();
  }

  void SearchController::addIncludedDirectory(const QString& url)
  {
    const auto directory = canonicalDirectoryPath(url);

    if (directory.isEmpty())
      return;

    const auto root = longestContainingRoot(selectedDirectoryValues, directory);

    if (root.isEmpty()) {
      setStatus(tr("A pasta incluída precisa estar dentro de um escopo selecionado"));
      return;
    }

    const auto relativePath = relativeDirectoryPath(root, directory);

    if (relativePath == QStringLiteral(".")) {
      setStatus(tr("Selecione uma subpasta para incluir, não a raiz da busca"));
      return;
    }

    auto inclusions = includedDirectoryValues.value(root);
    inclusions.removeAll(relativePath);
    inclusions.push_back(relativePath);
    includedDirectoryValues.insert(root, inclusions);

    saveScopeHistory();
    emit scopeHistoryChanged();
  }

  void SearchController::removeIncludedDirectory(const QString& root, const QString& relativePath)
  {
    auto inclusions = includedDirectoryValues.value(root);
    inclusions.removeAll(relativePath);

    if (inclusions.empty())
      includedDirectoryValues.remove(root);
    else
      includedDirectoryValues.insert(root, inclusions);

    saveScopeHistory();
    emit scopeHistoryChanged();
  }

  void SearchController::addExcludedDirectory(const QString& url)
  {
    const auto directory = canonicalDirectoryPath(url);

    if (directory.isEmpty())
      return;

    const auto root = longestContainingRoot(selectedDirectoryValues, directory);

    if (root.isEmpty()) {
      setStatus(tr("A pasta ignorada precisa estar dentro de um escopo selecionado"));
      return;
    }

    const auto relativePath = relativeDirectoryPath(root, directory);

    if (relativePath == QStringLiteral(".")) {
      setStatus(tr("Selecione uma subpasta para ignorar, não a raiz da busca"));
      return;
    }

    auto exclusions = excludedDirectoryValues.value(root);
    exclusions.removeAll(relativePath);
    exclusions.push_back(relativePath);
    excludedDirectoryValues.insert(root, exclusions);

    saveScopeHistory();
    emit scopeHistoryChanged();
  }

  void SearchController::removeExcludedDirectory(const QString& root, const QString& relativePath)
  {
    auto exclusions = excludedDirectoryValues.value(root);
    exclusions.removeAll(relativePath);

    if (exclusions.empty())
      excludedDirectoryValues.remove(root);
    else
      excludedDirectoryValues.insert(root, exclusions);

    saveScopeHistory();
    emit scopeHistoryChanged();
  }

  void SearchController::toggleCurrentDirectoryFavorite()
  {
    if (directoryValue.isEmpty())
      return;

    toggleFavoriteDirectory(directoryValue);
  }

  void SearchController::toggleFavoriteDirectory(const QString& path)
  {
    const auto directory = canonicalDirectoryPath(path);

    if (directory.isEmpty())
      return;

    if (favoriteDirectoryValues.contains(directory)) {
      favoriteDirectoryValues.removeAll(directory);
    } else {
      favoriteDirectoryValues.push_front(directory);
    }

    saveScopeHistory();
    emit scopeHistoryChanged();
  }

  void SearchController::openFile(const QString& path)
  {
    if (path.isEmpty())
      return;

    const QFileInfo file(path);

    if (!file.exists()) {
      setStatus(tr("Arquivo não encontrado: %1").arg(path));
      return;
    }

    if (QDesktopServices::openUrl(QUrl::fromLocalFile(file.absoluteFilePath()))) {
      setStatus(tr("Arquivo aberto"));
      return;
    }

    setStatus(tr("Não foi possível abrir o arquivo"));
  }

  void SearchController::openWith(const QString& path)
  {
    if (path.isEmpty())
      return;

    const QFileInfo file(path);

    if (!file.exists()) {
      setStatus(tr("Arquivo não encontrado: %1").arg(path));
      return;
    }

    if (openWithApplicationChooser(file.absoluteFilePath())) {
      setStatus(tr("Seletor de aplicativo aberto"));
      return;
    }

    setStatus(tr("Não foi possível abrir o seletor de aplicativo"));
  }

  void SearchController::openContainingFolder(const QString& path)
  {
    if (path.isEmpty())
      return;

    const QFileInfo file(path);
    const auto directory = file.exists() ? file.absolutePath() : QFileInfo(path).absolutePath();

    if (directory.isEmpty()) {
      setStatus(tr("Local do arquivo não encontrado: %1").arg(path));
      return;
    }

    if (QDesktopServices::openUrl(QUrl::fromLocalFile(directory))) {
      setStatus(tr("Local do arquivo aberto"));
      return;
    }

    setStatus(tr("Não foi possível abrir o local do arquivo"));
  }

  void SearchController::copyToClipboard(const QString& text)
  {
    auto* clipboard = QGuiApplication::clipboard();

    if (clipboard == nullptr)
      return;

    clipboard->setText(text);
    setStatus(tr("Copiado para a área de transferência"));
  }

  void SearchController::loadPreview(const QString& path,
                                     const QString& location,
                                     const QString& fallbackPreview,
                                     const QVariantList& highlights)
  {
    if (path.isEmpty()) {
      clearPreview();
      return;
    }

    previewStopSource.request_stop();
    previewStopSource = std::stop_source{};

    if (activePreviewWatcher != nullptr) {
      activePreviewWatcher->deleteLater();
      activePreviewWatcher = nullptr;
    }

    previewFilePathValue = path;
    previewLocationValue = location;
    previewTextValue = fallbackPreview;
    previewHtmlValue.clear();
    emit previewChanged();
    setPreviewLoading(true);

    const auto token = previewStopSource.get_token();
    auto previewHighlights = highlightRangesFromVariantList(highlights);
    auto* watcher = new QFutureWatcher<PreviewLoadResult>(this);
    activePreviewWatcher = watcher;

    connect(watcher, &QFutureWatcher<PreviewLoadResult>::finished, this, [this, watcher] {
      if (watcher != activePreviewWatcher) {
        watcher->deleteLater();
        return;
      }

      const auto result = watcher->result();
      activePreviewWatcher->deleteLater();
      activePreviewWatcher = nullptr;
      setPreviewResult(result);
      setPreviewLoading(false);
    });
    watcher->setFuture(
      QtConcurrent::run([path, location, fallbackPreview, previewHighlights = std::move(previewHighlights), token] {
        return loadPreviewText(path, location, fallbackPreview, previewHighlights, token);
      }));
  }

  void SearchController::clearPreview()
  {
    previewStopSource.request_stop();

    if (activePreviewWatcher != nullptr) {
      activePreviewWatcher->deleteLater();
      activePreviewWatcher = nullptr;
    }

    previewFilePathValue.clear();
    previewLocationValue.clear();
    previewTextValue.clear();
    previewHtmlValue.clear();
    emit previewChanged();
    setPreviewLoading(false);
  }

  void SearchController::startSearch(const QString& expression,
                                     bool regex,
                                     bool caseSensitive,
                                     bool wholeWord,
                                     bool respectGitignore,
                                     bool includeHidden,
                                     bool includeBinary,
                                     bool includeSubdirectories,
                                     const QString& documentTypes)
  {
    if (runningValue || selectedDirectoryValues.empty() || expression.isEmpty())
      return;

    resultsModel.clear();
    clearPreview();
    resetSearchMetrics();
    stopSource = std::stop_source{};
    setCancelling(false);
    setRunning(true);
    setStatus(tr("Buscando..."));

    SearchQuery query{.root = nativePath(selectedDirectoryValues.front()),
                      .scope = {},
                      .expression = expression.toUtf8().toStdString(),
                      .options = {}};
    query.options.mode = regex ? SearchMode::regex : SearchMode::literal;
    query.options.caseSensitive = caseSensitive;
    query.options.wholeWord = wholeWord;
    query.options.respectGitignore = respectGitignore;
    query.options.includeHidden = includeHidden;
    query.options.includeBinary = includeBinary;
    query.options.includeSubdirectories = includeSubdirectories;
    query.options.target = SearchTarget::contentAndFileName;
    query.options.maximumFileSize = std::numeric_limits<std::uintmax_t>::max();
    query.options.extensions = parseDocumentTypes(documentTypes);

    query.scope.roots.reserve(static_cast<std::size_t>(selectedDirectoryValues.size()));

    for (const auto& selectedDirectory : selectedDirectoryValues) {
      const auto inclusions = includedDirectoryValues.value(selectedDirectory);
      const auto exclusions = excludedDirectoryValues.value(selectedDirectory);

      query.scope.roots.push_back(SearchRoot{.path = nativePath(selectedDirectory),
                                             .includedDirectories = nativePaths(inclusions),
                                             .excludedDirectories = nativePaths(exclusions)});
    }

    const auto token = stopSource.get_token();
    const auto startedAt = std::chrono::steady_clock::now();
    const auto firstResultNanoseconds = std::make_shared<std::atomic<std::int64_t>>(missingFirstResultTime);

    activeWatcher = new QFutureWatcher<search::SearchSummary>(this);
    connect(activeWatcher, &QFutureWatcher<search::SearchSummary>::finished, this, [this] {
      auto summary = activeWatcher->result();
      updateSearchMetrics(summary);

      if (summary.cancelled)
        setStatus(tr("Busca cancelada"));
      else if (summary.partialFailure)
        setStatus(partialSearchStatus(summary, this));
      else if (!summary.errors.empty())
        setStatus(failedSearchStatus(summary, this));
      else
        setStatus(completedSearchStatus(summary, this));

      setCancelling(false);
      setRunning(false);
      activeWatcher->deleteLater();
      activeWatcher = nullptr;
    });
    activeWatcher->setFuture(
      QtConcurrent::run([this, query = std::move(query), token, startedAt, firstResultNanoseconds] {
        try {
          auto summary = searchService->search(
            query,
            [this, startedAt, firstResultNanoseconds](SearchResult result) {
              auto expected = missingFirstResultTime;
              const auto elapsed = std::chrono::steady_clock::now() - startedAt;
              const auto elapsedNanoseconds = std::chrono::duration_cast<std::chrono::nanoseconds>(elapsed).count();
              firstResultNanoseconds->compare_exchange_strong(expected, elapsedNanoseconds);

              QMetaObject::invokeMethod(
                this,
                [this, result = std::move(result)]() mutable { resultsModel.append(std::move(result)); },
                Qt::QueuedConnection);
              return true;
            },
            token);

          const auto observedFirstResultNanoseconds = firstResultNanoseconds->load();

          if (observedFirstResultNanoseconds != missingFirstResultTime)
            summary.metrics.timeToFirstResult = std::chrono::nanoseconds(observedFirstResultNanoseconds);

          return summary;
        } catch (const std::exception& exception) {
          return failedSearchSummary(QString::fromLocal8Bit(exception.what()));
        } catch (...) {
          return failedSearchSummary(QStringLiteral("unknown exception"));
        }
      }));
  }

  void SearchController::cancel()
  {
    if (!runningValue || cancellingValue)
      return;

    setCancelling(true);
    setStatus(tr("Cancelando..."));
    stopSource.request_stop();
  }

  void SearchController::startIndexing(bool respectGitignore,
                                       bool includeHidden,
                                       bool includeBinary,
                                       bool includeSubdirectories,
                                       const QString& documentTypes)
  {
    if (indexingRunningValue)
      return;

    if (selectedDirectoryValues.empty()) {
      setIndexingProgress(tr("Selecione um escopo para indexar"), 0);
      return;
    }

    indexingStopSource = std::stop_source{};
    const auto token = indexingStopSource.get_token();
    const auto roots = selectedDirectoryValues;
    const auto databasePath = localDataPath();
    const auto options =
      indexingOptions(respectGitignore, includeHidden, includeBinary, includeSubdirectories, documentTypes);

    setIndexingProgress(tr("Preparando indexação..."), 0);
    setIndexingRunning(true);

    activeIndexingWatcher = new QFutureWatcher<index::IndexUpdateSummary>(this);
    connect(activeIndexingWatcher, &QFutureWatcher<index::IndexUpdateSummary>::finished, this, [this] {
      const auto summary = activeIndexingWatcher->result();

      if (summary.cancelled) {
        setIndexingProgress(tr("Indexação cancelada"), indexingProgressValue);
      } else {
        const auto reusedDocuments = summary.reusedByCatalog + summary.reusedByBlob + summary.reusedByHash;
        auto status =
          tr("Índice atualizado: %1 indexado(s), %2 reutilizado(s), %3 removido(s), %4 ignorado(s), %5 falha(s)");
        status = status.arg(summary.indexed)
          .arg(reusedDocuments)
          .arg(summary.removed)
          .arg(skippedIndexingFiles(summary))
          .arg(summary.failed);

        setIndexingProgress(status, completeProgressPercentage);
      }

      setIndexingRunning(false);
      activeIndexingWatcher->deleteLater();
      activeIndexingWatcher = nullptr;
    });

    activeIndexingWatcher->setFuture(QtConcurrent::run([this, roots, databasePath, options, token] {
      index::IndexUpdateSummary totalSummary;

      try {
        auto storageService = std::make_shared<storage::SQLiteStorageService>(databasePath);
        storageService->initialize();

        auto gitService = makeGitService();
        auto scanner = std::make_shared<filesystem::RecursiveFileScanner>();
        auto indexService = std::make_shared<index::PersistentIndexService>(*storageService);
        DefaultIndexingService indexingService(scanner, gitService, indexService);

        for (const auto& root : roots) {
          if (token.stop_requested()) {
            totalSummary.cancelled = true;
            break;
          }

          const auto nativeRoot = nativePath(root);
          auto worktree = worktreeForRoot(*gitService, nativeRoot);

          if (!worktree) {
            QMetaObject::invokeMethod(
              this,
              [this, root] { setIndexingProgress(tr("Indexando diretório sem Git: %1").arg(root), 0); },
              Qt::QueuedConnection);

            auto filesystemWorktree = filesystemWorktreeForRoot(nativeRoot);
            storageService->upsertRepository(repositoryForWorktree(filesystemWorktree));
            storageService->upsertWorktree(filesystemWorktree);

            auto files = scanFilesForIndex(*scanner, filesystemWorktree.root, options, token);

            if (token.stop_requested()) {
              totalSummary.cancelled = true;
              break;
            }

            auto summary = indexService->update(
              filesystemWorktree,
              files,
              [this](const index::IndexUpdateProgress& progress) {
                QMetaObject::invokeMethod(
                  this, [this, progress] { updateIndexingProgress(progress); }, Qt::QueuedConnection);
              },
              token);

            addIndexSummary(totalSummary, summary);
            continue;
          }

          QMetaObject::invokeMethod(
            this, [this, root] { setIndexingProgress(tr("Indexando %1").arg(root), 0); }, Qt::QueuedConnection);

          storageService->upsertRepository(repositoryForWorktree(*worktree));
          storageService->upsertWorktree(*worktree);

          auto summary = indexingService.requestManualReindex(
            *worktree,
            options,
            [this](const index::IndexUpdateProgress& progress) {
              QMetaObject::invokeMethod(
                this, [this, progress] { updateIndexingProgress(progress); }, Qt::QueuedConnection);
            },
            token);

          addIndexSummary(totalSummary, summary);
        }
      } catch (const std::exception&) {
        ++totalSummary.failed;
      } catch (...) {
        ++totalSummary.failed;
      }

      return totalSummary;
    }));
  }

  void SearchController::cancelIndexing()
  {
    if (!indexingRunningValue)
      return;

    setIndexingProgress(tr("Cancelando indexação..."), indexingProgressValue);
    indexingStopSource.request_stop();
  }

  void SearchController::loadScopeHistory()
  {
    QSettings settings;
    settings.beginGroup(QString::fromLatin1(settingsScopeGroup));
    selectedDirectoryValues = settings.value(QString::fromLatin1(selectedDirectoriesKey)).toStringList();
    includedDirectoryValues = parseScopedDirectories(
      settings.value(QString::fromLatin1(includedDirectoriesKey)).toString(), QStringLiteral("included"));
    excludedDirectoryValues = parseScopedDirectories(
      settings.value(QString::fromLatin1(excludedDirectoriesKey)).toString(), QStringLiteral("excluded"));
    recentDirectoryValues = settings.value(QString::fromLatin1(recentDirectoriesKey)).toStringList();
    favoriteDirectoryValues = settings.value(QString::fromLatin1(favoriteDirectoriesKey)).toStringList();
    settings.endGroup();

    selectedDirectoryValues.removeIf(
      [](const QString& directory) { return directory.isEmpty() || !QFileInfo::exists(directory); });

    if (!selectedDirectoryValues.empty()) {
      directoryValue = selectedDirectoryValues.front();
      return;
    }

    for (const auto& directory : recentDirectoryValues) {
      if (!directory.isEmpty() && QFileInfo::exists(directory)) {
        directoryValue = directory;
        selectedDirectoryValues = {directory};
        break;
      }
    }
  }

  void SearchController::saveScopeHistory() const
  {
    QSettings settings;
    settings.beginGroup(QString::fromLatin1(settingsScopeGroup));
    settings.setValue(QString::fromLatin1(selectedDirectoriesKey), selectedDirectoryValues);
    settings.setValue(QString::fromLatin1(includedDirectoriesKey),
                      serializedScopedDirectories(includedDirectoryValues, QStringLiteral("included")));
    settings.setValue(QString::fromLatin1(excludedDirectoriesKey),
                      serializedScopedDirectories(excludedDirectoryValues, QStringLiteral("excluded")));
    settings.setValue(QString::fromLatin1(recentDirectoriesKey), recentDirectoryValues);
    settings.setValue(QString::fromLatin1(favoriteDirectoriesKey), favoriteDirectoryValues);
    settings.endGroup();
  }

  void SearchController::setDirectory(QString directory)
  {
    if (directoryValue == directory)
      return;

    directoryValue = std::move(directory);
    emit directoryChanged();
    emit scopeHistoryChanged();
  }

  void SearchController::addSelectedDirectory(const QString& directory)
  {
    selectedDirectoryValues.removeAll(directory);
    selectedDirectoryValues.push_back(directory);
    emit scopeHistoryChanged();
  }

  void SearchController::addRecentDirectory(const QString& directory)
  {
    recentDirectoryValues = withoutDirectory(std::move(recentDirectoryValues), directory);
    recentDirectoryValues.push_front(directory);

    while (recentDirectoryValues.size() > maximumRecentDirectories)
      recentDirectoryValues.removeLast();

    emit scopeHistoryChanged();
  }

  void SearchController::setStatus(QString status)
  {
    statusValue = std::move(status);
    emit statusChanged();
  }

  void SearchController::setRunning(bool running)
  {
    if (runningValue == running)
      return;

    runningValue = running;
    emit runningChanged();
  }

  void SearchController::setCancelling(bool cancelling)
  {
    if (cancellingValue == cancelling)
      return;

    cancellingValue = cancelling;
    emit cancellingChanged();
  }

  void SearchController::setIndexingRunning(bool running)
  {
    if (indexingRunningValue == running)
      return;

    indexingRunningValue = running;
    emit indexingRunningChanged();
  }

  void SearchController::setIndexingProgress(QString status, int progress)
  {
    const auto normalizedProgress = std::clamp(progress, 0, completeProgressPercentage);

    if (indexingStatusValue == status && indexingProgressValue == normalizedProgress)
      return;

    indexingStatusValue = std::move(status);
    indexingProgressValue = normalizedProgress;
    emit indexingProgressChanged();
  }

  void SearchController::updateIndexingProgress(const index::IndexUpdateProgress& progress)
  {
    auto progressPercent = 0;

    if (progress.total > 0)
      progressPercent = static_cast<int>((progress.processed * completeProgressPercentage) / progress.total);

    const auto currentPath = QString::fromUtf8(pathToUtf8(progress.currentPath));
    auto status = tr("Indexando %1/%2").arg(progress.processed).arg(progress.total);

    if (!currentPath.isEmpty())
      status = tr("Indexando %1/%2: %3").arg(progress.processed).arg(progress.total).arg(currentPath);

    setIndexingProgress(status, progressPercent);
  }

  void SearchController::setPreviewLoading(bool loading)
  {
    if (previewLoadingValue == loading)
      return;

    previewLoadingValue = loading;
    emit previewLoadingChanged();
  }

  void SearchController::setPreviewResult(const PreviewLoadResult& result)
  {
    if (result.status == PreviewLoadStatus::cancelled)
      return;

    previewFilePathValue = result.filePath;
    previewLocationValue = result.location;
    previewTextValue = result.text;
    previewHtmlValue = result.html;

    if (result.truncated)
      previewTextValue += tr("\n\nâ€¦ prévia limitada para manter a interface responsiva.");

    if (result.status == PreviewLoadStatus::binarySkipped) {
      previewTextValue = tr("Arquivo binário: prévia de texto indisponível.");
      previewHtmlValue.clear();
    }

    if (result.status == PreviewLoadStatus::openFailed) {
      previewTextValue = tr("Não foi possível abrir o arquivo para pré-visualização.");
      previewHtmlValue.clear();
    }

    if (result.status == PreviewLoadStatus::readFailed) {
      previewTextValue = tr("Não foi possível ler o arquivo para pré-visualização.");
      previewHtmlValue.clear();
    }

    if (result.status == PreviewLoadStatus::invalidEncoding) {
      previewTextValue = tr("Codificação inválida: prévia indisponível.");
      previewHtmlValue.clear();
    }

    if (result.status == PreviewLoadStatus::lineTooLong) {
      previewTextValue = tr("Linha muito longa: prévia limitada.");
      previewHtmlValue.clear();
    }

    emit previewChanged();
  }

  void SearchController::resetSearchMetrics()
  {
    filesScannedValue = 0;
    matchesFoundValue = 0;
    timeToFirstResultValue = QStringLiteral("—");
    searchDurationValue = QStringLiteral("—");
    emit searchMetricsChanged();
  }

  void SearchController::updateSearchMetrics(const search::SearchSummary& summary)
  {
    filesScannedValue = static_cast<qulonglong>(summary.filesScanned);
    matchesFoundValue = static_cast<qulonglong>(summary.matches);
    timeToFirstResultValue = formatDuration(summary.metrics.timeToFirstResult);
    searchDurationValue = formatDuration(summary.metrics.totalTime);
    emit searchMetricsChanged();
  }

} // namespace uburu::app
