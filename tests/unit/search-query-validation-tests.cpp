#include "core/search/search-query-validation.hpp"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <system_error>
#include <vector>

namespace
{

  bool has_error(const std::vector<uburu::search::SearchError>& errors,
                 uburu::search::SearchErrorCode code)
  {
    return std::ranges::any_of(
        errors, [code](const uburu::search::SearchError& error) { return error.code == code; });
  }

} // namespace

TEST_CASE("search query validation accepts a literal query with an existing directory")
{
  const uburu::SearchQuery query{.root = std::filesystem::temp_directory_path(),
                                 .expression = "needle"};

  CHECK(uburu::search::validate_search_query(query).empty());
}

TEST_CASE("search query validation reports empty root and expression")
{
  const uburu::SearchQuery query;

  const auto errors = uburu::search::validate_search_query(query);

  CHECK(has_error(errors, uburu::search::SearchErrorCode::empty_root));
  CHECK(has_error(errors, uburu::search::SearchErrorCode::empty_expression));
}

TEST_CASE("search query validation reports a missing root")
{
  const auto missing_root =
      std::filesystem::temp_directory_path() / "uburu-validation-missing-root";
  std::error_code error;
  std::filesystem::remove_all(missing_root, error);
  const uburu::SearchQuery query{.root = missing_root, .expression = "needle"};

  const auto errors = uburu::search::validate_search_query(query);

  REQUIRE(errors.size() == 1);
  CHECK(errors.front().code == uburu::search::SearchErrorCode::root_not_found);
}

TEST_CASE("search query validation reports a root that is not a directory")
{
  const auto path = std::filesystem::temp_directory_path() / "uburu-validation-file-root.txt";
  {
    std::ofstream file(path, std::ios::binary);
    file << "fixture";
  }
  const auto cleanup = [&] { std::filesystem::remove(path); };

  const uburu::SearchQuery query{.root = path, .expression = "needle"};

  const auto errors = uburu::search::validate_search_query(query);
  cleanup();

  REQUIRE(errors.size() == 1);
  CHECK(errors.front().code == uburu::search::SearchErrorCode::root_not_directory);
}

TEST_CASE("search query validation reports incompatible options")
{
  uburu::SearchQuery query{.root = std::filesystem::temp_directory_path(), .expression = "needle"};
  query.options.mode = uburu::SearchMode::regex;
  query.options.result_limit = 0;
  query.options.maximum_file_size = 0;

  const auto errors = uburu::search::validate_search_query(query);

  CHECK(has_error(errors, uburu::search::SearchErrorCode::unsupported_search_mode));
  CHECK(has_error(errors, uburu::search::SearchErrorCode::invalid_result_limit));
  CHECK(has_error(errors, uburu::search::SearchErrorCode::invalid_maximum_file_size));
}
