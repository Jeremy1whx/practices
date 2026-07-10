#pragma once

#include "../Lock_Free_Ring_Buffer/mpsc/Ringbuffer.h"
#include "LogMessage.h"

#include <string>
#include <thread>
#include <fstream>
#include <atomic>
#include <filesystem>

class AsyncLogger {
public:
    explicit AsyncLogger(const std::filesystem::path& path);

    ~AsyncLogger();

    void log(const char* msg);

    void start();

    void stop();

private:
    void process();

private:
    RingBuffer<LogMessage> queue_{1024 * 1024};

    std::thread worker_;

    std::ofstream file_;

    std::atomic<bool> running_{false};
};