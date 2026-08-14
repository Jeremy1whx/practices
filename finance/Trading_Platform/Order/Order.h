#pragma once

#include <cstdint>
#include <optional>
#include <chrono>

enum class Side {
    Buy,
    Sell
};

enum class Type {
    IOC,
    FOk,
    GTC,
    GTD,
    DAY,
    GTT
};

struct Order {
    uint64_t order_id;

    Side side;

    Type type;

    double price;

    uint32_t quantity;
    
    uint64_t sequence;

    uint64_t expire_time = 0; // UTC

    uint64_t ingress_timestamp_ns = 0;

    uint64_t egress_timestamp_ns = 0;

    Order* next = nullptr;

    Order* prev = nullptr;
};