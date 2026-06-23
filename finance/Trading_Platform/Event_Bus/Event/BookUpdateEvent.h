#pragma once

#include "EventHeader.h"
#include "../../Market_Data_Service/MarketDataEvent.h"

namespace exchange {
struct BookUpdateEvent {
    EventHeader header;
    MarketDataEvent data;
};
}
