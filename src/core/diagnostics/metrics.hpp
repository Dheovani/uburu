#pragma once

#include <chrono>
#include <cstdint>

namespace uburu::diagnostics
{

  struct SearchMetrics
  {
    std::chrono::nanoseconds timeToFirstResult{};
    std::chrono::nanoseconds totalTime{};
    std::uint64_t filesProcessed{0};
    std::uint64_t bytesProcessed{0};
    std::uint64_t resultsEmitted{0};
    std::uint64_t ignoredFiles{0};
    std::uint64_t hiddenFiles{0};
    std::uint64_t binaryFiles{0};
    std::uint64_t binaryFilesSkipped{0};
  };

  class MetricsSink
  {
  public:
    virtual ~MetricsSink() = default;
    virtual void record(const SearchMetrics& metrics) = 0;
  };

} // namespace uburu::diagnostics
