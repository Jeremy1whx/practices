#pragma once

#include <array>
#include <cstring>
#include <chrono>

struct LogMessage {
    std::chrono::steady_clock::time_point timestamp;

    std::array<char, 256> buffer{};
    size_t length = 0;

    void set(std::string_view msg) {
        length = std::min(msg.size(), buffer.size() - 1);
        std::copy_n(msg.data(), length, buffer.data());
        buffer[length] = '\0'; 
    }
};