#pragma once

#include <cstdint>
#include "EventType.h"

namespace exchange {
struct EventHeader {
    EventType type;
    uint64_t timestamp_ns;
};
}