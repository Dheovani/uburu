#include "core/search/search-query-validation.hpp"

#include "core/search/search-errors.hpp"
#include "core/search/search-scope.hpp"

#include <filesystem>
#include <optional>
#include <system_error>
#include <utility>

namespace uburu::search
{
  namespace
  {

    void appendIf(bool condition, std::vector<SearchError>& errors, SearchErrorCode code, std::string context = {})
    {
      if (condition)
        errors.push_back(makeSearchError(code, std::move(context)));
    }

    std::string pathToUtf8(const std::filesystem::path& path)
    {
      const auto text = path.generic_u8string();

      return {reinterpret_cast<const char*>(text.data()), text.size()};
    }

    std::string rootUnavailableContext(const std::filesystem::path& root, const std::error_code& error)
    {
      if (!error)
        return pathToUtf8(root);

      return pathToUtf8(root) + ": " + error.message();
    }

    bool isMissingPathError(const std::error_code& error)
    {
      return error.default_error_condition() == std::errc::no_such_file_or_directory;
    }

    bool canOpenRootDirectory(const std::filesystem::path& root, std::error_code& error)
    {
      const std::filesystem::directory_iterator iterator(root, error);
      (void)iterator;

      return !error;
    }

    void validateRoot(const std::filesystem::path& root, std::vector<SearchError>& errors)
    {
      std::error_code error;
      const auto status = std::filesystem::status(root, error);

      if (error && isMissingPathError(error)) {
        errors.push_back(makeSearchError(SearchErrorCode::rootNotFound, pathToUtf8(root)));
      } else if (error) {
        errors.push_back(makeSearchError(SearchErrorCode::rootUnavailable, rootUnavailableContext(root, error)));
      } else if (!std::filesystem::exists(status)) {
        errors.push_back(makeSearchError(SearchErrorCode::rootNotFound, pathToUtf8(root)));
      } else if (!std::filesystem::is_directory(status)) {
        errors.push_back(makeSearchError(SearchErrorCode::rootNotDirectory, pathToUtf8(root)));
      } else if (!canOpenRootDirectory(root, error)) {
        errors.push_back(makeSearchError(SearchErrorCode::rootUnavailable, rootUnavailableContext(root, error)));
      }
    }

  } // namespace

  std::vector<SearchError> validateSearchQuery(const SearchQuery& query)
  {
    std::vector<SearchError> errors;
    const auto roots = effectiveSearchRoots(query);

    appendIf(roots.empty(), errors, SearchErrorCode::emptyRoot);

    for (const auto& root : roots) {
      if (root.path.empty()) {
        errors.push_back(makeSearchError(SearchErrorCode::emptyRoot));

        continue;
      }

      validateRoot(root.path, errors);
    }

    appendIf(query.expression.empty(), errors, SearchErrorCode::emptyExpression);
#ifndef UBURU_HAS_PCRE2
    appendIf(query.options.mode == SearchMode::regex, errors, SearchErrorCode::unsupportedSearchMode, "regex");
#endif
    appendIf(query.options.resultLimit == 0, errors, SearchErrorCode::invalidResultLimit);
    appendIf(query.options.perFileResultLimit == 0, errors, SearchErrorCode::invalidPerFileResultLimit);
    appendIf(query.options.maximumFileSize == 0, errors, SearchErrorCode::invalidMaximumFileSize);
    appendIf(query.options.regexMatchLimit == 0 || query.options.regexDepthLimit == 0 ||
               query.options.regexHeapLimitKib == 0,
             errors,
             SearchErrorCode::invalidRegexLimit);

    return errors;
  }

} // namespace uburu::search
