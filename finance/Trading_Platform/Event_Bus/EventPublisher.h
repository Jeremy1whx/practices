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

    // void publish(const uint64_t order_id, const Side& side, const double price, const uint32_t quantity) {
    //     bus_.publish(OrderAcceptedEvent{
    //         EventHeader{
    //             EventType::OrderAccepted,
    //             now_ns()
    //         },
    //         order_id,
    //         side,
    //         price,
    //         quantity            
    //     });
    // }

    void publish(const uint64_t order_id, const CancelReason& reason) {
        bus_.publish(OrderCancelledEvent{
            EventHeader{
                EventType::OrderCancelled,
                now_ns()
            },
            order_id,
            reason
        });
    }

    void publish(const uint64_t order_id, const RejectReason& reason) {
        bus_.publish(OrderRejectedEvent{
            EventHeader{
                EventType::OrderRejected,
                now_ns()
            },
            order_id,
            reason
        });
    }

    // void publish(const uint64_t order_id, const RiskViolationReason& reason) {
    //     bus_.publish(RiskViolationEvent{
    //         EventHeader{
    //             EventType::RiskViolation,
    //             now_ns()
    //         },
    //         order_id,
    //         reason
    //     });
    // }

private:
    EventBus& bus_;
};
}