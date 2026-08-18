#pragma once 

#include "SnapshotService.h"

namespace exchange {
std::string_view to_string(Side side) {
    switch (side)
    {
    case Side::Buy:
        return "Buy";
        break;
    case Side::Sell:
        return "Sell";
        break;    
    default:
        break;
    }
}

std::string_view to_string(Type type) {
    switch (type)
    {
    case Type::DAY:
        return "DAY";
        break;
    case Type::GTC:
        return "GTC";
        break;
    case Type::GTD:
        return "GTD";
        break;
    case Type::GTT:
        return "GTT";
        break;    
    default:
        break;
    }
}

class SnapshotTextWriter {
public:
    explicit SnapshotTextWriter(const std::filesystem::path& path, SnapshotService& snapshot_service) : snapshot_service_(snapshot_service) {
        journal_.open(path.string(), std::ios::out | std::ios::app);
    }

    void process_snapshot() {
        SnapshotBatch batch;
        snapshot_service_.order_snapshot().pop(batch);
        for (const OrderSnapshot& snapshot : batch.orders) {
            write(snapshot);
        }            
    }

    
private:
    std::ofstream journal_;
    SnapshotService& snapshot_service_;

    void write(const OrderSnapshot& snapshot) {
        journal_ << "order_id= " << snapshot.order_id 
            << " side= " << to_string(snapshot.side)
            << " price= " << snapshot.price
            << " quantity= " << snapshot.quantity
            << " sequence= " << snapshot.sequence
            << " type= " << to_string(snapshot.type)
            << "expire_time= " << snapshot.expire_time << std::endl;
    }
};
}