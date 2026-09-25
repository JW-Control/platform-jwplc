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
Write-Host " A14 RTU-F2C - TCP500 GAP CONFIRMATION"
Write-Host "============================================================"

Assert-G2Branch

if ($DurationPerCaseS -lt 30.0) {
    throw "RTUF2C_DURATION_TOO_SHORT"
}

$ports = @(
    [System.IO.Ports.SerialPort]::GetPortNames() |
        Sort-Object
)

if ($MasterPort -notin $ports) {
    throw "RTUF2C_MASTER_COM_NOT_PRESENT"
}

if ($SlavePort -notin $ports) {
    throw "RTUF2C_SLAVE_COM_NOT_PRESENT"
}

$runner = Get-G2Path "tools/modbus-tcp-benchmark/pc/a14_rtuf2c_tcp500_confirmation.py"

if (-not (Test-Path -LiteralPath $runner)) {
    throw "RTUF2C_RUNNER_MISSING"
}

$pythonCommand = Get-Command python.exe -ErrorAction SilentlyContinue

if ($null -eq $pythonCommand) {
    $pythonCommand = Get-Command python -ErrorAction SilentlyContinue
}

if ($null -eq $pythonCommand) {
    throw "RTUF2C_PYTHON_NOT_FOUND"
}

$pythonExe = $pythonCommand.Source
$durationText = $DurationPerCaseS.ToString(
    [System.Globalization.CultureInfo]::InvariantCulture
)

Write-Host "HEAD=$(Get-G2Head)"
Write-Host "MASTER_PORT=$MasterPort"
Write-Host "SLAVE_PORT=$SlavePort"
Write-Host "DUT_IP=$DutIp"
Write-Host "DURATION_PER_CASE_S=$durationText"
Write-Host "GAPS_US=600,500,300"
Write-Host "TCP_TARGET_REQ_S=500"
Write-Host "COMPILE=NO"
Write-Host "UPLOAD=NO"
Write-Host "EXPECTED_LOADED_FIRMWARE=RTU_F2B"

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
Write-Host "RTUF2C_RUNNER_EXIT=$runnerExit"

if ($runnerExit -ne 0) {
    throw "RTUF2C_RUN_FAILED"
}

Write-Host ""
Write-Host "A14_RTU_F2C_TCP500_CONFIRMATION_GATE=PASS"
Write-Host "NEXT=RETURN_OUTPUT_TO_CHAT_BEFORE_RTU_F3_TX"
