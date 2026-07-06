#include "benchmark-output.hpp"

#include <chrono>
#include <string_view>

namespace uburu::benchmarks
{
  namespace
  {

    constexpr double counterOne = 1.0;
    constexpr std::string_view regexJitExecutionMode = "jit";

    [[nodiscard]] double asDouble(std::uint64_t value)
    {
      return static_cast<double>(value);
    }

    [[nodiscard]] double nanoseconds(std::chrono::nanoseconds value)
    {
      return static_cast<double>(value.count());
    }

    [[nodiscard]] double ratio(std::chrono::nanoseconds numerator, std::chrono::nanoseconds denominator)
    {
      if (denominator.count() == 0)
        return 0.0;

      return static_cast<double>(numerator.count()) / static_cast<double>(denominator.count());
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
    state.counters["result_memory_bytes"] = asDouble(metrics.approximateMemoryBytes);
    state.counters["result_batches"] = asDouble(result.resultBatches);
    state.counters["consumed_results"] = asDouble(result.consumedResults);
    state.counters["consumed_bytes"] =
      benchmark::Counter(asDouble(result.consumedBytes), benchmark::Counter::kAvgIterations);
    state.counters["result_queue_peak_items"] = asDouble(result.peakBatchResults);
    state.counters["result_queue_peak_bytes"] = asDouble(result.peakBatchBytes);

    if (summary.partialFailure)
      state.counters["partial_failure"] = counterOne;

    if (summary.cancelled)
      state.counters["cancelled"] = counterOne;

    if (summary.regexExecutionMode == regexJitExecutionMode)
      state.counters["regex_jit_enabled"] = counterOne;
  }

  void publishRepeatedSearchCounters(benchmark::State& state,
                                     const BenchmarkDataset& dataset,
                                     const SearchBenchmarkResult& firstPass,
                                     const SearchBenchmarkResult& secondPass)
  {
    const auto& firstSummary = firstPass.summary;
    const auto& firstMetrics = firstSummary.metrics;
    const auto& secondSummary = secondPass.summary;
    const auto& secondMetrics = secondSummary.metrics;

    state.counters["dataset_files"] = asDouble(dataset.fileCount);
    state.counters["dataset_bytes"] = asDouble(dataset.byteCount);
    state.counters["expected_matches"] = asDouble(dataset.expectedMatches);
    state.counters["first_pass_matches"] = asDouble(static_cast<std::uint64_t>(firstSummary.matches));
    state.counters["second_pass_matches"] = asDouble(static_cast<std::uint64_t>(secondSummary.matches));
    state.counters["first_pass_total_time_ns"] = nanoseconds(firstMetrics.totalTime);
    state.counters["second_pass_total_time_ns"] = nanoseconds(secondMetrics.totalTime);
    state.counters["first_pass_time_to_first_result_ns"] = nanoseconds(firstMetrics.timeToFirstResult);
    state.counters["second_pass_time_to_first_result_ns"] = nanoseconds(secondMetrics.timeToFirstResult);
    state.counters["first_pass_bytes_per_second"] = asDouble(firstMetrics.bytesPerSecond);
    state.counters["second_pass_bytes_per_second"] = asDouble(secondMetrics.bytesPerSecond);
    state.counters["second_pass_speedup"] = ratio(firstMetrics.totalTime, secondMetrics.totalTime);
    state.counters["first_pass_result_batches"] = asDouble(firstPass.resultBatches);
    state.counters["second_pass_result_batches"] = asDouble(secondPass.resultBatches);

    if (firstSummary.partialFailure || secondSummary.partialFailure)
      state.counters["partial_failure"] = counterOne;

    if (firstSummary.cancelled || secondSummary.cancelled)
      state.counters["cancelled"] = counterOne;
  }

  void
  publishIndexCounters(benchmark::State& state, const BenchmarkDataset& dataset, const IndexBenchmarkResult& result)
  {
    state.counters["dataset_files"] = asDouble(dataset.fileCount);
    state.counters["dataset_bytes"] = asDouble(dataset.byteCount);
    state.counters["indexed"] = asDouble(static_cast<std::uint64_t>(result.summary.indexed));
    state.counters["reused_by_catalog"] = asDouble(static_cast<std::uint64_t>(result.summary.reusedByCatalog));
    state.counters["reused_by_blob"] = asDouble(static_cast<std::uint64_t>(result.summary.reusedByBlob));
    state.counters["reused_by_hash"] = asDouble(static_cast<std::uint64_t>(result.summary.reusedByHash));
    state.counters["removed"] = asDouble(static_cast<std::uint64_t>(result.summary.removed));
    state.counters["failed"] = asDouble(static_cast<std::uint64_t>(result.summary.failed));
    state.counters["progress_events"] = asDouble(static_cast<std::uint64_t>(result.progressEvents));
    state.counters["index_update_time_ns"] = nanoseconds(result.elapsed);
    state.counters["index_catalog_memory_bytes"] = asDouble(result.approximateCatalogMemoryBytes);
    state.counters["index_database_bytes"] = asDouble(result.databaseBytes);

    if (result.summary.cancelled)
      state.counters["cancelled"] = counterOne;

    if (result.stalenessChecked)
      state.counters["staleness_checked"] = counterOne;

    if (result.stale)
      state.counters["stale_generation"] = counterOne;

    if (result.headChanged)
      state.counters["head_changed"] = counterOne;

    if (result.branchChanged)
      state.counters["branch_changed"] = counterOne;
  }

} // namespace uburu::benchmarks
