// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include "yolo/config.hpp"
#include "yolo/frame_bus.hpp"
#include <atomic>

namespace yolo {
// Runs synchronously on the caller's reader thread; owns no inference state.
void read_source(const Config& config, const FrameBus& bus, const std::atomic<bool>& stopping);
}  // namespace yolo
