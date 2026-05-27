#pragma once

#include "PriceLevel.h"
#include "Trade.h"
#include "TradeFormatter.h"
#include "MarketDataEvent.h"
#include "Clock.h"
#include "LatencyCollector.h"
#include "../Memory_Pool/MemoryPool.h"
#include "../Async_Logger/AsyncLogger.h"

#include <map>
#include <deque>
#include <vector>
#include <unordered_map>

class MatchingEngine {
public:

    void submit_order(const Order& order);

    const auto& bids() const { return bids_; }

    const auto& asks() const { return asks_; }

    const auto& trades() const {return trades_;}

    const auto& order_lookup() const {return order_lookup_;}

    const auto& market_data_events() const {return market_data_events_;}

    const auto& latency_collector() const {return latency_collector_;}

    void set_logger(AsyncLogger* logger) {logger_ = logger;}

    void publish_market_data(const Trade& trade);

    bool cancel_order(uint64_t order_id);

private:

    std::map<double, PriceLevel, std::greater<>> bids_;

    std::map<double, PriceLevel> asks_;

    std::vector<Trade> trades_;

    std::vector<MarketDataEvent> market_data_events_;

    AsyncLogger* logger_ = nullptr;

    LatencyCollector latency_collector_;

    MemoryPool<Trade> trade_pool_{1024 * 1024};

    MemoryPool<Order> order_pool_{1024 * 1024};

    std::unordered_map<uint64_t, Order*> order_lookup_;

    void match_buy(Order order);

    void match_sell(Order order);

    void add_to_book(const Order& order);

    void append_order(PriceLevel& level, Order* order);

    void remove_order(PriceLevel& level, Order* order);
};