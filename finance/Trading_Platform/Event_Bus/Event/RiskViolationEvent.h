#pragma once

#include <string>

#include "EventHeader.h"
#include "../RiskViolationReason.h"

namespace exchange{
struct RiskViolationEvent
{
    EventHeader header;

    uint64_t account_id;

    RiskViolationReason reason;
};
}