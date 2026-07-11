#pragma once

#include <chrono>
#include <cstdint>

namespace uburu::diagnostics
{

  /**
   * Runtime counters for search, indexing reuse, queues, and memory growth.
   */
  struct SearchMetrics
  {
    std::chrono::nanoseconds timeToFirstResult{};
    std::chrono::nanoseconds totalTime{};
    std::uint64_t filesProcessed{0};
    std::uint64_t bytesProcessed{0};
    std::uint64_t filesPerSecond{0};
    std::uint64_t bytesPerSecond{0};
    std::uint64_t resultsEmitted{0};
    std::uint64_t ignoredFiles{0};
    std::uint64_t hiddenFiles{0};
    std::uint64_t binaryFiles{0};
    std::uint64_t binaryFilesSkipped{0};
    std::uint64_t queueProducerWaits{0};
    std::uint64_t queueConsumerWaits{0};
    std::uint64_t cacheHits{0};
    std::uint64_t cacheMisses{0};
    std::uint64_t reusedByCatalog{0};
    std::uint64_t reusedByBlob{0};
    std::uint64_t reusedByHash{0};
    std::uint64_t approximateMemoryBytes{0};
    std::uint64_t memoryGrowthBytes{0};
    bool memoryIncreased{false};
  };

  /**
   * Receives metric snapshots without binding producers to a concrete logging backend.
   */
  class MetricsSink
  {
  public:
    virtual ~MetricsSink() = default;
    virtual void record(const SearchMetrics& metrics) = 0;
  };

} // namespace uburu::diagnostics
