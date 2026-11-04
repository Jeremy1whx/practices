#pragma once

#include <cstdint>
#include <limits>

inline constexpr uint64_t INVALID_INDEX = std::numeric_limits<uint64_t>::max();

enum class STPPolicy : uint16_t {
    None = 0,
    CancelNewest = 1,
    CancelOldest = 2,
    CancelBoth = 3,
    Decrement = 4,
};

struct RiskLimits {
    uint32_t max_order_size = std::numeric_limits<uint32_t>::max();
    uint32_t max_orders_per_sec = 10000;
    int64_t  max_position = std::numeric_limits<int64_t>::max();
    int64_t  max_credit = std::numeric_limits<int64_t>::max();  // cents
    int64_t  max_daily_loss = std::numeric_limits<int64_t>::max();  // cents
    double   price_band_pct = 0.0; 
    STPPolicy stp_policy = STPPolicy::CancelNewest;     
};