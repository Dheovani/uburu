#pragma once

#include "shared/types/domain-types.hpp"

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

namespace uburu::storage
{

  class StorageService
  {
  public:
    virtual ~StorageService() = default;
    virtual void initialize() = 0;
    virtual void upsertRepository(const RepositoryInfo& repository) = 0;
    virtual void upsertWorktree(const WorktreeInfo& worktree) = 0;
    virtual void upsertDocument(const IndexDocument& document) = 0;
    virtual void publishGeneration(const IndexGeneration& generation) = 0;
    [[nodiscard]] virtual std::size_t recoverIncompleteGenerations() = 0;
    [[nodiscard]] virtual std::size_t collectOrphanDocuments() = 0;
    [[nodiscard]] virtual StoragePragmaSnapshot pragmaSnapshot() const = 0;
    [[nodiscard]] virtual StorageIntegrityReport validateIntegrity() const = 0;
    virtual void rebuildIndexCatalog() = 0;
    virtual void
    setPreference(std::optional<RepositoryId> repositoryId, const std::string& key, const std::string& value) = 0;
    [[nodiscard]] virtual std::optional<std::string> preference(std::optional<RepositoryId> repositoryId,
                                                                const std::string& key) const = 0;
    virtual void recordSearch(const SearchHistoryEntry& entry, std::size_t retentionLimit) = 0;
    [[nodiscard]] virtual std::vector<SearchHistoryEntry> recentSearches(std::size_t limit) const = 0;
    virtual void saveSearch(const SavedSearch& search) = 0;
    [[nodiscard]] virtual std::vector<SavedSearch> savedSearches() const = 0;
    virtual void recordIndexingMetric(const IndexingMetric& metric, std::size_t retentionLimit) = 0;
    [[nodiscard]] virtual std::vector<IndexingMetric> recentIndexingMetrics(const std::string& name,
                                                                            std::size_t limit) const = 0;
    virtual void removeDocument(const WorktreeId& worktreeId, const std::filesystem::path& relativePath) = 0;
    [[nodiscard]] virtual std::optional<IndexDocument>
    findDocument(const WorktreeId& worktreeId, const std::filesystem::path& relativePath) const = 0;
    [[nodiscard]] virtual std::optional<IndexedDocumentIdentity>
    findReusableDocumentByContentHash(ContentHashAlgorithm algorithm, const std::string& contentHash) const = 0;
    [[nodiscard]] virtual std::optional<IndexedDocumentIdentity>
    findReusableDocumentByGitBlobHash(GitObjectHashAlgorithm algorithm, const std::string& gitBlobHash) const = 0;
  };

} // namespace uburu::storage
