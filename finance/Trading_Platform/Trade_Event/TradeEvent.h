#pragma once

#include "Trade.h"
#include "MarketDataEvent.h"

#include <optional>

struct TradeEvent {
    Trade trade;
    MarketDataEvent marketDataEvent;
};