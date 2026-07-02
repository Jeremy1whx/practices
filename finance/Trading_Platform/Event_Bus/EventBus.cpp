#include "EventBus.h"

namespace exchange {
void EventBus::subscribe(Subscriber* subscriber) {
    if (std::find(subscribers_.begin(), subscribers_.end(), subscriber) != subscribers_.end()) return;

    subscribers_.push_back(subscriber);
}

void EventBus::publish(const Event& event) {
    EventType type = get_event_type(event);

    ++stats_.published_events;
    switch (type) {
        case EventType::Trade:
            ++stats_.trade_events;
            break;

        case EventType::BookUpdate:
            ++stats_.book_update_events;
            break;
    }

    for (auto* subscriber : subscribers_) {
        if (subscriber->interested_in(type)) {
            subscriber->stats_.events_received++;
            subscriber->on_event(event);
        }
    }
}

const uint64_t& EventBus::get_stats(Subscriber* subscriber) const {
    return subscriber->stats_.events_received;
}

void EventBus::print_stats() const
{
    std::cout << "Published: " << stats_.published_events << std::endl;

    for (auto* subscriber : subscribers_) {
        std::cout << subscriber->name() << ": " << subscriber->stats_.events_received << std::endl;
    }
}
}