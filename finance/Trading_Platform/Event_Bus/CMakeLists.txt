cmake_minimum_required(VERSION 3.20)

project(exchange_event_bus)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}../Async_Logger
    ${CMAKE_CURRENT_SOURCE_DIR}../Latency
    ${CMAKE_CURRENT_SOURCE_DIR}../Lock_Free_Ring_Buffer/mpsc
)

add_library(AsyncLogger STATIC
    ../Async_Logger/AsyncLogger.cpp
)

add_library(EventBus
    EventBus.cpp
)

add_executable(test
    test.cpp
)

target_link_libraries(EventBus
    AsyncLogger
)

target_link_libraries(test
    EventBus
)