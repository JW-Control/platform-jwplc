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
Write-Host " ALPHA14 G3B-D2-C0 - CURRENT-HEAD POLLING FULL-RUNTIME CONTROL"
Write-Host "=============================================================================="
Write-Host "BRANCH=$branch"
Write-Host "HEAD=$head"
Write-Host "C0_PRODUCT_SOURCE_MUTATION=NO"
Write-Host "C0_MASTER_EXTRA_CPP_FLAGS=NONE"

if ($branch -ne "v2.1.0-alpha.14/feature/modbus-tcp") {
    throw "C0_BRANCH_MISMATCH=$branch"
}

$dirty = @(& git -C $repo diff --name-only)
$staged = @(& git -C $repo diff --cached --name-only)

if ($dirty.Count -ne 0 -or $staged.Count -ne 0) {
    $dirty | ForEach-Object { Write-Host "DIRTY=$_" }
    $staged | ForEach-Object { Write-Host "STAGED=$_" }
    throw "C0_TREE_NOT_CLEAN"
}

$header = Join-Path $repo "JWPLC\2.1.0\libraries\JWPLC_ModbusTCP\src\JWPLC_ModbusTCP.h"
$headerText = [IO.File]::ReadAllText($header)

if (-not $headerText.Contains("#define JWPLC_MODBUS_TCP_INT_GUIDED_RX 0")) {
    throw "C0_INT_DEFAULT_NOT_OFF"
}
if (-not $headerText.Contains("#define JWPLC_MODBUS_TCP_INT_HOT_POLL_US 0UL")) {
    throw "C0_HOT_POLL_DEFAULT_NOT_ZERO"
}

Write-Host "C0_INT_GUIDED_RX_PACKAGE_DEFAULT=0"
Write-Host "C0_INT_HOT_POLL_US_PACKAGE_DEFAULT=0"

$h3er = Join-Path $PSScriptRoot "a14_h3er_current_package_full_runtime.ps1"

$args = @{
    MasterPort = $MasterPort
    SlavePort = $SlavePort
    TcpRate = 1000
    DurationS = 120
}
if (-not [string]::IsNullOrWhiteSpace($PythonExe)) {
    $args.PythonExe = $PythonExe
}

[object[]]$out = @(& $h3er @args *>&1)
[string[]]$lines = @($out | ForEach-Object { $_.ToString() })
$lines | ForEach-Object { Write-Host $_ }
$text = $lines -join [Environment]::NewLine

function Get-C0Double {
    param([string]$Key)
    $m = [regex]::Matches($text, "(?m)^" + [regex]::Escape($Key) + "=([0-9.]+)\r?$")
    if ($m.Count -ne 1) { throw "C0_KEY_INVALID=$Key COUNT=$($m.Count)" }
    return [double]::Parse($m[0].Groups[1].Value, [Globalization.CultureInfo]::InvariantCulture)
}

function Get-C0Int {
    param([string]$Key)
    $m = [regex]::Matches($text, "(?m)^" + [regex]::Escape($Key) + "=(\d+)\r?$")
    if ($m.Count -ne 1) { throw "C0_KEY_INVALID=$Key COUNT=$($m.Count)" }
    return [int64]::Parse($m[0].Groups[1].Value, [Globalization.CultureInfo]::InvariantCulture)
}

if (-not $text.Contains("A14_H3E_R_POST_P4_2=PASS")) {
    throw "C0_H3ER_NOT_PASS"
}
if (-not $text.Contains("H3ER_INT_GUIDED_RX_BUILD=OFF")) {
    throw "C0_INT_BUILD_NOT_OFF"
}
if (-not $text.Contains("H3ER_INT_HOT_POLL_US_BUILD=0")) {
    throw "C0_HOT_POLL_BUILD_NOT_ZERO"
}

$req = Get-C0Double "H3ER_TCP_ACHIEVED_REQ_S"
$sent = Get-C0Int "H3ER_TCP_REQUESTS_SENT"
$ok = Get-C0Int "H3ER_TCP_REQUESTS_OK"
$avg = Get-C0Double "H3ER_TCP_LATENCY_AVG_US"
$p95 = Get-C0Double "H3ER_TCP_P95_US"
$p99 = Get-C0Double "H3ER_TCP_P99_US"
$max = Get-C0Double "H3ER_TCP_MAX_US"
$loopAvg = Get-C0Double "H3ER_LOOP_AVG_US"
$loopMax = Get-C0Double "H3ER_LOOP_MAX_US"

$d0Avg = 884.8
$d0P95 = 1269.2
$d0P99 = 2272.5
$d0Max = 7942.6
$d2Avg = 937.8
$d2P95 = 1360.8
$d2P99 = 2953.7
$d2Max = 17182.9

function DeltaPct([double]$value, [double]$reference) {
    return (($value / $reference) - 1.0) * 100.0
}

$reproducesLowTail =
    $sent -eq 120000 -and
    $ok -eq 120000 -and
    $req -ge 999.95 -and
    $p95 -le ($d0P95 * 1.05) -and
    $p99 -le ($d0P99 * 1.10)

Write-Host ""
Write-Host "=============================================================================="
Write-Host " G3B-D2-C0 SUMMARY"
Write-Host "=============================================================================="
Write-Host ("C0_TCP_REQ_S={0:F3}" -f $req)
Write-Host "C0_TCP_REQUESTS_SENT=$sent"
Write-Host "C0_TCP_REQUESTS_OK=$ok"
Write-Host ("C0_AVG_US={0:F1}" -f $avg)
Write-Host ("C0_P95_US={0:F1}" -f $p95)
Write-Host ("C0_P99_US={0:F1}" -f $p99)
Write-Host ("C0_MAX_US={0:F1}" -f $max)
Write-Host ("C0_LOOP_AVG_US={0:F1}" -f $loopAvg)
Write-Host ("C0_LOOP_MAX_US={0:F1}" -f $loopMax)

Write-Host ("C0_DELTA_VS_D0_AVG_PCT={0:F3}" -f (DeltaPct $avg $d0Avg))
Write-Host ("C0_DELTA_VS_D0_P95_PCT={0:F3}" -f (DeltaPct $p95 $d0P95))
Write-Host ("C0_DELTA_VS_D0_P99_PCT={0:F3}" -f (DeltaPct $p99 $d0P99))
Write-Host ("C0_DELTA_VS_D0_MAX_PCT={0:F3}" -f (DeltaPct $max $d0Max))

Write-Host ("C0_DELTA_VS_D2_AVG_PCT={0:F3}" -f (DeltaPct $avg $d2Avg))
Write-Host ("C0_DELTA_VS_D2_P95_PCT={0:F3}" -f (DeltaPct $p95 $d2P95))
Write-Host ("C0_DELTA_VS_D2_P99_PCT={0:F3}" -f (DeltaPct $p99 $d2P99))
Write-Host ("C0_DELTA_VS_D2_MAX_PCT={0:F3}" -f (DeltaPct $max $d2Max))

Write-Host "C0_POLLING_REPRODUCES_LOW_TAIL=$(
    if ($reproducesLowTail) { "YES" } else { "NO" })"

if ($reproducesLowTail) {
    Write-Host "C0_INTERPRETATION=D2_TAIL_COST_CONFIRMED"
    Write-Host "C0_NEXT=G3B_D3_LOAD_ADAPTIVE_DESIGN"
    Write-Host "C0_STATUS=PASS_COST_CONFIRMED"
}
else {
    Write-Host "C0_INTERPRETATION=FULL_RUNTIME_VARIABILITY_REVIEW"
    Write-Host "C0_NEXT=REVIEW_C0_BEFORE_D3"
    Write-Host "C0_STATUS=REVIEW"
}
