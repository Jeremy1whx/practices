#define CATCH_CONFIG_MAIN
#define CATCH_CONFIG_ENABLE_BENCHMARKING 

#include "EventBus.h"
#include "Subscriber.h"
#include "../Lock_Free_Ring_Buffer/mpsc/catch.hpp"

// TEST_CASE("EventBusTest, Publish")
// {
//     exchange::EventBus bus;

//     exchange::TestSubscriber sub;

//     bus.subscribe(&sub);

//     exchange::Trade trade {
//         .buy_order_id = 1,
//         .sell_order_id = 2,
//         .price = 100.0,
//         .quantity = 10
//     };

//     exchange::TradeEvent tradeEvent{
//         {
//             exchange::EventType::Trade,
//             123
//         },
//         &trade
//     };

//     bus.publish(tradeEvent);

//     REQUIRE(sub.count == 1);
// }

TEST_CASE("TradeSubscriber") {
    exchange::TradeSubscriber sub;
    exchange::EventBus bus;

    bus.subscribe(&sub);

    exchange::Trade trade {
        .buy_order_id = 1,
        .sell_order_id = 2,
        .price = 100.0,
        .quantity = 10
    };

    exchange::TradeEvent trade_event{
        {
            exchange::EventType::Trade,
            123
        },
        trade
    };

    bus.publish(trade_event);
    
    REQUIRE(sub.count == 1);
    REQUIRE(bus.stats().published_events == 1);
    REQUIRE(bus.stats().trade_events == 1);
    REQUIRE(bus.get_stats(&sub).events_received == 1);
}

TEST_CASE("BookSubscriber") {
    exchange::BookSubscriber sub;
    exchange::EventBus bus;

    bus.subscribe(&sub);

    exchange::Trade trade {
        .buy_order_id = 1,
        .sell_order_id = 2,
        .price = 100.0,
        .quantity = 10
    };

    exchange::TradeEvent trade_event{
        {
            exchange::EventType::BookUpdate,
            123
        },
        trade
    };

    bus.publish(trade_event);
    
    REQUIRE(sub.count == 1);
    REQUIRE(bus.stats().published_events == 1);
    REQUIRE(bus.stats().book_update_events == 1);
    REQUIRE(bus.get_stats(&sub).events_received == 1);
}

TEST_CASE("Publish Benchmark") {
    constexpr size_t N = 10'000'000;
    exchange::TradeSubscriber tsub;
    exchange::EventBus bus;

    bus.subscribe(&tsub);

    exchange::Trade trade {
        .buy_order_id = 1,
        .sell_order_id = 2,
        .price = 100.0,
        .quantity = 10
    };

    exchange::TradeEvent trade_event{
        {
            exchange::EventType::Trade,
            123
        },
        trade
    };

    auto start = std::chrono::steady_clock::now();

    for (size_t i = 0; i < N; ++i) {
        bus.publish(trade_event);
    }

    auto end = std::chrono::steady_clock::now();

    auto duration_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
    double events_per_sec = static_cast<double>(N) * 1e9/ duration_ns;

    std::cout << "Events/sec: " << events_per_sec << std::endl;
}