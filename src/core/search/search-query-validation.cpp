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

    void append_if(bool condition, std::vector<SearchError>& errors, SearchErrorCode code,
                   std::string context = {})
    {
      if (condition)
        errors.push_back(make_search_error(code, std::move(context)));
    }

    void validate_root(const std::filesystem::path& root, std::vector<SearchError>& errors)
    {
      std::error_code error;
      const auto exists = std::filesystem::exists(root, error);

      if (error || !exists) {
        errors.push_back(make_search_error(SearchErrorCode::root_not_found, root.string()));
      } else if (!std::filesystem::is_directory(root, error) || error) {
        errors.push_back(make_search_error(SearchErrorCode::root_not_directory, root.string()));
      }
    }

  } // namespace

  std::vector<SearchError> validate_search_query(const SearchQuery& query)
  {
    std::vector<SearchError> errors;
    const auto roots = effective_search_roots(query);

    append_if(roots.empty(), errors, SearchErrorCode::empty_root);

    for (const auto& root : roots) {
      if (root.path.empty()) {
        errors.push_back(make_search_error(SearchErrorCode::empty_root));

        continue;
      }

      validate_root(root.path, errors);
    }

    append_if(query.expression.empty(), errors, SearchErrorCode::empty_expression);
#ifndef UBURU_HAS_PCRE2
    append_if(query.options.mode == SearchMode::regex, errors,
              SearchErrorCode::unsupported_search_mode, "regex");
#endif
    append_if(query.options.result_limit == 0, errors, SearchErrorCode::invalid_result_limit);
    append_if(query.options.per_file_result_limit == 0, errors,
              SearchErrorCode::invalid_per_file_result_limit);
    append_if(query.options.maximum_file_size == 0, errors,
              SearchErrorCode::invalid_maximum_file_size);
    append_if(query.options.regex_match_limit == 0 || query.options.regex_depth_limit == 0 ||
                  query.options.regex_heap_limit_kib == 0,
              errors, SearchErrorCode::invalid_regex_limit);

    return errors;
  }

} // namespace uburu::search
