#pragma once

#include "../Order/Order.h"

namespace exchange {
    struct OrderSnapshot {
        uint64_t order_id;
        Side side;
        double price;
        uint64_t quantity;
        uint64_t sequence;
        Type type;
        uint64_t expire_time;
    };

    struct SnapshotBatch {
        uint64_t snapshot_time = 0;
        uint64_t sequence = 0;
        std::vector<OrderSnapshot> orders;
    }; 
}