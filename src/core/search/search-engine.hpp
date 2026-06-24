#pragma once

#include "shared/types/domain-types.hpp"

#include <functional>
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
    invalid_result_limit,
    invalid_maximum_file_size
  };

  struct SearchError
  {
    SearchErrorCode code{SearchErrorCode::empty_expression};
    std::string context;
  };

  struct SearchSummary
  {
    std::size_t files_scanned{0};
    std::size_t matches{0};
    bool cancelled{false};
    bool limit_reached{false};
    std::vector<SearchError> errors;
  };

  class SearchEngine
  {
  public:
    virtual ~SearchEngine() = default;
    [[nodiscard]] virtual SearchSummary search(const SearchQuery& query, ResultSink sink,
                                               std::stop_token stop_token = {}) const = 0;
  };

} // namespace uburu::search
