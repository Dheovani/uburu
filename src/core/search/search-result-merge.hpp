#pragma once

#include "shared/types/domain-types.hpp"

#include <cstddef>
#include <span>
#include <vector>

namespace uburu::search
{

  [[nodiscard]] bool searchResultLess(const SearchResult& left, const SearchResult& right);
  [[nodiscard]] bool searchResultSameMatch(const SearchResult& left, const SearchResult& right);
  [[nodiscard]] std::vector<SearchResult> mergeSearchResults(std::span<const SearchResult> indexedResults,
                                                             std::span<const SearchResult> directResults,
                                                             std::size_t resultLimit);

} // namespace uburu::search
