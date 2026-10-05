// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include "yolo/types.hpp"

namespace app::cli {
// Always draws on a copy; shared input pixels remain unchanged.
cv::Mat render(const yolo::InferenceResult& result);
}  // namespace app::cli
