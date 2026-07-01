#pragma once

#include "core/concurrency/bounded-queue.hpp"

#include <cstddef>
#include <functional>
#include <stop_token>
#include <thread>
#include <vector>

namespace uburu::concurrency
{

  class WorkerPool
  {
  public:
    using Task = std::function<void(std::stop_token)>;

    explicit WorkerPool(std::size_t workerCount, std::size_t queueCapacity = 0)
      : queue(normalizedQueueCapacity(workerCount, queueCapacity))
    {
      workerCount = normalizedWorkerCount(workerCount);

      workers.reserve(workerCount);

      for (std::size_t index = 0; index < workerCount; ++index) {
        workers.emplace_back([this](std::stop_token stop_token) { run(stop_token); });
      }
    }

    WorkerPool(const WorkerPool&) = delete;
    WorkerPool& operator=(const WorkerPool&) = delete;

    ~WorkerPool()
    {
      requestStop();
    }

    [[nodiscard]] bool submit(Task task, std::stop_token stop_token = {})
    {
      return queue.push(std::move(task), stop_token);
    }

    void close()
    {
      queue.close();
      joinWorkers();
    }

    void requestStop()
    {
      for (auto& worker : workers) {
        worker.request_stop();
      }

      queue.close();
      joinWorkers();
    }

    [[nodiscard]] std::size_t workerCount() const
    {
      return workers.size();
    }

  private:
    static constexpr std::size_t defaultQueueCapacityMultiplier = 2;

    [[nodiscard]] static std::size_t normalizedWorkerCount(std::size_t workerCount)
    {
      if (workerCount == 0)
        return 1;

      return workerCount;
    }

    [[nodiscard]]
    static std::size_t normalizedQueueCapacity(std::size_t workerCount, std::size_t requestedCapacity)
    {
      if (requestedCapacity != 0)
        return requestedCapacity;

      return normalizedWorkerCount(workerCount) * defaultQueueCapacityMultiplier;
    }

    void run(std::stop_token stop_token)
    {
      while (!stop_token.stop_requested()) {
        auto task = queue.pop(stop_token);

        if (!task)
          return;

        (*task)(stop_token);
      }
    }

    void joinWorkers()
    {
      for (auto& worker : workers) {
        if (worker.joinable())
          worker.join();
      }
    }

    BoundedQueue<Task> queue;
    std::vector<std::jthread> workers;
  };

} // namespace uburu::concurrency
