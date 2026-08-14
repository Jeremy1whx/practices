#pragma once

#include <chrono>

inline uint64_t now_ns() {
    return std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
}

inline uint64_t now_absolute_ns() {
    return std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
}

constexpr uint64_t SECOND_NS = 1'000'000'000ULL;
constexpr uint64_t DAY_NS    = 86400ULL * SECOND_NS;

inline uint64_t today_start_ns_utc() {
    return (now_absolute_ns() / DAY_NS) * DAY_NS;;
}