#pragma once

#include "shared/types/domain-types.hpp"

#include <cstdint>
#include <span>

namespace uburu::search
{

  /**
   * Estimates the retained memory owned by one search result.
   */
  [[nodiscard]]
  std::uint64_t approximateSearchResultMemoryBytes(const SearchResult& result);

  /**
   * Estimates the retained memory owned by a contiguous search-result collection.
   */
  [[nodiscard]]
  std::uint64_t approximateSearchResultsMemoryBytes(std::span<const SearchResult> results);

  /**
   * Checks whether an additional result fits a zero-as-unlimited byte budget without integer overflow.
   */
  [[nodiscard]]
  bool searchResultFitsMemoryBudget(
    std::uint64_t retainedMemoryBytes,
    std::uint64_t resultMemoryBytes,
    std::uintmax_t memoryBudgetBytes);

} // namespace uburu::search
