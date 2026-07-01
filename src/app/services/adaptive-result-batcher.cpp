#include "app/services/adaptive-result-batcher.hpp"

#include <algorithm>

namespace uburu::app
{
  namespace
  {

    constexpr std::size_t minimumAllowedBatchSize = 1;
    constexpr std::size_t batchGrowthFactor = 2;
    constexpr std::size_t batchShrinkDivisor = 2;
    constexpr std::size_t fastDeliveryLatencyDivisor = 2;
    constexpr std::size_t slowDeliveryLatencyMultiplier = 2;

    [[nodiscard]] std::size_t normalizedBatchSize(std::size_t batchSize)
    {
      return std::max(batchSize, minimumAllowedBatchSize);
    }

    [[nodiscard]] std::chrono::nanoseconds normalizedTargetLatency(std::chrono::milliseconds targetLatency)
    {
      if (targetLatency <= std::chrono::milliseconds::zero())
        return defaultTargetBatchDeliveryLatency;

      return targetLatency;
    }

  } // namespace

  AdaptiveResultBatcher::AdaptiveResultBatcher(const SearchExecutionOptions& options)
    : adaptive(options.adaptiveBatching), minimumBatchSize(normalizedBatchSize(options.minimumResultBatchSize)),
      maximumBatchSize(normalizedBatchSize(options.maximumResultBatchSize)),
      batchSize(normalizedBatchSize(options.resultBatchSize)),
      targetDeliveryLatency(normalizedTargetLatency(options.targetBatchDeliveryLatency))
  {
    if (!adaptive) {
      minimumBatchSize = batchSize;
      maximumBatchSize = batchSize;

      return;
    }

    if (maximumBatchSize < minimumBatchSize)
      maximumBatchSize = minimumBatchSize;

    batchSize = std::clamp(batchSize, minimumBatchSize, maximumBatchSize);
  }

  std::size_t AdaptiveResultBatcher::currentBatchSize() const
  {
    return batchSize;
  }

  void AdaptiveResultBatcher::recordDeliveryLatency(std::chrono::nanoseconds elapsed)
  {
    if (!adaptive)
      return;

    if (elapsed <= targetDeliveryLatency / fastDeliveryLatencyDivisor) {
      batchSize = std::min(batchSize * batchGrowthFactor, maximumBatchSize);

      return;
    }

    if (elapsed >= targetDeliveryLatency * slowDeliveryLatencyMultiplier)
      batchSize = std::max(batchSize / batchShrinkDivisor, minimumBatchSize);
  }

} // namespace uburu::app
