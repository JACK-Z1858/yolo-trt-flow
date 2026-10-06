// SPDX-License-Identifier: AGPL-3.0-only
#include "app/cli/config.hpp"
#include "app/cli/reader.hpp"
#include "common/frame_bus.hpp"
#include "app/cli/renderer.hpp"
#include <opencv2/core.hpp>
#include <opencv2/core/persistence.hpp>
#include <opencv2/videoio.hpp>
#include <atomic>
#include <chrono>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <vector>

void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

int main() {
    const auto path = std::filesystem::current_path() / "frontend_test_config.yaml";
    const auto video_path = std::filesystem::current_path() / "frontend_test_video.avi";
    try {
        {
            cv::FileStorage file(path.string(), cv::FileStorage::WRITE);
            file << "model" << "{" << "engine" << "fake.engine" << "}";
            file << "input" << "{" << "source" << "fake.mp4" << "fake_camera" << 1 << "}";
            file << "pipeline" << "{" << "mode" << "offline" << "queue_depth" << 99 << "}";
        }
        const auto config = app::cli::load_config(path.string());
        require(config.fake_camera, "input.fake_camera must be read");
        require(config.mode == app::cli::InputMode::Realtime && config.queue_depth == 1,
                "fake camera must override offline and force capacity 1");
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
        require(!app::cli::load_config(path.string()).fake_camera,
                "legacy config defaults to unpaced input");
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

        // No inference/GPU is needed: a subscriber records the reader's delivery times.
        cv::VideoWriter writer(video_path.string(), cv::VideoWriter::fourcc('M', 'J', 'P', 'G'),
                               10.0, cv::Size(64, 64));
        if (writer.isOpened()) {
            for (int i = 0; i < 3; ++i) writer.write(cv::Mat::zeros(64, 64, CV_8UC3));
            writer.release();
            app::cli::Config playback;
            playback.source = video_path.string();
            playback.fake_camera = true;
            std::atomic<bool> stopping{false};
            std::vector<std::chrono::steady_clock::time_point> arrivals;
            common::FrameBus playback_bus;
            playback_bus.subscribe([&](common::FramePtr value) {
                require(value->id == arrivals.size(), "paced reader preserves frame order");
                arrivals.push_back(std::chrono::steady_clock::now());
                return true;
            });
            app::cli::read_source(playback, playback_bus, stopping);
            require(arrivals.size() == 3, "paced reader drains video at EOF");
            for (std::size_t i = 1; i < arrivals.size(); ++i) {
                require(arrivals[i] - arrivals[i - 1] >= std::chrono::milliseconds(80),
                        "10 FPS video must not be published at full decoding speed");
            }
            common::FrameBus stop_bus;
            int delivered = 0;
            stop_bus.subscribe([&](common::FramePtr) {
                ++delivered;
                stopping = true;
                return true;
            });
            app::cli::read_source(playback, stop_bus, stopping);
            require(delivered == 1, "paced reader respects stop");
        } else {
            std::cout << "SKIP video pacing integration: MJPG encoder unavailable\n";
        }

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
        std::filesystem::remove(video_path);
        std::cout << "frontend tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::filesystem::remove(path);
        std::filesystem::remove(video_path);
        std::cerr << error.what() << '\n';
        return 1;
    }
}
