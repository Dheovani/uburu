#pragma once

#include "app/dto/search-dto.hpp"
#include "benchmark-dataset.hpp"

#include <chrono>
#include <cstddef>
#include <cstdint>

namespace uburu::benchmarks
{

  /**
   * Controls benchmark-side batching and optional simulated UI rendering work.
   */
  struct SearchBenchmarkRunOptions
  {
    app::SearchExecutionOptions executionOptions{.adaptiveBatching = false};
    std::uint32_t simulatedUiRenderPasses{0};
  };

  /**
   * Captures service-level search counters relevant to perceived UI throughput.
   */
  struct SearchBenchmarkResult
  {
    app::SearchSummaryDto summary;
    std::uint64_t resultBatches{0};
    std::uint64_t consumedResults{0};
    std::uint64_t consumedBytes{0};
    std::uint64_t peakBatchResults{0};
    std::uint64_t peakBatchBytes{0};
    std::uint64_t simulatedUiRenderBytes{0};
    std::uint64_t simulatedUiRenderChecksum{0};
  };

  [[nodiscard]]
  SearchBenchmarkResult runDefaultSearchServiceBenchmark(const BenchmarkDataset& dataset);

  [[nodiscard]]
  SearchBenchmarkResult runDefaultSearchServiceBenchmark(
    const BenchmarkDataset& dataset,
    SearchBenchmarkRunOptions options);

} // namespace uburu::benchmarks
