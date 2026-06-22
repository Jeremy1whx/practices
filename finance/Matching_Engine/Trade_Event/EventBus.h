#pragma once

#include "TradeEventListener.h"

#include <vector>

class EventBus {
public:

    void subscribe(TradeEventListener* listener) {
        if (listener) listeners_.push_back(listener);
    };

    void publish(const Trade& event) {
        for (auto* listener : listeners_) {
            listener->on_trade(event);
    }
};

private:
    std::vector<TradeEventListener*> listeners_;
};