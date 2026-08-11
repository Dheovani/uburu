#include "app/services/indexing-service.hpp"

#include "core/index/index-working-memory.hpp"

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

    [[nodiscard]]
    index::IndexUpdateSummary memoryLimitedSummary(std::uint64_t workingMemoryBytes)
    {
      index::IndexUpdateSummary summary;
      summary.workingMemoryPeakBytes = workingMemoryBytes;
      summary.memoryLimitReached = true;

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
    std::uint64_t retainedMemoryBytes = 0;
    bool memoryLimitReached = false;

    scanner->scan(
      worktree.root,
      options,
      [&](FileEntry file) {
        if (stopToken.stop_requested())
          return false;

        const auto fileMemoryBytes = index::approximateFileEntryMemoryBytes(file);

        if (!index::indexWorkingMemoryFitsBudget(
              retainedMemoryBytes, fileMemoryBytes, options.resultMemoryBudgetBytes)) {
          memoryLimitReached = true;

          return false;
        }

        retainedMemoryBytes += fileMemoryBytes;
        files.push_back(std::move(file));

        return true;
      },
      stopToken);

    if (stopToken.stop_requested())
      return cancelledSummary();

    if (memoryLimitReached)
      return memoryLimitedSummary(retainedMemoryBytes);

    const auto overlayResult = gitService->workingTreeOverlay(worktree);
    const auto* overlay = std::get_if<std::vector<GitOverlayEntry>>(&overlayResult);

    if (overlay == nullptr)
      return failedSummary();

    index::IndexUpdateOptions indexOptions;
    indexOptions.memoryBudgetBytes = options.resultMemoryBudgetBytes;

    return indexService->update(worktree, files, *overlay, onProgress, stopToken, indexOptions);
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
