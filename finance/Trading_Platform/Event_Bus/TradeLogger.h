#pragma once

#include "TradeFormatter.h"
#include "Subscriber.h"
#include "../Async_Logger/AsyncLogger.h"

namespace exchange {
class TradeLogger : public Subscriber {
public:
    explicit TradeLogger(AsyncLogger& logger) : logger_(logger) {}
    void on_event(const Event& event) override{        
        logger_.log(format_trade(std::get<TradeEvent>(event).trade).c_str());
    }

    bool interested_in(EventType type) const override {
        return type == EventType::Trade;
    }

    const char* name() const override {
        return "TradeLogger";
    }

    
private:
    AsyncLogger& logger_;  
};
}