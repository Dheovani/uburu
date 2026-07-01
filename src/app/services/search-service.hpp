#pragma once

#include "app/dto/search-dto.hpp"
#include "core/index/index-service.hpp"
#include "core/search/search-engine.hpp"

#include <memory>

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
    [[nodiscard]] search::SearchSummary search(const SearchQuery& query,
                                               search::ResultSink sink,
                                               std::stop_token stopToken = {}) const override;
    [[nodiscard]] search::SearchSummary searchWithEvents(const SearchQuery& query,
                                                         const SearchEventSink& sink,
                                                         SearchExecutionOptions executionOptions = {},
                                                         std::stop_token stopToken = {}) const override;

  private:
    std::shared_ptr<const search::SearchEngine> directEngine;
    std::shared_ptr<const index::IndexService> indexService;
    SearchServiceOptions options;
  };

} // namespace uburu::app
