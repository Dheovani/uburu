#include "core/search/search-scope.hpp"

#include <catch2/catch_test_macros.hpp>

#include <filesystem>

TEST_CASE("search scope falls back to the legacy root and query filters")
{
  uburu::SearchQuery query;
  query.root = std::filesystem::path("workspace");
  query.options.includedDirectories = {std::filesystem::path("src")};
  query.options.excludedDirectories = {std::filesystem::path("build")};

  const auto roots = uburu::search::effectiveSearchRoots(query);

  REQUIRE(roots.size() == 1);
  CHECK(roots.front().path == std::filesystem::path("workspace"));
  CHECK(roots.front().includedDirectories == query.options.includedDirectories);
  CHECK(roots.front().excludedDirectories == query.options.excludedDirectories);
}

TEST_CASE("search scope prefers explicit roots over the legacy root")
{
  uburu::SearchQuery query;
  query.root = std::filesystem::path("legacy");
  query.scope.roots = {
    uburu::SearchRoot{
      .path = std::filesystem::path("first"),
      .includedDirectories = {std::filesystem::path("src")},
      .excludedDirectories = {std::filesystem::path("out")},
    },
    uburu::SearchRoot{
      .path = std::filesystem::path("second"),
      .includedDirectories = {},
      .excludedDirectories = {},
    },
  };

  const auto roots = uburu::search::effectiveSearchRoots(query);

  REQUIRE(roots.size() == 2);
  CHECK(roots[0].path == std::filesystem::path("first"));
  CHECK(roots[1].path == std::filesystem::path("second"));
}

TEST_CASE("search scope returns no roots when no root source is configured")
{
  const uburu::SearchQuery query;

  CHECK(uburu::search::effectiveSearchRoots(query).empty());
}

TEST_CASE("search scope root filters override only configured filter sets")
{
  uburu::SearchOptions baseOptions;
  baseOptions.includedDirectories = {std::filesystem::path("base-include")};
  baseOptions.excludedDirectories = {std::filesystem::path("base-exclude")};
  baseOptions.extensions = {"cpp"};
  baseOptions.includedGlobs = {"src/*"};

  const uburu::SearchRoot root{
    .path = std::filesystem::path("workspace"),
    .includedDirectories = {std::filesystem::path("root-include")},
    .excludedDirectories = {},
  };

  const auto options = uburu::search::optionsForRoot(baseOptions, root);

  CHECK(options.includedDirectories == root.includedDirectories);
  CHECK(options.excludedDirectories == baseOptions.excludedDirectories);
  CHECK(options.extensions == baseOptions.extensions);
  CHECK(options.includedGlobs == baseOptions.includedGlobs);
}
