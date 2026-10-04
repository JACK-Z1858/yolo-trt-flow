// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include "yolo/types.hpp"

namespace yolo {
// Always draws on a copy; shared input pixels remain unchanged.
cv::Mat render(const InferenceResult& result);
}  // namespace yolo
