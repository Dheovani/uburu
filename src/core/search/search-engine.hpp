#pragma once

#include "shared/types/domain-types.hpp"

#include <functional>
#include <optional>
#include <stop_token>
#include <string>
#include <vector>

namespace uburu::search
{

  using ResultSink = std::function<bool(SearchResult)>;

  enum class SearchErrorCode
  {
    empty_root,
    root_not_found,
    root_not_directory,
    empty_expression,
    unsupported_search_mode,
    regex_compile_failed,
    regex_resource_limit_exceeded,
    regex_timeout,
    invalid_regex_limit,
    invalid_result_limit,
    invalid_per_file_result_limit,
    invalid_maximum_file_size,
    file_open_failed,
    file_read_failed
  };

  struct SearchError
  {
    SearchErrorCode code{SearchErrorCode::empty_expression};
    std::string translation_key;
    std::string context;
    std::optional<std::size_t> offset;
  };

  enum class RegexExecutionMode
  {
    not_used,
    jit,
    interpreted_fallback
  };

  struct SearchSummary
  {
    std::size_t files_scanned{0};
    std::size_t matches{0};
    std::size_t files_with_match_limit_reached{0};
    std::size_t files_with_read_errors{0};
    bool cancelled{false};
    bool limit_reached{false};
    bool partial_failure{false};
    std::vector<SearchError> errors;
    RegexExecutionMode regex_execution_mode{RegexExecutionMode::not_used};
  };

  class SearchEngine
  {
  public:
    virtual ~SearchEngine() = default;
    [[nodiscard]] virtual SearchSummary search(const SearchQuery& query, ResultSink sink,
                                               std::stop_token stop_token = {}) const = 0;
  };

} // namespace uburu::search
