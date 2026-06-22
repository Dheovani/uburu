#pragma once

#include <chrono>
#include <cstdint>

namespace uburu::diagnostics
{

  struct SearchMetrics
  {
    std::chrono::nanoseconds time_to_first_result{};
    std::chrono::nanoseconds total_time{};
    std::uint64_t files_processed{0};
    std::uint64_t bytes_processed{0};
    std::uint64_t results_emitted{0};
    std::uint64_t binary_files_skipped{0};
  };

  class MetricsSink
  {
  public:
    virtual ~MetricsSink() = default;
    virtual void record(const SearchMetrics& metrics) = 0;
  };

} // namespace uburu::diagnostics
