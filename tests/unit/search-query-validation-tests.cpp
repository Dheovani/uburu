#include "core/search/search-query-validation.hpp"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <system_error>
#include <vector>

namespace
{

  bool hasError(const std::vector<uburu::search::SearchError>& errors,
                 uburu::search::SearchErrorCode code)
  {
    return std::ranges::any_of(
        errors, [code](const uburu::search::SearchError& error) { return error.code == code; });
  }

} // namespace

TEST_CASE("search query validation accepts a literal query with an existing directory")
{
  const uburu::SearchQuery query{
    .root = std::filesystem::temp_directory_path(), .scope = {}, .expression = "needle", .options = {}};

  CHECK(uburu::search::validateSearchQuery(query).empty());
}

TEST_CASE("search query validation accepts multiple scoped roots without legacy root")
{
  const uburu::SearchQuery query{
    .root = {},
    .scope = uburu::SearchScope{.roots = {uburu::SearchRoot{.path = std::filesystem::temp_directory_path(),
                                                            .includedDirectories = {},
                                                            .excludedDirectories = {}}}},
    .expression = "needle",
    .options = {}};

  CHECK(uburu::search::validateSearchQuery(query).empty());
}

TEST_CASE("search query validation reports empty root and expression")
{
  const uburu::SearchQuery query;

  const auto errors = uburu::search::validateSearchQuery(query);

  CHECK(hasError(errors, uburu::search::SearchErrorCode::emptyRoot));
  CHECK(hasError(errors, uburu::search::SearchErrorCode::emptyExpression));
}

TEST_CASE("search query validation reports a missing root")
{
  const auto missingRoot =
      std::filesystem::temp_directory_path() / "uburu-validation-missing-root";
  std::error_code error;
  std::filesystem::remove_all(missingRoot, error);
  const uburu::SearchQuery query{
    .root = missingRoot, .scope = {}, .expression = "needle", .options = {}};

  const auto errors = uburu::search::validateSearchQuery(query);

  REQUIRE(errors.size() == 1);
  CHECK(errors.front().code == uburu::search::SearchErrorCode::rootNotFound);
}

TEST_CASE("search query validation reports a root that is not a directory")
{
  const auto path = std::filesystem::temp_directory_path() / "uburu-validation-file-root.txt";
  {
    std::ofstream file(path, std::ios::binary);
    file << "fixture";
  }
  const auto cleanup = [&] { std::filesystem::remove(path); };

  const uburu::SearchQuery query{.root = path, .scope = {}, .expression = "needle", .options = {}};

  const auto errors = uburu::search::validateSearchQuery(query);
  cleanup();

  REQUIRE(errors.size() == 1);
  CHECK(errors.front().code == uburu::search::SearchErrorCode::rootNotDirectory);
}

TEST_CASE("search query validation reports incompatible options")
{
  uburu::SearchQuery query{
    .root = std::filesystem::temp_directory_path(), .scope = {}, .expression = "needle", .options = {}};
  query.options.mode = uburu::SearchMode::regex;
  query.options.resultLimit = 0;
  query.options.perFileResultLimit = 0;
  query.options.maximumFileSize = 0;

  const auto errors = uburu::search::validateSearchQuery(query);

#ifdef UBURU_HAS_PCRE2
  CHECK_FALSE(hasError(errors, uburu::search::SearchErrorCode::unsupportedSearchMode));
#else
  CHECK(hasError(errors, uburu::search::SearchErrorCode::unsupportedSearchMode));
#endif
  CHECK(hasError(errors, uburu::search::SearchErrorCode::invalidResultLimit));
  CHECK(hasError(errors, uburu::search::SearchErrorCode::invalidPerFileResultLimit));
  CHECK(hasError(errors, uburu::search::SearchErrorCode::invalidMaximumFileSize));
}

TEST_CASE("search query validation reports invalid regex limits")
{
  uburu::SearchQuery query{
    .root = std::filesystem::temp_directory_path(), .scope = {}, .expression = "needle", .options = {}};
  query.options.mode = uburu::SearchMode::regex;
  query.options.regexMatchLimit = 0;

  const auto errors = uburu::search::validateSearchQuery(query);

  CHECK(hasError(errors, uburu::search::SearchErrorCode::invalidRegexLimit));
}
