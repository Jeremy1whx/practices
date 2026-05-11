#define CATCH_CONFIG_MAIN
#include <thread>
#include <chrono>
#include <iostream>

#include "catch.hpp"
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

TEST_CASE("SPSC multithread test") {
    RingBuffer<int> rb(1024);

    const int N = 100000;

    std::thread producer([&]() {
        for (int i = 0; i < N; ++i) {
            while (!rb.push(i)) {}
        }
    });

    std::thread consumer([&]() {
        int value;
        for (int i = 0; i < N; ++i) {
            while (!rb.pop(value)) {}
        }
    });

    producer.join();
    consumer.join();

    SUCCEED();
}

TEST_CASE("Performance test") {
    RingBuffer<int> rb(1024);

    const int N = 1'000'000;

    auto start = std::chrono::high_resolution_clock::now();

    std::thread producer([&]() {
        for (int i = 0; i < N; ++i) {
            while (!rb.push(i)) {}
        }
    });

    std::thread consumer([&]() {
        int value;
        for (int i = 0; i < N; ++i) {
            while (!rb.pop(value)) {}
        }
    });

    producer.join();
    consumer.join();

    auto end = std::chrono::high_resolution_clock::now();

    double seconds = std::chrono::duration<double>(end - start).count();
    double ops = N / seconds;

    std::cout << "Throughput: " << ops << " ops/sec\n";

    REQUIRE(ops > 1e6); // 1M ops/sec
}