# Unit testing slides: content and design

Use the first slide for the four-row layout in the reference image. Use the
supporting slides if time permits; keep the detailed tables as presenter notes
or an appendix.

The timing values below come from the latest saved benchmark report, recorded
on **4 November 2026 at 18:35:15 IST**. They supersede earlier timing values in
the conversation. Test outcomes refer to the completed runs in this session.

## Slide 1 — Unit tests

### Copy-ready slide content

**Title:** UNIT TESTS

**Subtitle:** What was tested: AVLTree, OrderQueue, HashMap, and Book validation

| ID | Test → expected result | Result |
| --- | --- | :---: |
| UT-1 | AVLTree: rotations and deletion preserve price ordering, parent links, heights, and AVL balance. | PASS |
| UT-2 | OrderQueue: same-price orders pop in FIFO order; removal and quantity reduction preserve links and total volume. | PASS |
| UT-3 | HashMap: insertion above 0.75 load triggers resizing; all keys remain accessible; updates, collisions, and removal work correctly. | PASS |
| UT-4 | Book validation: duplicate IDs, quantity ≤ 0, invalid side, and price ≤ 0 are rejected without changing existing orders. | PASS |

**Results:** 49/49 reported checks passed; 0 failed.

**Coverage:** Code coverage not measured. Test pass rate: 100%.

**Evidence:** `tests/testcases.csv`, C++ test suites, and passing test-run output.

### Layout

- Match the reference: pale blue background, blue title, dark blue table header,
  alternating white/light blue rows, and green PASS labels.
- Use a 16:9 canvas, title at the upper left, subtitle beneath, and the table
  across the middle. Keep results, coverage, and evidence in a small footer.
- Use approximately 28–32 pt for the title, 18–20 pt for the subtitle, and
  16–18 pt for the table. Keep the test-description column widest.
- If retaining the reference's single-letter result format, use **P** and add
  the legend **P = Pass**.
- UT-1 through UT-4 are presentation group labels, not four individual C++ tests.

### Presenter notes

The complete implementation also tests Order construction/reduction, LimitNode
aggregation, and Book integration. There is no separate `OrderValidator` class:
validation is performed by Book. Zero and negative order IDs are supported;
duplicate IDs are rejected. Do not state that all IDs ≤ 0 are rejected.

The 49 checks include registry/schema checks and shared scenarios. They are not
49 independent unit-test functions. The breakdown appears in the appendix.

The screenshot's REQ-01/REQ-02/REQ-04 mappings have not been verified against a
requirements document. Leave those tags out unless that mapping is confirmed.

## Slide 2 — Stress testing

### Copy-ready slide content

**Title:** STRESS TESTING UP TO ONE MILLION ENTRIES

**Headline:** 84 configurations · 252 trials · All passed

| Test dimension | Executed values |
| --- | --- |
| Simultaneously live entries | 1,000; 10,000; 100,000; 1,000,000 |
| Input order | Ascending, descending, shuffled |
| Repetitions | 3 fresh trials per configuration |
| Workloads | Order, OrderQueue, LimitNode, AVLTree, HashMap, Book, BookShared |
| Book layouts | One order per price; or orders spread over 100 price levels |
| Additional mixed workload | 1,000,000 seeded insert/modify/cancel/reduce/pop operations |

**Correctness:** Every filled entry was verified before removal; final empty
state was checked.

**Evidence:** `tests/results/benchmark_summary.csv` and
`tests/results/benchmark_raw.csv`.

### Presenter notes

84 configurations = 7 workloads × 4 sizes × 3 input patterns.
252 trials = 84 configurations × 3 repetitions.

BookShared is a workload of Book, not another data structure. It uses 100 buy
price levels; Book uses N distinct buy prices for N orders. Benchmark quantities
are one share per order. Shuffled input uses seed 42.

The million-entry benchmark retains all one million entries simultaneously.
The separate million-operation mixed test reuses 600 IDs and 80 possible price
slots; it does not retain one million live orders. It uses seed 1337, checks
every operation result against an independent STL model, and performs full
invariant scans every 10,000 operations and at the final operation. Remaining
orders are drained with checks after every cleanup deletion.

An additional five seeded scenarios execute 10,000 operations each, with full
invariant scans after every operation. Their seeds are 0, 1, 42, 2026, and
8675309. Thus the random scenarios execute **1,050,000 mixed operations**, plus
their cleanup deletions and the explicit scenario operations.

## Slide 3 — Performance at one million entries

### Copy-ready slide content

**Title:** TIME TO FILL ONE MILLION ENTRIES

**Subtitle:** Shuffled input · Median of 3 trials · Time in milliseconds

| Structure / workload | Fill / creation (ms) | Removal / deletion (ms) |
| --- | ---: | ---: |
| Order objects | 63.403 | 24.640 |
| OrderQueue | 9.370 | 18.659 |
| LimitNode objects | 55.256 | 21.992 |
| AVLTree | 1,072.509 | 589.399 |
| HashMap | 171.487 | 91.934 |
| Book — distinct prices | 1,390.580 | 1,106.031 |
| Book — 100 price levels | 278.290 | 179.911 |

**Measurement note:** Queue orders are preallocated. Order/LimitNode values
measure object creation. Verification is timed separately.

**Environment:** Intel Core i7-14650HX · Windows x64 · GCC 13.2.0 · C++17 `-O2`.

**Evidence:** `tests/results/benchmark_report.md` and
`tests/results/benchmark_summary.csv`.

### Presenter notes

These workloads perform different work, so the table is not a direct ranking
of interchangeable containers. OrderQueue measures linking existing orders;
Book includes order allocation, indexing, price-level creation, and queueing.

Timing uses `std::chrono::steady_clock`. Data generation, reference building,
container setup/reserve, file output, and correctness scans are outside the
fill/removal timers. Each trial starts with fresh instances, but allocator and
cache state are not reset. Local elapsed times vary between runs.

Book removal uses cancellation and includes deletion and empty-level pruning.
OrderQueue removal pops and detaches existing orders; it excludes their deletion.
HashMap starts with capacity 1, so fill time includes resizing and rehashing.
The CSV also retains minimum/maximum fill times and nanoseconds per entry.

The full build uses `-std=c++17 -O2 -Wall -Wextra -Werror`, statically linked
GCC/C++ runtimes, and no LTO. The machine exposes 24 logical processors, but
these benchmark operations run on a single thread.

## Slide 4 — Failure and boundary cases

### Copy-ready slide content

**Title:** FAILURE HANDLING AND BOUNDARY TESTS

**Headline:** 8/8 reported checks passed in both optimized and checked-debug runs

| Failure condition | Expected behavior | Result |
| --- | --- | :---: |
| Queue quantity overflow or invalid attachment | Reject; preserve queue links, membership, and totals. | PASS |
| Invalid Book inputs or combined-volume overflow | Reject invalid updates; throw on overflowing volume query. | PASS |
| Nonempty or foreign AVL level | Reject nonempty removal; ignore foreign removal; preserve surviving node identity. | PASS |
| Deliberate hash collisions and 64-bit key limits | Keep valid keys accessible through collision-chain removal. | PASS |
| Allocation failure during hash resize, Book insert, or Book modify | Throw `std::bad_alloc`; preserve existing entries and Book state. | PASS |

**Outcome:** No unexpected failures observed in the executed workloads.

**Evidence:** `tests/results/failure_results.txt` and
`tests/results/failure_debug_results.txt`.

### Presenter notes

There are seven failure-test functions plus one CSV registry check. The
optimized and debug runs execute the same cases; they are not 16 distinct tests.

Allocation failures are injected one allocation at a time until each operation
succeeds. Book insert and modify rollback are tested on both buy and sell sides.
The replacement allocator is linked only into the failure-test executable.
This simulates allocation exhaustion without exhausting the host's memory.

Debug failure tests use `-Og -g -D_GLIBCXX_DEBUG` with warnings treated as errors.
This enables checked STL behavior; it is not AddressSanitizer or leak checking.

## Appendix — Accurate test-count breakdown

### Existing behavioral test functions

| Suite | Functions | Main checks |
| --- | ---: | --- |
| Order | 4 | Construction, filled status, detached and attached reduction |
| OrderQueue | 7 | Empty state, attachment validation, FIFO, removal, reduction, detachment, overflow |
| LimitNode | 2 | Initial state, order counts, aggregate volume |
| AVLTree | 6 | Empty state, duplicates, rotations, neighbors, deletion, balancing |
| HashMap | 6 | Capacity, lookup/update, pointer values, removal, signed/64-bit keys, resize |
| Book integration | 8 | Validation, priority, cancellation, modification, pop, both sides, destruction |
| **Total** | **33** | **25 core unit cases + 8 Book integration cases** |

### Normal runner total

| Category | Reported checks |
| --- | ---: |
| Existing behavioral functions | 33 |
| Original suite CSV registry checks | 6 |
| Shared scenarios: 3 explicit + 5 seeded + 1 million-operation | 9 |
| Shared CSV schema/scenario wrapper | 1 |
| **Total passed** | **49/49** |

The three explicit shared scenarios are `fifo_and_priority`, `rejections`, and
`integer_boundaries`. Wrapper checks contribute to the reported total; they do
not represent additional behavioral test functions.

### Other completed runs

| Run | Outcome | Counting unit |
| --- | --- | --- |
| Optimized failure suite | 8/8 passed | 7 functions + 1 registry check |
| Checked-debug failure suite | 8/8 passed | Same functions and registry in a debug build |
| CSV validation script | 18/18 passed | 17 rejection checks + 1 valid quoted/custom-path check |
| Large-entry benchmark | 252/252 trials passed | 84 configurations, each repeated 3 times |

Do not add these counts together and call the result a number of unique unit
tests. They count different kinds of checks and repeat some workloads.

## Appendix — Full one-million-entry fill timings

All values are medians of three trials, in milliseconds, rounded to three
decimal places from `benchmark_summary.csv`.

| Workload | Ascending | Descending | Shuffled |
| --- | ---: | ---: | ---: |
| Order | 65.732 | 65.912 | 63.403 |
| OrderQueue | 8.243 | 8.160 | 9.370 |
| LimitNode | 50.922 | 53.991 | 55.256 |
| AVLTree | 138.142 | 141.379 | 1,072.509 |
| HashMap | 178.058 | 184.579 | 171.487 |
| Book | 591.665 | 580.313 | 1,390.580 |
| BookShared | 285.797 | 280.875 | 278.290 |

## Evidence and reproducibility

| Source | Purpose |
| --- | --- |
| [Unified CSV](tests/testcases.csv) | Behavioral case registry, shared scenarios, failure cases, benchmark configurations |
| [Main runner](tests/main_test_runner.cpp) | Normal suite execution and reported totals |
| [Shared reference-model tests](tests/test_csv.cpp) | Cross-structure comparison and mixed-operation checks |
| [Benchmark implementation](tests/benchmark.cpp) | Timers, workloads, full-entry checks, and drain logic |
| [Failure implementation](tests/failure_cases.cpp) | Boundary cases, collisions, and allocation-failure injection |
| [CSV validation](tests/validate_csv.ps1) | Malformed-input checks and valid quoted-input acceptance |
| [Saved benchmark report](tests/results/benchmark_report.md) | Recorded machine, methodology, and results |
| [Summary CSV](tests/results/benchmark_summary.csv) | Per-configuration medians, minimum/maximum fill times |
| [Raw CSV](tests/results/benchmark_raw.csv) | All 252 individual trial measurements |
| [Failure output](tests/results/failure_results.txt) | Optimized failure-suite outcome |
| [Debug failure output](tests/results/failure_debug_results.txt) | Checked-debug failure-suite outcome |

Run from the repository root:

```powershell
mingw32-make test
mingw32-make test_csv_validation
mingw32-make failure_test
mingw32-make failure_debug_test
mingw32-make benchmark
```

Run the benchmark without another build or heavy test job. It regenerates the
saved timing files and report; update slide values if those files change.

For screenshots, capture the overall normal-run summary showing 49/49, the
failure-run summary showing 8/8, and the benchmark completion showing 84
configurations. The normal-run summary was observed in the completed run;
it is not currently stored in a separate result file. Rerun `make test` to
capture it. No screenshot is included in this Markdown file.

## Wording limits

- Use **100% test pass rate**, not **100% code coverage**. No line, branch, or
  function coverage percentage has been measured.
- Use **no unexpected failures observed in these runs**, not **cannot fail**.
- Do not claim trade matching, multithreaded correctness, or sanitizer/leak
  verification. These runs test storage behavior and deterministic failures.
- Do not claim constant-time cancellation in every case. Hash lookup is
  expected O(1), but removing the last order at a level also requires AVL cleanup.
- Distinguish unit tests, Book integration, model-based stress, and performance
  measurements when presenting the results.
