#include "app/services/adaptive-result-batcher.hpp"

#include <catch2/catch_test_macros.hpp>

#include <chrono>

TEST_CASE("adaptive result batcher clamps invalid limits to safe values")
{
  uburu::app::SearchExecutionOptions options;

  options.resultBatchSize = 0;
  options.minimumResultBatchSize = 0;
  options.maximumResultBatchSize = 0;

  const uburu::app::AdaptiveResultBatcher batcher(options);

  CHECK(batcher.currentBatchSize() == 1);
}

TEST_CASE("adaptive result batcher grows when delivery is consistently cheap")
{
  uburu::app::SearchExecutionOptions options;

  options.resultBatchSize = 4;
  options.minimumResultBatchSize = 2;
  options.maximumResultBatchSize = 16;
  options.targetBatchDeliveryLatency = std::chrono::milliseconds(10);

  uburu::app::AdaptiveResultBatcher batcher(options);

  batcher.recordDeliveryLatency(std::chrono::milliseconds(1));
  CHECK(batcher.currentBatchSize() == 8);

  batcher.recordDeliveryLatency(std::chrono::milliseconds(1));
  CHECK(batcher.currentBatchSize() == 16);

  batcher.recordDeliveryLatency(std::chrono::milliseconds(1));
  CHECK(batcher.currentBatchSize() == 16);
}

TEST_CASE("adaptive result batcher shrinks when delivery is expensive")
{
  uburu::app::SearchExecutionOptions options;

  options.resultBatchSize = 16;
  options.minimumResultBatchSize = 2;
  options.maximumResultBatchSize = 32;
  options.targetBatchDeliveryLatency = std::chrono::milliseconds(10);

  uburu::app::AdaptiveResultBatcher batcher(options);

  batcher.recordDeliveryLatency(std::chrono::milliseconds(25));
  CHECK(batcher.currentBatchSize() == 8);

  batcher.recordDeliveryLatency(std::chrono::milliseconds(25));
  CHECK(batcher.currentBatchSize() == 4);

  batcher.recordDeliveryLatency(std::chrono::milliseconds(25));
  batcher.recordDeliveryLatency(std::chrono::milliseconds(25));
  CHECK(batcher.currentBatchSize() == 2);
}

TEST_CASE("adaptive result batcher preserves fixed batches when adaptation is disabled")
{
  uburu::app::SearchExecutionOptions options;

  options.resultBatchSize = 4;
  options.maximumResultBatchSize = 16;
  options.targetBatchDeliveryLatency = std::chrono::milliseconds(10);
  options.adaptiveBatching = false;

  uburu::app::AdaptiveResultBatcher batcher(options);

  batcher.recordDeliveryLatency(std::chrono::milliseconds(1));
  batcher.recordDeliveryLatency(std::chrono::milliseconds(50));

  CHECK(batcher.currentBatchSize() == 4);
}
