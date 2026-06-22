#pragma once

#include "core/search/search-engine.hpp"

#include <memory>

namespace uburu::app
{

  class SearchService
  {
  public:
    virtual ~SearchService() = default;
    [[nodiscard]] virtual search::SearchSummary search(const SearchQuery& query,
                                                       search::ResultSink sink,
                                                       std::stop_token stop_token = {}) const = 0;
  };

  class DefaultSearchService final : public SearchService
  {
  public:
    explicit DefaultSearchService(std::shared_ptr<const search::SearchEngine> direct_engine);
    [[nodiscard]] search::SearchSummary search(const SearchQuery& query, search::ResultSink sink,
                                               std::stop_token stop_token = {}) const override;

  private:
    std::shared_ptr<const search::SearchEngine> direct_engine_;
  };

} // namespace uburu::app
