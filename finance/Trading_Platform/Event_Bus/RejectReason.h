#pragma once

#include "RiskViolationReason.h"

namespace exchange{
enum class RejectReason : uint8_t{
    InvalidPrice = 1,
    InvalidQuantity = 2,
    DuplicateOrderId = 3,
    MarketClosed = 4,
    InternalError = 5,
    RiskViolation = 6
};
}