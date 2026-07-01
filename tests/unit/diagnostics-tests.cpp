#include "core/diagnostics/structured-logger.hpp"
#include "core/diagnostics/structured-metrics-sink.hpp"

#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <cstdint>
#include <memory>
#include <stdexcept>

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
    uburu::diagnostics::StructuredLogOptions{.includeSensitiveFields = true});

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

TEST_CASE("structured metrics sink records search metrics as structured log fields")
{
  using namespace std::chrono_literals;

  constexpr auto timeToFirstResult = 7ns;
  constexpr auto totalTime = 11ns;
  constexpr std::uint64_t filesProcessed = 3;
  constexpr std::uint64_t bytesProcessed = 128;
  constexpr std::uint64_t resultsEmitted = 2;
  constexpr std::uint64_t ignoredFiles = 5;
  constexpr std::uint64_t hiddenFiles = 1;
  constexpr std::uint64_t binaryFiles = 4;
  constexpr std::uint64_t binaryFilesSkipped = 4;

  auto logger = std::make_shared<uburu::diagnostics::InMemoryStructuredLogger>();
  uburu::diagnostics::StructuredMetricsSink sink(logger);

  sink.record(uburu::diagnostics::SearchMetrics{.timeToFirstResult = timeToFirstResult,
                                                .totalTime = totalTime,
                                                .filesProcessed = filesProcessed,
                                                .bytesProcessed = bytesProcessed,
                                                .resultsEmitted = resultsEmitted,
                                                .ignoredFiles = ignoredFiles,
                                                .hiddenFiles = hiddenFiles,
                                                .binaryFiles = binaryFiles,
                                                .binaryFilesSkipped = binaryFilesSkipped});

  REQUIRE(logger->entries().size() == 1);
  const auto& event = logger->entries().front();

  CHECK(event.level == uburu::diagnostics::LogLevel::info);
  CHECK(event.category == uburu::diagnostics::LogCategory::search);
  CHECK(event.message == "search metrics");
  REQUIRE(event.fields.size() == 9);
  CHECK(event.fields[0].key == "time_to_first_result_ns");
  CHECK(event.fields[0].value == "7");
  CHECK(event.fields[1].key == "total_time_ns");
  CHECK(event.fields[1].value == "11");
  CHECK(event.fields[4].key == "results_emitted");
  CHECK(event.fields[4].value == "2");
}

TEST_CASE("structured metrics sink rejects a missing logger")
{
  CHECK_THROWS_AS(uburu::diagnostics::StructuredMetricsSink(nullptr), std::invalid_argument);
}
