#include "core/search/search-scope.hpp"

namespace uburu::search
{

  std::vector<SearchRoot> effectiveSearchRoots(const SearchQuery& query)
  {
    if (!query.scope.roots.empty())
      return query.scope.roots;

    if (query.root.empty())
      return {};

    return {SearchRoot{
      .path = query.root,
      .includedDirectories = query.options.includedDirectories,
      .excludedDirectories = query.options.excludedDirectories}};
  }

  SearchOptions optionsForRoot(const SearchOptions& options, const SearchRoot& root)
  {
    auto rootOptions = options;

    if (!root.includedDirectories.empty())
      rootOptions.includedDirectories = root.includedDirectories;

    if (!root.excludedDirectories.empty())
      rootOptions.excludedDirectories = root.excludedDirectories;

    return rootOptions;
  }

} // namespace uburu::search
