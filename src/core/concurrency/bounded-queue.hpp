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

  /**
   * Provides a cancellation-aware queue with fixed capacity and explicit close semantics.
   */
  template <typename T> class BoundedQueue
  {
  public:
    explicit BoundedQueue(std::size_t capacity) : capacityValue(capacity == 0 ? 1 : capacity) {}

    BoundedQueue(const BoundedQueue&) = delete;
    BoundedQueue& operator=(const BoundedQueue&) = delete;

    /**
     * Pushes one item, waiting for capacity unless the queue is closed or cancellation is requested.
     */
    [[nodiscard]]
    bool push(T value, std::stop_token stopToken = {})
    {
      std::unique_lock lock(mutex);

      if (!closedValue && queue.size() >= capacityValue)
        ++metricsValue.producerWaits;

      notFull.wait(lock, stopToken, [&] { return closedValue || queue.size() < capacityValue; });

      if (closedValue || stopToken.stop_requested())
        return false;

      queue.push_back(std::move(value));
      notEmpty.notify_one();

      return true;
    }

    /**
     * Pops one item or returns empty when the queue is closed, empty, or cancellation is requested.
     */
    [[nodiscard]]
    std::optional<T> pop(std::stop_token stopToken = {})
    {
      std::unique_lock lock(mutex);

      if (!closedValue && queue.empty())
        ++metricsValue.consumerWaits;

      notEmpty.wait(lock, stopToken, [&] { return closedValue || !queue.empty(); });

      if (queue.empty())
        return std::nullopt;

      auto value = std::move(queue.front());
      queue.pop_front();
      notFull.notify_one();

      return value;
    }

    /**
     * Closes the queue and wakes all waiting producers and consumers.
     */
    void close()
    {
      {
        std::lock_guard lock(mutex);
        closedValue = true;
      }

      notEmpty.notify_all();
      notFull.notify_all();
    }

    [[nodiscard]]
    std::size_t size() const
    {
      std::lock_guard lock(mutex);

      return queue.size();
    }

    [[nodiscard]]
    bool closed() const
    {
      std::lock_guard lock(mutex);

      return closedValue;
    }

    [[nodiscard]]
    BoundedQueueMetrics metrics() const
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
