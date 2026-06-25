#include "core/search/search-scope.hpp"

namespace uburu::search
{

  std::vector<SearchRoot> effective_search_roots(const SearchQuery& query)
  {
    if (!query.scope.roots.empty())
      return query.scope.roots;

    if (query.root.empty())
      return {};

    return {SearchRoot{
      .path = query.root,
      .included_directories = query.options.included_directories,
      .excluded_directories = query.options.excluded_directories}};
  }

  SearchOptions options_for_root(const SearchOptions& options, const SearchRoot& root)
  {
    auto root_options = options;

    if (!root.included_directories.empty())
      root_options.included_directories = root.included_directories;

    if (!root.excluded_directories.empty())
      root_options.excluded_directories = root.excluded_directories;

    return root_options;
  }

} // namespace uburu::search
