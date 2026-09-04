#pragma once

#include "../Event_Bus/Subscriber.h"
#include "../Lock_Free_Ring_Buffer/mpsc/Ringbuffer.h"

namespace exchange {
class EventJournalSink : public Subscriber {
public:
    explicit EventJournalSink(MPSCRingBuffer<Event>& queue)
        : event_queue_(queue) {}

    void on_event(const Event& event) override {
        event_queue_.push(event);
    }

    bool interested_in(EventType) const override {
        return true;
    }

    const char* name() const override {
        return "EventSink";
    }

private:
    MPSCRingBuffer<Event>& event_queue_;
};
}