#pragma once

#include <cstdint>

namespace exchange {
struct SubscriberStats {
    uint64_t events_received = 0;
};
}