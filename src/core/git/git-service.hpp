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
    not_repository,
    repository_open_failed,
    head_read_failed,
    worktree_read_failed,
    status_read_failed,
    blob_read_failed,
    backend_unavailable
  };

  struct GitError
  {
    GitErrorCode code{GitErrorCode::backend_unavailable};
    std::string message;
  };

  struct GitChangeState
  {
    std::optional<std::string> branch;
    std::string head_oid;
    bool detached_head{false};
    std::string head_signature;
    std::string index_signature;
    std::string relevant_refs_signature;
  };

  template <typename T>
  using GitResult = std::variant<T, GitError>;

  template <typename T>
  [[nodiscard]] bool succeeded(const GitResult<T>& result)
  {
    return std::holds_alternative<T>(result);
  }

  class GitService
  {
  public:
    virtual ~GitService() = default;
    [[nodiscard]] virtual GitResult<RepositoryInfo>
    discover_repository(const std::filesystem::path& path) const = 0;
    [[nodiscard]] virtual GitResult<std::vector<WorktreeInfo>>
    list_worktrees(const RepositoryInfo& repository) const = 0;
    [[nodiscard]] virtual GitResult<GitFileStatus>
    file_status(const WorktreeInfo& worktree, const std::filesystem::path& relative_path) const = 0;
    [[nodiscard]] virtual GitResult<std::optional<std::string>>
    blob_hash(const WorktreeInfo& worktree, const std::filesystem::path& relative_path) const = 0;
    [[nodiscard]] virtual GitResult<GitChangeState>
    change_state(const WorktreeInfo& worktree) const = 0;
  };

} // namespace uburu::git
