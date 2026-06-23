cmake_minimum_required(VERSION 3.20)

project(exchange_event_bus)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(EventBus
    EventBus.cpp
)

add_executable(test
    test.cpp
)

target_link_libraries(test
    EventBus
)