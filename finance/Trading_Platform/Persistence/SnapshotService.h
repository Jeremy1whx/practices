#pragma once

#include "../Match_Engine/MatchingEngine.h"
#include "../Lock_Free_Ring_Buffer/spsc/Ringbuffer.h"

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
    uint64_t snapshot_time;
    uint64_t sequence;
    uint64_t order_count;
    std::vector<OrderSnapshot> orders;
};

class SnapshotService {
public:
    SnapshotService(MatchingEngine& engine) : engine_(engine) {}

    void append_batch(SnapshotBatch& batch) {
        order_snapshot_.push(std::move(append_snapshot()));
    }

    auto& order_snapshot() {return order_snapshot_;}

private:
    
    MatchingEngine& engine_;

    SPSCRingBuffer<SnapshotBatch> order_snapshot_{1024 * 1024};

    OrderSnapshot create_snapshot(Order* order) {
        OrderSnapshot snapshot;
        snapshot.order_id = order->order_id;
        snapshot.expire_time = order->expire_time;
        snapshot.price = order->price;
        snapshot.quantity = order->quantity;
        snapshot.sequence = order->sequence;
        snapshot.side = order->side;
        snapshot.type = order->type;

        return snapshot;
    }

    SnapshotBatch append_snapshot() {

        SnapshotBatch batch;

        for (const auto& price_level : engine_.bids()) {
            Order* order = price_level.level.head;
            
            while (order) {
                batch.orders.push_back(create_snapshot(order));
                order = order->next;
            }
        }

        for (const auto& price_level : engine_.asks()) {
            Order* order = price_level.level.head;

            while (order) {
                batch.orders.push_back(create_snapshot(order));
                order = order->next;
            }
        }

        batch.snapshot_time = now_absolute_ns();
        return batch;
    }
};
}