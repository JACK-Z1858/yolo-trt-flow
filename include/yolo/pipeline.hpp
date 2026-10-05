// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include "common/blocking_queue.hpp"
#include "yolo/trt_engine.hpp"
#include <atomic>
#include <exception>
#include <functional>
#include <thread>

namespace yolo {
struct PipelineConfig {
    std::string engine_path;
    int device{0};
    std::size_t queue_depth{4};
    common::QueueFullPolicy queue_full_policy{common::QueueFullPolicy::Block};
};
struct PipelineStats {
    std::uint64_t submitted{};
    std::uint64_t processed{};
    std::uint64_t dropped{};  // Replaced pending frames, excluding stop cancellation.
};

class InferencePipeline {
public:
    using ResultCallback = std::function<void(InferenceResult)>;
    using ErrorCallback = std::function<void(std::exception_ptr)>;
    using FinishedCallback = std::function<void()>;
    // Loads engine synchronously. Callbacks run on the single worker thread.
    // Return promptly; never call stop() from callbacks. request_stop() is safe.
    InferencePipeline(PipelineConfig config, ResultCallback on_result,
                      ErrorCallback on_error = {}, FinishedCallback on_finished = {});
    ~InferencePipeline();
    InferencePipeline(const InferencePipeline&) = delete;
    InferencePipeline& operator=(const InferencePipeline&) = delete;

    // Single-use instance. start()/stop()/destruction belong to the owning thread.
    void start();
    bool submit(common::FramePtr frame);
    // Thread-safe, non-joining request. Cancel may escalate a previous Drain.
    void request_stop(common::StopMode mode = common::StopMode::CancelPending);
    // Waits for worker and callbacks to finish. Safe to repeat from the owner.
    void stop(common::StopMode mode = common::StopMode::CancelPending);
    PipelineStats stats() const;
private:
    void run() noexcept;
    void report_error(std::exception_ptr error) noexcept;
    PipelineConfig config_;
    ResultCallback on_result_;
    ErrorCallback on_error_;
    FinishedCallback on_finished_;
    common::BlockingQueue<common::FramePtr> input_;
    std::unique_ptr<TrtEngine> engine_;
    std::thread worker_;
    bool started_{false};
    std::atomic<bool> stop_requested_{false};
    std::atomic<bool> accepting_{false};
    std::atomic<std::uint64_t> submitted_{0}, processed_{0}, dropped_{0};
};
}  // namespace yolo
