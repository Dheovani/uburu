#pragma once

#include "core/search/search-engine.hpp"

#include <vector>

namespace uburu::search
{

  /**
   * Validates a query before any scanner, index, or regex backend is invoked.
   */
  [[nodiscard]]
  std::vector<SearchError> validateSearchQuery(const SearchQuery& query);

} // namespace uburu::search
