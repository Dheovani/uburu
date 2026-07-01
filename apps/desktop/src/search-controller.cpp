#include "search-controller.hpp"

#include "core/filesystem/recursive-file-scanner.hpp"
#include "core/search/direct-search-engine.hpp"
#include "core/search/search-errors.hpp"

#include <QFileInfo>
#include <QFutureWatcher>
#include <QMetaObject>
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

  void SearchController::startSearch(
    const QString& expression, bool regex, bool caseSensitive, bool wholeWord, bool respectGitignore)
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
    const auto token = stopSource.get_token();

    activeWatcher = new QFutureWatcher<search::SearchSummary>(this);
    connect(activeWatcher, &QFutureWatcher<search::SearchSummary>::finished, this, [this] {
      const auto summary = activeWatcher->result();

      if (!summary.errors.empty()) {
        const auto context = QString::fromUtf8(summary.errors.front().context);
        setStatus(context.isEmpty() ? tr("Erro ao pesquisar") : tr("Erro ao pesquisar: %1").arg(context));
      } else {
        setStatus(summary.cancelled
                    ? tr("Busca cancelada")
                    : tr("%n ocorrência(s) encontrada(s)", nullptr, static_cast<int>(summary.matches)));
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
