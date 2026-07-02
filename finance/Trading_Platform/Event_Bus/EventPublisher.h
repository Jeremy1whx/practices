#pragma once

#include "../Latency/Clock.h"
#include "Trade.h"
#include "EventBus.h"

namespace exchange{

class EventPublisher {
public:
    EventPublisher(EventBus& bus) : bus_(bus) {}

    void publish(const Trade& trade) {
        bus_.publish(TradeEvent{
            EventHeader{
                EventType::Trade,
                now_ns()
            }, trade
        });
    }

    void publish(const BookUpdate& update) {
        bus_.publish(BookUpdateEvent{
            EventHeader{
                EventType::BookUpdate,
                now_ns()
            },
            update
        });
    }

private:
    EventBus& bus_;
};
}