// SPDX-License-Identifier: AGPL-3.0-only

#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

namespace yolo {

enum class InputMode { Offline, Realtime };

struct Config {
    std::string engine_path;
    std::string source;
    int device{0};
    InputMode mode{InputMode::Offline};
    std::size_t queue_depth{4};
    bool show{true};
    std::string output_video;
    double output_fps{30.0};
    std::uint64_t warmup_frames{0};
    std::uint64_t max_frames{0};
};

Config load_config(const std::string& path);

}  // namespace yolo
