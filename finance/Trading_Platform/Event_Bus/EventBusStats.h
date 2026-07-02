#pragma once

#include <cstdint>

namespace exchange {
struct EventBusStats {
    uint64_t published_events = 0;
    uint64_t trade_events = 0;
    uint64_t book_update_events = 0;
};
}