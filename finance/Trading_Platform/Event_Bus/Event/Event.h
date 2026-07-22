#pragma once

#include <variant>

#include "TradeEvent.h"
#include "BookUpdateEvent.h"
#include "OrderAcceptedEvent.h"
#include "OrderCancelledEvent.h"
#include "OrderRejectedEvent.h"
#include "RiskViolationEvent.h"

namespace exchange {
using Event = std::variant<
                        TradeEvent, 
                        BookUpdateEvent, 
                        // OrderAcceptedEvent, 
                        OrderCancelledEvent, 
                        OrderRejectedEvent 
                        // RiskViolationEvent
                        >;
}