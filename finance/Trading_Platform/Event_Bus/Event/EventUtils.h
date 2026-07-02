#pragma once

#include "Event.h"

namespace exchange{
inline EventType get_event_type(const Event& event) {
    return std::visit([](const auto& e){
            return e.header.type;
        }, event);
}
}