param(
    [string]$MasterPort = "COM14",
    [string]$SlavePort = "COM4",
    [string]$DutIp = "192.168.0.159",
    [double]$DurationPerCaseS = 60.0
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

. (Join-Path $PSScriptRoot "common.ps1")

Write-Host "============================================================"
Write-Host " A14 RTU-F1 - MEASURE LOADED FIRMWARE"
Write-Host "============================================================"

Assert-G2Branch

if ($DurationPerCaseS -lt 30.0) {
    throw "RTUF1_MEASURE_DURATION_TOO_SHORT"
}

$ports = @(
    [System.IO.Ports.SerialPort]::GetPortNames() |
        Sort-Object
)

if ($MasterPort -notin $ports) {
    throw "RTUF1_MEASURE_MASTER_COM_NOT_PRESENT"
}

if ($SlavePort -notin $ports) {
    throw "RTUF1_MEASURE_SLAVE_COM_NOT_PRESENT"
}

$runner = Get-G2Path "tools/modbus-tcp-benchmark/pc/a14_rtuf1_microsecond_timing.py"

if (-not (Test-Path -LiteralPath $runner)) {
    throw "RTUF1_MEASURE_RUNNER_MISSING"
}

$pythonCommand = Get-Command python.exe -ErrorAction SilentlyContinue

if ($null -eq $pythonCommand) {
    $pythonCommand = Get-Command python -ErrorAction SilentlyContinue
}

if ($null -eq $pythonCommand) {
    throw "RTUF1_MEASURE_PYTHON_NOT_FOUND"
}

$pythonExe = $pythonCommand.Source
$durationText = $DurationPerCaseS.ToString(
    [System.Globalization.CultureInfo]::InvariantCulture
)

Write-Host "MASTER_PORT=$MasterPort"
Write-Host "SLAVE_PORT=$SlavePort"
Write-Host "DUT_IP=$DutIp"
Write-Host "DURATION_PER_CASE_S=$durationText"
Write-Host "COMPILE=NO"
Write-Host "UPLOAD=NO"
Write-Host "EXPECTED_LOADED_FIRMWARE=RTU_F1"

$runnerArgs = @(
    "-u",
    $runner,
    "--master-serial", $MasterPort,
    "--slave-serial", $SlavePort,
    "--host", $DutIp,
    "--port", "502",
    "--duration-per-case", $durationText
)

& $pythonExe @runnerArgs

$runnerExit = [int]$LASTEXITCODE
Write-Host "RTUF1_MEASURE_RUNNER_EXIT=$runnerExit"

if ($runnerExit -ne 0) {
    throw "RTUF1_MEASURE_RUN_FAILED"
}

Write-Host ""
Write-Host "A14_RTU_F1_MEASURE_LOADED_FIRMWARE=PASS"
Write-Host "NEXT=RETURN_OUTPUT_TO_CHAT_BEFORE_RTU_F2"
