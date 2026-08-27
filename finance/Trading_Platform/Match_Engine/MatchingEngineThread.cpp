#include "MatchingEngineThread.h"

exchange::MatchingEngineThread::MatchingEngineThread(MatchingEngine& engine, SnapshotService& service, size_t queue_size) 
    : engine_(engine), service_(service), ingress_(queue_size) {}

exchange::MatchingEngineThread::~MatchingEngineThread() {
    stop();
}

void exchange::MatchingEngineThread::start() {
    if (running_) return;

    running_ = true;

    thread_ = std::thread(&MatchingEngineThread::run, this);
    expiry_thread_ = std::thread(&MatchingEngineThread::expiry_loop, this);

    pin_thread_to_core(thread_, 2);
}

void exchange::MatchingEngineThread::stop() {
    running_ = false;

    if (thread_.joinable()) {
        thread_.join();
    }
    if (expiry_thread_.joinable()) {
        expiry_thread_.join();
    }
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

    while (running_) {
        size_t count = 0;

        while (count < BATCH_SIZE && ingress_.try_get(order)) {
            order.egress_timestamp_ns = now_ns();
            batch[count++] = std::move(order);
        }

        for (size_t i = 0; i < count; ++i) {
            engine_.process_order(batch[i]);
        }

        if (expiry_requested_.load(std::memory_order_relaxed)) {            
            engine_.process_expiry(now_absolute_ns());
            expiry_requested_.store(false, std::memory_order_relaxed);
        }

        if (snapshot_requested_.load(std::memory_order_relaxed)) {
            service_.append_snapshot(snapshot_sequence_, now_absolute_ns());
            completed_snapshot_.store(snapshot_sequence_, std::memory_order_release);
            ++snapshot_sequence_;
            snapshot_requested_.store(false, std::memory_order_relaxed);
        }

        if (count == 0) {
            std::this_thread::yield();
        }
    }
}

void exchange::MatchingEngineThread::expiry_loop() {
    auto next_expiry = std::chrono::system_clock::now() + std::chrono::seconds(60);

    while (running_) {
        auto now = std::chrono::system_clock::now();

        if (now >= next_expiry) {
            expiry_requested_.store(true, std::memory_order_relaxed);
            
            do {
                next_expiry += std::chrono::seconds(60);
            } while (next_expiry <= now);
            continue;
        }

        std::this_thread::sleep_until(next_expiry);
    }
}