#include "core/search/search-errors.hpp"

#include <utility>

namespace uburu::search
{

  std::string translationKeyFor(SearchErrorCode code)
  {
    switch (code) {
    case SearchErrorCode::emptyRoot:
      return "search.error.emptyRoot";
    case SearchErrorCode::rootNotFound:
      return "search.error.rootNotFound";
    case SearchErrorCode::rootNotDirectory:
      return "search.error.rootNotDirectory";
    case SearchErrorCode::emptyExpression:
      return "search.error.emptyExpression";
    case SearchErrorCode::unsupportedSearchMode:
      return "search.error.unsupportedSearchMode";
    case SearchErrorCode::regexCompileFailed:
      return "search.error.regexCompileFailed";
    case SearchErrorCode::regexResourceLimitExceeded:
      return "search.error.regexResourceLimitExceeded";
    case SearchErrorCode::regexTimeout:
      return "search.error.regexTimeout";
    case SearchErrorCode::invalidRegexLimit:
      return "search.error.invalidRegexLimit";
    case SearchErrorCode::invalidResultLimit:
      return "search.error.invalidResultLimit";
    case SearchErrorCode::invalidPerFileResultLimit:
      return "search.error.invalidPerFileResultLimit";
    case SearchErrorCode::invalidMaximumFileSize:
      return "search.error.invalidMaximumFileSize";
    case SearchErrorCode::fileOpenFailed:
      return "search.error.fileOpenFailed";
    case SearchErrorCode::fileReadFailed:
      return "search.error.fileReadFailed";
    }

    return "search.error.unknown";
  }

  SearchError makeSearchError(SearchErrorCode code, std::string context, std::optional<std::size_t> offset)
  {
    return SearchError{code, translationKeyFor(code), std::move(context), offset};
  }

} // namespace uburu::search
