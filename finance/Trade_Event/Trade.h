#pragma once

#include <cstdint>

struct Trade {

    uint64_t buy_order_id;

    uint64_t sell_order_id;

    double price;

    uint32_t quantity;

    uint64_t latency_ns = 0;
};