#pragma once

#include <cstdint>

namespace exchange{
struct Trade {

    uint64_t buy_order_id;

    uint64_t sell_order_id;

    double price;

    uint32_t quantity;

    uint64_t total_latency_ns = 0;

    uint64_t queue_latency_ns = 0;

    uint64_t match_duration_ns = 0;
};}