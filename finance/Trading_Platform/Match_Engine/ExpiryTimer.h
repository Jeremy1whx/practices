// #pragma once

// #include <atomic>
// #include <thread>
// #include <chrono>

// #include "../Latency/Clock.h"
// #include "MatchingEngineThread.h"

// namespace exchange {
// class ExpiryTimer {
// public:

//     explicit ExpiryTimer(MatchingEngineThread& matching_thread) : matching_thread_(matching_thread) {}

//     ~ExpiryTimer() {
//         stop();
//     }

//     void start() {
//         if (running_) {
//             return;
//         }

//         running_ = true;

//         thread_ = std::thread(&ExpiryTimer::run, this);
//     }

//     void stop() {
//         running_ = false;

//         if (thread_.joinable()) {
//             thread_.join();
//         }
//     }

// private:

//     MatchingEngineThread& matching_thread_;

//     std::thread thread_;

//     std::atomic<bool> running_{false};

//     void run() {
//         uint64_t next_tick = now_absolute_ns() + SECOND_NS;

//         while (running_) {uint64_t now = now_absolute_ns();

//             if (now >= next_tick) {

//                 matching_thread_.request_expiry();

//                 do {
//                     next_tick += SECOND_NS;
//                 } while (next_tick <= now);

//                 continue;
//             }

//             uint64_t remaining = next_tick - now;

//             std::this_thread::sleep_for(
//                 std::chrono::nanoseconds(remaining)
//             );
//         }
//     }
// };
// }