#pragma once

#include "MatchingEngine.h"
#include "OrderIngress.h"


#include <thread>
#include <atomic>

class MatchingEngineThread {
public:

    explicit MatchingEngineThread(
        size_t queue_size = 1024 * 1024, TradeEventListener* listener = nullptr
    );

    ~MatchingEngineThread();

    void start();

    void stop();

    bool empty() const;

    bool submit_order(const Order& order);

    MatchingEngine& engine() {
        return engine_;
    }

private:

    void run();

    OrderIngress ingress_;

    MatchingEngine engine_;

    TradeEventListener* listener_ = nullptr;

    std::thread thread_;

    std::atomic<bool> running_{false};
};