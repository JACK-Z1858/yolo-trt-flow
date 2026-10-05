// SPDX-License-Identifier: AGPL-3.0-only
#include "yolo/pipeline.hpp"
#include "common/log.hpp"
#include <stdexcept>
#include <utility>

namespace yolo {
using common::FramePtr;
using common::LogLevel;
using common::PushResult;
using common::StopMode;
using common::log;
InferencePipeline::InferencePipeline(PipelineConfig config, ResultCallback on_result,
                                     ErrorCallback on_error, FinishedCallback on_finished)
    : config_(std::move(config)), on_result_(std::move(on_result)),
      on_error_(std::move(on_error)), on_finished_(std::move(on_finished)),
      input_(config_.queue_depth, config_.queue_full_policy) {
    if (!on_result_) throw std::invalid_argument("result callback is required");
    if (cudaSetDevice(config_.device) != cudaSuccess) {
        throw std::runtime_error("cannot select CUDA device " + std::to_string(config_.device));
    }
    log(LogLevel::Info, "pipeline", "loading engine: " + config_.engine_path);
    engine_ = std::make_unique<TrtEngine>(config_.engine_path);
}
InferencePipeline::~InferencePipeline() { stop(StopMode::CancelPending); }

void InferencePipeline::start() {
    if (started_ || stop_requested_) throw std::logic_error("pipeline instances cannot be restarted after start or stop");
    started_ = true;
    accepting_ = true;
    try { worker_ = std::thread(&InferencePipeline::run, this); }
    catch (...) { request_stop(); throw; }
}
bool InferencePipeline::submit(FramePtr frame) {
    if (!frame || frame->image.empty() || frame->image.type() != CV_8UC3) {
        throw std::invalid_argument("expected a nonempty BGR CV_8UC3 frame");
    }
    if (!accepting_) return false;
    const auto status = input_.submit(std::move(frame));
    if (status == PushResult::Closed) return false;
    ++submitted_;
    if (status == PushResult::ReplacedOldest) ++dropped_;
    return true;
}
void InferencePipeline::request_stop(StopMode mode) {
    stop_requested_ = true;
    accepting_ = false;
    input_.close(mode);
}
void InferencePipeline::stop(StopMode mode) {
    if (worker_.joinable() && worker_.get_id() == std::this_thread::get_id()) {
        throw std::logic_error("stop() cannot join its own callback thread; use request_stop()");
    }
    request_stop(mode);
    if (worker_.joinable()) worker_.join();
}
PipelineStats InferencePipeline::stats() const {
    return {submitted_.load(), processed_.load(), dropped_.load()};
}
void InferencePipeline::report_error(std::exception_ptr error) noexcept {
    request_stop(StopMode::CancelPending);
    try { std::rethrow_exception(error); }
    catch (const std::exception& cause) { log(LogLevel::Error, "pipeline", cause.what()); }
    catch (...) { log(LogLevel::Error, "pipeline", "unknown background error"); }
    if (on_error_) {
        try { on_error_(error); }
        catch (...) { log(LogLevel::Error, "pipeline", "error callback threw"); }
    }
}
void InferencePipeline::run() noexcept {
    log(LogLevel::Info, "pipeline", "worker started");
    try {
        if (cudaSetDevice(config_.device) != cudaSuccess) {
            throw std::runtime_error("worker cannot select CUDA device");
        }
        TrtWorker worker(*engine_);
        FramePtr frame;
        while (input_.pop(frame)) {
            auto result = worker.process(std::move(frame));
            ++processed_;
            on_result_(std::move(result));
        }
    } catch (...) { report_error(std::current_exception()); }
    accepting_ = false;
    if (on_finished_) {
        try { on_finished_(); }
        catch (...) { report_error(std::current_exception()); }
    }
    log(LogLevel::Info, "pipeline", "worker stopped");
}
}  // namespace yolo
