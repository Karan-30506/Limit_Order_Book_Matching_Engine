# Worklet implementation and verification

Reviewed on `qt_gui_implementation`; updated after integrating Worklet 3.

## Worklet 1 — Data structures

In `Limit_Order_Book/include/` and `Limit_Order_Book/src/`:

- `Order` and `OrderQueue` provide intrusive, FIFO queues and maintain remaining-quantity aggregates.
- `LimitNode` and `AVLTree` store balanced, ordered price levels.
- `Book` maintains separate buy/sell trees, order and price indexes, cached best prices, and insertion, cancellation, modification, volume and traversal operations.
- `HashMap` provides an integral-key separate-chaining index.

The unified test target exercises hash-map growth/removal, AVL insertion/traversal/deletion, book FIFO, volume, modification and cancellation.

## Worklet 2 — Matching engine

In `Matching_Engine/include/` and `Matching_Engine/src/`:

- `BaseOrderMatcher` shares the fill loop; the limit matcher enforces limit prices and rests remainders; the market matcher takes available liquidity and discards any unfilled remainder.
- The engine selects best opposing prices and queue heads, giving price-time priority, and records trades at resting prices.
- The JSON parser and command-line application handle sequential input, validation, rejection logs and detailed order output.
- The parser now checks JSON field types before converting them, so invalid per-record types produce parser rejections instead of an uncaught JSON exception.

The legacy Worklet 2 `ReplayExporter` and its snapshot JSON output were removed to avoid maintaining a second replay implementation.

## Worklet 3 — Events, feed, replay and performance

In `Testing_and_Performance/include/` and `Testing_and_Performance/src/`:

- `EventLogger` assigns ordered sequence numbers/timestamps, checksums event records, persists them asynchronously and exposes explicit `flush()` error reporting.
- `EventLoggerIntegration` routes successful submissions through the matching engine, logs incoming orders and trades, handles cancellation/modification, and publishes logged events to the market-data feed.
- `MarketDataFeed` is a bounded ring buffer with consumer registration, batch reads, overflow recovery and statistics.
- `ReplayEngine` is now the sole C++ replay engine. It reconstructs a book from the event stream and validates sequence, trades, price-time priority and book state. The CLI compares replayed state with the live book at the end of each run.
- `PerformanceMetrics` records each parsed order's engine-handling latency to CSV and summarizes mean, p50, p95, p99 and max latency, order throughput and trade throughput.

The event log for each CLI run is fresh (truncated at startup), because a run creates a fresh book; appending events from a different run would make replay inconsistent. Use `matchingengine --replay <events.log>` to independently replay a saved event log.

## Unified verification

Tests for all worklets are consolidated in `tests/test_main.cpp` and exposed as one CTest test (`lob_tests`). Scenario inputs, random datasets and the legacy viewer's sample replay are centralized in `tests/data/`. The test covers data structures, FIFO and matching/partial fill behavior, event persistence/readback, feed consumption, modify/cancel, market remainders, replay/live-book consistency, replay priority rejection, parser wrong-type rejection, and performance summary calculations.

Verification after integration:

- CMake configure and full build succeeded with C++17.
- CTest: 1/1 test passed.
- All seven designed datasets ran through the event logger, feed and Worklet 3 replay; every replayed book matched the live book.
- `tests/data/stress_test_10000.json`: 10,000 accepted orders, 9,682 trades, and a matching replayed book.
- Malformed `type: 5` was recorded as one parser rejection; the CLI exited normally.
- Standalone replay of a generated event file succeeded.
- The final tested 10,000-order CLI run reported roughly 60,900 submitted orders/s and 59,000 trades/s; this is a single local run, not a standardized benchmark.

Throughput includes the request loop and per-order log formatting. Per-order latency includes matching, event logging and feed publication. These measurements exclude JSON parsing and final event flush/replay validation and should not be read as production capacity claims.

## Still out of scope

- The static HTML/CSS/JavaScript visualizer is deliberately left untouched and disconnected from the current application flow. No Qt GUI exists yet.
- The unified tests are focused regression tests, not exhaustive randomized/property tests for every AVL/hash-map invariant.
- There is no controlled multi-run benchmark harness, concurrency test, load model or machine/compiler metadata capture yet; see [`feedback.md`](feedback.md).
