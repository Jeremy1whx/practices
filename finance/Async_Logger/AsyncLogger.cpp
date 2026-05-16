#include "AsyncLogger.h"

#include <iostream>

AsyncLogger::AsyncLogger(const std::string& filename)
    : file_(filename, std::ios::out | std::ios::app)
{
}

AsyncLogger::~AsyncLogger() {
    stop();
}

void AsyncLogger::start() {
    running_ = true;

    worker_ = std::thread(&AsyncLogger::process, this);
}

void AsyncLogger::stop() {
    running_ = false;

    if (worker_.joinable()) {
        worker_.join();
    }
}

void AsyncLogger::log(const char* msg) {
    LogMessage logMsg;
    logMsg.timestamp = std::chrono::steady_clock::now();
    logMsg.set(msg);

    while (!queue_.push(logMsg)) {
        // busy spin
    }
}

void AsyncLogger::process() {
    LogMessage msg;

    while (running_) {

        while (queue_.pop(msg)) {

            file_.write(msg.buffer.data(), msg.length);
            file_.put('\n');
        }

        file_.flush();

        std::this_thread::yield();
    }

    // flush remaining messages
    while (queue_.pop(msg)) {
        file_.write(msg.buffer.data(), msg.length);
        file_.put('\n');
    }

    file_.flush();
}