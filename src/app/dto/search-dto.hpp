#pragma once

#include "core/search/search-engine.hpp"
#include "shared/types/domain-types.hpp"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <string>
#include <vector>

namespace uburu::app
{

  using SearchRunId = std::uint64_t;
  inline constexpr std::size_t defaultResultBatchSize = 64;
  inline constexpr std::size_t defaultMinimumResultBatchSize = 16;
  inline constexpr std::size_t defaultMaximumResultBatchSize = 512;
  inline constexpr std::chrono::milliseconds defaultTargetBatchDeliveryLatency{16};

  enum class SearchResultKindDto
  {
    content,
    fileName
  };

  enum class SearchEventKind
  {
    started,
    resultBatch,
    completed,
    cancelled,
    failed
  };

  struct SearchExecutionOptions
  {
    SearchRunId runId{0};
    std::size_t resultBatchSize{defaultResultBatchSize};
    bool adaptiveBatching{true};
    std::size_t minimumResultBatchSize{defaultMinimumResultBatchSize};
    std::size_t maximumResultBatchSize{defaultMaximumResultBatchSize};
    std::chrono::milliseconds targetBatchDeliveryLatency{defaultTargetBatchDeliveryLatency};
  };

  struct SearchMatchDto
  {
    std::size_t column{0};
    std::size_t byteOffset{0};
    std::size_t byteLength{0};
  };

  struct SearchResultDto
  {
    SearchResultKindDto kind{SearchResultKindDto::content};
    std::filesystem::path path;
    std::size_t line{0};
    std::size_t column{0};
    std::size_t matchLength{0};
    std::string lineText;
    std::vector<SearchMatchDto> highlights;
    std::vector<std::string> contextBefore;
    std::vector<std::string> contextAfter;
    std::filesystem::path searchRoot;
  };

  struct SearchErrorDto
  {
    std::string code;
    std::string translationKey;
    std::string context;
    bool hasOffset{false};
    std::size_t offset{0};
  };

  struct SearchMetricsDto
  {
    std::chrono::nanoseconds timeToFirstResult{};
    std::chrono::nanoseconds totalTime{};
    std::uint64_t filesProcessed{0};
    std::uint64_t bytesProcessed{0};
    std::uint64_t resultsEmitted{0};
    std::uint64_t ignoredFiles{0};
    std::uint64_t hiddenFiles{0};
    std::uint64_t binaryFiles{0};
    std::uint64_t binaryFilesSkipped{0};
  };

  struct SearchSummaryDto
  {
    std::size_t filesScanned{0};
    std::size_t matches{0};
    std::size_t filesWithMatchLimitReached{0};
    std::size_t filesWithReadErrors{0};
    bool cancelled{false};
    bool limitReached{false};
    bool partialFailure{false};
    std::vector<SearchErrorDto> errors;
    std::string regexExecutionMode;
    SearchMetricsDto metrics;
  };

  struct SearchEventDto
  {
    SearchRunId runId{0};
    SearchEventKind kind{SearchEventKind::started};
    std::vector<SearchResultDto> results;
    SearchSummaryDto summary;
    std::chrono::nanoseconds elapsed{};
  };

  using SearchEventSink = std::function<bool(const SearchEventDto&)>;

  [[nodiscard]] SearchResultKindDto toSearchResultKindDto(SearchResultKind kind);
  [[nodiscard]] SearchResultKind toCoreSearchResultKind(SearchResultKindDto kind);
  [[nodiscard]] SearchMatchDto toSearchMatchDto(const MatchSpan& match);
  [[nodiscard]] SearchResultDto toSearchResultDto(const SearchResult& result);
  [[nodiscard]] SearchMetricsDto toSearchMetricsDto(const diagnostics::SearchMetrics& metrics);
  [[nodiscard]] SearchErrorDto toSearchErrorDto(const search::SearchError& error);
  [[nodiscard]] SearchSummaryDto toSearchSummaryDto(const search::SearchSummary& summary);

} // namespace uburu::app
