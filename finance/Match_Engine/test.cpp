#define CATCH_CONFIG_MAIN
#define CATCH_CONFIG_ENABLE_BENCHMARKING 

#include "MatchingEngineThread.h"
#include "../Lock_Free_Ring_Buffer/mpsc/catch.hpp"
#include <chrono>

TEST_CASE("Simple full match") {
    MatchingEngine engine;
    
    Order sell{1, Side::Sell, 100.0, 10, 1};
    Order buy{2, Side::Buy, 100.0, 10, 2};
    
    engine.submit_order(sell);
    engine.submit_order(buy);
    
    REQUIRE(engine.asks().empty());
    REQUIRE(engine.bids().empty());
    REQUIRE(engine.trades().size() == 1);
    
    const auto& trade = engine.trades()[0];
    REQUIRE(trade.price == 100.0);
    REQUIRE(trade.quantity == 10);
}

TEST_CASE("Partial fill - buy less than sell") {
    MatchingEngine engine;
    
    engine.submit_order({1, Side::Sell, 100.0, 100, 1});
    engine.submit_order({2, Side::Buy, 100.0, 30, 2});
    
    REQUIRE(engine.asks().size() == 1);
    auto& ask_queue = engine.asks().begin()->second;
    REQUIRE(ask_queue.head->quantity == 70);
    
    REQUIRE(engine.bids().empty());
    REQUIRE(engine.trades().size() == 1);
    REQUIRE(engine.trades()[0].quantity == 30);
}

TEST_CASE("Partial fill - sell less than buy") {
    MatchingEngine engine;
    
    engine.submit_order({1, Side::Buy, 100.0, 100, 1});
    engine.submit_order({2, Side::Sell, 100.0, 30, 2});
    
    REQUIRE(engine.bids().size() == 1);
    auto& bid_queue = engine.bids().begin()->second;
    REQUIRE(bid_queue.head->quantity == 70);
    
    REQUIRE(engine.asks().empty());
    REQUIRE(engine.trades().size() == 1);
}

TEST_CASE("Price priority - higher bid wins") {
    MatchingEngine engine;
    
    engine.submit_order({1, Side::Buy, 99.0, 50, 1});  
    engine.submit_order({2, Side::Buy, 100.0, 50, 2}); 
    
    engine.submit_order({3, Side::Sell, 99.5, 80, 3});
    
    REQUIRE(engine.bids().size() == 1);  
    
    auto& bid_queue = engine.bids().begin()->second;  
    REQUIRE(bid_queue.head->quantity == 50);
    REQUIRE(engine.trades().size() == 1);
}

TEST_CASE("Price priority - lower ask wins") {
    MatchingEngine engine;
    
    engine.submit_order({1, Side::Sell, 101.0, 50, 1}); 
    engine.submit_order({2, Side::Sell, 99.0, 50, 2}); 
    
    engine.submit_order({3, Side::Buy, 100.0, 80, 3});
    
    REQUIRE(engine.asks().size() == 1);  
    auto& ask_queue = engine.asks().begin()->second; 
    REQUIRE(ask_queue.head->quantity == 50); 
    REQUIRE(engine.trades().size() == 1);
}

TEST_CASE("Time priority - same price") {
    MatchingEngine engine;
    
    engine.submit_order({1, Side::Sell, 100.0, 50, 1});
    engine.submit_order({2, Side::Sell, 100.0, 30, 2});
    engine.submit_order({3, Side::Sell, 100.0, 20, 3});
    
    engine.submit_order({4, Side::Buy, 100.0, 80, 4});
    
    auto& ask_queue = engine.asks().begin()->second;
    REQUIRE(ask_queue.tail->quantity == 20);
    REQUIRE(ask_queue.head->order_id == 3);  
    
    REQUIRE(engine.trades().size() == 2);
    REQUIRE(engine.trades()[0].sell_order_id == 1);
    REQUIRE(engine.trades()[1].sell_order_id == 2);
}

TEST_CASE("No match - price too low") {
    MatchingEngine engine;
    
    engine.submit_order({1, Side::Sell, 100.0, 50, 1});
    engine.submit_order({2, Side::Buy, 99.0, 30, 2});  
    
    REQUIRE(engine.asks().size() == 1);
    REQUIRE(engine.bids().size() == 1);
    REQUIRE(engine.trades().empty());
}

TEST_CASE("No match - price too high") {
    MatchingEngine engine;
    
    engine.submit_order({1, Side::Buy, 100.0, 50, 1});
    engine.submit_order({2, Side::Sell, 101.0, 30, 2}); 
    
    REQUIRE(engine.bids().size() == 1);
    REQUIRE(engine.asks().size() == 1);
    REQUIRE(engine.trades().empty());
}

TEST_CASE("Multiple price levels - buy") {
    MatchingEngine engine;
    
    engine.submit_order({1, Side::Sell, 99.0, 30, 1});
    engine.submit_order({2, Side::Sell, 99.5, 40, 2});
    engine.submit_order({3, Side::Sell, 100.0, 50, 3});
    
    engine.submit_order({4, Side::Buy, 100.0, 100, 4});
    
    REQUIRE(engine.asks().size() == 1);
    REQUIRE(engine.bids().empty());
    REQUIRE(engine.trades().size() == 3);
    
    REQUIRE(engine.trades()[0].quantity == 30); 
    REQUIRE(engine.trades()[1].quantity == 40);  
    REQUIRE(engine.trades()[2].quantity == 30);  
}

TEST_CASE("Multiple price levels - sell") {
    MatchingEngine engine;
    
    engine.submit_order({1, Side::Buy, 100.0, 30, 1});
    engine.submit_order({2, Side::Buy, 99.5, 40, 2});
    engine.submit_order({3, Side::Buy, 99.0, 50, 3});
    
    engine.submit_order({4, Side::Sell, 99.0, 100, 4});
    
    REQUIRE(engine.bids().size() == 1);
    REQUIRE(engine.asks().empty());
    REQUIRE(engine.trades().size() == 3);
}

TEST_CASE("Zero quantity order") {
    MatchingEngine engine;
    
    engine.submit_order({1, Side::Buy, 100.0, 0, 1});
    
    REQUIRE(engine.bids().empty());
    REQUIRE(engine.asks().empty());
}

TEST_CASE("Large quantities - no overflow") {
    MatchingEngine engine;
    
    uint32_t max_uint32 = std::numeric_limits<uint32_t>::max();
    
    engine.submit_order({1, Side::Sell, 100.0, max_uint32, 1});
    engine.submit_order({2, Side::Buy, 100.0, max_uint32, 2});
    
    REQUIRE(engine.trades().size() == 1);
    REQUIRE(engine.trades()[0].quantity == max_uint32);
    REQUIRE(engine.asks().empty());
    REQUIRE(engine.bids().empty());
}

TEST_CASE("Multiple orders same side") {
    MatchingEngine engine;
    
    engine.submit_order({1, Side::Buy, 100.0, 30, 1});
    engine.submit_order({2, Side::Buy, 99.0, 40, 2});
    engine.submit_order({3, Side::Buy, 101.0, 50, 3});
    
    REQUIRE(engine.bids().size() == 3);
    
    auto it = engine.bids().begin();
    REQUIRE(it->first == 101.0);  
    ++it;
    REQUIRE(it->first == 100.0);
    ++it;
    REQUIRE(it->first == 99.0); 
}

TEST_CASE("Complex scenario - interleaved orders") {
    MatchingEngine engine;
    
    engine.submit_order({1, Side::Sell, 100.0, 50, 1});
    engine.submit_order({2, Side::Buy, 99.0, 30, 2});
    engine.submit_order({3, Side::Sell, 99.5, 40, 3});
    engine.submit_order({4, Side::Buy, 101.0, 60, 4});    
   
    engine.submit_order({5, Side::Buy, 100.5, 80, 5});
    
    REQUIRE(engine.asks().empty());  
    REQUIRE(engine.bids().size() == 2);  
    REQUIRE(engine.trades().size() == 3);
}

TEST_CASE("Trade recording - buy order") {
    MatchingEngine engine;
    
    engine.submit_order({1, Side::Sell, 100.0, 50, 1});
    engine.submit_order({2, Side::Buy, 100.0, 30, 2});
    
    REQUIRE(engine.trades().size() == 1);
    const auto& trade = engine.trades()[0];
    
    REQUIRE(trade.buy_order_id == 2);
    REQUIRE(trade.sell_order_id == 1);
    REQUIRE(trade.price == 100.0);
    REQUIRE(trade.quantity == 30);
}

TEST_CASE("Trade recording - sell order") {
    MatchingEngine engine;
    
    engine.submit_order({1, Side::Buy, 100.0, 50, 1});
    engine.submit_order({2, Side::Sell, 100.0, 30, 2});
    
    REQUIRE(engine.trades().size() == 1);
    const auto& trade = engine.trades()[0];
    
    REQUIRE(trade.buy_order_id == 1);
    REQUIRE(trade.sell_order_id == 2);
    REQUIRE(trade.price == 100.0);
    REQUIRE(trade.quantity == 30);
}

TEST_CASE("Multiple trades from one order") {
    MatchingEngine engine;
    
    for (int i = 1; i <= 5; ++i) {
        engine.submit_order({uint64_t(i), Side::Sell, 100.0, 10, uint64_t(i)});
    }
    
    engine.submit_order({6, Side::Buy, 100.0, 45, 6});
    
    REQUIRE(engine.trades().size() == 5);
    REQUIRE(engine.asks().size() == 1);
    auto& ask_queue = engine.asks().begin()->second;
    REQUIRE(ask_queue.head->quantity == 5);  
}

TEST_CASE("Trade logger integration") {

    AsyncLogger logger("trade.log");

    logger.start();

    MatchingEngine engine;

    engine.set_logger(&logger);

    engine.submit_order({
        1,
        Side::Sell,
        100,
        10,
        1
    });

    engine.submit_order({
        2,
        Side::Buy,
        100,
        10,
        2
    });

    logger.stop();

    REQUIRE(engine.trades().size() == 1);
}

TEST_CASE("Market data event generation") {

    MatchingEngine engine;

    engine.submit_order({
        1,
        Side::Sell,
        101,
        10,
        1
    });

    engine.submit_order({
        2,
        Side::Buy,
        100,
        10,
        2
    });

    engine.submit_order({
        3,
        Side::Buy,
        101,
        5,
        3
    });

    REQUIRE(engine.market_data_events().size() == 1);

    const auto& md = engine.market_data_events().front();

    REQUIRE(md.last_trade_price == 101);

    REQUIRE(md.last_trade_quantity == 5);

    REQUIRE(md.best_ask.has_value());

    REQUIRE(md.best_ask.value() == 101);
}

TEST_CASE("MPSC order ingress") {

    MatchingEngineThread system;

    system.start();

    constexpr int threads = 4;
    constexpr int N = 100000;

    std::vector<std::thread> workers;

    for (int i = 0; i < threads; ++i) {

        workers.emplace_back([&, i]() {

            for (int j = 0; j < N; ++j) {

                Order order {
                    static_cast<uint64_t>(i * N + j),
                    Side::Buy,
                    100,
                    1,
                    static_cast<uint64_t>(j)
                };

                while (!system.submit_order(order)) {}
            }
        });
    }

    for (auto& t : workers) {
        t.join();
    }

    std::this_thread::sleep_for(std::chrono::seconds(1));

    system.stop();

    SUCCEED();
}

TEST_CASE("Trade latency measurement") {

    MatchingEngineThread system;

    system.start();

    Order sell {
        1,
        Side::Sell,
        100,
        10,
        1
    };

    Order buy {
        2,
        Side::Buy,
        100,
        10,
        2
    };

    REQUIRE(system.submit_order(sell));

    REQUIRE(system.submit_order(buy));

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    system.stop();

    const auto& trades = system.engine().trades();

    REQUIRE(trades.size() == 1);

    REQUIRE(trades[0].latency_ns > 0);
}

TEST_CASE("Latency benchmark") {

    MatchingEngineThread system;

    system.start();

    constexpr int N = 100000;

    auto start = std::chrono::steady_clock::now();

    for (int i = 0; i < N; ++i) {

        Order sell {
            static_cast<uint64_t>(i * 2),
            Side::Sell,
            100,
            1,
            static_cast<uint64_t>(i)
        };

        Order buy {
            static_cast<uint64_t>(i * 2 + 1),
            Side::Buy,
            100,
            1,
            static_cast<uint64_t>(i)
        };

        while (!system.submit_order(sell)) {}

        while (!system.submit_order(buy)) {}
    }

    while (system.engine().trades().size()< static_cast<size_t>(N)) {
        std::this_thread::yield();
    }

    auto end = std::chrono::steady_clock::now();

    system.stop();

    double seconds = std::chrono::duration<double>(end - start).count();

    double throughput = (2.0 * N) / seconds;

    const auto& collector = system.engine().latency_collector();

    std::cout << "\n";

    std::cout << "Throughput: " << throughput << " orders/sec\n";

    std::cout << "P50 latency: " << collector.p50() << " ns\n";

    std::cout << "P99 latency: " << collector.p99() << " ns\n";

    std::cout << "Max latency: " << collector.max() << " ns\n";

    REQUIRE(collector.size() == N);
}

TEST_CASE("Batch processing benchmark") {

    MatchingEngineThread system;

    system.start();

    constexpr int N = 500000;

    auto start = std::chrono::steady_clock::now();

    std::thread producer([&]() {

        for (int i = 0; i < N; ++i) {

            Order sell {
                static_cast<uint64_t>(i * 2),
                Side::Sell,
                100,
                1,
                static_cast<uint64_t>(i)
            };

            Order buy {
                static_cast<uint64_t>(i * 2 + 1),
                Side::Buy,
                100,
                1,
                static_cast<uint64_t>(i)
            };

            while (!system.submit_order(sell)) {}

            while (!system.submit_order(buy)) {}
        }
    });

    producer.join();

    while (system.engine().trades().size() < static_cast<size_t>(N)) {
        std::this_thread::yield();
    }

    auto end = std::chrono::steady_clock::now();

    system.stop();

    double seconds = std::chrono::duration<double>(end - start).count();

    double throughput = (2.0 * N) / seconds;

    std::cout << "\nBatch Throughput: " << throughput << " orders/sec\n";

    SUCCEED();
}

TEST_CASE("Memory pool allocation") {

    MemoryPool<int> pool(10);

    std::vector<int*> ptrs;

    for (int i = 0; i < 10; ++i) {

        auto ptr = pool.allocate();

        REQUIRE(ptr != nullptr);

        ptrs.push_back(ptr);
    }

    REQUIRE(pool.allocate() == nullptr);

    for (auto ptr : ptrs) {
        pool.deallocate(ptr);
    }

    REQUIRE(pool.available() == 10);
}

TEST_CASE("False sharing benchmark") {

    constexpr int N = 10'000'000;

    std::atomic<size_t> a{0};
    std::atomic<size_t> b{0};

    auto start = std::chrono::steady_clock::now();

    std::thread t1([&]() {
        for (int i = 0; i < N; ++i) {
            a.fetch_add(1, std::memory_order_relaxed);
        }
    });

    std::thread t2([&]() {
        for (int i = 0; i < N; ++i) {
            b.fetch_add(1, std::memory_order_relaxed);
        }
    });

    t1.join();
    t2.join();

    auto end = std::chrono::steady_clock::now();

    std::cout << "Duration: " << std::chrono::duration<double>(end - start).count() << " sec\n";

    SUCCEED();
}

TEST_CASE("Cancel existing order") {

    MatchingEngine engine;

    engine.submit_order({
        1,
        Side::Buy,
        100,
        10,
        1
    });

    REQUIRE(engine.order_lookup().size() == 1);

    REQUIRE(engine.cancel_order(1));

    REQUIRE_FALSE(engine.cancel_order(1));
}

TEST_CASE("Cancel removes order from book") {

    MatchingEngine engine;

    engine.submit_order({
        1,
        Side::Buy,
        100,
        10,
        1
    });

    REQUIRE(engine.cancel_order(1));

    REQUIRE(engine.bids().empty());
}