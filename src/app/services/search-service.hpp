#pragma once

#include "app/dto/search-dto.hpp"
#include "core/index/index-service.hpp"
#include "core/search/search-engine.hpp"

#include <chrono>
#include <cstdint>
#include <memory>
#include <mutex>

namespace uburu::app
{

  enum class SearchStrategy
  {
    direct,
    indexed,
    hybrid
  };

  struct SearchServiceOptions
  {
    SearchStrategy strategy{SearchStrategy::direct};
  };

  class SearchService
  {
  public:
    virtual ~SearchService() = default;
    [[nodiscard]] virtual search::SearchSummary
    search(const SearchQuery& query, search::ResultSink sink, std::stop_token stopToken = {}) const = 0;
    [[nodiscard]] virtual search::SearchSummary searchWithEvents(const SearchQuery& query,
                                                                 const SearchEventSink& sink,
                                                                 SearchExecutionOptions executionOptions = {},
                                                                 std::stop_token stopToken = {}) const = 0;
  };

  class DefaultSearchService final : public SearchService
  {
  public:
    explicit DefaultSearchService(std::shared_ptr<const search::SearchEngine> directEngine);
    DefaultSearchService(std::shared_ptr<const search::SearchEngine> directEngine,
                         std::shared_ptr<const index::IndexService> indexService);
    DefaultSearchService(std::shared_ptr<const search::SearchEngine> directEngine,
                         std::shared_ptr<const index::IndexService> indexService,
                         SearchServiceOptions options);
    [[nodiscard]] search::SearchSummary
    search(const SearchQuery& query, search::ResultSink sink, std::stop_token stopToken = {}) const override;
    [[nodiscard]] search::SearchSummary searchWithEvents(const SearchQuery& query,
                                                         const SearchEventSink& sink,
                                                         SearchExecutionOptions executionOptions = {},
                                                         std::stop_token stopToken = {}) const override;

  private:
    void finalizeRuntimeMetrics(search::SearchSummary& summary,
                                std::chrono::steady_clock::time_point startedAt,
                                std::uint64_t approximateMemoryBytes) const;

    std::shared_ptr<const search::SearchEngine> directEngine;
    std::shared_ptr<const index::IndexService> indexService;
    SearchServiceOptions options;
    mutable std::mutex metricsMutex;
    mutable std::uint64_t previousApproximateMemoryBytes{0};
  };

} // namespace uburu::app
