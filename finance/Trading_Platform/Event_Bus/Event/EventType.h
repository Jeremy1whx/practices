#pragma once

namespace exchange {
    enum class EventType {
        Trade,
        BookUpdate,
        OrderAccepted,
        OrderCancelled,
        OrderRejected,
        RiskViolation
    };
}