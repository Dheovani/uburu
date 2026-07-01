#include "app/services/search-service.hpp"

#include "app/services/adaptive-result-batcher.hpp"
#include "core/search/search-query-validation.hpp"
#include "core/search/search-result-merge.hpp"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <stdexcept>
#include <utility>
#include <vector>

namespace uburu::app
{
  namespace
  {

    constexpr std::uint64_t nanosecondsPerSecond = 1'000'000'000;

    [[nodiscard]] search::SearchSummary invalidQuerySummary(std::vector<search::SearchError> errors)
    {
      search::SearchSummary summary;
      summary.errors = std::move(errors);

      return summary;
    }

    [[nodiscard]] std::uint64_t ratePerSecond(std::uint64_t value, std::chrono::nanoseconds elapsed)
    {
      if (elapsed.count() <= 0)
        return 0;

      return value * nanosecondsPerSecond / static_cast<std::uint64_t>(elapsed.count());
    }

    void updateThroughputMetrics(diagnostics::SearchMetrics& metrics)
    {
      metrics.filesPerSecond = ratePerSecond(metrics.filesProcessed, metrics.totalTime);
      metrics.bytesPerSecond = ratePerSecond(metrics.bytesProcessed, metrics.totalTime);
    }

    [[nodiscard]] std::uint64_t stringMemoryBytes(const std::string& value)
    {
      return static_cast<std::uint64_t>(value.capacity());
    }

    [[nodiscard]] std::uint64_t pathMemoryBytes(const std::filesystem::path& path)
    {
      return static_cast<std::uint64_t>(path.native().size() * sizeof(std::filesystem::path::value_type));
    }

    [[nodiscard]] std::uint64_t stringVectorMemoryBytes(const std::vector<std::string>& values)
    {
      std::uint64_t memoryBytes = static_cast<std::uint64_t>(values.capacity() * sizeof(std::string));

      for (const auto& value : values)
        memoryBytes += stringMemoryBytes(value);

      return memoryBytes;
    }

    [[nodiscard]] std::uint64_t searchResultMemoryBytes(const SearchResult& result)
    {
      std::uint64_t memoryBytes = sizeof(SearchResult);

      memoryBytes += pathMemoryBytes(result.path);
      memoryBytes += pathMemoryBytes(result.searchRoot);
      memoryBytes += stringMemoryBytes(result.lineText);
      memoryBytes += static_cast<std::uint64_t>(result.highlights.capacity() * sizeof(MatchSpan));
      memoryBytes += stringVectorMemoryBytes(result.contextBefore);
      memoryBytes += stringVectorMemoryBytes(result.contextAfter);

      return memoryBytes;
    }

    [[nodiscard]] std::uint64_t searchResultsMemoryBytes(const std::vector<SearchResult>& results)
    {
      std::uint64_t memoryBytes = static_cast<std::uint64_t>(results.capacity() * sizeof(SearchResult));

      for (const auto& result : results)
        memoryBytes += searchResultMemoryBytes(result);

      return memoryBytes;
    }

    [[nodiscard]] SearchEventDto makeEvent(SearchRunId runId,
                                           SearchEventKind kind,
                                           search::SearchSummary summary,
                                           std::chrono::steady_clock::time_point startedAt)
    {
      SearchEventDto event;

      event.runId = runId;
      event.kind = kind;
      event.summary = toSearchSummaryDto(summary);
      event.elapsed = std::chrono::steady_clock::now() - startedAt;

      return event;
    }

    [[nodiscard]] SearchEventDto makeResultBatchEvent(SearchRunId runId,
                                                      std::vector<SearchResultDto> results,
                                                      std::chrono::steady_clock::time_point startedAt)
    {
      SearchEventDto event;

      event.runId = runId;
      event.kind = SearchEventKind::resultBatch;
      event.results = std::move(results);
      event.elapsed = std::chrono::steady_clock::now() - startedAt;

      return event;
    }

    [[nodiscard]] SearchEventKind completionEventKind(const search::SearchSummary& summary)
    {
      if (!summary.errors.empty())
        return SearchEventKind::failed;

      if (summary.cancelled)
        return SearchEventKind::cancelled;

      return SearchEventKind::completed;
    }

    bool emitResult(SearchResult result, search::ResultSink& sink, std::vector<SearchResult>& emittedResults)
    {
      if (!sink(result))
        return false;

      emittedResults.push_back(std::move(result));

      return true;
    }

    bool emitIndexedResults(const std::vector<SearchResult>& indexedResults,
                            const SearchQuery& query,
                            search::ResultSink& sink,
                            std::vector<SearchResult>& emittedResults)
    {
      for (const auto& result : indexedResults) {
        if (emittedResults.size() >= query.options.resultLimit)
          return false;

        if (!emitResult(result, sink, emittedResults))
          return false;
      }

      return true;
    }

    [[nodiscard]] search::SearchSummary
    indexedSummary(const std::vector<SearchResult>& emittedResults, const SearchQuery& query, std::stop_token stopToken)
    {
      search::SearchSummary summary;
      summary.matches = emittedResults.size();
      summary.cancelled = stopToken.stop_requested();
      summary.limitReached = emittedResults.size() >= query.options.resultLimit;
      summary.metrics.resultsEmitted = emittedResults.size();
      summary.metrics.cacheHits = emittedResults.size();

      return summary;
    }

    bool emitDirectRefinements(const std::vector<SearchResult>& directResults,
                               const SearchQuery& query,
                               search::ResultSink& sink,
                               std::vector<SearchResult>& emittedResults)
    {
      const auto refinement = search::refineSearchResults(emittedResults, directResults, query.options.resultLimit);

      for (const auto& result : refinement.added) {
        if (!emitResult(result, sink, emittedResults))
          return false;
      }

      return true;
    }

  } // namespace

  DefaultSearchService::DefaultSearchService(std::shared_ptr<const search::SearchEngine> directEngine)
    : DefaultSearchService(std::move(directEngine), nullptr, SearchServiceOptions{.strategy = SearchStrategy::direct})
  {}

  DefaultSearchService::DefaultSearchService(std::shared_ptr<const search::SearchEngine> directEngine,
                                             std::shared_ptr<const index::IndexService> indexService)
    : DefaultSearchService(std::move(directEngine),
                           std::move(indexService),
                           SearchServiceOptions{.strategy = SearchStrategy::hybrid})
  {}

  DefaultSearchService::DefaultSearchService(std::shared_ptr<const search::SearchEngine> directEngine,
                                             std::shared_ptr<const index::IndexService> indexService,
                                             SearchServiceOptions options)
    : directEngine(std::move(directEngine)), indexService(std::move(indexService)), options(options)
  {
    if (!this->directEngine)
      throw std::invalid_argument("DefaultSearchService requires a direct engine");

    if (this->options.strategy != SearchStrategy::direct && !this->indexService)
      throw std::invalid_argument("DefaultSearchService requires an index service for indexed strategies");
  }

  void DefaultSearchService::finalizeRuntimeMetrics(search::SearchSummary& summary,
                                                    std::chrono::steady_clock::time_point startedAt,
                                                    std::uint64_t approximateMemoryBytes) const
  {
    summary.metrics.totalTime = std::chrono::steady_clock::now() - startedAt;
    summary.metrics.approximateMemoryBytes = approximateMemoryBytes;
    updateThroughputMetrics(summary.metrics);

    std::lock_guard lock(metricsMutex);

    if (approximateMemoryBytes > previousApproximateMemoryBytes) {
      summary.metrics.memoryGrowthBytes = approximateMemoryBytes - previousApproximateMemoryBytes;
      summary.metrics.memoryIncreased = previousApproximateMemoryBytes > 0;
    }

    previousApproximateMemoryBytes = approximateMemoryBytes;
  }

  search::SearchSummary
  DefaultSearchService::search(const SearchQuery& query, search::ResultSink sink, std::stop_token stopToken) const
  {
    const auto startedAt = std::chrono::steady_clock::now();

    if (options.strategy == SearchStrategy::direct) {
      std::uint64_t approximateMemoryBytes = 0;
      auto summary = directEngine->search(
        query,
        [&](SearchResult result) {
          approximateMemoryBytes += searchResultMemoryBytes(result);

          return sink(std::move(result));
        },
        stopToken);

      finalizeRuntimeMetrics(summary, startedAt, approximateMemoryBytes);

      return summary;
    }

    auto validationErrors = search::validateSearchQuery(query);
    if (!validationErrors.empty()) {
      auto summary = invalidQuerySummary(std::move(validationErrors));
      finalizeRuntimeMetrics(summary, startedAt, 0);

      return summary;
    }

    std::vector<SearchResult> emittedResults;
    const auto indexedResults = indexService->search(query, stopToken);

    if (options.strategy == SearchStrategy::indexed) {
      static_cast<void>(emitIndexedResults(indexedResults, query, sink, emittedResults));
      auto summary = indexedSummary(emittedResults, query, stopToken);

      finalizeRuntimeMetrics(summary, startedAt, searchResultsMemoryBytes(emittedResults));

      return summary;
    }

    if (!emitIndexedResults(indexedResults, query, sink, emittedResults)) {
      auto summary = indexedSummary(emittedResults, query, stopToken);
      finalizeRuntimeMetrics(summary, startedAt, searchResultsMemoryBytes(emittedResults));

      return summary;
    }

    const auto indexedHitCount = emittedResults.size();
    std::vector<SearchResult> directResults;
    auto summary = directEngine->search(
      query,
      [&](SearchResult result) {
        directResults.push_back(std::move(result));

        return !stopToken.stop_requested();
      },
      stopToken);

    if (!emitDirectRefinements(directResults, query, sink, emittedResults))
      summary.cancelled = summary.cancelled || stopToken.stop_requested();

    summary.matches = emittedResults.size();
    summary.limitReached = summary.limitReached || emittedResults.size() >= query.options.resultLimit;
    summary.metrics.resultsEmitted = emittedResults.size();
    summary.metrics.cacheHits = indexedHitCount;
    summary.metrics.cacheMisses = emittedResults.size() - indexedHitCount;

    finalizeRuntimeMetrics(summary, startedAt, searchResultsMemoryBytes(emittedResults));

    return summary;
  }

  search::SearchSummary DefaultSearchService::searchWithEvents(const SearchQuery& query,
                                                               const SearchEventSink& sink,
                                                               SearchExecutionOptions executionOptions,
                                                               std::stop_token stopToken) const
  {
    const auto startedAt = std::chrono::steady_clock::now();
    AdaptiveResultBatcher batcher(executionOptions);
    std::vector<SearchResultDto> pendingResults;
    bool abortedBySink = false;
    bool observedFirstResult = false;
    std::uint64_t emittedResultCount = 0;
    std::chrono::nanoseconds timeToFirstResult{};

    pendingResults.reserve(batcher.currentBatchSize());

    const auto emitBatch = [&]() {
      if (pendingResults.empty())
        return true;

      auto event = makeResultBatchEvent(executionOptions.runId, std::move(pendingResults), startedAt);
      const auto deliveryStartedAt = std::chrono::steady_clock::now();
      const auto delivered = sink(event);
      const auto deliveryElapsed = std::chrono::steady_clock::now() - deliveryStartedAt;

      batcher.recordDeliveryLatency(deliveryElapsed);
      pendingResults.clear();
      pendingResults.reserve(batcher.currentBatchSize());

      return delivered;
    };

    if (!sink(makeEvent(executionOptions.runId, SearchEventKind::started, {}, startedAt))) {
      search::SearchSummary cancelledSummary;
      cancelledSummary.cancelled = true;

      return cancelledSummary;
    }

    auto summary = search(
      query,
      [&](SearchResult result) {
        if (!observedFirstResult) {
          observedFirstResult = true;
          timeToFirstResult = std::chrono::steady_clock::now() - startedAt;
        }

        pendingResults.push_back(toSearchResultDto(result));
        ++emittedResultCount;

        if (pendingResults.size() < batcher.currentBatchSize())
          return true;

        if (emitBatch())
          return true;

        abortedBySink = true;

        return false;
      },
      stopToken);

    if (!emitBatch())
      abortedBySink = true;

    if (abortedBySink)
      summary.cancelled = true;

    summary.metrics.timeToFirstResult = timeToFirstResult;
    summary.metrics.totalTime = std::chrono::steady_clock::now() - startedAt;
    summary.metrics.resultsEmitted = emittedResultCount;
    updateThroughputMetrics(summary.metrics);
    const auto completionKind = completionEventKind(summary);

    if (!sink(makeEvent(executionOptions.runId, completionKind, summary, startedAt)))
      summary.cancelled = true;

    return summary;
  }

} // namespace uburu::app
