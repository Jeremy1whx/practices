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
        uint64_t snapshot_time = 0;
        uint64_t sequence = 0;
        std::vector<OrderSnapshot> orders;
    };     

class SnapshotService {
public:
    SnapshotService(MatchingEngine& engine) : engine_(engine) {
        flaga_.batch.orders.reserve(1024 * 1024);
        flagb_.batch.orders.reserve(1024 * 1024);
    }
    
    void append_snapshot(uint64_t sequence, uint64_t timestamp) {

        SnapshotBatchWithFlag& flag = choose_batch(sequence);

        for (const auto& price_level : engine_.bids()) {
            Order* order = price_level.level.head;
            
            while (order) {
                flag.batch.orders.push_back(create_snapshot(order));
                order = order->next;
            }
        }

        for (const auto& price_level : engine_.asks()) {
            Order* order = price_level.level.head;

            while (order) {
                flag.batch.orders.push_back(create_snapshot(order));
                order = order->next;
            }
        }        

        flag.batch.snapshot_time = timestamp;
        flag.batch.sequence = sequence;
        flag.ready.store(true, std::memory_order_release);
    }

    void reset_batch(uint64_t sequence) {

        SnapshotBatchWithFlag& flag = choose_batch(sequence);
        
        flag.batch.orders.clear();
        flag.batch.orders.reserve(1024 * 1024);
        flag.batch.sequence = 0;
        flag.batch.snapshot_time = 0;
        flag.ready.store(false, std::memory_order_release);
    }

    auto& batch(uint64_t sequence) {return choose_batch(sequence);} 
    
private:

    struct SnapshotBatchWithFlag {
        SnapshotBatch batch;        
        std::atomic<bool> ready{false};
    };
    
    MatchingEngine& engine_;

    SnapshotBatchWithFlag flaga_;
    SnapshotBatchWithFlag flagb_;

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

    SnapshotBatchWithFlag& choose_batch(uint64_t sequence) {
        if (sequence % 2 == 0) {
            return flaga_;
        }
        return flagb_;
    }
};
}