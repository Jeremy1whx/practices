#pragma once

#include "TradeEventListener.h"

class TradeLogger : public TradeEventListener {
public:
    void on_trade(const Trade& trade) override{
        if (logger_) {
            logger_->log((format_trade(trade)).c_str());
        } else {
            trade_pool_.recycle_oldest();}
    }
};