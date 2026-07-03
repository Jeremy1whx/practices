#include "MatchingEngineThread.h"

exchange::MatchingEngineThread::MatchingEngineThread(MatchingEngine& engine, size_t queue_size) : engine_(engine), ingress_(queue_size){}

exchange::MatchingEngineThread::~MatchingEngineThread() {
    stop();
}

void exchange::MatchingEngineThread::start() {

    running_ = true;

    thread_ = std::thread(&MatchingEngineThread::run, this);

    pin_thread_to_core(thread_, 2);
}

void exchange::MatchingEngineThread::stop() {

    running_ = false;

    if (thread_.joinable()) {thread_.join();}
}

bool exchange::MatchingEngineThread::submit_order(Order& order) {
    order.ingress_timestamp_ns = now_ns();
    return ingress_.submit(std::move(order));
}

void exchange::MatchingEngineThread::run() {

    Order order;

    #ifdef __linux__

    std::cout << "Matching thread running on CPU " << sched_getcpu() << "\n";

    #endif

    constexpr size_t BATCH_SIZE = 64;

    std::array<Order, BATCH_SIZE> batch;

    while (running_ || !ingress_.empty()) {
        size_t count = 0;

        while (count < BATCH_SIZE &&ingress_.try_get(order)) {
            order.egress_timestamp_ns = now_ns();
            batch[count++] = std::move(order);
        }

        for (size_t i = 0; i < count; ++i) {
            engine_.process_order(batch[i]);
        }

        if (count == 0) std::this_thread::yield();
    }
}