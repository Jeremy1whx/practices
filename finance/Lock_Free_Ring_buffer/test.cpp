#include <catch2/catch_all.hpp>
#include "Ringbuffer.h"

TEST_CASE("Basic push and pop") {
    RingBuffer<int> rb(8);

    REQUIRE(rb.push(1));
    REQUIRE(rb.push(2));

    int value;
    REQUIRE(rb.pop(value));
    REQUIRE(value == 1);

    REQUIRE(rb.pop(value));
    REQUIRE(value == 2);
}

TEST_CASE("Buffer full and empty") {
    RingBuffer<int> rb(4); // should store 3 elements

    REQUIRE(rb.push(1));
    REQUIRE(rb.push(2));
    REQUIRE(rb.push(3));

    REQUIRE_FALSE(rb.push(4)); // should be full

    int value;
    REQUIRE(rb.pop(value));
    REQUIRE(rb.pop(value));
    REQUIRE(rb.pop(value));

    REQUIRE_FALSE(rb.pop(value)); // should be empty
}