#pragma once

namespace exchange{
enum class RejectReason{
    InvalidPrice = 1,
    InvalidQuantity = 2,
    DuplicateOrderId = 3,
    MarketClosed = 4,
    // RiskViolation,
    InternalError = 5
};
}