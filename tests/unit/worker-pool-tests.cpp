#include "core/concurrency/worker-pool.hpp"

#include <catch2/catch_test_macros.hpp>

#include <atomic>
#include <chrono>
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

TEST_CASE("worker pool executes submitted tasks")
{
  std::atomic<int> completed{0};

  {
    uburu::concurrency::WorkerPool pool(2);

    REQUIRE(pool.submit([&](std::stop_token) {
      completed.fetch_add(1);
    }));
    REQUIRE(pool.submit([&](std::stop_token) {
      completed.fetch_add(1);
    }));

    pool.close();
  }

  CHECK(completed.load() == 2);
}

TEST_CASE("worker pool normalizes zero workers to one worker")
{
  uburu::concurrency::WorkerPool pool(0, 1);

  CHECK(pool.worker_count() == 1);
}

TEST_CASE("worker pool rejects submissions after close")
{
  uburu::concurrency::WorkerPool pool(1, 1);

  pool.close();

  CHECK_FALSE(pool.submit([](std::stop_token) {}));
}

TEST_CASE("worker pool forwards stop tokens to tasks")
{
  std::atomic<bool> task_started{false};
  std::atomic<bool> saw_stop{false};

  {
    uburu::concurrency::WorkerPool pool(1, 1);

    REQUIRE(pool.submit([&](std::stop_token stop_token) {
      task_started = true;

      while (!stop_token.stop_requested())
        std::this_thread::sleep_for(std::chrono::milliseconds{1});

      saw_stop = true;
    }));

    REQUIRE(eventually([&] {
      return task_started.load();
    }));

    pool.request_stop();
  }

  CHECK(saw_stop.load());
}
