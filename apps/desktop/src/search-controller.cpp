#include "search-controller.hpp"

#include "core/filesystem/recursive-file-scanner.hpp"
#include "core/search/direct-search-engine.hpp"
#include "core/search/search-errors.hpp"
#include "core/text/text-file-reader.hpp"

#include <QClipboard>
#include <QDesktopServices>
#include <QDir>
#include <QFileInfo>
#include <QFutureWatcher>
#include <QGuiApplication>
#include <QMetaObject>
#include <QProcess>
#include <QRegularExpression>
#include <QSettings>
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
    constexpr auto recentDirectoriesKey = "recentDirectories";
    constexpr auto favoriteDirectoriesKey = "favoriteDirectories";

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

    QStringList withoutDirectory(QStringList directories, const QString& directory)
    {
      directories.removeAll(directory);

      return directories;
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

      return context.isEmpty() ? receiver->tr("Erro ao pesquisar")
                               : receiver->tr("Erro ao pesquisar: %1").arg(context);
    }

    QString partialSearchStatus(const search::SearchSummary& summary, QObject* receiver)
    {
      auto status = completedSearchStatus(summary, receiver);
      const auto errorCount = static_cast<int>(summary.errors.size());
      const auto context = firstSearchErrorContext(summary);

      if (errorCount == 0)
        return status;

      const auto warning =
        context.isEmpty()
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

    QString formattedPreviewHtmlLine(const text::TextLine& line,
                                     bool selected,
                                     const std::vector<MatchSpan>& highlights)
    {
      const auto lineNumber = QString::number(static_cast<qulonglong>(line.lineNumber)).rightJustified(
        previewLineNumberWidth, QLatin1Char(' '));
      const auto lineText = selected ? highlightedHtml(line.text, highlights)
                                     : QString::fromStdString(line.text).toHtmlEscaped();
      const auto lineStyle = selected ? QStringLiteral(" style=\"background:%1;\"")
                                          .arg(QString::fromLatin1(previewHtmlSelectedLineBackground))
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
          QStringLiteral("<html><body style=\"white-space:pre;font-family:Consolas,monospace;color:%1;\">%2</body></html>")
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

  SearchController::SearchController(QObject* parent)
    : QObject(parent), statusValue(tr("Pronto")), timeToFirstResultValue(QStringLiteral("—")),
      searchDurationValue(QStringLiteral("—")), resultsModel(this),
      searchService(std::make_shared<DefaultSearchService>(
        std::make_shared<search::DirectSearchEngine>(std::make_shared<filesystem::RecursiveFileScanner>())))
  {
    loadScopeHistory();
  }

  SearchController::~SearchController()
  {
    cancel();
    previewStopSource.request_stop();

    if (activeWatcher != nullptr)
      activeWatcher->waitForFinished();

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
    addRecentDirectory(directory);
    saveScopeHistory();
  }

  void SearchController::selectSavedDirectory(const QString& path)
  {
    selectDirectory(path);
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
    watcher->setFuture(QtConcurrent::run([path, location, fallbackPreview, previewHighlights = std::move(previewHighlights), token] {
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
    if (runningValue || directoryValue.isEmpty() || expression.isEmpty())
      return;

    resultsModel.clear();
    clearPreview();
    resetSearchMetrics();
    stopSource = std::stop_source{};
    setCancelling(false);
    setRunning(true);
    setStatus(tr("Buscando..."));

    SearchQuery query{.root = nativePath(directoryValue),
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
    activeWatcher->setFuture(QtConcurrent::run([this,
                                                query = std::move(query),
                                                token,
                                                startedAt,
                                                firstResultNanoseconds] {
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

  void SearchController::loadScopeHistory()
  {
    QSettings settings;
    settings.beginGroup(QString::fromLatin1(settingsScopeGroup));
    recentDirectoryValues = settings.value(QString::fromLatin1(recentDirectoriesKey)).toStringList();
    favoriteDirectoryValues = settings.value(QString::fromLatin1(favoriteDirectoriesKey)).toStringList();
    settings.endGroup();

    for (const auto& directory : recentDirectoryValues) {
      if (!directory.isEmpty() && QFileInfo::exists(directory)) {
        directoryValue = directory;
        break;
      }
    }
  }

  void SearchController::saveScopeHistory() const
  {
    QSettings settings;
    settings.beginGroup(QString::fromLatin1(settingsScopeGroup));
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
