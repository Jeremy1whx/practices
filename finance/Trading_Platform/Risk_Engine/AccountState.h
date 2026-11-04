#pragma once

#include <cstdint>

struct alignas(64) AccountState {
    uint64_t account_id;
    int64_t cash_flow;
    int64_t  position;    
    int64_t  open_buy_quantity;  
    int64_t  open_sell_quantity;   
    int64_t  credit_used;       
    int64_t  realized_pnl; 
    uint32_t status;  // 0=active, 1=suspended, 2=closed
    uint32_t _pad0;
    
    uint32_t max_order_size;
    uint32_t max_orders_per_sec;
    int64_t  max_position;
    int64_t  max_credit;
    int64_t  max_daily_loss;
    double   price_band_pct;    
    uint64_t last_order_ns; 
    uint16_t stp_policy;
    uint16_t _pad1;
    uint32_t _pad2;
    uint64_t _pad3; 

    int64_t  unrealized_pnl; 
    uint64_t last_update_ns;
    uint64_t _pad4[6];
};