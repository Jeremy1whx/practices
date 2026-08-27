#pragma once

#include "MatchingEngine.h"
#include "../Persistence/SnapshotService.h"
#include "../Order/OrderIngress.h"


#include <thread>
#include <atomic>

namespace exchange {
class MatchingEngineThread {
public:

    explicit MatchingEngineThread(MatchingEngine& engine, SnapshotService& service, size_t queue_size = 1024 * 1024);

    ~MatchingEngineThread();

    void start();

    void stop();    

    bool submit_order(Order& order);

    bool submit_order(const Order& order) {
        Order copy = order;
        return submit_order(copy);
    };
    
    const MatchingEngine& engine() const {
        return engine_;
    }

    const uint64_t completed_sequence() const {return completed_snapshot_.load(std::memory_order_acquire);}

    void snapshot_requested() {snapshot_requested_.store(true, std::memory_order_relaxed);}

private:

    void run();

    void expiry_loop();

    OrderIngress ingress_;

    MatchingEngine& engine_;

    SnapshotService& service_;

    uint64_t snapshot_sequence_ = 0;

    std::thread thread_;

    std::thread expiry_thread_;

    std::atomic<bool> running_{false};

    std::atomic<bool> expiry_requested_{false};

    std::atomic<bool> snapshot_requested_{false};

    std::atomic<uint64_t> completed_snapshot_{0};
};
}