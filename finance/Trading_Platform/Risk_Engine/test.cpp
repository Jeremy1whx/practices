#define CATCH_CONFIG_MAIN
#define CATCH_CONFIG_ENABLE_BENCHMARKING

#include "../Lock_Free_Ring_Buffer/mpsc/catch.hpp"
#include "RiskMonitor.h"

#include <cstring>
#include <vector>


Order make_order(uint64_t order_id, uint64_t account_id, Side side,
                 double price, uint32_t qty,
                 Type type = Type::GTC, uint64_t expire_time = 0) {
    Order order;
    std::memset(&order, 0, sizeof(order));
    order.order_id = order_id;
    order.account_id = account_id;
    order.side = side;
    order.type = type;
    order.price = price;
    order.quantity = qty;
    order.sequence = order_id;
    order.expire_time = expire_time;
    order.publish = false;
    return order;
}

exchange::Trade make_trade(uint64_t buy_id, uint64_t sell_id, double price, uint32_t qty) {
    exchange::Trade t;
    std::memset(&t, 0, sizeof(t));
    t.buy_order_id = buy_id;
    t.sell_order_id = sell_id;
    t.account_id = 9999;
    t.price = price;
    t.quantity = qty;
    return t;
}

RiskLimits default_limits() {
    RiskLimits l;
    l.max_order_size = 10000;
    l.max_orders_per_sec = 1000000; 
    l.max_position = 100000;
    l.max_credit = 100'000'000'00;
    l.max_daily_loss = 100'000'000'00;
    l.price_band_pct = 0.0;  
    l.stp_policy = STPPolicy::None;
    return l;
}

// credit_per_unit (cents)
int64_t cpu_of(double price) {
    return static_cast<int64_t>(price * 100.0 + 0.5);
}

TEST_CASE("RiskEngine credit invariant") {
    exchange::RiskEngine reng(1024);
    RiskLimits limits;
    limits.max_credit = 1'000'000'00;
    reng.register_account(limits);
    reng.set_limits(0, limits);

    Order o{};
    o.order_id = 1; o.account_id = 0; o.side = Side::Buy;
    o.price = 100.5; o.quantity = 10; o.type = Type::GTC;

    auto r = reng.pre_trade_check(o, now_ns());
    REQUIRE(r.passed);
    REQUIRE(reng.get_account(0)->credit_used == 10050 * 10);

    exchange::Trade t{}; t.buy_order_id = 1; t.sell_order_id = 99;
    t.price = 100.0; t.quantity = 4;
    reng.on_trade(t, now_ns());
    REQUIRE(reng.get_account(0)->credit_used == 10050 * 6); 

    reng.on_order_removed(1, 6, Side::Buy);
    REQUIRE(reng.get_account(0)->credit_used == 0); 
}

TEST_CASE("Register accounts: sequential IDs") {
    exchange::RiskEngine engine(1024);

    REQUIRE(engine.account_count() == 0);

    uint32_t id0 = engine.register_account(default_limits());
    uint32_t id1 = engine.register_account(default_limits());
    uint32_t id2 = engine.register_account(default_limits());

    REQUIRE(id0 == 0);
    REQUIRE(id1 == 1);
    REQUIRE(id2 == 2);
    REQUIRE(engine.account_count() == 3);

    const auto* a = engine.get_account(0);
    REQUIRE(a != nullptr);
    REQUIRE(a->position == 0);
    REQUIRE(a->open_buy_quantity == 0);
    REQUIRE(a->open_sell_quantity == 0);
    REQUIRE(a->credit_used == 0);
    REQUIRE(a->realized_pnl == 0);
    REQUIRE(a->status == 0);
}

TEST_CASE("Register accounts: overflow") {
    exchange::RiskEngine engine(2);
    REQUIRE(engine.register_account(default_limits()) != INVALID_INDEX);
    REQUIRE(engine.register_account(default_limits()) != INVALID_INDEX);
    REQUIRE(engine.register_account(default_limits()) == INVALID_INDEX);  
}

TEST_CASE("set_limits updates fields") {
    exchange::RiskEngine engine(16);
    uint32_t acc = engine.register_account(default_limits());

    RiskLimits l;
    l.max_order_size = 500;
    l.max_orders_per_sec = 100;
    l.max_position = 2000;
    l.max_credit = 5'000'000;
    l.max_daily_loss = 1'000'000;
    l.price_band_pct = 0.05;
    l.stp_policy = STPPolicy::CancelNewest;

    engine.set_limits(acc, l);

    const auto* a = engine.get_account(acc);
    REQUIRE(a->max_order_size == 500);
    REQUIRE(a->max_orders_per_sec == 100);
    REQUIRE(a->max_position == 2000);
    REQUIRE(a->max_credit == 5'000'000);
    REQUIRE(a->max_daily_loss == 1'000'000);
    REQUIRE(a->price_band_pct == 0.05);
    REQUIRE(engine.get_stp_policy(acc) == STPPolicy::CancelNewest);
}

TEST_CASE("get_account: out of bounds") {
    exchange::RiskEngine engine(16);
    engine.register_account(default_limits());
    REQUIRE(engine.get_account(1000) == nullptr);
    REQUIRE(engine.get_account(99) == nullptr);  
}

TEST_CASE("Order size limit") {
    exchange::RiskEngine engine(16);
    auto l = default_limits();
    l.max_order_size = 100;
    uint32_t acc = engine.register_account(l);

    auto too_big = make_order(1, acc, Side::Buy, 100.0, 101);
    auto r = engine.pre_trade_check(too_big, now_ns());
    REQUIRE_FALSE(r.passed);
    REQUIRE(r.reason == exchange::RiskViolationReason::OrderSizeLimitExceeded);

    REQUIRE(engine.get_account(acc)->open_buy_quantity == 0);

    auto ok = make_order(2, acc, Side::Buy, 100.0, 100);
    REQUIRE(engine.pre_trade_check(ok, now_ns()).passed);
}

TEST_CASE("Price band: rejected when deviation > limit") {
    exchange::RiskEngine engine(16);
    auto l = default_limits();
    l.price_band_pct = 0.10;   // 10%
    uint32_t acc = engine.register_account(l);

    {
        auto t = make_trade(999, 998, 100.0, 1);
        engine.on_trade(t, now_ns());
    }

    REQUIRE(engine.pre_trade_check(
        make_order(1, acc, Side::Buy, 105.0, 1), now_ns()).passed);

    auto r = engine.pre_trade_check(
        make_order(2, acc, Side::Buy, 111.0, 1), now_ns());
    REQUIRE_FALSE(r.passed);
    REQUIRE(r.reason == exchange::RiskViolationReason::PriceOutOfBand);
}

TEST_CASE("Position limit: long side") {
    exchange::RiskEngine engine(16);
    auto l = default_limits();
    l.max_position = 100;
    uint32_t acc = engine.register_account(l);

    REQUIRE(engine.pre_trade_check(
        make_order(1, acc, Side::Buy, 100.0, 80), now_ns()).passed);

    auto r = engine.pre_trade_check(
        make_order(2, acc, Side::Buy, 100.0, 30), now_ns());
    REQUIRE_FALSE(r.passed);
    REQUIRE(r.reason == exchange::RiskViolationReason::PositionLimitExceeded);

    REQUIRE(engine.pre_trade_check(
        make_order(3, acc, Side::Buy, 100.0, 20), now_ns()).passed);
}

TEST_CASE("Position limit: short side") {
    exchange::RiskEngine engine(16);
    auto l = default_limits();
    l.max_position = 100;
    uint32_t acc = engine.register_account(l);

    REQUIRE(engine.pre_trade_check(
        make_order(1, acc, Side::Sell, 100.0, 80), now_ns()).passed);

    auto r = engine.pre_trade_check(
        make_order(2, acc, Side::Sell, 100.0, 30), now_ns());
    REQUIRE_FALSE(r.passed);
    REQUIRE(r.reason == exchange::RiskViolationReason::PositionLimitExceeded);
}

TEST_CASE("Position limit: buy offsets short") {
    exchange::RiskEngine engine(16);
    auto l = default_limits();
    l.max_position = 100;
    uint32_t acc = engine.register_account(l);

    engine.pre_trade_check(make_order(1, acc, Side::Sell, 100.0, 80), now_ns());

    auto r = engine.pre_trade_check(
        make_order(2, acc, Side::Buy, 100.0, 200), now_ns());
    REQUIRE_FALSE(r.passed);

    REQUIRE(engine.pre_trade_check(
        make_order(3, acc, Side::Buy, 100.0, 180), now_ns()).passed);
}

TEST_CASE("Credit limit") {
    exchange::RiskEngine engine(16);
    auto l = default_limits();
    l.max_credit = 1000000; 
    uint32_t acc = engine.register_account(l);

    REQUIRE(engine.pre_trade_check(
        make_order(1, acc, Side::Buy, 100.0, 50), now_ns()).passed);

    REQUIRE(engine.pre_trade_check(
        make_order(2, acc, Side::Buy, 100.0, 50), now_ns()).passed);

    auto r = engine.pre_trade_check(
        make_order(3, acc, Side::Buy, 100.0, 1), now_ns());
    REQUIRE_FALSE(r.passed);
    REQUIRE(r.reason == exchange::RiskViolationReason::CreditLimitExceeded);
}

TEST_CASE("Credit: sell side not affected") {
    exchange::RiskEngine engine(16);
    auto l = default_limits();
    l.max_credit = 1000;
    uint32_t acc = engine.register_account(l);

    REQUIRE(engine.pre_trade_check(
        make_order(1, acc, Side::Sell, 1'000'000.0, 10000), now_ns()).passed);
    REQUIRE(engine.get_account(acc)->credit_used == 0);
}

TEST_CASE("Daily loss limit") {
    exchange::RiskEngine engine(16);
    auto l = default_limits();
    l.max_daily_loss = 100'000;
    uint32_t a = engine.register_account(l);
    uint32_t cp = engine.register_account(l);
    
    auto buy_a = make_order(1, a, Side::Buy, 100.0, 100);
    auto sell_cp = make_order(2, cp, Side::Sell, 100.0, 100);
    REQUIRE(engine.pre_trade_check(buy_a, now_ns()).passed);
    REQUIRE(engine.pre_trade_check(sell_cp, now_ns()).passed);

    auto buy_cp = make_order(3, cp, Side::Buy, 80.0, 100);
    auto sell_a = make_order(4, a, Side::Sell, 80.0, 100);
    REQUIRE(engine.pre_trade_check(sell_a, now_ns()).passed);
    REQUIRE(engine.pre_trade_check(buy_cp, now_ns()).passed);

    auto t1 = make_trade(1, 2, 100.0, 100);
    engine.on_trade(t1, now_ns());
    auto t2 = make_trade(3, 4, 80.0, 100);
    engine.on_trade(t2, now_ns());

    REQUIRE(engine.get_account(a)->position == 0);
    REQUIRE(engine.get_account(a)->realized_pnl == -200'000);
    REQUIRE(engine.get_account(a)->cash_flow == 0);

    auto buy2 = make_order(3, a, Side::Buy, 100.0, 1);
    auto r = engine.pre_trade_check(buy2, now_ns());
    REQUIRE_FALSE(r.passed);
    REQUIRE(r.reason == exchange::RiskViolationReason::DailyLossLimitExceeded);
}

TEST_CASE("Daily loss: accumulates across multiple round trips") {
    exchange::RiskEngine engine(16);
    auto l = default_limits();
    l.max_daily_loss = 1'000'000;
    uint32_t a = engine.register_account(l);
    uint32_t cp = engine.register_account(l);

    uint64_t oid = 1;

    auto run_round = [&](double open_price, double close_price, uint32_t qty) {
        auto b1 = make_order(oid++, a,  Side::Buy,  open_price,  qty);
        auto s1 = make_order(oid++, cp, Side::Sell, open_price,  qty);
        REQUIRE(engine.pre_trade_check(b1, now_ns()).passed);
        REQUIRE(engine.pre_trade_check(s1, now_ns()).passed);
        engine.on_trade(make_trade(b1.order_id, s1.order_id, open_price, qty), now_ns());

        auto s2 = make_order(oid++, a,  Side::Sell, close_price, qty);
        auto b2 = make_order(oid++, cp, Side::Buy,  close_price, qty);
        REQUIRE(engine.pre_trade_check(s2, now_ns()).passed);
        REQUIRE(engine.pre_trade_check(b2, now_ns()).passed);
        engine.on_trade(make_trade(b2.order_id, s2.order_id, close_price, qty), now_ns());
    };

    run_round(100.0, 90.0, 100);
    REQUIRE(engine.get_account(a)->position == 0);
    REQUIRE(engine.get_account(a)->realized_pnl == -100'000);
    REQUIRE(engine.get_account(cp)->realized_pnl == +100'000);

    run_round(100.0, 85.0, 100);
    REQUIRE(engine.get_account(a)->realized_pnl == -250'000);

    run_round(100.0, 75.0, 100);
    REQUIRE(engine.get_account(a)->realized_pnl == -500'000);

    run_round(100.0, 50.0, 100);
    REQUIRE(engine.get_account(a)->realized_pnl == -1'000'000);

    auto probe1 = make_order(oid++, a, Side::Buy, 100.0, 1);
    REQUIRE(engine.pre_trade_check(probe1, now_ns()).passed);  

    run_round(100.0, 90.0, 100);
    REQUIRE(engine.get_account(a)->realized_pnl == -1'100'000);

    auto probe2 = make_order(oid++, a, Side::Buy, 100.0, 1);
    auto r2 = engine.pre_trade_check(probe2, now_ns());
    REQUIRE_FALSE(r2.passed);
    REQUIRE(r2.reason == exchange::RiskViolationReason::DailyLossLimitExceeded);

    auto probe3 = make_order(oid++, a, Side::Sell, 100.0, 1);
    auto r3 = engine.pre_trade_check(probe3, now_ns());
    REQUIRE_FALSE(r3.passed);
    REQUIRE(r3.reason == exchange::RiskViolationReason::DailyLossLimitExceeded);
}

TEST_CASE("Daily loss: short position realized loss") {
    exchange::RiskEngine engine(16);
    auto l = default_limits();
    l.max_daily_loss = 1'000'000;
    uint32_t a = engine.register_account(l);
    uint32_t cp  = engine.register_account(l);

    auto sell_a = make_order(1, a,  Side::Sell, 100.0, 100);
    auto buy_cp = make_order(2, cp, Side::Buy,  100.0, 100);
    REQUIRE(engine.pre_trade_check(sell_a, now_ns()).passed);
    REQUIRE(engine.pre_trade_check(buy_cp, now_ns()).passed);
    engine.on_trade(make_trade(2, 1, 100.0, 100), now_ns());

    REQUIRE(engine.get_account(a)->position == -100);

    auto buy_a   = make_order(3, a,  Side::Buy,  110.0, 100);
    auto sell_cp = make_order(4, cp, Side::Sell, 110.0, 100);
    REQUIRE(engine.pre_trade_check(buy_a, now_ns()).passed);
    REQUIRE(engine.pre_trade_check(sell_cp, now_ns()).passed);
    engine.on_trade(make_trade(3, 4, 110.0, 100), now_ns());

    REQUIRE(engine.get_account(a)->position == 0);
    REQUIRE(engine.get_account(a)->realized_pnl == -100'000);
    REQUIRE(engine.get_account(a)->cash_flow == 0);

    REQUIRE(engine.get_account(cp)->position == 0);
    REQUIRE(engine.get_account(cp)->realized_pnl == +100'000);
}

TEST_CASE("Account suspended") {
    exchange::RiskEngine engine(16);
    uint32_t acc = engine.register_account(default_limits());

    engine.set_circuit_breaker(acc, true);
    REQUIRE(engine.is_circuit_broken(acc));

    engine.set_circuit_breaker(acc, false);
    REQUIRE_FALSE(engine.is_circuit_broken(acc));
}

TEST_CASE("Unknown account") {
    exchange::RiskEngine engine(16);
    auto r = engine.pre_trade_check(
        make_order(1, 999, Side::Buy, 100.0, 1), now_ns());
    REQUIRE_FALSE(r.passed);
    REQUIRE(r.reason == exchange::RiskViolationReason::UnknownAccount);
    REQUIRE(engine.total_checks() == 1);
    REQUIRE(engine.total_rejects() == 1);
}


TEST_CASE("Credit invariant: full lifecycle (partial fills + cancel)") {
    exchange::RiskEngine engine(16);
    uint32_t acc = engine.register_account(default_limits());

    const double price = 100.55;
    const int64_t cpu  = cpu_of(price); 
    const uint32_t qty = 10;

    auto order = make_order(1, acc, Side::Buy, price, qty);
    REQUIRE(engine.pre_trade_check(order, now_ns()).passed);
    REQUIRE(engine.get_account(acc)->credit_used == cpu * qty);
    REQUIRE(engine.get_account(acc)->open_buy_quantity == qty);

    auto t1 = make_trade(1, 999, 100.00, 4);
    engine.on_trade(t1, now_ns());
    REQUIRE(engine.get_account(acc)->credit_used == cpu * 6);
    REQUIRE(engine.get_account(acc)->open_buy_quantity == 6);
    REQUIRE(engine.get_account(acc)->position == 4);

    auto t2 = make_trade(1, 998, 99.00, 3);
    engine.on_trade(t2, now_ns());
    REQUIRE(engine.get_account(acc)->credit_used == cpu * 3);
    REQUIRE(engine.get_account(acc)->open_buy_quantity == 3);
    REQUIRE(engine.get_account(acc)->position == 7);

    engine.on_order_removed(1, 3, Side::Buy);
    REQUIRE(engine.get_account(acc)->credit_used == 0); 
    REQUIRE(engine.get_account(acc)->open_buy_quantity == 0);
    REQUIRE(engine.get_account(acc)->position == 7);
}

TEST_CASE("Credit invariant: taker fills at better price") {
    exchange::RiskEngine engine(16);
    uint32_t acc = engine.register_account(default_limits());

    const double taker_price = 100.50;
    const int64_t cpu = cpu_of(taker_price);   

    auto taker = make_order(1, acc, Side::Buy, taker_price, 10);
    REQUIRE(engine.pre_trade_check(taker, now_ns()).passed);
    REQUIRE(engine.get_account(acc)->credit_used == cpu * 10);

    auto t = make_trade(1, 999, 100.00, 10);
    engine.on_trade(t, now_ns());

    REQUIRE(engine.get_account(acc)->credit_used == 0);
    REQUIRE(engine.get_account(acc)->open_buy_quantity == 0);
    REQUIRE(engine.get_account(acc)->position == 10);
}

TEST_CASE("Credit invariant: many sequential orders") {
    exchange::RiskEngine engine(16);
    uint32_t acc = engine.register_account(default_limits());

    for (uint64_t i = 1; i <= 100; ++i) {
        double price = 100.0 + (i % 10) * 0.01;
        uint32_t qty = static_cast<uint32_t>(i % 5 + 1);

        auto o = make_order(i, acc, Side::Buy, price, qty);
        REQUIRE(engine.pre_trade_check(o, now_ns()).passed);

        uint32_t traded = qty / 2;
        if (traded > 0) {
            auto t = make_trade(i, 900 + i, price - 0.01, traded);
            engine.on_trade(t, now_ns());
        }
        uint32_t remaining = qty - traded;
        if (remaining > 0) {
            engine.on_order_removed(i, remaining, Side::Buy);
        }
    }

    REQUIRE(engine.get_account(acc)->credit_used == 0);
    REQUIRE(engine.get_account(acc)->open_buy_quantity == 0);
}

TEST_CASE("Credit invariant: release_reservation fully rolls back") {
    exchange::RiskEngine engine(16);
    uint32_t acc = engine.register_account(default_limits());

    auto o = make_order(1, acc, Side::Buy, 100.0, 10);
    REQUIRE(engine.pre_trade_check(o, now_ns()).passed);

    int64_t before = engine.get_account(acc)->credit_used;
    REQUIRE(before > 0);

    engine.release_reservation(o);

    REQUIRE(engine.get_account(acc)->credit_used == 0);
    REQUIRE(engine.get_account(acc)->open_buy_quantity == 0);
}

TEST_CASE("on_trade: buy side updates position and open quantity") {
    exchange::RiskEngine engine(16);
    uint32_t buyer  = engine.register_account(default_limits());
    uint32_t seller = engine.register_account(default_limits());

    auto b = make_order(1, buyer,  Side::Buy,  100.0, 10);
    auto s = make_order(2, seller, Side::Sell, 100.0, 10);
    REQUIRE(engine.pre_trade_check(b, now_ns()).passed);
    REQUIRE(engine.pre_trade_check(s, now_ns()).passed);

    REQUIRE(engine.get_account(buyer)->open_buy_quantity  == 10);
    REQUIRE(engine.get_account(seller)->open_sell_quantity == 10);

    auto t = make_trade(1, 2, 100.0, 10);
    engine.on_trade(t, now_ns());

    REQUIRE(engine.get_account(buyer)->position           ==  10);
    REQUIRE(engine.get_account(buyer)->open_buy_quantity  ==   0);
    REQUIRE(engine.get_account(buyer)->credit_used        ==   0);

    REQUIRE(engine.get_account(seller)->position          == -10);
    REQUIRE(engine.get_account(seller)->open_sell_quantity ==  0);
}

TEST_CASE("on_trade: taker has no lookup -> no state change") {
    exchange::RiskEngine engine(16);
    uint32_t acc = engine.register_account(default_limits());

    auto t = make_trade(999, 998, 100.0, 5);
    engine.on_trade(t, now_ns());

    REQUIRE(engine.get_account(acc)->position == 0);
}

TEST_CASE("Taker fully filled: second on_trade has no effect") {
    exchange::RiskEngine engine(16);
    uint32_t acc = engine.register_account(default_limits());

    auto taker = make_order(1, acc, Side::Buy, 100.0, 5);
    REQUIRE(engine.pre_trade_check(taker, now_ns()).passed);

    auto t = make_trade(1, 999, 100.0, 5);
    engine.on_trade(t, now_ns());
    engine.on_order_fully_filled(1);

    REQUIRE(engine.get_account(acc)->position == 5);
    REQUIRE(engine.get_account(acc)->credit_used == 0);

    engine.on_trade(t, now_ns());

    REQUIRE(engine.get_account(acc)->position == 5);
}

TEST_CASE("on_order_removed: releases remaining quantity") {
    exchange::RiskEngine engine(16);
    uint32_t acc = engine.register_account(default_limits());

    auto o = make_order(1, acc, Side::Buy, 100.0, 10);
    REQUIRE(engine.pre_trade_check(o, now_ns()).passed);

    engine.on_trade(make_trade(1, 999, 100.0, 3), now_ns());

    engine.on_order_removed(1, 7, Side::Buy);

    REQUIRE(engine.get_account(acc)->open_buy_quantity == 0);
    REQUIRE(engine.get_account(acc)->credit_used == 0);
    REQUIRE(engine.get_account(acc)->position == 3);
}


TEST_CASE("STP policy: default is None") {
    exchange::RiskEngine engine(16);
    uint32_t acc = engine.register_account(default_limits());
    REQUIRE(engine.get_stp_policy(acc) == STPPolicy::None);
}

TEST_CASE("STP policy: CancelNewest") {
    exchange::RiskEngine engine(16);
    auto l = default_limits();
    l.stp_policy = STPPolicy::CancelNewest;
    uint32_t acc = engine.register_account(l);
    REQUIRE(engine.get_stp_policy(acc) == STPPolicy::CancelNewest);
}


TEST_CASE("Circuit breaker: set/clear") {
    exchange::RiskEngine engine(16);
    uint32_t a = engine.register_account(default_limits());
    uint32_t b = engine.register_account(default_limits());

    REQUIRE_FALSE(engine.is_circuit_broken(a));

    engine.set_circuit_breaker(a, true);
    REQUIRE(engine.is_circuit_broken(a));
    REQUIRE_FALSE(engine.is_circuit_broken(b));

    engine.set_circuit_breaker(a, false);
    REQUIRE_FALSE(engine.is_circuit_broken(a));
}

TEST_CASE("Circuit breaker: out of bounds safe") {
    exchange::RiskEngine engine(16);
    REQUIRE_FALSE(engine.is_circuit_broken(999));
    REQUIRE_NOTHROW(engine.set_circuit_breaker(999, true));
}

TEST_CASE("Liquidation queue: FIFO order") {
    exchange::RiskEngine engine(16);
    uint32_t a = engine.register_account(default_limits());
    uint32_t b = engine.register_account(default_limits());

    engine.request_liquidation(a);
    engine.request_liquidation(b);
    engine.request_liquidation(a);

    uint64_t id;
    REQUIRE(engine.pop_liquidation_request(id));
    REQUIRE(id == a);
    REQUIRE(engine.pop_liquidation_request(id));
    REQUIRE(id == b);
    REQUIRE(engine.pop_liquidation_request(id));
    REQUIRE(id == a);
    REQUIRE_FALSE(engine.pop_liquidation_request(id));  
}

TEST_CASE("Circuit breaker: daily loss triggers liquidation") {
    exchange::RiskEngine risk(16);
    auto l = default_limits();
    l.max_daily_loss = 100'000;
    uint32_t a  = risk.register_account(l);
    uint32_t cp = risk.register_account(l);

    auto b1 = make_order(1, a,  Side::Buy,  100.0, 100);
    auto s1 = make_order(2, cp, Side::Sell, 100.0, 100);
    risk.pre_trade_check(b1, now_ns());
    risk.pre_trade_check(s1, now_ns());
    risk.on_trade(make_trade(1, 2, 100.0, 100), now_ns());

    auto s2 = make_order(3, a,  Side::Sell, 80.0, 100);
    auto b2 = make_order(4, cp, Side::Buy,  80.0, 100);
    risk.pre_trade_check(s2, now_ns());
    risk.pre_trade_check(b2, now_ns());
    risk.on_trade(make_trade(4, 3, 80.0, 100), now_ns());

    REQUIRE(risk.get_account(a)->position == 0);
    REQUIRE(risk.get_account(a)->realized_pnl == -200'000);

    exchange::RiskMonitor monitor(risk);
    monitor.check_once();

    REQUIRE(risk.is_circuit_broken(a));

    uint64_t id;
    REQUIRE(risk.pop_liquidation_request(id));
    REQUIRE(id == a);
}


TEST_CASE("Export / restore round-trip") {
    exchange::RiskEngine engine(16);
    uint32_t a = engine.register_account(default_limits());
    uint32_t b = engine.register_account(default_limits());

    engine.pre_trade_check(make_order(1, a, Side::Buy, 100.0, 10), now_ns());
    engine.pre_trade_check(make_order(2, b, Side::Sell, 101.0, 20), now_ns());
    engine.on_trade(make_trade(1, 2, 100.0, 5), now_ns());

    std::vector<exchange::AccountSnapshot> snapshots;
    engine.export_accounts(snapshots);
    REQUIRE(snapshots.size() == 2);

    exchange::RiskEngine engine2(16);
    engine2.register_account(default_limits());   // id 0
    engine2.register_account(default_limits());   // id 1
    engine2.restore(snapshots, 100.0);

    const auto* a2 = engine2.get_account(a);
    const auto* b2 = engine2.get_account(b);

    REQUIRE(a2->position == 5);
    REQUIRE(a2->open_buy_quantity == 5);
    REQUIRE(a2->credit_used > 0);

    REQUIRE(b2->position == -5);
    REQUIRE(b2->open_sell_quantity == 15);

    REQUIRE(engine2.last_trade_price() == 100.0);
}

TEST_CASE("Export: skip empty accounts") {
    exchange::RiskEngine engine(16);
    engine.register_account(default_limits());
    engine.register_account(default_limits());

    std::vector<exchange::AccountSnapshot> out;
    engine.export_accounts(out);
    REQUIRE(out.empty());  

    engine.pre_trade_check(make_order(1, 0, Side::Buy, 100.0, 10), now_ns());
    engine.export_accounts(out);
    REQUIRE(out.size() == 1);
    REQUIRE(out[0].account_id == 0);
}

TEST_CASE("Replay: accepted + trade produces same state as live") {
    exchange::RiskEngine live(16);
    exchange::RiskEngine replay(16);
    for (int i = 0; i < 4; ++i) {
        live.register_account(default_limits());
        replay.register_account(default_limits());
    }

    struct Step {
        uint64_t order_id;
        uint32_t account_id;
        Side side;
        double price;
        uint32_t qty;
        bool trade;
        uint32_t traded_qty;
    };
    std::vector<Step> steps = {
        {1, 0, Side::Buy,  100.0, 10, true,  4},
        {2, 1, Side::Sell, 100.0, 10, true,  4},
        {3, 2, Side::Buy,  100.5, 20, false, 0},
        {4, 3, Side::Sell, 99.5,  5,  true,  5},
    };

    for (const auto& s : steps) {
        auto o = make_order(s.order_id, s.account_id, s.side, s.price, s.qty);
        auto r = live.pre_trade_check(o, now_ns());
        REQUIRE(r.passed);
        
        if (s.trade) {
            auto t = make_trade(s.side == Side::Buy ? s.order_id : 900 + s.order_id,
                                s.side == Side::Buy ? 900 + s.order_id : s.order_id,
                                s.price, s.traded_qty);
            t.account_id = s.account_id; 
            
            live.on_trade(t, now_ns());
            
            replay.apply_replay_accepted(o, now_ns());
            replay.apply_replay_trade(t, now_ns());
        } else {
            replay.apply_replay_accepted(o, now_ns());
        }
    }

    for (uint32_t i = 0; i < 4; ++i) {
        const auto* l = live.get_account(i);
        const auto* r = replay.get_account(i);
        REQUIRE(l->position == r->position);
        REQUIRE(l->open_buy_quantity == r->open_buy_quantity);
        REQUIRE(l->open_sell_quantity == r->open_sell_quantity);
        REQUIRE(l->credit_used == r->credit_used);
        REQUIRE(l->cash_flow == r->cash_flow);
    }
}

TEST_CASE("Replay: multi-step consistency") {
    exchange::RiskEngine live(16);
    exchange::RiskEngine replay(16);
    for (int i = 0; i < 4; ++i) {
        live.register_account(default_limits());
        replay.register_account(default_limits());
    }

    struct Step {
        uint64_t order_id;
        uint32_t account_id;
        Side side;
        double price;
        uint32_t qty;
        bool trade;
        uint32_t traded_qty;
    };
    std::vector<Step> steps = {
        {1, 0, Side::Buy,  100.0, 10, true,  4},
        {2, 1, Side::Sell, 100.0, 10, true,  4},
        {3, 2, Side::Buy,  100.5, 20, false, 0},
        {4, 3, Side::Sell, 99.5,  5,  true,  5},
    };

    for (const auto& s : steps) {
        auto o = make_order(s.order_id, s.account_id, s.side, s.price, s.qty);

        auto r = live.pre_trade_check(o, now_ns());
        REQUIRE(r.passed);
        if (s.trade) {
            auto t = make_trade(s.side == Side::Buy ? s.order_id : 900 + s.order_id,
                                s.side == Side::Buy ? 900 + s.order_id : s.order_id,
                                s.price, s.traded_qty);
            live.on_trade(t, now_ns());

            replay.apply_replay_accepted(o, now_ns());
            replay.apply_replay_trade(t, now_ns());
        } else {
            replay.apply_replay_accepted(o, now_ns());
        }
    }

    for (uint32_t i = 0; i < 4; ++i) {
        const auto* l = live.get_account(i);
        const auto* r = replay.get_account(i);
        REQUIRE(l->position           == r->position);
        REQUIRE(l->open_buy_quantity  == r->open_buy_quantity);
        REQUIRE(l->open_sell_quantity == r->open_sell_quantity);
        REQUIRE(l->credit_used        == r->credit_used);
    }
}

TEST_CASE("Replay: clear_state resets everything") {
    exchange::RiskEngine engine(16);
    uint32_t a = engine.register_account(default_limits());
    engine.pre_trade_check(make_order(1, a, Side::Buy, 100.0, 10), now_ns());
    engine.on_trade(make_trade(1, 999, 100.0, 5), now_ns());

    REQUIRE(engine.get_account(a)->position == 5);
    REQUIRE(engine.get_account(a)->open_buy_quantity == 5);

    engine.clear_state();

    REQUIRE(engine.get_account(a)->position == 0);
    REQUIRE(engine.get_account(a)->open_buy_quantity == 0);
    REQUIRE(engine.get_account(a)->credit_used == 0);
    REQUIRE(engine.last_trade_price() == 0.0);

    REQUIRE(engine.account_count() >= 1);
}


TEST_CASE("Invariant: open quantities never negative") {
    exchange::RiskEngine engine(16);
    uint32_t a = engine.register_account(default_limits());

    for (uint64_t i = 1; i <= 20; ++i) {
        auto o = make_order(i, a, Side::Buy, 100.0, 10);
        engine.pre_trade_check(o, now_ns());

        engine.on_trade(make_trade(i, 900 + i, 100.0, 3), now_ns());

        engine.on_order_removed(i, 7, Side::Buy);

        REQUIRE(engine.get_account(a)->open_buy_quantity >= 0);
        REQUIRE(engine.get_account(a)->credit_used >= 0);
    }
}

TEST_CASE("Invariant: credit is zero after all orders cancelled") {
    exchange::RiskEngine engine(16);
    uint32_t a = engine.register_account(default_limits());

    for (uint64_t i = 1; i <= 50; ++i) {
        auto o = make_order(i, a, Side::Buy, 100.0 + i * 0.01, 5);
        REQUIRE(engine.pre_trade_check(o, now_ns()).passed);
        engine.on_order_removed(i, 5, Side::Buy); 
    }

    REQUIRE(engine.get_account(a)->credit_used == 0);
    REQUIRE(engine.get_account(a)->open_buy_quantity == 0);
}

TEST_CASE("Invariant: credit is zero after all orders fully filled") {
    exchange::RiskEngine engine(16);
    uint32_t a = engine.register_account(default_limits());

    for (uint64_t i = 1; i <= 50; ++i) {
        double price = 100.0 + i * 0.01;
        auto o = make_order(i, a, Side::Buy, price, 5);
        REQUIRE(engine.pre_trade_check(o, now_ns()).passed);

        auto t = make_trade(i, 900 + i, price, 5);
        engine.on_trade(t, now_ns());

        engine.on_order_fully_filled(i);
    }

    REQUIRE(engine.get_account(a)->credit_used == 0);
    REQUIRE(engine.get_account(a)->open_buy_quantity == 0);
    REQUIRE(engine.get_account(a)->position == 250);
}

TEST_CASE("Invariant: rejected orders leave no trace") {
    exchange::RiskEngine engine(16);
    auto l = default_limits();
    l.max_order_size = 100;
    uint32_t a = engine.register_account(l);

    for (uint64_t i = 1; i <= 10; ++i) {
        auto o = make_order(i, a, Side::Buy, 100.0, 999);
        auto r = engine.pre_trade_check(o, now_ns());
        REQUIRE_FALSE(r.passed);
    }

    REQUIRE(engine.get_account(a)->open_buy_quantity == 0);
    REQUIRE(engine.get_account(a)->credit_used == 0);
    REQUIRE(engine.total_rejects() == 10);
}

TEST_CASE("Benchmark: pre_trade_check") {
    exchange::RiskEngine engine(1024);
    uint32_t a = engine.register_account(default_limits());

    constexpr int N = 1'000'000;
    auto o = make_order(1, a, Side::Buy, 100.0, 10);

    auto t0 = std::chrono::steady_clock::now();
    for (int i = 0; i < N; ++i) {
        o.order_id = static_cast<uint64_t>(i + 1);
        auto r = engine.pre_trade_check(o, now_ns());
        engine.release_reservation(o);
    }
    auto t1 = std::chrono::steady_clock::now();

    double ns = std::chrono::duration<double, std::nano>(t1 - t0).count() / N;
    std::cout << "\npre_trade_check: " << ns << " ns/op\n";
    REQUIRE(ns < 1000.0);
}

TEST_CASE("Benchmark: on_trade") {
    exchange::RiskEngine engine(1024);
    uint32_t a = engine.register_account(default_limits());

    constexpr int N = 1'000'000;

    auto t0 = std::chrono::steady_clock::now();
    for (int i = 0; i < N; ++i) {
        auto o = make_order(i + 1, a, Side::Buy, 100.0, 1);
        engine.pre_trade_check(o, now_ns());
        auto t = make_trade(static_cast<uint64_t>(i + 1), 999, 100.0, 1);
        engine.on_trade(t, now_ns());
        engine.on_order_fully_filled(i + 1);
    }
    auto t1 = std::chrono::steady_clock::now();

    double ns = std::chrono::duration<double, std::nano>(t1 - t0).count() / N;
    std::cout << "\non_trade: " << ns << " ns/op\n";
    REQUIRE(ns < 1000.0);
}