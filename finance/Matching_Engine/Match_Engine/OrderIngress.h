#pragma once

#include "Order.h"
#include "MPSCQueue.h"

class OrderIngress {
public:

    explicit OrderIngress(size_t capacity)
        : queue_(capacity)
    {}

    bool submit(const Order& order) {
        Order copy = order;
        return submit(copy);
    }

    bool submit(Order& order) {
        return queue_.push(std::move(order));
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