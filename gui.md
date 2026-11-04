# Qt 6 GUI Proposal

## Decisions and first implementation

The implementation uses five order fields (`id,type,side,price,quantity`), forward-only replay, separate actual buy and sell AVL trees, and Windows as the first target. The optional `lob_gui` CMake target fetches QCustomPlot; users still need a Qt 6 SDK installed for their compiler. To avoid the crowded dock layout, the UI uses a maximized main window with a top navigation bar and one full-size page visible at a time, rather than ADS docks. The Order Book page has tabs for levels, FIFO queue, open orders and trades; selecting a rendered AVL node opens its queue tab. The tree uses subtree-aware spacing, zoom buttons and mouse-wheel zoom, with red buy and green sell accents. The Orders page uses the previously empty area for live open-order and execution tables. Metrics now has KPI cards, a larger interactive latency chart, a cumulative activity chart, and Worklet 3 feed statistics. Replay includes event inspection and final reconstructed-book comparison against the live book. Engine operations, order-file parsing, batch processing, and replay processing run on a worker thread; the GUI receives copied snapshots and bounded event-table data. Phase 2/3 tests have been added, but the current CMake Tools configuration has not succeeded, so the changes still require a configured Qt build and passing test run.

## Worklet 3 GUI plan

The Worklet 3 surface should make event capture and deterministic reconstruction inspectable without conflating them with matching:

- **Event stream:** show sequence, event kind, order/trade participants, side, price and quantity; surface sequence gaps, checksum/parse errors and feed overflow.
- **Replay controls:** forward step by incoming order, play/pause, fast-forward and progress by event sequence. Keep replay state isolated from the live engine.
- **Reconstructed book:** show replayed bid/ask levels and FIFO queues, and compare the replay snapshot to the live/final book with a concise pass/fail plus expandable differences.
- **Performance dashboard:** show latency distribution and session activity with explicit units and timing boundaries; report event-log/feed counts, publish timing, overflows and missed events separately from core matching metrics.
- **Run artifacts:** make event log and latency/metrics CSV paths visible and provide export/open actions; distinguish event writing/flush time from engine submission timing.

The current UI implements event-log table inspection, forward step, final-state live-book comparison, latency/activity charts and in-memory feed diagnostics. Replay stepping, loading, and validation are handled asynchronously on the engine worker. Playback controls, deeper event-integrity diagnostics and export/report management remain subsequent Worklet 3 GUI tasks.

## Goal

Build a native Qt 6 Widgets application for the limit order book, matching engine and Worklet 3 testing/replay features. Retain the CLI as a supported workflow. The GUI should use the existing engine and replay implementation rather than reimplement matching rules or book state.

## User-requested capabilities

- Catppuccin Mocha visual theme.
- View both buy and sell AVL price-level trees; select a level to inspect its FIFO order queue.
- Submit, cancel and modify orders through the matching engine.
- Load order requests from JSON and CSV.
- Show matches with clear, controllable animations.
- Replay recorded events and show Worklet 3 reconstruction state.
- Display latency, throughput/TPS and other available metrics in charts.
- Inspect CSV files in the GUI.
- Use Qt 6 Widgets, `QOpenGLWidget` with `QGraphicsView`, and QCustomPlot.

## Suggested application layout

Use a maximized `QMainWindow` with a top navigation bar and a `QStackedWidget`; show one full-size page at a time rather than opening multiple dock panels.

1. **Order book**
   - Two clearly distinguished views for bids and asks.
   - Show price levels with price, aggregate remaining quantity and order count.
   - Show the actual AVL shape separately from a sorted depth/price-level view; they answer different questions.
   - Selecting a level displays its FIFO queue in priority order, including order ID, remaining quantity, type and timestamp.
   - Highlight best bid/ask and crossed/matching levels.
2. **Order entry and management**
   - Form for limit/market order, side, price and quantity with validation before submission.
   - An open-orders table with cancel and modify actions.
   - A recent-orders/trades table with outcome, fill quantity, execution price and rejection reason.
   - Require confirmation for destructive actions when appropriate; do not allow direct widget edits to mutate model state.
3. **Replay and reconstruction**
   - Load an event log and show play, pause, step forward/back (where supported), seek, speed and reset controls.
   - Display current replay sequence, replayed book and validation status against the live/final book.
   - Show a clear message when seeking or stepping backward requires a replay reset/reconstruction.
4. **Metrics**
   - Time-series plots for latency and throughput, with clearly labeled units and selected time range.
   - Summary cards for submission TPS, accepted orders/s, trade executions/s, latency percentiles and replay/validation status.
   - Keep submission TPS and trade execution rate distinct. Identify the timing boundary and whether Worklet 3 event/feed handling is included.
5. **Data and CSV inspection**
   - File-open actions for JSON order data, event logs and CSV files.
   - Provide a table-based CSV viewer with column sorting, filtering, and clear parsing errors. Use a model/view implementation and incremental loading for large files.
   - Distinguish order-input CSV from output metrics/latency CSV; they are different schemas.
6. **Status and diagnostics**
   - Persistent status bar for selected run mode, loaded input, engine state and errors.
   - A run/output pane for parser rejections, engine rejections, replay errors and file I/O problems.

## Visualization recommendations

- Use a custom `QGraphicsScene`/`QGraphicsView` for the AVL graph: nodes represent price levels; edges show parent/child links. Selecting a node selects the corresponding level and its FIFO queue.
- Use `QGraphicsView::setViewport(new QOpenGLWidget)` if OpenGL-backed rendering is useful for large/animated scenes. Keep a non-OpenGL fallback for unsupported drivers and easier debugging.
- Keep the graph layout deterministic and separate from engine logic. Rendering should consume immutable snapshots, not retain raw engine node pointers.
- Use QCustomPlot for charts, with decimation/downsampling for long runs. Make units, axes, aggregation interval and timing scope visible.
- Animate fills with short, optional highlights/transitions; provide a reduced-motion/disable-animation setting. Animation must never delay or change matching.

## Proposed architecture

```text
Qt Widgets / single-window navigation
        |
GUI controllers and presentation models
        |
Engine service (single serialized owner of mutable Book and MatchingEngine)
        |                         |
Worklets 1-2 matching       Worklet 3 logger/feed/replay
        |
Immutable book/event/metrics snapshots -> GUI models, scene and plots
```

- Keep all mutations serialized through one engine service. The current matching engine is sequential; do not mutate a `Book` concurrently from GUI and worker threads.
- Run long file loading, scenario execution and replay work in a worker `QObject` moved to a `QThread`. Communicate through queued signals/slots and value-type snapshots.
- Never render by traversing live `Book`, AVL nodes or intrusive order pointers from the GUI thread. `EngineService` now copies the AVL shape, price levels, FIFO orders and trades into value snapshots before rendering.
- Make controllers invoke existing engine operations for submit/cancel/modify. The full-system path should use `EventLoggerIntegration`; engine-only mode should use the core engine path and avoid Worklet 3 runtime operations.
- Reuse `ReplayEngine` and expose value snapshots (`ReplaySnapshot`) to the presentation layer.
- Keep performance collection and presentation independent: metric sampling must not block matching or GUI rendering. Clearly identify sampling overhead and timing scope.
- Prefer Qt model/view widgets (`QAbstractItemModel`/`QTableView`) for order, trade, queue and CSV tables rather than creating one widget per row.

## Input and output formats

- Reuse `OrderParser` for JSON input to preserve the CLI's validation and rejection semantics.
- Add a separate CSV order reader with a documented header and validation. Suggested initial columns: `id,type,side,price,quantity`; decide whether to support timestamps and optional columns before implementation.
- Reuse existing latency CSV output where possible. Add a general CSV table viewer that can display latency exports and other compatible CSV files.
- Support event-log loading for replay. Report malformed files, unsupported versions and parse errors in the UI without silently substituting empty data.
- For large inputs, show progress and cancellation during file loading and batch processing.

## Implementation status and remaining phases

The roadmap has **six phases total**. Phases 1–3 are implemented; Phase 2/3 build and automated-test verification is pending because CMake Tools currently reports that it cannot configure the project and has no CMake diagnostics. Phases 4–6 remain in progress at the feature/polish level; the current UI is not a finished production GUI.

1. **Build and dependency foundation — implemented:** Qt 6 Widgets/CMake target, QCustomPlot integration, application startup, Catppuccin theme and Windows build setup.
2. **Engine service and snapshots — implemented; verification pending:** `EngineService` owns the book, matching engine and optional Worklet 3 logger/feed. A `QThread` worker serializes mutations, file parsing, batch processing and replay; the GUI uses copied snapshots. A service test covers tree/FIFO snapshots, matching, modifications, cancellations and feed counters.
3. **Order-book and order-management UI — implemented; verification pending:** separate buy/sell trees, FIFO queue drill-down, order entry/cancel/modify, asynchronous JSON/CSV order loading, chunked batch processing with cancellation, order outcomes, open-order/trade tables, best bid/ask/spread summary and trade highlights. A worker test covers initialization, asynchronous submission, batch matching and cancellation.
4. **Replay UI — initial implementation:** event-log loading/table, forward order-boundary stepping, reconstructed book view and final-state comparison with the current live book. Playback, seeking and replay progress controls remain future work.
5. **Metrics and data tools — initial implementation:** latency/activity charts, session rates, recent-window latency percentiles, CSV viewer and Worklet 3 feed statistics. Large-file streaming and exportable run summaries remain future work.
6. **Polish and verification — in progress:** tree zoom and responsive layout are present; add GUI-specific tests, accessibility support, robust large-run handling and Windows packaging.

**Remaining:** three roadmap phases (4–6) are not fully complete. Phase 2/3 code has been implemented, but should not be considered verified until a configured Qt build and all applicable CTest tests pass. Remaining feature gaps include stronger replay diagnostics and controls, scalable CSV viewing, report export, accessibility and a distributable Windows package.

Each phase should preserve the CLI and run the existing CTest suite. GUI-specific tests should verify model contents and controller-to-engine behavior; avoid tests that depend on fragile pixel-perfect layouts.

## Additional recommendations

- Establish a single documented transaction/TPS definition shared by CLI and GUI. Show submissions/s and trades/s separately, and state whether timing includes event/feed work.
- Add stable identifiers and snapshot value types for price levels, orders, trades and replay sequence numbers; the UI should not depend on internal pointer identity.
- Add a user-selectable replay speed and a fast-forward mode that does not animate every event.
- Persist window size and theme preference using `QSettings`.
- Provide a text/table alternative to every graph so data remains usable without OpenGL or animation.
- Keep Qt-specific code in a dedicated GUI target/directory; core matching and CLI should remain usable without Qt installed.
- Confirm licenses and redistribution requirements for QCustomPlot before packaging it.

## Confirmed initial decisions

1. **CSV order schema:** five fields only initially (`id,type,side,price,quantity`), while leaving room to extend.
2. **Replay controls:** forward-only initially.
3. **AVL visualization:** show separate buy and sell trees.
4. **Initial book data:** a future non-test data file will initialize the book before manual order entry.
5. **Dependencies:** require the teammate's installed Qt SDK and fetch/build pinned GUI dependencies through CMake.
6. **Supported platform:** Windows initially.
