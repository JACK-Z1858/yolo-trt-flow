// Standalone C++17 example: no CUDA, TensorRT, or OpenCV required.

#include <chrono>
#include <functional>
#include <iostream>
#include <mutex>
#include <string>
#include <thread>

std::mutex log_mutex;

void log(const std::string& message) {
    std::lock_guard<std::mutex> lock(log_mutex);
    std::cout << "[thread " << std::this_thread::get_id() << "] "
              << message << std::endl;
}

using ResultCallback = std::function<void(int)>;

// Simulate processing one frame. This function creates no thread itself.
void process_frame(int frame_id, const ResultCallback& on_result) {
    log("processing frame " + std::to_string(frame_id));
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    on_result(frame_id);  // An ordinary function call on the current thread.
    log("callback returned; processing can continue");
}

int main() {
    log("main started");

    ResultCallback callback = [](int frame_id) {
        log("callback received frame " + std::to_string(frame_id));
        // Experiment: uncomment this line to make the callback slow.
        // std::this_thread::sleep_for(std::chrono::seconds(2));
    };

    log("A: call process_frame directly");
    process_frame(1, callback);
    log("A: main continues after processing AND callback return");

    log("B: create a worker thread");
    std::thread worker([callback] {
        process_frame(2, callback);
        process_frame(3, callback);
    });

    log("B: main can continue while the worker processes frames");
    worker.join();  // Main waits for the worker; never join it from its callback.
    log("worker finished; main exits");
}
