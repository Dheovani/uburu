#include "search-controller.hpp"

#include "core/filesystem/recursive-file-scanner.hpp"
#include "core/search/direct-search-engine.hpp"
#include "core/search/search-errors.hpp"

#include <QFileInfo>
#include <QFutureWatcher>
#include <QMetaObject>
#include <QRegularExpression>
#include <QStringList>
#include <QUrl>
#include <QtConcurrentRun>

#include <exception>
#include <string>
#include <utility>

namespace uburu::app
{
  namespace
  {

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
        status += receiver->tr(" — pulados: %1").arg(skipped.join(receiver->tr(", ")));

      return status;
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

    const auto& result = results[static_cast<std::size_t>(index.row())];

    if (role == PathRole)
      return QString::fromUtf8(pathToUtf8(result.path));

    if (role == LocationRole)
      return QStringLiteral("%1:%2").arg(result.line).arg(result.column);

    if (role == PreviewRole)
      return QString::fromStdString(result.lineText);

    return {};
  }

  QHash<int, QByteArray> SearchResultModel::roleNames() const
  {
    return {{PathRole, "filePath"}, {LocationRole, "location"}, {PreviewRole, "preview"}};
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
    : QObject(parent), statusValue(tr("Pronto")), resultsModel(this),
      searchService(std::make_shared<DefaultSearchService>(
        std::make_shared<search::DirectSearchEngine>(std::make_shared<filesystem::RecursiveFileScanner>())))
  {}

  SearchController::~SearchController()
  {
    cancel();

    if (activeWatcher != nullptr)
      activeWatcher->waitForFinished();
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

  QAbstractItemModel* SearchController::results()
  {
    return &resultsModel;
  }

  void SearchController::selectDirectory(const QString& url)
  {
    const auto selectedUrl = QUrl(url);
    directoryValue = selectedUrl.isLocalFile() ? selectedUrl.toLocalFile() : url;
    emit directoryChanged();
  }

  void SearchController::startSearch(const QString& expression,
                                     bool regex,
                                     bool caseSensitive,
                                     bool wholeWord,
                                     bool respectGitignore,
                                     bool includeSubdirectories,
                                     const QString& documentTypes)
  {
    if (runningValue || directoryValue.isEmpty() || expression.isEmpty())
      return;

    resultsModel.clear();
    stopSource = std::stop_source{};
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
    query.options.includeSubdirectories = includeSubdirectories;
    query.options.target = SearchTarget::contentAndFileName;
    query.options.extensions = parseDocumentTypes(documentTypes);
    const auto token = stopSource.get_token();

    activeWatcher = new QFutureWatcher<search::SearchSummary>(this);
    connect(activeWatcher, &QFutureWatcher<search::SearchSummary>::finished, this, [this] {
      const auto summary = activeWatcher->result();

      if (!summary.errors.empty()) {
        const auto context = QString::fromUtf8(summary.errors.front().context);
        setStatus(context.isEmpty() ? tr("Erro ao pesquisar") : tr("Erro ao pesquisar: %1").arg(context));
      } else {
        setStatus(summary.cancelled ? tr("Busca cancelada") : completedSearchStatus(summary, this));
      }

      setRunning(false);
      activeWatcher->deleteLater();
      activeWatcher = nullptr;
    });
    activeWatcher->setFuture(QtConcurrent::run([this, query = std::move(query), token] {
      try {
        return searchService->search(
          query,
          [this](SearchResult result) {
            QMetaObject::invokeMethod(
              this,
              [this, result = std::move(result)]() mutable { resultsModel.append(std::move(result)); },
              Qt::QueuedConnection);
            return true;
          },
          token);
      } catch (const std::exception& exception) {
        return failedSearchSummary(QString::fromLocal8Bit(exception.what()));
      } catch (...) {
        return failedSearchSummary(QStringLiteral("unknown exception"));
      }
    }));
  }

  void SearchController::cancel()
  {
    stopSource.request_stop();
  }

  void SearchController::setStatus(QString status)
  {
    statusValue = std::move(status);
    emit statusChanged();
  }

  void SearchController::setRunning(bool running)
  {
    runningValue = running;
    emit runningChanged();
  }

} // namespace uburu::app
