#define CATCH_CONFIG_MAIN
#define CATCH_CONFIG_ENABLE_BENCHMARKING 

#include "EventBus.h"
#include "Subscriber.h"
#include "EventPublisher.h"
#include "../Lock_Free_Ring_Buffer/mpsc/catch.hpp"

TEST_CASE("Multiple Subscribers") {
    exchange::EventBus bus;
    exchange::TradeSubscriber trade_sub;
    exchange::BookSubscriber book_sub;

    bus.subscribe(&trade_sub);
    bus.subscribe(&book_sub);

    exchange::Trade trade{
        .buy_order_id = 1,
        .sell_order_id = 2,
        .price = 100.0,
        .quantity = 10
    };

    exchange::TradeEvent trade_event{
        {exchange::EventType::Trade, 123},
        trade
    };

    bus.publish(trade_event);

    REQUIRE(trade_sub.count == 1);
    REQUIRE(book_sub.count == 0);
    REQUIRE(bus.stats().published_events == 1);
    REQUIRE(bus.stats().trade_events == 1);
    REQUIRE(bus.get_stats(&trade_sub) == 1);
    REQUIRE(bus.get_stats(&book_sub) == 0);

    exchange::BookUpdate update{
        .best_bid = 99.5,
        .best_ask = 100.5,
        .last_trade_price = 100.0,
        .last_trade_quantity = 10
    };

    exchange::BookUpdateEvent book_event{
        {exchange::EventType::BookUpdate, 124},
        update
    };

    bus.publish(book_event);

    REQUIRE(trade_sub.count == 1);
    REQUIRE(book_sub.count == 1);
    REQUIRE(bus.stats().published_events == 2);
    REQUIRE(bus.stats().book_update_events == 1);
}

TEST_CASE("Duplicate Subscription Prevention") {
    exchange::EventBus bus;
    exchange::TradeSubscriber sub;

    bus.subscribe(&sub);
    bus.subscribe(&sub); 

    exchange::Trade trade{
        .buy_order_id = 1,
        .sell_order_id = 2,
        .price = 100.0,
        .quantity = 10
    };

    exchange::TradeEvent trade_event{
        {exchange::EventType::Trade, 123},
        trade
    };

    bus.publish(trade_event);

    REQUIRE(sub.count == 1);
    REQUIRE(bus.get_stats(&sub) == 1);
}

TEST_CASE("Mixed Event Types") {
    exchange::EventBus bus;
    exchange::TradeSubscriber trade_sub;
    exchange::BookSubscriber book_sub;

    bus.subscribe(&trade_sub);
    bus.subscribe(&book_sub);

    for (uint32_t i = 0; i < 5; ++i) {
        exchange::Trade trade{
            .buy_order_id = static_cast<uint64_t>(i),
            .sell_order_id = static_cast<uint64_t>(i + 100),
            .price = 100.0 + i,
            .quantity = 10 + i
        };

        exchange::TradeEvent trade_event{
            {exchange::EventType::Trade, static_cast<uint64_t>(i)},
            trade
        };

        bus.publish(trade_event);

        exchange::BookUpdate update{
            .best_bid = 99.5 + i,
            .best_ask = 100.5 + i,
            .last_trade_price = 100.0 + i,
            .last_trade_quantity = 10 + i
        };

        exchange::BookUpdateEvent book_event{
            {exchange::EventType::BookUpdate, static_cast<uint64_t>(i + 1000)},
            update
        };

        bus.publish(book_event);
    }

    REQUIRE(trade_sub.count == 5);
    REQUIRE(book_sub.count == 5);
    REQUIRE(bus.stats().published_events == 10);
    REQUIRE(bus.stats().trade_events == 5);
    REQUIRE(bus.stats().book_update_events == 5);
}

TEST_CASE("Event with Empty Subscribers") {
    exchange::EventBus bus;

    exchange::Trade trade{
        .buy_order_id = 1,
        .sell_order_id = 2,
        .price = 100.0,
        .quantity = 10
    };

    exchange::TradeEvent trade_event{
        {exchange::EventType::Trade, 123},
        trade
    };

    bus.publish(trade_event);

    REQUIRE(bus.stats().published_events == 1);
    REQUIRE(bus.stats().trade_events == 1);

    exchange::TradeSubscriber sub;
    bus.subscribe(&sub);

    bus.publish(trade_event);

    REQUIRE(sub.count == 1);
    REQUIRE(bus.stats().published_events == 2);
    REQUIRE(bus.get_stats(&sub) == 1);
}

TEST_CASE("EventPublisher Integration") {
    exchange::EventBus bus;
    exchange::TradeSubscriber trade_sub;
    exchange::BookSubscriber book_sub;

    bus.subscribe(&trade_sub);
    bus.subscribe(&book_sub);

    exchange::EventPublisher publisher(bus);

    exchange::Trade trade{
        .buy_order_id = 1,
        .sell_order_id = 2,
        .price = 100.0,
        .quantity = 10
    };

    publisher.publish(trade);

    REQUIRE(trade_sub.count == 1);
    REQUIRE(book_sub.count == 0);
    REQUIRE(bus.stats().published_events == 1);
    REQUIRE(bus.stats().trade_events == 1);

    exchange::BookUpdate update{
        .best_bid = 99.5,
        .best_ask = 100.5,
        .last_trade_price = 100.0,
        .last_trade_quantity = 10
    };

    publisher.publish(update);

    REQUIRE(trade_sub.count == 1);
    REQUIRE(book_sub.count == 1);
    REQUIRE(bus.stats().published_events == 2);
    REQUIRE(bus.stats().book_update_events == 1);
}

TEST_CASE("TradeLogger Integration") {
    exchange::EventBus bus;
    AsyncLogger logger("test.log");
    logger.start();
    
    exchange::TradeLogger trade_logger(logger);
    bus.subscribe(&trade_logger);

    exchange::Trade trade{
        .buy_order_id = 1,
        .sell_order_id = 2,
        .price = 100.0,
        .quantity = 10
    };

    exchange::TradeEvent trade_event{
        {exchange::EventType::Trade, 123},
        trade
    };

    bus.publish(trade_event);
    logger.stop();

    REQUIRE(trade_logger.events_received() == 1);
    REQUIRE(bus.stats().published_events == 1);
    REQUIRE(bus.stats().trade_events == 1);

    exchange::BookUpdate update{
        .best_bid = 99.5,
        .best_ask = 100.5,
        .last_trade_price = 100.0,
        .last_trade_quantity = 10
    };

    exchange::BookUpdateEvent book_event{
        {exchange::EventType::BookUpdate, 124},
        update
    };

    bus.publish(book_event);

    REQUIRE(trade_logger.events_received() == 1);
    REQUIRE(bus.stats().published_events == 2);
    REQUIRE(bus.stats().book_update_events == 1);
}

TEST_CASE("Publish Benchmark with Multiple Subscribers") {
    constexpr size_t N = 1'000'000;
    exchange::TradeSubscriber tsub1, tsub2, tsub3;
    exchange::BookSubscriber bsub1, bsub2;
    exchange::EventBus bus;

    bus.subscribe(&tsub1);
    bus.subscribe(&tsub2);
    bus.subscribe(&tsub3);
    bus.subscribe(&bsub1);
    bus.subscribe(&bsub2);

    exchange::Trade trade{
        .buy_order_id = 1,
        .sell_order_id = 2,
        .price = 100.0,
        .quantity = 10
    };

    exchange::TradeEvent trade_event{
        {exchange::EventType::Trade, 123},
        trade
    };

    auto start = std::chrono::steady_clock::now();

    for (size_t i = 0; i < N; ++i) {
        bus.publish(trade_event);
    }

    auto end = std::chrono::steady_clock::now();

    auto duration_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
    double events_per_sec = static_cast<double>(N) * 1e9 / duration_ns;
    double avg_latency_ns = static_cast<double>(duration_ns) / N;

    std::cout << "Events/sec (5 subscribers): " << events_per_sec << std::endl;
    std::cout << "Average latency per event: " << avg_latency_ns << " ns" << std::endl;

    REQUIRE(tsub1.count == N);
    REQUIRE(tsub2.count == N);
    REQUIRE(tsub3.count == N);
    REQUIRE(bsub1.count == 0);
    REQUIRE(bsub2.count == 0);

    REQUIRE(bus.stats().published_events == N);
    REQUIRE(bus.stats().trade_events == N);
}