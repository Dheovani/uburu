#include "benchmark-output.hpp"

#include <chrono>

namespace uburu::benchmarks
{
  namespace
  {

    constexpr double counterOne = 1.0;

    [[nodiscard]] double asDouble(std::uint64_t value)
    {
      return static_cast<double>(value);
    }

    [[nodiscard]] double nanoseconds(std::chrono::nanoseconds value)
    {
      return static_cast<double>(value.count());
    }

  } // namespace

  void
  publishSearchCounters(benchmark::State& state, const BenchmarkDataset& dataset, const SearchBenchmarkResult& result)
  {
    const auto& summary = result.summary;
    const auto& metrics = summary.metrics;

    state.counters["dataset_files"] = asDouble(dataset.fileCount);
    state.counters["dataset_bytes"] = asDouble(dataset.byteCount);
    state.counters["expected_matches"] = asDouble(dataset.expectedMatches);
    state.counters["expected_ignored_files"] = asDouble(dataset.expectedIgnoredFiles);
    state.counters["expected_hidden_files"] = asDouble(dataset.expectedHiddenFiles);
    state.counters["expected_binary_files"] = asDouble(dataset.expectedBinaryFiles);
    state.counters["matches"] = asDouble(static_cast<std::uint64_t>(summary.matches));
    state.counters["files_scanned"] = asDouble(static_cast<std::uint64_t>(summary.filesScanned));
    state.counters["files_processed"] = asDouble(metrics.filesProcessed);
    state.counters["bytes_processed"] = asDouble(metrics.bytesProcessed);
    state.counters["files_per_second"] = asDouble(metrics.filesPerSecond);
    state.counters["bytes_per_second"] = asDouble(metrics.bytesPerSecond);
    state.counters["time_to_first_result_ns"] = nanoseconds(metrics.timeToFirstResult);
    state.counters["total_time_ns"] = nanoseconds(metrics.totalTime);
    state.counters["ignored_files"] = asDouble(metrics.ignoredFiles);
    state.counters["hidden_files"] = asDouble(metrics.hiddenFiles);
    state.counters["binary_files"] = asDouble(metrics.binaryFiles);
    state.counters["binary_files_skipped"] = asDouble(metrics.binaryFilesSkipped);
    state.counters["approximate_memory_bytes"] = asDouble(metrics.approximateMemoryBytes);
    state.counters["result_batches"] = asDouble(result.resultBatches);
    state.counters["consumed_results"] = asDouble(result.consumedResults);
    state.counters["consumed_bytes"] =
      benchmark::Counter(asDouble(result.consumedBytes), benchmark::Counter::kAvgIterations);

    if (summary.partialFailure)
      state.counters["partial_failure"] = counterOne;

    if (summary.cancelled)
      state.counters["cancelled"] = counterOne;
  }

} // namespace uburu::benchmarks
