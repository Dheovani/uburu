#include "app/services/search-service.hpp"

#include <stdexcept>

namespace uburu::app
{

  DefaultSearchService::DefaultSearchService(
      std::shared_ptr<const search::SearchEngine> direct_engine)
      : direct_engine_(std::move(direct_engine))
  {
    if (!direct_engine_)
      throw std::invalid_argument("DefaultSearchService requires a direct engine");
  }

  search::SearchSummary DefaultSearchService::search(const SearchQuery& query,
                                                     search::ResultSink sink,
                                                     std::stop_token stop_token) const
  {
    // Indexed-first refinement will be orchestrated here when IndexService is implemented.
    return direct_engine_->search(query, std::move(sink), stop_token);
  }

} // namespace uburu::app
