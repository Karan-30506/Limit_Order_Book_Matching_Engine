# Limit Order Book Matching Engine

A C++17 limit order book and matching engine for processing limit and market orders. The repository integrates three worklets: the order-book data structures, order matching and CLI, and event/replay/performance tooling. It provides a command-line application and an optional Qt 6 desktop GUI.

## What the worklets do

### Worklet 1 — `Limit_Order_Book`

- Maintains buy and sell sides as AVL trees of price levels.
- Stores orders at each price in FIFO queues to preserve time priority.
- Provides book operations, best-price access and order lookup/storage.

### Worklet 2 — `Matching_Engine`

- Validates and parses JSON order requests.
- Matches market and limit orders against the best available opposing prices.
- Applies price-time priority: best price first, FIFO within a price level.
- Executes trades at the resting order's price, rests unmatched limit remainders, and discards unfilled market remainders.
- Supplies the shared CLI executable and the engine-only execution path.

### Worklet 3 — `Testing_and_Performance`

- Records and serializes matching events.
- Integrates event capture and the market-data feed with the matching engine.
- Reconstructs and validates the book by replaying recorded events.
- Collects per-order latency and batch throughput metrics and writes latency CSV reports.

The CLI builds one executable, `matchingengine.exe`. Use `--engine-only` to run the matching engine without Worklet 3 runtime activity, or `--test-system` to include event/feed processing and replay validation.

## Repository structure

```text
.
|-- CMakeLists.txt
|-- README.md
|-- GUI/
|   |-- include/
|   |   |-- CsvReader.hpp
|   |   |-- CsvTableModel.hpp
|   |   |-- CsvWorker.hpp
|   |   |-- EngineService.hpp
|   |   |-- EngineWorker.hpp
|   |   `-- MainWindow.hpp
|   `-- src/
|       |-- CsvTableModel.cpp
|       |-- CsvWorker.cpp
|       |-- EngineService.cpp
|       |-- EngineWorker.cpp
|       |-- MainWindow.cpp
|       `-- main.cpp
|-- Limit_Order_Book/
|   |-- include/
|   |   |-- AVLTree.hpp
|   |   |-- Book.hpp
|   |   |-- Hashmap.hpp
|   |   |-- Hashmap.tpp
|   |   |-- LimitNode.hpp
|   |   |-- Order.hpp
|   |   `-- OrderQueue.hpp
|   `-- src/
|       |-- AVLTree.cpp
|       |-- Book.cpp
|       |-- LimitNode.cpp
|       |-- Order.cpp
|       `-- OrderQueue.cpp
|-- Matching_Engine/
|   |-- include/
|   |   |-- BaseOrderMatcher.hpp
|   |   |-- LimitOrderMatcher.hpp
|   |   |-- MarketOrderMatcher.hpp
|   |   |-- MatchingEngine.hpp
|   |   |-- OrderParser.hpp
|   |   |-- OrderValidator.hpp
|   |   |-- Trade.hpp
|   |   |-- TradeRecorder.hpp
|   |   `-- nlohmann/json.hpp
|   `-- src/
|       |-- BaseOrderMatcher.cpp
|       |-- LimitOrderMatcher.cpp
|       |-- MarketOrderMatcher.cpp
|       |-- MatchingEngine.cpp
|       |-- OrderParser.cpp
|       |-- OrderValidator.cpp
|       |-- TradeRecorder.cpp
|       `-- main.cpp
|-- Testing_and_Performance/
|   |-- include/
|   |   |-- Event.hpp
|   |   |-- EventLogger.hpp
|   |   |-- EventLoggerIntegration.hpp
|   |   |-- MarketDataFeed.hpp
|   |   |-- PerformanceMetrics.hpp
|   |   `-- replayEngine.hpp
|   `-- src/
|       |-- EventLogger.cpp
|       |-- EventPersistance.cpp
|       |-- EventSerialization.cpp
|       |-- MarketDataFeed.cpp
|       |-- PerformanceMetrics.cpp
|       `-- replayEngine.cpp
|-- tests/
|   |-- data/                    Scenario, stress, random and replay JSON inputs
|   |-- csv_viewer_test.cpp
|   |-- engine_service_test.cpp
|   |-- engine_worker_test.cpp
|   |-- generate_test_cases.py
|   |-- gui_smoke_test.cpp
|   `-- test_main.cpp
|-- figures/
|   |-- LOB_class_diagram.png
|   `-- architecture.png
`-- visualizer/
    |-- app.js                    Legacy browser visualizer
    |-- index.html
    `-- styles.css
```

`build/` and `build-gui/` are generated build directories and are not part of the source tree. Runtime reports are written under `log/`.

## Requirements

### CLI

- CMake 3.15 or newer
- A C++17 compiler (GCC/MinGW, Clang, or MSVC) and its matching CMake generator
- Threads support

Python 3 is optional and is only needed to generate random JSON inputs.

### Qt GUI

- Windows x64 is the currently supported GUI platform.
- Qt 6.2 or newer with the Widgets, OpenGLWidgets and PrintSupport components.
- The Qt kit compiler must match the compiler selected by CMake. For example, use the Qt MinGW kit with the MinGW generator, or an MSVC Qt kit with the corresponding Visual Studio generator.
- CMake downloads the pinned QCustomPlot 2.1.1 source when configuring the GUI, so the first GUI configure needs internet access. Qt itself must be installed separately.

## Build, test and run the CLI

Run these commands from the repository root in PowerShell. Choose a generator and compiler that are installed on your machine; this example uses MinGW Makefiles:

```powershell
cmake -S . -B build -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

The CLI executable is `build\matchingengine.exe`. The CLI-only CTest configuration runs the core and engine-service test targets.

### Input JSON

The input file is a JSON array. Each order has the five fields below:

```json
[
  {
    "id": 1001,
    "type": "LIMIT",
    "side": "BUY",
    "price": 100,
    "quantity": 10
  }
]
```

- `id`: integer order identifier
- `type`: `LIMIT` or `MARKET`
- `side`: `BUY` or `SELL`
- `price`: positive integer for a limit order; use `0` for a market order
- `quantity`: integer order quantity

Malformed records are reported as parser rejections. Requests that parse but fail matching-engine validation are reported separately as engine rejections.

### CLI modes

**Engine-only (Worklets 1–2 at runtime):**

```powershell
.\build\matchingengine.exe --engine-only .\tests\data\02_limit_full_match.json
```

**Full system (Worklets 1–3):**

```powershell
.\build\matchingengine.exe --test-system .\tests\data\02_limit_full_match.json
```

The full-system run records events, processes the feed and validates the replayed final book against the live book. Replay validation runs after the matching throughput timer stops.

**Replay an event log created by a full-system run:**

```powershell
.\build\matchingengine.exe --replay .\log\02_limit_full_match_events.log
```

The CLI prints its usage syntax if invoked without a supported mode and input path.

### Test data and generated inputs

The `tests\data\` directory includes matching scenarios, a 10,000-order stress input, randomized inputs and a sample replay file. To generate a repeatable 10,000-order input:

```powershell
python .\tests\generate_test_cases.py 10000 --seed 42
```

By default, the generator writes `tests\data\test_random_10000.json`. Choose a different output path with `--output`:

```powershell
python .\tests\generate_test_cases.py 50000 --seed 42 --output .\tests\data\benchmark_50000.json
.\build\matchingengine.exe --engine-only .\tests\data\benchmark_50000.json
```

The full-system throughput can be measured by running the same input with `--test-system`. Compare runs using the same input, build type, compiler and machine.

### Reports

CLI reports are written under `log/`, using the input filename as their base name:

- `<name>_logs.log` — detailed per-order results and summary.
- `<name>_reject.log` — parser and engine rejections.
- `<name>_latency.csv` — per-order latency, outcome and rejection reason.
- `<name>_events.log` — Worklet 3 event stream, produced by full-system mode.

Outputs for the same input basename are overwritten by the next run.

## Build and run the Qt GUI (Windows)

Install a Qt 6 SDK with a compiler-matched kit first. For the Qt installation used to verify this repository, configure with Qt 6.12.0 and its MinGW 13.1 toolchain:

```powershell
cmake -S . -B build-gui -G "MinGW Makefiles" `
  -DCMAKE_BUILD_TYPE=Release `
  -DBUILD_QT_GUI=ON `
  -DCMAKE_PREFIX_PATH="D:\Qt\6.12.0\mingw_64" `
  -DCMAKE_CXX_COMPILER="D:\Qt\Tools\mingw1310_64\bin\g++.exe" `
  -DCMAKE_MAKE_PROGRAM="D:\Qt\Tools\mingw1310_64\bin\mingw32-make.exe" `
  -DCMAKE_RC_COMPILER=D:/Qt/Tools/mingw1310_64/bin/windres.exe
cmake --build build-gui --target lob_gui --parallel
.\build-gui\lob_gui.exe
```

If Qt is installed elsewhere, update the Qt and toolchain paths together. Do not mix compilers from different Qt kits. The resource compiler path above uses forward slashes intentionally for CMake compatibility.

Run the GUI-enabled tests with the Qt and compiler runtime directories on `PATH`:

```powershell
$env:PATH = "D:\Qt\6.12.0\mingw_64\bin;D:\Qt\Tools\mingw1310_64\bin;$env:PATH"
ctest --test-dir build-gui --output-on-failure
```

The GUI-enabled CTest configuration includes five targets: core tests, engine-service tests, engine-worker tests, CSV viewer tests and a GUI smoke test.

To prepare a Windows deployment directory with Qt runtime dependencies:

```powershell
cmake --build build-gui --target package_lob_gui
```

The packaged executable and dependencies are placed in `build-gui\dist`. Review the Qt and QCustomPlot license terms before redistribution.

The GUI has single-window navigation for the order book, order management, replay, metrics and CSV viewing. It displays separate buy/sell trees with FIFO queue inspection, supports order operations and JSON/CSV order loading, and provides forward replay controls, event browsing, charts and metrics export. GUI throughput charts measure session rates and include idle time; use CLI runs for performance measurements.

## Specifications and performance

### Software and verified environment

- Language standard: C++17.
- Data structures: AVL price-level trees and FIFO queues; orders are maintained through book/order lookup structures.
- Verified CLI build: Windows x64, Release, CMake 3.29.2 and MinGW-W64 GCC 13.2.0.
- Verified Qt GUI build: Qt 6.12.0 with the matching Qt MinGW 13.1 toolchain.
- Example benchmark host: 13th Gen Intel Core i5-13450HX (10 cores / 16 logical processors), 24 GB RAM.

### Throughput and latency

The CLI reports:

- **Submissions/s (TPS):** structurally valid order requests submitted divided by timed batch duration. A request counts as one transaction, including a request subsequently rejected by engine validation.
- **Accepted orders/s:** successfully accepted submissions divided by the same duration.
- **Trade executions/s:** individual matches divided by the same duration. One order may produce multiple trades.
- **Per-order latency:** mean, p50, p95, p99 and maximum submission latency.

Engine-only timing covers matching, book updates and metric collection; it excludes JSON parsing, event/feed work and CLI/file output. Full-system timing additionally includes event publication and feed draining. Event-file flush and replay validation are outside that timer. The throughput and latency fields measure different scopes; per-order latency samples do not sum to batch duration.

As one local example, the supplied `stress_test_10000.json` produced **about 2.90 million submissions/s** and **2.81 million trade executions/s**, with **0.24 μs mean** and **0.40 μs p95** submission latency in one engine-only run on the host above. The timed interval was only a few milliseconds. This is a sample observation, not a sustained-capacity claim or a portable performance guarantee. Results can vary with workload, build/compiler, operating-system scheduling, background load and hardware. Re-run the same generated input several times and compare results on the target machine.

## Matching behavior and limitations

- Incoming orders match the best opposing price first; orders at one price level are matched FIFO.
- Trades execute at the resting order's price.
- Unmatched limit quantity rests in the book; unmatched market quantity is discarded.
- Replay is forward-only.
- The GUI CSV viewer searches across a file and displays pages of up to 500 rows; sorting applies to the displayed page. CSV parsing supports quoted commas and UTF-8 BOMs but not quoted embedded newlines.
- The `visualizer/` directory is a legacy browser UI; the supported desktop GUI is the optional Qt application.
