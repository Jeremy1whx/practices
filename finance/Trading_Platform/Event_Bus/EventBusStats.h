#pragma once

#include <cstdint>

namespace exchange {
struct EventBusStats {
    uint64_t published_events = 0;
    uint64_t trade_events = 0;
    uint64_t book_update_events = 0;
    uint64_t order_accepted_events = 0;
    uint64_t order_cancelled_events = 0;
    uint64_t order_rejected_events = 0;
    // uint64_t risk_violation_events = 0;
};
}