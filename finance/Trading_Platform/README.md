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
![alt text](image.png)

## Building

### Prerequisites

- C++20 compatible compiler (GCC 13, G++ 13)
- CMake 3.10+
- Linux (for CPU affinity) – other platforms fall back gracefully

# By Jeremy