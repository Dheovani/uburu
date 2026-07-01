#include "core/search/search-result-merge.hpp"

#include <algorithm>
#include <tuple>

namespace uburu::search
{
  namespace
  {

    [[nodiscard]] int resultKindRank(SearchResultKind kind)
    {
      return kind == SearchResultKind::fileName ? 0 : 1;
    }

    [[nodiscard]] auto resultSortKey(const SearchResult& result)
    {
      return std::tuple{result.searchRoot.generic_string(),
                        result.path.generic_string(),
                        resultKindRank(result.kind),
                        result.line,
                        result.column,
                        result.matchLength,
                        result.lineText};
    }

    [[nodiscard]] bool containsSameMatch(std::span<const SearchResult> results, const SearchResult& candidate)
    {
      return std::ranges::any_of(results, [&](const auto& result) {
        return searchResultSameMatch(result, candidate);
      });
    }

  } // namespace

  bool searchResultLess(const SearchResult& left, const SearchResult& right)
  {
    return resultSortKey(left) < resultSortKey(right);
  }

  bool searchResultSameMatch(const SearchResult& left, const SearchResult& right)
  {
    return resultSortKey(left) == resultSortKey(right);
  }

  SearchResultRefinement refineSearchResults(std::span<const SearchResult> indexedResults,
                                             std::span<const SearchResult> directResults,
                                             std::size_t resultLimit)
  {
    SearchResultRefinement refinement;

    for (const auto& indexedResult : indexedResults) {
      if (containsSameMatch(directResults, indexedResult)) {
        refinement.confirmed.push_back(indexedResult);

        continue;
      }

      refinement.removed.push_back(indexedResult);
    }

    for (const auto& directResult : directResults) {
      if (!containsSameMatch(indexedResults, directResult))
        refinement.added.push_back(directResult);
    }

    std::ranges::sort(refinement.confirmed, searchResultLess);
    std::ranges::sort(refinement.added, searchResultLess);
    std::ranges::sort(refinement.removed, searchResultLess);
    refinement.merged = mergeSearchResults(indexedResults, directResults, resultLimit);

    return refinement;
  }

  std::vector<SearchResult> mergeSearchResults(std::span<const SearchResult> indexedResults,
                                               std::span<const SearchResult> directResults,
                                               std::size_t resultLimit)
  {
    std::vector<SearchResult> merged;
    merged.reserve(indexedResults.size() + directResults.size());

    for (const auto& result : directResults) {
      merged.push_back(result);
    }

    for (const auto& result : indexedResults) {
      if (!containsSameMatch(merged, result))
        merged.push_back(result);
    }

    std::ranges::sort(merged, searchResultLess);

    if (merged.size() > resultLimit)
      merged.resize(resultLimit);

    return merged;
  }

} // namespace uburu::search
