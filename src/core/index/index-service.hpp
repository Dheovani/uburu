#pragma once

#include "shared/types/domain-types.hpp"

#include <span>
#include <stop_token>

namespace uburu::index
{

  struct IndexUpdateSummary
  {
    std::size_t indexed{0};
    std::size_t reusedByHash{0};
    std::size_t removed{0};
    bool cancelled{false};
  };

  class IndexService
  {
  public:
    virtual ~IndexService() = default;
    [[nodiscard]] virtual IndexUpdateSummary update(const WorktreeInfo& worktree,
                                                    std::span<const FileEntry> files,
                                                    std::stop_token stop_token = {}) = 0;
    [[nodiscard]] virtual std::vector<SearchResult>
    search(const SearchQuery& query, std::stop_token stop_token = {}) const = 0;
  };

} // namespace uburu::index
