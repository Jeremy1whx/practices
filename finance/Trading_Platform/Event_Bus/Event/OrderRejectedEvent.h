#pragma once

#include "EventHeader.h"
#include "../RejectReason.h"

namespace exchange{
struct OrderRejectedEvent{
    EventHeader header;

    uint64_t order_id;

    RejectReason reason;
};}