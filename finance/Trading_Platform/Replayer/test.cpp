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
#include "../Risk_Engine/RiskEngine.h"
#include "ReplayReader.h"
#include "ReplayEngine.h"
#include "../Lock_Free_Ring_Buffer/mpsc/catch.hpp"

namespace fs = std::filesystem;
using namespace exchange;

void cleanup_dir(const fs::path& dir) {
    if (fs::exists(dir)) fs::remove_all(dir);
}

Order make_order(uint64_t order_id, uint64_t account_id, Side side, double price, uint32_t qty,
                 Type type = Type::GTC, uint64_t expire = 0) {
    Order order{};
    order.order_id = order_id;
    order.account_id = account_id;
    order.side = side;
    order.type = type;
    order.price = price;
    order.quantity = qty;
    order.sequence = order_id;
    order.expire_time = expire;
    return order;
}

void setup_risk_accounts(RiskEngine& risk) {
    RiskLimits limits;
    limits.max_order_size = 100000;
    limits.max_position = 100000;
    limits.max_credit = 100'000'000;
    limits.max_daily_loss = 100'000'000;
    for (int i = 0; i < 10; ++i) {
        risk.register_account(limits);
    }
}

bool books_equal(const MatchingEngine& a, const MatchingEngine& b) {
    if (a.bids().size() != b.bids().size()) return false;
    if (a.asks().size() != b.asks().size()) return false;

    auto compare_levels = [](const auto& levels_a, const auto& levels_b) {
        for (size_t i = 0; i < levels_a.size(); ++i) {
            if (levels_a[i].price != levels_b[i].price) return false;
            if (levels_a[i].avaliable != levels_b[i].avaliable) return false;
            auto* oa = levels_a[i].level.head;
            auto* ob = levels_b[i].level.head;
            while (oa && ob) {
                if (oa->order_id != ob->order_id) return false;
                if (oa->quantity != ob->quantity) return false;
                if (oa->account_id != ob->account_id) return false;
                oa = oa->next;
                ob = ob->next;
            }
            if (oa || ob) return false;
        }
        return true;
    };

    return compare_levels(a.bids(), b.bids()) && compare_levels(a.asks(), b.asks());
}

bool risk_states_equal(const RiskEngine& a, const RiskEngine& b, uint32_t acc_id) {
    const auto* acc_a = a.get_account(acc_id);
    const auto* acc_b = b.get_account(acc_id);
    if (!acc_a || !acc_b) return false;
    return acc_a->position == acc_b->position &&
           acc_a->open_buy_quantity == acc_b->open_buy_quantity &&
           acc_a->open_sell_quantity == acc_b->open_sell_quantity &&
           acc_a->credit_used == acc_b->credit_used &&
           acc_a->cash_flow == acc_b->cash_flow &&
           acc_a->realized_pnl == acc_b->realized_pnl;
}

TEST_CASE("Replay - no snapshot found") {
    fs::path dir = "./replay_empty";
    cleanup_dir(dir);
    fs::create_directories(dir / "snapshot");
    fs::create_directories(dir / "event_journal");

    EventBus bus;
    EventPublisher pub(bus);
    ExpiryScheduler sched;
    RiskEngine risk(1024);
    MatchingEngine engine(pub, sched, risk);

    BinaryReader reader(dir);
    ReplayEngine replayer(reader, engine, risk);
    
    replayer.replay(); 

    REQUIRE(engine.bids().empty());
    REQUIRE(engine.asks().empty());
    cleanup_dir(dir);
}

TEST_CASE("Replay - restore snapshot orders and accounts") {
    fs::path dir = "./replay_snapshot_only";
    cleanup_dir(dir);
    fs::create_directories(dir / "snapshot");
    fs::create_directories(dir / "event_journal");

    {
        SnapshotBatch batch;
        batch.sequence = 0;
        batch.snapshot_time = 1000;
        batch.last_trade_price = 100.0;
        
        batch.orders.push_back({1, 1, Side::Buy, 100.0, 10, 1, Type::GTC, 0});
        batch.orders.push_back({2, 2, Side::Sell, 101.0, 20, 2, Type::GTC, 0});
        
        batch.accounts.push_back({1, 10, 10, 0, 100000, -500, -1000});
        batch.accounts.push_back({2, -20, 0, 20, 0, 200, 2000});

        SnapshotBinaryWriter writer(dir);
        writer.process_snapshot(batch);
    }

    {
        EventBus bus;
        EventPublisher pub(bus);
        ExpiryScheduler sched;
        RiskEngine risk(1024);
        setup_risk_accounts(risk); 
        MatchingEngine engine(pub, sched, risk);

        BinaryReader reader(dir);
        ReplayEngine replayer(reader, engine, risk);
        replayer.replay();

        REQUIRE(engine.bids().size() == 1);
        REQUIRE(engine.asks().size() == 1);
        REQUIRE(engine.bids()[0].level.head->order_id == 1);
        REQUIRE(engine.asks()[0].level.head->order_id == 2);

        const auto* acc1 = risk.get_account(1);
        REQUIRE(acc1->position == 10);
        REQUIRE(acc1->open_buy_quantity == 10);
        REQUIRE(acc1->credit_used == 100000);
        REQUIRE(acc1->cash_flow == -1000);

        const auto* acc2 = risk.get_account(2);
        REQUIRE(acc2->position == -20);
        REQUIRE(acc2->open_sell_quantity == 20);
        REQUIRE(acc2->realized_pnl == 200);
    }

    cleanup_dir(dir);
}

TEST_CASE("Replay - Trade event applies to book and risk engine") {
    fs::path dir = "./replay_trade";
    cleanup_dir(dir);
    fs::create_directories(dir / "snapshot");
    fs::create_directories(dir / "event_journal");

    {
        SnapshotBatch batch;
        batch.sequence = 0;
        batch.last_trade_price = 100.0;
        batch.orders.push_back({1, 1, Side::Buy, 100.0, 10, 1, Type::GTC, 0});
        batch.orders.push_back({2, 2, Side::Sell, 100.0, 10, 2, Type::GTC, 0});
        batch.accounts.push_back({1, 0, 10, 0, 100000, 0, 0});
        batch.accounts.push_back({2, 0, 0, 10, 0, 0, 0});
        SnapshotBinaryWriter writer(dir);
        writer.process_snapshot(batch);

        std::ofstream f(dir / "event_journal" / "event_journal_1.bin", std::ios::binary);
        uint16_t type = static_cast<uint16_t>(EventType::Trade);
        f.write(reinterpret_cast<const char*>(&type), sizeof(type));
        
        struct TradeData {
            EventType event_type;
            uint64_t timestamp_ns;
            uint64_t buy_order_id;
            uint64_t sell_order_id;
            uint64_t account_id; 
            double price;
            uint64_t quantity;
        } td{EventType::Trade, 2000, 1, 2, 2, 100.0, 5};
        f.write(reinterpret_cast<const char*>(&td), sizeof(td));
    }

    {
        EventBus bus;
        EventPublisher pub(bus);
        ExpiryScheduler sched;
        RiskEngine risk(1024);
        setup_risk_accounts(risk);
        MatchingEngine engine(pub, sched, risk);

        BinaryReader reader(dir);
        ReplayEngine replayer(reader, engine, risk);
        replayer.replay();

        REQUIRE(engine.bids()[0].avaliable == 5);
        REQUIRE(engine.asks()[0].avaliable == 5);
        REQUIRE(engine.bids()[0].level.head->quantity == 5);
        REQUIRE(engine.asks()[0].level.head->quantity == 5);

        const auto* acc1 = risk.get_account(1); 
        REQUIRE(acc1->position == 5);
        REQUIRE(acc1->open_buy_quantity == 5);
        REQUIRE(acc1->credit_used == 50000);
        REQUIRE(acc1->cash_flow == -50000); 

        const auto* acc2 = risk.get_account(2); 
        REQUIRE(acc2->position == -5);
        REQUIRE(acc2->open_sell_quantity == 5);
        REQUIRE(acc2->cash_flow == 50000);
    }
    cleanup_dir(dir);
}

TEST_CASE("Replay - Cancel event removes order and releases risk") {
    fs::path dir = "./replay_cancel";
    cleanup_dir(dir);
    fs::create_directories(dir / "snapshot");
    fs::create_directories(dir / "event_journal");

    {
        SnapshotBatch batch;
        batch.sequence = 0;
        batch.last_trade_price = 100.0;
        batch.orders.push_back({1, 1, Side::Buy, 100.0, 10, 1, Type::GTC, 0});
        batch.accounts.push_back({1, 0, 10, 0, 100000, 0, 0}); 
        SnapshotBinaryWriter writer(dir);
        writer.process_snapshot(batch);

        std::ofstream f(dir / "event_journal" / "event_journal_1.bin", std::ios::binary);
        uint16_t type = static_cast<uint16_t>(EventType::OrderCancelled);
        f.write(reinterpret_cast<const char*>(&type), sizeof(type));
        
        struct CancelData {
            EventType event_type;
            uint64_t timestamp_ns;
            uint64_t order_id;
            uint64_t account_id;
            uint16_t reason;
        } cd{EventType::OrderCancelled, 2000, 1, 1, static_cast<uint16_t>(CancelReason::UserRequest)};
        f.write(reinterpret_cast<const char*>(&cd), sizeof(cd));
    }

    {
        EventBus bus;
        EventPublisher pub(bus);
        ExpiryScheduler sched;
        RiskEngine risk(1024);
        setup_risk_accounts(risk);
        MatchingEngine engine(pub, sched, risk);

        BinaryReader reader(dir);
        ReplayEngine replayer(reader, engine, risk);
        replayer.replay();

        REQUIRE(engine.bids().empty());

        const auto* acc1 = risk.get_account(1);
        REQUIRE(acc1->position == 0);
        REQUIRE(acc1->open_buy_quantity == 0);
        REQUIRE(acc1->credit_used == 0); 
    }
    cleanup_dir(dir);
}

TEST_CASE("Replay - End-to-End equivalence with Live Engine") {
    fs::path dir = "./replay_e2e_equiv";
    cleanup_dir(dir);
    fs::create_directories(dir / "snapshot");
    fs::create_directories(dir / "event_journal");

    EventBus bus1, bus2;
    EventPublisher pub1(bus1), pub2(bus2);
    ExpiryScheduler sched1, sched2;
    RiskEngine risk_live(1024), risk_replay(1024);
    setup_risk_accounts(risk_live);
    setup_risk_accounts(risk_replay);

    MatchingEngine live(pub1, sched1, risk_live);
    MatchingEngine replayed(pub2, sched2, risk_replay);

    live.process_order(make_order(1, 1, Side::Buy, 100.0, 10));
    live.process_order(make_order(2, 2, Side::Buy, 99.0, 20));
    live.process_order(make_order(3, 3, Side::Sell, 101.0, 30));

    {
        SnapshotBatch batch;
        batch.sequence = 0;
        batch.last_trade_price = 100.0;
        batch.orders.push_back({1, 1, Side::Buy, 100.0, 10, 1, Type::GTC, 0});
        batch.orders.push_back({2, 2, Side::Buy, 99.0, 20, 2, Type::GTC, 0});
        batch.orders.push_back({3, 3, Side::Sell, 101.0, 30, 3, Type::GTC, 0});
        batch.accounts.push_back({1, 0, 10, 0, 100000, 0, 0});
        batch.accounts.push_back({2, 0, 20, 0, 198000, 0, 0});
        batch.accounts.push_back({3, 0, 0, 30, 0, 0, 0});
        SnapshotBinaryWriter writer(dir);
        writer.process_snapshot(batch);
    }

    std::ofstream f(dir / "event_journal" / "event_journal_1.bin", std::ios::binary);

    live.process_order(make_order(4, 4, Side::Sell, 102.0, 40));
    uint16_t type_order = static_cast<uint16_t>(EventType::OrderAccepted);
    f.write(reinterpret_cast<const char*>(&type_order), sizeof(type_order));
    struct OrderData {
        EventType event_type; uint64_t timestamp_ns; uint64_t order_id; uint64_t account_id;
        Side side; Type type; double price; uint32_t quantity; uint64_t sequence; uint64_t expire_time;
    } od{EventType::OrderAccepted, 2000, 4, 4, Side::Sell, Type::GTC, 102.0, 40, 4, 0};
    f.write(reinterpret_cast<const char*>(&od), sizeof(od));

    live.process_order(make_order(5, 5, Side::Sell, 100.0, 10));
    uint16_t type_trade = static_cast<uint16_t>(EventType::Trade);
    f.write(reinterpret_cast<const char*>(&type_trade), sizeof(type_trade));
    struct TradeData {
        EventType event_type; uint64_t timestamp_ns; uint64_t buy_order_id; uint64_t sell_order_id;
        uint64_t account_id; double price; uint64_t quantity;
    } td{EventType::Trade, 2100, 1, 5, 5, 100.0, 10};
    f.write(reinterpret_cast<const char*>(&td), sizeof(td));

    live.cancel_order(2, 2, CancelReason::UserRequest);
    uint16_t type_cancel = static_cast<uint16_t>(EventType::OrderCancelled);
    f.write(reinterpret_cast<const char*>(&type_cancel), sizeof(type_cancel));
    struct CancelData {
        EventType event_type; uint64_t timestamp_ns; uint64_t order_id; uint64_t account_id; uint16_t reason;
    } cd{EventType::OrderCancelled, 2200, 2, 2, static_cast<uint16_t>(CancelReason::UserRequest)};
    f.write(reinterpret_cast<const char*>(&cd), sizeof(cd));
    f.close();

    BinaryReader reader(dir);
    ReplayEngine replayer(reader, replayed, risk_replay);
    replayer.replay();

    REQUIRE(books_equal(live, replayed));
    
    REQUIRE(risk_states_equal(risk_live, risk_replay, 1)); 
    REQUIRE(risk_states_equal(risk_live, risk_replay, 2)); 
    REQUIRE(risk_states_equal(risk_live, risk_replay, 3)); 
    REQUIRE(risk_states_equal(risk_live, risk_replay, 4)); 
    REQUIRE(risk_states_equal(risk_live, risk_replay, 5)); 
    
    cleanup_dir(dir);
}