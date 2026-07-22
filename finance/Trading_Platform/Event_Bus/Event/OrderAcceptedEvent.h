#pragma once

#include "EventHeader.h"
#include "../../Order/Order.h"

namespace exchange{
struct OrderAcceptedEvent{
    EventHeader header;

    uint64_t order_id;

    Side side;

    double price;

    uint32_t quantity;
};}