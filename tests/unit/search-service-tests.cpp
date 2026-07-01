#include "app/services/search-service.hpp"

#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <filesystem>
#include <memory>
#include <span>
#include <stdexcept>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

namespace
{

  class FakeSearchEngine final : public uburu::search::SearchEngine
  {
  public:
    [[nodiscard]] uburu::search::SearchSummary
    search(const uburu::SearchQuery&, uburu::search::ResultSink sink, std::stop_token = {}) const override
    {
      ++calls;

      for (const auto& result : results) {
        if (!sink(result))
          break;
      }

      uburu::search::SearchSummary summary;
      summary.filesScanned = scannedFiles;
      summary.matches = results.size();

      return summary;
    }

    mutable std::size_t calls{0};
    std::size_t scannedFiles{7};
    std::vector<uburu::SearchResult> results;
  };

  class FakeIndexService final : public uburu::index::IndexService
  {
  public:
    [[nodiscard]] uburu::index::IndexUpdateSummary update(const uburu::WorktreeInfo&,
                                                          std::span<const uburu::FileEntry>,
                                                          const uburu::index::IndexProgressCallback& = {},
                                                          std::stop_token = {}) override
    {
      return {};
    }

    [[nodiscard]] uburu::index::IndexUpdateSummary update(const uburu::WorktreeInfo&,
                                                          std::span<const uburu::index::IndexFileCandidate>,
                                                          const uburu::index::IndexProgressCallback& = {},
                                                          std::stop_token = {}) override
    {
      return {};
    }

    [[nodiscard]] uburu::index::IndexUpdateSummary update(const uburu::WorktreeInfo&,
                                                          std::span<const uburu::FileEntry>,
                                                          std::span<const uburu::GitOverlayEntry>,
                                                          const uburu::index::IndexProgressCallback& = {},
                                                          std::stop_token = {}) override
    {
      return {};
    }

    [[nodiscard]] uburu::index::IndexStalenessReport staleness(const uburu::WorktreeInfo&) const override
    {
      return {};
    }

    [[nodiscard]] std::vector<uburu::SearchResult> search(const uburu::SearchQuery&,
                                                          std::stop_token = {}) const override
    {
      ++calls;

      return results;
    }

    mutable std::size_t calls{0};
    std::vector<uburu::SearchResult> results;
  };

  class TemporaryDirectory
  {
  public:
    explicit TemporaryDirectory(std::string name)
      : pathValue(std::filesystem::temp_directory_path() / uniqueName(std::move(name)))
    {
      std::error_code error;

      std::filesystem::remove_all(pathValue, error);
      std::filesystem::create_directories(pathValue);
    }

    ~TemporaryDirectory()
    {
      std::error_code error;

      std::filesystem::remove_all(pathValue, error);
    }

    [[nodiscard]] const std::filesystem::path& path() const
    {
      return pathValue;
    }

  private:
    [[nodiscard]] static std::string uniqueName(std::string name)
    {
      const auto now = std::chrono::steady_clock::now().time_since_epoch().count();

      return name + "-" + std::to_string(now);
    }

    std::filesystem::path pathValue;
  };

  [[nodiscard]] uburu::SearchResult
  result(uburu::SearchResultKind kind, std::filesystem::path path, std::size_t line, std::string lineText)
  {
    return uburu::SearchResult{.kind = kind,
                               .path = std::move(path),
                               .line = line,
                               .column = 1,
                               .matchLength = 6,
                               .lineText = std::move(lineText),
                               .searchRoot = "repo"};
  }

  [[nodiscard]] uburu::SearchQuery validQuery(const std::filesystem::path& root)
  {
    return uburu::SearchQuery{.root = root, .expression = "needle", .options = {}};
  }

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

TEST_CASE("default search service uses direct strategy explicitly")
{
  TemporaryDirectory directory("uburu-search-service-direct-policy-test");
  auto directEngine = std::make_shared<FakeSearchEngine>();
  auto indexService = std::make_shared<FakeIndexService>();

  directEngine->results = {result(uburu::SearchResultKind::content, "src/direct.cpp", 1, "needle direct")};
  indexService->results = {result(uburu::SearchResultKind::content, "src/indexed.cpp", 1, "needle indexed")};

  const uburu::app::DefaultSearchService service(
    directEngine, indexService, uburu::app::SearchServiceOptions{.strategy = uburu::app::SearchStrategy::direct});
  std::vector<uburu::SearchResult> emittedResults;
  const auto summary = service.search(validQuery(directory.path()), [&](uburu::SearchResult searchResult) {
    emittedResults.push_back(std::move(searchResult));

    return true;
  });

  REQUIRE(emittedResults.size() == 1);
  CHECK(emittedResults.front().path == std::filesystem::path("src/direct.cpp"));
  CHECK(summary.filesScanned == directEngine->scannedFiles);
  CHECK(directEngine->calls == 1);
  CHECK(indexService->calls == 0);
}

TEST_CASE("default search service uses indexed strategy explicitly")
{
  TemporaryDirectory directory("uburu-search-service-indexed-policy-test");
  auto directEngine = std::make_shared<FakeSearchEngine>();
  auto indexService = std::make_shared<FakeIndexService>();

  directEngine->results = {result(uburu::SearchResultKind::content, "src/direct.cpp", 1, "needle direct")};
  indexService->results = {result(uburu::SearchResultKind::content, "src/indexed.cpp", 1, "needle indexed")};

  const uburu::app::DefaultSearchService service(
    directEngine, indexService, uburu::app::SearchServiceOptions{.strategy = uburu::app::SearchStrategy::indexed});
  std::vector<uburu::SearchResult> emittedResults;
  const auto summary = service.search(validQuery(directory.path()), [&](uburu::SearchResult searchResult) {
    emittedResults.push_back(std::move(searchResult));

    return true;
  });

  REQUIRE(emittedResults.size() == 1);
  CHECK(emittedResults.front().path == std::filesystem::path("src/indexed.cpp"));
  CHECK(summary.matches == emittedResults.size());
  CHECK(summary.metrics.resultsEmitted == emittedResults.size());
  CHECK(directEngine->calls == 0);
  CHECK(indexService->calls == 1);
}

TEST_CASE("default search service rejects indexed strategies without an index service")
{
  auto directEngine = std::make_shared<FakeSearchEngine>();

  CHECK_THROWS_AS(
    uburu::app::DefaultSearchService(
      directEngine, nullptr, uburu::app::SearchServiceOptions{.strategy = uburu::app::SearchStrategy::indexed}),
    std::invalid_argument);
  CHECK_THROWS_AS(
    uburu::app::DefaultSearchService(
      directEngine, nullptr, uburu::app::SearchServiceOptions{.strategy = uburu::app::SearchStrategy::hybrid}),
    std::invalid_argument);
}

TEST_CASE("default search service emits indexed results before direct refinements")
{
  TemporaryDirectory directory("uburu-search-service-hybrid-test");
  auto directEngine = std::make_shared<FakeSearchEngine>();
  auto indexService = std::make_shared<FakeIndexService>();
  std::vector<uburu::SearchResult> emittedResults;

  const auto indexed = result(uburu::SearchResultKind::content, "src/indexed.cpp", 1, "needle indexed");
  const auto duplicate = result(uburu::SearchResultKind::content, "src/shared.cpp", 2, "needle shared");
  const auto directOnly = result(uburu::SearchResultKind::content, "src/direct.cpp", 3, "needle direct");

  indexService->results = {indexed, duplicate};
  directEngine->results = {duplicate, directOnly};

  const uburu::app::DefaultSearchService service(directEngine, indexService);
  const auto summary = service.search(validQuery(directory.path()), [&](uburu::SearchResult searchResult) {
    emittedResults.push_back(std::move(searchResult));

    return true;
  });

  REQUIRE(emittedResults.size() == 3);
  CHECK(emittedResults[0].path == std::filesystem::path("src/indexed.cpp"));
  CHECK(emittedResults[1].path == std::filesystem::path("src/shared.cpp"));
  CHECK(emittedResults[2].path == std::filesystem::path("src/direct.cpp"));
  CHECK(summary.matches == emittedResults.size());
  CHECK(summary.metrics.resultsEmitted == emittedResults.size());
  CHECK(indexService->calls == 1);
  CHECK(directEngine->calls == 1);
}

TEST_CASE("default search service validates hybrid queries before consulting sources")
{
  auto directEngine = std::make_shared<FakeSearchEngine>();
  auto indexService = std::make_shared<FakeIndexService>();
  const uburu::app::DefaultSearchService service(directEngine, indexService);

  uburu::SearchQuery query{.root = "missing-root", .expression = "needle", .options = {}};
  const auto summary = service.search(query, [](uburu::SearchResult) { return true; });

  CHECK_FALSE(summary.errors.empty());
  CHECK(indexService->calls == 0);
  CHECK(directEngine->calls == 0);
}

TEST_CASE("default search service emits run scoped result batches")
{
  constexpr uburu::app::SearchRunId runId = 42;
  constexpr std::size_t batchSize = 2;

  auto directEngine = std::make_shared<FakeSearchEngine>();
  std::vector<uburu::app::SearchEventDto> events;

  directEngine->results = {
    result(uburu::SearchResultKind::content, "src/a.cpp", 1, "needle a"),
    result(uburu::SearchResultKind::content, "src/b.cpp", 2, "needle b"),
    result(uburu::SearchResultKind::content, "src/c.cpp", 3, "needle c"),
  };

  const uburu::app::DefaultSearchService service(directEngine);
  const auto summary = service.searchWithEvents(
    uburu::SearchQuery{},
    [&](const uburu::app::SearchEventDto& event) {
      events.push_back(event);

      return true;
    },
    uburu::app::SearchExecutionOptions{.runId = runId, .resultBatchSize = batchSize, .adaptiveBatching = false});

  REQUIRE(events.size() == 4);
  CHECK(events[0].kind == uburu::app::SearchEventKind::started);
  CHECK(events[1].kind == uburu::app::SearchEventKind::resultBatch);
  CHECK(events[1].results.size() == batchSize);
  CHECK(events[1].results.front().kind == uburu::app::SearchResultKindDto::content);
  CHECK(events[1].results.front().path == std::filesystem::path("src/a.cpp"));
  CHECK(events[2].kind == uburu::app::SearchEventKind::resultBatch);
  CHECK(events[2].results.size() == 1);
  CHECK(events[3].kind == uburu::app::SearchEventKind::completed);

  for (const auto& event : events) {
    CHECK(event.runId == runId);
  }

  CHECK(summary.matches == directEngine->results.size());
  CHECK(summary.metrics.resultsEmitted == directEngine->results.size());
  CHECK(summary.metrics.totalTime.count() >= 0);
}

TEST_CASE("default search service adapts result batches when event delivery is cheap")
{
  constexpr uburu::app::SearchRunId runId = 77;
  constexpr std::size_t initialBatchSize = 2;
  constexpr std::size_t maximumBatchSize = 8;

  auto directEngine = std::make_shared<FakeSearchEngine>();
  std::vector<std::size_t> batchSizes;

  directEngine->results = {
    result(uburu::SearchResultKind::content, "src/a.cpp", 1, "needle a"),
    result(uburu::SearchResultKind::content, "src/b.cpp", 2, "needle b"),
    result(uburu::SearchResultKind::content, "src/c.cpp", 3, "needle c"),
    result(uburu::SearchResultKind::content, "src/d.cpp", 4, "needle d"),
    result(uburu::SearchResultKind::content, "src/e.cpp", 5, "needle e"),
    result(uburu::SearchResultKind::content, "src/f.cpp", 6, "needle f"),
    result(uburu::SearchResultKind::content, "src/g.cpp", 7, "needle g"),
    result(uburu::SearchResultKind::content, "src/h.cpp", 8, "needle h"),
  };

  const uburu::app::DefaultSearchService service(directEngine);
  const auto summary = service.searchWithEvents(
    uburu::SearchQuery{},
    [&](const uburu::app::SearchEventDto& event) {
      if (event.kind == uburu::app::SearchEventKind::resultBatch)
        batchSizes.push_back(event.results.size());

      return true;
    },
    uburu::app::SearchExecutionOptions{.runId = runId,
                                       .resultBatchSize = initialBatchSize,
                                       .adaptiveBatching = true,
                                       .minimumResultBatchSize = initialBatchSize,
                                       .maximumResultBatchSize = maximumBatchSize,
                                       .targetBatchDeliveryLatency = std::chrono::seconds(1)});

  REQUIRE(batchSizes.size() == 3);
  CHECK(batchSizes[0] == 2);
  CHECK(batchSizes[1] == 4);
  CHECK(batchSizes[2] == 2);
  CHECK(summary.matches == directEngine->results.size());
}

TEST_CASE("default search service emits failed events for invalid hybrid queries")
{
  constexpr uburu::app::SearchRunId runId = 13;

  auto directEngine = std::make_shared<FakeSearchEngine>();
  auto indexService = std::make_shared<FakeIndexService>();
  std::vector<uburu::app::SearchEventDto> events;
  const uburu::app::DefaultSearchService service(
    directEngine, indexService, uburu::app::SearchServiceOptions{.strategy = uburu::app::SearchStrategy::hybrid});

  uburu::SearchQuery query{.root = "missing-root", .expression = "needle", .options = {}};
  const auto summary = service.searchWithEvents(
    query,
    [&](const uburu::app::SearchEventDto& event) {
      events.push_back(event);

      return true;
    },
    uburu::app::SearchExecutionOptions{.runId = runId});

  REQUIRE(events.size() == 2);
  CHECK(events[0].kind == uburu::app::SearchEventKind::started);
  CHECK(events[1].kind == uburu::app::SearchEventKind::failed);
  CHECK(events[1].runId == runId);
  CHECK_FALSE(events[1].summary.errors.empty());
  CHECK_FALSE(summary.errors.empty());
  CHECK(indexService->calls == 0);
  CHECK(directEngine->calls == 0);
}
