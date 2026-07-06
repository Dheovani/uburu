#include "core/search/search-scope.hpp"

#include <system_error>

namespace uburu::search
{
  namespace
  {

    bool relativePathEscapesRoot(const std::filesystem::path& path)
    {
      const auto begin = path.begin();

      return begin != path.end() && *begin == "..";
    }

    std::filesystem::path rootRelativeFilterPath(const std::filesystem::path& root, const std::filesystem::path& path)
    {
      if (path.empty() || !path.is_absolute())
        return path.lexically_normal();

      std::error_code error;
      auto relative = std::filesystem::relative(path, root, error);

      if (error)
        relative = path.lexically_normal().lexically_relative(root.lexically_normal());

      if (relative.empty() || relative.is_absolute() || relativePathEscapesRoot(relative))
        return path.lexically_normal();

      if (relative == ".")
        return {};

      return relative.lexically_normal();
    }

    std::vector<std::filesystem::path> rootRelativeFilterPaths(const std::filesystem::path& root,
                                                               const std::vector<std::filesystem::path>& paths)
    {
      std::vector<std::filesystem::path> relativePaths;
      relativePaths.reserve(paths.size());

      for (const auto& path : paths)
        relativePaths.push_back(rootRelativeFilterPath(root, path));

      return relativePaths;
    }

  } // namespace

  std::vector<SearchRoot> effectiveSearchRoots(const SearchQuery& query)
  {
    if (!query.scope.roots.empty())
      return query.scope.roots;

    if (query.root.empty())
      return {};

    return {SearchRoot{.path = query.root,
                       .includedDirectories = query.options.includedDirectories,
                       .excludedDirectories = query.options.excludedDirectories}};
  }

  SearchOptions optionsForRoot(const SearchOptions& options, const SearchRoot& root)
  {
    auto rootOptions = options;

    if (!root.includedDirectories.empty())
      rootOptions.includedDirectories = rootRelativeFilterPaths(root.path, root.includedDirectories);

    if (!root.excludedDirectories.empty())
      rootOptions.excludedDirectories = rootRelativeFilterPaths(root.path, root.excludedDirectories);

    return rootOptions;
  }

} // namespace uburu::search
