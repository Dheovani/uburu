#include "app/services/indexing-service.hpp"

#include <stdexcept>
#include <utility>
#include <variant>
#include <vector>

namespace uburu::app
{

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

  index::IndexUpdateSummary DefaultIndexingService::update(const WorktreeInfo& worktree,
                                                           const SearchOptions& options,
                                                           const index::IndexProgressCallback& onProgress,
                                                           std::stop_token stopToken)
  {
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
      return index::IndexUpdateSummary{.cancelled = true};

    const auto overlayResult = gitService->workingTreeOverlay(worktree);
    const auto* overlay = std::get_if<std::vector<GitOverlayEntry>>(&overlayResult);

    if (overlay == nullptr)
      return index::IndexUpdateSummary{.failed = 1};

    return indexService->update(worktree, files, *overlay, onProgress, stopToken);
  }

} // namespace uburu::app
