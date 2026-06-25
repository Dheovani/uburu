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
    std::uint64_t producer_waits{0};
    std::uint64_t consumer_waits{0};
  };

  template <typename T>
  class BoundedQueue
  {
  public:
    explicit BoundedQueue(std::size_t capacity) : capacity_(capacity == 0 ? 1 : capacity) {}

    BoundedQueue(const BoundedQueue&) = delete;
    BoundedQueue& operator=(const BoundedQueue&) = delete;

    [[nodiscard]] bool push(T value, std::stop_token stop_token = {})
    {
      std::unique_lock lock(mutex_);

      if (!closed_ && queue_.size() >= capacity_)
        ++metrics_.producer_waits;

      not_full_.wait(lock, stop_token, [&] {
        return closed_ || queue_.size() < capacity_;
      });

      if (closed_ || stop_token.stop_requested())
        return false;

      queue_.push_back(std::move(value));
      not_empty_.notify_one();

      return true;
    }

    [[nodiscard]] std::optional<T> pop(std::stop_token stop_token = {})
    {
      std::unique_lock lock(mutex_);

      if (!closed_ && queue_.empty())
        ++metrics_.consumer_waits;

      not_empty_.wait(lock, stop_token, [&] {
        return closed_ || !queue_.empty();
      });

      if (queue_.empty())
        return std::nullopt;

      auto value = std::move(queue_.front());
      queue_.pop_front();
      not_full_.notify_one();

      return value;
    }

    void close()
    {
      {
        std::lock_guard lock(mutex_);
        closed_ = true;
      }

      not_empty_.notify_all();
      not_full_.notify_all();
    }

    [[nodiscard]] std::size_t size() const
    {
      std::lock_guard lock(mutex_);

      return queue_.size();
    }

    [[nodiscard]] bool closed() const
    {
      std::lock_guard lock(mutex_);

      return closed_;
    }

    [[nodiscard]] BoundedQueueMetrics metrics() const
    {
      std::lock_guard lock(mutex_);

      return metrics_;
    }

  private:
    std::size_t capacity_{1};
    mutable std::mutex mutex_;
    std::condition_variable_any not_empty_;
    std::condition_variable_any not_full_;
    std::deque<T> queue_;
    BoundedQueueMetrics metrics_;
    bool closed_{false};
  };

} // namespace uburu::concurrency
