#pragma once

#include "core/git/git-service.hpp"

namespace uburu::git
{

  class GitCliGitService final : public GitService
  {
  public:
    [[nodiscard]] GitResult<RepositoryInfo> discoverRepository(const std::filesystem::path& path) const override;
    [[nodiscard]] GitResult<std::vector<WorktreeInfo>>
    listWorktrees(const RepositoryInfo& repository) const override;
    [[nodiscard]] GitResult<GitFileStatus>
    fileStatus(const WorktreeInfo& worktree, const std::filesystem::path& relativePath) const override;
    [[nodiscard]] GitResult<std::optional<std::string>>
    blobHash(const WorktreeInfo& worktree, const std::filesystem::path& relativePath) const override;
    [[nodiscard]] GitResult<std::vector<GitOverlayEntry>>
    workingTreeOverlay(const WorktreeInfo& worktree) const override;
    [[nodiscard]] GitResult<GitRepositoryBoundary>
    repositoryBoundary(const WorktreeInfo& worktree, const std::filesystem::path& relativePath) const override;
    [[nodiscard]] GitResult<GitObjectHashAlgorithm> objectHashAlgorithm(const RepositoryInfo& repository) const override;
    [[nodiscard]] GitResult<GitChangeState> changeState(const WorktreeInfo& worktree) const override;
  };

} // namespace uburu::git
