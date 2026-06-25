#include "core/filesystem/file-scanner.hpp"
#include "core/search/direct-search-engine.hpp"

#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
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
    void scan(const std::filesystem::path&, const uburu::SearchOptions&, uburu::filesystem::FileSink,
              std::stop_token, uburu::diagnostics::SearchMetrics*) const override
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
      std::vector<std::filesystem::path> excluded_directories;
    };

    void scan(const std::filesystem::path& root, const uburu::SearchOptions& options,
              uburu::filesystem::FileSink sink, std::stop_token,
              uburu::diagnostics::SearchMetrics*) const override
    {
      calls.push_back(Call{.root = root, .excluded_directories = options.excluded_directories});

      const auto file_name = "source-" + std::to_string(calls.size()) + ".txt";
      const auto absolute_path = root / file_name;
      {
        std::ofstream file(absolute_path, std::ios::binary);
        file << "needle\n";
      }

      sink(uburu::FileEntry{.absolute_path = absolute_path,
                            .relative_path = file_name,
                            .size = 7,
                            .modified_at = {},
                            .hidden = false,
                            .binary = false,
                            .symlink = false,
                            .sparse = false,
                            .search_root = root});
    }

    mutable std::vector<Call> calls;
  };

  class SingleFileScanner final : public uburu::filesystem::FileScanner
  {
  public:
    explicit SingleFileScanner(std::filesystem::path path) : path_(std::move(path)) {}
    void scan(const std::filesystem::path&, const uburu::SearchOptions&, uburu::filesystem::FileSink sink,
              std::stop_token, uburu::diagnostics::SearchMetrics*) const override
    {
      sink(uburu::FileEntry{.absolute_path = path_,
                            .relative_path = "source.cpp",
                            .size = file_size(path_),
                            .modified_at = {},
                            .hidden = false,
                            .binary = false,
                            .symlink = false,
                            .sparse = false,
                            .search_root = {}});
    }

  private:
    static std::uintmax_t file_size(const std::filesystem::path& path)
    {
      std::error_code error;
      const auto size = std::filesystem::file_size(path, error);

      return error ? 0 : size;
    }

    std::filesystem::path path_;
  };

  class ObservingScanner final : public uburu::filesystem::FileScanner
  {
  public:
    ObservingScanner(std::filesystem::path first_path, std::filesystem::path second_path,
                     std::function<void()> after_first_entry)
        : first_path_(std::move(first_path)), second_path_(std::move(second_path)),
          after_first_entry_(std::move(after_first_entry))
    {}

    void scan(const std::filesystem::path&, const uburu::SearchOptions&, uburu::filesystem::FileSink sink,
              std::stop_token, uburu::diagnostics::SearchMetrics*) const override
    {
      sink(uburu::FileEntry{.absolute_path = first_path_,
                            .relative_path = "first.txt",
                            .size = 0,
                            .modified_at = {},
                            .hidden = false,
                            .binary = false,
                            .symlink = false,
                            .sparse = false,
                            .search_root = {}});
      after_first_entry_();
      sink(uburu::FileEntry{.absolute_path = second_path_,
                            .relative_path = "second.txt",
                            .size = 0,
                            .modified_at = {},
                            .hidden = false,
                            .binary = false,
                            .symlink = false,
                            .sparse = false,
                            .search_root = {}});
    }

  private:
    std::filesystem::path first_path_;
    std::filesystem::path second_path_;
    std::function<void()> after_first_entry_;
  };

  class DeletingScanner final : public uburu::filesystem::FileScanner
  {
  public:
    explicit DeletingScanner(std::filesystem::path path) : path_(std::move(path)) {}

    void scan(const std::filesystem::path&, const uburu::SearchOptions&, uburu::filesystem::FileSink sink,
              std::stop_token, uburu::diagnostics::SearchMetrics*) const override
    {
      std::filesystem::remove(path_);
      sink(uburu::FileEntry{.absolute_path = path_,
                            .relative_path = "removed.txt",
                            .size = 0,
                            .modified_at = {},
                            .hidden = false,
                            .binary = false,
                            .symlink = false,
                            .sparse = false,
                            .search_root = {}});
    }

  private:
    std::filesystem::path path_;
  };

  uburu::SearchQuery make_query(std::filesystem::path root, std::string expression)
  {
    return {.root = std::move(root), .scope = {}, .expression = std::move(expression), .options = {}};
  }

  void write_bytes(const std::filesystem::path& path, const std::vector<unsigned char>& bytes)
  {
    std::ofstream file(path, std::ios::binary);
    for (const auto byte : bytes)
      file.put(static_cast<char>(byte));
  }

} // namespace

TEST_CASE("an empty expression does not start filesystem traversal")
{
  auto scanner = std::make_shared<EmptyScanner>();
  uburu::search::DirectSearchEngine engine(scanner);
  uburu::SearchQuery query = make_query(std::filesystem::temp_directory_path(), "");

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
  uburu::SearchQuery query = make_query(
      std::filesystem::temp_directory_path() / "uburu-direct-search-missing-root", "needle");
  std::error_code error;
  std::filesystem::remove_all(query.root, error);

  const auto summary = engine.search(query, [](uburu::SearchResult) { return true; });

  CHECK(scanner->calls == 0);
  CHECK(summary.files_scanned == 0);
  CHECK(summary.matches == 0);
  REQUIRE(summary.errors.size() == 1);
  CHECK(summary.errors.front().code == uburu::search::SearchErrorCode::root_not_found);
}

TEST_CASE("direct search scans every root in the search scope")
{
  auto scanner = std::make_shared<CapturingScanner>();
  uburu::search::DirectSearchEngine engine(scanner);
  const auto first_root = std::filesystem::temp_directory_path() / "uburu-direct-search-scope-first";
  const auto second_root = std::filesystem::temp_directory_path() / "uburu-direct-search-scope-second";
  std::filesystem::create_directories(first_root);
  std::filesystem::create_directories(second_root);

  uburu::SearchQuery query{
    .root = {},
    .scope = uburu::SearchScope{
      .roots = {uburu::SearchRoot{.path = first_root,
                                  .included_directories = {},
                                  .excluded_directories = {"node_modules"}},
                uburu::SearchRoot{.path = second_root,
                                  .included_directories = {},
                                  .excluded_directories = {"node_modules"}}}},
    .expression = "needle",
    .options = {}};

  std::vector<uburu::SearchResult> results;
  const auto summary = engine.search(query, [&](uburu::SearchResult result) {
    results.push_back(std::move(result));

    return true;
  });

  std::error_code error;
  std::filesystem::remove_all(first_root, error);
  std::filesystem::remove_all(second_root, error);

  REQUIRE(scanner->calls.size() == 2);
  CHECK(scanner->calls.front().root == first_root);
  CHECK(scanner->calls.back().root == second_root);
  REQUIRE(scanner->calls.front().excluded_directories.size() == 1);
  CHECK(scanner->calls.front().excluded_directories.front() == "node_modules");
  CHECK(summary.matches == 2);
  REQUIRE(results.size() == 2);
  CHECK(results.front().search_root == first_root);
  CHECK(results.back().search_root == second_root);
}

TEST_CASE("a pre-cancelled search reports cancellation")
{
  auto scanner = std::make_shared<EmptyScanner>();
  uburu::search::DirectSearchEngine engine(scanner);
  std::stop_source cancellation;
  cancellation.request_stop();
  uburu::SearchQuery query = make_query(std::filesystem::temp_directory_path(), "needle");

  const auto summary =
      engine.search(query, [](uburu::SearchResult) { return true; }, cancellation.get_token());

  CHECK(summary.cancelled);
}

TEST_CASE("a pre-cancelled regex search reports cancellation")
{
  auto scanner = std::make_shared<EmptyScanner>();
  uburu::search::DirectSearchEngine engine(scanner);
  std::stop_source cancellation;
  cancellation.request_stop();
  uburu::SearchQuery query = make_query(std::filesystem::temp_directory_path(), "needle");
  query.options.mode = uburu::SearchMode::regex;

  const auto summary =
      engine.search(query, [](uburu::SearchResult) { return true; }, cancellation.get_token());

#ifdef UBURU_HAS_PCRE2
  CHECK(summary.cancelled);
#else
  CHECK_FALSE(summary.cancelled);
  REQUIRE(summary.errors.size() == 1);
  CHECK(summary.errors.front().code == uburu::search::SearchErrorCode::unsupported_search_mode);
#endif
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
  uburu::SearchQuery query = make_query(std::filesystem::temp_directory_path(), "needle");
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
  uburu::SearchQuery query = make_query(std::filesystem::temp_directory_path(), "needle");
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

TEST_CASE("direct search publishes results before scanner completion")
{
  const auto first_path = std::filesystem::temp_directory_path() / "uburu-progressive-first.txt";
  const auto second_path = std::filesystem::temp_directory_path() / "uburu-progressive-second.txt";
  {
    std::ofstream first_file(first_path, std::ios::binary);
    first_file << "needle\n";
    std::ofstream second_file(second_path, std::ios::binary);
    second_file << "needle\n";
  }
  const auto cleanup = [&] {
    std::filesystem::remove(first_path);
    std::filesystem::remove(second_path);
  };

  std::vector<uburu::SearchResult> results;
  auto scanner = std::make_shared<ObservingScanner>(first_path, second_path, [&] {
    REQUIRE(results.size() == 1);
    CHECK(results.front().path == std::filesystem::path("first.txt"));
  });
  uburu::search::DirectSearchEngine engine(scanner);
  uburu::SearchQuery query = make_query(std::filesystem::temp_directory_path(), "needle");

  const auto summary = engine.search(query, [&](uburu::SearchResult result) {
    results.push_back(std::move(result));
    return true;
  });
  cleanup();

  REQUIRE(results.size() == 2);
  CHECK(results[1].path == std::filesystem::path("second.txt"));
  CHECK(summary.matches == 2);
}

TEST_CASE("direct search supports CRLF, LF, empty lines and files without final newline")
{
  const auto path = std::filesystem::temp_directory_path() / "uburu-search-line-ending-test.txt";
  {
    std::ofstream file(path, std::ios::binary);
    file << "needle\r\n\nlast needle";
  }
  const auto cleanup = [&] { std::filesystem::remove(path); };

  auto scanner = std::make_shared<SingleFileScanner>(path);
  uburu::search::DirectSearchEngine engine(scanner);
  uburu::SearchQuery query = make_query(std::filesystem::temp_directory_path(), "needle");
  std::vector<uburu::SearchResult> results;

  const auto summary = engine.search(query, [&](uburu::SearchResult result) {
    results.push_back(std::move(result));
    return true;
  });
  cleanup();

  REQUIRE(results.size() == 2);
  CHECK(results[0].line == 1);
  CHECK(results[0].column == 1);
  CHECK(results[1].line == 3);
  CHECK(results[1].column == 6);
  CHECK(summary.matches == 2);
}

TEST_CASE("direct search decodes UTF-16 content before matching")
{
  const auto path = std::filesystem::temp_directory_path() / "uburu-search-utf16-content.txt";
  write_bytes(
      path, {0xFFU, 0xFEU, 'n', 0x00U, 'e', 0x00U, 'e', 0x00U, 'd', 0x00U, 'l', 0x00U, 'e', 0x00U});
  const auto cleanup = [&] { std::filesystem::remove(path); };

  auto scanner = std::make_shared<SingleFileScanner>(path);
  uburu::search::DirectSearchEngine engine(scanner);
  uburu::SearchQuery query = make_query(std::filesystem::temp_directory_path(), "needle");
  std::vector<uburu::SearchResult> results;

  const auto summary = engine.search(query, [&](uburu::SearchResult result) {
    results.push_back(std::move(result));

    return true;
  });
  cleanup();

  REQUIRE(results.size() == 1);
  CHECK(results.front().line_text == "needle");
  CHECK(results.front().column == 1);
  CHECK(summary.matches == 1);
}

TEST_CASE("direct search skips sampled binary files without reporting read errors")
{
  const auto path = std::filesystem::temp_directory_path() / "uburu-search-binary-content.bin";
  write_bytes(path, {'n', 'e', 'e', 'd', 'l', 'e', 0x00U, 'n', 'e', 'e', 'd', 'l', 'e'});
  const auto cleanup = [&] { std::filesystem::remove(path); };

  auto scanner = std::make_shared<SingleFileScanner>(path);
  uburu::search::DirectSearchEngine engine(scanner);
  uburu::SearchQuery query = make_query(std::filesystem::temp_directory_path(), "needle");
  std::vector<uburu::SearchResult> results;

  const auto summary = engine.search(query, [&](uburu::SearchResult result) {
    results.push_back(std::move(result));

    return true;
  });
  cleanup();

  CHECK(results.empty());
  CHECK(summary.matches == 0);
  CHECK_FALSE(summary.partial_failure);
  CHECK(summary.files_with_read_errors == 0);
  CHECK(summary.metrics.files_processed == 1);
  CHECK(summary.metrics.binary_files == 1);
  CHECK(summary.metrics.binary_files_skipped == 1);
  CHECK(summary.metrics.results_emitted == 0);
}

TEST_CASE("direct search records processed file and result metrics")
{
  const auto path = std::filesystem::temp_directory_path() / "uburu-search-metrics-content.txt";
  {
    std::ofstream file(path, std::ios::binary);
    file << "needle\n";
  }
  const auto cleanup = [&] { std::filesystem::remove(path); };

  auto scanner = std::make_shared<SingleFileScanner>(path);
  uburu::search::DirectSearchEngine engine(scanner);
  uburu::SearchQuery query = make_query(std::filesystem::temp_directory_path(), "needle");

  const auto summary = engine.search(query, [](uburu::SearchResult) { return true; });
  cleanup();

  CHECK(summary.matches == 1);
  CHECK(summary.metrics.files_processed == 1);
  CHECK(summary.metrics.bytes_processed == 7);
  CHECK(summary.metrics.results_emitted == 1);
}

TEST_CASE("direct search returns context lines and highlight spans")
{
  const auto path = std::filesystem::temp_directory_path() / "uburu-search-context-test.txt";
  {
    std::ofstream file(path, std::ios::binary);
    file << "before\nneedle and needle\nafter\n";
  }
  const auto cleanup = [&] { std::filesystem::remove(path); };

  auto scanner = std::make_shared<SingleFileScanner>(path);
  uburu::search::DirectSearchEngine engine(scanner);
  uburu::SearchQuery query = make_query(std::filesystem::temp_directory_path(), "needle");
  query.options.context_before_lines = 1;
  query.options.context_after_lines = 1;
  std::vector<uburu::SearchResult> results;

  const auto summary = engine.search(query, [&](uburu::SearchResult result) {
    results.push_back(std::move(result));

    return true;
  });
  cleanup();

  REQUIRE(results.size() == 2);
  CHECK(results.front().context_before == std::vector<std::string>{"before"});
  CHECK(results.front().context_after == std::vector<std::string>{"after"});
  REQUIRE(results.front().highlights.size() == 2);
  CHECK(results.front().highlights[0].column == 1);
  CHECK(results.front().highlights[1].column == 12);
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
  uburu::SearchQuery query = make_query(std::filesystem::temp_directory_path(), "needle");
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

TEST_CASE("direct search applies the per-file result limit without stopping the full search")
{
  const auto path = std::filesystem::temp_directory_path() / "uburu-search-file-limit-test.txt";
  {
    std::ofstream file(path, std::ios::binary);
    file << "needle needle needle\n";
  }
  const auto cleanup = [&] { std::filesystem::remove(path); };

  auto scanner = std::make_shared<SingleFileScanner>(path);
  uburu::search::DirectSearchEngine engine(scanner);
  uburu::SearchQuery query = make_query(std::filesystem::temp_directory_path(), "needle");
  query.options.per_file_result_limit = 2;
  std::vector<uburu::SearchResult> results;

  const auto summary = engine.search(query, [&](uburu::SearchResult result) {
    results.push_back(std::move(result));
    return true;
  });
  cleanup();

  REQUIRE(results.size() == 2);
  CHECK(summary.matches == 2);
  CHECK(summary.files_with_match_limit_reached == 1);
  CHECK_FALSE(summary.limit_reached);
}

TEST_CASE("direct search supports regex queries")
{
  const auto path = std::filesystem::temp_directory_path() / "uburu-search-regex-test.txt";
  {
    std::ofstream file(path, std::ios::binary);
    file << "todo(1) skip todo(42)\n";
  }
  const auto cleanup = [&] { std::filesystem::remove(path); };

  auto scanner = std::make_shared<SingleFileScanner>(path);
  uburu::search::DirectSearchEngine engine(scanner);
  uburu::SearchQuery query = make_query(std::filesystem::temp_directory_path(), R"(todo\(\d+\))");
  query.options.mode = uburu::SearchMode::regex;
  std::vector<uburu::SearchResult> results;

  const auto summary = engine.search(query, [&](uburu::SearchResult result) {
    results.push_back(std::move(result));
    return true;
  });
  cleanup();

#ifdef UBURU_HAS_PCRE2
  REQUIRE(results.size() == 2);
  CHECK(results[0].column == 1);
  CHECK(results[0].match_length == 7);
  CHECK(results[1].column == 14);
  CHECK(results[1].match_length == 8);
  CHECK(summary.matches == 2);
  CHECK(summary.regex_execution_mode != uburu::search::RegexExecutionMode::not_used);
#else
  CHECK(results.empty());
  REQUIRE(summary.errors.size() == 1);
  CHECK(summary.errors.front().code == uburu::search::SearchErrorCode::unsupported_search_mode);
#endif
}

TEST_CASE("direct search can target file names without opening file content")
{
  const auto missing_path =
      std::filesystem::temp_directory_path() / "uburu-search-file-name-target-missing-file.cpp";

  auto scanner = std::make_shared<SingleFileScanner>(missing_path);
  uburu::search::DirectSearchEngine engine(scanner);
  uburu::SearchQuery query = make_query(std::filesystem::temp_directory_path(), "source");
  query.options.target = uburu::SearchTarget::file_name;
  std::vector<uburu::SearchResult> results;

  const auto summary = engine.search(query, [&](uburu::SearchResult result) {
    results.push_back(std::move(result));
    return true;
  });

  REQUIRE(results.size() == 1);
  CHECK(results.front().kind == uburu::SearchResultKind::file_name);
  CHECK(results.front().path == std::filesystem::path("source.cpp"));
  CHECK(results.front().line == 0);
  CHECK(results.front().column == 1);
  CHECK(results.front().line_text == "source.cpp");
  CHECK(summary.matches == 1);
  CHECK(summary.errors.empty());
}

TEST_CASE("direct search can target file names and content in the same query")
{
  const auto path =
      std::filesystem::temp_directory_path() / "uburu-search-file-name-and-content-target-test.txt";
  {
    std::ofstream file(path, std::ios::binary);
    file << "source appears in content\n";
  }
  const auto cleanup = [&] { std::filesystem::remove(path); };

  auto scanner = std::make_shared<SingleFileScanner>(path);
  uburu::search::DirectSearchEngine engine(scanner);
  uburu::SearchQuery query = make_query(std::filesystem::temp_directory_path(), "source");
  query.options.target = uburu::SearchTarget::content_and_file_name;
  std::vector<uburu::SearchResult> results;

  const auto summary = engine.search(query, [&](uburu::SearchResult result) {
    results.push_back(std::move(result));
    return true;
  });
  cleanup();

  REQUIRE(results.size() == 2);
  CHECK(results[0].kind == uburu::SearchResultKind::file_name);
  CHECK(results[1].kind == uburu::SearchResultKind::content);
  CHECK(results[1].line == 1);
  CHECK(summary.matches == 2);
}

TEST_CASE("direct search reports file open failures as partial failures")
{
  const auto missing_path =
      std::filesystem::temp_directory_path() / "uburu-search-missing-content-file.txt";

  auto scanner = std::make_shared<SingleFileScanner>(missing_path);
  uburu::search::DirectSearchEngine engine(scanner);
  uburu::SearchQuery query = make_query(std::filesystem::temp_directory_path(), "needle");

  const auto summary = engine.search(query, [](uburu::SearchResult) { return true; });

  CHECK(summary.partial_failure);
  CHECK(summary.files_with_read_errors == 1);
  REQUIRE(summary.errors.size() == 1);
  CHECK(summary.errors.front().code == uburu::search::SearchErrorCode::file_open_failed);
  CHECK(summary.errors.front().translation_key == "search.error.fileOpenFailed");
}

TEST_CASE("direct search reports files removed between scan and read as partial failures")
{
  const auto path = std::filesystem::temp_directory_path() / "uburu-search-removed-after-scan.txt";
  {
    std::ofstream file(path, std::ios::binary);
    file << "needle\n";
  }

  auto scanner = std::make_shared<DeletingScanner>(path);
  uburu::search::DirectSearchEngine engine(scanner);
  uburu::SearchQuery query = make_query(std::filesystem::temp_directory_path(), "needle");

  const auto summary = engine.search(query, [](uburu::SearchResult) { return true; });

  CHECK(summary.partial_failure);
  CHECK(summary.files_with_read_errors == 1);
  REQUIRE(summary.errors.size() == 1);
  CHECK(summary.errors.front().code == uburu::search::SearchErrorCode::file_open_failed);
  CHECK(summary.errors.front().context == "removed.txt");
}

TEST_CASE("direct search reports regex compilation errors")
{
  auto scanner = std::make_shared<EmptyScanner>();
  uburu::search::DirectSearchEngine engine(scanner);
  uburu::SearchQuery query = make_query(std::filesystem::temp_directory_path(), "(");
  query.options.mode = uburu::SearchMode::regex;

  const auto summary = engine.search(query, [](uburu::SearchResult) { return true; });

#ifdef UBURU_HAS_PCRE2
  CHECK(scanner->calls == 0);
  REQUIRE(summary.errors.size() == 1);
  CHECK(summary.errors.front().code == uburu::search::SearchErrorCode::regex_compile_failed);
  CHECK(summary.errors.front().translation_key == "search.error.regexCompileFailed");
  CHECK(!summary.errors.front().context.empty());
  REQUIRE(summary.errors.front().offset.has_value());
#else
  CHECK(scanner->calls == 0);
  REQUIRE(summary.errors.size() == 1);
  CHECK(summary.errors.front().code == uburu::search::SearchErrorCode::unsupported_search_mode);
#endif
}

TEST_CASE("direct search stops when regex times out")
{
  const auto path = std::filesystem::temp_directory_path() / "uburu-search-regex-timeout-test.txt";
  {
    std::ofstream file(path, std::ios::binary);
    file << "needle\n";
  }
  const auto cleanup = [&] { std::filesystem::remove(path); };

  auto scanner = std::make_shared<SingleFileScanner>(path);
  uburu::search::DirectSearchEngine engine(scanner);
  uburu::SearchQuery query = make_query(std::filesystem::temp_directory_path(), "needle");
  query.options.mode = uburu::SearchMode::regex;
  query.options.regex_timeout = std::chrono::milliseconds{0};

  const auto summary = engine.search(query, [](uburu::SearchResult) { return true; });
  cleanup();

#ifdef UBURU_HAS_PCRE2
  REQUIRE(summary.errors.size() == 1);
  CHECK(summary.errors.front().code == uburu::search::SearchErrorCode::regex_timeout);
#else
  REQUIRE(summary.errors.size() == 1);
  CHECK(summary.errors.front().code == uburu::search::SearchErrorCode::unsupported_search_mode);
#endif
}
