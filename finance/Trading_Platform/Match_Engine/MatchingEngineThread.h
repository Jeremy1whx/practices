#pragma once

#include "MatchingEngine.h"
#include "../Order/OrderIngress.h"


#include <thread>
#include <atomic>

namespace exchange {
class MatchingEngineThread {
public:

    explicit MatchingEngineThread(MatchingEngine& engine, size_t queue_size = 1024 * 1024);

    ~MatchingEngineThread();

    void start();

    void stop();

    bool empty() const;

    bool submit_order(Order& order);

    bool submit_order(const Order& order) {
        Order copy = order;
        return submit_order(copy);
    };
    
    const MatchingEngine& engine() const {
        return engine_;
    }

private:

    void run();

    OrderIngress ingress_;

    MatchingEngine& engine_;

    std::thread thread_;

    std::atomic<bool> running_{false};
};
}