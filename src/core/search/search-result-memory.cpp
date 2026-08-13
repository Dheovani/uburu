#include "core/search/search-result-memory.hpp"

#include <filesystem>
#include <string>
#include <vector>

namespace uburu::search
{
  namespace
  {

    [[nodiscard]]
    std::uint64_t stringMemoryBytes(const std::string& value)
    {
      return static_cast<std::uint64_t>(value.capacity());
    }

    [[nodiscard]]
    std::uint64_t pathMemoryBytes(const std::filesystem::path& path)
    {
      return static_cast<std::uint64_t>(path.native().size() * sizeof(std::filesystem::path::value_type));
    }

    [[nodiscard]]
    std::uint64_t stringVectorMemoryBytes(const std::vector<std::string>& values)
    {
      std::uint64_t memoryBytes = static_cast<std::uint64_t>(values.capacity() * sizeof(std::string));

      for (const auto& value : values)
        memoryBytes += stringMemoryBytes(value);

      return memoryBytes;
    }

  } // namespace

  std::uint64_t approximateSearchResultMemoryBytes(const SearchResult& result)
  {
    std::uint64_t memoryBytes = sizeof(SearchResult);

    memoryBytes += pathMemoryBytes(result.path);
    memoryBytes += pathMemoryBytes(result.searchRoot);
    memoryBytes += stringMemoryBytes(result.lineText);
    memoryBytes += stringMemoryBytes(result.documentSection);
    memoryBytes += static_cast<std::uint64_t>(result.highlights.capacity() * sizeof(MatchSpan));
    memoryBytes += stringVectorMemoryBytes(result.contextBefore);
    memoryBytes += stringVectorMemoryBytes(result.contextAfter);

    return memoryBytes;
  }

  std::uint64_t approximateSearchResultsMemoryBytes(std::span<const SearchResult> results)
  {
    std::uint64_t memoryBytes = static_cast<std::uint64_t>(results.size() * sizeof(SearchResult));

    for (const auto& result : results)
      memoryBytes += approximateSearchResultMemoryBytes(result) - sizeof(SearchResult);

    return memoryBytes;
  }

  bool searchResultFitsMemoryBudget(std::uint64_t retainedMemoryBytes,
                                    std::uint64_t resultMemoryBytes,
                                    std::uintmax_t memoryBudgetBytes)
  {
    if (memoryBudgetBytes == 0)
      return true;

    return retainedMemoryBytes <= memoryBudgetBytes && resultMemoryBytes <= memoryBudgetBytes - retainedMemoryBytes;
  }

} // namespace uburu::search
