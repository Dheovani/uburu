#pragma once

#include "core/search/search-engine.hpp"

#include <vector>

namespace uburu::search
{

  [[nodiscard]] std::vector<SearchError> validateSearchQuery(const SearchQuery& query);

} // namespace uburu::search
