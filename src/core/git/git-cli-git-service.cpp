#include "core/git/git-cli-git-service.hpp"

namespace uburu::git
{
  namespace
  {

    [[nodiscard]] GitError unavailable()
    {
      return GitError{.code = GitErrorCode::backendUnavailable,
                      .message = "Git CLI fallback adapter is explicit but not enabled"};
    }

  } // namespace

  GitResult<RepositoryInfo> GitCliGitService::discoverRepository(const std::filesystem::path& path) const
  {
    static_cast<void>(path);

    return unavailable();
  }

  GitResult<std::vector<WorktreeInfo>> GitCliGitService::listWorktrees(const RepositoryInfo& repository) const
  {
    static_cast<void>(repository);

    return unavailable();
  }

  GitResult<GitFileStatus> GitCliGitService::fileStatus(const WorktreeInfo& worktree,
                                                        const std::filesystem::path& relativePath) const
  {
    static_cast<void>(worktree);
    static_cast<void>(relativePath);

    return unavailable();
  }

  GitResult<std::optional<std::string>> GitCliGitService::blobHash(const WorktreeInfo& worktree,
                                                                   const std::filesystem::path& relativePath) const
  {
    static_cast<void>(worktree);
    static_cast<void>(relativePath);

    return unavailable();
  }

  GitResult<std::vector<GitOverlayEntry>> GitCliGitService::workingTreeOverlay(const WorktreeInfo& worktree) const
  {
    static_cast<void>(worktree);

    return unavailable();
  }

  GitResult<GitRepositoryBoundary> GitCliGitService::repositoryBoundary(const WorktreeInfo& worktree,
                                                                        const std::filesystem::path& relativePath) const
  {
    static_cast<void>(worktree);
    static_cast<void>(relativePath);

    return unavailable();
  }

  GitResult<GitObjectHashAlgorithm> GitCliGitService::objectHashAlgorithm(const RepositoryInfo& repository) const
  {
    static_cast<void>(repository);

    return unavailable();
  }

  GitResult<GitChangeState> GitCliGitService::changeState(const WorktreeInfo& worktree) const
  {
    static_cast<void>(worktree);

    return unavailable();
  }

} // namespace uburu::git
