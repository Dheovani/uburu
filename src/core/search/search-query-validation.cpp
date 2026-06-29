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

    void validateRoot(const std::filesystem::path& root, std::vector<SearchError>& errors)
    {
      std::error_code error;
      const auto exists = std::filesystem::exists(root, error);

      if (error || !exists) {
        errors.push_back(makeSearchError(SearchErrorCode::rootNotFound, root.string()));
      } else if (!std::filesystem::is_directory(root, error) || error) {
        errors.push_back(makeSearchError(SearchErrorCode::rootNotDirectory, root.string()));
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
             errors, SearchErrorCode::invalidRegexLimit);

    return errors;
  }

} // namespace uburu::search
