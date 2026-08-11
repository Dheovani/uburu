#include "app/services/search-service.hpp"

#include "app/services/adaptive-result-batcher.hpp"
#include "core/search/search-query-validation.hpp"
#include "core/search/search-result-memory.hpp"
#include "core/search/search-result-merge.hpp"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <span>
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
      if (summary.cancelled)
        return SearchEventKind::cancelled;

      if (!summary.errors.empty() && !summary.partialFailure)
        return SearchEventKind::failed;

      return SearchEventKind::completed;
    }

    [[nodiscard]] search::SearchSummary cancelledSearchSummary()
    {
      search::SearchSummary summary;
      summary.cancelled = true;

      return summary;
    }

    struct IndexedPublication
    {
      std::size_t resultCount{0};
      std::uint64_t resultMemoryBytes{0};
      bool stoppedBySink{false};
    };

    IndexedPublication emitIndexedResults(std::span<const SearchResult> indexedResults, search::ResultSink& sink)
    {
      IndexedPublication publication;

      for (const auto& result : indexedResults) {
        if (!sink(result)) {
          publication.stoppedBySink = true;

          break;
        }

        ++publication.resultCount;
        publication.resultMemoryBytes += search::approximateSearchResultMemoryBytes(result);
      }

      return publication;
    }

    [[nodiscard]]
    search::SearchSummary indexedSummary(
      const index::IndexSearchResult& indexSearchResult,
      const IndexedPublication& publication,
      std::stop_token stopToken)
    {
      search::SearchSummary summary;
      summary.matches = publication.resultCount;
      summary.resultMemoryBytes = publication.resultMemoryBytes;
      summary.cancelled = stopToken.stop_requested() || publication.stoppedBySink;
      summary.limitReached = indexSearchResult.resultLimitReached;
      summary.memoryLimitReached = indexSearchResult.memoryLimitReached;
      summary.metrics.resultsEmitted = publication.resultCount;
      summary.metrics.cacheHits = publication.resultCount;

      return summary;
    }

  } // namespace

  DefaultSearchService::DefaultSearchService(std::shared_ptr<const search::SearchEngine> directEngine)
    : DefaultSearchService(std::move(directEngine), nullptr, SearchServiceOptions{.strategy = SearchStrategy::direct})
  {}

  DefaultSearchService::DefaultSearchService(std::shared_ptr<const search::SearchEngine> directEngine,
                                             std::shared_ptr<const index::IndexService> indexService)
    : DefaultSearchService(
        std::move(directEngine), std::move(indexService), SearchServiceOptions{.strategy = SearchStrategy::hybrid})
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
      auto summary = directEngine->search(query, std::move(sink), stopToken);

      finalizeRuntimeMetrics(summary, startedAt, summary.resultMemoryBytes);

      return summary;
    }

    auto validationErrors = search::validateSearchQuery(query);
    if (!validationErrors.empty()) {
      auto summary = invalidQuerySummary(std::move(validationErrors));
      finalizeRuntimeMetrics(summary, startedAt, 0);

      return summary;
    }

    if (stopToken.stop_requested()) {
      auto summary = cancelledSearchSummary();
      finalizeRuntimeMetrics(summary, startedAt, 0);

      return summary;
    }

    auto indexSearchResult = indexService->search(query, stopToken);

    if (options.strategy == SearchStrategy::indexed) {
      const auto publication = emitIndexedResults(indexSearchResult.results, sink);
      auto summary = indexedSummary(indexSearchResult, publication, stopToken);

      finalizeRuntimeMetrics(summary, startedAt, indexSearchResult.resultMemoryBytes);

      return summary;
    }

    auto indexedResults = std::move(indexSearchResult.results);
    search::sortAndRemoveDuplicateSearchResults(indexedResults);
    std::size_t emittedResultCount = 0;
    std::uint64_t emittedResultMemoryBytes = 0;
    std::uint64_t cacheHits = 0;
    std::uint64_t cacheMisses = 0;
    auto summary = directEngine->search(
      query,
      [&](SearchResult result) {
        const auto cacheHit = search::orderedSearchResultsContain(indexedResults, result);
        const auto resultMemoryBytes = search::approximateSearchResultMemoryBytes(result);

        if (!sink(std::move(result)))
          return false;

        if (cacheHit)
          ++cacheHits;
        else
          ++cacheMisses;

        ++emittedResultCount;
        emittedResultMemoryBytes += resultMemoryBytes;

        return !stopToken.stop_requested();
      },
      stopToken);

    summary.cancelled = summary.cancelled || stopToken.stop_requested();
    summary.matches = emittedResultCount;
    summary.resultMemoryBytes = emittedResultMemoryBytes;
    summary.limitReached = summary.limitReached || emittedResultCount >= query.options.resultLimit;
    summary.metrics.resultsEmitted = emittedResultCount;
    summary.metrics.cacheHits = cacheHits;
    summary.metrics.cacheMisses = cacheMisses;

    const auto retainedIndexMemoryBytes = search::approximateSearchResultsMemoryBytes(indexedResults);
    finalizeRuntimeMetrics(summary, startedAt, retainedIndexMemoryBytes + emittedResultMemoryBytes);

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
