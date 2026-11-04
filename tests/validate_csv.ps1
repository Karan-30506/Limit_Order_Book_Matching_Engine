param([string]$Runner = './run_all_tests.exe')

$ErrorActionPreference = 'Stop'
$originalPath = $env:LOB_TEST_CSV
$temporaryCsv = Join-Path ([IO.Path]::GetTempPath()) ('lob-cases-' + [guid]::NewGuid() + '.csv')
$source = [IO.File]::ReadAllText((Join-Path $PSScriptRoot 'testcases.csv'))
# Keep negative-path checks fast while retaining every named unit and shared scenario.
$quick = [regex]::Replace($source, ',10000,', ',1,').Replace(',1000000,', ',3,').TrimEnd("`r", "`n")
$checks = @(
    @{ Name = 'missing file'; Data = $null; Message = 'cannot open test CSV' },
    @{ Name = 'empty file'; Data = ''; Message = 'test CSV has no cases' },
    @{ Name = 'header only'; Data = ($source -split "`n")[0]; Message = 'test CSV has no cases' },
    @{ Name = 'wrong header'; Data = $quick.Replace('suite,case,', 'wrong,case,'); Message = 'invalid CSV header' },
    @{ Name = 'short row'; Data = $quick + "`nShared,broken"; Message = 'invalid CSV row' },
    @{ Name = 'unknown suite'; Data = $quick.Replace('Order,testConstructors', 'Typo,testConstructors'); Message = 'unknown suite' },
    @{ Name = 'unknown unit'; Data = $quick.Replace('testConstructors', 'testTypo'); Message = 'unknown or duplicate unit case' },
    @{ Name = 'duplicate unit'; Data = $quick + "`nOrder,testConstructors,unit,,,,,,,"; Message = 'unknown or duplicate unit case' },
    @{ Name = 'missing unit'; Data = [regex]::Replace($quick, '(?m)^Order,testConstructors[^\n]*\n', ''); Message = 'must list every registered' },
    @{ Name = 'unclosed quote'; Data = $quick + "`n`"broken"; Message = 'unterminated CSV quote' },
    @{ Name = 'quote in field'; Data = $quick + "`nSha`"red,broken,,,,,,,,"; Message = 'invalid CSV quoting' },
    @{ Name = 'unknown operation'; Data = $quick.Replace('fifo_and_priority,insert,1', 'fifo_and_priority,typo,1'); Message = 'unknown operation' },
    @{ Name = 'bad side'; Data = $quick.Replace('insert,1,100,10,Buy', 'insert,1,100,10,Typo'); Message = 'invalid side' },
    @{ Name = 'bad integer'; Data = $quick.Replace('insert,1,100,10,Buy', 'insert,1x,100,10,Buy'); Message = 'invalid integer' },
    @{ Name = 'wrong expectation'; Data = $quick.Replace('insert,1,100,10,Buy,,,true', 'insert,1,100,10,Buy,,,false'); Message = 'CSV expectation disagrees' },
    @{ Name = 'bad stress steps'; Data = $quick.Replace(',42,1,', ',42,0,'); Message = 'invalid stress seed/steps' },
    @{ Name = 'no shared cases'; Data = [regex]::Replace($quick, '(?m)^Shared[^\n]*\n?', ''); Message = 'must contain shared scenarios' }
)

try {
    $env:LOB_TEST_CSV = $temporaryCsv
    foreach ($check in $checks) {
        if ($null -eq $check.Data) {
            $env:LOB_TEST_CSV = $temporaryCsv + '.missing'
        } else {
            $env:LOB_TEST_CSV = $temporaryCsv
            [IO.File]::WriteAllText($temporaryCsv, $check.Data)
        }
        $output = (& $Runner 2>&1 | Out-String)
        if ($LASTEXITCODE -eq 0 -or !$output.Contains($check.Message)) {
            throw "CSV check failed: $($check.Name)`n$output"
        }
        Write-Output "[PASS] CSV rejects $($check.Name)"
    }
    # Check a valid override with CRLF, quoted fields and escaped quote parsing.
    $env:LOB_TEST_CSV = $temporaryCsv
    $quoted = $quick.Replace('Order,testConstructors,', '"Order","testConstructors",')
    $quoted = $quoted.Replace('fifo_and_priority', '"fifo,""priority"""')
    [IO.File]::WriteAllText($temporaryCsv, $quoted)
    $output = (& $Runner 2>&1 | Out-String)
    if ($LASTEXITCODE -ne 0) {
        throw "Valid quoted CSV override failed:`n$output"
    }
    Write-Output '[PASS] CSV accepts quoted fields and custom path'
    Write-Output "$($checks.Count + 1) CSV validation checks passed"
} finally {
    $env:LOB_TEST_CSV = $originalPath
    if (Test-Path -LiteralPath $temporaryCsv) {
        Remove-Item -LiteralPath $temporaryCsv
    }
}
