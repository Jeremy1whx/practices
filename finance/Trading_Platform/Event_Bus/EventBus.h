#pragma once

#include <vector>
#include <algorithm>
#include <iostream>

#include "Subscriber.h"
#include "TradeLogger.h"
#include "MarketDataService.h"
#include "Event/EventUtils.h"
#include "EventBusStats.h"

namespace exchange {
class EventBus {
public:

    void subscribe(Subscriber* subscriber);

    void publish(const Event& event);

    const EventBusStats& stats() const {return stats_;}

    const uint64_t& get_stats(Subscriber* subscriber) const;

    void print_stats() const;

private:

    std::vector<Subscriber*> subscribers_;
    // std::unordered_map<Subscriber*, SubscriberStats> subscriber_stats_;

    EventBusStats stats_;
};
}