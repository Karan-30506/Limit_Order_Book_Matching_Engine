# Limit Order Book Matching Engine

A C++17 command-line limit order book that processes JSON order requests sequentially, records trades, persists a checksummed event log, and validates the reconstructed state using Worklet 3's replay engine.

## What is integrated

- **Worklet 1:** intrusive FIFO order queues, AVL price-level trees, indexed order/book storage and best-price access.
- **Worklet 2:** limit and market matching, price-time priority, JSON parsing and the command-line application.
- **Worklet 3:** asynchronous event persistence, bounded market-data feed, event-sourced replay, live-book consistency validation and performance metrics.

Worklet 3 is included in the shared `lob_engine` library and used by the command-line application. Its replay engine is the sole C++ replay implementation. The older Worklet 2 snapshot exporter was removed. The existing browser UI files remain in the repository but are not part of the current application flow; Qt is future work.

## Requirements and build

- CMake 3.15 or later
- A C++17 compiler
- Threads support
- Python 3 only for generating random input

From the project root:

```powershell
cmake -S . -B build -G "MinGW Makefiles"
cmake --build build
ctest --test-dir build --output-on-failure
```

The build produces `build/matchingengine.exe` and the unified `lob_tests` CTest target. Tests for the data structures, matching rules, event log, feed, replay, parser and performance summary live in one consolidated test executable.

## Run orders

```powershell
.\build\matchingengine.exe .\tests\data\02_limit_full_match.json
```

The input is a JSON array with `id`, `type`, `side`, `price` and `quantity` fields. `type` is `LIMIT` or `MARKET`; `side` is `BUY` or `SELL`. A market order's price is ignored (the included datasets use zero). Wrong field types and malformed individual records are written as parser rejections instead of terminating the process.

The executable checks each accepted order/trade event stream by replaying it and comparing the resulting book with the live book. A mismatch or replay error returns a failure.

### Generated output

Output is written under `log/` using the input filename (without extension):

- `<name>_logs.log` — detailed per-order results and final summary.
- `<name>_reject.log` — parser- and engine-level rejections.
- `<name>_events.log` — checksummed, sequential Worklet 3 event log.
- `<name>_latency.csv` — per parsed order's engine handling time, outcome and rejection reason.

The event and output logs are overwritten for each run of the same input basename. The event file can be replayed independently:

```powershell
.\build\matchingengine.exe --replay .\log\02_limit_full_match_events.log
```

The console summary reports average and p50/p95/p99/max engine-handling latency, submitted orders per second and trades per second. These are local sequential CLI measurements: individual latency includes matching, event logging and feed publication; throughput covers the request loop, including per-order log formatting, and excludes initial JSON parsing and final flush/replay validation. Do not treat them as production or concurrent-server benchmarks.

## Supplied scenarios

The `tests/data/` directory contains:

| File                                               | Scenario                                                  |
| -------------------------------------------------- | --------------------------------------------------------- |
| `01_limit_orders_resting.json`                   | Non-crossing limit orders rest in the book                |
| `02_limit_full_match.json`                       | Limit-order matches                                       |
| `03_limit_partial_fill_remainder.json`           | Partial fills and limit remainders                        |
| `04_market_buy_multilevel.json`                  | Market buys across ask levels                             |
| `05_market_sell_multilevel.json`                 | Market sells across bid levels                            |
| `06_market_insufficient_liquidity.json`          | Market orders with insufficient liquidity                 |
| `07_same_price_fifo.json`                        | FIFO queue priority at a shared price                     |
| `stress_test_10000.json`                         | 10,000-request functional stress dataset                  |
| `test_random_250.json`, `test_random_300.json` | Randomized datasets                                       |
| `sample_replay.json`                             | Sample replay JSON retained for the legacy browser viewer |

Run all seven designed scenarios, for example:

```powershell
.\build\matchingengine.exe .\tests\data\01_limit_orders_resting.json
.\build\matchingengine.exe .\tests\data\02_limit_full_match.json
.\build\matchingengine.exe .\tests\data\03_limit_partial_fill_remainder.json
.\build\matchingengine.exe .\tests\data\04_market_buy_multilevel.json
.\build\matchingengine.exe .\tests\data\05_market_sell_multilevel.json
.\build\matchingengine.exe .\tests\data\06_market_insufficient_liquidity.json
.\build\matchingengine.exe .\tests\data\07_same_price_fifo.json
```

The generator is [`generate_test_cases.py`](tests/generate_test_cases.py):

```powershell
python tests\generate_test_cases.py 10000
python tests\generate_test_cases.py 500 --seed 7
python tests\generate_test_cases.py 500 --output .\tests\data\my_stress_test.json
```

## Project layout

```text
Limit_Order_Book/
  include/                      Worklet 1 headers
  src/                          Worklet 1 implementations
Matching_Engine/
  include/                      Worklet 2 headers and JSON dependency
  src/                          Worklet 2 implementations and CLI entry point
Testing_and_Performance/
  include/                      Worklet 3 headers
  src/                          Worklet 3 implementations and metrics
tests/
  test_main.cpp                 Consolidated C++ regression tests
  generate_test_cases.py        Random JSON dataset generator
  data/                         All scenario, stress and sample replay JSON files
visualizer/                     Legacy browser UI; its sample data is in tests/data/
```

## Matching rules

The best opposing price is matched first; orders at the same price are matched FIFO. Trades execute at the resting order's price. An unmatched limit remainder rests in the book. An unmatched market remainder is discarded.
