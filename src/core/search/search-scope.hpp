#pragma once

#include "shared/types/domain-types.hpp"

#include <vector>

namespace uburu::search
{

  [[nodiscard]] std::vector<SearchRoot> effectiveSearchRoots(const SearchQuery& query);

  [[nodiscard]] SearchOptions optionsForRoot(const SearchOptions& options, const SearchRoot& root);

} // namespace uburu::search
