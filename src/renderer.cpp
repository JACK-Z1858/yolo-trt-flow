// SPDX-License-Identifier: AGPL-3.0-only
#include "yolo/renderer.hpp"
#include <opencv2/imgproc.hpp>
#include <string>
#include <vector>

namespace yolo {
namespace {
const std::vector<std::string> kCocoNames = {
    "person", "bicycle", "car", "motorcycle", "airplane", "bus", "train", "truck", "boat", "traffic light",
    "fire hydrant", "stop sign", "parking meter", "bench", "bird", "cat", "dog", "horse", "sheep", "cow",
    "elephant", "bear", "zebra", "giraffe", "backpack", "umbrella", "handbag", "tie", "suitcase", "frisbee",
    "skis", "snowboard", "sports ball", "kite", "baseball bat", "baseball glove", "skateboard", "surfboard",
    "tennis racket", "bottle", "wine glass", "cup", "fork", "knife", "spoon", "bowl", "banana", "apple",
    "sandwich", "orange", "broccoli", "carrot", "hot dog", "pizza", "donut", "cake", "chair", "couch",
    "potted plant", "bed", "dining table", "toilet", "tv", "laptop", "mouse", "remote", "keyboard",
    "cell phone", "microwave", "oven", "toaster", "sink", "refrigerator", "book", "clock", "vase", "scissors",
    "teddy bear", "hair drier", "toothbrush"
};
}
cv::Mat render(const InferenceResult& result) {
    auto image = result.frame->image.clone();
    for (const auto& detection : result.detections) {
        const auto color = cv::Scalar((37 * detection.label + 80) % 255,
                                     (17 * detection.label + 160) % 255,
                                     (29 * detection.label + 40) % 255);
        cv::rectangle(image, detection.box, color, 2);
        const auto name = detection.label >= 0 && static_cast<std::size_t>(detection.label) < kCocoNames.size()
            ? kCocoNames[static_cast<std::size_t>(detection.label)] : "class_" + std::to_string(detection.label);
        cv::putText(image, name + " " + cv::format("%.1f%%", detection.score * 100.0f),
                    detection.box.tl(), cv::FONT_HERSHEY_SIMPLEX, 0.5, color, 1);
    }
    return image;
}
}  // namespace yolo
