# Integration feedback and recommended follow-up

## Completed in this integration

- Worklet 1, Worklet 2 and Worklet 3 now build into one C++17 `lob_engine` library.
- Worklet 3's event logger, feed, replay engine and performance metrics are wired into the CLI.
- Worklet 3's replay engine is the only replay implementation. The duplicate Worklet 2 snapshot exporter was removed.
- The CLI persists a fresh event log per run, checks persistence failures with an explicit flush, replays the generated events and fails if replayed state differs from the live book.
- `matchingengine --replay <events.log>` independently replays a saved event stream.
- A single consolidated CTest target covers the full code path rather than separate worklet-specific test targets.
- The parser's wrong-type crash was fixed.

## Current performance reporting

Each parsed request's engine-handling duration is written to `log/<input>_latency.csv` in nanoseconds and microseconds. The CLI summary includes mean/p50/p95/p99/max latency, submitted orders per second, and trades per second.

These are useful initial measurements, not definitive engine capacity metrics:

- Latency includes matching, event serialization/enqueueing and feed publication.
- Throughput measures the sequential CLI request loop, including per-order log formatting.
- JSON parsing is outside the throughput interval; final event-log flush and replay validation are also outside it.
- The measured path is single-threaded for matching, is not a controlled benchmark, and does not capture CPU/compiler/build metadata.
- The final 10,000-order run on this development environment reported approximately 60,900 submitted orders/s and 59,000 trades/s. Results will vary by machine, build mode, workload and output path; do not use this sample as a deployment guarantee.
- Parser-rejected records are reported separately and do not receive engine-handling latency rows.

## Recommended next steps

1. **Add a reproducible benchmark mode.** Run warmups and multiple measured repetitions with a fixed seed and input, report median and spread, identify compiler/build mode and host CPU, and separate matching-only timings from end-to-end CLI/event-persistence timings.
2. **Choose durability semantics.** The current logger flushes and reports write failures, but it does not `fsync`/`FlushFileBuffers` every order or atomically commit matching state with the event log. If crash recovery is a requirement, define acceptable loss and recovery guarantees before optimizing persistence.
3. **Decide replay retention policy.** The CLI truncates the named event file each run so its new empty book and replay history align. If events must be retained across runs, add explicit session boundaries/snapshots and replay initialization rather than appending unrelated runs.
4. **Broaden correctness tests.** Add deterministic randomized tests/reference-model comparisons for AVL operations, hash collisions/resizing, order cancellation/modification edge cases, duplicate IDs and integer-volume boundaries.
5. **Exercise feed overflow intentionally.** The feed is bounded; consumers can miss events and must handle overflow/recovery. The CLI drains a consumer as each order is processed. Future real-time consumers need a defined resynchronization path, such as a fresh book snapshot plus a sequence watermark.
6. **Build the Qt GUI later.** The browser visualizer remains in the repository but is no longer generated or used by the engine. A future Qt client should consume replay snapshots/events through a defined API rather than adding GUI responsibilities to matching code.
7. **Measure memory and longer runs.** Current performance output reports latency and throughput only. Peak memory, event-log size, queue depth, per-level order counts and feed-overflow behavior are not benchmarked.

## Current scope boundaries

- Matching is sequential; this does not claim lock-free or multi-threaded order-book mutation.
- The event log contains successful state-changing orders and their trades. Invalid parser/engine submissions are in the rejection log, not in the replay event stream.
- `--replay` validates/reconstructs the C++ book state; the legacy browser visualizer's JSON snapshot input is not currently produced.
- No Qt target or Qt dependencies are present yet.
