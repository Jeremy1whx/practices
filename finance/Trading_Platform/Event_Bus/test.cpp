#define CATCH_CONFIG_MAIN
#define CATCH_CONFIG_ENABLE_BENCHMARKING 

#include "EventBus.h"
#include "Subscriber.h"
#include "../Trading_Platform/Lock_Free_Ring_Buffer/mpsc/catch.hpp"

TEST_CASE("EventBusTest, Publish")
{
    exchange::EventBus bus;

    exchange::TestSubscriber sub;

    bus.subscribe(&sub);

    Trade trade {
        .buy_order_id = 1,
        .sell_order_id = 2,
        .price = 100.0,
        .quantity = 10
    };

    exchange::TradeEvent tradeEvent{
        .header = {
            .type = exchange::EventType::Trade,
            .timestamp_ns = 123456789
        },
        .trade = &trade
    };

    bus.publish(tradeEvent);

    REQUIRE(sub.count == 1);
}