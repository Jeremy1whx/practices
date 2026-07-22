#pragma once

namespace exchange{
enum class RiskViolationReason{
    PositionLimitExceeded = 1,
    OrderSizeLimitExceeded = 2,
    DailyLossLimitExceeded = 3,
    CreditLimitExceeded = 4,
    MarginInsufficient = 5,
    SelfTradeDetected = 6
};
}