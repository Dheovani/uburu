#pragma once

#include "core/filesystem/file-scanner.hpp"
#include "core/git/git-service.hpp"
#include "core/index/index-service.hpp"

#include <memory>

namespace uburu::app
{

  class IndexingService
  {
  public:
    virtual ~IndexingService() = default;
    [[nodiscard]] virtual index::IndexUpdateSummary update(const WorktreeInfo& worktree,
                                                           const SearchOptions& options,
                                                           const index::IndexProgressCallback& onProgress = {},
                                                           std::stop_token stopToken = {}) = 0;
  };

  class DefaultIndexingService final : public IndexingService
  {
  public:
    DefaultIndexingService(std::shared_ptr<const filesystem::FileScanner> scanner,
                           std::shared_ptr<const git::GitService> gitService,
                           std::shared_ptr<index::IndexService> indexService);

    [[nodiscard]] index::IndexUpdateSummary update(const WorktreeInfo& worktree,
                                                   const SearchOptions& options,
                                                   const index::IndexProgressCallback& onProgress = {},
                                                   std::stop_token stopToken = {}) override;

  private:
    std::shared_ptr<const filesystem::FileScanner> scanner;
    std::shared_ptr<const git::GitService> gitService;
    std::shared_ptr<index::IndexService> indexService;
  };

} // namespace uburu::app
