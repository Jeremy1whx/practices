# Low-Latency Matching Engine

A high-performance order matching engine written in C++20, designed for low-latency trading applications. Features lock-free queues, custom memory pools, price-time priority matching, and comprehensive latency measurement.

## Features

- **Price-Time Priority Matching** – Implements standard exchange matching logic
- **Lock-Free MPSC Ring Buffer** – Multi-producer single-consumer queue for order ingress
- **Custom Memory Pools** – Pre-allocated object pools with LRU recycling, zero dynamic allocation during runtime
- **Latency Measurement** – Per-trade total, queue, and match latency collection with percentile statistics
- **Batch Processing** – Configurable batch sizes for improved throughput
- **CPU Affinity** – Pin matching thread to dedicated core (Linux)
- **Asynchronous Logging** – Non-blocking log writer using lock-free queue
- **Market Data Publishing** – Observer pattern for trade and market data events
- **Comprehensive Testing** – 27 test cases including benchmarks using Catch2

## Architecture
┌────────────────────────────────────────────────────────────────────┐
│                         MatchingEngineThread                        │
│  ┌─────────────────────────────────────────────────────────────┐   │
│  │                                                              │   │
│  │   ┌──────────────┐      ┌──────────────────────┐           │   │
│  │   │ OrderIngress │─────▶│   MatchingEngine     │           │   │
│  │   │              │      │                      │           │   │
│  │   │ ┌──────────┐ │      │ ┌─────────────────┐  │           │   │
│  │   │ │MPSCQueue │ │      │ │  order_book     │  │           │   │
│  │   │ │RingBuffer│ │      │ │  (bids/asks)    │  │           │   │
│  │   │ └──────────┘ │      │ └─────────────────┘  │           │   │
│  │   └──────────────┘      │ ┌─────────────────┐  │           │   │
│  │                         │ │  LatencyCollector│  │           │   │
│  │                         │ └─────────────────┘  │           │   │
│  │                         └──────────┬───────────┘           │   │
│  │                                    │                        │   │
│  │                                    ▼                        │   │
│  │                         ┌──────────────────────┐           │   │
│  │                         │  TradeEventListener  │           │   │
│  │                         │       (Observer)     │           │   │
│  │                         └──────────┬───────────┘           │   │
│  │                                    │                        │   │
│  │              ┌─────────────────────┼─────────────────────┐  │   │
│  │              ▼                     ▼                     ▼  │   │
│  │   ┌──────────────────┐  ┌──────────────────┐  ┌────────────┐│   │
│  │   │   TradeLogger    │  │TradeDataPublisher│  │   Future   ││   │
│  │   │                  │  │                  │  │  Extensions││   │
│  │   │ ┌──────────────┐ │  │ ┌──────────────┐ │  │            ││   │
│  │   │ │ AsyncLogger  │ │  │ │MarketDataEvent││  │            ││   │
│  │   │ │ (lock-free)  │ │  │ │   (best_bid,  ││  │            ││   │
│  │   │ └──────────────┘ │  │ │    best_ask,  ││  │            ││   │
│  │   └──────────────────┘  │ │   last_trade) ││  │            ││   │
│  │                         │ └──────────────┘│  │            ││   │
│  │                         └──────────────────┘  └────────────┘│   │
│  └─────────────────────────────────────────────────────────────┘   │
└────────────────────────────────────────────────────────────────────┘

## Key Components

| Component | Description |
|-----------|-------------|
| `MatchingEngine` | Core matching logic with price-time priority |
| `RingBuffer<T>` | Lock-free MPSC queue with cache-aligned head/tail |
| `MemoryPool<T>` | Fixed-size object pool with O(1) allocation |
| `LRUPool<T>` | Memory pool with LRU recycling policy |
| `LatencyCollector` | Percentile (P50/P99) and max latency statistics |
| `AsyncLogger` | Non-blocking file logger |
| `TradeEventListener` | Observer interface for trade events |

## Performance Benchmarks

Results from running on a dedicated CPU core (Linux):

| Metric | Value |
|--------|-------|
| **Throughput (Standard)** | 3 million orders/sec |
| **Throughput (Batch)** | 3.45 million orders/sec |
| **P50 Queue Latency** | 222.23 ms |
| **P99 Queue Latency** | 1328.18 ms |
| **Max Queue Latency** | 1329.74 ms |
| **P50 Match Duration** | 0 ns (below measurement resolution) |
| **P99 Match Duration** | 100 ns |
| **Max Match Duration** | 211.7 µs |
| **P50 Total Latency (end to end)** | 222.23 ms |
| **P99 Total Latency (end to end)** | 1328.19 ms |

*Test configuration: 5 million trade pairs (10 million orders)*
![alt text](image1.png)

## Building

### Prerequisites

- C++20 compatible compiler (GCC 13, G++ 13)
- CMake 3.10+
- Linux (for CPU affinity) – other platforms fall back gracefully

# Updated Version 2

## Architecture
┌─────────────────────────────────────────────────────────────────────────────┐
│                             INPUT LAYER                                     │
│  ┌───────────────────────────────────────────────────────────────────────┐  │
│  │                                                                        │  │
│  │   ┌────────────────────┐          ┌─────────────────────────┐        │  │
│  │   │   OrderIngress     │──────────▶│  OrderIngress::submit  │        │  │
│  │   │                    │          │                         │        │  │
│  │   │  ┌──────────────┐  │          │  ┌───────────────────┐ │        │  │
│  │   │  │  MPSCQueue   │  │          │  │   Order Object    │ │        │  │
│  │   │  │  RingBuffer  │  │          │  │  (GTC/IOC/FOK/   │ │        │  │
│  │   │  │  (Lock-free) │  │          │  │   GTD/GTT/DAY)   │ │        │  │
│  │   │  └──────────────┘  │          │  └───────────────────┘ │        │  │
│  │   └────────────────────┘          └─────────────────────────┘        │  │
│  │                                                                        │  │
│  └───────────────────────────────────────────────────────────────────────┘  │
└─────────────────────────────────────────────────────────────────────────────┘
                                      │
                                      ▼
┌─────────────────────────────────────────────────────────────────────────────┐
│                          CORE ENGINE LAYER                                  │
│  ┌───────────────────────────────────────────────────────────────────────┐  │
│  │                                                                        │  │
│  │   ┌──────────────────────────────────────────────────────────────┐    │  │
│  │   │              MatchingEngineThread (Worker Thread)             │    │  │
│  │   │                                                               │    │  │
│  │   │  ┌─────────────────────────────────────────────────────────┐ │    │  │
│  │   │  │                  MatchingEngine                         │ │    │  │
│  │   │  │                                                         │ │    │  │
│  │   │  │  ┌───────────────────┐    ┌─────────────────────────┐  │ │    │  │
│  │   │  │  │     Order Book    │    │    Order Book (Asks)    │  │ │    │  │
│  │   │  │  │      (Bids)       │    │    (Price: Ascending)   │  │ │    │  │
│  │   │  │  │ (Price:Descending)│    │                         │  │ │    │  │
│  │   │  │  │                   │    │  ┌───────────────────┐  │  │ │    │  │
│  │   │  │  │  ┌─────────────┐  │    │  │   Price Level    │  │  │ │    │  │
│  │   │  │  │  │ Price Level │  │    │  │   (100.50)       │  │  │ │    │  │
│  │   │  │  │  │   (100.00)  │  │    │  │  ┌─────────────┐ │  │  │ │    │  │
│  │   │  │  │  │  ┌───────┐  │  │    │  │  │  Order 1    │ │  │  │ │    │  │
│  │   │  │  │  │  │Order 1│  │  │    │  │  │  Order 2    │ │  │  │ │    │  │
│  │   │  │  │  │  │Order 2│  │  │    │  │  │  Order 3    │ │  │  │ │    │  │
│  │   │  │  │  │  │Order 3│  │  │    │  │  └─────────────┘ │  │  │ │    │  │
│  │   │  │  │  │  └───────┘  │  │    │  └───────────────────┘  │  │ │    │  │
│  │   │  │  │  └─────────────┘  │    └─────────────────────────┘  │ │    │  │
│  │   │  │  └───────────────────┘                                 │ │    │  │
│  │   │  │                                                         │ │    │  │
│  │   │  │  ┌─────────────────────────────────────────────────────┐│ │    │  │
│  │   │  │  │          ExpiryScheduler (Time Wheel)              ││ │    │  │
│  │   │  │  │  ┌─────────────────┐      ┌─────────────────────┐  ││ │    │  │
│  │   │  │  │  │   Daily Wheel   │      │   Second Wheel      │  ││ │    │  │
│  │   │  │  │  │   (366 slots)   │      │   (86400 slots)     │  ││ │    │  │
│  │   │  │  │  │  ┌───────────┐  │      │  ┌───────────────┐  │  ││ │    │  │
│  │   │  │  │  │  │  Day 0    │  │      │  │  Second 0     │  │  ││ │    │  │
│  │   │  │  │  │  │  Day 1    │  │      │  │  Second 1     │  │  ││ │    │  │
│  │   │  │  │  │  │  Day 2    │  │      │  │  Second 2     │  │  ││ │    │  │
│  │   │  │  │  │  │   ...     │  │      │  │   ...         │  │  ││ │    │  │
│  │   │  │  │  │  │  Day 365  │  │      │  │  Second 86399 │  │  ││ │    │  │
│  │   │  │  │  │  └───────────┘  │      │  └───────────────┘  │  ││ │    │  │
│  │   │  │  │  └─────────────────┘      └─────────────────────┘  ││ │    │  │
│  │   │  │  └─────────────────────────────────────────────────────┘│ │    │  │
│  │   │  └─────────────────────────────────────────────────────────┘ │    │  │
│  │   └──────────────────────────────────────────────────────────────┘    │  │
│  │                                                                        │  │
│  │   ┌──────────────────────────────────────────────────────────────┐    │  │
│  │   │                    Performance Metrics                        │    │  │
│  │   │  ┌─────────────────┐  ┌─────────────────┐  ┌───────────────┐ │    │  │
│  │   │  │ LatencyCollector│  │  Trade Counter  │  │  Memory Pool  │ │    │  │
│  │   │  │  (P50/P99/Max)  │  │  (Atomic)       │  │  (Lock-free)  │ │    │  │
│  │   │  └─────────────────┘  └─────────────────┘  └───────────────┘ │    │  │
│  │   └──────────────────────────────────────────────────────────────┘    │  │
│  └───────────────────────────────────────────────────────────────────────┘  │
└─────────────────────────────────────────────────────────────────────────────┘
                                      │
                                      ▼
┌─────────────────────────────────────────────────────────────────────────────┐
│                            EVENT LAYER                                      │
│  ┌───────────────────────────────────────────────────────────────────────┐  │
│  │                                                                        │  │
│  │   ┌────────────────────┐          ┌─────────────────────────┐        │  │
│  │   │   EventPublisher   │──────────▶│       EventBus          │        │  │
│  │   │                    │          │                         │        │  │
│  │   │  ┌──────────────┐  │          │  ┌───────────────────┐ │        │  │
│  │   │  │ publish()    │  │          │  │  Subscriber List  │ │        │  │
│  │   │  │  TradeEvent  │  │          │  │  (Observer Pattern)│ │        │  │
│  │   │  │  BookUpdate  │  │          │  └───────────────────┘ │        │  │
│  │   │  │  Cancelled   │  │          │                         │        │  │
│  │   │  │  Rejected    │  │          │  ┌───────────────────┐ │        │  │
│  │   │  └──────────────┘  │          │  │  Event Filtering  │ │        │  │
│  │   └────────────────────┘          │  │  (interested_in)  │ │        │  │
│  │                                   │  └───────────────────┘ │        │  │
│  │                                   └───────────┬─────────────┘        │  │
│  └───────────────────────────────────────────────┼───────────────────────┘  │
└─────────────────────────────────────────────────┼───────────────────────────┘
                                                  │
                    ┌─────────────────────────────┼─────────────────────────────┐
                    │                             │                             │
                    ▼                             ▼                             ▼
┌───────────────────────────┐ ┌──────────────────────────┐ ┌──────────────────────────┐
│     CONSUMERS &           │ │     CONSUMERS &          │ │     CONSUMERS &          │
│     PERSISTENCE           │ │     PERSISTENCE          │ │     PERSISTENCE          │
│                           │ │                          │ │                          │
│  ┌─────────────────────┐  │ │  ┌─────────────────────┐ │ │  ┌─────────────────────┐ │
│  │   TradeLogger       │  │ │  │  MarketDataService  │ │ │  │   TextWriter        │ │
│  │                     │  │ │  │                     │ │ │  │                     │ │
│  │  ┌───────────────┐  │  │ │  │  ┌───────────────┐  │ │ │  │  ┌───────────────┐  │ │
│  │  │  on_event()   │  │  │ │  │  │  on_event()   │  │ │ │  │  │  on_event()   │  │ │
│  │  │  (TradeEvent) │  │  │ │  │  │ (BookUpdate)  │  │ │ │  │  │  (All Events) │  │ │
│  │  └───────────────┘  │  │ │  │  └───────────────┘  │ │ │  │  └───────────────┘  │ │
│  │                     │  │ │  │                     │ │ │  │                     │ │
│  │  ┌───────────────┐  │  │ │  │  ┌───────────────┐  │ │ │  │  ┌───────────────┐  │ │
│  │  │  AsyncLogger  │  │  │ │  │  │  latest()     │  │ │ │  │  │  Journal File │  │ │
│  │  │  (Non-block)  │  │  │ │  │  │  Best Bid/Ask │  │ │ │  │  │  (Persistence)│  │ │
│  │  └───────────────┘  │  │ │  │  │  Last Trade   │  │ │ │  │  └───────────────┘  │ │
│  │                     │  │ │  │  └───────────────┘  │ │ │  │                     │ │
│  │  ┌───────────────┐  │  │ │  └─────────────────────┘ │ │  │  ┌───────────────┐  │ │
│  │  │  Trade Log    │  │  │ │                          │ │  │  │  Snapshot     │  │ │
│  │  │  (File)       │  │  │ │                          │ │  │  │  Service      │  │ │
│  │  └───────────────┘  │  │ │                          │ │  │  │  (State Save) │  │ │
│  └─────────────────────┘  │ │                          │ │  │  └───────────────┘  │ │
│                           │ │                          │ │  │                     │ │
│                           │ │                          │ │  │  ┌───────────────┐  │ │
│                           │ │                          │ │  │  │  ReplayEngine │  │ │
│                           │ │                          │ │  │  │  (Recovery)   │  │ │
│                           │ │                          │ │  │  └───────────────┘  │ │
│                           │ │                          │ │  └─────────────────────┘ │
└───────────────────────────┘ └──────────────────────────┘ └──────────────────────────┘
                    │                             │                             │
                    └─────────────────────────────┼─────────────────────────────┘
                                                  │
                                                  ▼
┌─────────────────────────────────────────────────────────────────────────────┐
│                         INFRASTRUCTURE LAYER                               │
│  ┌───────────────────────────────────────────────────────────────────────┐  │
│  │                                                                        │  │
│  │   ┌────────────────────┐  ┌────────────────────┐  ┌───────────────┐  │  │
│  │   │    RingBuffer      │  │    MemoryPool      │  │  AsyncLogger  │  │  │
│  │   │                    │  │                    │  │               │  │  │
│  │   │  ┌──────────────┐  │  │  ┌──────────────┐  │  │ ┌───────────┐ │  │  │
│  │   │  │ Lock-free    │  │  │  │ Pre-allocated│  │  │ │Background │ │  │  │
│  │   │  │ MPSC Queue   │  │  │  │ Object Pool  │  │  │ │ Thread    │ │  │  │
│  │   │  │ Cache-aligned│  │  │  │ LRU Strategy │  │  │ │ Batch     │ │  │  │
│  │   │  │ Atomic Ops   │  │  │  │ Type-safe    │  │  │ │ Write     │ │  │  │
│  │   │  └──────────────┘  │  │  └──────────────┘  │  │ └───────────┘ │  │  │
│  │   └────────────────────┘  └────────────────────┘  └───────────────┘  │  │
│  │                                                                      │  │
│  │   ┌────────────────────┐  ┌────────────────────┐  ┌───────────────┐  │  │
│  │   │  CPU Affinity      │  │  CacheAligned      │  │  Clock        │  │  │
│  │   │                    │  │                    │  │               │  │  │
│  │   │  ┌──────────────┐  │  │  ┌──────────────┐  │  │ ┌───────────┐ │  │  │
│  │   │  │ Pin Thread   │  │  │  │ False Sharing│  │  │ │ now_ns()  │ │  │  │
│  │   │  │ to Core      │  │  │  │ Prevention   │  │  │ │ Timestamp │ │  │  │
│  │   │  │ (Linux)      │  │  │  │ (64-byte     │  │  │ └───────────┘ │  │  │
│  │   │  └──────────────┘  │  │  │  alignment)  │  │  │               │  │  │
│  │   └────────────────────┘  │  └──────────────┘  │  │               │  │  │
│  │                            └────────────────────┘  └───────────────┘  │  │
│  └───────────────────────────────────────────────────────────────────────┘  │
└─────────────────────────────────────────────────────────────────────────────┘

## Key Components

| Component | Description |
|-----------|-------------|
| `MatchingEngine` | Core matching logic with price-time priority |
| `MatchingEngineThread` |	Worker thread with batch processing and CPU affinity |
| `ExpiryScheduler` |	Time-wheel based order expiration manager |
| `EventBus` |	Central event dispatcher with observer pattern |
| `EventPublisher` |	Event publishing interface with timestamp generation |
| `RingBuffer<T>` |	Lock-free MPSC queue with cache-aligned head/tail |
| `MemoryPool<T>` |	Fixed-size object pool with O(1) allocation |
| `LRUPool<T>` |	Memory pool with LRU recycling policy |
| `LatencyCollector` |	Percentile (P50/P99) and max latency statistics |
| `AsyncLogger` |	Non-blocking file logger with background thread |
| `OrderIngress` |	Order submission entry point with MPSC queue |
| `TextWriter` |	Event persistence to journal file |
| `SnapshotService` |	Order book state snapshot capture |
| `ReplayEngine` |	System recovery from journal + snapshots |
| `PriceLevel` |	Order book price node with doubly-linked list |
| `Order` |	Core order data structure with expiry support |
| `CacheAligned<T>` |	False-sharing prevention with 64-byte alignment |
| `pin_thread_to_core` |	CPU affinity utility for Linux platforms |

## Performance Benchmarks

Results from running on a dedicated CPU core (Linux):

| Metric | Value |
|--------|-------|
| **Throughput (Standard)** | 4.06 million orders/sec |
| **Throughput (Batch)** | 4.38 million orders/sec |
| **P50 Queue Latency** | 218.71 ms |
| **P99 Queue Latency** | 468.08 ms |
| **Max Queue Latency** | 470.11 ms |
| **P50 Match Duration** | 0 ns (below measurement resolution) |
| **P99 Match Duration** | 100 ns |
| **Max Match Duration** | 69.1 µs |
| **P50 Total Latency (end to end)** | 218.71 ms |
| **P99 Total Latency (end to end)** | 468.08 ms |

*Test configuration: 5 million trade pairs (10 million orders)*

![alt text](image2match.png)

![alt text](image2event.png)

## What's new

- 1. Matching Engine – 4M+ orders/sec with 100ns P99 match latency

Designed deterministic price-time priority matching using cache-optimized std::vector + intrusive linked list (replaced std::map for better cache locality)

Implemented 6 order types (GTC/GTD/GTT/DAY/IOC/FOK) with time-wheel based expiry management (366-day wheel + 86,400-second wheel)

Built lock-free MPSC ring buffer for concurrent order ingestion with cache-aligned head/tail to prevent false sharing

Developed custom memory pool with O(1) allocation, eliminating dynamic allocation in hot path

Achieved 4.05M orders/sec throughput with P99 match duration of 100ns (peak: 69µs)

Optimized via CPU affinity (dedicated core) and batch processing (4.38M orders/sec in batch mode)

- 2. Event Bus – 6.9M+ events/sec with 197ns per-event latency

Built event-driven architecture with std::variant-based typed events (Trade, BookUpdate, OrderCancelled, OrderRejected)

Implemented observer pattern with event filtering (interested_in) for fine-grained subscription control

Achieved 6.99M mixed events/sec to 4 subscribers, 5.05M Trade events/sec to 5 subscribers

Average per-event latency: 197ns (subscriber dispatch overhead)

Designed for extensibility – new event types/subscribers can be added without modifying core dispatcher

- 3. Persistence & Replay – Journaling and state recovery (in progress)

Implemented event journaling via TextWriter with async file I/O (non-blocking persistence)

Designed snapshot service for order book state capture to enable fast recovery

Built replay engine to reconstruct system state from journal + snapshots

Currently validating integration: end-to-end persistence + replay tests passing

Next: crash recovery scenarios and performance tuning for high-throughput replay

# By Jeremy