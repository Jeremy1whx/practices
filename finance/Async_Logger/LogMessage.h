#pragma once

#include <array>
#include <cstring>
#include <chrono>

struct LogMessage {
    std::chrono::steady_clock::time_point timestamp;

    std::array<char, 256> buffer{};
    size_t length = 0;

    void set(const char* msg) {
        length = std::min(strlen(msg), buffer.size() - 1);
        memcpy(buffer.data(), msg, length);
        buffer[length] = '\0';
    }
};