Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

Write-Host "============================================================"
Write-Host " A14 P3J - DUMP LATEST FAILURE"
Write-Host "============================================================"
Write-Host "BENCHMARK_RERUN=NO"
Write-Host "PRODUCT_MUTATION=NO"

$repeatabilityRoot = @(
    Get-ChildItem -LiteralPath $env:TEMP -Directory -Filter "jwplc_a14_p3j_repeatability_*" -ErrorAction SilentlyContinue |
        Sort-Object LastWriteTime -Descending
) | Select-Object -First 1

if ($null -eq $repeatabilityRoot) {
    throw "P3J_LATEST_REPEATABILITY_ROOT_NOT_FOUND"
}

Write-Host "P3J_LATEST_REPEATABILITY_ROOT=$($repeatabilityRoot.FullName)"

$blockLogs = @(
    Get-ChildItem -LiteralPath $repeatabilityRoot.FullName -File -Filter "block_*.log" -ErrorAction SilentlyContinue |
        Sort-Object Name
)

if ($blockLogs.Count -eq 0) {
    throw "P3J_BLOCK_LOG_NOT_FOUND"
}

foreach ($blockLog in $blockLogs) {
    Write-Host ""
    Write-Host "=== BLOCK LOG: $($blockLog.Name) ==="
    Get-Content -LiteralPath $blockLog.FullName -Tail 320 | ForEach-Object { Write-Host $_ }
}

$runRoot = @(
    Get-ChildItem -LiteralPath $env:TEMP -Directory -Filter "jwplc_a14_p3j_udp_rx_commit_ab_*" -ErrorAction SilentlyContinue |
        Sort-Object LastWriteTime -Descending
) | Select-Object -First 1

if ($null -eq $runRoot) {
    throw "P3J_LATEST_RUN_ROOT_NOT_FOUND"
}

Write-Host ""
Write-Host "P3J_LATEST_RUN_ROOT=$($runRoot.FullName)"

$commitDir = Join-Path $runRoot.FullName "INT_COMMIT2"
if (-not (Test-Path -LiteralPath $commitDir)) {
    throw "P3J_INT_COMMIT2_DIR_NOT_FOUND"
}

$runLogs = @(
    Get-ChildItem -LiteralPath $commitDir -File -Filter "udp_rx_INT_COMMIT2_payload_1016_run_*.log" -ErrorAction SilentlyContinue |
        Sort-Object Name
)

if ($runLogs.Count -eq 0) {
    throw "P3J_INT_COMMIT2_RUN_LOG_NOT_FOUND"
}

foreach ($runLog in $runLogs) {
    Write-Host ""
    Write-Host "============================================================"
    Write-Host " RUN LOG: $($runLog.Name)"
    Write-Host "============================================================"

    $text = [System.IO.File]::ReadAllText($runLog.FullName)

    foreach ($key in @(
        "P3H_ARM_RESET_PASS",
        "P3H_ARM_RESET_RX_BYTES",
        "P3H_ARM_RESET_RX_OPERATIONS",
        "RAW_BENCH_FUNCTIONAL_PASS",
        "SUMMARY_UDP_RX_DUT_MBPS",
        "SUMMARY_UDP_RX_PASS",
        "P3H_FINAL_MODE",
        "P3H_FINAL_RX_BYTES",
        "P3H_FINAL_RX_OPERATIONS",
        "P3H_FINAL_TRANSPORT_ERRORS",
        "P3H_FINAL_UDP_SPI_LOCK_ERRORS",
        "P3H_FINAL_UDP_RX_PACKETS",
        "P3H_FINAL_UDP_RX_SERVICE_HOLD_COUNT",
        "P3H_FINAL_UDP_RX_ACTIVE_HOLD_COUNT",
        "P3H_FINAL_UDP_RX_EMPTY_HOLD_COUNT",
        "P3H_FINAL_UDP_RX_ACTIVE_HOLD_US_AVG",
        "P3H_FINAL_UDP_RX_ACTIVE_HOLD_US_MAX",
        "P3H_FINAL_W5100_DIAG_READ_CALLS_TOTAL",
        "P3H_FINAL_ETH_INT_ISR_COUNT",
        "P3H_FINAL_UDP_RX_INT_SKIP_COUNT",
        "P3H_FINAL_UDP_RX_INT_WAKE_COUNT"
    )) {
        $m = [regex]::Match(
            $text,
            "(?m)^" + [regex]::Escape($key) + "=(.*)\r?$"
        )

        if ($m.Success) {
            Write-Host "$key=$($m.Groups[1].Value.Trim())"
        }
        else {
            Write-Host "$key=<MISSING>"
        }
    }

    Write-Host ""
    Write-Host "--- RUN LOG TAIL ---"
    Get-Content -LiteralPath $runLog.FullName -Tail 220 | ForEach-Object { Write-Host $_ }
}

Write-Host ""
Write-Host "A14_P3J_LATEST_FAILURE_DUMP=PASS"
Write-Host "NEXT_ACTION=RETURN_OUTPUT_TO_CHAT_FOR_ROOT_CAUSE"
