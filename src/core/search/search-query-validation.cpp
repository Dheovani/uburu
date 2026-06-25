#include "core/search/search-query-validation.hpp"

#include "core/search/search-errors.hpp"

#include <filesystem>
#include <optional>
#include <system_error>
#include <utility>

namespace uburu::search
{
  namespace
  {

    void append_if(bool condition, std::vector<SearchError>& errors, SearchErrorCode code,
                   std::string context = {})
    {
      if (condition)
        errors.push_back(make_search_error(code, std::move(context)));
    }

  } // namespace

  std::vector<SearchError> validate_search_query(const SearchQuery& query)
  {
    std::vector<SearchError> errors;

    append_if(query.root.empty(), errors, SearchErrorCode::empty_root);

    if (!query.root.empty()) {
      std::error_code error;
      const auto exists = std::filesystem::exists(query.root, error);
      if (error || !exists) {
        errors.push_back(make_search_error(SearchErrorCode::root_not_found, query.root.string()));
      } else if (!std::filesystem::is_directory(query.root, error) || error) {
        errors.push_back(
            make_search_error(SearchErrorCode::root_not_directory, query.root.string()));
      }
    }

    append_if(query.expression.empty(), errors, SearchErrorCode::empty_expression);
#ifndef UBURU_HAS_PCRE2
    append_if(query.options.mode == SearchMode::regex, errors, SearchErrorCode::unsupported_search_mode, "regex");
#endif
    append_if(query.options.result_limit == 0, errors, SearchErrorCode::invalid_result_limit);
    append_if(query.options.per_file_result_limit == 0, errors, SearchErrorCode::invalid_per_file_result_limit);
    append_if(query.options.maximum_file_size == 0, errors, SearchErrorCode::invalid_maximum_file_size);
    append_if(query.options.regex_match_limit == 0 ||
              query.options.regex_depth_limit == 0 ||
              query.options.regex_heap_limit_kib == 0, errors, SearchErrorCode::invalid_regex_limit);

    return errors;
  }

} // namespace uburu::search
