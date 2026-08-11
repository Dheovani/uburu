#pragma once

#include "shared/types/domain-types.hpp"

#include <cstddef>
#include <span>
#include <vector>

namespace uburu::search
{

  /**
   * Splits direct validation into confirmed, added, removed, and authoritative final results.
   */
  struct SearchResultRefinement
  {
    std::vector<SearchResult> confirmed;
    std::vector<SearchResult> added;
    std::vector<SearchResult> removed;
    std::vector<SearchResult> merged;
  };

  [[nodiscard]]
  bool searchResultLess(const SearchResult& left, const SearchResult& right);

  [[nodiscard]]
  bool searchResultSameMatch(const SearchResult& left, const SearchResult& right);

  void sortAndRemoveDuplicateSearchResults(std::vector<SearchResult>& results);

  [[nodiscard]]
  bool orderedSearchResultsContain(std::span<const SearchResult> orderedResults, const SearchResult& candidate);

  [[nodiscard]]
  SearchResultRefinement refineSearchResults(
    std::span<const SearchResult> indexedResults,
    std::span<const SearchResult> directResults,
    std::size_t resultLimit);

  [[nodiscard]]
  std::vector<SearchResult> mergeSearchResults(
    std::span<const SearchResult> indexedResults,
    std::span<const SearchResult> directResults,
    std::size_t resultLimit);

} // namespace uburu::search
