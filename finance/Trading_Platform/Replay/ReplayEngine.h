#pragma once


#include "ReplayReader.h"
#include "../Event_Bus/EventBus.h"

namespace exchange {
class ReplayEngine {
public:
    ReplayEngine(ReplayReader& reader, EventBus& bus) : reader_(reader), bus_(bus) {};

    void replay() {
        Event event;
        while(reader_.next(event)) {
            bus_.publish(event);
        }
    };

private:
    ReplayReader& reader_;
    EventBus& bus_;
};}