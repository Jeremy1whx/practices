#pragma once

#include "TradeEventListener.h"

#include <vector>

class TradeDataPublisher : public TradeEventListener {
public:
    void on_trade(const Trade& trade) override {
        MarketDataEvent event;

        if (!bids_.empty()) {
            event.best_bid = bids_.begin()->first;
        }

        if (!asks_.empty()) {
            event.best_ask = asks_.begin()->first;
        }
        event.last_trade_price = trade.price;

        event.last_trade_quantity = trade.quantity;
        trade_pool_.recycle_oldest();
        market_data_events_.push_back(event);
    }
};