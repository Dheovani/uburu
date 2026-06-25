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

    explicit WorkerPool(std::size_t worker_count, std::size_t queue_capacity = 0)
      : queue_(normalized_queue_capacity(worker_count, queue_capacity))
    {
      worker_count = normalized_worker_count(worker_count);

      workers_.reserve(worker_count);

      for (std::size_t index = 0; index < worker_count; ++index) {
        workers_.emplace_back([this](std::stop_token stop_token) {
          run(stop_token);
        });
      }
    }

    WorkerPool(const WorkerPool&) = delete;
    WorkerPool& operator=(const WorkerPool&) = delete;

    ~WorkerPool()
    {
      request_stop();
    }

    [[nodiscard]] bool submit(Task task, std::stop_token stop_token = {})
    {
      return queue_.push(std::move(task), stop_token);
    }

    void close()
    {
      queue_.close();
      join_workers();
    }

    void request_stop()
    {
      for (auto& worker : workers_) {
        worker.request_stop();
      }

      queue_.close();
      join_workers();
    }

    [[nodiscard]] std::size_t worker_count() const
    {
      return workers_.size();
    }

  private:
    static constexpr std::size_t default_queue_capacity_multiplier = 2;

    [[nodiscard]] static std::size_t normalized_worker_count(std::size_t worker_count)
    {
      if (worker_count == 0)
        return 1;

      return worker_count;
    }

    [[nodiscard]]
    static std::size_t normalized_queue_capacity(std::size_t worker_count, std::size_t requested_capacity)
    {
      if (requested_capacity != 0)
        return requested_capacity;

      return normalized_worker_count(worker_count) * default_queue_capacity_multiplier;
    }

    void run(std::stop_token stop_token)
    {
      while (!stop_token.stop_requested()) {
        auto task = queue_.pop(stop_token);

        if (!task)
          return;

        (*task)(stop_token);
      }
    }

    void join_workers()
    {
      for (auto& worker : workers_) {
        if (worker.joinable())
          worker.join();
      }
    }

    BoundedQueue<Task> queue_;
    std::vector<std::jthread> workers_;
  };

} // namespace uburu::concurrency
