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
    discover_repository(const std::filesystem::path& path) const override;
    [[nodiscard]] GitResult<std::vector<WorktreeInfo>>
    list_worktrees(const RepositoryInfo& repository) const override;
    [[nodiscard]] GitResult<GitFileStatus>
    file_status(const WorktreeInfo& worktree, const std::filesystem::path& relative_path) const override;
    [[nodiscard]] GitResult<std::optional<std::string>>
    blob_hash(const WorktreeInfo& worktree, const std::filesystem::path& relative_path) const override;
    [[nodiscard]] GitResult<GitChangeState>
    change_state(const WorktreeInfo& worktree) const override;
  };

} // namespace uburu::git
