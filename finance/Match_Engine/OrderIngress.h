#pragma once

#include "Order.h"
#include "MPSCQueue.h"

class OrderIngress {
public:

    explicit OrderIngress(size_t capacity)
        : queue_(capacity)
    {}

    bool submit(const Order& order) {

        return queue_.push(order);
    }

    bool try_get(Order& order) {

        return queue_.pop(order);
    }

    bool empty() const {
        return queue_.empty();
    }

private:

    RingBuffer<Order> queue_;
};