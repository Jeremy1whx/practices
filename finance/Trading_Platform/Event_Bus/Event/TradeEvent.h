#pragma once

#include "EventHeader.h"
#include "../../Trade_Event/Trade.h"

namespace exchange {
struct TradeEvent {
    EventHeader header;
    const Trade* trade;
};
}