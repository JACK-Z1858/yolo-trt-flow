// SPDX-License-Identifier: AGPL-3.0-only

#pragma once

#include "common/frame.hpp"
#include <opencv2/core/types.hpp>
#include <vector>

namespace yolo {

struct Detection {
    int label{};
    float score{};
    cv::Rect2f box;
};

struct InferenceResult {
    common::FramePtr frame;  // Keeps the original image alive for rendering.
    std::vector<Detection> detections;
    double inference_ms{};
};

}  // namespace yolo
