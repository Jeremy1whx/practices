#include "MatchingEngineThread.h"

MatchingEngineThread::MatchingEngineThread(size_t queue_size) : ingress_(queue_size){}

MatchingEngineThread::~MatchingEngineThread() {
    stop();
}

void MatchingEngineThread::start() {

    running_ = true;

    thread_ = std::thread(&MatchingEngineThread::run, this);
}

void MatchingEngineThread::stop() {

    running_ = false;

    if (thread_.joinable()) {thread_.join();}
}

bool MatchingEngineThread::submit_order(const Order& order) {
    Order copy = order;
    copy.ingress_timestamp_ns = now_ns();
    return ingress_.submit(copy);
}

void MatchingEngineThread::run() {

    Order order;

    constexpr size_t BATCH_SIZE = 64;

    std::array<Order, BATCH_SIZE> batch;

    while (running_ || !ingress_.empty()) {
        size_t count = 0;

        while (count < BATCH_SIZE &&ingress_.try_get(order)) {
            batch[count++] = order;
        }

        for (size_t i = 0; i < count; ++i) {
            engine_.submit_order(batch[i]);
        }

        if (count == 0) std::this_thread::yield();
    }
}