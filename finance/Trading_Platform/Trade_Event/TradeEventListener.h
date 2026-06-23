#pragma once

#include <map>

#include "Trade.h"
#include "TradeFormatter.h"
#include "PriceLevel.h"
#include "../Memory_Pool/LRU.h"
#include "../Async_Logger/AsyncLogger.h"

class TradeEventListener {
public:
    virtual ~TradeEventListener() = default;

    virtual void on_trade(const Trade& trade) = 0;
    
    virtual void set_logger(AsyncLogger* logger) {(void) logger;}
    
    // virtual std::vector<MarketDataEvent>& market_data_events() {return market_data_events_;}

    // virtual const std::vector<MarketDataEvent>& market_data_events() const {return market_data_events_;}

    auto& bids() { return bids_; }

    auto& asks() { return asks_; }

    auto& order_pool() { return order_pool_; }

    auto& trade_pool() { return trade_pool_; }

    const auto& bids() const { return bids_; }

    const auto& asks() const { return asks_; }

    const auto& order_pool() const { return order_pool_; }

    const auto& trade_pool() const { return trade_pool_; }

protected:
    std::map<double, PriceLevel, std::greater<>> bids_;
    std::map<double, PriceLevel> asks_;
    LRUPool<Trade> trade_pool_{1024 * 1024};
    MemoryPool<Order> order_pool_{1024 * 1024};
    AsyncLogger* logger_ = nullptr;    
    // std::vector<MarketDataEvent> market_data_events_;
};