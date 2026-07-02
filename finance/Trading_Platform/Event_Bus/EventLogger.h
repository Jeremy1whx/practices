#pragma once

#include "TradeFormatter.h"
#include "Subscriber.h"
#include "../Async_Logger/AsyncLogger.h"

namespace exchange {
class EventLogger : public Subscriber {
public:
    void on_event(const Event& event) override{        
        std::visit(
        [this](const auto& e) {
            handle(e);
        },
        event);
    }

    
private:
    void handle(const TradeEvent& event) {
        logger_.log(format_trade(event.trade).c_str());
    }

    void handle(const BookUpdateEvent& event) {}

    AsyncLogger& logger_;  
};
}