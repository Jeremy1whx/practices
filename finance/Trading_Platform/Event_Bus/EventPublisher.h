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

    void publish(const Order& info) {
        bus_.publish(OrderAcceptedEvent{
            EventHeader{
                EventType::OrderAccepted,
                now_ns()
            },
            info.order_id,
            info.side,
            info.type,
            info.price,
            info.quantity,
            info.sequence,
            info.expire_time            
        });
    }

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