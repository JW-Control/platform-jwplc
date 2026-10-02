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
Write-Host " ALPHA14 G3B-D3-C3 - SLOW-STREAK 4 FULL-RUNTIME"
Write-Host "=============================================================================="
Write-Host "BRANCH=$branch"
Write-Host "HEAD=$head"
Write-Host "D3C3_DURATION_S=60"
Write-Host "D3C3_TCP_TARGET_REQ_S=1000"
Write-Host "D3C3_PURPOSE=TEST_SLOW_STREAK_4_ONLY"
Write-Host "D3C3_PRODUCT_SOURCE_MUTATION=NO"

if ($branch -ne "v2.1.0-alpha.14/feature/modbus-tcp") { throw "D3C3_BRANCH_MISMATCH=$branch" }
$dirty = @(& git -C $repo diff --name-only)
$staged = @(& git -C $repo diff --cached --name-only)
if ($dirty.Count -ne 0 -or $staged.Count -ne 0) {
    $dirty | ForEach-Object { Write-Host "DIRTY=$_" }
    $staged | ForEach-Object { Write-Host "STAGED=$_" }
    throw "D3C3_TREE_NOT_CLEAN"
}

$header = Join-Path $repo "JWPLC\2.1.0\libraries\JWPLC_ModbusTCP\src\JWPLC_ModbusTCP.h"
$headerText = [IO.File]::ReadAllText($header)
$contracts = @(
    [PSCustomObject]@{ Key = "INT"; Token = "#define JWPLC_MODBUS_TCP_INT_GUIDED_RX 0" },
    [PSCustomObject]@{ Key = "HOT"; Token = "#define JWPLC_MODBUS_TCP_INT_HOT_POLL_US 0UL" },
    [PSCustomObject]@{ Key = "D3"; Token = "#define JWPLC_MODBUS_TCP_INT_LOAD_ADAPTIVE 0" },
    [PSCustomObject]@{ Key = "PROFILE"; Token = "#define JWPLC_MODBUS_TCP_ENABLE_PROFILE_HOOKS 0" },
    [PSCustomObject]@{ Key = "ACTIVE_IDLE"; Token = "#define JWPLC_MODBUS_TCP_ACTIVE_IDLE_EXIT_US 5000UL" },
    [PSCustomObject]@{ Key = "ACTIVE_SLOW_STREAK"; Token = "#define JWPLC_MODBUS_TCP_ACTIVE_SLOW_STREAK 2U" }
)
foreach ($contract in $contracts) {
    $ok = $headerText.Contains($contract.Token)
    Write-Host "D3C3_SOURCE_DEFAULT_$($contract.Key)=$(if ($ok) { "PASS" } else { "FAIL" })"
    if (-not $ok) { throw "D3C3_SOURCE_DEFAULT_CONTRACT_FAIL_$($contract.Key)" }
}

if ([string]::IsNullOrWhiteSpace($PythonExe)) {
    $cmd = Get-Command python.exe -ErrorAction SilentlyContinue
    if ($null -eq $cmd) { $cmd = Get-Command python -ErrorAction Stop }
    $python = $cmd.Source
} else {
    $python = (Resolve-Path -LiteralPath $PythonExe).Path
}

$p5b = Join-Path $PSScriptRoot "a14_p5b_physical_master_slave_combined.ps1"
$flags = "-DJWPLC_MODBUS_TCP_INT_GUIDED_RX=1 -DJWPLC_MODBUS_TCP_INT_LOAD_ADAPTIVE=1 -DJWPLC_MODBUS_TCP_ENABLE_PROFILE_HOOKS=1 -DJWPLC_MODBUS_TCP_ACTIVE_IDLE_EXIT_US=20000 -DJWPLC_MODBUS_TCP_ACTIVE_SLOW_STREAK=4"
Write-Host "D3C3_MASTER_EXTRA_CPP_FLAGS=$flags"

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
if ($rootMatches.Count -ne 1) { throw "D3C3_TEMP_ROOT_INVALID_COUNT=$($rootMatches.Count)" }
$root = $rootMatches[0].Groups[1].Value.Trim()
$masterSnapshot = Join-Path $root "master_final.txt"
$tcpCsv = Join-Path $root "tcp_result.csv"
$compileLog = Join-Path $root "compile_master.log"
foreach ($required in @($masterSnapshot, $tcpCsv, $compileLog)) {
    if (-not (Test-Path -LiteralPath $required)) { throw "D3C3_REQUIRED_RESULT_MISSING=$required" }
}

$masterText = [IO.File]::ReadAllText($masterSnapshot)
$compileText = [IO.File]::ReadAllText($compileLog).ToLowerInvariant()
$tcpRows = @(Import-Csv -LiteralPath $tcpCsv)
if ($tcpRows.Count -ne 1) { throw "D3C3_TCP_CSV_ROW_COUNT=$($tcpRows.Count)" }
$row = $tcpRows[0]
foreach ($flag in @("-djwplc_modbus_tcp_int_guided_rx=1", "-djwplc_modbus_tcp_int_load_adaptive=1", "-djwplc_modbus_tcp_enable_profile_hooks=1", "-djwplc_modbus_tcp_active_idle_exit_us=20000", "-djwplc_modbus_tcp_active_slow_streak=4")) {
    if (-not $compileText.Contains($flag)) { throw "D3C3_BUILD_FLAG_MISSING=$flag" }
}
Write-Host "D3C3_BUILD_FLAGS=PASS"

function Get-D3C2Int([string]$Key) {
    $m = [regex]::Matches($masterText, "(?m)^" + [regex]::Escape($Key) + "=(\d+)\r?$")
    if ($m.Count -ne 1) { throw "D3C3_PROFILE_KEY_INVALID=$Key COUNT=$($m.Count)" }
    return [int64]::Parse($m[0].Groups[1].Value, [Globalization.CultureInfo]::InvariantCulture)
}

$state = Get-D3C2Int "D3_PROFILE_STATE"
$frames = Get-D3C2Int "D3_PROFILE_COMPLETE_FRAMES"
$warm = Get-D3C2Int "D3_PROFILE_TO_WARM"
$active = Get-D3C2Int "D3_PROFILE_TO_ACTIVE_POLL"
$cooldown = Get-D3C2Int "D3_PROFILE_TO_COOLDOWN"
$idle = Get-D3C2Int "D3_PROFILE_TO_IDLE_INT"
$activePasses = Get-D3C2Int "D3_PROFILE_ACTIVE_POLL_PASSES"
$fallback = Get-D3C2Int "D3_PROFILE_FALLBACK_PASSES"
$realigns = Get-D3C2Int "D3_PROFILE_IDLE_FALLBACK_REALIGNS"
$activeIdleExits = Get-D3C2Int "D3_PROFILE_ACTIVE_IDLE_EXITS"
$nonActiveIdleExits = Get-D3C2Int "D3_PROFILE_NONACTIVE_IDLE_EXITS"
$lastGap = Get-D3C2Int "D3_PROFILE_LAST_FRAME_GAP_US"

$req = [double]::Parse($row.achieved_req_s, [Globalization.CultureInfo]::InvariantCulture)
$p95 = [double]::Parse($row.latency_p95_us, [Globalization.CultureInfo]::InvariantCulture)
$p99 = [double]::Parse($row.latency_p99_us, [Globalization.CultureInfo]::InvariantCulture)
$loopAvg = [int64]::Parse($row.loop_gap_avg_us, [Globalization.CultureInfo]::InvariantCulture)
$activeStable = ($active -eq 1 -and $cooldown -eq 0 -and $idle -eq 0 -and $state -eq 2)
$flapping = ($active -gt 1 -or $cooldown -gt 0 -or $idle -gt 0)

Write-Host ""
Write-Host "=============================================================================="
Write-Host " D3-C3 SUMMARY"
Write-Host "=============================================================================="
Write-Host ("D3C3_TCP_REQ_S={0:F3}" -f $req)
Write-Host ("D3C3_TCP_P95_US={0:F1}" -f $p95)
Write-Host ("D3C3_TCP_P99_US={0:F1}" -f $p99)
Write-Host "D3C3_LOOP_AVG_US=$loopAvg"
Write-Host "D3C3_STATE=$state"
Write-Host "D3C3_COMPLETE_FRAMES=$frames"
Write-Host "D3C3_TO_WARM=$warm"
Write-Host "D3C3_TO_ACTIVE_POLL=$active"
Write-Host "D3C3_TO_COOLDOWN=$cooldown"
Write-Host "D3C3_TO_IDLE_INT=$idle"
Write-Host "D3C3_ACTIVE_POLL_PASSES=$activePasses"
Write-Host "D3C3_FALLBACK_PASSES=$fallback"
Write-Host "D3C3_IDLE_FALLBACK_REALIGNS=$realigns"
Write-Host "D3C3_ACTIVE_IDLE_EXITS=$activeIdleExits"
Write-Host "D3C3_NONACTIVE_IDLE_EXITS=$nonActiveIdleExits"
Write-Host "D3C3_LAST_FRAME_GAP_US=$lastGap"
Write-Host "D3C3_ACTIVE_STABLE=$($activeStable.ToString().ToUpperInvariant())"
Write-Host "D3C3_TRANSITION_FLAPPING=$($flapping.ToString().ToUpperInvariant())"
Write-Host "D3C3_PRODUCT_DEFAULT_CHANGED=NO"
Write-Host "D3C3_RESULT_ROOT=$root"
Write-Host "D3C3_ACTIVE_IDLE_EXIT_US=20000"
Write-Host "D3C3_ACTIVE_SLOW_STREAK=4"
Write-Host "D3C3_NEXT=COMPARE_AGAINST_D3C2_BEFORE_ANY_FURTHER_TUNING"
Write-Host "D3C3_STATUS=PASS_CHARACTERIZATION"
