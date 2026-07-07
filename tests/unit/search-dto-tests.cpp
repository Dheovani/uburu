#include "app/dto/search-dto.hpp"

#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <filesystem>
#include <string>
#include <utility>
#include <vector>

TEST_CASE("search result dto keeps presentation fields without exposing core result type")
{
  uburu::SearchResult result;

  result.kind = uburu::SearchResultKind::fileName;
  result.path = "src/main.cpp";
  result.line = 7;
  result.column = 3;
  result.matchLength = 5;
  result.lineText = "main";
  result.highlights = {uburu::MatchSpan{.column = 3, .byteOffset = 2, .byteLength = 5}};
  result.contextBefore = {"before"};
  result.contextAfter = {"after"};
  result.searchRoot = "repo";

  const auto dto = uburu::app::toSearchResultDto(result);

  CHECK(dto.kind == uburu::app::SearchResultKindDto::fileName);
  CHECK(dto.path == std::filesystem::path("src/main.cpp"));
  CHECK(dto.line == 7);
  CHECK(dto.column == 3);
  CHECK(dto.matchLength == 5);
  CHECK(dto.lineText == "main");
  REQUIRE(dto.highlights.size() == 1);
  CHECK(dto.highlights.front().column == 3);
  CHECK(dto.highlights.front().byteOffset == 2);
  CHECK(dto.highlights.front().byteLength == 5);
  CHECK(dto.contextBefore == std::vector<std::string>{"before"});
  CHECK(dto.contextAfter == std::vector<std::string>{"after"});
  CHECK(dto.searchRoot == std::filesystem::path("repo"));
}

TEST_CASE("search summary dto maps internal enum values to stable names")
{
  uburu::search::SearchSummary summary;
  summary.filesScanned = 11;
  summary.matches = 4;
  summary.filesWithMatchLimitReached = 2;
  summary.filesWithReadErrors = 1;
  summary.partialFailure = true;
  summary.regexExecutionMode = uburu::search::RegexExecutionMode::jit;
  summary.metrics.filesPerSecond = 17;
  summary.metrics.bytesPerSecond = 255;
  summary.metrics.resultsEmitted = 4;
  summary.metrics.totalTime = std::chrono::milliseconds(9);
  summary.metrics.cacheHits = 3;
  summary.metrics.cacheMisses = 1;
  summary.metrics.reusedByHash = 2;
  summary.metrics.approximateMemoryBytes = 1024;
  summary.metrics.memoryGrowthBytes = 64;
  summary.metrics.memoryIncreased = true;

  uburu::search::SearchError error;

  error.code = uburu::search::SearchErrorCode::rootUnavailable;
  error.translationKey = "search.error.rootUnavailable";
  error.context = "missing network share";
  error.offset = 3;
  summary.errors.push_back(std::move(error));

  const auto dto = uburu::app::toSearchSummaryDto(summary);

  CHECK(dto.filesScanned == 11);
  CHECK(dto.matches == 4);
  CHECK(dto.filesWithMatchLimitReached == 2);
  CHECK(dto.filesWithReadErrors == 1);
  CHECK(dto.partialFailure);
  CHECK(dto.regexExecutionMode == "jit");
  CHECK(dto.metrics.filesPerSecond == 17);
  CHECK(dto.metrics.bytesPerSecond == 255);
  CHECK(dto.metrics.resultsEmitted == 4);
  CHECK(dto.metrics.totalTime == std::chrono::milliseconds(9));
  CHECK(dto.metrics.cacheHits == 3);
  CHECK(dto.metrics.cacheMisses == 1);
  CHECK(dto.metrics.reusedByHash == 2);
  CHECK(dto.metrics.approximateMemoryBytes == 1024);
  CHECK(dto.metrics.memoryGrowthBytes == 64);
  CHECK(dto.metrics.memoryIncreased);
  REQUIRE(dto.errors.size() == 1);
  CHECK(dto.errors.front().code == "rootUnavailable");
  CHECK(dto.errors.front().translationKey == "search.error.rootUnavailable");
  CHECK(dto.errors.front().context == "missing network share");
  CHECK(dto.errors.front().hasOffset);
  CHECK(dto.errors.front().offset == 3);
}
