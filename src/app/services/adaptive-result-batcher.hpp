#pragma once

#include "app/dto/search-dto.hpp"

#include <chrono>
#include <cstddef>

namespace uburu::app
{

  class AdaptiveResultBatcher final
  {
  public:
    explicit AdaptiveResultBatcher(const SearchExecutionOptions& options);

    [[nodiscard]] std::size_t currentBatchSize() const;
    void recordDeliveryLatency(std::chrono::nanoseconds elapsed);

  private:
    bool adaptive{true};
    std::size_t minimumBatchSize{defaultMinimumResultBatchSize};
    std::size_t maximumBatchSize{defaultMaximumResultBatchSize};
    std::size_t batchSize{defaultResultBatchSize};
    std::chrono::nanoseconds targetDeliveryLatency{defaultTargetBatchDeliveryLatency};
  };

} // namespace uburu::app
