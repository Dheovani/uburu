#pragma once

#include "shared/types/domain-types.hpp"

#include <filesystem>
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace uburu::git
{

  enum class GitErrorCode
  {
    notRepository,
    repositoryOpenFailed,
    headReadFailed,
    worktreeReadFailed,
    statusReadFailed,
    blobReadFailed,
    boundaryReadFailed,
    backendUnavailable
  };

  struct GitError
  {
    GitErrorCode code{GitErrorCode::backendUnavailable};
    std::string message;
  };

  struct GitChangeState
  {
    std::optional<std::string> branch;
    std::string headOid;
    bool detachedHead{false};
    std::string headSignature;
    std::string indexSignature;
    std::string relevantRefsSignature;
  };

  template <typename T> using GitResult = std::variant<T, GitError>;

  template <typename T> [[nodiscard]] bool succeeded(const GitResult<T>& result)
  {
    return std::holds_alternative<T>(result);
  }

  class GitService
  {
  public:
    virtual ~GitService() = default;
    [[nodiscard]] virtual GitResult<RepositoryInfo> discoverRepository(const std::filesystem::path& path) const = 0;
    [[nodiscard]] virtual GitResult<std::vector<WorktreeInfo>>
    listWorktrees(const RepositoryInfo& repository) const = 0;
    [[nodiscard]] virtual GitResult<GitFileStatus> fileStatus(const WorktreeInfo& worktree,
                                                              const std::filesystem::path& relativePath) const = 0;
    [[nodiscard]] virtual GitResult<std::optional<std::string>>
    blobHash(const WorktreeInfo& worktree, const std::filesystem::path& relativePath) const = 0;
    [[nodiscard]] virtual GitResult<std::vector<GitOverlayEntry>>
    workingTreeOverlay(const WorktreeInfo& worktree) const = 0;
    [[nodiscard]] virtual GitResult<GitRepositoryBoundary>
    repositoryBoundary(const WorktreeInfo& worktree, const std::filesystem::path& relativePath) const = 0;
    [[nodiscard]] virtual GitResult<GitObjectHashAlgorithm>
    objectHashAlgorithm(const RepositoryInfo& repository) const = 0;
    [[nodiscard]] virtual GitResult<GitChangeState> changeState(const WorktreeInfo& worktree) const = 0;
  };

} // namespace uburu::git
