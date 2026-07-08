#include "core/filesystem/file-scanner.hpp"
#include "core/filesystem/recursive-file-scanner.hpp"
#include "core/search/direct-search-engine.hpp"
#include "helpers/temporary-paths.hpp"

#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <memory>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

namespace
{

  class EmptyScanner final : public uburu::filesystem::FileScanner
  {
  public:
    void scan(const std::filesystem::path&,
              const uburu::SearchOptions&,
              uburu::filesystem::FileSink,
              std::stop_token,
              uburu::diagnostics::SearchMetrics*) const override
    {
      ++calls;
    }
    mutable int calls{0};
  };

  class CapturingScanner final : public uburu::filesystem::FileScanner
  {
  public:
    struct Call
    {
      std::filesystem::path root;
      std::vector<std::filesystem::path> excludedDirectories;
    };

    void scan(const std::filesystem::path& root,
              const uburu::SearchOptions& options,
              uburu::filesystem::FileSink sink,
              std::stop_token,
              uburu::diagnostics::SearchMetrics*) const override
    {
      calls.push_back(Call{.root = root, .excludedDirectories = options.excludedDirectories});

      const auto fileName = "source-" + std::to_string(calls.size()) + ".txt";
      const auto absolutePath = root / fileName;
      uburu::tests::writeFile(absolutePath, "needle\n");

      sink(uburu::FileEntry{.absolutePath = absolutePath,
                            .relativePath = fileName,
                            .size = 7,
                            .modifiedAt = {},
                            .hidden = false,
                            .binary = false,
                            .symlink = false,
                            .sparse = false,
                            .searchRoot = root});
    }

    mutable std::vector<Call> calls;
  };

  class SingleFileScanner final : public uburu::filesystem::FileScanner
  {
  public:
    explicit SingleFileScanner(std::filesystem::path path, std::filesystem::path relativePath = "source.cpp")
      : pathValue(std::move(path)), relativePathValue(std::move(relativePath))
    {}

    void scan(const std::filesystem::path&,
              const uburu::SearchOptions&,
              uburu::filesystem::FileSink sink,
              std::stop_token,
              uburu::diagnostics::SearchMetrics*) const override
    {
      sink(uburu::FileEntry{.absolutePath = pathValue,
                            .relativePath = relativePathValue,
                            .size = file_size(pathValue),
                            .modifiedAt = {},
                            .hidden = false,
                            .binary = false,
                            .symlink = false,
                            .sparse = false,
                            .searchRoot = {}});
    }

  private:
    static std::uintmax_t file_size(const std::filesystem::path& path)
    {
      std::error_code error;
      const auto size = std::filesystem::file_size(path, error);

      return error ? 0 : size;
    }

    std::filesystem::path pathValue;
    std::filesystem::path relativePathValue;
  };

  class ObservingScanner final : public uburu::filesystem::FileScanner
  {
  public:
    ObservingScanner(std::filesystem::path firstPath,
                     std::filesystem::path secondPath,
                     std::function<void()> afterFirstEntry)
      : firstPath(std::move(firstPath)), secondPath(std::move(secondPath)), afterFirstEntry(std::move(afterFirstEntry))
    {}

    void scan(const std::filesystem::path&,
              const uburu::SearchOptions&,
              uburu::filesystem::FileSink sink,
              std::stop_token,
              uburu::diagnostics::SearchMetrics*) const override
    {
      sink(uburu::FileEntry{.absolutePath = firstPath,
                            .relativePath = "first.txt",
                            .size = 0,
                            .modifiedAt = {},
                            .hidden = false,
                            .binary = false,
                            .symlink = false,
                            .sparse = false,
                            .searchRoot = {}});
      afterFirstEntry();
      sink(uburu::FileEntry{.absolutePath = secondPath,
                            .relativePath = "second.txt",
                            .size = 0,
                            .modifiedAt = {},
                            .hidden = false,
                            .binary = false,
                            .symlink = false,
                            .sparse = false,
                            .searchRoot = {}});
    }

  private:
    std::filesystem::path firstPath;
    std::filesystem::path secondPath;
    std::function<void()> afterFirstEntry;
  };

  class DeletingScanner final : public uburu::filesystem::FileScanner
  {
  public:
    explicit DeletingScanner(std::filesystem::path path) : pathValue(std::move(path)) {}

    void scan(const std::filesystem::path&,
              const uburu::SearchOptions&,
              uburu::filesystem::FileSink sink,
              std::stop_token,
              uburu::diagnostics::SearchMetrics*) const override
    {
      std::filesystem::remove(pathValue);
      sink(uburu::FileEntry{.absolutePath = pathValue,
                            .relativePath = "removed.txt",
                            .size = 0,
                            .modifiedAt = {},
                            .hidden = false,
                            .binary = false,
                            .symlink = false,
                            .sparse = false,
                            .searchRoot = {}});
    }

  private:
    std::filesystem::path pathValue;
  };

  uburu::SearchQuery makeQuery(std::filesystem::path root, std::string expression)
  {
    return {.root = std::move(root), .scope = {}, .expression = std::move(expression), .options = {}};
  }

} // namespace

TEST_CASE("an empty expression does not start filesystem traversal")
{
  auto scanner = std::make_shared<EmptyScanner>();
  uburu::search::DirectSearchEngine engine(scanner);
  uburu::SearchQuery query = makeQuery(std::filesystem::temp_directory_path(), "");

  const auto summary = engine.search(query, [](uburu::SearchResult) { return true; });

  CHECK(scanner->calls == 0);
  CHECK(summary.filesScanned == 0);
  CHECK(summary.matches == 0);
  REQUIRE(summary.errors.size() == 1);
  CHECK(summary.errors.front().code == uburu::search::SearchErrorCode::emptyExpression);
}

TEST_CASE("an invalid root does not start filesystem traversal")
{
  auto scanner = std::make_shared<EmptyScanner>();
  uburu::search::DirectSearchEngine engine(scanner);
  uburu::SearchQuery query = makeQuery(uburu::tests::uniqueTemporaryPath("uburu-direct-search-missing-root"), "needle");

  const auto summary = engine.search(query, [](uburu::SearchResult) { return true; });

  CHECK(scanner->calls == 0);
  CHECK(summary.filesScanned == 0);
  CHECK(summary.matches == 0);
  REQUIRE(summary.errors.size() == 1);
  CHECK(summary.errors.front().code == uburu::search::SearchErrorCode::rootNotFound);
}

TEST_CASE("direct search scans every root in the search scope")
{
  auto scanner = std::make_shared<CapturingScanner>();
  uburu::search::DirectSearchEngine engine(scanner);
  const uburu::tests::TemporaryDirectory firstDirectory("uburu-direct-search-scope-first");
  const uburu::tests::TemporaryDirectory secondDirectory("uburu-direct-search-scope-second");
  const auto& firstRoot = firstDirectory.path();
  const auto& secondRoot = secondDirectory.path();

  uburu::SearchQuery query{
    .root = {},
    .scope =
      uburu::SearchScope{
        .roots =
          {uburu::SearchRoot{.path = firstRoot, .includedDirectories = {}, .excludedDirectories = {"node_modules"}},
           uburu::SearchRoot{.path = secondRoot, .includedDirectories = {}, .excludedDirectories = {"node_modules"}}}},
    .expression = "needle",
    .options = {}};

  std::vector<uburu::SearchResult> results;
  const auto summary = engine.search(query, [&](uburu::SearchResult result) {
    results.push_back(std::move(result));

    return true;
  });

  REQUIRE(scanner->calls.size() == 2);
  CHECK(scanner->calls.front().root == firstRoot);
  CHECK(scanner->calls.back().root == secondRoot);
  REQUIRE(scanner->calls.front().excludedDirectories.size() == 1);
  CHECK(scanner->calls.front().excludedDirectories.front() == "node_modules");
  CHECK(summary.matches == 2);
  REQUIRE(results.size() == 2);
  CHECK(results.front().searchRoot == firstRoot);
  CHECK(results.back().searchRoot == secondRoot);
}

TEST_CASE("a pre-cancelled search reports cancellation")
{
  auto scanner = std::make_shared<EmptyScanner>();
  uburu::search::DirectSearchEngine engine(scanner);
  std::stop_source cancellation;
  cancellation.request_stop();
  uburu::SearchQuery query = makeQuery(std::filesystem::temp_directory_path(), "needle");

  const auto summary = engine.search(query, [](uburu::SearchResult) { return true; }, cancellation.get_token());

  CHECK(summary.cancelled);
  CHECK(scanner->calls == 0);
  CHECK(summary.filesScanned == 0);
}

TEST_CASE("a pre-cancelled regex search reports cancellation")
{
  auto scanner = std::make_shared<EmptyScanner>();
  uburu::search::DirectSearchEngine engine(scanner);
  std::stop_source cancellation;
  cancellation.request_stop();
  uburu::SearchQuery query = makeQuery(std::filesystem::temp_directory_path(), "needle");
  query.options.mode = uburu::SearchMode::regex;

  const auto summary = engine.search(query, [](uburu::SearchResult) { return true; }, cancellation.get_token());

#ifdef UBURU_HAS_PCRE2
  CHECK(summary.cancelled);
#else
  CHECK_FALSE(summary.cancelled);
  REQUIRE(summary.errors.size() == 1);
  CHECK(summary.errors.front().code == uburu::search::SearchErrorCode::unsupportedSearchMode);
#endif
}

TEST_CASE("direct search observes cancellation while scanning entries")
{
  const uburu::tests::TemporaryFile firstFile("uburu-search-cancel-scan-first.txt");
  const uburu::tests::TemporaryFile secondFile("uburu-search-cancel-scan-second.txt");
  const auto& firstPath = firstFile.path();
  const auto& secondPath = secondFile.path();
  uburu::tests::writeFile(firstPath, "needle\n");
  uburu::tests::writeFile(secondPath, "needle\n");

  std::stop_source cancellation;
  auto scanner = std::make_shared<ObservingScanner>(firstPath, secondPath, [&] { cancellation.request_stop(); });
  uburu::search::DirectSearchEngine engine(scanner);
  uburu::SearchQuery query = makeQuery(firstPath.parent_path(), "needle");
  std::vector<uburu::SearchResult> results;

  const auto summary = engine.search(
    query,
    [&](uburu::SearchResult result) {
      results.push_back(std::move(result));

      return true;
    },
    cancellation.get_token());

  CHECK(summary.cancelled);
  CHECK(summary.filesScanned == 1);
  REQUIRE(results.size() == 1);
  CHECK(results.front().path == std::filesystem::path("first.txt"));
}

TEST_CASE("direct search observes cancellation while reading file content")
{
  const uburu::tests::TemporaryFile file("uburu-search-cancel-read.txt");
  const auto& path = file.path();
  std::string content;

  for (int line = 0; line < 128; ++line)
    content += "needle on a cancellable line\n";

  uburu::tests::writeFile(path, content);

  std::stop_source cancellation;
  auto scanner = std::make_shared<SingleFileScanner>(path);
  uburu::search::DirectSearchEngine engine(scanner);
  uburu::SearchQuery query = makeQuery(path.parent_path(), "needle");
  std::vector<uburu::SearchResult> results;

  const auto summary = engine.search(
    query,
    [&](uburu::SearchResult result) {
      results.push_back(std::move(result));
      cancellation.request_stop();

      return true;
    },
    cancellation.get_token());

  CHECK(summary.cancelled);
  CHECK(summary.filesScanned == 1);
  REQUIRE(results.size() == 1);
  CHECK(results.front().line == 1);
}

TEST_CASE("direct search streams a match with one-based line and column")
{
  const uburu::tests::TemporaryFile file("uburu-search-test.txt");
  const auto& path = file.path();
  uburu::tests::writeFile(path, "first line\nfind the Needle here\n");

  auto scanner = std::make_shared<SingleFileScanner>(path);
  uburu::search::DirectSearchEngine engine(scanner);
  uburu::SearchQuery query = makeQuery(path.parent_path(), "needle");
  std::vector<uburu::SearchResult> results;

  const auto summary = engine.search(query, [&](uburu::SearchResult result) {
    results.push_back(std::move(result));
    return true;
  });

  REQUIRE(results.size() == 1);
  CHECK(results.front().path == std::filesystem::path("source.cpp"));
  CHECK(results.front().line == 2);
  CHECK(results.front().column == 10);
  CHECK(summary.matches == 1);
}

TEST_CASE("direct search finds text through the recursive scanner")
{
  const uburu::tests::TemporaryDirectory directory("uburu-direct-search-real-scanner-test");
  const auto& root = directory.path();
  const auto path = root / "nested" / "sample.txt";
  uburu::tests::writeFile(path, "first line\nselected fragment here\n");

  auto scanner = std::make_shared<uburu::filesystem::RecursiveFileScanner>();
  uburu::search::DirectSearchEngine engine(scanner);
  uburu::SearchQuery query = makeQuery(root, "selected fragment");
  std::vector<uburu::SearchResult> results;

  const auto summary = engine.search(query, [&](uburu::SearchResult result) {
    results.push_back(std::move(result));
    return true;
  });

  REQUIRE(results.size() == 1);
  CHECK(results.front().path == std::filesystem::path("nested") / "sample.txt");
  CHECK(summary.errors.empty());
  CHECK(summary.filesScanned == 1);
  CHECK(summary.matches == 1);
}

TEST_CASE("direct search finds file names through the recursive scanner")
{
  const uburu::tests::TemporaryDirectory directory("uburu-direct-search-real-file-name-test");
  const auto& root = directory.path();
  const auto path = root / "docs" / "important-report.pdf";
  uburu::tests::writeBytes(path, {0x25U, 0x50U, 0x44U, 0x46U, 0x00U});

  auto scanner = std::make_shared<uburu::filesystem::RecursiveFileScanner>();
  uburu::search::DirectSearchEngine engine(scanner);
  uburu::SearchQuery query = makeQuery(root, "important-report");
  query.options.target = uburu::SearchTarget::contentAndFileName;
  std::vector<uburu::SearchResult> results;

  const auto summary = engine.search(query, [&](uburu::SearchResult result) {
    results.push_back(std::move(result));

    return true;
  });

  REQUIRE(results.size() == 1);
  CHECK(results.front().kind == uburu::SearchResultKind::fileName);
  CHECK(results.front().path == std::filesystem::path("docs") / "important-report.pdf");
  CHECK(summary.errors.empty());
  CHECK(summary.filesScanned == 1);
  CHECK(summary.matches == 1);
}

TEST_CASE("direct search preserves unicode relative paths")
{
  const uburu::tests::TemporaryFile file("uburu-search-unicode-path-test.txt");
  const auto& path = file.path();
  uburu::tests::writeFile(path, "needle\n");

  auto scanner = std::make_shared<SingleFileScanner>(path, std::filesystem::path(L"código") / L"ação.cpp");
  uburu::search::DirectSearchEngine engine(scanner);
  uburu::SearchQuery query = makeQuery(path.parent_path(), "needle");
  std::vector<uburu::SearchResult> results;

  const auto summary = engine.search(query, [&](uburu::SearchResult result) {
    results.push_back(std::move(result));
    return true;
  });

  REQUIRE(results.size() == 1);
  CHECK(results.front().path == std::filesystem::path(L"código") / L"ação.cpp");
  CHECK(summary.errors.empty());
  CHECK(summary.matches == 1);
}

TEST_CASE("direct search streams every match on the same line")
{
  const uburu::tests::TemporaryFile file("uburu-search-multi-match-test.txt");
  const auto& path = file.path();
  uburu::tests::writeFile(path, "needle needle\n");

  auto scanner = std::make_shared<SingleFileScanner>(path);
  uburu::search::DirectSearchEngine engine(scanner);
  uburu::SearchQuery query = makeQuery(path.parent_path(), "needle");
  std::vector<uburu::SearchResult> results;

  const auto summary = engine.search(query, [&](uburu::SearchResult result) {
    results.push_back(std::move(result));
    return true;
  });

  REQUIRE(results.size() == 2);
  CHECK(results[0].column == 1);
  CHECK(results[1].column == 8);
  CHECK(summary.matches == 2);
}

TEST_CASE("direct search publishes results before scanner completion")
{
  const uburu::tests::TemporaryFile firstFile("uburu-progressive-first.txt");
  const uburu::tests::TemporaryFile secondFile("uburu-progressive-second.txt");
  const auto& firstPath = firstFile.path();
  const auto& secondPath = secondFile.path();
  uburu::tests::writeFile(firstPath, "needle\n");
  uburu::tests::writeFile(secondPath, "needle\n");

  std::vector<uburu::SearchResult> results;
  auto scanner = std::make_shared<ObservingScanner>(firstPath, secondPath, [&] {
    REQUIRE(results.size() == 1);
    CHECK(results.front().path == std::filesystem::path("first.txt"));
  });
  uburu::search::DirectSearchEngine engine(scanner);
  uburu::SearchQuery query = makeQuery(firstPath.parent_path(), "needle");

  const auto summary = engine.search(query, [&](uburu::SearchResult result) {
    results.push_back(std::move(result));
    return true;
  });

  REQUIRE(results.size() == 2);
  CHECK(results[1].path == std::filesystem::path("second.txt"));
  CHECK(summary.matches == 2);
}

TEST_CASE("direct search supports CRLF, LF, empty lines and files without final newline")
{
  const uburu::tests::TemporaryFile file("uburu-search-line-ending-test.txt");
  const auto& path = file.path();
  uburu::tests::writeFile(path, "needle\r\n\nlast needle");

  auto scanner = std::make_shared<SingleFileScanner>(path);
  uburu::search::DirectSearchEngine engine(scanner);
  uburu::SearchQuery query = makeQuery(path.parent_path(), "needle");
  std::vector<uburu::SearchResult> results;

  const auto summary = engine.search(query, [&](uburu::SearchResult result) {
    results.push_back(std::move(result));
    return true;
  });

  REQUIRE(results.size() == 2);
  CHECK(results[0].line == 1);
  CHECK(results[0].column == 1);
  CHECK(results[1].line == 3);
  CHECK(results[1].column == 6);
  CHECK(summary.matches == 2);
}

TEST_CASE("direct search decodes UTF-16 content before matching")
{
  const uburu::tests::TemporaryFile file("uburu-search-utf16-content.txt");
  const auto& path = file.path();
  uburu::tests::writeBytes(path,
                           {0xFFU, 0xFEU, 'n', 0x00U, 'e', 0x00U, 'e', 0x00U, 'd', 0x00U, 'l', 0x00U, 'e', 0x00U});

  auto scanner = std::make_shared<SingleFileScanner>(path);
  uburu::search::DirectSearchEngine engine(scanner);
  uburu::SearchQuery query = makeQuery(path.parent_path(), "needle");
  std::vector<uburu::SearchResult> results;

  const auto summary = engine.search(query, [&](uburu::SearchResult result) {
    results.push_back(std::move(result));

    return true;
  });

  REQUIRE(results.size() == 1);
  CHECK(results.front().lineText == "needle");
  CHECK(results.front().column == 1);
  CHECK(summary.matches == 1);
}

TEST_CASE("direct search uses visible text for html content")
{
  const uburu::tests::TemporaryDirectory directory("uburu-direct-search-html-content-test");
  const auto path = directory.path() / "page.html";
  uburu::tests::writeFile(path, "<body><h1>Visible needle</h1><script>hiddenNeedle()</script></body>");

  auto scanner = std::make_shared<SingleFileScanner>(path, "page.html");
  uburu::search::DirectSearchEngine engine(scanner);
  uburu::SearchQuery visibleQuery = makeQuery(directory.path(), "needle");
  uburu::SearchQuery hiddenQuery = makeQuery(directory.path(), "hiddenNeedle");
  std::vector<uburu::SearchResult> visibleResults;
  std::vector<uburu::SearchResult> hiddenResults;

  const auto visibleSummary = engine.search(visibleQuery, [&](uburu::SearchResult result) {
    visibleResults.push_back(std::move(result));

    return true;
  });
  const auto hiddenSummary = engine.search(hiddenQuery, [&](uburu::SearchResult result) {
    hiddenResults.push_back(std::move(result));

    return true;
  });

  REQUIRE(visibleResults.size() == 1);
  CHECK(visibleResults.front().lineText == "Visible needle");
  CHECK(visibleResults.front().line == 1);
  CHECK(visibleSummary.matches == 1);
  CHECK(hiddenResults.empty());
  CHECK(hiddenSummary.matches == 0);
}

TEST_CASE("direct search skips sampled binary files without reporting read errors")
{
  const uburu::tests::TemporaryFile file("uburu-search-binary-content.bin");
  const auto& path = file.path();
  uburu::tests::writeBytes(path, {'n', 'e', 'e', 'd', 'l', 'e', 0x00U, 'n', 'e', 'e', 'd', 'l', 'e'});

  auto scanner = std::make_shared<SingleFileScanner>(path);
  uburu::search::DirectSearchEngine engine(scanner);
  uburu::SearchQuery query = makeQuery(path.parent_path(), "needle");
  std::vector<uburu::SearchResult> results;

  const auto summary = engine.search(query, [&](uburu::SearchResult result) {
    results.push_back(std::move(result));

    return true;
  });

  CHECK(results.empty());
  CHECK(summary.matches == 0);
  CHECK_FALSE(summary.partialFailure);
  CHECK(summary.filesWithReadErrors == 0);
  CHECK(summary.metrics.filesProcessed == 1);
  CHECK(summary.metrics.binaryFiles == 1);
  CHECK(summary.metrics.binaryFilesSkipped == 1);
  CHECK(summary.metrics.resultsEmitted == 0);
}

TEST_CASE("direct search records processed file and result metrics")
{
  const uburu::tests::TemporaryFile file("uburu-search-metrics-content.txt");
  const auto& path = file.path();
  uburu::tests::writeFile(path, "needle\n");

  auto scanner = std::make_shared<SingleFileScanner>(path);
  uburu::search::DirectSearchEngine engine(scanner);
  uburu::SearchQuery query = makeQuery(path.parent_path(), "needle");

  const auto summary = engine.search(query, [](uburu::SearchResult) { return true; });

  CHECK(summary.matches == 1);
  CHECK(summary.metrics.filesProcessed == 1);
  CHECK(summary.metrics.bytesProcessed == 7);
  CHECK(summary.metrics.resultsEmitted == 1);
}

TEST_CASE("direct search returns context lines and highlight spans")
{
  const uburu::tests::TemporaryFile file("uburu-search-context-test.txt");
  const auto& path = file.path();
  uburu::tests::writeFile(path, "before\nneedle and needle\nafter\n");

  auto scanner = std::make_shared<SingleFileScanner>(path);
  uburu::search::DirectSearchEngine engine(scanner);
  uburu::SearchQuery query = makeQuery(path.parent_path(), "needle");
  query.options.contextBeforeLines = 1;
  query.options.contextAfterLines = 1;
  std::vector<uburu::SearchResult> results;

  const auto summary = engine.search(query, [&](uburu::SearchResult result) {
    results.push_back(std::move(result));

    return true;
  });

  REQUIRE(results.size() == 2);
  CHECK(results.front().contextBefore == std::vector<std::string>{"before"});
  CHECK(results.front().contextAfter == std::vector<std::string>{"after"});
  REQUIRE(results.front().highlights.size() == 2);
  CHECK(results.front().highlights[0].column == 1);
  CHECK(results.front().highlights[1].column == 12);
  CHECK(summary.matches == 2);
}

TEST_CASE("direct search applies the global result limit before publishing")
{
  const uburu::tests::TemporaryFile file("uburu-search-limit-test.txt");
  const auto& path = file.path();
  uburu::tests::writeFile(path, "needle needle\n");

  auto scanner = std::make_shared<SingleFileScanner>(path);
  uburu::search::DirectSearchEngine engine(scanner);
  uburu::SearchQuery query = makeQuery(path.parent_path(), "needle");
  query.options.resultLimit = 1;
  std::vector<uburu::SearchResult> results;

  const auto summary = engine.search(query, [&](uburu::SearchResult result) {
    results.push_back(std::move(result));
    return true;
  });

  REQUIRE(results.size() == 1);
  CHECK(results.front().column == 1);
  CHECK(summary.matches == 1);
  CHECK(summary.limitReached);
}

TEST_CASE("direct search applies the per-file result limit without stopping the full search")
{
  const uburu::tests::TemporaryFile file("uburu-search-file-limit-test.txt");
  const auto& path = file.path();
  uburu::tests::writeFile(path, "needle needle needle\n");

  auto scanner = std::make_shared<SingleFileScanner>(path);
  uburu::search::DirectSearchEngine engine(scanner);
  uburu::SearchQuery query = makeQuery(path.parent_path(), "needle");
  query.options.perFileResultLimit = 2;
  std::vector<uburu::SearchResult> results;

  const auto summary = engine.search(query, [&](uburu::SearchResult result) {
    results.push_back(std::move(result));
    return true;
  });

  REQUIRE(results.size() == 2);
  CHECK(summary.matches == 2);
  CHECK(summary.filesWithMatchLimitReached == 1);
  CHECK_FALSE(summary.limitReached);
}

TEST_CASE("direct search supports regex queries")
{
  const uburu::tests::TemporaryFile file("uburu-search-regex-test.txt");
  const auto& path = file.path();
  uburu::tests::writeFile(path, "todo(1) skip todo(42)\n");

  auto scanner = std::make_shared<SingleFileScanner>(path);
  uburu::search::DirectSearchEngine engine(scanner);
  uburu::SearchQuery query = makeQuery(path.parent_path(), R"(todo\(\d+\))");
  query.options.mode = uburu::SearchMode::regex;
  std::vector<uburu::SearchResult> results;

  const auto summary = engine.search(query, [&](uburu::SearchResult result) {
    results.push_back(std::move(result));
    return true;
  });

#ifdef UBURU_HAS_PCRE2
  REQUIRE(results.size() == 2);
  CHECK(results[0].column == 1);
  CHECK(results[0].matchLength == 7);
  CHECK(results[1].column == 14);
  CHECK(results[1].matchLength == 8);
  CHECK(summary.matches == 2);
  CHECK(summary.regexExecutionMode != uburu::search::RegexExecutionMode::notUsed);
#else
  CHECK(results.empty());
  REQUIRE(summary.errors.size() == 1);
  CHECK(summary.errors.front().code == uburu::search::SearchErrorCode::unsupportedSearchMode);
#endif
}

TEST_CASE("direct search can target file names without opening file content")
{
  const auto missingPath = uburu::tests::uniqueTemporaryPath("uburu-search-file-name-target-missing-file.cpp");

  auto scanner = std::make_shared<SingleFileScanner>(missingPath);
  uburu::search::DirectSearchEngine engine(scanner);
  uburu::SearchQuery query = makeQuery(missingPath.parent_path(), "source");
  query.options.target = uburu::SearchTarget::fileName;
  std::vector<uburu::SearchResult> results;

  const auto summary = engine.search(query, [&](uburu::SearchResult result) {
    results.push_back(std::move(result));
    return true;
  });

  REQUIRE(results.size() == 1);
  CHECK(results.front().kind == uburu::SearchResultKind::fileName);
  CHECK(results.front().path == std::filesystem::path("source.cpp"));
  CHECK(results.front().line == 0);
  CHECK(results.front().column == 1);
  CHECK(results.front().lineText == "source.cpp");
  CHECK(summary.matches == 1);
  CHECK(summary.errors.empty());
}

TEST_CASE("direct search can target file names and content in the same query")
{
  const uburu::tests::TemporaryFile file("uburu-search-file-name-and-content-target-test.txt");
  const auto& path = file.path();
  uburu::tests::writeFile(path, "source appears in content\n");

  auto scanner = std::make_shared<SingleFileScanner>(path);
  uburu::search::DirectSearchEngine engine(scanner);
  uburu::SearchQuery query = makeQuery(path.parent_path(), "source");
  query.options.target = uburu::SearchTarget::contentAndFileName;
  std::vector<uburu::SearchResult> results;

  const auto summary = engine.search(query, [&](uburu::SearchResult result) {
    results.push_back(std::move(result));
    return true;
  });

  REQUIRE(results.size() == 2);
  CHECK(results[0].kind == uburu::SearchResultKind::fileName);
  CHECK(results[1].kind == uburu::SearchResultKind::content);
  CHECK(results[1].line == 1);
  CHECK(summary.matches == 2);
}

TEST_CASE("direct search reports file open failures as partial failures")
{
  const auto missingPath = uburu::tests::uniqueTemporaryPath("uburu-search-missing-content-file.txt");

  auto scanner = std::make_shared<SingleFileScanner>(missingPath);
  uburu::search::DirectSearchEngine engine(scanner);
  uburu::SearchQuery query = makeQuery(missingPath.parent_path(), "needle");

  const auto summary = engine.search(query, [](uburu::SearchResult) { return true; });

  CHECK(summary.partialFailure);
  CHECK(summary.filesWithReadErrors == 1);
  REQUIRE(summary.errors.size() == 1);
  CHECK(summary.errors.front().code == uburu::search::SearchErrorCode::fileOpenFailed);
  CHECK(summary.errors.front().translationKey == "search.error.fileOpenFailed");
}

TEST_CASE("direct search reports files removed between scan and read as partial failures")
{
  const uburu::tests::TemporaryFile file("uburu-search-removed-after-scan.txt");
  const auto& path = file.path();
  uburu::tests::writeFile(path, "needle\n");

  auto scanner = std::make_shared<DeletingScanner>(path);
  uburu::search::DirectSearchEngine engine(scanner);
  uburu::SearchQuery query = makeQuery(path.parent_path(), "needle");

  const auto summary = engine.search(query, [](uburu::SearchResult) { return true; });

  CHECK(summary.partialFailure);
  CHECK(summary.filesWithReadErrors == 1);
  REQUIRE(summary.errors.size() == 1);
  CHECK(summary.errors.front().code == uburu::search::SearchErrorCode::fileOpenFailed);
  CHECK(summary.errors.front().context == "removed.txt");
}

TEST_CASE("direct search reports regex compilation errors")
{
  auto scanner = std::make_shared<EmptyScanner>();
  uburu::search::DirectSearchEngine engine(scanner);
  uburu::SearchQuery query = makeQuery(std::filesystem::temp_directory_path(), "(");
  query.options.mode = uburu::SearchMode::regex;

  const auto summary = engine.search(query, [](uburu::SearchResult) { return true; });

#ifdef UBURU_HAS_PCRE2
  CHECK(scanner->calls == 0);
  REQUIRE(summary.errors.size() == 1);
  CHECK(summary.errors.front().code == uburu::search::SearchErrorCode::regexCompileFailed);
  CHECK(summary.errors.front().translationKey == "search.error.regexCompileFailed");
  CHECK(!summary.errors.front().context.empty());
  REQUIRE(summary.errors.front().offset.has_value());
#else
  CHECK(scanner->calls == 0);
  REQUIRE(summary.errors.size() == 1);
  CHECK(summary.errors.front().code == uburu::search::SearchErrorCode::unsupportedSearchMode);
#endif
}

TEST_CASE("direct search stops when regex times out")
{
  const uburu::tests::TemporaryFile file("uburu-search-regex-timeout-test.txt");
  const auto& path = file.path();
  uburu::tests::writeFile(path, "needle\n");

  auto scanner = std::make_shared<SingleFileScanner>(path);
  uburu::search::DirectSearchEngine engine(scanner);
  uburu::SearchQuery query = makeQuery(path.parent_path(), "needle");
  query.options.mode = uburu::SearchMode::regex;
  query.options.regexTimeout = std::chrono::milliseconds{0};

  const auto summary = engine.search(query, [](uburu::SearchResult) { return true; });

#ifdef UBURU_HAS_PCRE2
  auto foundRegexTimeout = false;

  for (const auto& searchError : summary.errors) {
    if (searchError.code == uburu::search::SearchErrorCode::regexTimeout)
      foundRegexTimeout = true;
  }

  CHECK(foundRegexTimeout);
#else
  REQUIRE(summary.errors.size() == 1);
  CHECK(summary.errors.front().code == uburu::search::SearchErrorCode::unsupportedSearchMode);
#endif
}
