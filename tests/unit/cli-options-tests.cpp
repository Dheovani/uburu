#include "cli-options.hpp"
#include "cli-output.hpp"

#include <catch2/catch_test_macros.hpp>

#include <sstream>
#include <string_view>
#include <vector>

namespace
{

  [[nodiscard]]
  std::vector<std::string_view> args(std::initializer_list<std::string_view> values)
  {
    return std::vector<std::string_view>(values);
  }

} // namespace

TEST_CASE("CLI parser creates direct search request")
{
  const auto parsed = uburu::cli::parseCliOptions(args({"search", "C:/repo", "needle"}));

  REQUIRE(parsed.options.has_value());

  const auto& options = *parsed.options;

  CHECK(options.command == uburu::cli::CliCommand::search);
  CHECK(options.query.root.generic_string() == "C:/repo");
  CHECK(options.query.expression == "needle");
  CHECK(options.query.options.target == uburu::SearchTarget::contentAndFileName);
  REQUIRE(options.query.scope.roots.size() == 1);
  CHECK(options.query.scope.roots.front().path.generic_string() == "C:/repo");
}

TEST_CASE("CLI parser applies search flags")
{
  const auto parsed = uburu::cli::parseCliOptions(args({
    "search",
    "C:/repo",
    "needle",
    "--format",
    "jsonl",
    "--strategy",
    "hybrid",
    "--regex",
    "--case-sensitive",
    "--whole-word",
    "--types",
    "cpp,hpp",
    "--max-size-mib",
    "4",
    "--no-gitignore",
    "--no-subdirectories",
  }));

  REQUIRE(parsed.options.has_value());

  const auto& options = *parsed.options;

  CHECK(options.outputFormat == uburu::cli::CliOutputFormat::jsonLines);
  CHECK(options.searchStrategy == uburu::cli::CliSearchStrategy::hybrid);
  CHECK(options.query.options.mode == uburu::SearchMode::regex);
  CHECK(options.query.options.caseSensitive);
  CHECK(options.query.options.wholeWord);
  CHECK_FALSE(options.query.options.respectGitignore);
  CHECK_FALSE(options.query.options.includeSubdirectories);
  REQUIRE(options.query.options.extensions.size() == 2);
  CHECK(options.query.options.extensions[0] == "cpp");
  CHECK(options.query.options.extensions[1] == "hpp");
  CHECK(options.query.options.maximumFileSize == 4U * 1024U * 1024U);
}

TEST_CASE("CLI parser creates index status request")
{
  const auto parsed =
    uburu::cli::parseCliOptions(args({"index-status", "C:/repo", "--format", "jsonl", "--database", "C:/db.sqlite"}));

  REQUIRE(parsed.options.has_value());

  const auto& options = *parsed.options;

  CHECK(options.command == uburu::cli::CliCommand::indexStatus);
  CHECK(options.outputFormat == uburu::cli::CliOutputFormat::jsonLines);
  CHECK(options.query.root.generic_string() == "C:/repo");
  REQUIRE(options.databasePath.has_value());
  CHECK(options.databasePath->generic_string() == "C:/db.sqlite");
}

TEST_CASE("CLI parser creates index rebuild request")
{
  const auto parsed = uburu::cli::parseCliOptions(args({"index-rebuild", "C:/repo", "--types", "txt,md"}));

  REQUIRE(parsed.options.has_value());

  const auto& options = *parsed.options;

  CHECK(options.command == uburu::cli::CliCommand::indexRebuild);
  CHECK(options.query.root.generic_string() == "C:/repo");
  REQUIRE(options.query.options.extensions.size() == 2);
  CHECK(options.query.options.extensions[0] == "txt");
  CHECK(options.query.options.extensions[1] == "md");
}

TEST_CASE("CLI parser handles empty arguments as help")
{
  const auto parsed = uburu::cli::parseCliOptions({});

  REQUIRE(parsed.options.has_value());
  CHECK(parsed.options->showHelp);
}

TEST_CASE("CLI parser rejects unknown options")
{
  const auto parsed = uburu::cli::parseCliOptions(args({"search", "C:/repo", "needle", "--wat"}));

  CHECK_FALSE(parsed.options.has_value());
  CHECK(parsed.error == "unknown option: --wat");
}

TEST_CASE("CLI JSON Lines output escapes result payload")
{
  uburu::SearchResult result;
  result.path = "C:/repo/file.txt";
  result.line = 7;
  result.column = 3;
  result.matchLength = 5;
  result.lineText = "hello \"needle\"";

  std::ostringstream output;
  uburu::cli::writeSearchResult(output, result, uburu::cli::CliOutputFormat::jsonLines);

  CHECK(output.str() ==
        "{\"type\":\"result\",\"path\":\"C:/repo/file.txt\",\"line\":7,\"column\":3,\"matchLength\":5,"
        "\"text\":\"hello \\\"needle\\\"\"}\n");
}
