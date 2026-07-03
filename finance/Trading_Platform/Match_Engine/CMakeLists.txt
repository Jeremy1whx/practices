cmake_minimum_required(VERSION 3.10)
project(TradingSystem)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O2 -g -fno-omit-frame-pointer")

include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}../Async_Logger
    ${CMAKE_CURRENT_SOURCE_DIR}../Match_Engine
    ${CMAKE_CURRENT_SOURCE_DIR}../Lock_Free_Ring_Buffer/mpsc
)

find_package(Threads REQUIRED)

add_library(AsyncLogger STATIC
    Async_Logger/AsyncLogger.cpp
)

target_link_libraries(AsyncLogger
    Threads::Threads
)

add_library(MatchingEngine STATIC
    Match_Engine/MatchingEngine.cpp
    Match_Engine/MatchingEngineThread.cpp
)

target_link_libraries(MatchingEngine
    AsyncLogger
    Threads::Threads
)

add_executable(test
    Match_Engine/test.cpp
)

target_link_libraries(test
    MatchingEngine
)