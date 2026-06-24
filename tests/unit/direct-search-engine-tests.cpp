#include "core/filesystem/file-scanner.hpp"
#include "core/search/direct-search-engine.hpp"

#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <fstream>
#include <memory>
#include <system_error>
#include <vector>

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
  uburu::SearchQuery query{.root = std::filesystem::temp_directory_path(), .expression = ""};

  const auto summary = engine.search(query, [](uburu::SearchResult) { return true; });

  CHECK(scanner->calls == 0);
  CHECK(summary.files_scanned == 0);
  CHECK(summary.matches == 0);
  REQUIRE(summary.errors.size() == 1);
  CHECK(summary.errors.front().code == uburu::search::SearchErrorCode::empty_expression);
}

TEST_CASE("an invalid root does not start filesystem traversal")
{
  auto scanner = std::make_shared<EmptyScanner>();
  uburu::search::DirectSearchEngine engine(scanner);
  uburu::SearchQuery query{.root = std::filesystem::temp_directory_path() /
                                   "uburu-direct-search-missing-root",
                           .expression = "needle"};
  std::error_code error;
  std::filesystem::remove_all(query.root, error);

  const auto summary = engine.search(query, [](uburu::SearchResult) { return true; });

  CHECK(scanner->calls == 0);
  CHECK(summary.files_scanned == 0);
  CHECK(summary.matches == 0);
  REQUIRE(summary.errors.size() == 1);
  CHECK(summary.errors.front().code == uburu::search::SearchErrorCode::root_not_found);
}

TEST_CASE("a pre-cancelled search reports cancellation")
{
  auto scanner = std::make_shared<EmptyScanner>();
  uburu::search::DirectSearchEngine engine(scanner);
  std::stop_source cancellation;
  cancellation.request_stop();
  uburu::SearchQuery query{.root = std::filesystem::temp_directory_path(), .expression = "needle"};

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
  uburu::SearchQuery query{.root = std::filesystem::temp_directory_path(), .expression = "needle"};
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

TEST_CASE("direct search streams every match on the same line")
{
  const auto path = std::filesystem::temp_directory_path() / "uburu-search-multi-match-test.txt";
  {
    std::ofstream file(path, std::ios::binary);
    file << "needle needle\n";
  }
  const auto cleanup = [&] { std::filesystem::remove(path); };

  auto scanner = std::make_shared<SingleFileScanner>(path);
  uburu::search::DirectSearchEngine engine(scanner);
  uburu::SearchQuery query{.root = std::filesystem::temp_directory_path(), .expression = "needle"};
  std::vector<uburu::SearchResult> results;

  const auto summary = engine.search(query, [&](uburu::SearchResult result) {
    results.push_back(std::move(result));
    return true;
  });
  cleanup();

  REQUIRE(results.size() == 2);
  CHECK(results[0].column == 1);
  CHECK(results[1].column == 8);
  CHECK(summary.matches == 2);
}

TEST_CASE("direct search applies the global result limit before publishing")
{
  const auto path = std::filesystem::temp_directory_path() / "uburu-search-limit-test.txt";
  {
    std::ofstream file(path, std::ios::binary);
    file << "needle needle\n";
  }
  const auto cleanup = [&] { std::filesystem::remove(path); };

  auto scanner = std::make_shared<SingleFileScanner>(path);
  uburu::search::DirectSearchEngine engine(scanner);
  uburu::SearchQuery query{.root = std::filesystem::temp_directory_path(), .expression = "needle"};
  query.options.result_limit = 1;
  std::vector<uburu::SearchResult> results;

  const auto summary = engine.search(query, [&](uburu::SearchResult result) {
    results.push_back(std::move(result));
    return true;
  });
  cleanup();

  REQUIRE(results.size() == 1);
  CHECK(results.front().column == 1);
  CHECK(summary.matches == 1);
  CHECK(summary.limit_reached);
}
