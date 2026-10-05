param(
    [string]$MasterPort = "COM14",
    [string]$SlavePort = "COM4",
    [string]$PythonExe = ""
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$repo = (& git -C (Join-Path $PSScriptRoot "..\..\..") rev-parse --show-toplevel).Trim()
$branch = (& git -C $repo branch --show-current).Trim()
$head = (& git -C $repo rev-parse HEAD).Trim()

Write-Host "=============================================================================="
Write-Host " ALPHA14 G3B-E1 - INT RSR-DRAIN FULL-RUNTIME"
Write-Host "=============================================================================="
Write-Host "BRANCH=$branch"
Write-Host "HEAD=$head"
Write-Host "E1_DURATION_S=60"
Write-Host "E1_TCP_TARGET_REQ_S=1000"
Write-Host "E1_POLICY=INT_WAKE_RSR_DRAIN"
Write-Host "E1_D2_HOT_POLL=OFF"
Write-Host "E1_D3_LOAD_ADAPTIVE=OFF"
Write-Host "E1_PROFILE_HOOKS=OFF"
Write-Host "E1_PRODUCT_DEFAULT_CHANGED=NO"

if ($branch -ne "v2.1.0-alpha.14/feature/modbus-tcp") { throw "E1_BRANCH_MISMATCH=$branch" }
$dirty = @(& git -C $repo diff --name-only)
$staged = @(& git -C $repo diff --cached --name-only)
if ($dirty.Count -ne 0 -or $staged.Count -ne 0) {
    $dirty | ForEach-Object { Write-Host "DIRTY=$_" }
    $staged | ForEach-Object { Write-Host "STAGED=$_" }
    throw "E1_TREE_NOT_CLEAN"
}

$header = Join-Path $repo "JWPLC\2.1.0\libraries\JWPLC_ModbusTCP\src\JWPLC_ModbusTCP.h"
$headerText = [IO.File]::ReadAllText($header)
$contracts = @(
    [PSCustomObject]@{ Key = "INT"; Token = "#define JWPLC_MODBUS_TCP_INT_GUIDED_RX 0" },
    [PSCustomObject]@{ Key = "HOT"; Token = "#define JWPLC_MODBUS_TCP_INT_HOT_POLL_US 0UL" },
    [PSCustomObject]@{ Key = "D3"; Token = "#define JWPLC_MODBUS_TCP_INT_LOAD_ADAPTIVE 0" },
    [PSCustomObject]@{ Key = "RSR_DRAIN"; Token = "#define JWPLC_MODBUS_TCP_INT_RSR_DRAIN 0" }
)
foreach ($contract in $contracts) {
    $ok = $headerText.Contains($contract.Token)
    Write-Host "E1_SOURCE_DEFAULT_$($contract.Key)=$(if ($ok) { "PASS" } else { "FAIL" })"
    if (-not $ok) { throw "E1_SOURCE_DEFAULT_CONTRACT_FAIL_$($contract.Key)" }
}

if ([string]::IsNullOrWhiteSpace($PythonExe)) {
    $cmd = Get-Command python.exe -ErrorAction SilentlyContinue
    if ($null -eq $cmd) { $cmd = Get-Command python -ErrorAction Stop }
    $python = $cmd.Source
} else {
    $python = (Resolve-Path -LiteralPath $PythonExe).Path
}

$p5b = Join-Path $PSScriptRoot "a14_p5b_physical_master_slave_combined.ps1"
$flags = "-DJWPLC_MODBUS_TCP_INT_GUIDED_RX=1 -DJWPLC_MODBUS_TCP_INT_RSR_DRAIN=1"
Write-Host "E1_MASTER_EXTRA_CPP_FLAGS=$flags"

$p5bArgs = @{
    MasterPort = $MasterPort
    SlavePort = $SlavePort
    TcpRate = 1000
    DurationS = 60
    PythonExe = $python
    MasterExtraCppFlags = $flags
    ReturnOnQualificationFailure = $true
    AutomatedOnly = $true
}
[object[]]$out = @(& $p5b @p5bArgs *>&1)
[string[]]$lines = @($out | ForEach-Object { $_.ToString() })
$lines | ForEach-Object { Write-Host $_ }
$text = $lines -join [Environment]::NewLine

$rootMatches = [regex]::Matches($text, "(?m)^P5B_TEMP_ROOT=(.+)\r?$")
if ($rootMatches.Count -ne 1) { throw "E1_TEMP_ROOT_INVALID_COUNT=$($rootMatches.Count)" }
$root = $rootMatches[0].Groups[1].Value.Trim()
$tcpCsv = Join-Path $root "tcp_result.csv"
$compileLog = Join-Path $root "compile_master.log"
foreach ($required in @($tcpCsv, $compileLog)) {
    if (-not (Test-Path -LiteralPath $required)) { throw "E1_REQUIRED_RESULT_MISSING=$required" }
}

$compileText = [IO.File]::ReadAllText($compileLog).ToLowerInvariant()
foreach ($flag in @("-djwplc_modbus_tcp_int_guided_rx=1", "-djwplc_modbus_tcp_int_rsr_drain=1")) {
    if (-not $compileText.Contains($flag)) { throw "E1_BUILD_FLAG_MISSING=$flag" }
}
foreach ($forbidden in @("-djwplc_modbus_tcp_int_load_adaptive=1", "-djwplc_modbus_tcp_int_hot_poll_us=", "-djwplc_modbus_tcp_enable_profile_hooks=1")) {
    if ($compileText.Contains($forbidden)) { throw "E1_FORBIDDEN_BUILD_FLAG=$forbidden" }
}
Write-Host "E1_BUILD_FLAGS=PASS"

$rows = @(Import-Csv -LiteralPath $tcpCsv)
if ($rows.Count -ne 1) { throw "E1_TCP_CSV_ROW_COUNT=$($rows.Count)" }
$row = $rows[0]
$inv = [Globalization.CultureInfo]::InvariantCulture
$req = [double]::Parse($row.achieved_req_s, $inv)
$avg = [double]::Parse($row.latency_avg_us, $inv)
$p95 = [double]::Parse($row.latency_p95_us, $inv)
$p99 = [double]::Parse($row.latency_p99_us, $inv)
$max = [double]::Parse($row.latency_max_us, $inv)
$loopAvg = [int64]::Parse($row.loop_gap_avg_us, $inv)
$loopMax = [int64]::Parse($row.loop_gap_max_us, $inv)

Write-Host ""
Write-Host "=============================================================================="
Write-Host " G3B-E1 SUMMARY"
Write-Host "=============================================================================="
Write-Host ("E1_TCP_REQ_S={0:F3}" -f $req)
Write-Host ("E1_TCP_AVG_US={0:F1}" -f $avg)
Write-Host ("E1_TCP_P95_US={0:F1}" -f $p95)
Write-Host ("E1_TCP_P99_US={0:F1}" -f $p99)
Write-Host ("E1_TCP_MAX_US={0:F1}" -f $max)
Write-Host "E1_LOOP_AVG_US=$loopAvg"
Write-Host "E1_LOOP_MAX_US=$loopMax"
Write-Host "E1_RESULT_ROOT=$root"
Write-Host "E1_PRODUCT_DEFAULT_CHANGED=NO"
Write-Host "E1_NEXT=COMPARE_TO_C0_AND_DECIDE_CLOSE_OR_ONE_MATCHED_C0"
Write-Host "E1_STATUS=PASS_CHARACTERIZATION"
