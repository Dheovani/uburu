#include "core/diagnostics/diagnostic-report.hpp"
#include "core/diagnostics/file-structured-logger.hpp"
#include "core/diagnostics/search-tracing.hpp"
#include "core/diagnostics/structured-logger.hpp"
#include "core/diagnostics/structured-metrics-sink.hpp"

#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <memory>
#include <stdexcept>
#include <string>
#include <system_error>

TEST_CASE("structured logger masks sensitive fields by default")
{
  uburu::diagnostics::InMemoryStructuredLogger logger;

  logger.write(uburu::diagnostics::LogEvent{
    .level = uburu::diagnostics::LogLevel::info,
    .category = uburu::diagnostics::LogCategory::search,
    .message = "event",
    .fields = {uburu::diagnostics::LogField{.key = "path", .value = "C:/private/file.cpp", .sensitive = true},
               uburu::diagnostics::LogField{.key = "results", .value = "2", .sensitive = false}},
    .timestamp = {}});

  REQUIRE(logger.entries().size() == 1);
  REQUIRE(logger.entries().front().fields.size() == 2);
  CHECK(logger.entries().front().fields[0].value == "<redacted>");
  CHECK(logger.entries().front().fields[1].value == "2");
}

TEST_CASE("structured logger can retain sensitive fields when explicitly enabled")
{
  uburu::diagnostics::InMemoryStructuredLogger logger(
    uburu::diagnostics::StructuredLogOptions{.includeSensitiveFields = true,
                                             .minimumLevel = uburu::diagnostics::LogLevel::trace,
                                             .enabledCategories = {}});

  logger.write(uburu::diagnostics::LogEvent{
    .level = uburu::diagnostics::LogLevel::debug,
    .category = uburu::diagnostics::LogCategory::filesystem,
    .message = "event",
    .fields = {uburu::diagnostics::LogField{.key = "path", .value = "C:/private/file.cpp", .sensitive = true}},
    .timestamp = {}});

  REQUIRE(logger.entries().size() == 1);
  REQUIRE(logger.entries().front().fields.size() == 1);
  CHECK(logger.entries().front().fields.front().value == "C:/private/file.cpp");
}

TEST_CASE("structured logger filters entries by minimum level and category")
{
  uburu::diagnostics::InMemoryStructuredLogger logger(
    uburu::diagnostics::StructuredLogOptions{.includeSensitiveFields = false,
                                             .minimumLevel = uburu::diagnostics::LogLevel::warning,
                                             .enabledCategories = {uburu::diagnostics::LogCategory::search}});

  logger.write(uburu::diagnostics::LogEvent{.level = uburu::diagnostics::LogLevel::info,
                                            .category = uburu::diagnostics::LogCategory::search,
                                            .message = "too low",
                                            .fields = {},
                                            .timestamp = {}});
  logger.write(uburu::diagnostics::LogEvent{.level = uburu::diagnostics::LogLevel::error,
                                            .category = uburu::diagnostics::LogCategory::git,
                                            .message = "wrong category",
                                            .fields = {},
                                            .timestamp = {}});
  logger.write(uburu::diagnostics::LogEvent{.level = uburu::diagnostics::LogLevel::warning,
                                            .category = uburu::diagnostics::LogCategory::search,
                                            .message = "accepted",
                                            .fields = {},
                                            .timestamp = {}});

  REQUIRE(logger.entries().size() == 1);
  CHECK(logger.entries().front().message == "accepted");
}

TEST_CASE("file structured logger writes sanitized json lines")
{
  const auto path = std::filesystem::temp_directory_path() / "uburu-file-structured-logger-test.log";
  std::error_code error;

  std::filesystem::remove(path, error);

  uburu::diagnostics::FileStructuredLogger logger(
    uburu::diagnostics::FileStructuredLogOptions{.structuredOptions = {},
                                                 .path = path,
                                                 .maximumFileSizeBytes = 1024,
                                                 .maximumRotatedFiles = 3});

  logger.write(uburu::diagnostics::LogEvent{
    .level = uburu::diagnostics::LogLevel::warning,
    .category = uburu::diagnostics::LogCategory::diagnostics,
    .message = "hello \"logger\"",
    .fields = {uburu::diagnostics::LogField{.key = "path", .value = "C:/private/file.cpp", .sensitive = true}},
    .timestamp = std::chrono::system_clock::time_point{std::chrono::milliseconds{42}}});

  std::ifstream stream(path, std::ios::binary);
  std::string line;

  std::getline(stream, line);

  CHECK(line.find("\"level\":\"[WARNING]\"") != std::string::npos);
  CHECK(line.find("\"category\":\"<diagnostics>\"") != std::string::npos);
  CHECK(line.find("hello \\\"logger\\\"") != std::string::npos);
  CHECK(line.find("<redacted>") != std::string::npos);

  std::filesystem::remove(path, error);
}

TEST_CASE("file structured logger rotates files when size limit is reached")
{
  const auto path = std::filesystem::temp_directory_path() / "uburu-file-structured-logger-rotation-test.log";
  const auto rotated = std::filesystem::path(path.string() + ".1");
  std::error_code error;

  std::filesystem::remove(path, error);
  std::filesystem::remove(rotated, error);

  uburu::diagnostics::FileStructuredLogger logger(
    uburu::diagnostics::FileStructuredLogOptions{.structuredOptions = {},
                                                 .path = path,
                                                 .maximumFileSizeBytes = 1,
                                                 .maximumRotatedFiles = 1});

  logger.write(uburu::diagnostics::LogEvent{.level = uburu::diagnostics::LogLevel::info,
                                            .category = uburu::diagnostics::LogCategory::diagnostics,
                                            .message = "first",
                                            .fields = {},
                                            .timestamp = {}});
  logger.write(uburu::diagnostics::LogEvent{.level = uburu::diagnostics::LogLevel::info,
                                            .category = uburu::diagnostics::LogCategory::diagnostics,
                                            .message = "second",
                                            .fields = {},
                                            .timestamp = {}});

  CHECK(std::filesystem::exists(path));
  CHECK(std::filesystem::exists(rotated));

  std::filesystem::remove(path, error);
  std::filesystem::remove(rotated, error);
}

TEST_CASE("search trace recorder stays empty when tracing is disabled")
{
  uburu::diagnostics::SearchTraceRecorder recorder;

  recorder.record("search", uburu::diagnostics::LogCategory::search, std::chrono::nanoseconds{7});

  {
    [[maybe_unused]] auto scope = uburu::diagnostics::traceSearchScope(recorder, "disabled scope");
  }

  CHECK_FALSE(recorder.enabled());
  CHECK(recorder.events().empty());
}

TEST_CASE("search trace recorder captures spans and masks sensitive fields")
{
  uburu::diagnostics::SearchTraceRecorder recorder(
    uburu::diagnostics::SearchTracingOptions{.enabled = true, .includeSensitiveFields = false, .maximumEvents = 2});

  recorder.record("instant",
                  uburu::diagnostics::LogCategory::search,
                  std::chrono::nanoseconds{7},
                  {uburu::diagnostics::LogField{.key = "path", .value = "C:/private/file.cpp", .sensitive = true}});

  {
    [[maybe_unused]] auto scope =
      uburu::diagnostics::traceSearchScope(recorder, "scoped", uburu::diagnostics::LogCategory::storage);
  }

  recorder.record("ignored", uburu::diagnostics::LogCategory::git, std::chrono::nanoseconds{1});

  REQUIRE(recorder.events().size() == 2);
  CHECK(recorder.events()[0].name == "instant");
  CHECK(recorder.events()[0].elapsed == std::chrono::nanoseconds{7});
  REQUIRE(recorder.events()[0].fields.size() == 1);
  CHECK(recorder.events()[0].fields.front().value == "<redacted>");
  CHECK(recorder.events()[1].name == "scoped");
  CHECK(recorder.events()[1].category == uburu::diagnostics::LogCategory::storage);
}

TEST_CASE("diagnostic report exports sanitized logs metrics and traces")
{
  uburu::diagnostics::DiagnosticReport report;
  report.generatedAt = std::chrono::system_clock::time_point{std::chrono::milliseconds{42}};
  report.logs.push_back(uburu::diagnostics::LogEvent{
    .level = uburu::diagnostics::LogLevel::error,
    .category = uburu::diagnostics::LogCategory::search,
    .message = "failure",
    .fields = {uburu::diagnostics::LogField{.key = "path", .value = "C:/private/file.cpp", .sensitive = true}},
    .timestamp = std::chrono::system_clock::time_point{std::chrono::milliseconds{43}}});
  uburu::diagnostics::SearchMetrics metrics;
  metrics.filesProcessed = 3;
  metrics.bytesProcessed = 128;
  metrics.resultsEmitted = 2;
  metrics.cacheHits = 1;
  metrics.memoryIncreased = true;
  report.searchMetrics.push_back(metrics);

  uburu::diagnostics::SearchTraceEvent traceEvent;
  traceEvent.name = "search";
  traceEvent.category = uburu::diagnostics::LogCategory::search;
  traceEvent.elapsed = std::chrono::nanoseconds{9};
  traceEvent.fields = {uburu::diagnostics::LogField{.key = "expression", .value = "secret", .sensitive = true}};
  report.traceEvents.push_back(std::move(traceEvent));

  const auto json = uburu::diagnostics::diagnosticReportJson(report);

  CHECK(json.find("\"product\":\"Uburu\"") != std::string::npos);
  CHECK(json.find("\"logs\"") != std::string::npos);
  CHECK(json.find("\"search_metrics\"") != std::string::npos);
  CHECK(json.find("\"trace_events\"") != std::string::npos);
  CHECK(json.find("\"files_processed\":3") != std::string::npos);
  CHECK(json.find("\"elapsed_ns\":9") != std::string::npos);
  CHECK(json.find("secret") == std::string::npos);
  CHECK(json.find("<redacted>") != std::string::npos);
}

TEST_CASE("diagnostic report can be exported to a file")
{
  const auto path = std::filesystem::temp_directory_path() / "uburu-diagnostic-report-test.json";
  std::error_code error;

  std::filesystem::remove(path, error);

  uburu::diagnostics::DiagnosticReport report;
  report.productName = "Uburu Test";
  report.generatedAt = std::chrono::system_clock::time_point{std::chrono::milliseconds{42}};

  uburu::diagnostics::exportDiagnosticReport(report, path);

  std::ifstream stream(path, std::ios::binary);
  std::string content;

  std::getline(stream, content);

  CHECK(content.find("Uburu Test") != std::string::npos);

  std::filesystem::remove(path, error);
}

TEST_CASE("structured metrics sink records search metrics as structured log fields")
{
  using namespace std::chrono_literals;

  constexpr auto timeToFirstResult = 7ns;
  constexpr auto totalTime = 11ns;
  constexpr std::uint64_t filesProcessed = 3;
  constexpr std::uint64_t bytesProcessed = 128;
  constexpr std::uint64_t filesPerSecond = 27;
  constexpr std::uint64_t bytesPerSecond = 512;
  constexpr std::uint64_t resultsEmitted = 2;
  constexpr std::uint64_t ignoredFiles = 5;
  constexpr std::uint64_t hiddenFiles = 1;
  constexpr std::uint64_t binaryFiles = 4;
  constexpr std::uint64_t binaryFilesSkipped = 4;
  constexpr std::uint64_t cacheHits = 8;
  constexpr std::uint64_t reusedByHash = 6;
  constexpr std::uint64_t approximateMemoryBytes = 4096;
  constexpr std::uint64_t memoryGrowthBytes = 128;

  auto logger = std::make_shared<uburu::diagnostics::InMemoryStructuredLogger>();
  uburu::diagnostics::StructuredMetricsSink sink(logger);

  uburu::diagnostics::SearchMetrics metrics;
  metrics.timeToFirstResult = timeToFirstResult;
  metrics.totalTime = totalTime;
  metrics.filesProcessed = filesProcessed;
  metrics.bytesProcessed = bytesProcessed;
  metrics.filesPerSecond = filesPerSecond;
  metrics.bytesPerSecond = bytesPerSecond;
  metrics.resultsEmitted = resultsEmitted;
  metrics.ignoredFiles = ignoredFiles;
  metrics.hiddenFiles = hiddenFiles;
  metrics.binaryFiles = binaryFiles;
  metrics.binaryFilesSkipped = binaryFilesSkipped;
  metrics.cacheHits = cacheHits;
  metrics.reusedByHash = reusedByHash;
  metrics.approximateMemoryBytes = approximateMemoryBytes;
  metrics.memoryGrowthBytes = memoryGrowthBytes;
  metrics.memoryIncreased = true;

  sink.record(metrics);

  REQUIRE(logger->entries().size() == 1);
  const auto& event = logger->entries().front();

  CHECK(event.level == uburu::diagnostics::LogLevel::info);
  CHECK(event.category == uburu::diagnostics::LogCategory::search);
  CHECK(event.message == "search metrics");
  REQUIRE(event.fields.size() == 21);
  CHECK(event.fields[0].key == "time_to_first_result_ns");
  CHECK(event.fields[0].value == "7");
  CHECK(event.fields[1].key == "total_time_ns");
  CHECK(event.fields[1].value == "11");
  CHECK(event.fields[4].key == "files_per_second");
  CHECK(event.fields[4].value == "27");
  CHECK(event.fields[5].key == "bytes_per_second");
  CHECK(event.fields[5].value == "512");
  CHECK(event.fields[6].key == "results_emitted");
  CHECK(event.fields[6].value == "2");
  CHECK(event.fields[13].key == "cache_hits");
  CHECK(event.fields[13].value == "8");
  CHECK(event.fields[17].key == "reused_by_hash");
  CHECK(event.fields[17].value == "6");
  CHECK(event.fields[18].key == "approximate_memory_bytes");
  CHECK(event.fields[18].value == "4096");
  CHECK(event.fields[19].key == "memory_growth_bytes");
  CHECK(event.fields[19].value == "128");
  CHECK(event.fields[20].key == "memory_increased");
  CHECK(event.fields[20].value == "true");
}

TEST_CASE("structured metrics sink rejects a missing logger")
{
  CHECK_THROWS_AS(uburu::diagnostics::StructuredMetricsSink(nullptr), std::invalid_argument);
}
