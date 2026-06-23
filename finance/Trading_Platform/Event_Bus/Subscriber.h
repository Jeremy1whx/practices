#pragma once

#include "Event/Event.h"

namespace exchange {
class Subscriber {
public:
    virtual ~Subscriber() = default;

    virtual void on_event(const Event& event) = 0;
};

class TestSubscriber : public Subscriber {
public:
    size_t count = 0;
    void on_event(const Event&) override {
        ++count;
    }
};
}
