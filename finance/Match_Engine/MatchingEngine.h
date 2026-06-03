#pragma once

// #include "PriceLevel.h"
// #include "Trade.h"
// #include "TradeFormatter.h"
// #include "MarketDataEvent.h"
#include "Clock.h"
#include "LatencyCollector.h"
#include "../Trade_Event/TradeEventListener.h"
#include "../Trade_Event/TradeLogger.h"
#include "../Trade_Event/TradeDataPublisher.h"
#include "../Memory_Pool/LRU.h"
#include "../Async_Logger/AsyncLogger.h"
#include "../CPU_Affinity/Affinity.h"

#include <map>
#include <deque>
#include <vector>
#include <unordered_map>
#include <iostream>

class MatchingEngine {
public:

    void submit_order(const Order& order);

    const auto& bids() const { return listener_->bids(); }

    const auto& asks() const { return listener_->asks(); }

    const auto& trades() const {return trades_;}

    const auto& order_lookup() const {return order_lookup_;}

    const auto& market_data_events() const {return listener_->market_data_events();}

    const auto& latency_collector() const {return latency_collector_;}

    const auto& trade_count() const {return trade_count_;}

    void set_listener(TradeEventListener* listener) {listener_ = listener;};

    void set_logger(AsyncLogger* logger) {listener_->set_logger(logger);}

    // void publish_market_data(const Trade& trade);

    bool cancel_order(uint64_t order_id);

private:

    // std::map<double, PriceLevel, std::greater<>> bids_;

    // std::map<double, PriceLevel> asks_;

    std::vector<Trade> trades_;

    // std::vector<MarketDataEvent> market_data_events_;

    // AsyncLogger* logger_ = nullptr;

    TradeEventListener* listener_ = nullptr;

    LatencyCollector latency_collector_;

    // LRUPool<Trade> trade_pool_{1024 * 1024};

    // MemoryPool<Order> order_pool_{1024 * 1024};

    std::unordered_map<uint64_t, Order*> order_lookup_;

    std::atomic<uint64_t> trade_count_{0};

    void match_buy(Order order);

    void match_sell(Order order);

    void add_to_book(const Order& order);

    void append_order(PriceLevel& level, Order* order);

    void remove_order(PriceLevel& level, Order* order);
};