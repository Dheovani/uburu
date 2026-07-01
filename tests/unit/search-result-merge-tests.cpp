#include "core/search/search-result-merge.hpp"

#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <vector>

namespace
{

  constexpr std::size_t fixtureMatchLength = 3;
  constexpr std::size_t generousResultLimit = 10;

  [[nodiscard]] uburu::SearchResult result(uburu::SearchResultKind kind,
                                           std::filesystem::path path,
                                           std::size_t line,
                                           std::size_t column,
                                           std::string lineText)
  {
    return uburu::SearchResult{.kind = kind,
                               .path = std::move(path),
                               .line = line,
                               .column = column,
                               .matchLength = fixtureMatchLength,
                               .lineText = std::move(lineText),
                               .searchRoot = "repo"};
  }

} // namespace

TEST_CASE("search result merge orders results deterministically")
{
  const std::vector indexedResults{
    result(uburu::SearchResultKind::content, "src/b.cpp", 4, 3, "bbb"),
    result(uburu::SearchResultKind::fileName, "src/a.cpp", 0, 5, "src/a.cpp"),
  };
  const std::vector directResults{
    result(uburu::SearchResultKind::content, "src/a.cpp", 2, 1, "aaa"),
  };

  const auto merged = uburu::search::mergeSearchResults(indexedResults, directResults, generousResultLimit);

  REQUIRE(merged.size() == 3);
  CHECK(merged[0].path == std::filesystem::path("src/a.cpp"));
  CHECK(merged[0].kind == uburu::SearchResultKind::fileName);
  CHECK(merged[1].path == std::filesystem::path("src/a.cpp"));
  CHECK(merged[1].kind == uburu::SearchResultKind::content);
  CHECK(merged[2].path == std::filesystem::path("src/b.cpp"));
}

TEST_CASE("search result merge removes duplicate indexed matches")
{
  const auto duplicate = result(uburu::SearchResultKind::content, "src/a.cpp", 2, 1, "aaa");
  const std::vector indexedResults{
    duplicate,
    result(uburu::SearchResultKind::content, "src/b.cpp", 1, 1, "bbb"),
  };
  const std::vector directResults{
    duplicate,
  };

  const auto merged = uburu::search::mergeSearchResults(indexedResults, directResults, generousResultLimit);

  REQUIRE(merged.size() == 2);
  CHECK(merged[0].path == std::filesystem::path("src/a.cpp"));
  CHECK(merged[1].path == std::filesystem::path("src/b.cpp"));
}

TEST_CASE("search result merge respects the result limit after ordering")
{
  const std::vector indexedResults{
    result(uburu::SearchResultKind::content, "src/c.cpp", 1, 1, "ccc"),
    result(uburu::SearchResultKind::content, "src/a.cpp", 1, 1, "aaa"),
  };
  const std::vector directResults{
    result(uburu::SearchResultKind::content, "src/b.cpp", 1, 1, "bbb"),
  };

  constexpr std::size_t limitedResultCount = 2;

  const auto merged = uburu::search::mergeSearchResults(indexedResults, directResults, limitedResultCount);

  REQUIRE(merged.size() == limitedResultCount);
  CHECK(merged[0].path == std::filesystem::path("src/a.cpp"));
  CHECK(merged[1].path == std::filesystem::path("src/b.cpp"));
}
