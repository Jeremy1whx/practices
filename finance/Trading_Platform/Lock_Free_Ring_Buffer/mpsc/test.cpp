#define CATCH_CONFIG_MAIN
#include <thread>
#include <vector>
#include <atomic>
#include <chrono>
#include <set>
#include <iostream>

#include "catch.hpp"
#include "Ringbuffer.h" 

TEST_CASE("Basic push/pop") {
    RingBuffer<int> rb(8);

    REQUIRE(rb.push(1));
    REQUIRE(rb.push(2));

    int value;

    REQUIRE(rb.pop(value));
    REQUIRE(value == 1);

    REQUIRE(rb.pop(value));
    REQUIRE(value == 2);

    REQUIRE_FALSE(rb.pop(value));
}

TEST_CASE("Queue full") {
    RingBuffer<int> rb(4);

    REQUIRE(rb.push(1));
    REQUIRE(rb.push(2));
    REQUIRE(rb.push(3));

    REQUIRE_FALSE(rb.push(4));
}

TEST_CASE("Queue empty") {
    RingBuffer<int> rb(8);

    int value;
    REQUIRE_FALSE(rb.pop(value));
}

TEST_CASE("FIFO order single producer") {
    RingBuffer<int> rb(1024);

    for (int i = 0; i < 1000; ++i) {
        REQUIRE(rb.push(i));
    }

    int value;

    for (int i = 0; i < 1000; ++i) {
        REQUIRE(rb.pop(value));
        REQUIRE(value == i);
    }
}

TEST_CASE("Multiple producers correctness") {
    constexpr int producers = 4;
    constexpr int messages_per_producer = 100000;

    RingBuffer<int> rb(1024);

    std::atomic<int> produced{0};
    std::atomic<int> consumed{0};

    std::vector<std::thread> producer_threads;

    for (int p = 0; p < producers; ++p) {
        producer_threads.emplace_back([&, p]() {
            for (int i = 0; i < messages_per_producer; ++i) {

                int value = p * messages_per_producer + i;

                while (!rb.push(value)) {
                    std::this_thread::yield();
                }

                produced.fetch_add(1, std::memory_order_relaxed);
            }
        });
    }

    std::vector<int> results;
    results.reserve(producers * messages_per_producer);

    std::thread consumer([&]() {
        int value;

        while (consumed.load() < producers * messages_per_producer) {

            if (rb.pop(value)) {
                results.push_back(value);

                consumed.fetch_add(1, std::memory_order_relaxed);
            } else {
                std::this_thread::yield();
            }
        }
    });

    for (auto& t : producer_threads) {
        t.join();
    }

    consumer.join();

    REQUIRE(produced == producers * messages_per_producer);
    REQUIRE(consumed == producers * messages_per_producer);

    std::set<int> unique_values(results.begin(), results.end());

    REQUIRE(unique_values.size() ==
            producers * messages_per_producer);
}

TEST_CASE("Stress test") {

    constexpr int producers = 8;
    constexpr int messages_per_producer = 500000;

    RingBuffer<int> rb(1 << 16);

    std::atomic<int> consumed{0};

    std::vector<std::thread> producer_threads;

    auto start = std::chrono::high_resolution_clock::now();

    for (int p = 0; p < producers; ++p) {

        producer_threads.emplace_back([&, p]() {

            for (int i = 0; i < messages_per_producer; ++i) {

                int value = p * messages_per_producer + i;

                while (!rb.push(value)) {}
            }
        });
    }

    std::thread consumer([&]() {

        int value;

        while (consumed.load() <
               producers * messages_per_producer) {

            if (rb.pop(value)) {
                consumed.fetch_add(1,
                    std::memory_order_relaxed);
            }
        }
    });

    for (auto& t : producer_threads) {
        t.join();
    }

    consumer.join();

    auto end = std::chrono::high_resolution_clock::now();

    double seconds =
        std::chrono::duration<double>(end - start).count();

    long long total_ops =
        1LL * producers * messages_per_producer;

    double throughput = total_ops / seconds;

    std::cout << "\n";
    std::cout << "Stress Test Results\n";
    std::cout << "===================\n";
    std::cout << "Total Operations : "
              << total_ops << "\n";

    std::cout << "Time             : "
              << seconds << " sec\n";

    std::cout << "Throughput       : "
              << throughput / 1e6
              << " M ops/sec\n";

    REQUIRE(consumed == total_ops);
}

TEST_CASE("Small capacity edge case") {

    RingBuffer<int> rb(2);

    REQUIRE(rb.push(1));

    REQUIRE_FALSE(rb.push(2));

    int value;

    REQUIRE(rb.pop(value));
    REQUIRE(value == 1);

    REQUIRE_FALSE(rb.pop(value));
}