#include "core/diagnostics/structured-metrics-sink.hpp"

#include <chrono>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <utility>

namespace uburu::diagnostics
{
  namespace
  {

    [[nodiscard]] LogField numericField(std::string key, std::uint64_t value)
    {
      return LogField{.key = std::move(key), .value = std::to_string(value), .sensitive = false};
    }

    [[nodiscard]] LogField durationField(std::string key, std::chrono::nanoseconds value)
    {
      return numericField(std::move(key), static_cast<std::uint64_t>(value.count()));
    }

  } // namespace

  StructuredMetricsSink::StructuredMetricsSink(std::shared_ptr<StructuredLogger> logger)
    : logger(std::move(logger))
  {
    if (!this->logger)
      throw std::invalid_argument("StructuredMetricsSink requires a structured logger");
  }

  void StructuredMetricsSink::record(const SearchMetrics& metrics)
  {
    logger->write(
      LogEvent{.level = LogLevel::info,
               .category = LogCategory::search,
               .message = "search metrics",
               .fields = {durationField("time_to_first_result_ns", metrics.timeToFirstResult),
                          durationField("total_time_ns", metrics.totalTime),
                          numericField("files_processed", metrics.filesProcessed),
                          numericField("bytes_processed", metrics.bytesProcessed),
                          numericField("results_emitted", metrics.resultsEmitted),
                          numericField("ignored_files", metrics.ignoredFiles),
                          numericField("hidden_files", metrics.hiddenFiles),
                          numericField("binary_files", metrics.binaryFiles),
                          numericField("binary_files_skipped", metrics.binaryFilesSkipped)},
               .timestamp = {}}
    );
  }

} // namespace uburu::diagnostics
