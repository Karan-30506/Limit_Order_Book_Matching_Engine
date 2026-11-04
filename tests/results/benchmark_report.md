# Million-entry stress and timing results

Recorded: 2026-11-04 18:35:15 IST
CPU: Intel(R) Core(TM) i7-14650HX; logical processors: 24
OS: Microsoft Windows NT 10.0.26200.0; architecture: AMD64
Compiler: g++.exe (Rev3, Built by MSYS2 project) 13.2.0
Build: C++17, -O2, -Wall -Wextra -Werror, static GCC/C++ runtimes; no LTO.

84 configurations and 252 trials passed. Each default configuration uses three fresh instances; reported times are medians. Default sizes: 1,000, 10,000, 100,000, and 1,000,000. Patterns: ascending, descending, and deterministic shuffle (default seed 42).

## One million live entries

| Structure / workload | Ascending fill (ms) | Descending fill (ms) | Shuffled fill (ms) | Shuffled removal (ms) |
| --- | ---: | ---: | ---: | ---: |
| Order | 65.732 | 65.912 | 63.403 | 24.640 |
| OrderQueue | 8.243 | 8.160 | 9.370 | 18.659 |
| LimitNode | 50.922 | 53.991 | 55.256 | 21.992 |
| AVLTree | 138.142 | 141.379 | 1072.509 | 589.399 |
| HashMap | 178.058 | 184.579 | 171.487 | 91.934 |
| Book | 591.665 | 580.313 | 1390.580 | 1106.031 |
| BookShared | 285.797 | 280.875 | 278.290 | 179.911 |

## What the timings include

- Order: allocate and construct N detached Order objects, then delete them. LimitNode: allocate and construct N empty levels, then delete them. These are object creation measurements, not indexed insertion.
- OrderQueue: append N preallocated caller-owned orders to one price level, then pop all N in FIFO order. Order allocation/deletion is excluded; comparisons with allocation-heavy structures need this distinction.
- AVLTree: allocate and insert N unique prices; remove all original node pointers in input order. HashMap: insert N unique keys starting at capacity 1, including all rehashes; remove all keys.
- Book: allocate and insert N orders at N distinct buy prices. BookShared: allocate and insert N orders over 100 buy price levels. Removal cancels every order and includes deletion and empty-level pruning.
- Input generation, reference construction, container setup/reserve, verification, file I/O, and final container destruction are outside fill/removal timers. No cache-flush or allocator reset occurs between trials; medians include ordinary system/allocator variation. These are local elapsed-time measurements, not latency guarantees.
- All entries remain live until the fill completes. Every entry is then checked before removal. Full AVL ordering, parent links, heights and balance, all hash lookups, queue links/FIFO, Book indexes/volumes, and final empty state are validated. Book benchmarks use the buy side; the mixed-operation and rollback suites exercise both sides.

## Failure coverage and mixed operations

failure_results.txt: 8/8 passed (ALL PASSED)

failure_debug_results.txt: 8/8 passed (ALL PASSED)

`make failure_test` covers queue overflow rollback; foreign/already-attached orders; invalid sides and quantities; combined-side volume overflow; nonempty/foreign AVL deletion and deep successor transplant; deliberate hash collisions; signed/unsigned 64-bit key extremes; and injected allocation failures during hash resize, Book insert, and Book modification on both sides.

Allocation failure points are tried one by one until the operation succeeds. Failed operations must preserve existing orders, pointer identity, quantities, queues, maps and best-price caches. The allocator override exists only in the failure-test executable. Allocation exhaustion is simulated deterministically rather than exhausting the host memory.

`make test` additionally runs one million seeded mixed operations against an independent STL model, checking operation results every time and fully checking both Book and separate primitives every 10,000 operations and at the end. That mixed workload reuses 600 IDs and 80 price slots; the benchmarks above separately prove one million simultaneous live entries. The original 50,000 mixed operations still perform full checks after every operation.

Passing these cases does not establish behavior for dangling pointers, caller-corrupted public links/fields, concurrent unsynchronized access, or trade matching. No address/leak sanitizer was used in this run.

## Reproduce

Run `mingw32-make benchmark`, `mingw32-make failure_test`, `mingw32-make failure_debug_test`, `mingw32-make test`, and `mingw32-make test_csv_validation` from the repository root. Avoid other CPU-heavy work while benchmarking. The debug failure target uses `-Og -g -D_GLIBCXX_DEBUG`.

`benchmark_raw.csv` retains every measurement, including verification time. `benchmark_summary.csv` includes medians, minimum/maximum fill times and nanoseconds per entry. `tests/testcases.csv` contains all workload configurations.
