// SPDX-License-Identifier: AGPL-3.0-only
#include "app/cli/config.hpp"
#include "common/frame_bus.hpp"
#include "app/cli/renderer.hpp"
#include <opencv2/core.hpp>
#include <opencv2/core/persistence.hpp>
#include <filesystem>
#include <iostream>
#include <stdexcept>

void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

int main() {
    const auto path = std::filesystem::current_path() / "frontend_test_config.yaml";
    try {
        {
            cv::FileStorage file(path.string(), cv::FileStorage::WRITE);
            file << "model" << "{" << "engine" << "fake.engine" << "}";
            file << "input" << "{" << "source" << "fake.mp4" << "}";
            file << "pipeline" << "{" << "mode" << "realtime" << "queue_depth" << 99 << "}";
        }
        const auto config = app::cli::load_config(path.string());
        require(config.mode == app::cli::InputMode::Realtime && config.queue_depth == 1,
                "realtime must force capacity 1");
        {
            cv::FileStorage file(path.string(), cv::FileStorage::WRITE);
            file << "model" << "{" << "engine" << "fake.engine" << "}";
            file << "input" << "{" << "source" << "fake.mp4" << "}";
            file << "pipeline" << "{" << "mode" << "offline" << "queue_depth" << 7 << "}";
        }
        require(app::cli::load_config(path.string()).queue_depth == 7, "offline reads capacity");
        {
            cv::FileStorage file(path.string(), cv::FileStorage::WRITE);
            file << "model" << "{" << "engine" << "fake.engine" << "}";
            file << "input" << "{" << "source" << "fake.mp4" << "}";
        }
        require(app::cli::load_config(path.string()).mode == app::cli::InputMode::Offline,
                "legacy config defaults to offline");
        {
            cv::FileStorage file(path.string(), cv::FileStorage::WRITE);
            file << "model" << "{" << "engine" << "fake.engine" << "}";
            file << "input" << "{" << "source" << "fake.mp4" << "}";
            file << "pipeline" << "{" << "mode" << "invalid" << "}";
        }
        bool rejected = false;
        try { app::cli::load_config(path.string()); }
        catch (const std::runtime_error&) { rejected = true; }
        require(rejected, "unknown mode must be rejected");

        auto frame = std::make_shared<common::Frame>();
        frame->id = 42;
        frame->image = cv::Mat::zeros(64, 64, CV_8UC3);
        common::FramePtr first, second;
        common::FrameBus bus;
        bus.subscribe([&](common::FramePtr value) { first = std::move(value); return false; });
        bus.subscribe([&](common::FramePtr value) { second = std::move(value); return true; });
        require(bus.publish(frame), "one closed subscriber must not skip others");
        require(first.get() == second.get() && first->image.data == second->image.data,
                "fan-out shares frame and pixel storage");
        common::FrameBus closed_bus;
        closed_bus.subscribe([](common::FramePtr) { return false; });
        require(!closed_bus.publish(frame), "all closed subscribers reject publish");

        yolo::InferenceResult result;
        result.frame = frame;
        result.detections.push_back({0, 0.9f, cv::Rect2f(10, 10, 20, 20)});
        const auto before = frame->image.clone();
        const auto rendered = app::cli::render(result);
        require(cv::norm(before, frame->image, cv::NORM_INF) == 0, "render must preserve shared pixels");
        require(cv::norm(before, rendered, cv::NORM_INF) > 0, "rendered copy must contain annotations");
        std::filesystem::remove(path);
        std::cout << "frontend tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::filesystem::remove(path);
        std::cerr << error.what() << '\n';
        return 1;
    }
}
