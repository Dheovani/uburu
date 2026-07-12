#pragma once

#include "shared/types/domain-types.hpp"

#include <filesystem>
#include <string>

namespace uburu::cli
{

  [[nodiscard]]
  std::string pathIdentity(const std::filesystem::path& path);

  [[nodiscard]]
  std::filesystem::path normalizedAbsolutePath(const std::filesystem::path& path);

  [[nodiscard]]
  uburu::WorktreeInfo filesystemWorktree(const std::filesystem::path& root);

  [[nodiscard]]
  uburu::RepositoryInfo filesystemRepository(const uburu::WorktreeInfo& worktree);

}
