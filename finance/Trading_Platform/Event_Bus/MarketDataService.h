#pragma once

#include "Subscriber.h"

namespace exchange{
class MarketDataService : public Subscriber {
public:

    bool interested_in(EventType type) const override {
        return type == EventType::BookUpdate;
    }

    const char* name() const override {
        return "MarketDataService";
    }

    void on_event(const Event& event) override {
        latest_ = std::get<BookUpdateEvent>(event).update;
    }

    const BookUpdate& latest() const {
        return latest_;
    }

private:
    BookUpdate latest_;
};}