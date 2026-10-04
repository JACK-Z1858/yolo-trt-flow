// SPDX-License-Identifier: AGPL-3.0-only
#include "yolo/config.hpp"
#include "yolo/frame_bus.hpp"
#include "yolo/renderer.hpp"
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
        const auto config = yolo::load_config(path.string());
        require(config.mode == yolo::InputMode::Realtime && config.queue_depth == 1,
                "realtime must force capacity 1");
        {
            cv::FileStorage file(path.string(), cv::FileStorage::WRITE);
            file << "model" << "{" << "engine" << "fake.engine" << "}";
            file << "input" << "{" << "source" << "fake.mp4" << "}";
            file << "pipeline" << "{" << "mode" << "offline" << "queue_depth" << 7 << "}";
        }
        require(yolo::load_config(path.string()).queue_depth == 7, "offline reads capacity");
        {
            cv::FileStorage file(path.string(), cv::FileStorage::WRITE);
            file << "model" << "{" << "engine" << "fake.engine" << "}";
            file << "input" << "{" << "source" << "fake.mp4" << "}";
        }
        require(yolo::load_config(path.string()).mode == yolo::InputMode::Offline,
                "legacy config defaults to offline");
        {
            cv::FileStorage file(path.string(), cv::FileStorage::WRITE);
            file << "model" << "{" << "engine" << "fake.engine" << "}";
            file << "input" << "{" << "source" << "fake.mp4" << "}";
            file << "pipeline" << "{" << "mode" << "invalid" << "}";
        }
        bool rejected = false;
        try { yolo::load_config(path.string()); }
        catch (const std::runtime_error&) { rejected = true; }
        require(rejected, "unknown mode must be rejected");

        auto frame = std::make_shared<yolo::Frame>();
        frame->id = 42;
        frame->image = cv::Mat::zeros(64, 64, CV_8UC3);
        yolo::FramePtr first, second;
        yolo::FrameBus bus;
        bus.subscribe([&](yolo::FramePtr value) { first = std::move(value); return false; });
        bus.subscribe([&](yolo::FramePtr value) { second = std::move(value); return true; });
        require(bus.publish(frame), "one closed subscriber must not skip others");
        require(first.get() == second.get() && first->image.data == second->image.data,
                "fan-out shares frame and pixel storage");
        yolo::FrameBus closed_bus;
        closed_bus.subscribe([](yolo::FramePtr) { return false; });
        require(!closed_bus.publish(frame), "all closed subscribers reject publish");

        yolo::InferenceResult result;
        result.frame = frame;
        result.detections.push_back({0, 0.9f, cv::Rect2f(10, 10, 20, 20)});
        const auto before = frame->image.clone();
        const auto rendered = yolo::render(result);
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
