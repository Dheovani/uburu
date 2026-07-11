#pragma once

#include "core/diagnostics/metrics.hpp"
#include "core/diagnostics/structured-logger.hpp"

#include <memory>

namespace uburu::diagnostics
{

  /**
   * Converts metric snapshots into structured log events.
   */
  class StructuredMetricsSink final : public MetricsSink
  {
  public:
    explicit StructuredMetricsSink(std::shared_ptr<StructuredLogger> logger);

    void record(const SearchMetrics& metrics) override;

  private:
    std::shared_ptr<StructuredLogger> logger;
  };

} // namespace uburu::diagnostics
