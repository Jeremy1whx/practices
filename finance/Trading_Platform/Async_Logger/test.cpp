#define CATCH_CONFIG_MAIN
#define CATCH_CONFIG_ENABLE_BENCHMARKING 

#include "AsyncLogger.h"
#include "../Lock_Free_Ring_Buffer/mpsc/catch.hpp"

#include <thread>
#include <vector>

TEST_CASE("Logger can start and stop cleanly") {

    AsyncLogger logger("test.log");

    REQUIRE_NOTHROW(logger.start());

    REQUIRE_NOTHROW(logger.stop());
}

TEST_CASE("Single log message") {

    AsyncLogger logger("single.log");

    logger.start();

    REQUIRE_NOTHROW(
        logger.log("hello")
    );

    logger.stop();
}

TEST_CASE("Queue handles full condition") {

    RingBuffer<int> queue(8);

    for (int i = 0; i < 7; ++i) {
        REQUIRE(queue.push(i));
    }

    REQUIRE_FALSE(queue.push(999));
}

TEST_CASE("Queue handles empty condition") {

    RingBuffer<int> queue(8);

    int value;

    REQUIRE_FALSE(queue.pop(value));
}

TEST_CASE("Queue wraparound correctness") {

    RingBuffer<int> queue(8);

    for (int round = 0; round < 10000; ++round) {

        for (int i = 0; i < 7; ++i) {
            REQUIRE(queue.push(i));
        }

        int value;

        for (int i = 0; i < 7; ++i) {
            REQUIRE(queue.pop(value));
            REQUIRE(value == i);
        }
    }
}

TEST_CASE("MPSC correctness under concurrency") {

    RingBuffer<int> queue(1024);

    constexpr int producers = 4;
    constexpr int N = 100000;

    std::atomic<int> produced = 0;
    std::atomic<int> consumed = 0;

    std::vector<std::thread> threads;

    for (int p = 0; p < producers; ++p) {

        threads.emplace_back([&]() {

            for (int i = 0; i < N; ++i) {

                while (!queue.push(i)) {}

                produced++;
            }
        });
    }

    std::thread consumer([&]() {

        int value;

        while (consumed < producers * N) {

            if (queue.pop(value)) {
                consumed++;
            }
        }
    });

    for (auto& t : threads) {
        t.join();
    }

    consumer.join();

    REQUIRE(produced == producers * N);
    REQUIRE(consumed == producers * N);
}

TEST_CASE("Stress test with heavy concurrency") {

    AsyncLogger logger("stress.log");

    logger.start();

    constexpr int threads = 8;
    constexpr int N = 500000;

    std::vector<std::thread> workers;

    for (int i = 0; i < threads; ++i) {

        workers.emplace_back([&]() {

            for (int j = 0; j < N; ++j) {

                logger.log("stress");
            }
        });
    }

    for (auto& t : workers) {
        t.join();
    }

    logger.stop();

    SUCCEED();
}

TEST_CASE("Logger throughput benchmark") {

    BENCHMARK("async logger throughput") {

        AsyncLogger logger("bench.log");

        logger.start();

        constexpr int N = 1000000;

        std::thread t1([&]() {

            for (int i = 0; i < N; ++i) {
                logger.log("benchmark");
            }
        });

        std::thread t2([&]() {

            for (int i = 0; i < N; ++i) {
                logger.log("benchmark");
            }
        });

        t1.join();
        t2.join();

        logger.stop();

        return N * 2;
    };
}

TEST_CASE("Long running stability test") {

    AsyncLogger logger("long.log");

    logger.start();

    constexpr int seconds = 10;

    auto start = std::chrono::steady_clock::now();

    std::thread producer([&]() {

        while (
            std::chrono::steady_clock::now() - start
            < std::chrono::seconds(seconds)
        ) {
            logger.log("running");
        }
    });

    producer.join();

    logger.stop();

    SUCCEED();
}