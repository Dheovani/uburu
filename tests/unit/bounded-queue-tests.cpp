#include "core/concurrency/bounded-queue.hpp"

#include <catch2/catch_test_macros.hpp>

#include <atomic>
#include <chrono>
#include <optional>
#include <thread>

namespace
{

  constexpr auto asyncPollInterval = std::chrono::milliseconds{1};
  constexpr auto asyncTimeout = std::chrono::milliseconds{200};

  template <typename Predicate> bool eventually(Predicate predicate)
  {
    const auto deadline = std::chrono::steady_clock::now() + asyncTimeout;

    while (std::chrono::steady_clock::now() < deadline) {
      if (predicate())
        return true;

      std::this_thread::sleep_for(asyncPollInterval);
    }

    return predicate();
  }

} // namespace

TEST_CASE("bounded queue preserves FIFO order")
{
  uburu::concurrency::BoundedQueue<int> queue(2);

  REQUIRE(queue.push(1));
  REQUIRE(queue.push(2));

  CHECK(queue.pop() == std::optional<int>{1});
  CHECK(queue.pop() == std::optional<int>{2});
}

TEST_CASE("bounded queue wakes blocked producer when an item is consumed")
{
  uburu::concurrency::BoundedQueue<int> queue(1);
  REQUIRE(queue.push(1));

  std::atomic_bool pushed{false};

  std::jthread producer([&] { pushed = queue.push(2); });

  std::this_thread::sleep_for(std::chrono::milliseconds{10});
  CHECK_FALSE(pushed.load());
  CHECK(queue.pop() == std::optional<int>{1});

  producer.join();

  CHECK(pushed.load());
  CHECK(queue.pop() == std::optional<int>{2});
}

TEST_CASE("bounded queue applies backpressure without growing past capacity")
{
  uburu::concurrency::BoundedQueue<int> queue(2);
  REQUIRE(queue.push(1));
  REQUIRE(queue.push(2));

  std::atomic_bool producerCompleted{false};

  std::jthread producer([&] { producerCompleted = queue.push(3); });

  REQUIRE(eventually([&] { return queue.metrics().producerWaits == 1; }));

  CHECK(queue.size() == 2);
  CHECK_FALSE(producerCompleted.load());
  CHECK(queue.pop() == std::optional<int>{1});

  producer.join();
  CHECK(producerCompleted.load());
  CHECK(queue.size() == 2);
  CHECK(queue.pop() == std::optional<int>{2});
  CHECK(queue.pop() == std::optional<int>{3});
}

TEST_CASE("bounded queue returns empty when closed")
{
  uburu::concurrency::BoundedQueue<int> queue(1);

  queue.close();

  CHECK_FALSE(queue.push(1));
  CHECK(queue.pop() == std::nullopt);
}

TEST_CASE("bounded queue push observes cancellation")
{
  uburu::concurrency::BoundedQueue<int> queue(1);
  REQUIRE(queue.push(1));

  std::stop_source cancellation;
  cancellation.request_stop();

  CHECK_FALSE(queue.push(2, cancellation.get_token()));
}

TEST_CASE("bounded queue tracks producer and consumer waits")
{
  uburu::concurrency::BoundedQueue<int> queue(1);
  REQUIRE(queue.push(1));

  std::atomic_bool producerResult{false};

  std::jthread producer([&] { producerResult = queue.push(2); });

  REQUIRE(eventually([&] { return queue.metrics().producerWaits == 1; }));

  CHECK(queue.metrics().producerWaits == 1);
  CHECK(queue.pop() == std::optional<int>{1});

  producer.join();
  CHECK(producerResult.load());
  CHECK(queue.pop() == std::optional<int>{2});

  std::atomic_bool consumerResult{false};

  std::jthread consumer([&] { consumerResult = queue.pop() == std::optional<int>{3}; });

  REQUIRE(eventually([&] { return queue.metrics().consumerWaits == 1; }));

  CHECK(queue.metrics().consumerWaits == 1);
  REQUIRE(queue.push(3));

  consumer.join();
  CHECK(consumerResult.load());
}
