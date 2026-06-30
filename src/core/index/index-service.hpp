#pragma once

#include "shared/types/domain-types.hpp"

#include <filesystem>
#include <functional>
#include <span>
#include <stop_token>
#include <vector>

namespace uburu::index
{

  struct IndexUpdateProgress
  {
    std::size_t processed{0};
    std::size_t total{0};
    std::size_t indexed{0};
    std::size_t reusedByCatalog{0};
    std::size_t reusedByHash{0};
    std::size_t failed{0};
    std::filesystem::path currentPath;
  };

  struct IndexUpdateSummary
  {
    std::size_t indexed{0};
    std::size_t reusedByCatalog{0};
    std::size_t reusedByHash{0};
    std::size_t removed{0};
    std::size_t failed{0};
    bool cancelled{false};
  };

  using IndexProgressCallback = std::function<void(const IndexUpdateProgress&)>;

  class IndexService
  {
  public:
    virtual ~IndexService() = default;
    [[nodiscard]] virtual IndexUpdateSummary update(const WorktreeInfo& worktree, std::span<const FileEntry> files,
                                                    const IndexProgressCallback& onProgress = {},
                                                    std::stop_token stopToken = {}) = 0;
    [[nodiscard]] virtual std::vector<SearchResult> search(const SearchQuery& query,
                                                           std::stop_token stopToken = {}) const = 0;
  };

} // namespace uburu::index
