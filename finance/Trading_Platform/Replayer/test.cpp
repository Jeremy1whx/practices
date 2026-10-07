// test_replay.cpp
#define CATCH_CONFIG_MAIN
#define CATCH_CONFIG_ENABLE_BENCHMARKING

#include <chrono>
#include <thread>
#include <filesystem>
#include <fstream>

#include "../Match_Engine/MatchingEngine.h"
#include "../Match_Engine/MatchingEngineThread.h"
#include "../Persistence/SnapshotWriter.h"
#include "../Event_Bus/EventBus.h"
#include "../Event_Bus/EventPublisher.h"
#include "../Persistence/EventWriter.h"
#include "ReplayReader.h"
#include "ReplayEngine.h"
#include "../Lock_Free_Ring_Buffer/mpsc/catch.hpp"

Order make_order(uint64_t id, Side side, double price, uint32_t qty,
                 Type type = Type::GTC, uint64_t expire = 0) {
    Order order;
    order.order_id = id;
    order.side = side;
    order.type = type;
    order.price = price;
    order.quantity = qty;
    order.sequence = id;
    order.expire_time = expire;
    order.ingress_timestamp_ns = 0;
    order.egress_timestamp_ns = 0;
    order.next = nullptr;
    order.prev = nullptr;
    return order;
}

bool books_equal(const exchange::MatchingEngine& a, const exchange::MatchingEngine& b) {
    if (a.bids().size() != b.bids().size()) return false;
    if (a.asks().size() != b.asks().size()) return false;

    for (size_t i = 0; i < a.bids().size(); ++i) {
        if (a.bids()[i].price != b.bids()[i].price) return false;
        if (a.bids()[i].avaliable != b.bids()[i].avaliable) return false;
        auto* oa = a.bids()[i].level.head;
        auto* ob = b.bids()[i].level.head;
        while (oa && ob) {
            if (oa->order_id != ob->order_id) return false;
            if (oa->quantity != ob->quantity) return false;
            oa = oa->next;
            ob = ob->next;
        }
        if (oa || ob) return false;
    }
    for (size_t i = 0; i < a.asks().size(); ++i) {
        if (a.asks()[i].price != b.asks()[i].price) return false;
        if (a.asks()[i].avaliable != b.asks()[i].avaliable) return false;
        auto* oa = a.asks()[i].level.head;
        auto* ob = b.asks()[i].level.head;
        while (oa && ob) {
            if (oa->order_id != ob->order_id) return false;
            if (oa->quantity != ob->quantity) return false;
            oa = oa->next;
            ob = ob->next;
        }
        if (oa || ob) return false;
    }
    return true;
}

void cleanup_dir(const std::filesystem::path& dir) {
    if (std::filesystem::exists(dir)) std::filesystem::remove_all(dir);
}

TEST_CASE("Replay - no snapshot returns gracefully") {
    std::filesystem::path test_dir = "./replay_test_empty";
    cleanup_dir(test_dir);
    std::filesystem::create_directories(test_dir / "snapshot");
    std::filesystem::create_directories(test_dir / "event_journal");

    exchange::EventBus bus;
    exchange::EventPublisher publisher(bus);
    exchange::ExpiryScheduler scheduler;
    exchange::MatchingEngine engine(publisher, scheduler);

    exchange::BinaryReader reader(test_dir);
    exchange::ReplayEngine replayer(reader, engine);
    replayer.replay(); 

    REQUIRE(engine.bids().empty());
    REQUIRE(engine.asks().empty());

    cleanup_dir(test_dir);
}

TEST_CASE("Replay - snapshot only, no events") {
    std::filesystem::path test_dir = "./replay_test_snapshot_only";
    cleanup_dir(test_dir);
    std::filesystem::create_directories(test_dir / "snapshot");
    std::filesystem::create_directories(test_dir / "event_journal");

    {
        exchange::EventBus bus;
        exchange::EventPublisher publisher(bus);
        exchange::ExpiryScheduler scheduler;
        exchange::MatchingEngine engine(publisher, scheduler);

        engine.process_order(make_order(1, Side::Buy, 100.0, 10));
        engine.process_order(make_order(2, Side::Buy, 99.5, 20));
        engine.process_order(make_order(3, Side::Sell, 101.0, 30));
        engine.process_order(make_order(4, Side::Sell, 102.0, 40));

        exchange::SnapshotBatch batch;
        batch.sequence = 0;
        batch.snapshot_time = 1000;
        for (const auto& lvl : engine.bids()) {
            for (auto* o = lvl.level.head; o; o = o->next) {
                batch.orders.push_back({o->order_id, o->side, o->price,
                                        o->quantity, o->sequence, o->type, o->expire_time});
            }
        }
        for (const auto& lvl : engine.asks()) {
            for (auto* o = lvl.level.head; o; o = o->next) {
                batch.orders.push_back({o->order_id, o->side, o->price,
                                        o->quantity, o->sequence, o->type, o->expire_time});
            }
        }

        exchange::SnapshotBinaryWriter writer(test_dir);
        writer.process_snapshot(batch);
    }

    {
        exchange::EventBus bus;
        exchange::EventPublisher publisher(bus);
        exchange::ExpiryScheduler scheduler;
        exchange::MatchingEngine engine(publisher, scheduler);

        exchange::BinaryReader reader(test_dir);
        exchange::ReplayEngine replayer(reader, engine);
        replayer.replay();

        REQUIRE(engine.bids().size() == 2);
        REQUIRE(engine.asks().size() == 2);
        REQUIRE(engine.bids()[0].price == 100.0);
        REQUIRE(engine.bids()[0].avaliable == 10);
        REQUIRE(engine.bids()[1].price == 99.5);
        REQUIRE(engine.bids()[1].avaliable == 20);
        REQUIRE(engine.asks()[0].price == 101.0);
        REQUIRE(engine.asks()[0].avaliable == 30);
        REQUIRE(engine.asks()[1].price == 102.0);
        REQUIRE(engine.asks()[1].avaliable == 40);
    }

    cleanup_dir(test_dir);
}

TEST_CASE("Replay - snapshot plus OrderAccepted events") {
    std::filesystem::path test_dir = "./replay_test_with_events";
    cleanup_dir(test_dir);
    std::filesystem::create_directories(test_dir / "snapshot");
    std::filesystem::create_directories(test_dir / "event_journal");

    {
        exchange::EventBus bus;
        exchange::EventPublisher publisher(bus);
        exchange::ExpiryScheduler scheduler;
        exchange::MatchingEngine engine(publisher, scheduler);

        engine.process_order(make_order(1, Side::Buy, 100.0, 10));
        engine.process_order(make_order(2, Side::Sell, 101.0, 20));

        exchange::SnapshotBatch batch;
        batch.sequence = 0;
        batch.snapshot_time = 1000;
        for (const auto& lvl : engine.bids()) {
            for (auto* o = lvl.level.head; o; o = o->next) {
                batch.orders.push_back({o->order_id, o->side, o->price,
                                        o->quantity, o->sequence, o->type, o->expire_time});
            }
        }
        for (const auto& lvl : engine.asks()) {
            for (auto* o = lvl.level.head; o; o = o->next) {
                batch.orders.push_back({o->order_id, o->side, o->price,
                                        o->quantity, o->sequence, o->type, o->expire_time});
            }
        }
        exchange::SnapshotBinaryWriter writer(test_dir);
        writer.process_snapshot(batch);

        engine.process_order(make_order(3, Side::Buy, 99.0, 15));
        engine.process_order(make_order(4, Side::Sell, 102.0, 25));

        std::filesystem::path journal = test_dir / "event_journal" / "event_journal_1.bin";
        std::ofstream f(journal, std::ios::binary);

        {
            uint16_t type = static_cast<uint16_t>(exchange::EventType::OrderAccepted);
            f.write(reinterpret_cast<const char*>(&type), sizeof(type));
            struct OrderData {
                uint64_t timestamp_ns;
                uint64_t order_id;
                Side side;
                Type type;
                double price;
                uint32_t quantity;
                uint64_t sequence;
                uint64_t expire_time;
            } d{2000, 3, Side::Buy, Type::GTC, 99.0, 15, 3, 0};
            f.write(reinterpret_cast<const char*>(&d), sizeof(d));
        }
        {
            uint16_t type = static_cast<uint16_t>(exchange::EventType::OrderAccepted);
            f.write(reinterpret_cast<const char*>(&type), sizeof(type));
            struct OrderData {
                uint64_t timestamp_ns;
                uint64_t order_id;
                Side side;
                Type type;
                double price;
                uint32_t quantity;
                uint64_t sequence;
                uint64_t expire_time;
            } d{2100, 4, Side::Sell, Type::GTC, 102.0, 25, 4, 0};
            f.write(reinterpret_cast<const char*>(&d), sizeof(d));
        }
        f.close();
    }

    {
        exchange::EventBus bus;
        exchange::EventPublisher publisher(bus);
        exchange::ExpiryScheduler scheduler;
        exchange::MatchingEngine engine(publisher, scheduler);

        exchange::BinaryReader reader(test_dir);
        exchange::ReplayEngine replayer(reader, engine);
        replayer.replay();

        REQUIRE(engine.bids().size() == 2); 
        REQUIRE(engine.asks().size() == 2);
        REQUIRE(engine.bids()[0].price == 100.0);
        REQUIRE(engine.bids()[0].avaliable == 10);
        REQUIRE(engine.bids()[1].price == 99.0);
        REQUIRE(engine.bids()[1].avaliable == 15);
        REQUIRE(engine.asks()[0].price == 101.0);
        REQUIRE(engine.asks()[0].avaliable == 20);
        REQUIRE(engine.asks()[1].price == 102.0);
        REQUIRE(engine.asks()[1].avaliable == 25);
    }

    cleanup_dir(test_dir);
}

TEST_CASE("Replay - Trade event reduces quantity") {
    std::filesystem::path test_dir = "./replay_test_trade";
    cleanup_dir(test_dir);
    std::filesystem::create_directories(test_dir / "snapshot");
    std::filesystem::create_directories(test_dir / "event_journal");

    {
        exchange::EventBus bus;
        exchange::EventPublisher publisher(bus);
        exchange::ExpiryScheduler scheduler;
        exchange::MatchingEngine engine(publisher, scheduler);

        exchange::SnapshotBatch batch;
        batch.sequence = 0;
        batch.snapshot_time = 1000;
        batch.orders.push_back({1, Side::Buy,  100.0, 10, 1, Type::GTC, 0});
        batch.orders.push_back({2, Side::Sell, 101.0, 20, 2, Type::GTC, 0});
        exchange::SnapshotBinaryWriter writer(test_dir);
        writer.process_snapshot(batch);

        std::filesystem::path journal = test_dir / "event_journal" / "event_journal_1.bin";
        std::ofstream f(journal, std::ios::binary);
        uint16_t type = static_cast<uint16_t>(exchange::EventType::Trade);
        f.write(reinterpret_cast<const char*>(&type), sizeof(type));
        struct TradeData {
            uint64_t timestamp_ns;
            uint64_t buy_order_id;
            uint64_t sell_order_id;
            double price;
            uint64_t quantity;
        } d{2000, 1, 2, 100.0, 5};
        f.write(reinterpret_cast<const char*>(&d), sizeof(d));
        f.close();
    }

    {
        exchange::EventBus bus;
        exchange::EventPublisher publisher(bus);
        exchange::ExpiryScheduler scheduler;
        exchange::MatchingEngine engine(publisher, scheduler);

        exchange::BinaryReader reader(test_dir);
        exchange::ReplayEngine replayer(reader, engine);
        replayer.replay();

        REQUIRE(engine.bids().size() == 1);
        REQUIRE(engine.asks().size() == 1);
        REQUIRE(engine.bids()[0].avaliable == 5); 
        REQUIRE(engine.asks()[0].avaliable == 15); 
        REQUIRE(engine.bids()[0].level.head->quantity == 5);
        REQUIRE(engine.asks()[0].level.head->quantity == 15);
    }

    cleanup_dir(test_dir);
}

TEST_CASE("Replay - OrderCancelled removes order") {
    std::filesystem::path test_dir = "./replay_test_cancel";
    cleanup_dir(test_dir);
    std::filesystem::create_directories(test_dir / "snapshot");
    std::filesystem::create_directories(test_dir / "event_journal");

    {
        exchange::SnapshotBatch batch;
        batch.sequence = 0;
        batch.snapshot_time = 1000;
        batch.orders.push_back({1, Side::Buy,  100.0, 10, 1, Type::GTC, 0});
        batch.orders.push_back({2, Side::Buy,   99.0, 20, 2, Type::GTC, 0});
        exchange::SnapshotBinaryWriter writer(test_dir);
        writer.process_snapshot(batch);

        std::filesystem::path journal = test_dir / "event_journal" / "event_journal_1.bin";
        std::ofstream f(journal, std::ios::binary);
        uint16_t type = static_cast<uint16_t>(exchange::EventType::OrderCancelled);
        f.write(reinterpret_cast<const char*>(&type), sizeof(type));
        struct CancelData {
            uint64_t timestamp_ns;
            uint64_t order_id;
            uint16_t reason;
        } d{2000, 1, static_cast<uint16_t>(exchange::CancelReason::UserRequest)};
        f.write(reinterpret_cast<const char*>(&d), sizeof(d));
        f.close();
    }

    {
        exchange::EventBus bus;
        exchange::EventPublisher publisher(bus);
        exchange::ExpiryScheduler scheduler;
        exchange::MatchingEngine engine(publisher, scheduler);

        exchange::BinaryReader reader(test_dir);
        exchange::ReplayEngine replayer(reader, engine);
        replayer.replay();

        REQUIRE(engine.bids().size() == 1);
        REQUIRE(engine.bids()[0].price == 99.0);
        REQUIRE(engine.bids()[0].avaliable == 20);
        REQUIRE(engine.order_lookup().find(1) == engine.order_lookup().end());
        REQUIRE(engine.order_lookup().find(2) != engine.order_lookup().end());
    }

    cleanup_dir(test_dir);
}

TEST_CASE("Replay - multiple event journal files") {
    std::filesystem::path test_dir = "./replay_test_multi";
    cleanup_dir(test_dir);
    std::filesystem::create_directories(test_dir / "snapshot");
    std::filesystem::create_directories(test_dir / "event_journal");

    {
        exchange::SnapshotBatch batch;
        batch.sequence = 0;
        batch.snapshot_time = 1000;
        exchange::SnapshotBinaryWriter writer(test_dir);
        writer.process_snapshot(batch);
    }

    {
        std::filesystem::path journal = test_dir / "event_journal" / "event_journal_1.bin";
        std::ofstream f(journal, std::ios::binary);
        uint16_t type = static_cast<uint16_t>(exchange::EventType::OrderAccepted);
        f.write(reinterpret_cast<const char*>(&type), sizeof(type));
        struct OrderData {
            uint64_t timestamp_ns;
            uint64_t order_id;
            Side side;
            Type type;
            double price;
            uint32_t quantity;
            uint64_t sequence;
            uint64_t expire_time;
        } d{2000, 1, Side::Buy, Type::GTC, 100.0, 10, 1, 0};
        f.write(reinterpret_cast<const char*>(&d), sizeof(d));
        f.close();
    }

    {
        std::filesystem::path journal = test_dir / "event_journal" / "event_journal_2.bin";
        std::ofstream f(journal, std::ios::binary);
        uint16_t type = static_cast<uint16_t>(exchange::EventType::OrderAccepted);
        f.write(reinterpret_cast<const char*>(&type), sizeof(type));
        struct OrderData {
            uint64_t timestamp_ns;
            uint64_t order_id;
            Side side;
            Type type;
            double price;
            uint32_t quantity;
            uint64_t sequence;
            uint64_t expire_time;
        } d{3000, 2, Side::Sell, Type::GTC, 101.0, 20, 2, 0};
        f.write(reinterpret_cast<const char*>(&d), sizeof(d));
        f.close();
    }

    {
        exchange::EventBus bus;
        exchange::EventPublisher publisher(bus);
        exchange::ExpiryScheduler scheduler;
        exchange::MatchingEngine engine(publisher, scheduler);

        exchange::BinaryReader reader(test_dir);
        exchange::ReplayEngine replayer(reader, engine);
        replayer.replay();

        REQUIRE(engine.bids().size() == 1);
        REQUIRE(engine.asks().size() == 1);
        REQUIRE(engine.bids()[0].price == 100.0);
        REQUIRE(engine.asks()[0].price == 101.0);
    }

    cleanup_dir(test_dir);
}

TEST_CASE("Replay - uses latest snapshot") {
    std::filesystem::path test_dir = "./replay_test_latest";
    cleanup_dir(test_dir);
    std::filesystem::create_directories(test_dir / "snapshot");
    std::filesystem::create_directories(test_dir / "event_journal");

    {
        exchange::SnapshotBatch batch;
        batch.sequence = 0;
        batch.snapshot_time = 1000;
        batch.orders.push_back({1, Side::Buy, 100.0, 10, 1, Type::GTC, 0});
        exchange::SnapshotBinaryWriter writer(test_dir);
        writer.process_snapshot(batch);
    }

    {
        exchange::SnapshotBatch batch;
        batch.sequence = 1;
        batch.snapshot_time = 2000;
        batch.orders.push_back({1, Side::Buy,  100.0, 10, 1, Type::GTC, 0});
        batch.orders.push_back({2, Side::Sell, 101.0, 20, 2, Type::GTC, 0});
        exchange::SnapshotBinaryWriter writer(test_dir);
        writer.process_snapshot(batch);
    }

    {
        exchange::EventBus bus;
        exchange::EventPublisher publisher(bus);
        exchange::ExpiryScheduler scheduler;
        exchange::MatchingEngine engine(publisher, scheduler);

        exchange::BinaryReader reader(test_dir);
        exchange::ReplayEngine replayer(reader, engine);
        replayer.replay();

        REQUIRE(engine.bids().size() == 1);
        REQUIRE(engine.asks().size() == 1); 
    }

    cleanup_dir(test_dir);
}

TEST_CASE("Replay - rebuild expiry scheduler") {
    std::filesystem::path test_dir = "./replay_test_expiry";
    cleanup_dir(test_dir);
    std::filesystem::create_directories(test_dir / "snapshot");
    std::filesystem::create_directories(test_dir / "event_journal");

    uint64_t now = now_absolute_ns();
    uint64_t expire1 = now + 60'000'000'000ULL; 
    uint64_t expire2 = now + 120'000'000'000ULL; 

    {
        exchange::SnapshotBatch batch;
        batch.sequence = 0;
        batch.snapshot_time = now;
        batch.orders.push_back({1, Side::Buy, 100.0, 10, 1, Type::GTD, expire1});
        batch.orders.push_back({2, Side::Buy,  99.0, 20, 2, Type::GTD, expire2});
        batch.orders.push_back({3, Side::Buy,  98.0, 30, 3, Type::GTC, 0});  
        exchange::SnapshotBinaryWriter writer(test_dir);
        writer.process_snapshot(batch);
    }

    {
        exchange::EventBus bus;
        exchange::EventPublisher publisher(bus);
        exchange::ExpiryScheduler scheduler;
        exchange::MatchingEngine engine(publisher, scheduler);

        exchange::BinaryReader reader(test_dir);
        exchange::ReplayEngine replayer(reader, engine);
        replayer.replay();

        REQUIRE(scheduler.expiry_count() == 2);
        REQUIRE(scheduler.contains(1));
        REQUIRE(scheduler.contains(2));
        REQUIRE_FALSE(scheduler.contains(3));
        // std::cout << "expiry_count=" << scheduler.expiry_count() << "\n"
        //   << "seconds_entry_count=" << scheduler.seconds_entry_count() << "\n"
        //   << "days_entry_count=" << scheduler.days_entry_count() << "\n";

        // size_t manual_count = scheduler.seconds_entry_count() + scheduler.days_entry_count();
        // std::cout << "manual wheel_count=" << manual_count << "\n";
        // std::cout << "lookup size=" << scheduler.expiry_count() << "\n";
        REQUIRE(scheduler.validate());
    }

    cleanup_dir(test_dir);
}

TEST_CASE("Replay - full pipeline matches original engine") {
    std::filesystem::path test_dir = "./replay_test_full";
    cleanup_dir(test_dir);
    std::filesystem::create_directories(test_dir / "snapshot");
    std::filesystem::create_directories(test_dir / "event_journal");

    {
        exchange::EventBus bus;
        exchange::EventPublisher publisher(bus);
        exchange::ExpiryScheduler scheduler;
        exchange::MatchingEngine engine(publisher, scheduler);

        engine.process_order(make_order(1, Side::Buy,  100.0, 10));
        engine.process_order(make_order(2, Side::Buy,   99.0, 20));
        engine.process_order(make_order(3, Side::Sell, 101.0, 30));
        engine.process_order(make_order(4, Side::Sell, 102.0, 40));

        exchange::SnapshotBatch batch;
        batch.sequence = 0;
        batch.snapshot_time = 1000;
        for (const auto& lvl : engine.bids()) {
            for (auto* o = lvl.level.head; o; o = o->next)
                batch.orders.push_back({o->order_id, o->side, o->price,
                                        o->quantity, o->sequence, o->type, o->expire_time});
        }
        for (const auto& lvl : engine.asks()) {
            for (auto* o = lvl.level.head; o; o = o->next)
                batch.orders.push_back({o->order_id, o->side, o->price,
                                        o->quantity, o->sequence, o->type, o->expire_time});
        }
        exchange::SnapshotBinaryWriter writer(test_dir);
        writer.process_snapshot(batch);

        engine.process_order(make_order(5, Side::Buy,  98.0, 50));
        engine.process_order(make_order(6, Side::Sell, 103.0, 60));
        engine.cancel_order(2, exchange::CancelReason::UserRequest);

        std::filesystem::path journal = test_dir / "event_journal" / "event_journal_1.bin";
        std::ofstream f(journal, std::ios::binary);
        struct OrderData {
            uint64_t timestamp_ns;
            uint64_t order_id;
            Side side;
            Type type;
            double price;
            uint32_t quantity;
            uint64_t sequence;
            uint64_t expire_time;
        };
        auto write_order = [&](uint16_t t, const OrderData& d) {
            f.write(reinterpret_cast<const char*>(&t), sizeof(t));
            f.write(reinterpret_cast<const char*>(&d), sizeof(d));
        };

        write_order(static_cast<uint16_t>(exchange::EventType::OrderAccepted),
                    {2000, 5, Side::Buy, Type::GTC, 98.0, 50, 5, 0});
        write_order(static_cast<uint16_t>(exchange::EventType::OrderAccepted),
                    {2100, 6, Side::Sell, Type::GTC, 103.0, 60, 6, 0});

        uint16_t ct = static_cast<uint16_t>(exchange::EventType::OrderCancelled);
        struct CancelData {
            uint64_t timestamp_ns;
            uint64_t order_id;
            uint16_t reason;
        } cd{2200, 2, static_cast<uint16_t>(exchange::CancelReason::UserRequest)};
        f.write(reinterpret_cast<const char*>(&ct), sizeof(ct));
        f.write(reinterpret_cast<const char*>(&cd), sizeof(cd));
        f.close();
    }

    exchange::EventBus bus;
    exchange::EventPublisher publisher(bus);
    exchange::ExpiryScheduler scheduler;
    exchange::MatchingEngine replayed(publisher, scheduler);

    exchange::BinaryReader reader(test_dir);
    exchange::ReplayEngine replayer(reader, replayed);
    replayer.replay();

    exchange::EventBus bus2;
    exchange::EventPublisher publisher2(bus2);
    exchange::ExpiryScheduler scheduler2;
    exchange::MatchingEngine expected(publisher2, scheduler2);

    expected.process_order(make_order(1, Side::Buy,  100.0, 10));
    expected.process_order(make_order(2, Side::Buy,   99.0, 20));
    expected.process_order(make_order(3, Side::Sell, 101.0, 30));
    expected.process_order(make_order(4, Side::Sell, 102.0, 40));
    expected.process_order(make_order(5, Side::Buy,   98.0, 50));
    expected.process_order(make_order(6, Side::Sell, 103.0, 60));
    expected.cancel_order(2, exchange::CancelReason::UserRequest);

    REQUIRE(books_equal(replayed, expected));

    cleanup_dir(test_dir);
}