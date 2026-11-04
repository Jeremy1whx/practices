#pragma once

#include "../Order/Order.h"

namespace exchange {
    struct OrderSnapshot {
        uint64_t order_id;
        uint64_t account_id;
        Side side;
        double price;
        uint64_t quantity;
        uint64_t sequence;
        Type type;
        uint64_t expire_time;
    };

    struct AccountSnapshot {
        uint64_t account_id;
        int64_t  position;
        int64_t  open_buy_quantity; 
        int64_t  open_sell_quantity;
        int64_t  credit_used;
        int64_t  realized_pnl;
        int64_t  cash_flow;
    };

    struct SnapshotBatch {
        uint64_t snapshot_time = 0;
        uint64_t sequence = 0;
        double last_trade_price;
        std::vector<OrderSnapshot> orders;
        std::vector<AccountSnapshot> accounts;
    }; 

    struct SnapshotHeader {
        uint32_t magic;
        uint32_t version;
        uint64_t timestamp;
        uint64_t sequence;
        uint64_t order_count;
        uint64_t account_count;
        double   last_trade_price;
        uint64_t reserved;
    };
}