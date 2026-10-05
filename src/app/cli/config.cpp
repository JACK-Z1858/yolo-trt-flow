// SPDX-License-Identifier: AGPL-3.0-only

#include "app/cli/config.hpp"
#include "common/log.hpp"

#include <opencv2/core/persistence.hpp>
#include <stdexcept>

namespace app::cli {
using common::LogLevel;
using common::log;
namespace {

template <typename T>
void read_if_present(const cv::FileNode& parent, const char* key, T& value) {
    const auto node = parent[key];
    if (!node.empty()) {
        node >> value;
    }
}

}  // namespace

Config load_config(const std::string& path) {
    cv::FileStorage file(path, cv::FileStorage::READ);
    if (!file.isOpened()) {
        throw std::runtime_error("cannot open config file: " + path);
    }

    Config config;
    const auto model = file["model"];
    const auto input = file["input"];
    const auto pipeline = file["pipeline"];
    const auto output = file["output"];
    const auto benchmark = file["benchmark"];

    read_if_present(model, "engine", config.engine_path);
    read_if_present(model, "device", config.device);
    read_if_present(input, "source", config.source);

    std::string mode = "offline";
    read_if_present(pipeline, "mode", mode);
    if (mode == "offline") config.mode = InputMode::Offline;
    else if (mode == "realtime") config.mode = InputMode::Realtime;
    else throw std::runtime_error("pipeline.mode must be offline or realtime");

    // Legacy configurations are accepted, but execution is now single-worker.
    if (!pipeline["workers"].empty()) {
        int workers = 1;
        read_if_present(pipeline, "workers", workers);
        if (workers != 1) log(LogLevel::Warning, "config", "pipeline.workers is ignored; one worker is used");
    }
    int queue_depth = static_cast<int>(config.queue_depth);
    if (config.mode == InputMode::Offline) {
        read_if_present(pipeline, "queue_depth", queue_depth);
        if (queue_depth <= 0) throw std::runtime_error("queue_depth must be greater than zero");
        config.queue_depth = static_cast<std::size_t>(queue_depth);
    } else {
        config.queue_depth = 1;
    }

    read_if_present(output, "show", config.show);
    read_if_present(output, "video", config.output_video);
    read_if_present(output, "fps", config.output_fps);

    double warmup_frames = static_cast<double>(config.warmup_frames);
    double max_frames = static_cast<double>(config.max_frames);
    read_if_present(benchmark, "warmup_frames", warmup_frames);
    read_if_present(benchmark, "max_frames", max_frames);
    if (warmup_frames < 0 || max_frames < 0) {
        throw std::runtime_error("frame counts cannot be negative");
    }
    config.warmup_frames = static_cast<std::uint64_t>(warmup_frames);
    config.max_frames = static_cast<std::uint64_t>(max_frames);

    if (config.engine_path.empty() || config.source.empty()) {
        throw std::runtime_error("model.engine and input.source are required");
    }
    if (config.output_fps <= 0) {
        throw std::runtime_error("output.fps must be greater than zero");
    }
    return config;
}

}  // namespace app::cli
