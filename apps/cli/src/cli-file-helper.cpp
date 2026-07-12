#include "cli-file-helper.hpp"

#include <string>

namespace uburu::cli
{

  [[nodiscard]]
  std::string pathIdentity(const std::filesystem::path& path)
  {
    return path.lexically_normal().generic_string();
  }

  [[nodiscard]]
  std::filesystem::path normalizedAbsolutePath(const std::filesystem::path& path)
  {
    std::error_code error;
    auto normalized = std::filesystem::absolute(path, error);

    if (!error)
      return normalized.lexically_normal();

    return path.lexically_normal();
  }

  [[nodiscard]]
  uburu::WorktreeInfo filesystemWorktree(const std::filesystem::path& root)
  {
    const auto normalizedRoot = normalizedAbsolutePath(root);
    const auto id = "filesystem:" + pathIdentity(normalizedRoot);

    uburu::WorktreeInfo worktree;
    worktree.id = id;
    worktree.repositoryId = id;
    worktree.root = normalizedRoot;
    worktree.gitDirectory = std::filesystem::path{};
    worktree.headOid = "filesystem";

    return worktree;
  }

  [[nodiscard]]
  uburu::RepositoryInfo filesystemRepository(const uburu::WorktreeInfo& worktree)
  {
    uburu::RepositoryInfo repository;
    repository.id = worktree.repositoryId;
    repository.commonGitDirectory = std::filesystem::path{};
    repository.worktreeRoot = worktree.root;
    repository.headOid = worktree.headOid;

    return repository;
  }

}
