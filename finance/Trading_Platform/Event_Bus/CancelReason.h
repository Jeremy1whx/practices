#pragma once

namespace exchange{
enum class CancelReason : uint8_t{
    UserRequest = 1,
    IOCResidual = 2,
    FOKNotFilled = 3,
    MarketClosed = 4,
    SystemShutdown = 5,    
    RiskLiquidation = 6,
    Expired = 7
};
}