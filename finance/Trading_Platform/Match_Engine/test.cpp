#define CATCH_CONFIG_MAIN
#define CATCH_CONFIG_ENABLE_BENCHMARKING 

#include "MatchingEngineThread.h"
// #include "ExpiryTimer.h"
#include "../Lock_Free_Ring_Buffer/mpsc/catch.hpp"
#include <chrono>

class TestEnvironment {
public:
    TestEnvironment() 
        : bus_(),
          publisher_(bus_),
          scheduler_(),
          risk_engine_(2048),
          engine_(publisher_, scheduler_, risk_engine_),
          event_queue_(1024 * 1024),
          event_writer_("./test_journal", event_queue_),
          service_(engine_, event_writer_, risk_engine_),
          thread_(engine_, service_, 1024 * 1024) {
        RiskLimits limits;
        limits.max_order_size = std::numeric_limits<uint32_t>::max(); 
        limits.max_orders_per_sec = std::numeric_limits<uint32_t>::max();
        limits.max_position = 1'000'000'000'000LL;
        limits.max_credit = 1'000'000'000'000'000LL;
        limits.max_daily_loss = 1'000'000'000'000'000LL;
        limits.price_band_pct = 0.0;
        limits.stp_policy = STPPolicy::CancelNewest;

        for (int i = 0; i < 1024; ++i) {
            risk_engine_.register_account(limits);
        }
        scheduler_.set_cancel_callback([this](uint64_t order_id, uint64_t account_id, exchange::CancelReason reason) {
            engine_.cancel_order(order_id, account_id, reason);
        });
    }
    
    ~TestEnvironment() {
        thread_.stop();
        if (std::filesystem::exists("./test_journal")) {
            std::filesystem::remove_all("./test_journal");
        }
    }

    exchange::EventBus& bus() { return bus_; }
    exchange::EventPublisher& publisher() { return publisher_; }
    exchange::ExpiryScheduler& scheduler() { return scheduler_; }
    exchange::RiskEngine& risk_engine() {return risk_engine_;}
    exchange::MatchingEngine& engine() { return engine_; }
    exchange::MatchingEngineThread& thread() { return thread_; }
    exchange::SnapshotService& service() { return service_; }
    exchange::EventBinaryWriter& writer() { return event_writer_; }
    
    void start() {
        thread_.start();
    }
    
    void stop() {
        thread_.stop();
    }
    
private:
    exchange::EventBus bus_;
    exchange::EventPublisher publisher_;
    exchange::ExpiryScheduler scheduler_;
    exchange::MatchingEngine engine_;
    exchange::RiskEngine risk_engine_;
    MPSCRingBuffer<exchange::Event> event_queue_;
    exchange::EventBinaryWriter event_writer_;
    exchange::SnapshotService service_;
    exchange::MatchingEngineThread thread_;
};

Order make_order(uint64_t order_id, uint64_t account_id, Side side, double price, uint32_t qty, Type type = Type::GTC) {
    Order order;
    order.order_id = order_id;
    order.account_id = account_id;
    order.side = side;
    order.type = type;
    order.price = price;
    order.quantity = qty;
    order.sequence = order_id;
    order.expire_time = 0;
    order.ingress_timestamp_ns = 0;
    order.egress_timestamp_ns = 0;
    order.next = nullptr;
    order.prev = nullptr;
    return order;
}

TestEnvironment create_test_env() {
    return TestEnvironment();
}

TEST_CASE("Simple full match") {
    auto env = create_test_env();
    env.start();
    
    auto sell = make_order(1, 1, Side::Sell, 100.0, 10);
    auto buy = make_order(2, 2, Side::Buy, 100.0, 10);
    
    env.thread().submit_order(sell);
    env.thread().submit_order(buy);
    
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    env.stop();
    
    REQUIRE(env.engine().asks().empty());
    REQUIRE(env.engine().bids().empty());
    REQUIRE(env.engine().trade_count() == 1);
    
    const auto& trades = env.engine().trades();
    REQUIRE(trades.size() == 1);
    REQUIRE(trades[0].price == 100.0);
    REQUIRE(trades[0].quantity == 10);

    const auto* acc1 = env.risk_engine().get_account(1);
    REQUIRE(acc1->position == -10);
    REQUIRE(acc1->cash_flow == 100000); 
    REQUIRE(acc1->open_sell_quantity == 0);

    const auto* acc2 = env.risk_engine().get_account(2); 
    REQUIRE(acc2->position == 10);
    REQUIRE(acc2->cash_flow == -100000); 
    REQUIRE(acc2->open_buy_quantity == 0);
}

TEST_CASE("Partial fill - buy less than sell") {
    auto env = create_test_env();
    env.start();

    auto sell = make_order(1, 1, Side::Sell, 100.0, 100);
    auto buy = make_order(2, 2, Side::Buy, 100.0, 30);
    
    env.thread().submit_order(sell);
    env.thread().submit_order(buy);
    
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    env.stop();
    
    REQUIRE(env.engine().asks().size() == 1);
    auto& ask_queue = env.engine().asks().begin()->level;
    REQUIRE(ask_queue.head->quantity == 70);
    
    REQUIRE(env.engine().bids().empty());
    REQUIRE(env.engine().trades().size() == 1);
    REQUIRE(env.engine().trades()[0].quantity == 30);
}

TEST_CASE("Partial fill - sell less than buy") {
    auto env = create_test_env();
    env.start();

    auto buy = make_order(1, 1, Side::Buy, 100.0, 100);
    auto sell = make_order(2, 2, Side::Sell, 100.0, 30);
    
    env.thread().submit_order(buy);
    env.thread().submit_order(sell);
    
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    env.stop();
    
    REQUIRE(env.engine().bids().size() == 1);
    auto& bid_queue = env.engine().bids().begin()->level;
    REQUIRE(bid_queue.head->quantity == 70);
    
    REQUIRE(env.engine().asks().empty());
    REQUIRE(env.engine().trades().size() == 1);
}

TEST_CASE("Price priority - higher bid wins") {
    auto env = create_test_env();
    env.start();

    auto buy1 = make_order(1, 1, Side::Buy, 99.0, 50);
    auto buy2 = make_order(2, 2, Side::Buy, 100.0, 50);
    auto sell = make_order(3, 3, Side::Sell, 99.5, 80);
    
    env.thread().submit_order(buy1);
    env.thread().submit_order(buy2);
    env.thread().submit_order(sell);
    
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    env.stop();
    
    REQUIRE(env.engine().bids().size() == 1);
    auto& bid_queue = env.engine().bids().begin()->level;
    REQUIRE(bid_queue.head->quantity == 50);
    REQUIRE(env.engine().trades().size() == 1);
}

TEST_CASE("Price priority - lower ask wins") {
    auto env = create_test_env();
    env.start();

    auto sell1 = make_order(1, 1, Side::Sell, 101.0, 50);
    auto sell2 = make_order(2, 2, Side::Sell, 99.0, 50);
    auto buy = make_order(3, 3, Side::Buy, 100.0, 80);
    
    env.thread().submit_order(sell1);
    env.thread().submit_order(sell2);
    env.thread().submit_order(buy);
    
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    env.stop();
    
    REQUIRE(env.engine().asks().size() == 1);
    auto& ask_queue = env.engine().asks().begin()->level;
    REQUIRE(ask_queue.head->quantity == 50);
    REQUIRE(env.engine().trades().size() == 1);
}

TEST_CASE("Time priority - same price") {
    auto env = create_test_env();
    env.start();

    auto sell1 = make_order(1, 1, Side::Sell, 100.0, 50);
    auto sell2 = make_order(2, 2, Side::Sell, 100.0, 30);
    auto sell3 = make_order(3, 3, Side::Sell, 100.0, 20);
    auto buy = make_order(4, 4, Side::Buy, 100.0, 80);
    
    env.thread().submit_order(sell1);
    env.thread().submit_order(sell2);
    env.thread().submit_order(sell3);
    env.thread().submit_order(buy);
    
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    env.stop();
    
    auto& ask_queue = env.engine().asks().begin()->level;
    REQUIRE(ask_queue.tail->quantity == 20);
    REQUIRE(ask_queue.head->order_id == 3);
    
    REQUIRE(env.engine().trades().size() == 2);
    REQUIRE(env.engine().trades()[0].sell_order_id == 1);
    REQUIRE(env.engine().trades()[1].sell_order_id == 2);
}

TEST_CASE("No match - price too low") {
    auto env = create_test_env();
    env.start();

    auto sell = make_order(1, 1, Side::Sell, 100.0, 50);
    auto buy = make_order(2, 2, Side::Buy, 99.0, 30);
    
    env.thread().submit_order(sell);
    env.thread().submit_order(buy);
    
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    env.stop();
    
    REQUIRE(env.engine().asks().size() == 1);
    REQUIRE(env.engine().bids().size() == 1);
    REQUIRE(env.engine().trades().empty());
}

TEST_CASE("No match - price too high") {
    auto env = create_test_env();
    env.start();

    auto buy = make_order(1, 1, Side::Buy, 100.0, 50);
    auto sell = make_order(2, 2, Side::Sell, 101.0, 30);
    
    env.thread().submit_order(buy);
    env.thread().submit_order(sell);
    
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    env.stop();
    
    REQUIRE(env.engine().bids().size() == 1);
    REQUIRE(env.engine().asks().size() == 1);
    REQUIRE(env.engine().trades().empty());
}

TEST_CASE("Multiple price levels - buy") {
    auto env = create_test_env();
    env.start();

    auto sell1 = make_order(1, 1, Side::Sell, 99.0, 30);
    auto sell2 = make_order(2, 2, Side::Sell, 99.5, 40);
    auto sell3 = make_order(3, 3, Side::Sell, 100.0, 50);
    auto buy = make_order(4, 4, Side::Buy, 100.0, 100);
    
    env.thread().submit_order(sell1);
    env.thread().submit_order(sell2);
    env.thread().submit_order(sell3);
    env.thread().submit_order(buy);
    
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    env.stop();
    
    REQUIRE(env.engine().asks().size() == 1);
    REQUIRE(env.engine().bids().empty());
    REQUIRE(env.engine().trades().size() == 3);
    
    REQUIRE(env.engine().trades()[0].quantity == 30);
    REQUIRE(env.engine().trades()[1].quantity == 40);
    REQUIRE(env.engine().trades()[2].quantity == 30);
}

TEST_CASE("Multiple price levels - sell") {
    auto env = create_test_env();
    env.start();
    
    auto buy1 = make_order(1, 1, Side::Buy, 100.0, 30);
    auto buy2 = make_order(2, 2, Side::Buy, 99.5, 40);
    auto buy3 = make_order(3, 3, Side::Buy, 99.0, 50);
    auto sell = make_order(4, 4, Side::Sell, 99.0, 100);

    env.thread().submit_order(buy1);
    env.thread().submit_order(buy2);
    env.thread().submit_order(buy3);
    env.thread().submit_order(sell);
    
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    env.stop();
    
    REQUIRE(env.engine().bids().size() == 1);
    REQUIRE(env.engine().asks().empty());
    REQUIRE(env.engine().trades().size() == 3);
}

TEST_CASE("Zero quantity order") {
    auto env = create_test_env();
    env.start();

    auto buy = make_order(1, 1, Side::Buy, 100.0, 0);
    
    env.thread().submit_order(buy);
    
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    env.stop();
    
    REQUIRE(env.engine().bids().empty());
    REQUIRE(env.engine().asks().empty());
}

TEST_CASE("Large quantities - no overflow") {
    auto env = create_test_env();
    env.start();
    
    uint32_t max_uint32 = std::numeric_limits<uint32_t>::max();

    auto sell = make_order(1, 1, Side::Sell, 100.0, max_uint32);
    auto buy = make_order(2, 2, Side::Buy, 100.0, max_uint32);
    
    env.thread().submit_order(sell);
    env.thread().submit_order(buy);
    
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    env.stop();
    
    REQUIRE(env.engine().trades().size() == 1);
    REQUIRE(env.engine().trades()[0].quantity == max_uint32);
    REQUIRE(env.engine().asks().empty());
    REQUIRE(env.engine().bids().empty());
}

TEST_CASE("Multiple orders same side") {
    auto env = create_test_env();
    env.start();

    auto buy1 = make_order(1, 1, Side::Buy, 100.0, 30);
    auto buy2 = make_order(2, 2, Side::Buy, 99.0, 40);
    auto buy3 = make_order(3, 3, Side::Buy, 101.0, 50);
    
    env.thread().submit_order(buy1);
    env.thread().submit_order(buy2);
    env.thread().submit_order(buy3);
    
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    env.stop();
    
    REQUIRE(env.engine().bids().size() == 3);
    
    auto it = env.engine().bids().begin();
    REQUIRE(it->price == 101.0);
    ++it;
    REQUIRE(it->price == 100.0);
    ++it;
    REQUIRE(it->price == 99.0);
}

TEST_CASE("Complex scenario - interleaved orders") {
    auto env = create_test_env();
    env.start();

    auto sell1 = make_order(1, 1, Side::Sell, 100.0, 50);
    auto sell2 = make_order(3, 3, Side::Sell, 99.5, 40);
    auto buy1 = make_order(2, 2, Side::Buy, 99.0, 30);
    auto buy2 = make_order(4, 4, Side::Buy, 101.0, 60);
    auto buy3 = make_order(5, 5, Side::Buy, 100.5, 80);
    
    env.thread().submit_order(sell1);
    env.thread().submit_order(buy1);
    env.thread().submit_order(sell2);
    env.thread().submit_order(buy2);
    env.thread().submit_order(buy3);
    
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    env.stop();
    
    REQUIRE(env.engine().asks().empty());
    REQUIRE(env.engine().bids().size() == 2);
    REQUIRE(env.engine().trades().size() == 3);
}

TEST_CASE("Trade recording - buy order") {
    auto env = create_test_env();
    env.start();

    auto sell = make_order(1, 1, Side::Sell, 100.0, 50);
    auto buy = make_order(2, 2, Side::Buy, 100.0, 30);
    
    env.thread().submit_order(sell);
    env.thread().submit_order(buy);
    
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    env.stop();
    
    REQUIRE(env.engine().trades().size() == 1);
    const auto& trade = env.engine().trades()[0];
    
    REQUIRE(trade.buy_order_id == 2);
    REQUIRE(trade.sell_order_id == 1);
    REQUIRE(trade.price == 100.0);
    REQUIRE(trade.quantity == 30);
}

TEST_CASE("Trade recording - sell order") {
    auto env = create_test_env();
    env.start();

    auto buy = make_order(1, 1, Side::Buy, 100.0, 50);
    auto sell = make_order(2, 2, Side::Sell, 100.0, 30);
    
    env.thread().submit_order(buy);
    env.thread().submit_order(sell);
    
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    env.stop();
    
    REQUIRE(env.engine().trades().size() == 1);
    const auto& trade = env.engine().trades()[0];
    
    REQUIRE(trade.buy_order_id == 1);
    REQUIRE(trade.sell_order_id == 2);
    REQUIRE(trade.price == 100.0);
    REQUIRE(trade.quantity == 30);
}

TEST_CASE("Multiple trades from one order") {
    auto env = create_test_env();
    env.start();
    
    for (int i = 1; i <= 5; ++i) {
        auto sell = make_order(static_cast<uint64_t>(i), static_cast<uint64_t>(i), Side::Sell, 100.0, 10);
        env.thread().submit_order(sell);
    }

    auto buy = make_order(6, 6, Side::Buy, 100.0, 45);
    env.thread().submit_order(buy);
    
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    env.stop();
    
    REQUIRE(env.engine().trades().size() == 5);
    REQUIRE(env.engine().asks().size() == 1);
    auto& ask_queue = env.engine().asks().begin()->level;
    REQUIRE(ask_queue.head->quantity == 5);
}

TEST_CASE("MPSC order ingress") {
    auto env = create_test_env();
    env.start();

    constexpr int threads = 4;
    constexpr int N = 250000;

    std::vector<std::thread> workers;

    for (int i = 0; i < threads; ++i) {
        workers.emplace_back([&, i]() {
            for (int j = 0; j < N; ++j) {
                auto order = make_order(
                    static_cast<uint64_t>(i * N + j),
                    1,
                    Side::Buy,
                    100.0,
                    1
                );
                while (!env.thread().submit_order(order)) {std::this_thread::yield();}
            }
        });
    }

    for (auto& t : workers) {
        t.join();
    }

    std::this_thread::sleep_for(std::chrono::seconds(1));
    env.stop();

    SUCCEED();
}

TEST_CASE("Trade latency measurement") {
    auto env = create_test_env();
    env.start();

    auto sell = make_order(1, 1, Side::Sell, 100.0, 10);
    auto buy = make_order(2, 2, Side::Buy, 100.0, 10);

    REQUIRE(env.thread().submit_order(sell));
    REQUIRE(env.thread().submit_order(buy));

    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    env.stop();

    const auto& trades = env.engine().trades();
    REQUIRE(trades.size() == 1);
    REQUIRE(trades[0].total_latency_ns > 0);
    REQUIRE(trades[0].queue_latency_ns > 0);
    REQUIRE(trades[0].match_duration_ns > 0);
}

TEST_CASE("Latency benchmark") {
    auto env = create_test_env();
    env.start();

    constexpr int N = 6000000;

    auto start = std::chrono::steady_clock::now();

    for (int i = 0; i < N; ++i) {
        uint64_t acc_id_1 = (i % 100) + 1;
        uint64_t acc_id_2 = (i % 100) + 101; 

        auto sell = make_order(static_cast<uint64_t>(i * 2 + 1), acc_id_1, Side::Sell, 100.0, 1);
        auto buy = make_order(static_cast<uint64_t>(i * 2 + 2), acc_id_2, Side::Buy, 100.0, 1);

        int retry = 0;
        while (!env.thread().submit_order(sell)) {
            if (++retry > 100000) { std::this_thread::yield(); retry = 0; }
        }
        retry = 0;
        while (!env.thread().submit_order(buy)) {
            if (++retry > 100000) { std::this_thread::yield(); retry = 0; }
        }
    }

    auto timeout = std::chrono::steady_clock::now() + std::chrono::seconds(60);

    while (env.engine().trade_count() < static_cast<size_t>(N)) {
        std::this_thread::yield();
        if (std::chrono::steady_clock::now() > timeout) {
            std::cerr << "Warning: Benchmark timeout! Processed trades: " << env.engine().trade_count() << "\n";
            break;
        }
    }

    auto end = std::chrono::steady_clock::now();
    env.stop();

    double seconds = std::chrono::duration<double>(end - start).count();
    double throughput = (2.0 * N) / seconds;

    const auto& collector = env.engine().latency_collector();

    std::cout << "\n";
    std::cout << "Throughput: " << throughput << " orders/sec\n";
    std::cout << "P50 total latency: " << collector.p50_total() << " ns\n";
    std::cout << "P99 total latency: " << collector.p99_total() << " ns\n";
    std::cout << "Max total latency: " << collector.max_total() << " ns\n";
    std::cout << "P50 queue latency: " << collector.p50_queue() << " ns\n";
    std::cout << "P99 queue latency: " << collector.p99_queue() << " ns\n";
    std::cout << "Max queue latency: " << collector.max_queue() << " ns\n";
    std::cout << "P50 match duration: " << collector.p50_match() << " ns\n";
    std::cout << "P99 match duration: " << collector.p99_match() << " ns\n";
    std::cout << "Max match duration: " << collector.max_match() << " ns\n";

    REQUIRE(collector.size() == N);
}

TEST_CASE("Batch processing benchmark") {
    auto env = create_test_env();
    env.start();

    constexpr int N = 5000000;

    auto start = std::chrono::steady_clock::now();

    std::thread producer([&]() {
        for (int i = 0; i < N; ++i) {
            uint64_t acc_id_1 = (i % 100) + 1;
            uint64_t acc_id_2 = (i % 100) + 101; 

            auto sell = make_order(static_cast<uint64_t>(i * 2 + 1), acc_id_1, Side::Sell, 100.0, 1);
            auto buy = make_order(static_cast<uint64_t>(i * 2 + 2), acc_id_2, Side::Buy, 100.0, 1);

            int retry = 0;
            while (!env.thread().submit_order(sell)) {
                if (++retry > 100000) { std::this_thread::yield(); retry = 0; }
            }
            retry = 0;
            while (!env.thread().submit_order(buy)) {
                if (++retry > 100000) { std::this_thread::yield(); retry = 0; }
            }
        }
    });

    producer.join();

    auto timeout = std::chrono::steady_clock::now() + std::chrono::seconds(10);

    while (env.engine().trade_count() < static_cast<size_t>(N)) {
        std::this_thread::yield();
        if (std::chrono::steady_clock::now() > timeout) {
            std::cerr << "Warning: Benchmark timeout! Processed trades: " << env.engine().trade_count() << "\n";
            break;
        }

    }

    auto end = std::chrono::steady_clock::now();
    env.stop();

    double seconds = std::chrono::duration<double>(end - start).count();
    double throughput = (2.0 * N) / seconds;

    std::cout << "\nBatch Throughput: " << throughput << " orders/sec\n";

    SUCCEED();
}

TEST_CASE("Cancel existing order") {
    auto env = create_test_env();
    env.start();
    
    auto buy = make_order(1, 1, Side::Buy, 100.0, 10);
    env.thread().submit_order(buy);
    
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    
    REQUIRE(env.engine().order_lookup().size() == 1);
    REQUIRE(env.engine().cancel_order(1, 1, exchange::CancelReason::UserRequest));
    REQUIRE_FALSE(env.engine().cancel_order(1, 1, exchange::CancelReason::UserRequest));
    
    env.stop();
}

TEST_CASE("Cancel removes order from book") {
    auto env = create_test_env();
    env.start();
    
    auto buy = make_order(1, 1, Side::Buy, 100.0, 10);
    env.thread().submit_order(buy);
    
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    
    REQUIRE(env.engine().cancel_order(1, 1, exchange::CancelReason::UserRequest));
    REQUIRE(env.engine().bids().empty());
    
    env.stop();
}

TEST_CASE("GTD order expires automatically") {
    auto env = create_test_env();
    env.start();
    
    auto buy = make_order(1, 1, Side::Buy, 100.0, 10, Type::GTD);
    uint64_t expire_time = now_absolute_ns() + 1'000'000'000ULL; 
    buy.expire_time = expire_time; 
    
    env.thread().submit_order(buy);
    
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    REQUIRE(env.engine().bids().size() == 1);
    REQUIRE(env.engine().order_lookup().size() == 1);
    REQUIRE(env.scheduler().expiry_count() == 1);
    REQUIRE(env.scheduler().contains(1));
    REQUIRE(env.scheduler().seconds_entry_count() == 1);
    REQUIRE(env.scheduler().days_entry_count() == 0);
    REQUIRE(env.scheduler().validate());
    
    env.engine().process_expiry(expire_time + 1'000'000'000ULL); 
    
    REQUIRE(env.engine().bids().empty());
    REQUIRE(env.engine().order_lookup().empty());
    REQUIRE(env.scheduler().expiry_count() == 0);
    REQUIRE_FALSE(env.scheduler().contains(1));
    REQUIRE(env.scheduler().seconds_entry_count() == 0);
    REQUIRE(env.scheduler().days_entry_count() == 0);
    REQUIRE(env.scheduler().validate());
    
    env.stop();
}

TEST_CASE("DAY order expires at market close") {
    auto env = create_test_env();
    env.start();
    
    auto buy = make_order(1, 1, Side::Buy, 100.0, 10, Type::DAY);
    uint64_t expire_time = now_absolute_ns() + 2'000'000'000ULL; 
    buy.expire_time = expire_time;
    
    env.thread().submit_order(buy);
    
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    REQUIRE(env.engine().bids().size() == 1);
    REQUIRE(env.engine().order_lookup().size() == 1);
    REQUIRE(env.scheduler().expiry_count() == 1);
    REQUIRE(env.scheduler().contains(1));
    REQUIRE(env.scheduler().seconds_entry_count() == 1);
    REQUIRE(env.scheduler().days_entry_count() == 0);
    REQUIRE(env.scheduler().validate());
    
    env.engine().process_expiry(expire_time + 2'000'000'000ULL); 
    
    REQUIRE(env.engine().bids().empty());
    REQUIRE(env.engine().order_lookup().empty());    
    REQUIRE(env.scheduler().expiry_count() == 0);
    REQUIRE_FALSE(env.scheduler().contains(1));
    REQUIRE(env.scheduler().seconds_entry_count() == 0);
    REQUIRE(env.scheduler().days_entry_count() == 0);
    REQUIRE(env.scheduler().validate());
    
    env.stop();
}

TEST_CASE("GTC order never expires") {
    auto env = create_test_env();
    env.start();
    
    auto buy = make_order(1, 1, Side::Buy, 100.0, 10, Type::GTC);
    
    env.thread().submit_order(buy);
    
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    REQUIRE(env.engine().bids().size() == 1);
    REQUIRE(env.engine().order_lookup().size() == 1);    
    REQUIRE(env.scheduler().expiry_count() == 0);
    REQUIRE_FALSE(env.scheduler().contains(1));
    REQUIRE(env.scheduler().seconds_entry_count() == 0);
    REQUIRE(env.scheduler().days_entry_count() == 0);
    REQUIRE(env.scheduler().validate());
        
    REQUIRE(env.engine().bids().size() == 1);
    REQUIRE(env.engine().order_lookup().size() == 1);
    
    
    env.stop();
}

TEST_CASE("ExpiryScheduler handles system pause") {
    auto env = create_test_env();
    env.start();
    
    auto buy = make_order(1, 1, Side::Buy, 100.0, 10, Type::GTD);
    uint64_t expire_time = now_absolute_ns() + 3'000'000'000ULL; 
    buy.expire_time = expire_time;
    
    env.thread().submit_order(buy);
    
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    REQUIRE(env.engine().bids().size() == 1);
    REQUIRE(env.engine().order_lookup().size() == 1);
    REQUIRE(env.scheduler().expiry_count() == 1);
    REQUIRE(env.scheduler().contains(1));
    REQUIRE(env.scheduler().seconds_entry_count() == 1);
    REQUIRE(env.scheduler().days_entry_count() == 0);
    REQUIRE(env.scheduler().validate());
    
    std::cout << "\n=== System paused for 5 seconds ===" << std::endl;
    env.engine().process_expiry(expire_time + 2'000'000'000ULL); 
    std::cout << "=== System resumed ===" << std::endl;
        
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    env.stop();
    
    REQUIRE(env.engine().bids().empty());
    REQUIRE(env.engine().order_lookup().empty());
    REQUIRE(env.scheduler().expiry_count() == 0);
    REQUIRE_FALSE(env.scheduler().contains(1));
    REQUIRE(env.scheduler().seconds_entry_count() == 0);
    REQUIRE(env.scheduler().days_entry_count() == 0);
    REQUIRE(env.scheduler().validate());
}

TEST_CASE("ExpiryScheduler handles multiple orders with different expiry times") {
    auto env = create_test_env();
    env.start();
    
    auto buy1 = make_order(1, 1, Side::Buy, 100.0, 10, Type::GTD);
    uint64_t expire_time = now_absolute_ns() + 2'000'000'000ULL;
    buy1.expire_time = expire_time;
    
    auto buy2 = make_order(2, 2, Side::Buy, 101.0, 20, Type::GTD);
    buy2.expire_time = expire_time + 3'000'000'000ULL;
    
    auto buy3 = make_order(3, 3, Side::Buy, 102.0, 30, Type::GTC);
    buy3.expire_time = 0;
    
    env.thread().submit_order(buy1);
    env.thread().submit_order(buy2);
    env.thread().submit_order(buy3);
    
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    REQUIRE(env.engine().bids().size() == 3);
    REQUIRE(env.engine().order_lookup().size() == 3);    
    REQUIRE(env.scheduler().expiry_count() == 2);
    REQUIRE(env.scheduler().contains(1));
    REQUIRE(env.scheduler().contains(2));
    REQUIRE(env.scheduler().seconds_entry_count() == 2);
    REQUIRE(env.scheduler().days_entry_count() == 0);
    REQUIRE(env.scheduler().validate());
    
    env.engine().process_expiry(expire_time + 1'000'000'000ULL);     
    
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    REQUIRE(env.engine().bids().size() == 2);
    REQUIRE(env.engine().order_lookup().size() == 2);    
    REQUIRE(env.scheduler().expiry_count() == 1);
    REQUIRE_FALSE(env.scheduler().contains(1));
    REQUIRE(env.scheduler().contains(2));
    REQUIRE(env.scheduler().seconds_entry_count() == 1);
    REQUIRE(env.scheduler().days_entry_count() == 0);
    REQUIRE(env.scheduler().validate());
    
    bool has_order2 = false;
    bool has_order3 = false;
    for (const auto& [id, order] : env.engine().order_lookup()) {
        if (id == 2) has_order2 = true;
        if (id == 3) has_order3 = true;
    }
    REQUIRE(has_order2 == true);
    REQUIRE(has_order3 == true);
    
    env.engine().process_expiry(expire_time + 4'000'000'000ULL); 
    
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    env.stop();
    
    REQUIRE(env.engine().bids().size() == 1);
    REQUIRE(env.engine().order_lookup().size() == 1);
    REQUIRE(env.engine().order_lookup().find(3) != env.engine().order_lookup().end());    
    REQUIRE(env.scheduler().expiry_count() == 0);
    REQUIRE_FALSE(env.scheduler().contains(2));
    REQUIRE(env.scheduler().seconds_entry_count() == 0);
    REQUIRE(env.scheduler().days_entry_count() == 0);
    REQUIRE(env.scheduler().validate());
}

TEST_CASE("ExpiryScheduler handles orders expiring during system pause") {
    auto env = create_test_env();
    env.start();
    
    std::vector<std::pair<uint64_t, uint64_t>> orders = {
        {1, 1'000'000'000ULL}, 
        {2, 2'000'000'000ULL}, 
        {3, 3'000'000'000ULL},   
        {4, 4'000'000'000ULL},  
        {5, 5'000'000'000ULL}   
    };
    
    for (const auto& [id, delay] : orders) {
        auto order = make_order(id, id, Side::Buy, 100.0 + id, 10, Type::GTD);
        order.expire_time = now_absolute_ns() + delay;
        env.thread().submit_order(order);
    }
    
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    REQUIRE(env.engine().bids().size() == 5);
    REQUIRE(env.engine().order_lookup().size() == 5);
    REQUIRE(env.scheduler().expiry_count() == 5);
    REQUIRE(env.scheduler().contains(5));
    REQUIRE(env.scheduler().seconds_entry_count() == 5);
    REQUIRE(env.scheduler().days_entry_count() == 0);
    REQUIRE(env.scheduler().validate());
    
    std::cout << "\n=== System paused for 6 seconds ===" << std::endl;
    env.engine().process_expiry(now_absolute_ns() + 6'000'000'000ULL);
    std::cout << "=== System resumed ===" << std::endl;    
    
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    env.stop();
    
    REQUIRE(env.engine().bids().empty());
    REQUIRE(env.engine().order_lookup().empty());
    REQUIRE(env.scheduler().expiry_count() == 0);
    REQUIRE_FALSE(env.scheduler().contains(5));
    REQUIRE(env.scheduler().seconds_entry_count() == 0);
    REQUIRE(env.scheduler().days_entry_count() == 0);
    REQUIRE(env.scheduler().validate());
}

TEST_CASE("ExpiryScheduler days_wheel promotion after system pause") {
    auto env = create_test_env();
    env.start();
    
    auto buy = make_order(1, 1, Side::Buy, 100.0, 10, Type::GTD);
    buy.expire_time = now_absolute_ns() + 25 * 3600'000'000'000ULL; 
    
    env.thread().submit_order(buy);
    
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    REQUIRE(env.engine().bids().size() == 1);
    REQUIRE(env.engine().order_lookup().size() == 1);    
    REQUIRE(env.scheduler().expiry_count() == 1);
    REQUIRE(env.scheduler().contains(1));
    REQUIRE(env.scheduler().seconds_entry_count() == 0);
    REQUIRE(env.scheduler().days_entry_count() == 1);
    REQUIRE(env.scheduler().validate());
    
    std::this_thread::sleep_for(std::chrono::seconds(2));
    
    REQUIRE(env.engine().bids().size() == 1);
    REQUIRE(env.engine().order_lookup().size() == 1);    
    REQUIRE(env.scheduler().expiry_count() == 1);
    REQUIRE(env.scheduler().contains(1));
    REQUIRE(env.scheduler().seconds_entry_count() == 0);
    REQUIRE(env.scheduler().days_entry_count() == 1);
    REQUIRE(env.scheduler().validate());

    uint64_t future_time = now_absolute_ns() + 26 * 3600'000'000'000ULL;
    env.engine().process_expiry(future_time);
    
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    env.stop();
    
    REQUIRE(env.engine().bids().empty());
    REQUIRE(env.engine().order_lookup().empty());
    REQUIRE(env.scheduler().expiry_count() == 0);
    REQUIRE_FALSE(env.scheduler().contains(1));
    REQUIRE(env.scheduler().seconds_entry_count() == 0);
    REQUIRE(env.scheduler().days_entry_count() == 0);
    REQUIRE(env.scheduler().validate());
}