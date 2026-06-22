#pragma once

#include "shared/types/domain-types.hpp"

#include <optional>

namespace uburu::storage
{

  class StorageService
  {
  public:
    virtual ~StorageService() = default;
    virtual void initialize() = 0;
    virtual void upsert_repository(const RepositoryInfo& repository) = 0;
    virtual void upsert_worktree(const WorktreeInfo& worktree) = 0;
    virtual void upsert_document(const IndexDocument& document) = 0;
    virtual void remove_document(const WorktreeId& worktree_id,
                                 const std::filesystem::path& relative_path) = 0;
    [[nodiscard]] virtual std::optional<IndexDocument>
    find_document(const WorktreeId& worktree_id,
                  const std::filesystem::path& relative_path) const = 0;
  };

} // namespace uburu::storage
