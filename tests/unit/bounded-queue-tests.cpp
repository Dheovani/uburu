#include "core/concurrency/bounded-queue.hpp"

#include <catch2/catch_test_macros.hpp>

#include <atomic>
#include <chrono>
#include <optional>
#include <thread>

namespace
{

  constexpr auto async_poll_interval = std::chrono::milliseconds{1};
  constexpr auto async_timeout = std::chrono::milliseconds{200};

  template <typename Predicate>
  bool eventually(Predicate predicate)
  {
    const auto deadline = std::chrono::steady_clock::now() + async_timeout;

    while (std::chrono::steady_clock::now() < deadline) {
      if (predicate())
        return true;

      std::this_thread::sleep_for(async_poll_interval);
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

  std::jthread producer([&] {
    pushed = queue.push(2);
  });

  std::this_thread::sleep_for(std::chrono::milliseconds{10});
  CHECK_FALSE(pushed.load());
  CHECK(queue.pop() == std::optional<int>{1});

  producer.join();

  CHECK(pushed.load());
  CHECK(queue.pop() == std::optional<int>{2});
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

  std::atomic_bool producer_result{false};

  std::jthread producer([&] {
    producer_result = queue.push(2);
  });

  REQUIRE(eventually([&] {
    return queue.metrics().producer_waits == 1;
  }));

  CHECK(queue.metrics().producer_waits == 1);
  CHECK(queue.pop() == std::optional<int>{1});

  producer.join();
  CHECK(producer_result.load());
  CHECK(queue.pop() == std::optional<int>{2});

  std::atomic_bool consumer_result{false};

  std::jthread consumer([&] {
    consumer_result = queue.pop() == std::optional<int>{3};
  });

  REQUIRE(eventually([&] {
    return queue.metrics().consumer_waits == 1;
  }));

  CHECK(queue.metrics().consumer_waits == 1);
  REQUIRE(queue.push(3));

  consumer.join();
  CHECK(consumer_result.load());
}
