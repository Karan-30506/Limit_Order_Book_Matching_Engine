# Limit Order Book Matching Engine

A C++17 command-line limit order book that processes JSON order requests sequentially and records trades.

## Integrated worklets

- **Worklet 1:** FIFO order queues, AVL price-level trees, indexed order/book storage and best-price access.
- **Worklet 2:** limit and market matching, price-time priority, JSON parsing and the command-line application.
- **Worklet 3:** event persistence, market-data feed, replay reconstruction/validation and performance support.

The project builds one CLI executable from one configure/build sequence. Choose the full-system or engine-only test path with a command-line argument. Worklet 3 is linked into the executable, but the engine-only path does not invoke its event, feed or replay functionality.

## Build and test

Requirements: CMake 3.15+, a C++17 compiler, and threads support. Python 3 is only needed to generate random input.

From the project root, configure and build:

```powershell
cmake -S . -B build -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure
```

The build produces `build/matchingengine.exe` and the consolidated `lob_tests` CTest target.

## Run modes

### Full-system test (Worklets 1–3)

```powershell
.\build\matchingengine.exe --test-system .\tests\data\02_limit_full_match.json
```

This runs matching through Worklet 3's event logger/feed integration, then reconstructs the events and checks the replayed final book against the live book. Replay and validation happen after submission timing. Event and detailed order logs are written under `log/`.

### Engine-only test (Worklets 1–2; Worklet 3 behavior skipped)

```powershell
.\build\matchingengine.exe --engine-only .\tests\data\02_limit_full_match.json
```

This uses the same executable as full-system testing. It performs no event logging, feed publication/draining or replay validation. Worklet 3 code is still present in the executable; this mode means that functionality is not used, not that Worklet 3 is absent from the binary.

Both runners accept the same JSON array format: each order has `id`, `type`, `side`, `price` and `quantity`. `type` is `LIMIT` or `MARKET`; `side` is `BUY` or `SELL`. A market order's price is ignored. Invalid records are reported as parser rejections.

Both runners measure the wall-clock duration of the sequential processing loop, starting immediately before the first valid order is submitted and ending after the last submission and its mode-specific processing. Parsing, setup, per-order report/file formatting, output, and final report generation are excluded. Both include matching, fills, and insertion/update of unmatched limit remainders. The full-system mode additionally includes Worklet 3 event publication and feed draining. Event-file flush and replay reconstruction/validation happen after the timer stops.

The CLI defines one transaction as **one structurally valid order request submitted to the matching engine**. The `Throughput` section reports submissions/s (TPS), accepted orders/s and trade executions/s, all divided by the same timed processing duration. The separate `Transactions per second (TPS)` field repeats the submissions/s value for clarity; the two values must match. TPS includes engine-rejected submissions because they still consume a submission call. `Accepted orders/s` excludes rejected submissions. `Trade executions/s` counts individual matches, so one order can produce multiple trades and an unmatched resting limit order can produce no trade. These rates are different measures. Results depend on workload, compiler, build configuration and hardware and are not a general capacity guarantee.

Per-order latency is measured separately around each `submitOrderDetailed` call (or its full-system integration wrapper) and reported as mean, p50, p95, p99 and max. It does not include parsing, feed draining, report formatting or replay. The TPS timer is a wall-clock batch measurement, not the sum of those per-order latency samples. The processing loop does include metric collection; full-system mode also includes feed draining and event publication/enqueueing, but not waiting for the event log flush to finish.

## CLI output reference

Each order's detail block shows:

- `ORDER ID`, `Type`, `Side`, `Limit Price`, and `Requested Quantity`: the submitted request fields (limit price is omitted for market orders).
- The `Trades generated` list: each `Trade ID`, participating buy/sell order IDs, execution price and quantity. A resting order's price determines the trade price.
- `Order Outcome`: whether the request was rejected, fully filled, partially filled/executed, left open in the book, or had an unfilled market remainder discarded. `Executed Quantity`, `Remaining Quantity`/`Unfilled Quantity Discarded`, and `Resting in Book` explain the final disposition.

The execution summary attributes mean:

- `Test mode`: whether the run included Worklet 3 (full system) or linked Worklets 1–2 only (engine only).
- `Valid order submissions`: structurally valid records passed to the engine; malformed records are not included.
- `Parser rejections` / `Engine rejections`: invalid input records / valid requests refused by matching validation.
- `Accepted orders`: successful engine submissions, including limit orders that rest without trading.
- `Recorded trades`: individual matching executions, not submitted orders.
- `Orders resting after run`: open orders remaining in the book.
- `Replay validation`: whether Worklet 3 reconstructed the event log and confirmed its final book matches the live book. This is not run in engine-only mode.
- `Timed processing duration`: elapsed wall time of the batch processing loop; exclusions and mode-specific inclusions are described above.
- `Submission latency`: per-call mean and tail percentiles. p50 is the median; p95/p99 are the latency at or below which 95%/99% of sampled calls fall; max is the slowest sampled call.
- `Transactions per second (TPS)`: valid submissions divided by timed processing duration. This project explicitly counts an order request as a transaction.
- `Throughput`: the same TPS value plus accepted orders/s and trade executions/s. All rates share one denominator; the separate TPS line should equal the submissions/s value here.
- `Accepted orders/s` and `Trade executions/s`: accepted submissions or generated trades divided by the same timed duration.
- `Per-order latency CSV`, `Event log`, and `Feed`: output paths and Worklet 3 event publication/read/missed-event counts when available.

The terminal shows a compact, aligned run header and summary so large inputs do not flood the console. Full per-order detail is written to `<name>_logs.log` in the `log/` folder.

## Generated output

Output is written under `log/` using the input filename (without extension):

- `<name>_logs.log` — detailed per-order results and execution summary.
- `<name>_reject.log` — parser- and engine-level rejections.
- `<name>_latency.csv` — per-order submission latency, outcome and rejection reason.
- `<name>_events.log` — emitted by the full-system runner; sequential Worklet 3 events.

Files for a matching input basename are overwritten by the next run. The full-system CLI validates its event stream before reporting success.

Run the consolidated automated tests with:

```powershell
ctest --test-dir build --output-on-failure
```

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

Run all seven designed scenarios in full-system mode from PowerShell:

```powershell
$scenarios = @(
  "01_limit_orders_resting.json",
  "02_limit_full_match.json",
  "03_limit_partial_fill_remainder.json",
  "04_market_buy_multilevel.json",
  "05_market_sell_multilevel.json",
  "06_market_insufficient_liquidity.json",
  "07_same_price_fifo.json"
)
foreach ($scenario in $scenarios) {
  .\build\matchingengine.exe --test-system ".\tests\data\$scenario"
}
```

For engine-only testing, run the same loop with `matchingengine.exe --engine-only` in place of `matchingengine.exe --test-system`.

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
