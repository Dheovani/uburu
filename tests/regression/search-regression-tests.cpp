#include "core/filesystem/recursive-file-scanner.hpp"
#include "core/search/direct-search-engine.hpp"
#include "helpers/temporary-paths.hpp"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

namespace
{

  std::vector<uburu::SearchResult> runDirectSearch(const uburu::SearchQuery& query)
  {
    uburu::search::DirectSearchEngine engine(std::make_shared<uburu::filesystem::RecursiveFileScanner>());
    std::vector<uburu::SearchResult> results;

    const auto summary = engine.search(query, [&](uburu::SearchResult result) {
      results.push_back(std::move(result));

      return true;
    });

    CHECK(summary.cancelled == false);

    return results;
  }

} // namespace

TEST_CASE("regression: literal phrases with spaces are found in txt files")
{
  uburu::tests::TemporaryDirectory directory("uburu-regression-phrase-search");
  uburu::tests::writeFile(directory.path() / "notes" / "argument.txt",
                          "The first line is irrelevant.\n"
                          "This exact phrase should be found inside a text document.\n");

  uburu::SearchQuery query{
    .root = directory.path(), .expression = "exact phrase should be found", .options = {.extensions = {"txt"}}};

  const auto results = runDirectSearch(query);

  REQUIRE(results.size() == 1);
  CHECK(results.front().path.filename() == "argument.txt");
  CHECK(results.front().line == 2);
  REQUIRE(results.front().highlights.size() == 1);
}

TEST_CASE("regression: per-root exclusions do not leak across selected search roots")
{
  uburu::tests::TemporaryDirectory firstRoot("uburu-regression-scope-first");
  uburu::tests::TemporaryDirectory secondRoot("uburu-regression-scope-second");
  const auto firstExcluded = firstRoot.path() / "excluded";
  const auto secondExcluded = secondRoot.path() / "excluded";

  uburu::tests::writeFile(firstRoot.path() / "included" / "first.txt", "needle\n");
  uburu::tests::writeFile(firstExcluded / "hidden.txt", "needle\n");
  uburu::tests::writeFile(secondRoot.path() / "included" / "second.txt", "needle\n");
  uburu::tests::writeFile(secondExcluded / "kept.txt", "needle\n");

  uburu::SearchQuery query{
    .scope = {.roots = {{.path = firstRoot.path(), .excludedDirectories = {firstExcluded}},
                        {.path = secondRoot.path(), .excludedDirectories = {}}}},
    .expression = "needle",
    .options = {.extensions = {"txt"}},
  };

  const auto results = runDirectSearch(query);
  std::vector<std::string> fileNames;
  fileNames.reserve(results.size());

  for (const auto& result : results)
    fileNames.push_back(result.path.filename().string());

  std::ranges::sort(fileNames);

  REQUIRE(results.size() == 3);
  CHECK(fileNames == std::vector<std::string>{"first.txt", "kept.txt", "second.txt"});
}
