#include "app/services/search-service.hpp"

#include "core/search/search-query-validation.hpp"
#include "core/search/search-result-merge.hpp"

#include <chrono>
#include <cstdint>
#include <stdexcept>
#include <utility>
#include <vector>

namespace uburu::app
{
  namespace
  {

    [[nodiscard]] search::SearchSummary invalidQuerySummary(std::vector<search::SearchError> errors)
    {
      search::SearchSummary summary;
      summary.errors = std::move(errors);

      return summary;
    }

    [[nodiscard]] std::size_t normalizedBatchSize(std::size_t batchSize)
    {
      if (batchSize == 0)
        return 1;

      return batchSize;
    }

    [[nodiscard]] SearchEvent makeEvent(SearchRunId runId,
                                        SearchEventKind kind,
                                        search::SearchSummary summary,
                                        std::chrono::steady_clock::time_point startedAt)
    {
      return SearchEvent{.runId = runId,
                         .kind = kind,
                         .results = {},
                         .summary = std::move(summary),
                         .elapsed = std::chrono::steady_clock::now() - startedAt};
    }

    [[nodiscard]] SearchEvent makeResultBatchEvent(SearchRunId runId,
                                                   std::vector<SearchResult> results,
                                                   std::chrono::steady_clock::time_point startedAt)
    {
      return SearchEvent{.runId = runId,
                         .kind = SearchEventKind::resultBatch,
                         .results = std::move(results),
                         .summary = {},
                         .elapsed = std::chrono::steady_clock::now() - startedAt};
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

  search::SearchSummary
  DefaultSearchService::search(const SearchQuery& query, search::ResultSink sink, std::stop_token stopToken) const
  {
    if (options.strategy == SearchStrategy::direct)
      return directEngine->search(query, std::move(sink), stopToken);

    auto validationErrors = search::validateSearchQuery(query);
    if (!validationErrors.empty())
      return invalidQuerySummary(std::move(validationErrors));

    std::vector<SearchResult> emittedResults;
    const auto indexedResults = indexService->search(query, stopToken);

    if (options.strategy == SearchStrategy::indexed) {
      static_cast<void>(emitIndexedResults(indexedResults, query, sink, emittedResults));

      return indexedSummary(emittedResults, query, stopToken);
    }

    if (!emitIndexedResults(indexedResults, query, sink, emittedResults)) {
      return indexedSummary(emittedResults, query, stopToken);
    }

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

    return summary;
  }

  search::SearchSummary DefaultSearchService::searchWithEvents(const SearchQuery& query,
                                                               const SearchEventSink& sink,
                                                               SearchExecutionOptions executionOptions,
                                                               std::stop_token stopToken) const
  {
    const auto startedAt = std::chrono::steady_clock::now();
    auto batchSize = normalizedBatchSize(executionOptions.resultBatchSize);
    std::vector<SearchResult> pendingResults;
    bool abortedBySink = false;
    bool observedFirstResult = false;
    std::uint64_t emittedResultCount = 0;
    std::chrono::nanoseconds timeToFirstResult{};

    pendingResults.reserve(batchSize);

    const auto emitBatch = [&]() {
      if (pendingResults.empty())
        return true;

      auto event = makeResultBatchEvent(executionOptions.runId, std::move(pendingResults), startedAt);
      pendingResults.clear();
      pendingResults.reserve(batchSize);

      return sink(event);
    };

    if (!sink(makeEvent(executionOptions.runId, SearchEventKind::started, {}, startedAt)))
      return search::SearchSummary{.cancelled = true};

    auto summary = search(
      query,
      [&](SearchResult result) {
        if (!observedFirstResult) {
          observedFirstResult = true;
          timeToFirstResult = std::chrono::steady_clock::now() - startedAt;
        }

        pendingResults.push_back(std::move(result));
        ++emittedResultCount;

        if (pendingResults.size() < batchSize)
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
    const auto completionKind = completionEventKind(summary);

    if (!sink(makeEvent(executionOptions.runId, completionKind, summary, startedAt)))
      summary.cancelled = true;

    return summary;
  }

} // namespace uburu::app
