#pragma once

#include "core/filesystem/file-scanner.hpp"
#include "core/filesystem/file-watcher.hpp"
#include "core/git/git-service.hpp"
#include "core/index/index-service.hpp"

#include <memory>

namespace uburu::app
{

  enum class IndexingServiceState
  {
    running,
    paused
  };

  /**
   * Application facade for manual and incremental indexing operations.
   */
  class IndexingService
  {
  public:
    virtual ~IndexingService() = default;

    virtual void pause() = 0;
    virtual void resume() = 0;

    [[nodiscard]]
    virtual IndexingServiceState state() const = 0;

    [[nodiscard]]
    virtual index::IndexUpdateSummary requestManualReindex(
      const WorktreeInfo& worktree,
      const SearchOptions& options,
      const index::IndexProgressCallback& onProgress = {},
      std::stop_token stopToken = {}) = 0;

    [[nodiscard]]
    virtual index::IndexUpdateSummary update(
      const WorktreeInfo& worktree,
      const SearchOptions& options,
      const index::IndexProgressCallback& onProgress = {},
      std::stop_token stopToken = {}) = 0;

    [[nodiscard]]
    virtual index::IndexUpdateSummary reconcile(
      const WorktreeInfo& worktree,
      const SearchOptions& options,
      const filesystem::FileChangeBatch& batch,
      const index::IndexProgressCallback& onProgress = {},
      std::stop_token stopToken = {}) = 0;
  };

  /**
   * Default indexing workflow that scans files, asks Git for overlay state, and updates the index service.
   */
  class DefaultIndexingService final : public IndexingService
  {
  public:
    DefaultIndexingService(std::shared_ptr<const filesystem::FileScanner> scanner,
                           std::shared_ptr<const git::GitService> gitService,
                           std::shared_ptr<index::IndexService> indexService);

    void pause() override;
    void resume() override;

    [[nodiscard]]
    IndexingServiceState state() const override;

    [[nodiscard]]
    index::IndexUpdateSummary requestManualReindex(
      const WorktreeInfo& worktree,
      const SearchOptions& options,
      const index::IndexProgressCallback& onProgress = {},
      std::stop_token stopToken = {}) override;

    [[nodiscard]]
    index::IndexUpdateSummary update(
      const WorktreeInfo& worktree,
      const SearchOptions& options,
      const index::IndexProgressCallback& onProgress = {},
      std::stop_token stopToken = {}) override;

    [[nodiscard]]
    index::IndexUpdateSummary reconcile(
      const WorktreeInfo& worktree,
      const SearchOptions& options,
      const filesystem::FileChangeBatch& batch,
      const index::IndexProgressCallback& onProgress = {},
      std::stop_token stopToken = {}) override;

  private:
    std::shared_ptr<const filesystem::FileScanner> scanner;
    std::shared_ptr<const git::GitService> gitService;
    std::shared_ptr<index::IndexService> indexService;
    IndexingServiceState currentState{IndexingServiceState::running};
  };

} // namespace uburu::app
