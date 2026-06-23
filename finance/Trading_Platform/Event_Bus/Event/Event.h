#pragma once

#include <variant>

#include "TradeEvent.h"
#include "BookUpdateEvent.h"

namespace exchange {
using Event = std::variant<TradeEvent, BookUpdateEvent>;
}