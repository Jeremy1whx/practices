#pragma once

#include "EventHeader.h"
#include "../../Order/Order.h"

namespace exchange{
struct OrderAcceptedEvent{
    EventHeader header;

    uint64_t order_id;

    Side side;

    Type type;

    double price;

    uint32_t quantity;
    
    uint64_t sequence;

    uint64_t expire_time = 0;
};}