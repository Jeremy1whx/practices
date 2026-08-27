#define CATCH_CONFIG_MAIN
#define CATCH_CONFIG_ENABLE_BENCHMARKING 

#include "Writer.h"
#include "../Event_Bus/EventBus.h"
#include "../Event_Bus/EventPublisher.h"
#include "../Lock_Free_Ring_Buffer/mpsc/catch.hpp"
#include "SnapshotThread.h"

void cleanup_snapshot_files() {
    for (const auto& entry : std::filesystem::directory_iterator(".")) {
        if (entry.path().extension() == ".txt" && 
            entry.path().stem().string().find("snapshot") == 0) {
            std::filesystem::remove(entry.path());
        }
    }
}

size_t count_lines(const std::string& filename) {
    std::ifstream file(filename);
    std::string line;
    size_t count = 0;
    while (std::getline(file, line)) {
        ++count;
    }
    return count;
}

std::string read_file_content(const std::string& filename) {
    std::ifstream file(filename);
    std::stringstream ss;
    ss << file.rdbuf();
    return ss.str();
}

TEST_CASE("Persistence receives events") {
    exchange::EventBus bus;

    exchange::TextWriter prs_srv("test.log");

    bus.subscribe(&prs_srv);

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

    std::ifstream file("test.log");
    std::string content;
    std::getline(file, content);

    REQUIRE(file.is_open());
    REQUIRE(content.find("Trade") != std::string::npos);
    file.close();

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

    file.open("test.log");
    std::stringstream ss;
    ss << file.rdbuf();
    content = ss.str();
    REQUIRE(content.find("Trade") != std::string::npos);
    REQUIRE(content.find("BookUpdate") != std::string::npos);
    file.close();
}

TEST_CASE("SnapshotService Basic Operations") {
    exchange::EventBus bus;
    exchange::EventPublisher publisher(bus);
    exchange::ExpiryScheduler scheduler;
    exchange::MatchingEngine engine(publisher, scheduler);
    exchange::SnapshotService service(engine);

    Order order1;
    order1.order_id = 1;
    order1.side = Side::Buy;
    order1.price = 100.0;
    order1.quantity = 10;
    order1.type = Type::GTC;
    order1.sequence = 1;
    order1.expire_time = 0;

    Order order2;
    order2.order_id = 2;
    order2.side = Side::Sell;
    order2.price = 101.0;
    order2.quantity = 20;
    order2.type = Type::DAY;
    order2.sequence = 2;
    order2.expire_time = 1234567890;

    engine.process_order(order1);
    engine.process_order(order2);

    uint64_t sequence = 1;
    uint64_t timestamp = 1234567890123ULL;
    service.append_snapshot(sequence, timestamp);

    auto& batch = service.batch(sequence);
    REQUIRE(batch.ready.load() == true);
    REQUIRE(batch.batch.sequence == sequence);
    REQUIRE(batch.batch.snapshot_time == timestamp);
    REQUIRE(batch.batch.orders.size() == 2);

    REQUIRE(batch.batch.orders[0].order_id == 1);
    REQUIRE(batch.batch.orders[0].side == Side::Buy);
    REQUIRE(batch.batch.orders[0].price == 100.0);
    REQUIRE(batch.batch.orders[0].quantity == 10);
    REQUIRE(batch.batch.orders[0].type == Type::GTC);
    REQUIRE(batch.batch.orders[0].expire_time == 0);

    REQUIRE(batch.batch.orders[1].order_id == 2);
    REQUIRE(batch.batch.orders[1].side == Side::Sell);
    REQUIRE(batch.batch.orders[1].price == 101.0);
    REQUIRE(batch.batch.orders[1].quantity == 20);
    REQUIRE(batch.batch.orders[1].type == Type::DAY);
    REQUIRE(batch.batch.orders[1].expire_time == 1234567890);

    service.reset_batch(sequence);
    REQUIRE(batch.ready.load() == false);
    REQUIRE(batch.batch.sequence == 0);
    REQUIRE(batch.batch.snapshot_time == 0);
    REQUIRE(batch.batch.orders.empty());

    cleanup_snapshot_files();
}

TEST_CASE("SnapshotService Batch Selection") {
    exchange::EventBus bus;
    exchange::EventPublisher publisher(bus);
    exchange::ExpiryScheduler scheduler;
    exchange::MatchingEngine engine(publisher, scheduler);
    exchange::SnapshotService service(engine);

    Order order;
    order.order_id = 1;
    order.side = Side::Buy;
    order.price = 100.0;
    order.quantity = 10;
    order.type = Type::GTC;
    order.sequence = 1;
    order.expire_time = 0;
    engine.process_order(order);

    service.append_snapshot(2, 1000);
    auto& batch_even = service.batch(2).batch;
    REQUIRE(batch_even.orders.size() == 1);

    service.append_snapshot(3, 2000);
    auto& batch_odd = service.batch(3).batch;
    REQUIRE(batch_odd.orders.size() == 1);

    service.reset_batch(2);
    REQUIRE(batch_even.orders.empty());
    REQUIRE(batch_odd.orders.size() == 1);

    cleanup_snapshot_files();
}

TEST_CASE("SnapshotService Large Order Book") {
    exchange::EventBus bus;
    exchange::EventPublisher publisher(bus);
    exchange::ExpiryScheduler scheduler;
    exchange::MatchingEngine engine(publisher, scheduler);
    exchange::SnapshotService service(engine);

    const size_t NUM_ORDERS = 10000;
    
    for (size_t i = 0; i < NUM_ORDERS / 2; ++i) {
        Order order;
        order.order_id = static_cast<uint64_t>(i + 1);
        order.side = Side::Buy;
        order.price = 100.0 - (i + 1) * 0.01;
        order.quantity = static_cast<uint32_t>(10 + i % 100);
        order.type = Type::GTC;
        order.sequence = i + 1;
        order.expire_time = 0;
        engine.process_order(order);
    }

    for (size_t i = 0; i < NUM_ORDERS / 2; ++i) {
        Order order;
        order.order_id = static_cast<uint64_t>(i + NUM_ORDERS / 2 + 1);
        order.side = Side::Sell;
        order.price = 100.0 + (i + 1) * 0.01;
        order.quantity = static_cast<uint32_t>(10 + i % 100);
        order.type = Type::GTC;
        order.sequence = i + NUM_ORDERS / 2 + 1;
        order.expire_time = 0;
        engine.process_order(order);
    }

    uint64_t sequence = 1;
    uint64_t timestamp = now_absolute_ns();
    service.append_snapshot(sequence, timestamp);

    auto& batch = service.batch(sequence).batch;

    REQUIRE(batch.orders.size() == NUM_ORDERS);
    REQUIRE(batch.sequence == sequence);
    REQUIRE(batch.snapshot_time == timestamp);

    REQUIRE(batch.orders[0].order_id == 1);
    REQUIRE(batch.orders[NUM_ORDERS - 1].order_id == NUM_ORDERS);

    cleanup_snapshot_files();
}

TEST_CASE("SnapshotTextWriter Basic Write") {
    exchange::EventBus bus;
    exchange::EventPublisher publisher(bus);
    exchange::ExpiryScheduler scheduler;
    exchange::MatchingEngine engine(publisher, scheduler);
    exchange::SnapshotService service(engine);
    exchange::SnapshotTextWriter writer;

    Order order1;
    order1.order_id = 1;
    order1.side = Side::Buy;
    order1.price = 100.0;
    order1.quantity = 10;
    order1.type = Type::GTC;
    order1.sequence = 1;
    order1.expire_time = 0;

    Order order2;
    order2.order_id = 2;
    order2.side = Side::Sell;
    order2.price = 101.5;
    order2.quantity = 20;
    order2.type = Type::DAY;
    order2.sequence = 2;
    order2.expire_time = 1234567890;

    engine.process_order(order1);
    engine.process_order(order2);

    uint64_t sequence = 100;
    uint64_t timestamp = 1234567890123ULL;
    service.append_snapshot(sequence, timestamp);

    auto& batch = service.batch(sequence).batch;
    writer.process_snapshot(batch);

    std::string filename = "snapshot100.txt";
    REQUIRE(std::filesystem::exists(filename));

    std::string content = read_file_content(filename);
    REQUIRE(content.find("snapshot timestamp= 1234567890123") != std::string::npos);
    REQUIRE(content.find("different orders quantity= 2") != std::string::npos);
    REQUIRE(content.find("order_id= 1") != std::string::npos);
    REQUIRE(content.find("side= Buy") != std::string::npos);
    REQUIRE(content.find("price= 100") != std::string::npos);
    REQUIRE(content.find("quantity= 10") != std::string::npos);
    REQUIRE(content.find("sequence= 1") != std::string::npos);
    REQUIRE(content.find("type= GTC") != std::string::npos);
    REQUIRE(content.find("order_id= 2") != std::string::npos);
    REQUIRE(content.find("side= Sell") != std::string::npos);
    REQUIRE(content.find("price= 101.5") != std::string::npos);
    REQUIRE(content.find("quantity= 20") != std::string::npos);
    REQUIRE(content.find("sequence= 2") != std::string::npos);
    REQUIRE(content.find("type= DAY") != std::string::npos);
    REQUIRE(content.find("expire_time= 1234567890") != std::string::npos);

    std::filesystem::remove(filename);
    cleanup_snapshot_files();
}

TEST_CASE("SnapshotTextWriter Multiple Snapshots") {
    exchange::EventBus bus;
    exchange::EventPublisher publisher(bus);
    exchange::ExpiryScheduler scheduler;
    exchange::MatchingEngine engine(publisher, scheduler);
    exchange::SnapshotService service(engine);
    exchange::SnapshotTextWriter writer;

    const size_t ORDERS_PER_SNAPSHOT = 6;

    for (uint64_t seq = 1; seq <= 5; ++seq) {
        for (size_t i = 0; i < ORDERS_PER_SNAPSHOT / 2; ++i) {
            Order order;
            size_t order_index = (seq - 1) * ORDERS_PER_SNAPSHOT + i;
            order.order_id = static_cast<uint64_t>(order_index + 1);
            order.side = Side::Buy;
            order.price = 100.0 - (order_index + 1) * 0.1;
            order.quantity = static_cast<uint32_t>(10 + order_index);
            order.type = Type::GTC;
            order.sequence = order_index + 1;
            order.expire_time = 0;
            engine.process_order(order);
        }

        for (size_t i = 0; i < ORDERS_PER_SNAPSHOT / 2; ++i) {
            Order order;
            size_t order_index = (seq - 1) * ORDERS_PER_SNAPSHOT + ORDERS_PER_SNAPSHOT / 2 + i;
            order.order_id = static_cast<uint64_t>(order_index + 1);
            order.side = Side::Sell;
            order.price = 100.0 + (order_index + 1) * 0.1;
            order.quantity = static_cast<uint32_t>(10 + order_index);
            order.type = Type::GTC;
            order.sequence = order_index + 1;
            order.expire_time = 0;
            engine.process_order(order);
        }

        uint64_t timestamp = now_absolute_ns();
        service.append_snapshot(seq, timestamp);
        auto& batch = service.batch(seq).batch;
        writer.process_snapshot(batch);

        std::string filename = "snapshot" + std::to_string(seq) + ".txt";
        REQUIRE(std::filesystem::exists(filename));

        size_t expected_orders = seq * ORDERS_PER_SNAPSHOT;
        REQUIRE(count_lines(filename) == expected_orders + 2);

        std::string content = read_file_content(filename);
        for (size_t i = 0; i < expected_orders; ++i) {
            std::string order_id = "order_id= " + std::to_string(i + 1);
            REQUIRE(content.find(order_id) != std::string::npos);
        }

        REQUIRE(content.find("snapshot timestamp= " + std::to_string(timestamp)) != std::string::npos);
        REQUIRE(content.find("different orders quantity= " + std::to_string(expected_orders)) != std::string::npos);

        std::filesystem::remove(filename);
        service.reset_batch(seq);
    }

    cleanup_snapshot_files();
}

TEST_CASE("SnapshotThread Integration") {
    exchange::EventBus bus;
    exchange::EventPublisher publisher(bus);
    exchange::ExpiryScheduler scheduler;
    exchange::MatchingEngine engine(publisher, scheduler);
    exchange::SnapshotService service(engine);
    exchange::SnapshotTextWriter writer;
    exchange::MatchingEngineThread engine_thread(engine, service);
    exchange::SnapshotThread snapshot_thread(engine_thread, service, writer);

    Order order1;
    order1.order_id = 1;
    order1.side = Side::Buy;
    order1.price = 100.0;
    order1.quantity = 10;
    order1.type = Type::GTC;
    order1.sequence = 1;
    order1.expire_time = 0;

    Order order2;
    order2.order_id = 2;
    order2.side = Side::Sell;
    order2.price = 101.0;
    order2.quantity = 20;
    order2.type = Type::DAY;
    order2.sequence = 2;
    order2.expire_time = 1234567890;

    engine.process_order(order1);
    engine.process_order(order2);

    engine_thread.start();
    snapshot_thread.start();

    engine_thread.snapshot_requested();

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    std::string filename = "snapshot0.txt"; 
    bool file_exists = std::filesystem::exists(filename);

    snapshot_thread.stop();
    engine_thread.stop();

    if (std::filesystem::exists(filename)) {
        std::filesystem::remove(filename);
    }
    cleanup_snapshot_files();
}

TEST_CASE("SnapshotThread Lifecycle") {
    exchange::EventBus bus;
    exchange::EventPublisher publisher(bus);
    exchange::ExpiryScheduler scheduler;
    exchange::MatchingEngine engine(publisher, scheduler);
    exchange::SnapshotService service(engine);
    exchange::SnapshotTextWriter writer;
    exchange::MatchingEngineThread engine_thread(engine, service);
    exchange::SnapshotThread snapshot_thread(engine_thread, service, writer);

    REQUIRE_NOTHROW(snapshot_thread.start());
    REQUIRE_NOTHROW(snapshot_thread.stop());

    REQUIRE_NOTHROW(snapshot_thread.start());

    REQUIRE_NOTHROW(snapshot_thread.stop());
    REQUIRE_NOTHROW(snapshot_thread.stop());

    cleanup_snapshot_files();
}

TEST_CASE("Snapshot with Large Order Book") {
    exchange::EventBus bus;
    exchange::EventPublisher publisher(bus);
    exchange::ExpiryScheduler scheduler;
    exchange::MatchingEngine engine(publisher, scheduler);
    exchange::SnapshotService service(engine);
    exchange::SnapshotTextWriter writer;

    const size_t NUM_ORDERS = 50000;
    const uint64_t SEQUENCE = 999;

    for (size_t i = 0; i < NUM_ORDERS / 2; ++i) {
        Order order;
        order.order_id = static_cast<uint64_t>(i + 1);
        order.side = Side::Buy;
        order.price = 1000.0 - (i + 1) * 0.01;
        order.quantity = static_cast<uint32_t>(1 + (i % 100));
        order.type = (i % 3 == 0) ? Type::GTC : Type::DAY;
        order.sequence = i + 1;
        order.expire_time = (i % 2 == 0) ? 0 : 1234567890;
        engine.process_order(order);
    }

    for (size_t i = 0; i < NUM_ORDERS / 2; ++i) {
        Order order;
        order.order_id = static_cast<uint64_t>(i + NUM_ORDERS / 2 + 1);
        order.side = Side::Sell;
        order.price = 1000.0 + (i + 1) * 0.01;
        order.quantity = static_cast<uint32_t>(1 + (i % 100));
        order.type = (i % 3 == 0) ? Type::GTC : Type::DAY;
        order.sequence = i + NUM_ORDERS / 2 + 1;
        order.expire_time = (i % 2 == 0) ? 0 : 1234567890;
        engine.process_order(order);
    }

    uint64_t timestamp = now_absolute_ns();
    service.append_snapshot(SEQUENCE, timestamp);
    auto& batch = service.batch(SEQUENCE).batch;
    writer.process_snapshot(batch);

    std::string filename = "snapshot" + std::to_string(SEQUENCE) + ".txt";
    REQUIRE(std::filesystem::exists(filename));

    auto file_size = std::filesystem::file_size(filename);
    REQUIRE(file_size > 0);

    std::string content = read_file_content(filename);
    REQUIRE(content.find("snapshot timestamp= " + std::to_string(timestamp)) != std::string::npos);
    

    size_t lines = count_lines(filename);
    REQUIRE(lines == NUM_ORDERS + 2);

    REQUIRE(content.find("order_id= 1") != std::string::npos);
    REQUIRE(content.find("order_id= " + std::to_string(NUM_ORDERS)) != std::string::npos);

    std::filesystem::remove(filename);
    cleanup_snapshot_files();
}

TEST_CASE("Snapshot Service Memory Management") {
    exchange::EventBus bus;
    exchange::EventPublisher publisher(bus);
    exchange::ExpiryScheduler scheduler;
    exchange::MatchingEngine engine(publisher, scheduler);
    exchange::SnapshotService service(engine);

    const size_t NUM_ORDERS = 10000;
    const uint64_t SEQUENCE_EVEN = 2;
    const uint64_t SEQUENCE_ODD = 3;

    for (size_t i = 0; i < NUM_ORDERS; ++i) {
        Order order;
        order.order_id = static_cast<uint64_t>(i + 1);
        order.side = Side::Buy;
        order.price = 100.0 + i * 0.01;
        order.quantity = static_cast<uint32_t>(10);
        order.type = Type::GTC;
        order.sequence = i + 1;
        order.expire_time = 0;
        engine.process_order(order);
    }

    service.append_snapshot(SEQUENCE_EVEN, 1000);
    auto& batch_a = service.batch(SEQUENCE_EVEN).batch;
    REQUIRE(batch_a.orders.size() == NUM_ORDERS);

    service.reset_batch(SEQUENCE_EVEN);
    REQUIRE(batch_a.orders.empty());

    service.append_snapshot(SEQUENCE_ODD, 2000);
    auto& batch_b = service.batch(SEQUENCE_ODD).batch;
    REQUIRE(batch_b.orders.size() == NUM_ORDERS);

    REQUIRE(batch_a.orders.empty());

    service.reset_batch(SEQUENCE_ODD);
    REQUIRE(batch_b.orders.empty());

    cleanup_snapshot_files();
}