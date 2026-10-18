# Worklet 3: Event Logging, Replay, Market Data Feed & Benchmarking

Worklet 3 extends the C++17 Limit Order Book Matching Engine with event logging, deterministic replay, market data distribution, and benchmarking. It builds on the order book and matching engine implemented in Worklets 1 and 2.

## Features

### 1. Event Logger
- Records order additions, cancellations, modifications, and trade events.
- Maintains event sequence numbers, timestamps, and checksums.
- Supports event persistence and reading stored events.
- Provides event history for replay and recovery.

### 2. Replay Engine
- Reconstructs order book state by replaying recorded events.
- Supports replaying events and replaying up to a specified sequence.
- Handles order operations and trade executions supported by the event format.
- Validates event sequence ordering.
- Provides replay snapshots for inspecting reconstructed order-book state.

### 3. Market Data Feed
- Publishes order-book updates and trade events to consumers.
- Uses a fixed-capacity ring buffer.
- Preserves event ordering.
- Supports single-event and batch consumption.
- Tracks buffer overflow and missed events.
- Provides consumer registration and recovery functionality.
- Collects feed statistics and publication latency information.

### 4. Benchmarking
- Measures matching-engine operation performance.
- Reports latency and throughput metrics supported by the benchmark implementation.
- Includes workloads for insertion, matching, partial fills, cancellation, multi-level matching, and mixed operations.

## Project Structure

```text
worklet-3-dummy-repo-lbm/
├── worklet3/
│   ├── include/
│   │   ├── Event.hpp
│   │   ├── EventLogger.hpp
│   │   ├── EventLoggerIntegration.hpp
│   │   └── MarketDataFeed.hpp
│   ├── src/
│   │   ├── EventLogger.cpp
│   │   ├── EventLoggerIntegration.cpp
│   │   ├── EventPersistance.cpp
│   │   ├── EventSerialization.cpp
│   │   └── MarketDataFeed.cpp
│   ├── tests/
│   │   ├── benchmark_tests.cpp
│   │   ├── event_logger_replay_integration_tests.cpp
│   │   ├── market_data_feed_tests.cpp
│   │   ├── replay_engine_tests.cpp
│   │   ├── replay_tests.hpp
│   │   └── test_main.cpp
│   ├── Makefile
│   └── README.md
├── include/
├── src/
├── worklet2/
│   ├── include/
│   └── src/
└── Makefile
```

## Building

### Requirements
- C++17-compatible compiler, such as GCC or Clang
- GNU Make
- POSIX threads support

### Build the project

Run from the repository root:

```bash
make
```

This creates two executables in the `build/` directory:

- `build/worklet3_tests` — integrated test suite
- `build/worklet3_benchmarks` — benchmarking executable

### Run tests

```bash
make test
```

### Run benchmarks

```bash
./build/worklet3_benchmarks
```

### Clean build files

```bash
make clean
```

This removes the generated test and benchmark executables.

## Usage

### Event Logging

The Event Logger records events generated during order-book operations. The recorded events can be inspected, persisted, and used for replay.

Typical workflow:
1. Initialize the Event Logger.
2. Connect event logging to the matching engine.
3. Perform order operations.
4. Retrieve or persist the recorded events.

### Market Data Feed

The Market Data Feed distributes events to registered consumers.

Typical workflow:
1. Create the feed with a fixed buffer capacity.
2. Register consumers.
3. Publish events as order-book changes occur.
4. Read events individually or in batches.
5. Monitor overflow and missed-event statistics.

### Replay

The Replay Engine reconstructs order-book state using recorded events.

Typical workflow:
1. Load the event history.
2. Initialize the Replay Engine.
3. Replay the events.
4. Inspect the resulting snapshot.
5. Compare the replayed state with the live order book if supported by the implementation.

Refer to the header files for exact APIs and supported options.

## Testing

Run the integrated test suite:

```bash
make test
```

The test suite contains tests for:
- Event logging
- Event Logger and Replay integration
- Replay Engine
- Market Data Feed
- Benchmarking

## Benchmarks

Run:

```bash
./build/worklet3_benchmarks
```

The benchmark workloads evaluate different matching-engine operations.

| Workload | Purpose |
|---|---|
| Order insertion | Measures order insertion performance |
| Successful matching | Measures matching performance |
| Partial fills | Evaluates partially executed orders |
| Cancellation | Measures cancellation performance |
| Multi-level matching | Evaluates matching across price levels |
| Mixed operations | Evaluates a combination of operations |

Results depend on the workload, compiler settings, and hardware.

## Design Principles

- **Event ordering:** Sequence numbers help maintain event order.
- **Bounded storage:** The Market Data Feed uses a fixed-capacity ring buffer.
- **Observability:** Feed statistics expose overflow and missed events.
- **Replayability:** Recorded events support reconstruction of order-book state.
- **Modularity:** Logging, replay, feed distribution, and benchmarking are separated into components.
- **Testability:** Dedicated tests validate component behavior.

## Known Limitations

- The Event Logger's memory usage depends on its event-retention strategy.
- The Market Data Feed has finite capacity, so slow consumers may miss events.
- Replay correctness depends on valid, correctly ordered event data.
- Benchmark results vary by environment.

## Conclusion

Worklet 3 adds event logging, replay, market data distribution, and benchmarking to the Limit Order Book Matching Engine. Together, these components improve observability, support order-book reconstruction, distribute events to consumers, and help evaluate engine performance.