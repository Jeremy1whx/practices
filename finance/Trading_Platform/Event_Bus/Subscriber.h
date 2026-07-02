#pragma once

#include "Event/Event.h"
#include "SubscriberStats.h"

namespace exchange {
class Subscriber {
public:
    virtual ~Subscriber() = default;

    virtual void on_event(const Event& event) = 0;

    virtual bool interested_in(EventType type) const = 0;

    virtual const char* name() const = 0;

    SubscriberStats stats_;
};

class TradeSubscriber : public Subscriber {
public:

    std::size_t count = 0;

    void on_event(const Event&) override {
        ++count;
    }

    bool interested_in(EventType type) const override {
        return type == EventType::Trade;
    }

    const char* name() const override {
        return "TradeSubscriber";
    }
};

class BookSubscriber : public Subscriber {
public:

    std::size_t count = 0;

    void on_event(const Event&) override {
        ++count;
    }

    bool interested_in(EventType type) const override {
        return type == EventType::BookUpdate;
    }

    const char* name() const override {
        return "BookSubscriber";
    }
};
}
