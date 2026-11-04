#define CATCH_CONFIG_MAIN
#define CATCH_CONFIG_ENABLE_BENCHMARKING 

#include "EventWriter.h"
#include "../Event_Bus/EventBus.h"
#include "../Event_Bus/EventPublisher.h"
#include "../Lock_Free_Ring_Buffer/mpsc/catch.hpp"
#include "PersistenceThread.h"

// void cleanup_snapshot_files() {
//     for (const auto& entry : fs::directory_iterator(".")) {
//         if (entry.path().extension() == ".txt" && 
//             entry.path().stem().string().find("snapshot") == 0) {
//             fs::remove(entry.path());
//         }
//     }
// }

// size_t count_lines(const std::string& filename) {
//     std::ifstream file(filename);
//     std::string line;
//     size_t count = 0;
//     while (std::getline(file, line)) {
//         ++count;
//     }
//     return count;
// }

// std::string read_file_content(const std::string& filename) {
//     std::ifstream file(filename);
//     std::stringstream ss;
//     ss << file.rdbuf();
//     return ss.str();
// }

// TEST_CASE("Persistence receives events") {
//     exchange::EventBus bus;

//     exchange::EventTextWriter prs_srv("test.log");

//     bus.subscribe(&prs_srv);

//     exchange::Trade trade{
//         .buy_order_id = 1,
//         .sell_order_id = 2,
//         .price = 100.0,
//         .quantity = 10
//     };

//     exchange::TradeEvent trade_event{
//         {exchange::EventType::Trade, 123},
//         trade
//     };

//     bus.publish(trade_event);

//     std::ifstream file("test.log");
//     std::string content;
//     std::getline(file, content);

//     REQUIRE(file.is_open());
//     REQUIRE(content.find("Trade") != std::string::npos);
//     file.close();

//     exchange::BookUpdate update{
//         .best_bid = 99.5,
//         .best_ask = 100.5,
//         .last_trade_price = 100.0,
//         .last_trade_quantity = 10
//     };

//     exchange::BookUpdateEvent book_event{
//         {exchange::EventType::BookUpdate, 124},
//         update
//     };

//     bus.publish(book_event);

//     file.open("test.log");
//     std::stringstream ss;
//     ss << file.rdbuf();
//     content = ss.str();
//     REQUIRE(content.find("Trade") != std::string::npos);
//     REQUIRE(content.find("BookUpdate") != std::string::npos);
//     file.close();
// }

// TEST_CASE("SnapshotService Basic Operations") {
//     exchange::EventBus bus;
//     exchange::EventPublisher publisher(bus);
//     exchange::ExpiryScheduler scheduler;
//     exchange::MatchingEngine engine(publisher, scheduler);
//     exchange::SnapshotService service(engine);

//     Order order1;
//     order1.order_id = 1;
//     order1.side = Side::Buy;
//     order1.price = 100.0;
//     order1.quantity = 10;
//     order1.type = Type::GTC;
//     order1.sequence = 1;
//     order1.expire_time = 0;

//     Order order2;
//     order2.order_id = 2;
//     order2.side = Side::Sell;
//     order2.price = 101.0;
//     order2.quantity = 20;
//     order2.type = Type::DAY;
//     order2.sequence = 2;
//     order2.expire_time = 1234567890;

//     engine.process_order(order1);
//     engine.process_order(order2);

//     uint64_t sequence = 1;
//     uint64_t timestamp = 1234567890123ULL;
//     service.append_snapshot(sequence, timestamp);

//     auto& batch = service.batch(sequence);
//     REQUIRE(batch.ready.load() == true);
//     REQUIRE(batch.batch.sequence == sequence);
//     REQUIRE(batch.batch.snapshot_time == timestamp);
//     REQUIRE(batch.batch.orders.size() == 2);

//     REQUIRE(batch.batch.orders[0].order_id == 1);
//     REQUIRE(batch.batch.orders[0].side == Side::Buy);
//     REQUIRE(batch.batch.orders[0].price == 100.0);
//     REQUIRE(batch.batch.orders[0].quantity == 10);
//     REQUIRE(batch.batch.orders[0].type == Type::GTC);
//     REQUIRE(batch.batch.orders[0].expire_time == 0);

//     REQUIRE(batch.batch.orders[1].order_id == 2);
//     REQUIRE(batch.batch.orders[1].side == Side::Sell);
//     REQUIRE(batch.batch.orders[1].price == 101.0);
//     REQUIRE(batch.batch.orders[1].quantity == 20);
//     REQUIRE(batch.batch.orders[1].type == Type::DAY);
//     REQUIRE(batch.batch.orders[1].expire_time == 1234567890);

//     service.reset_batch(sequence);
//     REQUIRE(batch.ready.load() == false);
//     REQUIRE(batch.batch.sequence == 0);
//     REQUIRE(batch.batch.snapshot_time == 0);
//     REQUIRE(batch.batch.orders.empty());

//     cleanup_snapshot_files();
// }

// TEST_CASE("SnapshotService Batch Selection") {
//     exchange::EventBus bus;
//     exchange::EventPublisher publisher(bus);
//     exchange::ExpiryScheduler scheduler;
//     exchange::MatchingEngine engine(publisher, scheduler);
//     exchange::SnapshotService service(engine);

//     Order order;
//     order.order_id = 1;
//     order.side = Side::Buy;
//     order.price = 100.0;
//     order.quantity = 10;
//     order.type = Type::GTC;
//     order.sequence = 1;
//     order.expire_time = 0;
//     engine.process_order(order);

//     service.append_snapshot(2, 1000);
//     auto& batch_even = service.batch(2).batch;
//     REQUIRE(batch_even.orders.size() == 1);

//     service.append_snapshot(3, 2000);
//     auto& batch_odd = service.batch(3).batch;
//     REQUIRE(batch_odd.orders.size() == 1);

//     service.reset_batch(2);
//     REQUIRE(batch_even.orders.empty());
//     REQUIRE(batch_odd.orders.size() == 1);

//     cleanup_snapshot_files();
// }

// TEST_CASE("SnapshotService Large Order Book") {
//     exchange::EventBus bus;
//     exchange::EventPublisher publisher(bus);
//     exchange::ExpiryScheduler scheduler;
//     exchange::MatchingEngine engine(publisher, scheduler);
//     exchange::SnapshotService service(engine);

//     const size_t NUM_ORDERS = 10000;
    
//     for (size_t i = 0; i < NUM_ORDERS / 2; ++i) {
//         Order order;
//         order.order_id = static_cast<uint64_t>(i + 1);
//         order.side = Side::Buy;
//         order.price = 100.0 - (i + 1) * 0.01;
//         order.quantity = static_cast<uint32_t>(10 + i % 100);
//         order.type = Type::GTC;
//         order.sequence = i + 1;
//         order.expire_time = 0;
//         engine.process_order(order);
//     }

//     for (size_t i = 0; i < NUM_ORDERS / 2; ++i) {
//         Order order;
//         order.order_id = static_cast<uint64_t>(i + NUM_ORDERS / 2 + 1);
//         order.side = Side::Sell;
//         order.price = 100.0 + (i + 1) * 0.01;
//         order.quantity = static_cast<uint32_t>(10 + i % 100);
//         order.type = Type::GTC;
//         order.sequence = i + NUM_ORDERS / 2 + 1;
//         order.expire_time = 0;
//         engine.process_order(order);
//     }

//     uint64_t sequence = 1;
//     uint64_t timestamp = now_absolute_ns();
//     service.append_snapshot(sequence, timestamp);

//     auto& batch = service.batch(sequence).batch;

//     REQUIRE(batch.orders.size() == NUM_ORDERS);
//     REQUIRE(batch.sequence == sequence);
//     REQUIRE(batch.snapshot_time == timestamp);

//     REQUIRE(batch.orders[0].order_id == 1);
//     REQUIRE(batch.orders[NUM_ORDERS - 1].order_id == NUM_ORDERS);

//     cleanup_snapshot_files();
// }

// TEST_CASE("SnapshotTextWriter Basic Write") {
//     exchange::EventBus bus;
//     exchange::EventPublisher publisher(bus);
//     exchange::ExpiryScheduler scheduler;
//     exchange::MatchingEngine engine(publisher, scheduler);
//     exchange::SnapshotService service(engine);
//     exchange::SnapshotTextWriter writer;

//     Order order1;
//     order1.order_id = 1;
//     order1.side = Side::Buy;
//     order1.price = 100.0;
//     order1.quantity = 10;
//     order1.type = Type::GTC;
//     order1.sequence = 1;
//     order1.expire_time = 0;

//     Order order2;
//     order2.order_id = 2;
//     order2.side = Side::Sell;
//     order2.price = 101.5;
//     order2.quantity = 20;
//     order2.type = Type::DAY;
//     order2.sequence = 2;
//     order2.expire_time = 1234567890;

//     engine.process_order(order1);
//     engine.process_order(order2);

//     uint64_t sequence = 100;
//     uint64_t timestamp = 1234567890123ULL;
//     service.append_snapshot(sequence, timestamp);

//     auto& batch = service.batch(sequence).batch;
//     writer.process_snapshot(batch);

//     std::string filename = "snapshot100.txt";
//     REQUIRE(fs::exists(filename));

//     std::string content = read_file_content(filename);
//     REQUIRE(content.find("snapshot timestamp= 1234567890123") != std::string::npos);
//     REQUIRE(content.find("different orders quantity= 2") != std::string::npos);
//     REQUIRE(content.find("order_id= 1") != std::string::npos);
//     REQUIRE(content.find("side= Buy") != std::string::npos);
//     REQUIRE(content.find("price= 100") != std::string::npos);
//     REQUIRE(content.find("quantity= 10") != std::string::npos);
//     REQUIRE(content.find("sequence= 1") != std::string::npos);
//     REQUIRE(content.find("type= GTC") != std::string::npos);
//     REQUIRE(content.find("order_id= 2") != std::string::npos);
//     REQUIRE(content.find("side= Sell") != std::string::npos);
//     REQUIRE(content.find("price= 101.5") != std::string::npos);
//     REQUIRE(content.find("quantity= 20") != std::string::npos);
//     REQUIRE(content.find("sequence= 2") != std::string::npos);
//     REQUIRE(content.find("type= DAY") != std::string::npos);
//     REQUIRE(content.find("expire_time= 1234567890") != std::string::npos);

//     fs::remove(filename);
//     cleanup_snapshot_files();
// }

// TEST_CASE("SnapshotTextWriter Multiple Snapshots") {
//     exchange::EventBus bus;
//     exchange::EventPublisher publisher(bus);
//     exchange::ExpiryScheduler scheduler;
//     exchange::MatchingEngine engine(publisher, scheduler);
//     exchange::SnapshotService service(engine);
//     exchange::SnapshotTextWriter writer;

//     const size_t ORDERS_PER_SNAPSHOT = 6;

//     for (uint64_t seq = 1; seq <= 5; ++seq) {
//         for (size_t i = 0; i < ORDERS_PER_SNAPSHOT / 2; ++i) {
//             Order order;
//             size_t order_index = (seq - 1) * ORDERS_PER_SNAPSHOT + i;
//             order.order_id = static_cast<uint64_t>(order_index + 1);
//             order.side = Side::Buy;
//             order.price = 100.0 - (order_index + 1) * 0.1;
//             order.quantity = static_cast<uint32_t>(10 + order_index);
//             order.type = Type::GTC;
//             order.sequence = order_index + 1;
//             order.expire_time = 0;
//             engine.process_order(order);
//         }

//         for (size_t i = 0; i < ORDERS_PER_SNAPSHOT / 2; ++i) {
//             Order order;
//             size_t order_index = (seq - 1) * ORDERS_PER_SNAPSHOT + ORDERS_PER_SNAPSHOT / 2 + i;
//             order.order_id = static_cast<uint64_t>(order_index + 1);
//             order.side = Side::Sell;
//             order.price = 100.0 + (order_index + 1) * 0.1;
//             order.quantity = static_cast<uint32_t>(10 + order_index);
//             order.type = Type::GTC;
//             order.sequence = order_index + 1;
//             order.expire_time = 0;
//             engine.process_order(order);
//         }

//         uint64_t timestamp = now_absolute_ns();
//         service.append_snapshot(seq, timestamp);
//         auto& batch = service.batch(seq).batch;
//         writer.process_snapshot(batch);

//         std::string filename = "snapshot" + std::to_string(seq) + ".txt";
//         REQUIRE(fs::exists(filename));

//         size_t expected_orders = seq * ORDERS_PER_SNAPSHOT;
//         REQUIRE(count_lines(filename) == expected_orders + 2);

//         std::string content = read_file_content(filename);
//         for (size_t i = 0; i < expected_orders; ++i) {
//             std::string order_id = "order_id= " + std::to_string(i + 1);
//             REQUIRE(content.find(order_id) != std::string::npos);
//         }

//         REQUIRE(content.find("snapshot timestamp= " + std::to_string(timestamp)) != std::string::npos);
//         REQUIRE(content.find("different orders quantity= " + std::to_string(expected_orders)) != std::string::npos);

//         fs::remove(filename);
//         service.reset_batch(seq);
//     }

//     cleanup_snapshot_files();
// }

// TEST_CASE("PersistenceThread Integration") {
//     exchange::EventBus bus;
//     exchange::EventPublisher publisher(bus);
//     exchange::ExpiryScheduler scheduler;
//     exchange::MatchingEngine engine(publisher, scheduler);
//     exchange::SnapshotService service(engine);
//     exchange::SnapshotTextWriter writer;
//     exchange::MatchingEngineThread engine_thread(engine, service);
//     exchange::PersistenceThread persistence_thread(engine_thread, service, writer);

//     Order order1;
//     order1.order_id = 1;
//     order1.side = Side::Buy;
//     order1.price = 100.0;
//     order1.quantity = 10;
//     order1.type = Type::GTC;
//     order1.sequence = 1;
//     order1.expire_time = 0;

//     Order order2;
//     order2.order_id = 2;
//     order2.side = Side::Sell;
//     order2.price = 101.0;
//     order2.quantity = 20;
//     order2.type = Type::DAY;
//     order2.sequence = 2;
//     order2.expire_time = 1234567890;

//     engine.process_order(order1);
//     engine.process_order(order2);

//     engine_thread.start();
//     persistence_thread.start();

//     engine_thread.snapshot_requested();

//     std::this_thread::sleep_for(std::chrono::milliseconds(100));

//     std::string filename = "snapshot0.txt"; 
//     bool file_exists = fs::exists(filename);

//     persistence_thread.stop();
//     engine_thread.stop();

//     if (fs::exists(filename)) {
//         fs::remove(filename);
//     }
//     cleanup_snapshot_files();
// }

// TEST_CASE("PersistenceThread Lifecycle") {
//     exchange::EventBus bus;
//     exchange::EventPublisher publisher(bus);
//     exchange::ExpiryScheduler scheduler;
//     exchange::MatchingEngine engine(publisher, scheduler);
//     exchange::SnapshotService service(engine);
//     exchange::SnapshotTextWriter writer;
//     exchange::MatchingEngineThread engine_thread(engine, service);
//     exchange::PersistenceThread persistence_thread(engine_thread, service, writer);

//     REQUIRE_NOTHROW(persistence_thread.start());
//     REQUIRE_NOTHROW(persistence_thread.stop());

//     REQUIRE_NOTHROW(persistence_thread.start());

//     REQUIRE_NOTHROW(persistence_thread.stop());
//     REQUIRE_NOTHROW(persistence_thread.stop());

//     cleanup_snapshot_files();
// }

// TEST_CASE("Snapshot with Large Order Book") {
//     exchange::EventBus bus;
//     exchange::EventPublisher publisher(bus);
//     exchange::ExpiryScheduler scheduler;
//     exchange::MatchingEngine engine(publisher, scheduler);
//     exchange::SnapshotService service(engine);
//     exchange::SnapshotTextWriter writer;

//     const size_t NUM_ORDERS = 50000;
//     const uint64_t SEQUENCE = 999;

//     for (size_t i = 0; i < NUM_ORDERS / 2; ++i) {
//         Order order;
//         order.order_id = static_cast<uint64_t>(i + 1);
//         order.side = Side::Buy;
//         order.price = 1000.0 - (i + 1) * 0.01;
//         order.quantity = static_cast<uint32_t>(1 + (i % 100));
//         order.type = (i % 3 == 0) ? Type::GTC : Type::DAY;
//         order.sequence = i + 1;
//         order.expire_time = (i % 2 == 0) ? 0 : 1234567890;
//         engine.process_order(order);
//     }

//     for (size_t i = 0; i < NUM_ORDERS / 2; ++i) {
//         Order order;
//         order.order_id = static_cast<uint64_t>(i + NUM_ORDERS / 2 + 1);
//         order.side = Side::Sell;
//         order.price = 1000.0 + (i + 1) * 0.01;
//         order.quantity = static_cast<uint32_t>(1 + (i % 100));
//         order.type = (i % 3 == 0) ? Type::GTC : Type::DAY;
//         order.sequence = i + NUM_ORDERS / 2 + 1;
//         order.expire_time = (i % 2 == 0) ? 0 : 1234567890;
//         engine.process_order(order);
//     }

//     uint64_t timestamp = now_absolute_ns();
//     service.append_snapshot(SEQUENCE, timestamp);
//     auto& batch = service.batch(SEQUENCE).batch;
//     writer.process_snapshot(batch);

//     std::string filename = "snapshot" + std::to_string(SEQUENCE) + ".txt";
//     REQUIRE(fs::exists(filename));

//     auto file_size = fs::file_size(filename);
//     REQUIRE(file_size > 0);

//     std::string content = read_file_content(filename);
//     REQUIRE(content.find("snapshot timestamp= " + std::to_string(timestamp)) != std::string::npos);
    

//     size_t lines = count_lines(filename);
//     REQUIRE(lines == NUM_ORDERS + 2);

//     REQUIRE(content.find("order_id= 1") != std::string::npos);
//     REQUIRE(content.find("order_id= " + std::to_string(NUM_ORDERS)) != std::string::npos);

//     fs::remove(filename);
//     cleanup_snapshot_files();
// }

// TEST_CASE("Snapshot Service Memory Management") {
//     exchange::EventBus bus;
//     exchange::EventPublisher publisher(bus);
//     exchange::ExpiryScheduler scheduler;
//     exchange::MatchingEngine engine(publisher, scheduler);
//     exchange::SnapshotService service(engine);

//     const size_t NUM_ORDERS = 10000;
//     const uint64_t SEQUENCE_EVEN = 2;
//     const uint64_t SEQUENCE_ODD = 3;

//     for (size_t i = 0; i < NUM_ORDERS; ++i) {
//         Order order;
//         order.order_id = static_cast<uint64_t>(i + 1);
//         order.side = Side::Buy;
//         order.price = 100.0 + i * 0.01;
//         order.quantity = static_cast<uint32_t>(10);
//         order.type = Type::GTC;
//         order.sequence = i + 1;
//         order.expire_time = 0;
//         engine.process_order(order);
//     }

//     service.append_snapshot(SEQUENCE_EVEN, 1000);
//     auto& batch_a = service.batch(SEQUENCE_EVEN).batch;
//     REQUIRE(batch_a.orders.size() == NUM_ORDERS);

//     service.reset_batch(SEQUENCE_EVEN);
//     REQUIRE(batch_a.orders.empty());

//     service.append_snapshot(SEQUENCE_ODD, 2000);
//     auto& batch_b = service.batch(SEQUENCE_ODD).batch;
//     REQUIRE(batch_b.orders.size() == NUM_ORDERS);

//     REQUIRE(batch_a.orders.empty());

//     service.reset_batch(SEQUENCE_ODD);
//     REQUIRE(batch_b.orders.empty());

//     cleanup_snapshot_files();
// }

namespace fs = std::filesystem;
using namespace exchange;

struct TempDir {
    fs::path path;
    explicit TempDir(const std::string& name)
        : path(fs::temp_directory_path() / name) {
        std::error_code ec;
        fs::remove_all(path, ec);
        fs::create_directories(path);
    }
    ~TempDir() {
        std::error_code ec;
        fs::remove_all(path, ec);
    }
};


struct SnapshotHeaderDisk {
    uint32_t magic;
    uint32_t version;
    uint64_t timestamp;
    uint64_t sequence;
    uint64_t order_count;
    uint64_t account_count;
    double last_trade_price;
    uint64_t reserved;
};

struct ReadSnapshot {
    SnapshotHeaderDisk header;
    std::vector<OrderSnapshot> orders;
    std::vector<AccountSnapshot> accounts;
};

bool read_snapshot(const fs::path& path, ReadSnapshot& out) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return false;
    if (!f.read(reinterpret_cast<char*>(&out.header), sizeof(out.header))) return false;
    if (out.header.magic != 0x534E5053) return false;

    out.orders.resize(out.header.order_count);
    if (out.header.order_count > 0) {
        f.read(reinterpret_cast<char*>(out.orders.data()),
               out.header.order_count * sizeof(OrderSnapshot));
    }
    out.accounts.resize(out.header.account_count);
    if (out.header.account_count > 0) {
        f.read(reinterpret_cast<char*>(out.accounts.data()),
               out.header.account_count * sizeof(AccountSnapshot));
    }
    return static_cast<bool>(f);
}

struct TradeDataDisk {
    EventType type;
    uint64_t timestamp_ns;
    uint64_t buy_order_id;
    uint64_t sell_order_id;
    uint64_t account_id;
    double price;
    uint64_t quantity;
};

struct BookDataDisk {
    EventType type;
    uint64_t timestamp_ns;
    double best_bid;
    double best_ask;
    double last_trade_price;
    uint64_t last_trade_quantity;
};

struct OrderDataDisk {
    EventType event_type;
    uint64_t timestamp_ns;
    uint64_t order_id;
    uint64_t account_id;
    Side side;
    Type type;
    double price;
    uint32_t quantity;
    uint64_t sequence;
    uint64_t expire_time;
};

struct CancelDataDisk {
    EventType event_type;
    uint64_t timestamp_ns;
    uint64_t order_id;
    uint64_t account_id;
    uint16_t reason;
};

struct RejectDataDisk {
    EventType event_type;
    uint64_t timestamp_ns;
    uint64_t order_id;
    uint64_t account_id;
    uint16_t reason;
    uint16_t risk_reason;
};

struct RawEvent {
    uint16_t variant_index;
    std::vector<char> payload;
};

std::vector<RawEvent> read_event_file(const fs::path& path) {
    std::vector<RawEvent> events;
    std::ifstream f(path, std::ios::binary);
    if (!f) return events;

    while (true) {
        uint16_t idx;
        if (!f.read(reinterpret_cast<char*>(&idx), sizeof(idx))) break;

        size_t payload_size = 0;
        switch (idx) {
            case 0: payload_size = sizeof(TradeDataDisk);  break;
            case 1: payload_size = sizeof(BookDataDisk);   break;
            case 2: payload_size = sizeof(OrderDataDisk);  break; 
            case 3: payload_size = sizeof(CancelDataDisk); break;
            case 4: payload_size = sizeof(RejectDataDisk); break;
            default: return events;
        }

        RawEvent ev{idx, std::vector<char>(payload_size)};
        if (!f.read(ev.payload.data(), payload_size)) break;
        events.push_back(std::move(ev));
    }
    return events;
}

Order make_order(uint64_t id, uint64_t acc, Side side, double price,
                 uint32_t qty, Type type = Type::GTC, uint64_t exp = 0) {
    Order o{};
    o.order_id = id;
    o.account_id = acc;
    o.side = side;
    o.price = price;
    o.quantity = qty;
    o.type = type;
    o.sequence = id;
    o.expire_time = exp;
    return o;
}

void cleanup_binary_files(const fs::path& directory) {
    if (fs::exists(directory)) {
        fs::remove_all(directory);
    }
}

TEST_CASE("Snapshot: round-trip orders, accounts, and last_trade_price") {
    TempDir dir("snap_roundtrip");

    SnapshotBatch batch;
    batch.sequence = 7;
    batch.snapshot_time = 1'700'000'000'000'000'000ULL;
    batch.last_trade_price = 99.75;

    batch.orders.push_back({1, 10, Side::Buy, 100.0, 100, 1, Type::GTC, 0});
    batch.orders.push_back({2, 20, Side::Sell, 101.5, 200, 2, Type::DAY, 1234567890});

    batch.accounts.push_back({10, 100, 100, 0, 1'000'000, -50'000, -100});
    batch.accounts.push_back({20, -200, 0, 200, 0, 20'000, 200});

    SnapshotBinaryWriter writer(dir.path);
    writer.process_snapshot(batch);

    fs::path file = dir.path / "snapshot" / "snapshot_7.bin";
    REQUIRE(fs::exists(file));

    ReadSnapshot rs;
    REQUIRE(read_snapshot(file, rs));

    REQUIRE(rs.header.magic == 0x534E5053);
    REQUIRE(rs.header.version == 1);
    REQUIRE(rs.header.sequence == 7);
    REQUIRE(rs.header.timestamp == 1'700'000'000'000'000'000ULL);
    REQUIRE(rs.header.order_count == 2);
    REQUIRE(rs.header.account_count == 2);
    REQUIRE(rs.header.last_trade_price == 99.75);

    REQUIRE(rs.orders.size() == 2);
    REQUIRE(rs.orders[0].order_id == 1);
    REQUIRE(rs.orders[0].account_id == 10);
    REQUIRE(rs.orders[0].side == Side::Buy);
    REQUIRE(rs.orders[1].type == Type::DAY);
    REQUIRE(rs.orders[1].expire_time == 1234567890);

    REQUIRE(rs.accounts.size() == 2);
    REQUIRE(rs.accounts[0].account_id == 10);
    REQUIRE(rs.accounts[0].position == 100);
    REQUIRE(rs.accounts[0].credit_used == 1'000'000);
    REQUIRE(rs.accounts[0].cash_flow == -100);
    REQUIRE(rs.accounts[1].account_id == 20);
    REQUIRE(rs.accounts[1].position == -200);
    REQUIRE(rs.accounts[1].realized_pnl == 20'000);
}

TEST_CASE("Snapshot: empty batch (no orders, no accounts)") {
    TempDir dir("snap_empty");
    SnapshotBinaryWriter writer(dir.path);

    SnapshotBatch batch;
    batch.sequence = 0;
    batch.snapshot_time = 42;
    writer.process_snapshot(batch);

    ReadSnapshot rs;
    REQUIRE(read_snapshot(dir.path / "snapshot" / "snapshot_0.bin", rs));
    REQUIRE(rs.header.order_count == 0);
    REQUIRE(rs.header.account_count == 0);
    REQUIRE(rs.orders.empty());
    REQUIRE(rs.accounts.empty());
}

TEST_CASE("Snapshot: orders without accounts") {
    TempDir dir("snap_orders_only");
    SnapshotBinaryWriter writer(dir.path);

    SnapshotBatch batch;
    batch.sequence = 3;
    batch.snapshot_time = 100;
    batch.last_trade_price = 100.0;
    for (int i = 0; i < 100; ++i) {
        batch.orders.push_back({static_cast<uint64_t>(i + 1), 1, Side::Buy,
                                100.0 + i * 0.01, 10, static_cast<uint64_t>(i), Type::GTC, 0});
    }
    writer.process_snapshot(batch);

    ReadSnapshot rs;
    REQUIRE(read_snapshot(dir.path / "snapshot" / "snapshot_3.bin", rs));
    REQUIRE(rs.orders.size() == 100);
    REQUIRE(rs.accounts.empty());
}

TEST_CASE("Snapshot: multiple sequences coexist") {
    TempDir dir("snap_multi");
    SnapshotBinaryWriter writer(dir.path);

    for (uint64_t seq = 0; seq < 5; ++seq) {
        SnapshotBatch batch;
        batch.sequence = seq;
        batch.snapshot_time = 1000 + seq;
        for (uint64_t i = 0; i <= seq; ++i) {
            batch.orders.push_back({seq * 10 + i, 1, Side::Buy, 100.0, 1, i, Type::GTC, 0});
        }
        batch.accounts.push_back({seq, static_cast<int64_t>(seq), 0, 0, 0, 0, 0});
        writer.process_snapshot(batch);
    }

    for (uint64_t seq = 0; seq < 5; ++seq) {
        ReadSnapshot rs;
        REQUIRE(read_snapshot(dir.path / "snapshot" / ("snapshot_" + std::to_string(seq) + ".bin"), rs));
        REQUIRE(rs.header.sequence == seq);
        REQUIRE(rs.header.timestamp == 1000 + seq);
        REQUIRE(rs.orders.size() == seq + 1);
        REQUIRE(rs.header.account_count == 1);
    }
}

TEST_CASE("EventWriter: Trade event bytes") {
    TempDir dir("evt_trade");
    MPSCRingBuffer<Event> queue(16);
    EventBinaryWriter writer(dir.path, queue);

    Trade trade{};
    trade.buy_order_id = 11;
    trade.sell_order_id = 22;
    trade.account_id = 1;
    trade.price = 100.25;
    trade.quantity = 50;
    TradeEvent ev{{EventType::Trade, 123456789}, trade};

    queue.push(Event{std::in_place_type<TradeEvent>, ev});
    writer.write_events();

    auto raw = read_event_file(dir.path / "event_journal" / "event_journal_0.bin");
    REQUIRE(raw.size() == 1);
    REQUIRE(raw[0].variant_index == 0);
    REQUIRE(raw[0].payload.size() == sizeof(TradeDataDisk));

    TradeDataDisk disk;
    std::memcpy(&disk, raw[0].payload.data(), sizeof(disk));
    REQUIRE(disk.timestamp_ns == 123456789);
    REQUIRE(disk.buy_order_id == 11);
    REQUIRE(disk.sell_order_id == 22);
    REQUIRE(disk.account_id == 1);
    REQUIRE(disk.price == 100.25);
    REQUIRE(disk.quantity == 50);
}

TEST_CASE("EventWriter: OrderAccepted event bytes") {
    TempDir dir("evt_order");
    MPSCRingBuffer<Event> queue(16);
    EventBinaryWriter writer(dir.path, queue);

    OrderAcceptedEvent ev{
        {EventType::OrderAccepted, 987654321},
        777, 5, Side::Sell, Type::GTC, 150.5, 300, 0, 42
    };
    queue.push(Event{std::in_place_type<OrderAcceptedEvent>, ev});
    writer.write_events();

    auto raw = read_event_file(dir.path / "event_journal" / "event_journal_0.bin");
    REQUIRE(raw.size() == 1);

    OrderDataDisk disk;
    std::memcpy(&disk, raw[0].payload.data(), sizeof(disk));
    REQUIRE(disk.timestamp_ns == 987654321);
    REQUIRE(disk.order_id == 777);
    REQUIRE(disk.account_id == 5);
    REQUIRE(disk.side == Side::Sell);
    REQUIRE(disk.type == Type::GTC);
    REQUIRE(disk.price == 150.5);
    REQUIRE(disk.quantity == 300);
    REQUIRE(disk.sequence == 0);
    REQUIRE(disk.expire_time == 42);
}

TEST_CASE("EventWriter: OrderRejected writes risk_reason") {
    TempDir dir("evt_reject");
    MPSCRingBuffer<Event> queue(16);
    EventBinaryWriter writer(dir.path, queue);

    OrderRejectedEvent ev{
        {EventType::OrderRejected, 111},
        42, 5,
        RejectReason::RiskViolation,
        RiskViolationReason::CreditLimitExceeded
    };
    queue.push(Event{std::in_place_type<OrderRejectedEvent>, ev});
    writer.write_events();

    auto raw = read_event_file(dir.path / "event_journal" / "event_journal_0.bin");
    REQUIRE(raw.size() == 1);

    RejectDataDisk disk;
    std::memcpy(&disk, raw[0].payload.data(), sizeof(disk));
    REQUIRE(disk.reason == static_cast<uint16_t>(RejectReason::RiskViolation));
    REQUIRE(disk.risk_reason == static_cast<uint16_t>(RiskViolationReason::CreditLimitExceeded));
}

TEST_CASE("EventWriter: mixed event types preserved in order") {
    TempDir dir("evt_mixed");
    MPSCRingBuffer<Event> queue(16);
    EventBinaryWriter writer(dir.path, queue);

    Trade t{1, 2, 1, 100.0, 5};
    BookUpdate u;
    u.best_bid = 99.5; u.best_ask = 100.5;
    u.last_trade_price = 100.0; u.last_trade_quantity = 5;

    queue.push(Event{std::in_place_type<TradeEvent>, TradeEvent{{EventType::Trade, 1}, t}});
    queue.push(Event{std::in_place_type<BookUpdateEvent>, BookUpdateEvent{{EventType::BookUpdate, 2}, u}});
    queue.push(Event{std::in_place_type<TradeEvent>, TradeEvent{{EventType::Trade, 3}, t}});

    writer.write_events();

    auto raw = read_event_file(dir.path / "event_journal" / "event_journal_0.bin");
    REQUIRE(raw.size() == 3);
    REQUIRE(raw[0].variant_index == 0);   // Trade
    REQUIRE(raw[1].variant_index == 1);   // BookUpdate
    REQUIRE(raw[2].variant_index == 0);   // Trade
}

TEST_CASE("Journal switch: events routed to correct file") {
    TempDir dir("evt_switch");
    MPSCRingBuffer<Event> queue(16);
    EventBinaryWriter writer(dir.path, queue);

    Trade t{1, 2, 1, 100.0, 1};

    queue.push(Event{std::in_place_type<TradeEvent>, TradeEvent{{EventType::Trade, 100}, t}});
    queue.push(Event{std::in_place_type<TradeEvent>, TradeEvent{{EventType::Trade, 200}, t}});
    writer.write_events();

    writer.request_journal_switch(1);
    writer.write_events(); 

    queue.push(Event{std::in_place_type<TradeEvent>, TradeEvent{{EventType::Trade, 300}, t}});
    writer.write_events();

    auto raw0 = read_event_file(dir.path / "event_journal" / "event_journal_0.bin");
    auto raw1 = read_event_file(dir.path / "event_journal" / "event_journal_1.bin");

    REQUIRE(raw0.size() == 2);
    REQUIRE(raw1.size() == 1);

    TradeDataDisk d0, d1;
    std::memcpy(&d0, raw0[0].payload.data(), sizeof(d0));
    std::memcpy(&d0, raw0[1].payload.data(), sizeof(d0));
    std::memcpy(&d1, raw1[0].payload.data(), sizeof(d1));
    REQUIRE(d1.timestamp_ns == 300);
}

TEST_CASE("Journal switch: deferred while queue non-empty") {
    TempDir dir("evt_defer");
    MPSCRingBuffer<Event> queue(16);
    EventBinaryWriter writer(dir.path, queue);

    Trade t{1, 2, 1, 100.0, 1};
    queue.push(Event{std::in_place_type<TradeEvent>, TradeEvent{{EventType::Trade, 100}, t}});
    writer.request_journal_switch(5);

    writer.write_events();

    REQUIRE(fs::exists(dir.path / "event_journal" / "event_journal_5.bin"));
}

TEST_CASE("EventJournalSink: forwards all event types") {
    EventBus bus;
    MPSCRingBuffer<Event> queue(64);
    EventJournalSink sink(queue);
    bus.subscribe(&sink);

    Trade t{1, 2, 1, 100.0, 1};
    bus.publish(TradeEvent{{EventType::Trade, 1}, t});

    BookUpdate u;
    u.best_bid = 99.0; u.best_ask = 100.0;
    bus.publish(BookUpdateEvent{{EventType::BookUpdate, 2}, u});

    bus.publish(OrderCancelledEvent{{EventType::OrderCancelled, 3}, 1, 0, CancelReason::UserRequest});

    REQUIRE(queue.size() == 3);

    Event ev;
    REQUIRE(queue.pop(ev));
    REQUIRE(std::holds_alternative<TradeEvent>(ev));
    REQUIRE(queue.pop(ev));
    REQUIRE(std::holds_alternative<BookUpdateEvent>(ev));
    REQUIRE(queue.pop(ev));
    REQUIRE(std::holds_alternative<OrderCancelledEvent>(ev));
}

TEST_CASE("Persistence: engine + snapshot + event journal end-to-end") {
    TempDir dir("integ_e2e");

    EventBus bus;
    EventPublisher publisher(bus);
    ExpiryScheduler scheduler;
    RiskEngine risk(1024);

    RiskLimits limits;
    limits.max_order_size = 10000;
    limits.max_orders_per_sec = 100000;
    limits.max_position = 100000;
    limits.max_credit = 100'000'000;
    limits.max_daily_loss = 100'000'000;
    limits.price_band_pct = 0.0;
    limits.stp_policy = STPPolicy::None;
    
    uint32_t acc_id = risk.register_account(limits);

    MatchingEngine engine(publisher, scheduler, risk);

    MPSCRingBuffer<Event> event_queue(1 << 16);
    EventBinaryWriter event_writer(dir.path, event_queue);
    EventJournalSink sink(event_queue);
    bus.subscribe(&sink);

    SnapshotBinaryWriter snapshot_writer(dir.path);
    SnapshotService service(engine, event_writer, risk);
    MatchingEngineThread engine_thread(engine, service); 

    for (int i = 0; i < 10; ++i) {
        engine.process_order(make_order(i + 1, acc_id, Side::Buy, 100.0 + i, 10));
    }

    while (!event_queue.empty()) {
        event_writer.write_events();
    }
    REQUIRE(event_queue.empty());

    service.append_snapshot(0, now_absolute_ns());
    auto& flag = service.batch(0);
    REQUIRE(flag.ready.load());
    snapshot_writer.process_snapshot(flag.batch);

    ReadSnapshot rs;
    REQUIRE(read_snapshot(dir.path / "snapshot" / "snapshot_0.bin", rs));
    REQUIRE(rs.header.order_count == 10);
    REQUIRE(rs.orders.size() == 10);
    
    for (size_t i = 0; i < rs.orders.size(); ++i) {
        REQUIRE(rs.orders[i].order_id == 10 - i);
        REQUIRE(rs.orders[i].side == Side::Buy);
        REQUIRE(rs.orders[i].price == 109.0 - i);
    }

    auto events = read_event_file(dir.path / "event_journal" / "event_journal_0.bin");
    REQUIRE(events.size() == 10);
    
    for (const auto& ev : events) {
        REQUIRE(ev.variant_index == 2); 
    }
}

TEST_CASE("Persistence: thread lifecycle with real timer") {
    TempDir dir("integ_thread");

    EventBus bus;
    EventPublisher publisher(bus);
    ExpiryScheduler scheduler;
    RiskEngine risk(1024);
    MatchingEngine engine(publisher, scheduler, risk);

    MPSCRingBuffer<Event> event_queue(1 << 16);
    EventBinaryWriter event_writer(dir.path, event_queue);
    EventJournalSink sink(event_queue);
    bus.subscribe(&sink);

    SnapshotBinaryWriter snapshot_writer(dir.path);
    SnapshotService service(engine, event_writer, risk);
    MatchingEngineThread engine_thread(engine, service);
    PersistenceThread persistence_thread(engine_thread, service, snapshot_writer, event_writer);

    engine_thread.start();
    persistence_thread.start();

    for (int i = 0; i < 5; ++i) {
        engine_thread.submit_order(make_order(i + 1, 1,
                                              i % 2 ? Side::Sell : Side::Buy,
                                              100.0 + i, 10));
    }

    engine_thread.snapshot_requested();
    std::this_thread::sleep_for(std::chrono::milliseconds(300));

    persistence_thread.stop();
    engine_thread.stop();

    bool has_snapshot = false;
    for (const auto& e : fs::directory_iterator(dir.path / "snapshot")) {
        if (e.path().extension() == ".bin") { has_snapshot = true; break; }
    }
    REQUIRE(has_snapshot);

    bool has_journal = false;
    for (const auto& e : fs::directory_iterator(dir.path / "event_journal")) {
        if (e.path().extension() == ".bin") { has_journal = true; break; }
    }
    REQUIRE(has_journal);
}

TEST_CASE("EventWriter: throughput (100K events)") {
    TempDir dir("perf_evt");
    MPSCRingBuffer<Event> queue(1 << 20);
    EventBinaryWriter writer(dir.path, queue);

    constexpr size_t N = 100'000;
    Trade t{1, 2, 1, 100.0, 1};
    for (size_t i = 0; i < N; ++i) {
        queue.push(Event{std::in_place_type<TradeEvent>,
                         TradeEvent{{EventType::Trade, i}, t}});
    }

    auto start = std::chrono::steady_clock::now();
    while (!queue.empty()) {
        writer.write_events();
    }
    auto elapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::steady_clock::now() - start).count();

    double ns_per_event = static_cast<double>(elapsed) / N;
    double mb_per_sec = static_cast<double>(N * sizeof(TradeDataDisk))
                      / (elapsed / 1e9) / (1024.0 * 1024.0);

    std::cout << "\nEventWriter: " << ns_per_event << " ns/event, "
              << mb_per_sec << " MB/s\n";

    auto raw = read_event_file(dir.path / "event_journal" / "event_journal_0.bin");
    REQUIRE(raw.size() == N);
}

TEST_CASE("SnapshotWriter: 10K orders write latency") {
    TempDir dir("perf_snap");
    SnapshotBinaryWriter writer(dir.path);

    SnapshotBatch batch;
    batch.sequence = 0;
    batch.snapshot_time = 1;
    batch.orders.reserve(10'000);
    batch.accounts.reserve(100);
    for (int i = 0; i < 10'000; ++i) {
        batch.orders.push_back({static_cast<uint64_t>(i), 1, Side::Buy,
                                100.0, 1, static_cast<uint64_t>(i), Type::GTC, 0});
    }
    for (int i = 0; i < 100; ++i) {
        batch.accounts.push_back({static_cast<uint64_t>(i), 0, 0, 0, 0, 0, 0});
    }

    auto start = std::chrono::steady_clock::now();
    writer.process_snapshot(batch);
    auto elapsed_us = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now() - start).count();

    std::cout << "Snapshot 10K orders: " << elapsed_us << " us\n";

    ReadSnapshot rs;
    REQUIRE(read_snapshot(dir.path / "snapshot" / "snapshot_0.bin", rs));
    REQUIRE(rs.orders.size() == 10'000);
    REQUIRE(rs.accounts.size() == 100);
}

