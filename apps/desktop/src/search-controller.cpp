#include "search-controller.hpp"

#include "core/filesystem/recursive-file-scanner.hpp"
#include "core/search/direct-search-engine.hpp"

#include <QFileInfo>
#include <QFutureWatcher>
#include <QMetaObject>
#include <QUrl>
#include <QtConcurrentRun>

namespace uburu::app
{
  namespace
  {

    std::filesystem::path native_path(const QString& path)
    {
#ifdef _WIN32
      return std::filesystem::path(path.toStdWString());
#else
      return std::filesystem::path(path.toUtf8().constData());
#endif
    }

  } // namespace

  SearchResultModel::SearchResultModel(QObject* parent) : QAbstractListModel(parent) {}

  int SearchResultModel::rowCount(const QModelIndex& parent) const
  {
    return parent.isValid() ? 0 : static_cast<int>(results_.size());
  }

  QVariant SearchResultModel::data(const QModelIndex& index, int role) const
  {
    if (!index.isValid() || index.row() < 0 ||
        static_cast<std::size_t>(index.row()) >= results_.size())
      return {};
    const auto& result = results_[static_cast<std::size_t>(index.row())];
    if (role == PathRole)
      return QString::fromStdString(result.path.generic_string());
    if (role == LocationRole)
      return QStringLiteral("%1:%2").arg(result.line).arg(result.column);
    if (role == PreviewRole)
      return QString::fromStdString(result.line_text);
    return {};
  }

  QHash<int, QByteArray> SearchResultModel::roleNames() const
  {
    return {
      {PathRole, "filePath"},
      {LocationRole, "location"},
      {PreviewRole, "preview"}
    };
  }

  void SearchResultModel::clear()
  {
    beginResetModel();
    results_.clear();
    endResetModel();
  }

  void SearchResultModel::append(SearchResult result)
  {
    const auto row = static_cast<int>(results_.size());
    beginInsertRows({}, row, row);
    results_.push_back(std::move(result));
    endInsertRows();
  }

  SearchController::SearchController(QObject* parent)
      : QObject(parent), status_(tr("Pronto")), results_(this),
        search_service_(
            std::make_shared<DefaultSearchService>(std::make_shared<search::DirectSearchEngine>(
                std::make_shared<filesystem::RecursiveFileScanner>())))
  {}

  SearchController::~SearchController()
  {
    cancel();
    if (active_watcher_ != nullptr)
      active_watcher_->waitForFinished();
  }

  QString SearchController::directory() const
  {
    return directory_;
  }

  QString SearchController::status() const
  {
    return status_;
  }

  bool SearchController::running() const
  {
    return running_;
  }

  QAbstractItemModel* SearchController::results()
  {
    return &results_;
  }

  void SearchController::selectDirectory(const QString& url)
  {
    directory_ = QUrl(url).toLocalFile();
    emit directoryChanged();
  }

  void SearchController::startSearch(const QString& expression, bool regex, bool case_sensitive,
                                     bool whole_word, bool respect_gitignore)
  {
    if (running_ || directory_.isEmpty() || expression.isEmpty())
      return;

    results_.clear();
    stop_source_ = std::stop_source{};
    set_running(true);
    set_status(tr("Buscando…"));
    SearchQuery query{.root = native_path(directory_),
                      .expression = expression.toUtf8().toStdString()};
    query.options.mode = regex ? SearchMode::regex : SearchMode::literal;
    query.options.case_sensitive = case_sensitive;
    query.options.whole_word = whole_word;
    query.options.respect_gitignore = respect_gitignore;
    const auto token = stop_source_.get_token();

    active_watcher_ = new QFutureWatcher<search::SearchSummary>(this);
    connect(active_watcher_, &QFutureWatcher<search::SearchSummary>::finished, this, [this] {
      const auto summary = active_watcher_->result();
      set_status(summary.cancelled ? tr("Busca cancelada")
                                   : tr("%n ocorrência(s) encontrada(s)", nullptr,
                                        static_cast<int>(summary.matches)));
      set_running(false);
      active_watcher_->deleteLater();
      active_watcher_ = nullptr;
    });
    active_watcher_->setFuture(QtConcurrent::run([this, query = std::move(query), token] {
      return search_service_->search(
          query,
          [this](SearchResult result) {
            QMetaObject::invokeMethod(
                this,
                [this, result = std::move(result)]() mutable {
                  results_.append(std::move(result));
                },
                Qt::QueuedConnection);
            return true;
          },
          token);
    }));
  }

  void SearchController::cancel()
  {
    stop_source_.request_stop();
  }

  void SearchController::set_status(QString status)
  {
    status_ = std::move(status);
    emit statusChanged();
  }

  void SearchController::set_running(bool running)
  {
    running_ = running;
    emit runningChanged();
  }

} // namespace uburu::app
