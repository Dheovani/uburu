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

    [[nodiscard]] LogField booleanField(std::string key, bool value)
    {
      return LogField{.key = std::move(key), .value = value ? "true" : "false", .sensitive = false};
    }

    [[nodiscard]] LogField durationField(std::string key, std::chrono::nanoseconds value)
    {
      return numericField(std::move(key), static_cast<std::uint64_t>(value.count()));
    }

  } // namespace

  StructuredMetricsSink::StructuredMetricsSink(std::shared_ptr<StructuredLogger> logger) : logger(std::move(logger))
  {
    if (!this->logger)
      throw std::invalid_argument("StructuredMetricsSink requires a structured logger");
  }

  void StructuredMetricsSink::record(const SearchMetrics& metrics)
  {
    logger->write(LogEvent{.level = LogLevel::info,
                           .category = LogCategory::search,
                           .message = "search metrics",
                           .fields = {durationField("time_to_first_result_ns", metrics.timeToFirstResult),
                                      durationField("total_time_ns", metrics.totalTime),
                                      numericField("files_processed", metrics.filesProcessed),
                                      numericField("bytes_processed", metrics.bytesProcessed),
                                      numericField("files_per_second", metrics.filesPerSecond),
                                      numericField("bytes_per_second", metrics.bytesPerSecond),
                                      numericField("results_emitted", metrics.resultsEmitted),
                                      numericField("ignored_files", metrics.ignoredFiles),
                                      numericField("hidden_files", metrics.hiddenFiles),
                                      numericField("binary_files", metrics.binaryFiles),
                                      numericField("binary_files_skipped", metrics.binaryFilesSkipped),
                                      numericField("queue_producer_waits", metrics.queueProducerWaits),
                                      numericField("queue_consumer_waits", metrics.queueConsumerWaits),
                                      numericField("cache_hits", metrics.cacheHits),
                                      numericField("cache_misses", metrics.cacheMisses),
                                      numericField("reused_by_catalog", metrics.reusedByCatalog),
                                      numericField("reused_by_blob", metrics.reusedByBlob),
                                      numericField("reused_by_hash", metrics.reusedByHash),
                                      numericField("approximate_memory_bytes", metrics.approximateMemoryBytes),
                                      numericField("memory_growth_bytes", metrics.memoryGrowthBytes),
                                      booleanField("memory_increased", metrics.memoryIncreased)},
                           .timestamp = {}});
  }

} // namespace uburu::diagnostics
