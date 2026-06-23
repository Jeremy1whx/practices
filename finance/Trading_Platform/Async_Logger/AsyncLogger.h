#pragma once

#include "../Lock_Free_Ring_Buffer/mpsc/Ringbuffer.h"
#include "LogMessage.h"

#include <string>
#include <thread>
#include <fstream>
#include <atomic>

class AsyncLogger {
public:
    explicit AsyncLogger(const std::string& filename);

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