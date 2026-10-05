// SPDX-License-Identifier: AGPL-3.0-only
#include "common/blocking_queue.hpp"
#include <chrono>
#include <future>
#include <iostream>
#include <memory>
#include <stdexcept>

using namespace std::chrono_literals;
using namespace common;

void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

int main() {
    try {
        bool rejected = false;
        try { BlockingQueue<int> invalid(0); }
        catch (const std::invalid_argument&) { rejected = true; }
        require(rejected, "zero capacity must fail");

        BlockingQueue<int> offline(1);
        require(offline.push(1), "initial push");
        std::promise<void> entered;
        auto producer = std::async(std::launch::async, [&] {
            entered.set_value();
            return offline.push(2);
        });
        entered.get_future().wait();
        require(producer.wait_for(50ms) == std::future_status::timeout, "full offline queue must block");
        int value = 0;
        require(offline.pop(value) && value == 1, "FIFO first item");
        require(producer.wait_for(1s) == std::future_status::ready && producer.get(), "pop must unblock producer");
        offline.close(StopMode::Drain);
        require(offline.pop(value) && value == 2, "drain preserves pending tasks");
        require(!offline.pop(value) && !offline.push(3), "drained queue rejects work");

        BlockingQueue<std::shared_ptr<int>> realtime(1, QueueFullPolicy::DropOldest);
        auto old = std::make_shared<int>(10);
        std::weak_ptr<int> lifetime = old;
        require(realtime.submit(old) == PushResult::Accepted, "accept first frame");
        old.reset();
        require(realtime.submit(std::make_shared<int>(11)) == PushResult::ReplacedOldest, "replace oldest frame");
        require(lifetime.expired(), "replacement releases old frame reference");
        std::shared_ptr<int> latest;
        require(realtime.pop(latest) && *latest == 11, "latest frame survives");

        BlockingQueue<int> cancelled(1);
        cancelled.push(1);
        auto blocked = std::async(std::launch::async, [&] { return cancelled.push(2); });
        cancelled.close(StopMode::CancelPending);
        require(blocked.wait_for(1s) == std::future_status::ready && !blocked.get(), "cancel wakes/rejects producer");
        require(!cancelled.pop(value), "cancel clears pending tasks");
        cancelled.close(StopMode::Drain);
        require(!cancelled.pop(value), "repeated stop must not restore cancelled tasks");

        BlockingQueue<int> empty(1);
        auto consumer = std::async(std::launch::async, [&] { int item; return empty.pop(item); });
        empty.close();
        require(consumer.wait_for(1s) == std::future_status::ready && !consumer.get(), "close wakes empty consumer");

        BlockingQueue<int> escalation(2);
        escalation.push(1);
        escalation.close(StopMode::Drain);
        escalation.close(StopMode::CancelPending);
        require(!escalation.pop(value), "cancel may escalate drain");
        std::cout << "queue tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
