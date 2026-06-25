#include "core/search/search-errors.hpp"

#include <utility>

namespace uburu::search
{

  std::string translation_key_for(SearchErrorCode code)
  {
    switch (code) {
    case SearchErrorCode::empty_root:
      return "search.error.emptyRoot";
    case SearchErrorCode::root_not_found:
      return "search.error.rootNotFound";
    case SearchErrorCode::root_not_directory:
      return "search.error.rootNotDirectory";
    case SearchErrorCode::empty_expression:
      return "search.error.emptyExpression";
    case SearchErrorCode::unsupported_search_mode:
      return "search.error.unsupportedSearchMode";
    case SearchErrorCode::regex_compile_failed:
      return "search.error.regexCompileFailed";
    case SearchErrorCode::regex_resource_limit_exceeded:
      return "search.error.regexResourceLimitExceeded";
    case SearchErrorCode::regex_timeout:
      return "search.error.regexTimeout";
    case SearchErrorCode::invalid_regex_limit:
      return "search.error.invalidRegexLimit";
    case SearchErrorCode::invalid_result_limit:
      return "search.error.invalidResultLimit";
    case SearchErrorCode::invalid_per_file_result_limit:
      return "search.error.invalidPerFileResultLimit";
    case SearchErrorCode::invalid_maximum_file_size:
      return "search.error.invalidMaximumFileSize";
    case SearchErrorCode::file_open_failed:
      return "search.error.fileOpenFailed";
    case SearchErrorCode::file_read_failed:
      return "search.error.fileReadFailed";
    }

    return "search.error.unknown";
  }

  SearchError make_search_error(SearchErrorCode code, std::string context,
                                std::optional<std::size_t> offset)
  {
    return SearchError{code, translation_key_for(code), std::move(context), offset};
  }

} // namespace uburu::search
