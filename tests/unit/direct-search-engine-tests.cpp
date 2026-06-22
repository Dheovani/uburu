#include "core/filesystem/file-scanner.hpp"
#include "core/search/direct-search-engine.hpp"

#include <catch2/catch_test_macros.hpp>

#include <fstream>
#include <memory>

namespace
{

  class EmptyScanner final : public uburu::filesystem::FileScanner
  {
  public:
    void scan(const std::filesystem::path&, const uburu::SearchOptions&,
              uburu::filesystem::FileSink, std::stop_token) const override
    {
      ++calls;
    }
    mutable int calls{0};
  };

  class SingleFileScanner final : public uburu::filesystem::FileScanner
  {
  public:
    explicit SingleFileScanner(std::filesystem::path path) : path_(std::move(path)) {}
    void scan(const std::filesystem::path&, const uburu::SearchOptions&,
              uburu::filesystem::FileSink sink, std::stop_token) const override
    {
      sink(uburu::FileEntry{.absolute_path = path_, .relative_path = "source.cpp"});
    }

  private:
    std::filesystem::path path_;
  };

} // namespace

TEST_CASE("an empty expression does not start filesystem traversal")
{
  auto scanner = std::make_shared<EmptyScanner>();
  uburu::search::DirectSearchEngine engine(scanner);
  uburu::SearchQuery query{.root = "fixture", .expression = ""};

  const auto summary = engine.search(query, [](uburu::SearchResult) { return true; });

  CHECK(scanner->calls == 0);
  CHECK(summary.files_scanned == 0);
  CHECK(summary.matches == 0);
}

TEST_CASE("a pre-cancelled search reports cancellation")
{
  auto scanner = std::make_shared<EmptyScanner>();
  uburu::search::DirectSearchEngine engine(scanner);
  std::stop_source cancellation;
  cancellation.request_stop();
  uburu::SearchQuery query{.root = "fixture", .expression = "needle"};

  const auto summary =
      engine.search(query, [](uburu::SearchResult) { return true; }, cancellation.get_token());

  CHECK(summary.cancelled);
}

TEST_CASE("direct search streams a match with one-based line and column")
{
  const auto path = std::filesystem::temp_directory_path() / "uburu-search-test.txt";
  {
    std::ofstream file(path, std::ios::binary);
    file << "first line\nfind the Needle here\n";
  }
  const auto cleanup = [&] { std::filesystem::remove(path); };

  auto scanner = std::make_shared<SingleFileScanner>(path);
  uburu::search::DirectSearchEngine engine(scanner);
  uburu::SearchQuery query{.root = "fixture", .expression = "needle"};
  std::vector<uburu::SearchResult> results;

  const auto summary = engine.search(query, [&](uburu::SearchResult result) {
    results.push_back(std::move(result));
    return true;
  });
  cleanup();

  REQUIRE(results.size() == 1);
  CHECK(results.front().path == std::filesystem::path("source.cpp"));
  CHECK(results.front().line == 2);
  CHECK(results.front().column == 10);
  CHECK(summary.matches == 1);
}
