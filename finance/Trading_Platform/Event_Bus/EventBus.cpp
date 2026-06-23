#include "EventBus.h"

namespace exchange {
void EventBus::subscribe(Subscriber* subscriber) {
    subscribers_.push_back(subscriber);
}

void EventBus::publish(const Event& event) {
    for (auto* subscriber : subscribers_) {
        subscriber->on_event(event);
    }
}
}