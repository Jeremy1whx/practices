#pragma once

#include "Trade.h"

#include <sstream>
#include <string>

inline std::string format_trade(const Trade& trade) {

    std::ostringstream oss;

    oss << "TRADE "
        << "buy=" << trade.buy_order_id
        << " sell=" << trade.sell_order_id
        << " price=" << trade.price
        << " qty=" << trade.quantity;

    return oss.str();
}