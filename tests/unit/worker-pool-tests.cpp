#include "core/concurrency/worker-pool.hpp"

#include <catch2/catch_test_macros.hpp>

#include <atomic>
#include <chrono>
#include <thread>

namespace
{

  constexpr auto asyncPollInterval = std::chrono::milliseconds{1};
  constexpr auto asyncTimeout = std::chrono::milliseconds{200};
  constexpr int repeatedConcurrencyRuns = 16;
  constexpr int repeatedTaskCount = 32;

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

TEST_CASE("worker pool executes submitted tasks")
{
  std::atomic<int> completed{0};

  {
    uburu::concurrency::WorkerPool pool(2);

    REQUIRE(pool.submit([&](std::stop_token) { completed.fetch_add(1); }));
    REQUIRE(pool.submit([&](std::stop_token) { completed.fetch_add(1); }));

    pool.close();
  }

  CHECK(completed.load() == 2);
}

TEST_CASE("worker pool normalizes zero workers to one worker")
{
  uburu::concurrency::WorkerPool pool(0, 1);

  CHECK(pool.workerCount() == 1);
}

TEST_CASE("worker pool rejects submissions after close")
{
  uburu::concurrency::WorkerPool pool(1, 1);

  pool.close();

  CHECK_FALSE(pool.submit([](std::stop_token) {}));
}

TEST_CASE("worker pool remains stable across repeated concurrent runs")
{
  for (int run = 0; run < repeatedConcurrencyRuns; ++run) {
    std::atomic<int> completed{0};

    {
      uburu::concurrency::WorkerPool pool(4, 4);

      for (int task = 0; task < repeatedTaskCount; ++task)
        REQUIRE(pool.submit([&](std::stop_token) { completed.fetch_add(1); }));

      pool.close();
    }

    CHECK(completed.load() == repeatedTaskCount);
  }
}

TEST_CASE("worker pool forwards stop tokens to tasks")
{
  std::atomic<bool> taskStarted{false};
  std::atomic<bool> sawStop{false};

  {
    uburu::concurrency::WorkerPool pool(1, 1);

    REQUIRE(pool.submit([&](std::stop_token stop_token) {
      taskStarted = true;

      while (!stop_token.stop_requested())
        std::this_thread::sleep_for(std::chrono::milliseconds{1});

      sawStop = true;
    }));

    REQUIRE(eventually([&] { return taskStarted.load(); }));

    pool.requestStop();
  }

  CHECK(sawStop.load());
}
