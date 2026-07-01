#include "app/services/search-service.hpp"

#include "core/search/search-query-validation.hpp"
#include "core/search/search-result-merge.hpp"

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
    : DefaultSearchService(std::move(directEngine), nullptr)
  {}

  DefaultSearchService::DefaultSearchService(std::shared_ptr<const search::SearchEngine> directEngine,
                                             std::shared_ptr<const index::IndexService> indexService)
    : directEngine(std::move(directEngine)), indexService(std::move(indexService))
  {
    if (!this->directEngine)
      throw std::invalid_argument("DefaultSearchService requires a direct engine");
  }

  search::SearchSummary
  DefaultSearchService::search(const SearchQuery& query, search::ResultSink sink, std::stop_token stop_token) const
  {
    if (!indexService)
      return directEngine->search(query, std::move(sink), stop_token);

    auto validationErrors = search::validateSearchQuery(query);
    if (!validationErrors.empty())
      return invalidQuerySummary(std::move(validationErrors));

    std::vector<SearchResult> emittedResults;
    const auto indexedResults = indexService->search(query, stop_token);

    if (!emitIndexedResults(indexedResults, query, sink, emittedResults)) {
      search::SearchSummary summary;
      summary.matches = emittedResults.size();
      summary.cancelled = stop_token.stop_requested();
      summary.limitReached = emittedResults.size() >= query.options.resultLimit;
      summary.metrics.resultsEmitted = emittedResults.size();

      return summary;
    }

    std::vector<SearchResult> directResults;
    auto summary = directEngine->search(
      query,
      [&](SearchResult result) {
        directResults.push_back(std::move(result));

        return !stop_token.stop_requested();
      },
      stop_token);

    if (!emitDirectRefinements(directResults, query, sink, emittedResults))
      summary.cancelled = summary.cancelled || stop_token.stop_requested();

    summary.matches = emittedResults.size();
    summary.limitReached = summary.limitReached || emittedResults.size() >= query.options.resultLimit;
    summary.metrics.resultsEmitted = emittedResults.size();

    return summary;
  }

} // namespace uburu::app
