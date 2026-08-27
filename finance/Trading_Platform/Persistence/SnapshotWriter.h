#pragma once 

#include "SnapshotService.h"

namespace exchange {
std::string_view to_string(Side side) {
    switch (side)
    {
    case Side::Buy:
        return "Buy";
    case Side::Sell:
        return "Sell";
    default:
        return "Unknown";
    }
}

std::string_view to_string(Type type) {
    switch (type)
    {
    case Type::DAY:
        return "DAY";
    case Type::GTC:
        return "GTC";
    case Type::GTD:
        return "GTD";
    case Type::GTT:
        return "GTT";
    default:
        return "Unknown";
    }
}

class SnapshotTextWriter {
public:

    void process_snapshot(SnapshotBatch& batch) {
        const std::filesystem::path path = "snapshot" + std::to_string(batch.sequence) + ".txt";
        journal_.open(path.string(), std::ios::out);
        journal_ << "snapshot timestamp= " << batch.snapshot_time << '\n';
        journal_ << "different orders quantity= " << batch.orders.size() << '\n';
        for (const OrderSnapshot& snapshot : batch.orders) {
            write(snapshot);
        }
        journal_.close();       
    }
    
private:
    std::ofstream journal_;

    void write(const OrderSnapshot& snapshot) {
        journal_ << "order_id= " << snapshot.order_id 
            << " side= " << to_string(snapshot.side)
            << " price= " << snapshot.price
            << " quantity= " << snapshot.quantity
            << " sequence= " << snapshot.sequence
            << " type= " << to_string(snapshot.type)
            << "expire_time= " << snapshot.expire_time << '\n';
    }
};
}