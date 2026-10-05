// SPDX-License-Identifier: AGPL-3.0-only
#pragma once

#include <chrono>
#include <cstdint>
#include <memory>
#include <opencv2/core/mat.hpp>

namespace common {
struct Frame {
    std::uint64_t id{};
    cv::Mat image;
    std::chrono::steady_clock::time_point enqueued_at;
};

// Producers must not modify pixel storage or its aliases after publishing.
using FramePtr = std::shared_ptr<const Frame>;
}  // namespace common
