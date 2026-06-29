#pragma once

#include "shared/types/domain-types.hpp"

#include <optional>
#include <cstddef>

namespace uburu::storage
{

  class StorageService
  {
  public:
    virtual ~StorageService() = default;
    virtual void initialize() = 0;
    virtual void upsertRepository(const RepositoryInfo& repository) = 0;
    virtual void upsertWorktree(const WorktreeInfo& worktree) = 0;
    virtual void upsertDocument(const IndexDocument& document) = 0;
    virtual void publishGeneration(const IndexGeneration& generation) = 0;
    [[nodiscard]] virtual std::size_t recoverIncompleteGenerations() = 0;
    [[nodiscard]] virtual std::size_t collectOrphanDocuments() = 0;
    virtual void removeDocument(const WorktreeId& worktreeId, const std::filesystem::path& relativePath) = 0;
    [[nodiscard]] virtual std::optional<IndexDocument>
    findDocument(const WorktreeId& worktreeId, const std::filesystem::path& relativePath) const = 0;
  };

} // namespace uburu::storage
