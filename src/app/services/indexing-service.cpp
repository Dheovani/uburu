#include "app/services/indexing-service.hpp"

#include <stdexcept>
#include <utility>
#include <variant>
#include <vector>

namespace uburu::app
{
  namespace
  {

    [[nodiscard]]
    index::IndexUpdateSummary cancelledSummary()
    {
      index::IndexUpdateSummary summary;
      summary.cancelled = true;

      return summary;
    }

    [[nodiscard]]
    index::IndexUpdateSummary failedSummary()
    {
      index::IndexUpdateSummary summary;
      summary.failed = 1;

      return summary;
    }

  } // namespace

  DefaultIndexingService::DefaultIndexingService(std::shared_ptr<const filesystem::FileScanner> scanner,
                                                 std::shared_ptr<const git::GitService> gitService,
                                                 std::shared_ptr<index::IndexService> indexService)
    : scanner(std::move(scanner)), gitService(std::move(gitService)), indexService(std::move(indexService))
  {
    if (!this->scanner)
      throw std::invalid_argument("DefaultIndexingService requires a file scanner");

    if (!this->gitService)
      throw std::invalid_argument("DefaultIndexingService requires a Git service");

    if (!this->indexService)
      throw std::invalid_argument("DefaultIndexingService requires an index service");
  }

  void DefaultIndexingService::pause()
  {
    currentState = IndexingServiceState::paused;
  }

  void DefaultIndexingService::resume()
  {
    currentState = IndexingServiceState::running;
  }

  IndexingServiceState DefaultIndexingService::state() const
  {
    return currentState;
  }

  index::IndexUpdateSummary DefaultIndexingService::requestManualReindex(const WorktreeInfo& worktree,
                                                                         const SearchOptions& options,
                                                                         const index::IndexProgressCallback& onProgress,
                                                                         std::stop_token stopToken)
  {
    return update(worktree, options, onProgress, stopToken);
  }

  index::IndexUpdateSummary DefaultIndexingService::update(const WorktreeInfo& worktree,
                                                           const SearchOptions& options,
                                                           const index::IndexProgressCallback& onProgress,
                                                           std::stop_token stopToken)
  {
    if (currentState == IndexingServiceState::paused)
      return cancelledSummary();

    std::vector<FileEntry> files;

    scanner->scan(
      worktree.root,
      options,
      [&](FileEntry file) {
        if (stopToken.stop_requested())
          return false;

        files.push_back(std::move(file));

        return true;
      },
      stopToken);

    if (stopToken.stop_requested())
      return cancelledSummary();

    const auto overlayResult = gitService->workingTreeOverlay(worktree);
    const auto* overlay = std::get_if<std::vector<GitOverlayEntry>>(&overlayResult);

    if (overlay == nullptr)
      return failedSummary();

    return indexService->update(worktree, files, *overlay, onProgress, stopToken);
  }

  index::IndexUpdateSummary DefaultIndexingService::reconcile(const WorktreeInfo& worktree,
                                                              const SearchOptions& options,
                                                              const filesystem::FileChangeBatch& batch,
                                                              const index::IndexProgressCallback& onProgress,
                                                              std::stop_token stopToken)
  {
    if (currentState == IndexingServiceState::paused)
      return cancelledSummary();

    if (batch.events.empty() && !batch.eventsMayBeIncomplete && !batch.requiresRescan)
      return {};

    return update(worktree, options, onProgress, stopToken);
  }

} // namespace uburu::app
