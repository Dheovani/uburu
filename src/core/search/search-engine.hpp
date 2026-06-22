#pragma once

#include "shared/types/domain-types.hpp"

#include <functional>
#include <stop_token>

namespace uburu::search
{

  using ResultSink = std::function<bool(SearchResult)>;

  struct SearchSummary
  {
    std::size_t files_scanned{0};
    std::size_t matches{0};
    bool cancelled{false};
    bool limit_reached{false};
  };

  class SearchEngine
  {
  public:
    virtual ~SearchEngine() = default;
    [[nodiscard]] virtual SearchSummary search(const SearchQuery& query, ResultSink sink,
                                               std::stop_token stop_token = {}) const = 0;
  };

} // namespace uburu::search
