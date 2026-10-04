// SPDX-License-Identifier: AGPL-3.0-only

#pragma once

#include <chrono>
#include <cstdint>
#include <memory>
#include <opencv2/core/mat.hpp>
#include <opencv2/core/types.hpp>
#include <vector>

namespace yolo {

struct Detection {
    int label{};
    float score{};
    cv::Rect2f box;
};

struct Frame {
    std::uint64_t id{};
    cv::Mat image;
    std::chrono::steady_clock::time_point enqueued_at;
};

// Producers must not modify pixel storage or its aliases after publishing.
using FramePtr = std::shared_ptr<const Frame>;

struct InferenceResult {
    FramePtr frame;  // Keeps the original image alive for rendering.
    std::vector<Detection> detections;
    double inference_ms{};
};

}  // namespace yolo
