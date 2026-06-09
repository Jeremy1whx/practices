#pragma once

#include <cstdint>

enum class Side {
    Buy,
    Sell
};

struct Order {
    uint64_t order_id;

    Side side;

    double price;

    uint32_t quantity;
    
    uint64_t sequence;

    uint64_t ingress_timestamp_ns = 0;

    uint64_t egress_timestamp_ns = 0;

    Order* next = nullptr;

    Order* prev = nullptr;
};