#pragma once

#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <mutex>
#include <optional>
#include <stop_token>
#include <utility>

namespace uburu::concurrency
{

  struct BoundedQueueMetrics
  {
    std::uint64_t producerWaits{0};
    std::uint64_t consumerWaits{0};
  };

  template <typename T>
  class BoundedQueue
  {
  public:
    explicit BoundedQueue(std::size_t capacity) : capacityValue(capacity == 0 ? 1 : capacity) {}

    BoundedQueue(const BoundedQueue&) = delete;
    BoundedQueue& operator=(const BoundedQueue&) = delete;

    [[nodiscard]] bool push(T value, std::stop_token stop_token = {})
    {
      std::unique_lock lock(mutex);

      if (!closedValue && queue.size() >= capacityValue)
        ++metricsValue.producerWaits;

      notFull.wait(lock, stop_token, [&] {
        return closedValue || queue.size() < capacityValue;
      });

      if (closedValue || stop_token.stop_requested())
        return false;

      queue.push_back(std::move(value));
      notEmpty.notify_one();

      return true;
    }

    [[nodiscard]] std::optional<T> pop(std::stop_token stop_token = {})
    {
      std::unique_lock lock(mutex);

      if (!closedValue && queue.empty())
        ++metricsValue.consumerWaits;

      notEmpty.wait(lock, stop_token, [&] {
        return closedValue || !queue.empty();
      });

      if (queue.empty())
        return std::nullopt;

      auto value = std::move(queue.front());
      queue.pop_front();
      notFull.notify_one();

      return value;
    }

    void close()
    {
      {
        std::lock_guard lock(mutex);
        closedValue = true;
      }

      notEmpty.notify_all();
      notFull.notify_all();
    }

    [[nodiscard]] std::size_t size() const
    {
      std::lock_guard lock(mutex);

      return queue.size();
    }

    [[nodiscard]] bool closed() const
    {
      std::lock_guard lock(mutex);

      return closedValue;
    }

    [[nodiscard]] BoundedQueueMetrics metrics() const
    {
      std::lock_guard lock(mutex);

      return metricsValue;
    }

  private:
    std::size_t capacityValue{1};
    mutable std::mutex mutex;
    std::condition_variable_any notEmpty;
    std::condition_variable_any notFull;
    std::deque<T> queue;
    BoundedQueueMetrics metricsValue;
    bool closedValue{false};
  };

} // namespace uburu::concurrency
