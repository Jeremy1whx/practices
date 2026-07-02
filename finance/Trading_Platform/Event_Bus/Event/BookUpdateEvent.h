#pragma once

#include <optional>
#include <cstdint>
#include "EventHeader.h"
#include "../BookUpdate.h"

namespace exchange {
struct BookUpdateEvent {
    EventHeader header;
    BookUpdate update;
};
}
