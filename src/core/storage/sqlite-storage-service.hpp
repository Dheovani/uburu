#pragma once

#include "core/storage/storage-service.hpp"

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace uburu::storage
{

  class SQLiteStorageService final : public StorageService
  {
  public:
    explicit SQLiteStorageService(std::filesystem::path databasePath);
    ~SQLiteStorageService() override;

    SQLiteStorageService(const SQLiteStorageService&) = delete;
    SQLiteStorageService& operator=(const SQLiteStorageService&) = delete;
    SQLiteStorageService(SQLiteStorageService&&) noexcept = delete;
    SQLiteStorageService& operator=(SQLiteStorageService&&) noexcept = delete;

    void initialize() override;
    void upsertRepository(const RepositoryInfo& repository) override;
    void upsertWorktree(const WorktreeInfo& worktree) override;
    void upsertDocument(const IndexDocument& document) override;
    void publishGeneration(const IndexGeneration& generation) override;
    [[nodiscard]] std::size_t recoverIncompleteGenerations() override;
    [[nodiscard]] std::size_t collectOrphanDocuments() override;
    [[nodiscard]] StoragePragmaSnapshot pragmaSnapshot() const override;
    [[nodiscard]] StorageIntegrityReport validateIntegrity() const override;
    void rebuildIndexCatalog() override;
    void setPreference(std::optional<RepositoryId> repositoryId, const std::string& key,
                       const std::string& value) override;
    [[nodiscard]] std::optional<std::string> preference(std::optional<RepositoryId> repositoryId,
                                                        const std::string& key) const override;
    void recordSearch(const SearchHistoryEntry& entry, std::size_t retentionLimit) override;
    [[nodiscard]] std::vector<SearchHistoryEntry> recentSearches(std::size_t limit) const override;
    void saveSearch(const SavedSearch& search) override;
    [[nodiscard]] std::vector<SavedSearch> savedSearches() const override;
    void recordIndexingMetric(const IndexingMetric& metric, std::size_t retentionLimit) override;
    [[nodiscard]] std::vector<IndexingMetric> recentIndexingMetrics(const std::string& name,
                                                                    std::size_t limit) const override;
    void removeDocument(const WorktreeId& worktreeId, const std::filesystem::path& relativePath) override;
    [[nodiscard]] std::optional<IndexDocument> findDocument(const WorktreeId& worktreeId,
                                                            const std::filesystem::path& relativePath) const override;
    [[nodiscard]] std::optional<IndexedDocumentIdentity>
    findReusableDocumentByContentHash(ContentHashAlgorithm algorithm, const std::string& contentHash) const override;
    [[nodiscard]] std::optional<IndexedDocumentIdentity>
    findReusableDocumentByGitBlobHash(GitObjectHashAlgorithm algorithm, const std::string& gitBlobHash) const override;

  private:
    std::filesystem::path databasePath;
    void* databaseHandle{nullptr};
  };

} // namespace uburu::storage
