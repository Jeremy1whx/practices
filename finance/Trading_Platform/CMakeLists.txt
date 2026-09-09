cmake_minimum_required(VERSION 3.10)
project(TradingSystem)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O2 -g -fno-omit-frame-pointer")

include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/Async_Logger
    ${CMAKE_CURRENT_SOURCE_DIR}/Match_Engine
    ${CMAKE_CURRENT_SOURCE_DIR}/Event_Bus
    ${CMAKE_CURRENT_SOURCE_DIR}/Persistence
    ${CMAKE_CURRENT_SOURCE_DIR}/Replayer
    ${CMAKE_CURRENT_SOURCE_DIR}/Lock_Free_Ring_Buffer
)

find_package(Threads REQUIRED)

add_library(AsyncLogger STATIC
    Async_Logger/AsyncLogger.cpp
)

add_library(EventBus STATIC
    Event_Bus/EventBus.cpp
)

target_link_libraries(AsyncLogger
    Threads::Threads
)

add_library(MatchingEngine STATIC
    Match_Engine/MatchingEngine.cpp
    Match_Engine/MatchingEngineThread.cpp
)

target_link_libraries(MatchingEngine
    EventBus
    AsyncLogger
    Threads::Threads
)

add_executable(test
    test.cpp
)

target_link_libraries(test
    MatchingEngine
    EventBus
    AsyncLogger
    Threads::Threads
)