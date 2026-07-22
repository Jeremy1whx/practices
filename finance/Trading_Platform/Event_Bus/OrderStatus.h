#pragma once

namespace exchange{
enum class OrderStatus{
    Pending,
    Accepted,
    PartiallyFilled,
    Filled,
    Cancelled,
    Rejected
};
}