#pragma once

#include <vector>

#include "Subscriber.h"

namespace exchange
{
class EventBus
{
public:

    void subscribe(Subscriber* subscriber);

    void publish(const Event& event);

private:

    std::vector<Subscriber*> subscribers_;
};
}