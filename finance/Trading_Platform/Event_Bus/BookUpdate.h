#pragma once

#include <optional>
#include <cstdint>


struct BookUpdate {

    std::optional<double> best_bid;

    std::optional<double> best_ask;

    std::optional<double> last_trade_price;

    std::optional<uint32_t> last_trade_quantity;
};