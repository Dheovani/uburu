#include "cli-options.hpp"
#include "cli-output.hpp"

#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
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
    "--memory-budget-mib",
    "32",
    "--threads",
    "4",
    "--summary-only",
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
  CHECK(options.query.options.resultMemoryBudgetBytes == 32U * 1024U * 1024U);
  CHECK(options.query.options.maximumThreadCount == 4);
  CHECK(options.summaryOnly);
}

TEST_CASE("CLI parser accepts automatic direct search worker selection")
{
  const auto parsed = uburu::cli::parseCliOptions(args({"search", "C:/repo", "needle", "--threads", "auto"}));

  REQUIRE(parsed.options.has_value());
  CHECK(parsed.options->query.options.maximumThreadCount == uburu::automaticSearchThreadCount);
}

TEST_CASE("CLI parser rejects invalid direct search worker counts")
{
  const auto excessiveValue = std::to_string(uburu::maximumSearchThreadCount + 1);
  const auto zero = uburu::cli::parseCliOptions(args({"search", "C:/repo", "needle", "--threads", "0"}));
  const auto excessive =
    uburu::cli::parseCliOptions(args({"search", "C:/repo", "needle", "--threads", excessiveValue}));
  const auto malformed = uburu::cli::parseCliOptions(args({"search", "C:/repo", "needle", "--threads", "many"}));
  const auto expectedError =
    "--threads requires auto or an integer from 1 to " + std::to_string(uburu::maximumSearchThreadCount);

  CHECK_FALSE(zero.options.has_value());
  CHECK_FALSE(excessive.options.has_value());
  CHECK_FALSE(malformed.options.has_value());
  CHECK(zero.error == expectedError);
  CHECK(excessive.error == zero.error);
  CHECK(malformed.error == zero.error);
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
  const auto parsed = uburu::cli::parseCliOptions(args({
    "index-rebuild",
    "C:/repo",
    "--types",
    "txt,md",
    "--max-size-mib",
    "256",
    "--memory-budget-mib",
    "64",
  }));

  REQUIRE(parsed.options.has_value());

  const auto& options = *parsed.options;

  CHECK(options.command == uburu::cli::CliCommand::indexRebuild);
  CHECK(options.query.root.generic_string() == "C:/repo");
  REQUIRE(options.query.options.extensions.size() == 2);
  CHECK(options.query.options.extensions[0] == "txt");
  CHECK(options.query.options.extensions[1] == "md");
  CHECK(options.query.options.maximumFileSize == 256U * 1024U * 1024U);
  CHECK(options.query.options.resultMemoryBudgetBytes == 64U * 1024U * 1024U);
}

TEST_CASE("CLI parser rejects invalid index file size limits")
{
  const auto missing = uburu::cli::parseCliOptions(args({"index-rebuild", "C:/repo", "--max-size-mib"}));
  const auto malformed =
    uburu::cli::parseCliOptions(args({"index-rebuild", "C:/repo", "--max-size-mib", "large"}));

  CHECK_FALSE(missing.options.has_value());
  CHECK_FALSE(malformed.options.has_value());
  CHECK(missing.error == "--max-size-mib requires a numeric value");
  CHECK(malformed.error == missing.error);
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

  CHECK(output.str() == "{\"type\":\"result\",\"path\":\"C:/repo/file.txt\",\"line\":7,\"column\":3,\"matchLength\":5,"
                        "\"text\":\"hello \\\"needle\\\"\"}\n");
}

TEST_CASE("CLI search summary exposes result memory exhaustion")
{
  uburu::search::SearchSummary summary;
  summary.matches = 3;
  summary.resultMemoryBytes = 4096;
  summary.memoryLimitReached = true;
  summary.metrics.timeToFirstResult = std::chrono::nanoseconds{150};
  summary.metrics.totalTime = std::chrono::nanoseconds{900};
  summary.metrics.filesProcessed = 12;
  summary.metrics.bytesProcessed = 40960;
  summary.metrics.documentExtractionSafetyLimited = 5;
  summary.metrics.documentExtractionProtected = 2;
  summary.metrics.documentExtractionParserFailures = 1;

  std::ostringstream output;
  uburu::cli::writeSearchSummary(output, summary, uburu::cli::CliOutputFormat::jsonLines);

  CHECK(output.str().find("\"memoryLimitReached\":true") != std::string::npos);
  CHECK(output.str().find("\"resultMemoryBytes\":4096") != std::string::npos);
  CHECK(output.str().find("\"timeToFirstResultNanoseconds\":150") != std::string::npos);
  CHECK(output.str().find("\"totalTimeNanoseconds\":900") != std::string::npos);
  CHECK(output.str().find("\"filesProcessed\":12") != std::string::npos);
  CHECK(output.str().find("\"bytesProcessed\":40960") != std::string::npos);
  CHECK(output.str().find("\"extractionSafetyLimited\":5") != std::string::npos);
  CHECK(output.str().find("\"extractionProtected\":2") != std::string::npos);
  CHECK(output.str().find("\"extractionParserFailures\":1") != std::string::npos);
}

TEST_CASE("CLI index summary exposes working memory exhaustion")
{
  uburu::index::IndexUpdateSummary summary;
  uburu::index::IndexExtractorMetrics extractorMetrics;

  summary.workingMemoryPeakBytes = 8192;
  summary.memoryLimitReached = true;
  extractorMetrics.extractorName = "pdf";
  extractorMetrics.filesProcessed = 9;
  extractorMetrics.skippedSafetyLimited = 5;
  extractorMetrics.skippedProtected = 2;
  extractorMetrics.parserFailures = 1;
  summary.extractorMetrics.push_back(std::move(extractorMetrics));

  std::ostringstream output;
  uburu::cli::writeIndexUpdateSummary(output, summary, uburu::cli::CliOutputFormat::jsonLines);

  CHECK(output.str().find("\"memoryLimitReached\":true") != std::string::npos);
  CHECK(output.str().find("\"workingMemoryPeakBytes\":8192") != std::string::npos);
  CHECK(output.str().find("\"extractionFilesProcessed\":9") != std::string::npos);
  CHECK(output.str().find("\"extractionSafetyLimited\":5") != std::string::npos);
  CHECK(output.str().find("\"extractionProtected\":2") != std::string::npos);
  CHECK(output.str().find("\"extractionParserFailures\":1") != std::string::npos);
}
