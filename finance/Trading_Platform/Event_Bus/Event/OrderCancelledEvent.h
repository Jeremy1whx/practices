#pragma once

#include "EventHeader.h"
#include "../CancelReason.h"

namespace exchange{
struct OrderCancelledEvent{
    EventHeader header;

    uint64_t order_id;

    uint64_t account_id;

    CancelReason reason;
};}