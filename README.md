# C++ Limit Order Book Worklet 1

Implemented C++17 domain objects and storage using custom AVL trees, intrusive FIFO queues, and separate-chaining integer hash tables. Insert, cancel, modify, volume lookup, best-level lookup, ordered traversal and ownership-transferring pop work. Matching, trades, market orders, UI, networking, databases and benchmarks are outside this worklet. Crossed books are allowed until Worklet 2 implements matching.

## Shared CSV tests

`tests/testcases.csv` is the single test catalog and shared scenario input. It lists
all 33 existing behavioral cases, three explicit operation scenarios, five
reproducible stress scenarios of 10,000 operations each, and a checkpointed
one-million-operation scenario. Existing C++ assertions
and specialized fixtures remain in their suite files; their execution is driven
by the CSV's `unit` rows. Missing, duplicate, or unknown unit cases fail the suite.

Run from the repository root with GCC and GNU Make (on Windows use
`mingw32-make` in place of `make`):

```sh
make test
make test_debug
make test_csv_validation
```

The debug target enables checked STL iterators and treats warnings as errors.
The CSV validation target runs PowerShell checks for malformed files, omitted or
unknown cases, invalid inputs, incorrect expectations, and quoted fields. GCC's
C++ runtime is linked statically to avoid picking up another application's DLL
from the Windows search path.

All six original standalone targets, such as `make test_order_queue`, also read
the same CSV. Set `LOB_TEST_CSV` to an alternate file path to replay another
dataset; missing files fail explicitly. CSV edits trigger the normal build target.

| Column | Meaning |
| --- | --- |
| `suite` | `Order`, `OrderQueue`, `LimitNode`, `AVLTree`, `HashMap`, `BookCombined`, `Shared`, `Benchmark`, or `Failure` |
| `case` | Registered C++ function for a unit row; scenario name for shared rows |
| `operation` | `unit`, `insert`, `modify`, `reduce`, `cancel`, `pop`, `stress`, `stress_checkpoint`, or a benchmark input pattern |
| `id` | Order ID; all signed 32-bit values are supported |
| `price` | Order or destination price; `pop` selects this exact price level |
| `quantity` | New quantity, or shares to subtract for `reduce` |
| `side` | `Buy` or `Sell`; modification retains the order's existing side |
| `seed`, `steps` | Nonnegative MT19937 seed and positive operation count for `stress` |
| `expected` | `true`/`false` operation result, or `auto` to use the reference model |

Unit rows leave the remaining columns empty. Stress rows need only `seed` and
`steps`. Shared rows with the same case name execute in file order against a fresh
state. Different scenarios have independent state. Quoted fields, escaped double
quotes, LF and CRLF are supported; records must occupy one physical line.

For each shared operation, the runner compares both `Book` and separate instances
of `Order`, `OrderQueue`, `LimitNode`, `AVLTree`, and `HashMap` with an independent
`std::map` reference model. It checks return values, rejected-operation state,
FIFO priority, both queue links, attachment, quantities, hash pointer identity,
full BST bounds, AVL parent links/heights/balance, price traversal, best-price
caches, level pruning, and side/combined volume including integer overflow.
Every scenario drains its remaining orders and verifies cleanup after each
deletion. Failures identify the CSV line and, for stress runs, the seed and step.

The explicit scenarios cover same-price buys and sells, priority loss on modify,
full and partial reductions, invalid inputs, duplicate/reused IDs, empty pops,
signed ID limits, maximum prices/quantities, and overflow rollback. These are
storage tests; the engine does not implement trade matching yet. Debug iterator
checks do not replace a memory sanitizer.

## Million-entry benchmarks and failure tests

```powershell
mingw32-make benchmark
mingw32-make failure_test
mingw32-make failure_debug_test
```

`benchmark` holds up to one million entries live at once, verifies every entry,
then drains each structure. It runs 84 configurations: seven workloads, four
sizes (1,000 through 1,000,000), and three input patterns (ascending, descending,
shuffled), with three fresh trials each. `Book` uses distinct prices;
`BookShared` distributes its orders over 100 levels. All configurations are
`Benchmark` rows in the same `tests/testcases.csv`.

For these rows, `case` selects the workload; `operation` is the input pattern;
`id` specifies the odd trial count; `price` specifies the price-level cap (only
`BookShared`); `quantity` is 1; `side` is Buy; `seed` controls shuffle; `steps`
specifies the number of simultaneously live entries; and `expected` is true.
The benchmark enforces the one-million-entry maximum. The ordinary `test`
runner leaves these potentially expensive workloads to the `benchmark` target.

Results are saved in `tests/results/benchmark_raw.csv`,
`tests/results/benchmark_summary.csv`, and `tests/results/benchmark_report.md`.
The report records the local machine/compiler and explains what each timing
includes. Fill, verification, and removal have separate timers. Queue timing
excludes order allocation; Order and LimitNode timing measures object creation.
For useful timing comparisons, run the benchmark without other heavy jobs.

`failure_test` reads the `Failure` unit rows. It tests rejected operations,
overflow rollback, forced hash collisions, 64-bit key extremes, AVL transplant
identity, and actual `bad_alloc` exceptions at each allocation point of hash
resize, Book insertion and Book modification. The allocation override is linked
only into this executable. On failure, existing order identity, quantities,
queues, indexes and cached best prices must survive intact.

The shared `million_mixed` case runs 1,000,000 random operations using seed 1337.
It checks every operation result against the reference model and performs a
full invariant scan every 10,000 operations, at the final operation, and after
each final cleanup deletion. Its working set reuses 600 IDs and 80 price slots;
the separate benchmarks test one million simultaneous entries. The new
`stress_checkpoint` row stores the scan interval in `price`. The original
`stress` cases still scan after every operation.

