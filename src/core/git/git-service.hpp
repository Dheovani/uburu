#pragma once

#include "shared/types/domain-types.hpp"

#include <optional>

namespace uburu::git
{

  class GitService
  {
  public:
    virtual ~GitService() = default;
    [[nodiscard]] virtual std::optional<RepositoryInfo>
    discover_repository(const std::filesystem::path& path) const = 0;
    [[nodiscard]] virtual std::vector<WorktreeInfo>
    list_worktrees(const RepositoryInfo& repository) const = 0;
    [[nodiscard]] virtual GitFileStatus
    file_status(const WorktreeInfo& worktree, const std::filesystem::path& relative_path) const = 0;
    [[nodiscard]] virtual std::optional<std::string>
    blob_hash(const WorktreeInfo& worktree, const std::filesystem::path& relative_path) const = 0;
  };

} // namespace uburu::git
