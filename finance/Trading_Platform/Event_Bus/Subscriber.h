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

    void record_event() {
        ++stats_.events_received;
    }

    const uint64_t& events_received() const {
        return stats_.events_received;
    }

    protected:
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

class AcceptanceSubscriber : public Subscriber {
public:

    std::size_t count = 0;

    void on_event(const Event&) override {
        ++count;
    }

    bool interested_in(EventType type) const override {
        return type == EventType::OrderAccepted;
    }

    const char* name() const override {
        return "AcceptanceSubscriber";
    }
};

class CancelationSubscriber : public Subscriber {
public:

    std::size_t count = 0;

    void on_event(const Event&) override {
        ++count;
    }

    bool interested_in(EventType type) const override {
        return type == EventType::OrderCancelled;
    }

    const char* name() const override {
        return "CancelationSubscriber";
    }
};

class RejectionSubscriber : public Subscriber {
public:

    std::size_t count = 0;

    void on_event(const Event&) override {
        ++count;
    }

    bool interested_in(EventType type) const override {
        return type == EventType::OrderRejected;
    }

    const char* name() const override {
        return "RejectionSubscriber";
    }
};

// class RiskViolationSubscriber : public Subscriber {
// public:

//     std::size_t count = 0;

//     void on_event(const Event&) override {
//         ++count;
//     }

//     bool interested_in(EventType type) const override {
//         return type == EventType::RiskViolation;
//     }

//     const char* name() const override {
//         return "RiskViolationSubscriber";
//     }
// };
}
