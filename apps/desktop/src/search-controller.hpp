#pragma once

#include "app/services/search-service.hpp"

#include <QAbstractListModel>
#include <QFutureWatcher>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariantList>

#include <chrono>
#include <memory>
#include <stop_token>

namespace uburu::app
{

  enum class PreviewLoadStatus
  {
    completed,
    cancelled,
    openFailed,
    readFailed,
    binarySkipped,
    invalidEncoding,
    lineTooLong
  };

  struct PreviewLoadResult
  {
    QString filePath;
    QString location;
    QString fallbackPreview;
    QString text;
    QString html;
    PreviewLoadStatus status{PreviewLoadStatus::completed};
    bool truncated{false};
  };

  class SearchResultModel final : public QAbstractListModel
  {
    Q_OBJECT

  public:
    enum Role
    {
      PathRole = Qt::UserRole + 1,
      AbsolutePathRole,
      LocationRole,
      PreviewRole,
      HighlightsRole
    };
    explicit SearchResultModel(QObject* parent = nullptr);
    [[nodiscard]] int rowCount(const QModelIndex& parent = {}) const override;
    [[nodiscard]] QVariant data(const QModelIndex& index, int role) const override;
    [[nodiscard]] QHash<int, QByteArray> roleNames() const override;
    void clear();
    void append(SearchResult result);

  private:
    std::vector<SearchResult> results;
  };

  class SearchController final : public QObject
  {
    Q_OBJECT
    Q_PROPERTY(QString directory READ directory NOTIFY directoryChanged)
    Q_PROPERTY(QString status READ status NOTIFY statusChanged)
    Q_PROPERTY(bool running READ running NOTIFY runningChanged)
    Q_PROPERTY(QAbstractItemModel* results READ results CONSTANT)
    Q_PROPERTY(QStringList recentDirectories READ recentDirectories NOTIFY scopeHistoryChanged)
    Q_PROPERTY(QStringList favoriteDirectories READ favoriteDirectories NOTIFY scopeHistoryChanged)
    Q_PROPERTY(bool currentDirectoryFavorite READ currentDirectoryFavorite NOTIFY scopeHistoryChanged)
    Q_PROPERTY(qulonglong filesScanned READ filesScanned NOTIFY searchMetricsChanged)
    Q_PROPERTY(qulonglong matchesFound READ matchesFound NOTIFY searchMetricsChanged)
    Q_PROPERTY(QString timeToFirstResult READ timeToFirstResult NOTIFY searchMetricsChanged)
    Q_PROPERTY(QString searchDuration READ searchDuration NOTIFY searchMetricsChanged)
    Q_PROPERTY(QString previewFilePath READ previewFilePath NOTIFY previewChanged)
    Q_PROPERTY(QString previewLocation READ previewLocation NOTIFY previewChanged)
    Q_PROPERTY(QString previewText READ previewText NOTIFY previewChanged)
    Q_PROPERTY(QString previewHtml READ previewHtml NOTIFY previewChanged)
    Q_PROPERTY(bool previewLoading READ previewLoading NOTIFY previewLoadingChanged)

  public:
    explicit SearchController(QObject* parent = nullptr);
    ~SearchController() override;
    [[nodiscard]] QString directory() const;
    [[nodiscard]] QString status() const;
    [[nodiscard]] bool running() const;
    [[nodiscard]] QAbstractItemModel* results();
    [[nodiscard]] QStringList recentDirectories() const;
    [[nodiscard]] QStringList favoriteDirectories() const;
    [[nodiscard]] bool currentDirectoryFavorite() const;
    [[nodiscard]] qulonglong filesScanned() const;
    [[nodiscard]] qulonglong matchesFound() const;
    [[nodiscard]] QString timeToFirstResult() const;
    [[nodiscard]] QString searchDuration() const;
    [[nodiscard]] QString previewFilePath() const;
    [[nodiscard]] QString previewLocation() const;
    [[nodiscard]] QString previewText() const;
    [[nodiscard]] QString previewHtml() const;
    [[nodiscard]] bool previewLoading() const;

    Q_INVOKABLE void selectDirectory(const QString& url);
    Q_INVOKABLE void selectSavedDirectory(const QString& path);
    Q_INVOKABLE void toggleCurrentDirectoryFavorite();
    Q_INVOKABLE void toggleFavoriteDirectory(const QString& path);
    Q_INVOKABLE void openFile(const QString& path);
    Q_INVOKABLE void openContainingFolder(const QString& path);
    Q_INVOKABLE void copyToClipboard(const QString& text);
    Q_INVOKABLE void loadPreview(const QString& path,
                                 const QString& location,
                                 const QString& fallbackPreview,
                                 const QVariantList& highlights);
    Q_INVOKABLE void clearPreview();
    Q_INVOKABLE void startSearch(const QString& expression,
                                 bool regex,
                                 bool caseSensitive,
                                 bool wholeWord,
                                 bool respectGitignore,
                                 bool includeSubdirectories,
                                 const QString& documentTypes);
    Q_INVOKABLE void cancel();

  signals:
    void directoryChanged();
    void statusChanged();
    void runningChanged();
    void scopeHistoryChanged();
    void searchMetricsChanged();
    void previewChanged();
    void previewLoadingChanged();

  private:
    void loadScopeHistory();
    void saveScopeHistory() const;
    void setDirectory(QString directory);
    void addRecentDirectory(const QString& directory);
    void setStatus(QString status);
    void setRunning(bool running);
    void setPreviewLoading(bool loading);
    void setPreviewResult(const PreviewLoadResult& result);
    void resetSearchMetrics();
    void updateSearchMetrics(const search::SearchSummary& summary);
    QString directoryValue;
    QString statusValue;
    QStringList recentDirectoryValues;
    QStringList favoriteDirectoryValues;
    qulonglong filesScannedValue{0};
    qulonglong matchesFoundValue{0};
    QString timeToFirstResultValue;
    QString searchDurationValue;
    QString previewFilePathValue;
    QString previewLocationValue;
    QString previewTextValue;
    QString previewHtmlValue;
    bool previewLoadingValue{false};
    bool runningValue{false};
    SearchResultModel resultsModel;
    std::shared_ptr<const SearchService> searchService;
    std::stop_source stopSource;
    std::stop_source previewStopSource;
    QFutureWatcher<search::SearchSummary>* activeWatcher{nullptr};
    QFutureWatcher<PreviewLoadResult>* activePreviewWatcher{nullptr};
  };

} // namespace uburu::app
