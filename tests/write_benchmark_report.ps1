param([string]$ResultsDirectory = (Join-Path $PSScriptRoot 'results'))

$ErrorActionPreference = 'Stop'
$summary = @(Import-Csv (Join-Path $ResultsDirectory 'benchmark_summary.csv'))
$raw = @(Import-Csv (Join-Path $ResultsDirectory 'benchmark_raw.csv'))
$expectedTrials = ($summary | ForEach-Object { [int]$_.repeats } | Measure-Object -Sum).Sum
if ($summary.Count -eq 0 -or $raw.Count -ne $expectedTrials -or
    @($raw | Where-Object status -ne 'PASS').Count -gt 0 -or
    @($summary | Where-Object status -ne 'PASS').Count -gt 0) {
    throw 'Expected every configured benchmark trial to pass before reporting.'
}
$cpu = $env:PROCESSOR_IDENTIFIER
try {
    $cpu = (Get-ItemProperty 'HKLM:\HARDWARE\DESCRIPTION\System\CentralProcessor\0').ProcessorNameString.Trim()
} catch {
    # PROCESSOR_IDENTIFIER remains usable when hardware inventory access is restricted.
}
$compiler = (& g++ --version | Select-Object -First 1)
$date = [TimeZoneInfo]::ConvertTimeBySystemTimeZoneId([DateTime]::UtcNow, 'India Standard Time')
$culture = [Globalization.CultureInfo]::InvariantCulture
$lines = [System.Collections.Generic.List[string]]::new()
$lines.Add('# Million-entry stress and timing results')
$lines.Add('')
$lines.Add("Recorded: $($date.ToString('yyyy-MM-dd HH:mm:ss')) IST")
$lines.Add("CPU: $cpu; logical processors: $env:NUMBER_OF_PROCESSORS")
$lines.Add("OS: $([Environment]::OSVersion.VersionString); architecture: $env:PROCESSOR_ARCHITECTURE")
$lines.Add("Compiler: $compiler")
$lines.Add('Build: C++17, -O2, -Wall -Wextra -Werror, static GCC/C++ runtimes; no LTO.')
$lines.Add('')
$lines.Add("$($summary.Count) configurations and $($raw.Count) trials passed. Each default configuration uses three fresh instances; reported times are medians. Default sizes: 1,000, 10,000, 100,000, and 1,000,000. Patterns: ascending, descending, and deterministic shuffle (default seed 42).")
$lines.Add('')
$lines.Add('## One million live entries')
$lines.Add('')
$lines.Add('| Structure / workload | Ascending fill (ms) | Descending fill (ms) | Shuffled fill (ms) | Shuffled removal (ms) |')
$lines.Add('| --- | ---: | ---: | ---: | ---: |')
foreach ($structure in @('Order','OrderQueue','LimitNode','AVLTree','HashMap','Book','BookShared')) {
    $rows = @($summary | Where-Object { $_.structure -eq $structure -and $_.entries -eq '1000000' })
    if ($rows.Count -eq 0) {
        continue
    }
    $values = foreach ($pattern in @('ascending','descending','shuffled')) {
        $row = $rows | Where-Object pattern -eq $pattern
        [double]::Parse($row.median_fill_ms, $culture).ToString('F3', $culture)
    }
    $shuffled = $rows | Where-Object pattern -eq 'shuffled'
    $drain = [double]::Parse($shuffled.median_drain_ms, $culture).ToString('F3', $culture)
    $lines.Add("| $structure | $($values -join ' | ') | $drain |")
}
$lines.Add('')
$lines.Add('## What the timings include')
$lines.Add('')
$lines.Add('- Order: allocate and construct N detached Order objects, then delete them. LimitNode: allocate and construct N empty levels, then delete them. These are object creation measurements, not indexed insertion.')
$lines.Add('- OrderQueue: append N preallocated caller-owned orders to one price level, then pop all N in FIFO order. Order allocation/deletion is excluded; comparisons with allocation-heavy structures need this distinction.')
$lines.Add('- AVLTree: allocate and insert N unique prices; remove all original node pointers in input order. HashMap: insert N unique keys starting at capacity 1, including all rehashes; remove all keys.')
$lines.Add('- Book: allocate and insert N orders at N distinct buy prices. BookShared: allocate and insert N orders over 100 buy price levels. Removal cancels every order and includes deletion and empty-level pruning.')
$lines.Add('- Input generation, reference construction, container setup/reserve, verification, file I/O, and final container destruction are outside fill/removal timers. No cache-flush or allocator reset occurs between trials; medians include ordinary system/allocator variation. These are local elapsed-time measurements, not latency guarantees.')
$lines.Add('- All entries remain live until the fill completes. Every entry is then checked before removal. Full AVL ordering, parent links, heights and balance, all hash lookups, queue links/FIFO, Book indexes/volumes, and final empty state are validated. Book benchmarks use the buy side; the mixed-operation and rollback suites exercise both sides.')
$lines.Add('')
$lines.Add('## Failure coverage and mixed operations')
$lines.Add('')
foreach ($log in @('failure_results.txt', 'failure_debug_results.txt')) {
    $path = Join-Path $ResultsDirectory $log
    if (Test-Path -LiteralPath $path) {
        $text = [IO.File]::ReadAllText($path)
        $result = [regex]::Match($text, 'Summary: (\d+/\d+ passed[^\r\n]*)')
        if ($result.Success) {
            $lines.Add("${log}: $($result.Groups[1].Value)")
            $lines.Add('')
        }
    }
}
$lines.Add('`make failure_test` covers queue overflow rollback; foreign/already-attached orders; invalid sides and quantities; combined-side volume overflow; nonempty/foreign AVL deletion and deep successor transplant; deliberate hash collisions; signed/unsigned 64-bit key extremes; and injected allocation failures during hash resize, Book insert, and Book modification on both sides.')
$lines.Add('')
$lines.Add('Allocation failure points are tried one by one until the operation succeeds. Failed operations must preserve existing orders, pointer identity, quantities, queues, maps and best-price caches. The allocator override exists only in the failure-test executable. Allocation exhaustion is simulated deterministically rather than exhausting the host memory.')
$lines.Add('')
$lines.Add('`make test` additionally runs one million seeded mixed operations against an independent STL model, checking operation results every time and fully checking both Book and separate primitives every 10,000 operations and at the end. That mixed workload reuses 600 IDs and 80 price slots; the benchmarks above separately prove one million simultaneous live entries. The original 50,000 mixed operations still perform full checks after every operation.')
$lines.Add('')
$lines.Add('Passing these cases does not establish behavior for dangling pointers, caller-corrupted public links/fields, concurrent unsynchronized access, or trade matching. No address/leak sanitizer was used in this run.')
$lines.Add('')
$lines.Add('## Reproduce')
$lines.Add('')
$lines.Add('Run `mingw32-make benchmark`, `mingw32-make failure_test`, `mingw32-make failure_debug_test`, `mingw32-make test`, and `mingw32-make test_csv_validation` from the repository root. Avoid other CPU-heavy work while benchmarking. The debug failure target uses `-Og -g -D_GLIBCXX_DEBUG`.')
$lines.Add('')
$lines.Add('`benchmark_raw.csv` retains every measurement, including verification time. `benchmark_summary.csv` includes medians, minimum/maximum fill times and nanoseconds per entry. `tests/testcases.csv` contains all workload configurations.')
[IO.File]::WriteAllLines((Join-Path $ResultsDirectory 'benchmark_report.md'), $lines)
Write-Output (Join-Path $ResultsDirectory 'benchmark_report.md')
