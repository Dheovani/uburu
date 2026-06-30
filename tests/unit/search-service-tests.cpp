#include "app/services/search-service.hpp"

#include <catch2/catch_test_macros.hpp>

#include <memory>
#include <stdexcept>

namespace
{

  class FakeSearchEngine final : public uburu::search::SearchEngine
  {
  public:
    [[nodiscard]] uburu::search::SearchSummary search(const uburu::SearchQuery&,
                                                      uburu::search::ResultSink,
                                                      std::stop_token = {}) const override
    {
      uburu::search::SearchSummary summary;
      summary.filesScanned = scannedFiles;

      return summary;
    }

    std::size_t scannedFiles{7};
  };

} // namespace

TEST_CASE("default search service accepts a valid direct engine")
{
  auto engine = std::make_shared<FakeSearchEngine>();

  const uburu::app::DefaultSearchService service(engine);
  const auto summary = service.search(uburu::SearchQuery{}, [](uburu::SearchResult) { return true; });

  CHECK(summary.filesScanned == 7);
}

TEST_CASE("default search service rejects a missing direct engine")
{
  CHECK_THROWS_AS(uburu::app::DefaultSearchService(nullptr), std::invalid_argument);
}
