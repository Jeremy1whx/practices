#define CATCH_CONFIG_MAIN
#define CATCH_CONFIG_ENABLE_BENCHMARKING 

#include "EventBus.h"
#include "Subscriber.h"
#include "EventPublisher.h"
#include "../Lock_Free_Ring_Buffer/mpsc/catch.hpp"

namespace exchange {

class MockAsyncLogger {
public:
    MockAsyncLogger() = default;
    void log(const char* msg) { 
        ++log_count; 
        last_msg = msg;
    }
    void start() {}
    void stop() {}
    
    size_t log_count = 0;
    std::string last_msg;
};

class CountingSubscriber : public Subscriber {
public:
    explicit CountingSubscriber(EventType type) : type_(type) {}
    
    void on_event(const Event&) override { ++count; }
    
    bool interested_in(EventType type) const override { return type == type_; }
    
    const char* name() const override { return "CountingSubscriber"; }
    
    size_t count = 0;
    
private:
    EventType type_;
};

} 

using namespace exchange;

TEST_CASE("EventBus with Single Subscriber") {
    EventBus bus;
    TradeSubscriber sub;
    bus.subscribe(&sub);
    
    Trade trade{.buy_order_id = 1, .sell_order_id = 2, .price = 100.0, .quantity = 10};
    TradeEvent event{{EventType::Trade, 123}, trade};
    
    bus.publish(event);
    
    REQUIRE(sub.count == 1);
    REQUIRE(bus.stats().published_events == 1);
    REQUIRE(bus.stats().trade_events == 1);
    REQUIRE(bus.get_stats(&sub) == 1);
}

TEST_CASE("Multiple Subscribers with Different Interests") {
    EventBus bus;
    TradeSubscriber trade_sub;
    BookSubscriber book_sub;
    CancelationSubscriber cancel_sub;
    RejectionSubscriber reject_sub;

    bus.subscribe(&trade_sub);
    bus.subscribe(&book_sub);
    bus.subscribe(&cancel_sub);
    bus.subscribe(&reject_sub);

    Trade trade{.buy_order_id = 1, .sell_order_id = 2, .price = 100.0, .quantity = 10};
    TradeEvent trade_event{{EventType::Trade, 123}, trade};
    bus.publish(trade_event);

    REQUIRE(trade_sub.count == 1);
    REQUIRE(book_sub.count == 0);
    REQUIRE(cancel_sub.count == 0);
    REQUIRE(reject_sub.count == 0);

    BookUpdate update{.best_bid = 99.5, .best_ask = 100.5, .last_trade_price = 100.0, .last_trade_quantity = 10};
    BookUpdateEvent book_event{{EventType::BookUpdate, 124}, update};
    bus.publish(book_event);

    REQUIRE(trade_sub.count == 1);
    REQUIRE(book_sub.count == 1);
    REQUIRE(cancel_sub.count == 0);
    REQUIRE(reject_sub.count == 0);

    OrderCancelledEvent cancel_event{{EventType::OrderCancelled, 125}, 1, CancelReason::UserRequest};
    bus.publish(cancel_event);

    REQUIRE(trade_sub.count == 1);
    REQUIRE(book_sub.count == 1);
    REQUIRE(cancel_sub.count == 1);
    REQUIRE(reject_sub.count == 0);

    OrderRejectedEvent reject_event{{EventType::OrderRejected, 126}, 2, RejectReason::InvalidPrice};
    bus.publish(reject_event);

    REQUIRE(trade_sub.count == 1);
    REQUIRE(book_sub.count == 1);
    REQUIRE(cancel_sub.count == 1);
    REQUIRE(reject_sub.count == 1);

    REQUIRE(bus.stats().published_events == 4);
    REQUIRE(bus.stats().trade_events == 1);
    REQUIRE(bus.stats().book_update_events == 1);
    REQUIRE(bus.stats().order_cancelled_events == 1);
    REQUIRE(bus.stats().order_rejected_events == 1);
}

TEST_CASE("Duplicate Subscription Prevention") {
    EventBus bus;
    TradeSubscriber sub;

    bus.subscribe(&sub);
    bus.subscribe(&sub); 

    Trade trade{.buy_order_id = 1, .sell_order_id = 2, .price = 100.0, .quantity = 10};
    TradeEvent event{{EventType::Trade, 123}, trade};

    bus.publish(event);

    REQUIRE(sub.count == 1);
    REQUIRE(bus.get_stats(&sub) == 1);
}

TEST_CASE("Event with No Subscribers") {
    EventBus bus;

    Trade trade{.buy_order_id = 1, .sell_order_id = 2, .price = 100.0, .quantity = 10};
    TradeEvent event{{EventType::Trade, 123}, trade};

    bus.publish(event);

    REQUIRE(bus.stats().published_events == 1);
    REQUIRE(bus.stats().trade_events == 1);

    TradeSubscriber sub;
    bus.subscribe(&sub);

    bus.publish(event);

    REQUIRE(sub.count == 1);
    REQUIRE(bus.stats().published_events == 2);
    REQUIRE(bus.get_stats(&sub) == 1);
}

TEST_CASE("EventPublisher Integration") {
    EventBus bus;
    TradeSubscriber trade_sub;
    BookSubscriber book_sub;

    bus.subscribe(&trade_sub);
    bus.subscribe(&book_sub);

    EventPublisher publisher(bus);

    Trade trade{.buy_order_id = 1, .sell_order_id = 2, .price = 100.0, .quantity = 10};
    publisher.publish(trade);

    REQUIRE(trade_sub.count == 1);
    REQUIRE(book_sub.count == 0);
    REQUIRE(bus.stats().published_events == 1);
    REQUIRE(bus.stats().trade_events == 1);

    BookUpdate update{.best_bid = 99.5, .best_ask = 100.5, .last_trade_price = 100.0, .last_trade_quantity = 10};
    publisher.publish(update);

    REQUIRE(trade_sub.count == 1);
    REQUIRE(book_sub.count == 1);
    REQUIRE(bus.stats().published_events == 2);
    REQUIRE(bus.stats().book_update_events == 1);

    publisher.publish(1, CancelReason::UserRequest);
    REQUIRE(bus.stats().order_cancelled_events == 1);

    publisher.publish(2, RejectReason::InvalidPrice);
    REQUIRE(bus.stats().order_rejected_events == 1);
}

TEST_CASE("Mixed Event Types Many Events") {
    EventBus bus;
    TradeSubscriber trade_sub;
    BookSubscriber book_sub;
    CancelationSubscriber cancel_sub;
    RejectionSubscriber reject_sub;

    bus.subscribe(&trade_sub);
    bus.subscribe(&book_sub);
    bus.subscribe(&cancel_sub);
    bus.subscribe(&reject_sub);

    const size_t N = 100;

    for (size_t i = 0; i < N; ++i) {
        Trade trade{.buy_order_id = static_cast<uint64_t>(i), 
                    .sell_order_id = static_cast<uint64_t>(i + 100),
                    .price = 100.0 + i, 
                    .quantity = 10 + static_cast<uint32_t>(i)};
        TradeEvent trade_event{{EventType::Trade, static_cast<uint64_t>(i)}, trade};
        bus.publish(trade_event);

        BookUpdate update{.best_bid = 99.5 + i, .best_ask = 100.5 + i, 
                          .last_trade_price = 100.0 + i, 
                          .last_trade_quantity = 10 + static_cast<uint32_t>(i)};
        BookUpdateEvent book_event{{EventType::BookUpdate, static_cast<uint64_t>(i + 1000)}, update};
        bus.publish(book_event);

        if (i % 2 == 0) {
            OrderCancelledEvent cancel_event{{EventType::OrderCancelled, static_cast<uint64_t>(i + 2000)}, 
                                             static_cast<uint64_t>(i), CancelReason::UserRequest};
            bus.publish(cancel_event);
        } else {
            OrderRejectedEvent reject_event{{EventType::OrderRejected, static_cast<uint64_t>(i + 3000)}, 
                                            static_cast<uint64_t>(i), RejectReason::InvalidPrice};
            bus.publish(reject_event);
        }
    }

    REQUIRE(trade_sub.count == N);
    REQUIRE(book_sub.count == N);
    REQUIRE(cancel_sub.count == (N + 1) / 2); 
    REQUIRE(reject_sub.count == N / 2);

    REQUIRE(bus.stats().published_events == 3 * N); 
    REQUIRE(bus.stats().trade_events == N);
    REQUIRE(bus.stats().book_update_events == N);
    REQUIRE(bus.stats().order_cancelled_events == (N + 1) / 2);
    REQUIRE(bus.stats().order_rejected_events == N / 2);
}

TEST_CASE("MarketDataService Integration") {
    EventBus bus;
    MarketDataService market_data;
    bus.subscribe(&market_data);

    REQUIRE(market_data.latest().best_bid.has_value() == false);
    REQUIRE(market_data.latest().best_ask.has_value() == false);

    BookUpdate update1{.best_bid = 99.5, .best_ask = 100.5, 
                       .last_trade_price = 100.0, .last_trade_quantity = 10};
    BookUpdateEvent event1{{EventType::BookUpdate, 123}, update1};
    bus.publish(event1);

    REQUIRE(market_data.latest().best_bid.value() == 99.5);
    REQUIRE(market_data.latest().best_ask.value() == 100.5);
    REQUIRE(market_data.latest().last_trade_price.value() == 100.0);
    REQUIRE(market_data.latest().last_trade_quantity.value() == 10);

    BookUpdate update2{.best_bid = 98.0, .best_ask = 101.0, 
                       .last_trade_price = 99.0, .last_trade_quantity = 15};
    BookUpdateEvent event2{{EventType::BookUpdate, 124}, update2};
    bus.publish(event2);

    REQUIRE(market_data.latest().best_bid.value() == 98.0);
    REQUIRE(market_data.latest().best_ask.value() == 101.0);
    REQUIRE(market_data.latest().last_trade_price.value() == 99.0);
    REQUIRE(market_data.latest().last_trade_quantity.value() == 15);

    Trade trade{.buy_order_id = 1, .sell_order_id = 2, .price = 200.0, .quantity = 20};
    TradeEvent trade_event{{EventType::Trade, 125}, trade};
    bus.publish(trade_event);

    REQUIRE(market_data.latest().best_bid.value() == 98.0);
    REQUIRE(market_data.latest().best_ask.value() == 101.0);

    REQUIRE(bus.stats().published_events == 3);
    REQUIRE(bus.stats().book_update_events == 2);
    REQUIRE(bus.stats().trade_events == 1);
}

TEST_CASE("Multiple Subscribers with Different Counts") {
    EventBus bus;
    
    TradeSubscriber tsub1, tsub2, tsub3;
    BookSubscriber bsub1, bsub2;
    
    bus.subscribe(&tsub1);
    bus.subscribe(&tsub2);
    bus.subscribe(&tsub3);
    bus.subscribe(&bsub1);
    bus.subscribe(&bsub2);

    const size_t N = 50;

    for (size_t i = 0; i < N; ++i) {
        Trade trade{.buy_order_id = static_cast<uint64_t>(i), 
                    .sell_order_id = static_cast<uint64_t>(i + 100),
                    .price = 100.0 + i, 
                    .quantity = 10 + static_cast<uint32_t>(i)};
        TradeEvent trade_event{{EventType::Trade, static_cast<uint64_t>(i)}, trade};
        bus.publish(trade_event);
    }

    REQUIRE(tsub1.count == N);
    REQUIRE(tsub2.count == N);
    REQUIRE(tsub3.count == N);
    
    REQUIRE(bsub1.count == 0);
    REQUIRE(bsub2.count == 0);

    REQUIRE(bus.stats().published_events == N);
    REQUIRE(bus.stats().trade_events == N);
    REQUIRE(bus.stats().book_update_events == 0);
}

TEST_CASE("Subscriber Stats Tracking") {
    EventBus bus;
    TradeSubscriber tsub;
    BookSubscriber bsub;
    
    bus.subscribe(&tsub);
    bus.subscribe(&bsub);

    REQUIRE(bus.get_stats(&tsub) == 0);
    REQUIRE(bus.get_stats(&bsub) == 0);

    Trade trade{.buy_order_id = 1, .sell_order_id = 2, .price = 100.0, .quantity = 10};
    TradeEvent trade_event{{EventType::Trade, 123}, trade};
    bus.publish(trade_event);

    REQUIRE(bus.get_stats(&tsub) == 1);
    REQUIRE(bus.get_stats(&bsub) == 0);

    BookUpdate update{.best_bid = 99.5, .best_ask = 100.5, .last_trade_price = 100.0, .last_trade_quantity = 10};
    BookUpdateEvent book_event{{EventType::BookUpdate, 124}, update};
    bus.publish(book_event);

    REQUIRE(bus.get_stats(&tsub) == 1);
    REQUIRE(bus.get_stats(&bsub) == 1);

    bus.publish(trade_event);

    REQUIRE(bus.get_stats(&tsub) == 2);
    REQUIRE(bus.get_stats(&bsub) == 1);
}

TEST_CASE("EventBus Print Stats") {
    EventBus bus;
    TradeSubscriber tsub;
    BookSubscriber bsub;
    
    bus.subscribe(&tsub);
    bus.subscribe(&bsub);

    Trade trade{.buy_order_id = 1, .sell_order_id = 2, .price = 100.0, .quantity = 10};
    TradeEvent trade_event{{EventType::Trade, 123}, trade};
    bus.publish(trade_event);

    BookUpdate update{.best_bid = 99.5, .best_ask = 100.5, .last_trade_price = 100.0, .last_trade_quantity = 10};
    BookUpdateEvent book_event{{EventType::BookUpdate, 124}, update};
    bus.publish(book_event);

    bus.print_stats();
}

TEST_CASE("Benchmark Trade Events with Multiple Subscribers") {
    constexpr size_t N = 10'000'000;
    TradeSubscriber tsub1, tsub2, tsub3;
    BookSubscriber bsub1, bsub2;
    EventBus bus;

    bus.subscribe(&tsub1);
    bus.subscribe(&tsub2);
    bus.subscribe(&tsub3);
    bus.subscribe(&bsub1);
    bus.subscribe(&bsub2);

    Trade trade{.buy_order_id = 1, .sell_order_id = 2, .price = 100.0, .quantity = 10};
    TradeEvent trade_event{{EventType::Trade, 123}, trade};

    auto start = std::chrono::steady_clock::now();

    for (size_t i = 0; i < N; ++i) {
        bus.publish(trade_event);
    }

    auto end = std::chrono::steady_clock::now();

    auto duration_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
    double events_per_sec = static_cast<double>(N) * 1e9 / duration_ns;
    double avg_latency_ns = static_cast<double>(duration_ns) / N;

    std::cout << "Benchmark: " << N << " Trade events to 5 subscribers (3 Trade, 2 Book)" << std::endl;
    std::cout << "Events/sec: " << events_per_sec << std::endl;
    std::cout << "Average latency per event: " << avg_latency_ns << " ns" << std::endl;

    REQUIRE(tsub1.count == N);
    REQUIRE(tsub2.count == N);
    REQUIRE(tsub3.count == N);
    REQUIRE(bsub1.count == 0);
    REQUIRE(bsub2.count == 0);

    REQUIRE(bus.stats().published_events == N);
    REQUIRE(bus.stats().trade_events == N);
}

TEST_CASE("Benchmark Mixed Events with Subscribers") {
    constexpr size_t N = 10'000'000;
    TradeSubscriber tsub;
    BookSubscriber bsub;
    CancelationSubscriber csub;
    RejectionSubscriber rsub;
    EventBus bus;

    bus.subscribe(&tsub);
    bus.subscribe(&bsub);
    bus.subscribe(&csub);
    bus.subscribe(&rsub);

    auto start = std::chrono::steady_clock::now();

    for (size_t i = 0; i < N; ++i) {
        Trade trade{.buy_order_id = static_cast<uint64_t>(i), 
                    .sell_order_id = static_cast<uint64_t>(i + 100),
                    .price = 100.0 + i, 
                    .quantity = 10 + static_cast<uint32_t>(i)};
        TradeEvent trade_event{{EventType::Trade, static_cast<uint64_t>(i)}, trade};
        bus.publish(trade_event);

        BookUpdate update{.best_bid = 99.5 + i, .best_ask = 100.5 + i, 
                          .last_trade_price = 100.0 + i, 
                          .last_trade_quantity = 10 + static_cast<uint32_t>(i)};
        BookUpdateEvent book_event{{EventType::BookUpdate, static_cast<uint64_t>(i + 1000)}, update};
        bus.publish(book_event);

        if (i % 2 == 0) {
            OrderCancelledEvent cancel_event{{EventType::OrderCancelled, static_cast<uint64_t>(i + 2000)}, 
                                             static_cast<uint64_t>(i), CancelReason::UserRequest};
            bus.publish(cancel_event);
        } else {
            OrderRejectedEvent reject_event{{EventType::OrderRejected, static_cast<uint64_t>(i + 3000)}, 
                                            static_cast<uint64_t>(i), RejectReason::InvalidPrice};
            bus.publish(reject_event);
        }
    }

    auto end = std::chrono::steady_clock::now();
    auto duration_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
    double events_per_sec = static_cast<double>(4 * N) * 1e9 / duration_ns;
    double avg_latency_ns = static_cast<double>(duration_ns) / (4 * N);

    std::cout << "Benchmark: " << 4 * N << " mixed events to 4 subscribers" << std::endl;
    std::cout << "Events/sec: " << events_per_sec << std::endl;
    std::cout << "Average latency per event: " << avg_latency_ns << " ns" << std::endl;

    REQUIRE(tsub.count == N);
    REQUIRE(bsub.count == N);
    REQUIRE(csub.count == N / 2 + (N % 2));
    REQUIRE(rsub.count == N / 2);
}