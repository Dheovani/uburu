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

    void sortAndRemoveDuplicates(std::vector<SearchResult>& results)
    {
      std::ranges::sort(results, searchResultLess);
      const auto duplicateBegin = std::ranges::unique(results, searchResultSameMatch).begin();
      results.erase(duplicateBegin, results.end());
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
    std::vector<SearchResult> orderedIndexed(indexedResults.begin(), indexedResults.end());
    std::vector<SearchResult> orderedDirect(directResults.begin(), directResults.end());

    sortAndRemoveDuplicates(orderedIndexed);
    sortAndRemoveDuplicates(orderedDirect);

    auto indexed = orderedIndexed.begin();
    auto direct = orderedDirect.begin();

    while (indexed != orderedIndexed.end() && direct != orderedDirect.end()) {
      if (searchResultSameMatch(*indexed, *direct)) {
        refinement.confirmed.push_back(*direct);
        ++indexed;
        ++direct;

        continue;
      }

      if (searchResultLess(*indexed, *direct)) {
        refinement.removed.push_back(*indexed);
        ++indexed;

        continue;
      }

      refinement.added.push_back(*direct);
      ++direct;
    }

    while (indexed != orderedIndexed.end()) {
      refinement.removed.push_back(*indexed);
      ++indexed;
    }

    while (direct != orderedDirect.end()) {
      refinement.added.push_back(*direct);
      ++direct;
    }

    refinement.merged = std::move(orderedDirect);

    if (refinement.merged.size() > resultLimit)
      refinement.merged.resize(resultLimit);

    return refinement;
  }

  std::vector<SearchResult> mergeSearchResults(std::span<const SearchResult> indexedResults,
                                               std::span<const SearchResult> directResults,
                                               std::size_t resultLimit)
  {
    std::vector<SearchResult> merged(indexedResults.begin(), indexedResults.end());
    merged.insert(merged.end(), directResults.begin(), directResults.end());
    sortAndRemoveDuplicates(merged);

    if (merged.size() > resultLimit)
      merged.resize(resultLimit);

    return merged;
  }

} // namespace uburu::search
