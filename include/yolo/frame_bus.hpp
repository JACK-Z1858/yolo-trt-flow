// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include "yolo/types.hpp"
#include <functional>
#include <stdexcept>
#include <utility>
#include <vector>

namespace yolo {
class FrameBus {
public:
    using Subscriber = std::function<bool(FramePtr)>;
    // Configure before publishing; no concurrent subscription changes.
    void subscribe(Subscriber subscriber) {
        if (!subscriber) throw std::invalid_argument("empty frame subscriber");
        subscribers_.push_back(std::move(subscriber));
    }
    // Synchronous fan-out of references. Blocking subscribers slow the producer.
    // Returns true if at least one subscriber accepted the frame.
    bool publish(const FramePtr& frame) const {
        if (!frame) throw std::invalid_argument("null frame");
        bool accepted = false;
        for (const auto& subscriber : subscribers_) {
            if (subscriber(frame)) accepted = true;
        }
        return accepted;
    }
private:
    std::vector<Subscriber> subscribers_;
};
}  // namespace yolo
