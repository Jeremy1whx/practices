#pragma once

#include "EventHeader.h"
#include "../Trade.h"

namespace exchange {
struct TradeEvent {
    EventHeader header;
    Trade trade;
};
}