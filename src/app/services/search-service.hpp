#pragma once

#include "core/index/index-service.hpp"
#include "core/search/search-engine.hpp"

#include <memory>

namespace uburu::app
{

  class SearchService
  {
  public:
    virtual ~SearchService() = default;
    [[nodiscard]] virtual search::SearchSummary
    search(const SearchQuery& query, search::ResultSink sink, std::stop_token stop_token = {}) const = 0;
  };

  class DefaultSearchService final : public SearchService
  {
  public:
    explicit DefaultSearchService(std::shared_ptr<const search::SearchEngine> directEngine);
    DefaultSearchService(std::shared_ptr<const search::SearchEngine> directEngine,
                         std::shared_ptr<const index::IndexService> indexService);
    [[nodiscard]] search::SearchSummary
    search(const SearchQuery& query, search::ResultSink sink, std::stop_token stop_token = {}) const override;

  private:
    std::shared_ptr<const search::SearchEngine> directEngine;
    std::shared_ptr<const index::IndexService> indexService;
  };

} // namespace uburu::app
