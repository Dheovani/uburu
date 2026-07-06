#pragma once

#include "app/dto/search-dto.hpp"
#include "benchmark-dataset.hpp"

#include <cstdint>

namespace uburu::benchmarks
{

  struct SearchBenchmarkResult
  {
    app::SearchSummaryDto summary;
    std::uint64_t resultBatches{0};
    std::uint64_t consumedResults{0};
    std::uint64_t consumedBytes{0};
    std::uint64_t peakBatchResults{0};
    std::uint64_t peakBatchBytes{0};
  };

  [[nodiscard]] SearchBenchmarkResult runDefaultSearchServiceBenchmark(const BenchmarkDataset& dataset);

} // namespace uburu::benchmarks
