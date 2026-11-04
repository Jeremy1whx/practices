#pragma once

namespace exchange{
enum class RiskViolationReason : uint8_t{
    None = 0,
    PositionLimitExceeded = 1,
    OrderSizeLimitExceeded = 2,
    DailyLossLimitExceeded = 3,
    CreditLimitExceeded = 4,
    MarginInsufficient = 5,
    SelfTradeDetected = 6,
    AccountSuspended = 7,
    PriceOutOfBand = 8,
    RateLimitExceeded = 9,
    UnknownAccount = 10,
    InternalError = 11
};
}