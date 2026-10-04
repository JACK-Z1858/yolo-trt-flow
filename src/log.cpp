// SPDX-License-Identifier: AGPL-3.0-only
#include "yolo/log.hpp"
#include <chrono>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <thread>

namespace yolo {
void log(LogLevel level, std::string_view module, std::string_view message) noexcept {
    try {
        static std::mutex mutex;
        std::lock_guard<std::mutex> lock(mutex);
        const auto now = std::chrono::system_clock::now();
        const auto time = std::chrono::system_clock::to_time_t(now);
        std::tm local{};
#ifdef _WIN32
        localtime_s(&local, &time);
#else
        localtime_r(&time, &local);
#endif
        const char* name = level == LogLevel::Info ? "INFO" :
                           level == LogLevel::Warning ? "WARN" : "ERROR";
        std::cerr << std::put_time(&local, "%Y-%m-%d %H:%M:%S") << " [" << name
                  << "] [" << module << "] [thread " << std::this_thread::get_id()
                  << "] " << message << '\n';
    } catch (...) {
        // Never throw into TensorRT's noexcept logger or shutdown.
    }
}
}  // namespace yolo
