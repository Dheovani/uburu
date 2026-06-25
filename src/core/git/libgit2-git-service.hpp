#pragma once

#include "core/git/git-service.hpp"

namespace uburu::git
{

  class Libgit2GitService final : public GitService
  {
  public:
    Libgit2GitService();
    ~Libgit2GitService() override;

    Libgit2GitService(const Libgit2GitService&) = delete;
    Libgit2GitService& operator=(const Libgit2GitService&) = delete;

    [[nodiscard]] GitResult<RepositoryInfo>
    discoverRepository(const std::filesystem::path& path) const override;
    [[nodiscard]] GitResult<std::vector<WorktreeInfo>>
    listWorktrees(const RepositoryInfo& repository) const override;
    [[nodiscard]] GitResult<GitFileStatus>
    fileStatus(const WorktreeInfo& worktree, const std::filesystem::path& relativePath) const override;
    [[nodiscard]] GitResult<std::optional<std::string>>
    blobHash(const WorktreeInfo& worktree, const std::filesystem::path& relativePath) const override;
    [[nodiscard]] GitResult<GitChangeState>
    changeState(const WorktreeInfo& worktree) const override;
  };

} // namespace uburu::git
