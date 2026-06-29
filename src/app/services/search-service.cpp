#include "app/services/search-service.hpp"

#include <stdexcept>

namespace uburu::app
{

  DefaultSearchService::DefaultSearchService(std::shared_ptr<const search::SearchEngine> directEngine)
      : directEngine(std::move(directEngine))
  {
    if (!directEngine)
      throw std::invalid_argument("DefaultSearchService requires a direct engine");
  }

  search::SearchSummary DefaultSearchService::search(const SearchQuery& query, search::ResultSink sink,
                                                     std::stop_token stop_token) const
  {
    // Indexed-first refinement will be orchestrated here when IndexService is implemented.
    return directEngine->search(query, std::move(sink), stop_token);
  }

} // namespace uburu::app
