param(
    [string]$MasterPort = "COM14",
    [string]$SlavePort = "COM4"
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$gate = Join-Path $PSScriptRoot "a14_p5b_physical_master_slave_combined.ps1"

if (-not (Test-Path -LiteralPath $gate)) {
    throw "P5D_BASE_GATE_NOT_FOUND"
}

Write-Host "============================================================"
Write-Host " A14 P5-D - FULL RUNTIME LONG RUN"
Write-Host " TCP1000 + RTU50 + ALL PERIPHERALS + HMI DIRTY"
Write-Host "============================================================"
Write-Host "MASTER_PORT=$MasterPort"
Write-Host "SLAVE_PORT=$SlavePort"
Write-Host "DURATION_S=600"
Write-Host "LONG_BUCKET_SECONDS=60"
Write-Host "RTU_TIMEOUT_MS=25"
Write-Host "RTU_LIFECYCLE=X_WAIT100MS_R_SLAVE_R_G_AND_FINAL_X"
Write-Host "SD_WORKLOAD_MODE=BUFFERED_DATALOG"
Write-Host "SD_BUFFER_BYTES=4096"
Write-Host "SD_COMMIT_THRESHOLD_BYTES=512"
Write-Host "SD_COMMIT_TIMEOUT_MS=5000"
Write-Host "W5500_SPI_HZ=26000000"
Write-Host "REUSES_P5B_GATE=YES"
Write-Host "PRODUCT_SOURCE_MUTATION=NO"
Write-Host ""

$args = @(
    "-NoLogo",
    "-NoProfile",
    "-ExecutionPolicy", "Bypass",
    "-File", $gate,
    "-MasterPort", $MasterPort,
    "-SlavePort", $SlavePort,
    "-TcpRate", "1000",
    "-DurationS", "600"
)

& powershell.exe @args
$exitCode = $LASTEXITCODE

Write-Host ""
Write-Host "P5D_P5B_BASE_EXIT=$exitCode"

if ($exitCode -ne 0) {
    throw "P5D_LONG_RUN_FAILED"
}

Write-Host ""
Write-Host "============================================================"
Write-Host " A14 P5-D FINAL SUMMARY"
Write-Host "============================================================"
Write-Host "A14_P5D_DURATION_S=600"
Write-Host "A14_P5D_TCP_TARGET_REQ_S=1000"
Write-Host "A14_P5D_RTU_TARGET_HZ=50"
Write-Host "A14_P5D_RTU_TIMEOUT_MS=25"
Write-Host "A14_P5D_SD_WORKLOAD_MODE=BUFFERED_DATALOG"
Write-Host "A14_P5D_W5500_SPI_HZ=26000000"
Write-Host "A14_P5D_FULL_RUNTIME_LONG_RUN=PASS"
Write-Host "NEXT=RETURN_OUTPUT_TO_CHAT_FOR_P5_FINAL_CLOSURE"
