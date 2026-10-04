// SPDX-License-Identifier: AGPL-3.0-only

#pragma once

#include <condition_variable>
#include <cstddef>
#include <mutex>
#include <queue>
#include <stdexcept>
#include <utility>

namespace yolo {

enum class QueueFullPolicy { Block, DropOldest };
enum class StopMode { Drain, CancelPending };
enum class PushResult { Accepted, ReplacedOldest, Closed };

template <typename T>
class BlockingQueue {
public:
    explicit BlockingQueue(std::size_t capacity, QueueFullPolicy policy = QueueFullPolicy::Block)
        : capacity_(capacity), policy_(policy) {
        if (capacity == 0) {
            throw std::invalid_argument("queue capacity must be greater than zero");
        }
    }

    BlockingQueue(const BlockingQueue&) = delete;
    BlockingQueue& operator=(const BlockingQueue&) = delete;

    bool push(T value) {
        return submit(std::move(value)) != PushResult::Closed;
    }

    PushResult submit(T value) {
        std::unique_lock<std::mutex> lock(mutex_);
        if (policy_ == QueueFullPolicy::Block) {
            not_full_.wait(lock, [this] { return closed_ || queue_.size() < capacity_; });
        }
        if (closed_) {
            return PushResult::Closed;
        }
        const bool replaced = queue_.size() == capacity_;
        if (replaced) queue_.pop();
        queue_.push(std::move(value));
        lock.unlock();
        not_empty_.notify_one();
        return replaced ? PushResult::ReplacedOldest : PushResult::Accepted;
    }

    bool pop(T& value) {
        std::unique_lock<std::mutex> lock(mutex_);
        not_empty_.wait(lock, [this] { return closed_ || !queue_.empty(); });
        if (queue_.empty()) {
            return false;
        }
        value = std::move(queue_.front());
        queue_.pop();
        lock.unlock();
        not_full_.notify_one();
        return true;
    }

    // CancelPending may escalate a previous Drain; closing never reopens the queue.
    void close(StopMode mode = StopMode::Drain) {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            closed_ = true;
            if (mode == StopMode::CancelPending) {
                std::queue<T> empty;
                queue_.swap(empty);
            }
        }
        not_empty_.notify_all();
        not_full_.notify_all();
    }

private:
    const std::size_t capacity_;
    const QueueFullPolicy policy_;
    std::queue<T> queue_;
    bool closed_{false};
    std::mutex mutex_;
    std::condition_variable not_empty_;
    std::condition_variable not_full_;
};

}  // namespace yolo
