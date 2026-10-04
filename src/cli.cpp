// SPDX-License-Identifier: AGPL-3.0-only
#include "yolo/cli.hpp"
#include "yolo/log.hpp"
#include "yolo/pipeline.hpp"
#include "yolo/reader.hpp"
#include "yolo/renderer.hpp"
#include <opencv2/highgui.hpp>
#include <opencv2/videoio.hpp>
#include <atomic>
#include <chrono>
#include <exception>
#include <iostream>
#include <mutex>
#include <stdexcept>
#include <thread>

namespace yolo {
void run_cli(const Config& config) {
    const bool realtime = config.mode == InputMode::Realtime;
    const auto policy = realtime ? QueueFullPolicy::DropOldest : QueueFullPolicy::Block;
    // This output queue belongs to the CLI, not the inference library.
    BlockingQueue<InferenceResult> results(realtime ? 1 : config.queue_depth, policy);
    std::atomic<bool> stopping{false};
    std::mutex error_mutex;
    std::exception_ptr background_error;
    const auto record_error = [&](std::exception_ptr error) {
        std::lock_guard<std::mutex> lock(error_mutex);
        if (!background_error) background_error = error;
        stopping = true;
    };

    InferencePipeline pipeline({config.engine_path, config.device,
                                realtime ? 1 : config.queue_depth, policy},
        [&](InferenceResult result) {
            // Offline output intentionally applies backpressure; realtime never
            // waits for space. A failing consumer closes this queue to unblock us.
            results.push(std::move(result));
        },
        record_error,
        [&] { results.close(); });

    FrameBus bus;
    bus.subscribe([&](FramePtr frame) { return pipeline.submit(std::move(frame)); });
    pipeline.start();
    std::thread reader;
    std::uint64_t consumed = 0, measured = 0;
    double inference_sum = 0.0, latency_sum = 0.0;
    std::chrono::steady_clock::time_point measurement_start;
    cv::VideoWriter writer;

    try {
        reader = std::thread([&] {
            try {
                read_source(config, bus, stopping);
                pipeline.request_stop(stopping ? StopMode::CancelPending : StopMode::Drain);
            } catch (...) {
                record_error(std::current_exception());
                pipeline.request_stop(StopMode::CancelPending);
            }
        });

        InferenceResult result;
        while (results.pop(result)) {
            const auto& frame = *result.frame;
            const auto now = std::chrono::steady_clock::now();
            const double latency = std::chrono::duration<double, std::milli>(now - frame.enqueued_at).count();
            ++consumed;
            if (frame.id >= config.warmup_frames) {
                if (!measured) measurement_start = frame.enqueued_at;
                ++measured;
                inference_sum += result.inference_ms;
                latency_sum += latency;
            }
            if (config.show || !config.output_video.empty()) {
                auto image = render(result);
                if (!config.output_video.empty()) {
                    if (!writer.isOpened()) {
                        writer.open(config.output_video, cv::VideoWriter::fourcc('m', 'p', '4', 'v'),
                                    config.output_fps, image.size());
                        if (!writer.isOpened()) throw std::runtime_error("cannot open output video: " + config.output_video);
                    }
                    writer.write(image);
                }
                if (config.show) {
                    cv::imshow("YoloTRTFlow", image);
                    if (cv::waitKey(1) == 'q' && !stopping.exchange(true)) {
                        log(LogLevel::Info, "cli", "user requested stop");
                        pipeline.request_stop(StopMode::CancelPending);
                    }
                }
            }
        }
    } catch (...) {
        record_error(std::current_exception());
        // Release a worker blocked in the offline result callback before joining.
        results.close(StopMode::CancelPending);
        pipeline.request_stop(StopMode::CancelPending);
    }

    pipeline.stop(stopping ? StopMode::CancelPending : StopMode::Drain);
    if (reader.joinable()) reader.join();
    writer.release();
    if (config.show) cv::destroyAllWindows();
    if (background_error) std::rethrow_exception(background_error);

    const auto stats = pipeline.stats();
    std::cout << "Frames: " << consumed << '\n'
              << "Submitted: " << stats.submitted << ", processed: " << stats.processed
              << ", input dropped: " << stats.dropped << '\n';
    if (measured) {
        const double elapsed_ms = std::chrono::duration<double, std::milli>(
            std::chrono::steady_clock::now() - measurement_start).count();
        std::cout << "Average TensorRT + D2H time: " << inference_sum / measured << " ms\n"
                  << "Average end-to-end latency: " << latency_sum / measured << " ms\n"
                  << "Pipeline throughput: " << measured * 1000.0 / elapsed_ms << " FPS\n";
    } else {
        std::cout << "No frames remained after benchmark warmup.\n";
    }
    log(LogLevel::Info, "cli", "shutdown complete");
}
}  // namespace yolo
