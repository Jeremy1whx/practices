cmake_minimum_required(VERSION 3.20)

project(exchange_event_bus)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}../Async_Logger
    ${CMAKE_CURRENT_SOURCE_DIR}../Latency
    ${CMAKE_CURRENT_SOURCE_DIR}../Event_Bus
    ${CMAKE_CURRENT_SOURCE_DIR}../Lock_Free_Ring_Buffer
    ${CMAKE_CURRENT_SOURCE_DIR}../Match_Engine
)

add_library(AsyncLogger STATIC
    ../Async_Logger/AsyncLogger.cpp
)

add_library(EventBus
    ../Event_Bus/EventBus.cpp
)

add_library(MatchingEngine STATIC
    ../Match_Engine/MatchingEngine.cpp
    ../Match_Engine/MatchingEngineThread.cpp)

add_executable(test
    test.cpp
)

target_link_libraries(EventBus
    AsyncLogger
)

target_link_libraries(test
    EventBus
    MatchingEngine
)