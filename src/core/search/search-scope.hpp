#pragma once

#include "shared/types/domain-types.hpp"

#include <vector>

namespace uburu::search
{

  /**
   * Resolves the active roots for a query, preferring explicit scoped roots over legacy root.
   */
  [[nodiscard]]
  std::vector<SearchRoot> effectiveSearchRoots(const SearchQuery& query);

  /**
   * Applies per-root filters over the base query options.
   */
  [[nodiscard]]
  SearchOptions optionsForRoot(const SearchOptions& options, const SearchRoot& root);

} // namespace uburu::search
