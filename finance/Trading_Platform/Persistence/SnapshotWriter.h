#pragma once 

#include "SnapshotService.h"

namespace exchange {
inline std::string_view to_string(Side side) {
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

inline std::string_view to_string(Type type) {
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

class SnapshotBinaryWriter {
public:

    explicit SnapshotBinaryWriter(const std::filesystem::path& directory) : directory_(directory / "snapshot") {
        std::filesystem::create_directories(directory_);
    }

    void process_snapshot(SnapshotBatch& batch) {
        const std::filesystem::path path = directory_  / ("snapshot_" + std::to_string(batch.sequence) + ".bin");
        std::ofstream file(path.string(), std::ios::out | std::ios::binary);
        
        SnapshotHeader header;
        header.magic = 0x534E5053;
        header.version = 1;
        header.timestamp = batch.snapshot_time;
        header.sequence = batch.sequence;
        header.order_count = batch.orders.size();
        header.reserved = 0;
        
        file.write(reinterpret_cast<const char*>(&header), sizeof(header));
        
        file.write(
            reinterpret_cast<const char*>(batch.orders.data()),
            batch.orders.size() * sizeof(OrderSnapshot)
        );
        
        file.close();
    }
    
private:
    std::filesystem::path directory_;
    struct SnapshotHeader {
        uint32_t magic;  
        uint32_t version;  
        uint64_t timestamp; 
        uint64_t sequence; 
        uint64_t order_count; 
        uint64_t reserved;   
    };
    
    static_assert(std::is_standard_layout_v<OrderSnapshot> && 
                  std::is_trivial_v<OrderSnapshot>, 
                  "OrderSnapshot must be POD");
    static_assert(std::is_standard_layout_v<SnapshotHeader> && 
                  std::is_trivial_v<SnapshotHeader>, 
                  "SnapshotHeader must be POD");
};
}