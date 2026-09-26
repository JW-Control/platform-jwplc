param(
    [int]$MHz = 26,
    [int]$ExpectedChunks = 8,
    [string]$SerialPort = "COM14",
    [string]$DutIp = "192.168.0.31",
    [double]$DurationSeconds = 2.0,
    [int]$Pairs = 12
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"
. (Join-Path $PSScriptRoot "common.ps1")

$expectedFirmwareSha256 = "08E16E60F008371416859B63F93D7C8B7D2FB88325FB7281AC6C5C72983FEA8F"
$signatureMinUs = 900000
$signatureMaxUs = 1100000

function Invoke-NativeToLog {
    param([string]$FilePath, [string[]]$Arguments, [string]$LogPath)
    & $FilePath @Arguments *> $LogPath
    return [int]$LASTEXITCODE
}

function Get-LogValue {
    param([string]$Text, [string]$Key)
    $pattern = "(?m)^" + [regex]::Escape($Key) + "=(.*)\r?$"
    $matches = @([regex]::Matches($Text, $pattern))
    if ($matches.Count -ne 1) { throw "G3_DIAG_LOG_KEY_COUNT_${Key}=$($matches.Count)" }
    return $matches[0].Groups[1].Value.Trim()
}

function Get-LogInt64 {
    param([string]$Text, [string]$Key)
    return [int64]::Parse((Get-LogValue -Text $Text -Key $Key), [System.Globalization.CultureInfo]::InvariantCulture)
}

function Get-ChunkCount {
    $firmwarePath = Get-G2Path $script:G2RawFirmwareRelative
    $text = [System.IO.File]::ReadAllText($firmwarePath)
    $pattern = '(?m)^\s*static constexpr uint8_t TCP_RX_MAX_CHUNKS_PER_LOCK = (\d+);\s*$'
    $matches = @([regex]::Matches($text, $pattern))
    if ($matches.Count -ne 1) { throw "G3_DIAG_CHUNK_DECLARATION_COUNT=$($matches.Count)" }
    return [int]$matches[0].Groups[1].Value
}

Write-Host "============================================================"
Write-Host " G3 TCP RX->TX TRANSITION HOLD DIAGNOSTIC"
Write-Host "============================================================"

Assert-G2Branch
Assert-G2AllowedMHz -MHz $MHz

if ($DurationSeconds -le 0 -or $Pairs -lt 1) { throw "G3_DIAG_INVALID_ARGUMENTS" }

$effectiveHz = Get-G2SpiHz
$targetHz = [int64]$MHz * 1000000
$effectiveChunks = Get-ChunkCount
$head = Get-G2Head

Write-Host "HEAD=$head"
Write-Host "TARGET_MHZ=$MHz"
Write-Host "EFFECTIVE_SPI_HZ=$effectiveHz"
Write-Host "EXPECTED_CHUNKS=$ExpectedChunks"
Write-Host "EFFECTIVE_CHUNKS=$effectiveChunks"
Write-Host "PAIRS=$Pairs"
Write-Host "DURATION_S=$DurationSeconds"
Write-Host "ONE_SECOND_SIGNATURE_MIN_US=$signatureMinUs"
Write-Host "ONE_SECOND_SIGNATURE_MAX_US=$signatureMaxUs"
Write-Host "NO_BUILD=YES"
Write-Host "NO_UPLOAD=YES"
Write-Host "SNAPSHOT_SOURCE=FROZEN_RUNNER_EXACT_FINAL"

if ($effectiveHz -ne $targetHz) { throw "G3_DIAG_SPI_FREQUENCY_MISMATCH" }
if ($effectiveChunks -ne $ExpectedChunks) { throw "G3_DIAG_CHUNKS_MISMATCH" }

$dirty = @(Get-G2TrackedDirtyPaths)
$expectedDirty = @($script:G2SpiHeaderRelative, $script:G2RawFirmwareRelative) | Sort-Object
Write-Host "TRACKED_DIRTY_COUNT=$($dirty.Count)"
$dirty | ForEach-Object { Write-Host "DIRTY=$_" }
if ($dirty.Count -ne 2) { throw "G3_DIAG_DIRTY_COUNT_INVALID" }
for ($i = 0; $i -lt 2; ++$i) { if ($dirty[$i] -ne $expectedDirty[$i]) { throw "G3_DIAG_DIRTY_PATH_INVALID=$($dirty[$i])" } }

$staged = @(& git -C $script:G2RepoRoot diff --cached --name-only)
if ($LASTEXITCODE -ne 0 -or $staged.Count -ne 0) { throw "G3_DIAG_INDEX_NOT_CLEAN" }
Write-Host "STAGED_COUNT=0"

Assert-G2ProtectedArtifacts
$firmwareHash = Get-G2Sha256 $script:G2RawFirmwareRelative
$runnerHash = Get-G2Sha256 $script:G2RawRunnerRelative
Write-Host "G3_FIRMWARE_SHA256=$firmwareHash"
Write-Host "RAW_RUNNER_SHA256=$runnerHash"
if ($firmwareHash -ne $expectedFirmwareSha256) { throw "G3_DIAG_FIRMWARE_HASH_MISMATCH" }
if ($runnerHash -ne $script:G2RawRunnerSha256) { throw "G3_DIAG_RUNNER_HASH_MISMATCH" }

$pythonCommand = Get-Command python.exe -ErrorAction SilentlyContinue
if ($null -eq $pythonCommand) { $pythonCommand = Get-Command python -ErrorAction SilentlyContinue }
if ($null -eq $pythonCommand) { throw "G3_DIAG_PYTHON_NOT_FOUND" }
$pythonExe = $pythonCommand.Source
$bridgePath = Join-Path $PSScriptRoot "g2_runner_snapshot_bridge.py"
if (-not (Test-Path -LiteralPath $bridgePath)) { throw "G3_DIAG_BRIDGE_NOT_FOUND" }

$durationText = $DurationSeconds.ToString([System.Globalization.CultureInfo]::InvariantCulture)
$timestamp = Get-Date -Format "yyyyMMdd_HHmmss"
$tempRoot = Join-Path $env:TEMP ("jwplc_g3_{0}mhz_{1}chunks_transition_diag_{2}" -f $MHz, $ExpectedChunks, $timestamp)
New-Item -ItemType Directory -Force -Path $tempRoot | Out-Null
Write-Host "TEMP_ROOT=$tempRoot"

$tcpRxSignatureCount = 0
$tcpTxSignatureCount = 0
$tcpRxOver10ms = 0
$tcpTxOver10ms = 0
$tcpRxMaxOverall = 0L
$tcpTxMaxOverall = 0L

function Invoke-DiagMode {
    param([int]$Pair, [string]$CliMode, [string]$Key)

    $logPath = Join-Path $tempRoot ("pair{0:D2}_{1}.log" -f $Pair, $Key.ToLowerInvariant())
    $args = @(
        $bridgePath,
        "--host", $DutIp,
        "--serial", $SerialPort,
        "--duration", $durationText,
        "--mode", $CliMode,
        "--tcp-chunk", "4096",
        "--udp-payload", "1472"
    )

    $exitCode = Invoke-NativeToLog -FilePath $pythonExe -Arguments $args -LogPath $logPath
    if ($exitCode -ne 0) {
        Get-Content -LiteralPath $logPath -Tail 100 | ForEach-Object { Write-Host $_ }
        throw "G3_DIAG_RUNNER_FAILED_${Key}_PAIR_$Pair"
    }

    $text = [System.IO.File]::ReadAllText($logPath)
    if ((Get-LogValue -Text $text -Key "RAW_BENCH_FUNCTIONAL_PASS") -ne "YES") { throw "G3_DIAG_FUNCTIONAL_FAIL_${Key}_PAIR_$Pair" }
    if ((Get-LogValue -Text $text -Key "G2_FINAL_SNAPSHOT_PRESENT") -ne "YES") { throw "G3_DIAG_SNAPSHOT_MISSING_${Key}_PAIR_$Pair" }

    foreach ($errorKey in @("TRANSPORT_ERRORS", "UDP_SPI_LOCK_ERRORS", "TCP_SPI_LOCK_ERRORS")) {
        $v = Get-LogInt64 -Text $text -Key ("G2_FINAL_{0}" -f $errorKey)
        if ($v -ne 0) { throw "G3_DIAG_${errorKey}_${Key}_PAIR_${Pair}=$v" }
    }

    $holdAvgUs = Get-LogInt64 -Text $text -Key "G2_FINAL_TCP_SPI_HOLD_AVG_US"
    $holdMaxUs = Get-LogInt64 -Text $text -Key "G2_FINAL_TCP_SPI_HOLD_MAX_US"
    $loopGapMaxUs = Get-LogInt64 -Text $text -Key "G2_FINAL_LOOP_GAP_MAX_US"
    $pcMbps = Get-LogValue -Text $text -Key ("SUMMARY_{0}_PC_MBPS" -f $Key)
    $signature = ($holdMaxUs -ge $signatureMinUs -and $holdMaxUs -le $signatureMaxUs)
    $over10ms = ($holdMaxUs -gt 10000)

    Write-Host ("PAIR={0} MODE={1} PC_MBPS={2} HOLD_AVG_US={3} HOLD_MAX_US={4} LOOP_GAP_MAX_US={5} ONE_SECOND_SIGNATURE={6} OVER_10MS={7}" -f $Pair, $Key, $pcMbps, $holdAvgUs, $holdMaxUs, $loopGapMaxUs, $(if ($signature) {"YES"} else {"NO"}), $(if ($over10ms) {"YES"} else {"NO"}))

    return [pscustomobject]@{ HoldMaxUs=$holdMaxUs; Signature=$signature; Over10ms=$over10ms }
}

Write-Host ""
Write-Host "=== REPEATED TCP_RX -> TCP_TX PAIRS ==="
for ($pair = 1; $pair -le $Pairs; ++$pair) {
    $rx = Invoke-DiagMode -Pair $pair -CliMode "tcp-rx" -Key "TCP_RX"
    if ($rx.HoldMaxUs -gt $tcpRxMaxOverall) { $tcpRxMaxOverall = $rx.HoldMaxUs }
    if ($rx.Signature) { ++$tcpRxSignatureCount }
    if ($rx.Over10ms) { ++$tcpRxOver10ms }

    Start-Sleep -Milliseconds 150

    $tx = Invoke-DiagMode -Pair $pair -CliMode "tcp-tx" -Key "TCP_TX"
    if ($tx.HoldMaxUs -gt $tcpTxMaxOverall) { $tcpTxMaxOverall = $tx.HoldMaxUs }
    if ($tx.Signature) { ++$tcpTxSignatureCount }
    if ($tx.Over10ms) { ++$tcpTxOver10ms }

    Start-Sleep -Milliseconds 150
}

Write-Host ""
Write-Host "=== DIAGNOSTIC SUMMARY ==="
Write-Host "PAIRS_COMPLETED=$Pairs"
Write-Host "TCP_RX_HOLD_MAX_US_OVERALL=$tcpRxMaxOverall"
Write-Host "TCP_TX_HOLD_MAX_US_OVERALL=$tcpTxMaxOverall"
Write-Host "TCP_RX_ONE_SECOND_SIGNATURE_COUNT=$tcpRxSignatureCount"
Write-Host "TCP_TX_ONE_SECOND_SIGNATURE_COUNT=$tcpTxSignatureCount"
Write-Host "TCP_RX_OVER_10MS_COUNT=$tcpRxOver10ms"
Write-Host "TCP_TX_OVER_10MS_COUNT=$tcpTxOver10ms"

Write-Host ""
Write-Host "OBSERVACION FISICA REQUERIDA: mira la TFT durante el diagnostico."
$visualAnswer = ""
while ($visualAnswer -notin @("S", "N")) {
    $visualAnswer = (Read-Host 'Aparecio el diagnostico visual "SPI" durante estas transiciones? (S/N)').Trim().ToUpperInvariant()
}
$visualEvents = $(if ($visualAnswer -eq "S") { 1 } else { 0 })
Write-Host "VISUAL_SPI_EVENTS=$visualEvents"

Assert-G2ProtectedArtifacts
if ((Get-G2Sha256 $script:G2RawFirmwareRelative) -ne $expectedFirmwareSha256) { throw "G3_DIAG_FIRMWARE_CHANGED" }
if ((Get-G2Sha256 $script:G2RawRunnerRelative) -ne $script:G2RawRunnerSha256) { throw "G3_DIAG_RUNNER_CHANGED" }

if (($tcpRxSignatureCount + $tcpTxSignatureCount) -gt 0) {
    Write-Host "G3_STOP_TIMEOUT_SIGNATURE=REPRODUCED"
} else {
    Write-Host "G3_STOP_TIMEOUT_SIGNATURE=NOT_REPRODUCED"
}
Write-Host "G3_TCP_RX_TX_TRANSITION_DIAG=COMPLETE"
