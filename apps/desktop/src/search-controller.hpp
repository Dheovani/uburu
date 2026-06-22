#pragma once

#include "app/services/search-service.hpp"

#include <QAbstractListModel>
#include <QFutureWatcher>
#include <QObject>
#include <QString>

#include <memory>
#include <stop_token>

namespace uburu::app
{

  class SearchResultModel final : public QAbstractListModel
  {
    Q_OBJECT

  public:
    enum Role
    {
      PathRole = Qt::UserRole + 1,
      LocationRole,
      PreviewRole
    };
    explicit SearchResultModel(QObject* parent = nullptr);
    [[nodiscard]] int rowCount(const QModelIndex& parent = {}) const override;
    [[nodiscard]] QVariant data(const QModelIndex& index, int role) const override;
    [[nodiscard]] QHash<int, QByteArray> roleNames() const override;
    void clear();
    void append(SearchResult result);

  private:
    std::vector<SearchResult> results_;
  };

  class SearchController final : public QObject
  {
    Q_OBJECT
    Q_PROPERTY(QString directory READ directory NOTIFY directoryChanged)
    Q_PROPERTY(QString status READ status NOTIFY statusChanged)
    Q_PROPERTY(bool running READ running NOTIFY runningChanged)
    Q_PROPERTY(QAbstractItemModel* results READ results CONSTANT)

  public:
    explicit SearchController(QObject* parent = nullptr);
    ~SearchController() override;
    [[nodiscard]] QString directory() const;
    [[nodiscard]] QString status() const;
    [[nodiscard]] bool running() const;
    [[nodiscard]] QAbstractItemModel* results();

    Q_INVOKABLE void selectDirectory(const QString& url);
    Q_INVOKABLE void startSearch(const QString& expression, bool regex, bool case_sensitive,
                                 bool whole_word, bool respect_gitignore);
    Q_INVOKABLE void cancel();

  signals:
    void directoryChanged();
    void statusChanged();
    void runningChanged();

  private:
    void set_status(QString status);
    void set_running(bool running);
    QString directory_;
    QString status_;
    bool running_{false};
    SearchResultModel results_;
    std::shared_ptr<const SearchService> search_service_;
    std::stop_source stop_source_;
    QFutureWatcher<search::SearchSummary>* active_watcher_{nullptr};
  };

} // namespace uburu::app
