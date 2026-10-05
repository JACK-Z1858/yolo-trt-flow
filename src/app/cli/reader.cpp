// SPDX-License-Identifier: AGPL-3.0-only
#include "app/cli/reader.hpp"
#include "common/log.hpp"
#include <opencv2/imgcodecs.hpp>
#include <opencv2/videoio.hpp>
#include <algorithm>
#include <cctype>
#include <filesystem>
#include <stdexcept>
#include <vector>

namespace app::cli {
using common::Frame;
using common::FrameBus;
using common::LogLevel;
using common::log;
namespace {
bool is_image(const std::filesystem::path& path) {
    auto extension = path.extension().string();
    std::transform(extension.begin(), extension.end(), extension.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    return extension == ".jpg" || extension == ".jpeg" || extension == ".png" || extension == ".bmp";
}
bool is_camera(const std::string& source) {
    return !source.empty() && std::all_of(source.begin(), source.end(),
                                        [](unsigned char c) { return std::isdigit(c); });
}
}

void read_source(const Config& config, const FrameBus& bus, const std::atomic<bool>& stopping) {
    namespace fs = std::filesystem;
    log(LogLevel::Info, "reader", "opening source: " + config.source);
    std::uint64_t id = 0;
    const auto can_read = [&] { return !stopping && (!config.max_frames || id < config.max_frames); };
    const auto publish = [&](const cv::Mat& image) {
        if (stopping) return false;
        if (image.empty()) throw std::runtime_error("input contains an unreadable frame");
        auto frame = std::make_shared<Frame>();
        frame->id = id++;
        frame->image = image.clone();
        frame->enqueued_at = std::chrono::steady_clock::now();
        return bus.publish(frame);
    };
    const fs::path source(config.source);
    if (fs::is_directory(source)) {
        std::vector<fs::path> images;
        for (const auto& entry : fs::directory_iterator(source)) {
            if (entry.is_regular_file() && is_image(entry.path())) images.push_back(entry.path());
        }
        std::sort(images.begin(), images.end());
        for (const auto& path : images) {
            if (!can_read() || !publish(cv::imread(path.string()))) break;
        }
    } else if (fs::is_regular_file(source) && is_image(source)) {
        if (can_read()) publish(cv::imread(source.string()));
    } else {
        cv::VideoCapture capture;
        if (is_camera(config.source)) capture.open(std::stoi(config.source));
        else capture.open(config.source);
        if (!capture.isOpened()) throw std::runtime_error("cannot open input source: " + config.source);
        cv::Mat image;
        while (can_read() && capture.read(image)) {
            if (!publish(image)) break;
        }
    }
    log(LogLevel::Info, "reader", "source finished");
}
}  // namespace app::cli
