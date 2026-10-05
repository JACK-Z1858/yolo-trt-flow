// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include "app/cli/config.hpp"
#include "common/frame_bus.hpp"
#include <atomic>

namespace app::cli {
// Runs synchronously on the caller's reader thread; owns no inference state.
void read_source(const Config& config, const common::FrameBus& bus, const std::atomic<bool>& stopping);
}  // namespace app::cli
