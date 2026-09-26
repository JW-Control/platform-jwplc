param(
    [int]$MHz = 26,
    [int]$ExpectedChunks = 8,
    [string]$SerialPort = "COM14",
    [string]$DutIp = "192.168.0.31",
    [double]$DurationSeconds = 2.0,
    [int]$Pairs = 20
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"
. (Join-Path $PSScriptRoot "common.ps1")

$expectedHeaderSha256 = "3E11094D68873D81F264F8AD97C1D55867F5399E28613061BC38484CD7DE614B"
$expectedCppSha256 = "93B71C92E298053425FF03D750632A30102BE743E1C8B1FDAB800214EABED4B2"
$expectedFirmwareSha256 = "9A0C6CF27E44A61DC97686FB1A769EBBBB34D036CDF8D0CA0EEAB597E699AB6B"
$headerRelative = "JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/JWPLC_W5x00_Ethernet.h"
$cppRelative = "JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/EthernetClient.cpp"
$signatureMinUs = 900000L
$signatureMaxUs = 1100000L
$hardHoldUs = 10000L

function Invoke-NativeToLog {
    param(
        [Parameter(Mandatory = $true)][string]$FilePath,
        [Parameter(Mandatory = $true)][string[]]$Arguments,
        [Parameter(Mandatory = $true)][string]$LogPath
    )
    & $FilePath @Arguments *> $LogPath
    return [int]$LASTEXITCODE
}

function Get-LogValue {
    param([string]$Text, [string]$Key)
    $pattern = "(?m)^" + [regex]::Escape($Key) + "=(.*)\r?$"
    $matches = @([regex]::Matches($Text, $pattern))
    if ($matches.Count -ne 1) {
        throw "A14_NB1C2_LOG_KEY_COUNT_${Key}=$($matches.Count)"
    }
    return $matches[0].Groups[1].Value.Trim()
}

function Get-LogInt64 {
    param([string]$Text, [string]$Key)
    return [int64]::Parse(
        (Get-LogValue -Text $Text -Key $Key),
        [System.Globalization.CultureInfo]::InvariantCulture
    )
}

function Get-ChunkCount {
    $firmwarePath = Get-G2Path $script:G2RawFirmwareRelative
    $text = [System.IO.File]::ReadAllText($firmwarePath)
    $pattern = '(?m)^[ \t]*static constexpr uint8_t TCP_RX_MAX_CHUNKS_PER_LOCK = (\d+);[ \t]*\r?$'
    $matches = @([regex]::Matches($text, $pattern))
    if ($matches.Count -ne 1) {
        throw "A14_NB1C2_CHUNK_DECLARATION_COUNT=$($matches.Count)"
    }
    return [int]$matches[0].Groups[1].Value
}

Write-Host "============================================================"
Write-Host " A14 NB1-C2 - ASYNC TCP STOP PHYSICAL DIAGNOSTIC"
Write-Host "============================================================"

Assert-G2Branch
Assert-G2AllowedMHz -MHz $MHz

if ($DurationSeconds -le 0 -or $Pairs -lt 1) {
    throw "A14_NB1C2_INVALID_ARGUMENTS"
}

$targetHz = [int64]$MHz * 1000000
$effectiveHz = Get-G2SpiHz
$effectiveChunks = Get-ChunkCount
$head = Get-G2Head

Write-Host "HEAD=$head"
Write-Host "TARGET_MHZ=$MHz"
Write-Host "EFFECTIVE_SPI_HZ=$effectiveHz"
Write-Host "EXPECTED_CHUNKS=$ExpectedChunks"
Write-Host "EFFECTIVE_CHUNKS=$effectiveChunks"
Write-Host "CONNECTION_TIMEOUT_MS=1000_UNCHANGED"
Write-Host "TCP_STOP_MODE=ASYNC"
Write-Host "PAIRS=$Pairs"
Write-Host "DURATION_S=$DurationSeconds"
Write-Host "ONE_SECOND_SIGNATURE_MIN_US=$signatureMinUs"
Write-Host "ONE_SECOND_SIGNATURE_MAX_US=$signatureMaxUs"
Write-Host "HARD_HOLD_MAX_US=$hardHoldUs"
Write-Host "SNAPSHOT_SOURCE=FROZEN_RUNNER_EXACT_FINAL"

if ($effectiveHz -ne $targetHz) { throw "A14_NB1C2_SPI_FREQUENCY_MISMATCH" }
if ($effectiveChunks -ne $ExpectedChunks) { throw "A14_NB1C2_CHUNKS_MISMATCH" }

$expectedDirty = @(
    $headerRelative,
    $cppRelative,
    $script:G2SpiHeaderRelative,
    $script:G2RawFirmwareRelative
) | Sort-Object
$dirty = @(Get-G2TrackedDirtyPaths)
Write-Host "TRACKED_DIRTY_COUNT=$($dirty.Count)"
$dirty | ForEach-Object { Write-Host "DIRTY=$_" }
if ($dirty.Count -ne $expectedDirty.Count) {
    throw "A14_NB1C2_DIRTY_COUNT_INVALID=$($dirty.Count)"
}
for ($i = 0; $i -lt $expectedDirty.Count; ++$i) {
    if ($dirty[$i] -ne $expectedDirty[$i]) {
        throw "A14_NB1C2_DIRTY_PATH_INVALID=$($dirty[$i])"
    }
}

$staged = @(& git -C $script:G2RepoRoot diff --cached --name-only)
if ($LASTEXITCODE -ne 0 -or $staged.Count -ne 0) {
    throw "A14_NB1C2_INDEX_NOT_CLEAN"
}
Write-Host "STAGED_COUNT=0"

& git -C $script:G2RepoRoot diff --check
if ($LASTEXITCODE -ne 0) { throw "A14_NB1C2_DIFF_CHECK_FAILED" }

Assert-G2ProtectedArtifacts

$headerHash = Get-G2Sha256 $headerRelative
$cppHash = Get-G2Sha256 $cppRelative
$firmwareHash = Get-G2Sha256 $script:G2RawFirmwareRelative
$runnerHash = Get-G2Sha256 $script:G2RawRunnerRelative
Write-Host "HEADER_SHA256=$headerHash"
Write-Host "CPP_SHA256=$cppHash"
Write-Host "FIRMWARE_SHA256=$firmwareHash"
Write-Host "RAW_RUNNER_SHA256=$runnerHash"
if ($headerHash -ne $expectedHeaderSha256) { throw "A14_NB1C2_HEADER_HASH_MISMATCH" }
if ($cppHash -ne $expectedCppSha256) { throw "A14_NB1C2_CPP_HASH_MISMATCH" }
if ($firmwareHash -ne $expectedFirmwareSha256) { throw "A14_NB1C2_FIRMWARE_HASH_MISMATCH" }
if ($runnerHash -ne $script:G2RawRunnerSha256) { throw "A14_NB1C2_RUNNER_HASH_MISMATCH" }

$firmwarePath = Get-G2Path $script:G2RawFirmwareRelative
$firmwareText = [System.IO.File]::ReadAllText($firmwarePath)
$blockingStopCount = ([regex]::Matches($firmwareText, '(?m)^[ \t]*tcpClient\.stop\(\);[ \t]*\r?$')).Count
$beginStopCount = ([regex]::Matches($firmwareText, [regex]::Escape('tcpClient.beginStopAsync()'))).Count
$pollStopCount = ([regex]::Matches($firmwareText, [regex]::Escape('tcpClient.pollStopAsync()'))).Count
$inProgressCount = ([regex]::Matches($firmwareText, [regex]::Escape('tcpClient.stopAsyncInProgress()'))).Count
Write-Host "TCP_CLIENT_BLOCKING_STOP_COUNT=$blockingStopCount"
Write-Host "TCP_CLIENT_BEGIN_STOP_ASYNC_COUNT=$beginStopCount"
Write-Host "TCP_CLIENT_POLL_STOP_ASYNC_COUNT=$pollStopCount"
Write-Host "TCP_CLIENT_STOP_ASYNC_IN_PROGRESS_COUNT=$inProgressCount"
if ($blockingStopCount -ne 0 -or $beginStopCount -ne 2 -or $pollStopCount -ne 1 -or $inProgressCount -ne 1) {
    throw "A14_NB1C2_RAW_ASYNC_STOP_MARKERS_INVALID"
}

$arduinoCli = "C:\Program Files\Arduino PLC IDE Tools\arduino-cli.exe"
if (-not (Test-Path -LiteralPath $arduinoCli)) {
    throw "A14_NB1C2_ARDUINO_CLI_NOT_FOUND=$arduinoCli"
}
$pythonCommand = Get-Command python.exe -ErrorAction SilentlyContinue
if ($null -eq $pythonCommand) { $pythonCommand = Get-Command python -ErrorAction SilentlyContinue }
if ($null -eq $pythonCommand) { throw "A14_NB1C2_PYTHON_NOT_FOUND" }
$pythonExe = $pythonCommand.Source
$fqbn = "jwplc_local:esp32:jwplcbasic"
$sketchDir = Split-Path -Parent $firmwarePath
$librariesRoot = Get-G2Path "JWPLC/2.1.0/libraries"
$expectedEthernetLibrary = Join-Path $librariesRoot "JWPLC_Ethernet"
$bridgePath = Join-Path $PSScriptRoot "g2_runner_snapshot_bridge.py"
if (-not (Test-Path -LiteralPath $bridgePath)) { throw "A14_NB1C2_BRIDGE_NOT_FOUND" }

$timestamp = Get-Date -Format "yyyyMMdd_HHmmss"
$tempRoot = Join-Path $env:TEMP ("jwplc_a14_nb1c2_{0}mhz_{1}chunks_{2}" -f $MHz, $ExpectedChunks, $timestamp)
$buildPath = Join-Path $tempRoot "build"
$compileLog = Join-Path $tempRoot "compile.log"
$uploadLog = Join-Path $tempRoot "upload.log"
New-Item -ItemType Directory -Force -Path $buildPath | Out-Null

Write-Host "ARDUINO_CLI=$arduinoCli"
Write-Host "PYTHON=$pythonExe"
Write-Host "FQBN=$fqbn"
Write-Host "TEMP_ROOT=$tempRoot"

Write-Host ""
Write-Host "=== COMPILE ASYNC-STOP RAW FIRMWARE ==="
$compileArgs = @(
    "compile",
    "--fqbn", $fqbn,
    "--build-path", $buildPath,
    "--libraries", $librariesRoot,
    $sketchDir
)
$compileExit = Invoke-NativeToLog -FilePath $arduinoCli -Arguments $compileArgs -LogPath $compileLog
Write-Host "COMPILE_EXIT=$compileExit"
Write-Host "COMPILE_LOG=$compileLog"
if ($compileExit -ne 0) {
    Get-Content -LiteralPath $compileLog -Tail 100 | ForEach-Object { Write-Host $_ }
    throw "A14_NB1C2_COMPILE_FAILED"
}
$compileText = [System.IO.File]::ReadAllText($compileLog)
$ethernetLibraryUsed = ($compileText.IndexOf($expectedEthernetLibrary, [System.StringComparison]::OrdinalIgnoreCase) -ge 0)
Write-Host "REPO_ETHERNET_LIBRARY_USED=$ethernetLibraryUsed"
if (-not $ethernetLibraryUsed) { throw "A14_NB1C2_WRONG_ETHERNET_LIBRARY" }
$binCount = @(Get-ChildItem -LiteralPath $buildPath -Recurse -File -Filter "*.bin").Count
Write-Host "BIN_COUNT=$binCount"
if ($binCount -lt 1) { throw "A14_NB1C2_NO_BIN_OUTPUT" }

Write-Host ""
Write-Host "=== UPLOAD ASYNC-STOP RAW FIRMWARE ==="
$uploadArgs = @(
    "upload",
    "--fqbn", $fqbn,
    "--port", $SerialPort,
    "--input-dir", $buildPath,
    $sketchDir
)
$uploadExit = Invoke-NativeToLog -FilePath $arduinoCli -Arguments $uploadArgs -LogPath $uploadLog
Write-Host "UPLOAD_EXIT=$uploadExit"
Write-Host "UPLOAD_LOG=$uploadLog"
if ($uploadExit -ne 0) {
    Get-Content -LiteralPath $uploadLog -Tail 100 | ForEach-Object { Write-Host $_ }
    throw "A14_NB1C2_UPLOAD_FAILED"
}

$durationText = $DurationSeconds.ToString([System.Globalization.CultureInfo]::InvariantCulture)
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
        throw "A14_NB1C2_RUNNER_FAILED_${Key}_PAIR_$Pair"
    }

    $text = [System.IO.File]::ReadAllText($logPath)
    if ((Get-LogValue -Text $text -Key "RAW_BENCH_FUNCTIONAL_PASS") -ne "YES") {
        throw "A14_NB1C2_FUNCTIONAL_FAIL_${Key}_PAIR_$Pair"
    }
    if ((Get-LogValue -Text $text -Key "G2_FINAL_SNAPSHOT_PRESENT") -ne "YES") {
        throw "A14_NB1C2_SNAPSHOT_MISSING_${Key}_PAIR_$Pair"
    }
    foreach ($errorKey in @("TRANSPORT_ERRORS", "UDP_SPI_LOCK_ERRORS", "TCP_SPI_LOCK_ERRORS")) {
        $v = Get-LogInt64 -Text $text -Key ("G2_FINAL_{0}" -f $errorKey)
        if ($v -ne 0) { throw "A14_NB1C2_${errorKey}_${Key}_PAIR_${Pair}=$v" }
    }

    $holdAvgUs = Get-LogInt64 -Text $text -Key "G2_FINAL_TCP_SPI_HOLD_AVG_US"
    $holdMaxUs = Get-LogInt64 -Text $text -Key "G2_FINAL_TCP_SPI_HOLD_MAX_US"
    $loopGapMaxUs = Get-LogInt64 -Text $text -Key "G2_FINAL_LOOP_GAP_MAX_US"
    $pcMbps = Get-LogValue -Text $text -Key ("SUMMARY_{0}_PC_MBPS" -f $Key)
    $signature = ($holdMaxUs -ge $signatureMinUs -and $holdMaxUs -le $signatureMaxUs)
    $over10ms = ($holdMaxUs -gt $hardHoldUs)

    Write-Host ("PAIR={0} MODE={1} PC_MBPS={2} HOLD_AVG_US={3} HOLD_MAX_US={4} LOOP_GAP_MAX_US={5} ONE_SECOND_SIGNATURE={6} OVER_10MS={7}" -f $Pair, $Key, $pcMbps, $holdAvgUs, $holdMaxUs, $loopGapMaxUs, $(if ($signature) {"YES"} else {"NO"}), $(if ($over10ms) {"YES"} else {"NO"}))

    return [pscustomobject]@{
        HoldMaxUs = $holdMaxUs
        Signature = $signature
        Over10ms = $over10ms
    }
}

Write-Host ""
Write-Host "=== 20 TCP_RX -> TCP_TX PAIRS WITH ASYNC STOP ==="
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
Write-Host "=== NB1-C2 DIAGNOSTIC SUMMARY ==="
Write-Host "PAIRS_COMPLETED=$Pairs"
Write-Host "TCP_RX_HOLD_MAX_US_OVERALL=$tcpRxMaxOverall"
Write-Host "TCP_TX_HOLD_MAX_US_OVERALL=$tcpTxMaxOverall"
Write-Host "TCP_RX_ONE_SECOND_SIGNATURE_COUNT=$tcpRxSignatureCount"
Write-Host "TCP_TX_ONE_SECOND_SIGNATURE_COUNT=$tcpTxSignatureCount"
Write-Host "TCP_RX_OVER_10MS_COUNT=$tcpRxOver10ms"
Write-Host "TCP_TX_OVER_10MS_COUNT=$tcpTxOver10ms"

Write-Host ""
Write-Host ("OBSERVACION FISICA REQUERIDA: mira la TFT durante los {0} pares." -f $Pairs)
$visualAnswer = ""
while ($visualAnswer -notin @("S", "N")) {
    $visualAnswer = (Read-Host 'Aparecio el diagnostico visual "SPI" durante NB1-C2? (S/N)').Trim().ToUpperInvariant()
}
$visualEvents = $(if ($visualAnswer -eq "S") { 1 } else { 0 })
Write-Host "VISUAL_SPI_EVENTS=$visualEvents"

Assert-G2ProtectedArtifacts
if ((Get-G2Sha256 $headerRelative) -ne $expectedHeaderSha256) { throw "A14_NB1C2_HEADER_CHANGED" }
if ((Get-G2Sha256 $cppRelative) -ne $expectedCppSha256) { throw "A14_NB1C2_CPP_CHANGED" }
if ((Get-G2Sha256 $script:G2RawFirmwareRelative) -ne $expectedFirmwareSha256) { throw "A14_NB1C2_FIRMWARE_CHANGED" }
if ((Get-G2Sha256 $script:G2RawRunnerRelative) -ne $script:G2RawRunnerSha256) { throw "A14_NB1C2_RUNNER_CHANGED" }

$dirtyFinal = @(Get-G2TrackedDirtyPaths)
Write-Host "TRACKED_DIRTY_COUNT_FINAL=$($dirtyFinal.Count)"
$dirtyFinal | ForEach-Object { Write-Host "DIRTY_FINAL=$_" }
if ($dirtyFinal.Count -ne $expectedDirty.Count) { throw "A14_NB1C2_DIRTY_COUNT_FINAL_INVALID" }
for ($i = 0; $i -lt $expectedDirty.Count; ++$i) {
    if ($dirtyFinal[$i] -ne $expectedDirty[$i]) { throw "A14_NB1C2_DIRTY_PATH_FINAL_INVALID=$($dirtyFinal[$i])" }
}
$stagedFinal = @(& git -C $script:G2RepoRoot diff --cached --name-only)
Write-Host "STAGED_COUNT_FINAL=$($stagedFinal.Count)"
if ($LASTEXITCODE -ne 0 -or $stagedFinal.Count -ne 0) { throw "A14_NB1C2_INDEX_NOT_CLEAN_FINAL" }

$fail = $false
if (($tcpRxSignatureCount + $tcpTxSignatureCount) -gt 0) { $fail = $true }
if (($tcpRxOver10ms + $tcpTxOver10ms) -gt 0) { $fail = $true }
if ($visualEvents -ne 0) { $fail = $true }

if ($fail) {
    Write-Host "NB1_ASYNC_STOP_PHYSICAL=FAIL"
    throw "A14_NB1C2_PHYSICAL_DIAGNOSTIC_FAILED"
}

Write-Host "NB1_ONE_SECOND_SIGNATURE=ELIMINATED"
Write-Host "NB1_ASYNC_STOP_TIMEOUT_MS=1000_PRESERVED"
Write-Host "NB1_VISUAL_SPI=PASS"
Write-Host "A14_NB1_ASYNC_STOP_PHYSICAL_DIAG=PASS"
