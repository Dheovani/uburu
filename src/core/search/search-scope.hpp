#pragma once

#include "shared/types/domain-types.hpp"

#include <vector>

namespace uburu::search
{

  [[nodiscard]] std::vector<SearchRoot> effective_search_roots(const SearchQuery& query);

  [[nodiscard]] SearchOptions options_for_root(const SearchOptions& options, const SearchRoot& root);

} // namespace uburu::search
