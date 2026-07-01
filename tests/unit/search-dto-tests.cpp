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
  summary.metrics.resultsEmitted = 4;
  summary.metrics.totalTime = std::chrono::milliseconds(9);

  uburu::search::SearchError error;

  error.code = uburu::search::SearchErrorCode::regexCompileFailed;
  error.translationKey = "search.error.regex.compile";
  error.context = "bad regex";
  error.offset = 3;
  summary.errors.push_back(std::move(error));

  const auto dto = uburu::app::toSearchSummaryDto(summary);

  CHECK(dto.filesScanned == 11);
  CHECK(dto.matches == 4);
  CHECK(dto.filesWithMatchLimitReached == 2);
  CHECK(dto.filesWithReadErrors == 1);
  CHECK(dto.partialFailure);
  CHECK(dto.regexExecutionMode == "jit");
  CHECK(dto.metrics.resultsEmitted == 4);
  CHECK(dto.metrics.totalTime == std::chrono::milliseconds(9));
  REQUIRE(dto.errors.size() == 1);
  CHECK(dto.errors.front().code == "regexCompileFailed");
  CHECK(dto.errors.front().translationKey == "search.error.regex.compile");
  CHECK(dto.errors.front().context == "bad regex");
  CHECK(dto.errors.front().hasOffset);
  CHECK(dto.errors.front().offset == 3);
}
