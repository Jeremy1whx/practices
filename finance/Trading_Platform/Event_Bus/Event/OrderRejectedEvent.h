#pragma once

#include "EventHeader.h"
#include "../RejectReason.h"

namespace exchange{
struct OrderRejectedEvent{
    EventHeader header;

    uint64_t order_id;

    uint64_t account_id;

    RejectReason reason;

    RiskViolationReason risk_reason = RiskViolationReason::None;
};}