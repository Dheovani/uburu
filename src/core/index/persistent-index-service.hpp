#pragma once

#include "core/index/index-service.hpp"
#include "core/storage/storage-service.hpp"

#include <chrono>

namespace uburu::index
{

  /**
   * Stores searchable index generations and reuses documents through StorageService.
   */
  class PersistentIndexService final : public IndexService
  {
  public:
    explicit PersistentIndexService(storage::StorageService& storage);

    [[nodiscard]]
    IndexUpdateSummary update(
      const WorktreeInfo& worktree,
      std::span<const FileEntry> files,
      const IndexProgressCallback& onProgress = {},
      std::stop_token stopToken = {}) override;

    [[nodiscard]]
    IndexUpdateSummary update(
      const WorktreeInfo& worktree,
      std::span<const IndexFileCandidate> files,
      const IndexProgressCallback& onProgress = {},
      std::stop_token stopToken = {}) override;

    [[nodiscard]]
    IndexUpdateSummary update(
      const WorktreeInfo& worktree,
      std::span<const FileEntry> files,
      std::span<const GitOverlayEntry> overlay,
      const IndexProgressCallback& onProgress = {},
      std::stop_token stopToken = {}) override;

    [[nodiscard]]
    IndexStalenessReport staleness(const WorktreeInfo& worktree) const override;

    [[nodiscard]]
    std::vector<SearchResult> search(
      const SearchQuery& query,
      std::stop_token stopToken = {}) const override;

  private:
    storage::StorageService* storageService{nullptr};
  };

} // namespace uburu::index
