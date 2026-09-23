param(
    [string]$SerialPort = "COM14"
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"
. (Join-Path $PSScriptRoot "common.ps1")

function Invoke-NB3NativeToLog {
    param(
        [Parameter(Mandatory = $true)][string]$FilePath,
        [Parameter(Mandatory = $true)][string[]]$Arguments,
        [Parameter(Mandatory = $true)][string]$LogPath
    )

    $previousPreference = $ErrorActionPreference
    try {
        $ErrorActionPreference = "Continue"
        & $FilePath @Arguments *> $LogPath
        return [int]$LASTEXITCODE
    }
    finally {
        $ErrorActionPreference = $previousPreference
    }
}

function Get-LogValue {
    param(
        [Parameter(Mandatory = $true)][string]$Text,
        [Parameter(Mandatory = $true)][string]$Key
    )

    $pattern = "(?m)^" + [regex]::Escape($Key) + "=(.*)\r?$"
    $matches = @([regex]::Matches($Text, $pattern))

    if ($matches.Count -ne 1) {
        throw "A14_NB3F2_KEY_COUNT_$($Key)=$($matches.Count)"
    }

    return $matches[0].Groups[1].Value.Trim()
}

function Get-LogInt64 {
    param(
        [Parameter(Mandatory = $true)][string]$Text,
        [Parameter(Mandatory = $true)][string]$Key
    )

    return [int64]::Parse(
        (Get-LogValue -Text $Text -Key $Key),
        [System.Globalization.CultureInfo]::InvariantCulture
    )
}

Write-Host "============================================================"
Write-Host " A14 NB3-F2 - LEGACY TCP SEND NORMAL + BACKPRESSURE"
Write-Host "============================================================"

Assert-G2Branch

Write-Host "HEAD=$(Get-G2Head)"
Write-Host "EFFECTIVE_SPI_HZ=$(Get-G2SpiHz)"
Write-Host "SERIAL_PORT=$SerialPort"
Write-Host "WRITE_TIMEOUT_MS=250"
Write-Host "NORMAL_WRITE_HOLD_LIMIT_US=50000"
Write-Host "BACKPRESSURE_TIMEOUT_HOLD_MIN_US=150000"
Write-Host "BACKPRESSURE_TIMEOUT_HOLD_MAX_US=450000"
Write-Host "LEGACY_BACKPRESSURE_LOOP_GAP_GATE=NOT_APPLICABLE_EXPECTED_BLOCKING_API"

if ((Get-G2SpiHz) -ne 26000000) {
    throw "A14_NB3F2_SPI_FREQUENCY_MISMATCH"
}

$expectedHashes = [ordered]@{
    "JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/Dns.cpp" = "5D67E2FE8F2333A9A8AAD2A290A26DBAE92761DF11661C4305F8EC9D09604A60"
    "JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/Dns.h" = "B92718E94D549D60A7F2E648DAAD2A18DF4FAB94936BFEB279025911949AAFED"
    "JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/EthernetClient.cpp" = "6ADD2904893DA8C3A61C19FC91BF146C40038834458BC9F3084339462C9A9770"
    "JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/EthernetUdp.cpp" = "F5AE79FA9E9642A670D0AF23E029621711D2DD857B776A5208CF2866EDB496E5"
    "JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/jwplc_ethernet_async_tx.cpp" = "CC8EEE779FC932C5A8FA0175ABBF0421AA8C95279B4260FADAC1276E7BD74E1C"
    "JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/jwplc_ethernet_async_tx.h" = "C1C5615DFF167F3572FBEA85CFFE1A7F5051732E2BD446387F1025A1D981FB47"
    "JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/JWPLC_W5x00_Ethernet.h" = "7A5923105CFEEF07CD4BACEB72854397B9E9389772F97DF6B9DF3AB5A4D0F15A"
    "JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/socket.cpp" = "6E55F494F5E43B6BCF327F40C268AB6FF8F739331C96C328E9A0BEC0DA38654A"
    "JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/utility/w5100.cpp" = "9F94AAC1BB25966C18EDFD6A5C5D5908A9DBD4CF11B6E9BC099E10B28CEF272F"
    "JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/utility/w5100.h" = "9A833532C44E0CFCD66429A764E8BDBF62A8871838B0355565BC0A63043455FC"
    "tools/modbus-tcp-benchmark/firmware/eth14_raw_transport_server/eth14_raw_transport_server.ino" = "9A0C6CF27E44A61DC97686FB1A769EBBBB34D036CDF8D0CA0EEAB597E699AB6B"
}

$expectedDirty = @($expectedHashes.Keys) | Sort-Object
$dirty = @(Get-G2TrackedDirtyPaths)

Write-Host "TRACKED_DIRTY_COUNT=$($dirty.Count)"
$dirty | ForEach-Object { Write-Host "DIRTY=$_" }

if ($dirty.Count -ne $expectedDirty.Count) {
    throw "A14_NB3F2_DIRTY_COUNT_INVALID=$($dirty.Count)"
}

for ($i = 0; $i -lt $expectedDirty.Count; ++$i) {
    if ($dirty[$i] -ne $expectedDirty[$i]) {
        throw "A14_NB3F2_DIRTY_PATH_INVALID=$($dirty[$i])"
    }
}

$staged = @(& git -C $script:G2RepoRoot diff --cached --name-only)
if ($LASTEXITCODE -ne 0 -or $staged.Count -ne 0) {
    throw "A14_NB3F2_INDEX_NOT_CLEAN"
}
Write-Host "STAGED_COUNT=0"

Assert-G2ProtectedArtifacts

foreach ($entry in $expectedHashes.GetEnumerator()) {
    $actual = Get-G2Sha256 $entry.Key
    Write-Host "CANDIDATE_SHA256=$($entry.Key)=$actual"

    if ($actual -ne $entry.Value) {
        throw "A14_NB3F2_CANDIDATE_HASH_MISMATCH=$($entry.Key)"
    }
}

$arduinoCli = "C:\Program Files\Arduino PLC IDE Tools\arduino-cli.exe"
if (-not (Test-Path -LiteralPath $arduinoCli)) {
    throw "A14_NB3F2_ARDUINO_CLI_NOT_FOUND=$arduinoCli"
}

$pythonCommand = Get-Command python.exe -ErrorAction SilentlyContinue
if ($null -eq $pythonCommand) {
    $pythonCommand = Get-Command python -ErrorAction Stop
}
$pythonExe = $pythonCommand.Source

$fqbn = "jwplc_local:esp32:jwplcbasic"
$librariesRoot = Get-G2Path "JWPLC/2.1.0/libraries"
$expectedEthernetLibrary = Join-Path $librariesRoot "JWPLC_Ethernet"

$probeRelative = "tools/modbus-tcp-benchmark/firmware/a14_nb3_legacy_tcp_send_probe/a14_nb3_legacy_tcp_send_probe.ino"
$clientRelative = "tools/modbus-tcp-benchmark/gates/a14_nb3_legacy_tcp_send_physical_client.py"

$probePath = Get-G2Path $probeRelative
$probeDir = Split-Path -Parent $probePath
$clientPath = Get-G2Path $clientRelative

if (-not (Test-Path -LiteralPath $probePath)) {
    throw "A14_NB3F2_PROBE_NOT_FOUND"
}

if (-not (Test-Path -LiteralPath $clientPath)) {
    throw "A14_NB3F2_CLIENT_NOT_FOUND"
}

$timestamp = Get-Date -Format "yyyyMMdd_HHmmss"
$tempRoot = Join-Path $env:TEMP ("jwplc_a14_nb3f2_tcp_send_{0}" -f $timestamp)
$buildPath = Join-Path $tempRoot "build"
$compileLog = Join-Path $tempRoot "compile.log"
$uploadLog = Join-Path $tempRoot "upload.log"
$clientLog = Join-Path $tempRoot "client.log"
$clientSyntaxLog = Join-Path $tempRoot "client_syntax.log"

New-Item -ItemType Directory -Force -Path $buildPath | Out-Null

Write-Host "ARDUINO_CLI=$arduinoCli"
Write-Host "PYTHON=$pythonExe"
Write-Host "FQBN=$fqbn"
Write-Host "PROBE=$probePath"
Write-Host "CLIENT=$clientPath"
Write-Host "TEMP_ROOT=$tempRoot"
Write-Host "SOURCE_MUTATION=NO"

Write-Host ""
Write-Host "=== PYTHON CLIENT SYNTAX PREFLIGHT ==="

$clientSyntaxExit = Invoke-NB3NativeToLog -FilePath $pythonExe -Arguments @(
    "-m",
    "py_compile",
    $clientPath
) -LogPath $clientSyntaxLog

Write-Host "CLIENT_SYNTAX_EXIT=$clientSyntaxExit"
Write-Host "CLIENT_SYNTAX_LOG=$clientSyntaxLog"

if ($clientSyntaxExit -ne 0) {
    Get-Content -LiteralPath $clientSyntaxLog -Tail 100 | ForEach-Object { Write-Host $_ }
    throw "A14_NB3F2_CLIENT_SYNTAX_FAILED"
}
Write-Host "CLIENT_SYNTAX=PASS"

Write-Host ""
Write-Host "=== COMPILE LEGACY TCP PHYSICAL PROBE ==="

$compileArgs = @(
    "compile",
    "--fqbn", $fqbn,
    "--build-path", $buildPath,
    "--libraries", $librariesRoot,
    $probeDir
)

$compileExit = Invoke-NB3NativeToLog -FilePath $arduinoCli -Arguments $compileArgs -LogPath $compileLog

Write-Host "COMPILE_EXIT=$compileExit"
Write-Host "COMPILE_LOG=$compileLog"

if ($compileExit -ne 0) {
    Invoke-G2CompileFinishedSound -Success $false
    Get-Content -LiteralPath $compileLog -Tail 140 | ForEach-Object { Write-Host $_ }
    throw "A14_NB3F2_COMPILE_FAILED"
}

$compileText = [System.IO.File]::ReadAllText($compileLog)
$repoEthernetUsed = (
    $compileText.IndexOf(
        $expectedEthernetLibrary,
        [System.StringComparison]::OrdinalIgnoreCase
    ) -ge 0
)

Write-Host "REPO_ETHERNET_LIBRARY_USED=$repoEthernetUsed"
if (-not $repoEthernetUsed) {
    throw "A14_NB3F2_WRONG_ETHERNET_LIBRARY"
}

$binCount = @(Get-ChildItem -LiteralPath $buildPath -Recurse -File -Filter "*.bin").Count
Write-Host "BIN_COUNT=$binCount"

if ($binCount -lt 1) {
    Invoke-G2CompileFinishedSound -Success $false
    throw "A14_NB3F2_NO_BIN"
}

Invoke-G2CompileFinishedSound -Success $true
Write-Host "COMPILE_FINISHED_SOUND=PLAYED"

Write-Host ""
Write-Host "=== UPLOAD LEGACY TCP PHYSICAL PROBE ==="

$uploadArgs = @(
    "upload",
    "--fqbn", $fqbn,
    "--port", $SerialPort,
    "--input-dir", $buildPath,
    $probeDir
)

$uploadExit = Invoke-NB3NativeToLog -FilePath $arduinoCli -Arguments $uploadArgs -LogPath $uploadLog

Write-Host "UPLOAD_EXIT=$uploadExit"
Write-Host "UPLOAD_LOG=$uploadLog"

if ($uploadExit -ne 0) {
    Get-Content -LiteralPath $uploadLog -Tail 140 | ForEach-Object { Write-Host $_ }
    throw "A14_NB3F2_UPLOAD_FAILED"
}

Write-Host ""
Write-Host "=== RUN LEGACY TCP NORMAL + BACKPRESSURE SCENARIO ==="

Start-Sleep -Milliseconds 300

$clientArgs = @(
    $clientPath,
    "--serial", $SerialPort,
    "--timeout-s", "12"
)

$clientExit = Invoke-NB3NativeToLog -FilePath $pythonExe -Arguments $clientArgs -LogPath $clientLog

Write-Host "CLIENT_EXIT=$clientExit"
Write-Host "CLIENT_LOG=$clientLog"
Get-Content -LiteralPath $clientLog | ForEach-Object { Write-Host $_ }

if ($clientExit -ne 0) {
    throw "A14_NB3F2_CLIENT_FAILED=$clientExit"
}

$logText = [System.IO.File]::ReadAllText($clientLog)

$resultCode = Get-LogInt64 -Text $logText -Key "RESULT_CODE"
$probeFailed = Get-LogValue -Text $logText -Key "PROBE_FAILED"
$dutIp = Get-LogValue -Text $logText -Key "DUT_IP_EFFECTIVE"
$normalConnected = Get-LogValue -Text $logText -Key "NORMAL_CONNECTED"
$normalWriteCalls = Get-LogInt64 -Text $logText -Key "NORMAL_WRITE_CALLS"
$normalBytes = Get-LogInt64 -Text $logText -Key "NORMAL_BYTES"
$normalHoldMaxUs = Get-LogInt64 -Text $logText -Key "NORMAL_WRITE_HOLD_MAX_US"
$bpConnected = Get-LogValue -Text $logText -Key "BACKPRESSURE_CONNECTED"
$bpSuccessfulWrites = Get-LogInt64 -Text $logText -Key "BACKPRESSURE_SUCCESSFUL_WRITES"
$bpBytes = Get-LogInt64 -Text $logText -Key "BACKPRESSURE_BYTES"
$bpSuccessfulHoldMaxUs = Get-LogInt64 -Text $logText -Key "BACKPRESSURE_SUCCESSFUL_HOLD_MAX_US"
$bpTimeoutObserved = Get-LogValue -Text $logText -Key "BACKPRESSURE_TIMEOUT_OBSERVED"
$bpTimeoutReturn = Get-LogInt64 -Text $logText -Key "BACKPRESSURE_TIMEOUT_WRITE_RETURN"
$bpTimeoutHoldUs = Get-LogInt64 -Text $logText -Key "BACKPRESSURE_TIMEOUT_HOLD_US"
$bpConnectedAfter = Get-LogValue -Text $logText -Key "BACKPRESSURE_CONNECTED_AFTER_TIMEOUT"
$writeTimeoutMs = Get-LogInt64 -Text $logText -Key "WRITE_TIMEOUT_MS"
$spiLockErrors = Get-LogInt64 -Text $logText -Key "SPI_LOCK_ERRORS"
$runDurationMs = Get-LogInt64 -Text $logText -Key "RUN_DURATION_MS"

$pcNormalAccepted = Get-LogValue -Text $logText -Key "PC_NORMAL_ACCEPTED"
$pcNormalBytes = Get-LogInt64 -Text $logText -Key "PC_NORMAL_BYTES"
$pcNormalPattern = Get-LogValue -Text $logText -Key "PC_NORMAL_PATTERN"
$pcBpAccepted = Get-LogValue -Text $logText -Key "PC_BACKPRESSURE_ACCEPTED"
$pcBpRcvbuf = Get-LogInt64 -Text $logText -Key "PC_BACKPRESSURE_RCVBUF"
$clientPass = Get-LogValue -Text $logText -Key "NB3_F2_CLIENT_PASS"

Write-Host ""
Write-Host "=== NB3-F2 PHYSICAL SUMMARY ==="
Write-Host "DUT_IP_EFFECTIVE=$dutIp"
Write-Host "NORMAL_CONNECTED=$normalConnected"
Write-Host "NORMAL_WRITE_CALLS=$normalWriteCalls"
Write-Host "NORMAL_BYTES=$normalBytes"
Write-Host "NORMAL_WRITE_HOLD_MAX_US=$normalHoldMaxUs"
Write-Host "PC_NORMAL_ACCEPTED=$pcNormalAccepted"
Write-Host "PC_NORMAL_BYTES=$pcNormalBytes"
Write-Host "PC_NORMAL_PATTERN=$pcNormalPattern"
Write-Host "BACKPRESSURE_CONNECTED=$bpConnected"
Write-Host "BACKPRESSURE_SUCCESSFUL_WRITES=$bpSuccessfulWrites"
Write-Host "BACKPRESSURE_BYTES=$bpBytes"
Write-Host "BACKPRESSURE_SUCCESSFUL_HOLD_MAX_US=$bpSuccessfulHoldMaxUs"
Write-Host "BACKPRESSURE_TIMEOUT_OBSERVED=$bpTimeoutObserved"
Write-Host "BACKPRESSURE_TIMEOUT_WRITE_RETURN=$bpTimeoutReturn"
Write-Host "BACKPRESSURE_TIMEOUT_HOLD_US=$bpTimeoutHoldUs"
Write-Host "BACKPRESSURE_CONNECTED_AFTER_TIMEOUT=$bpConnectedAfter"
Write-Host "PC_BACKPRESSURE_ACCEPTED=$pcBpAccepted"
Write-Host "PC_BACKPRESSURE_RCVBUF=$pcBpRcvbuf"
Write-Host "WRITE_TIMEOUT_MS=$writeTimeoutMs"
Write-Host "SPI_LOCK_ERRORS=$spiLockErrors"
Write-Host "RUN_DURATION_MS=$runDurationMs"
Write-Host "LEGACY_BACKPRESSURE_LOOP_GAP_GATE=NOT_APPLICABLE_EXPECTED_BLOCKING_API"

if ($resultCode -ne 1 -or $probeFailed -ne "NO") {
    throw "A14_NB3F2_PROBE_FAIL"
}

if ($clientPass -ne "YES") {
    throw "A14_NB3F2_CLIENT_PASS_MISSING"
}

if ($normalConnected -ne "YES") {
    throw "A14_NB3F2_NORMAL_CONNECT_FAIL"
}

if ($normalWriteCalls -ne 4 -or $normalBytes -ne 4096) {
    throw "A14_NB3F2_NORMAL_WRITE_INVALID calls=$normalWriteCalls bytes=$normalBytes"
}

if ($normalHoldMaxUs -gt 50000) {
    throw "A14_NB3F2_NORMAL_WRITE_HOLD_HIGH=$normalHoldMaxUs"
}

if (
    $pcNormalAccepted -ne "YES" -or
    $pcNormalBytes -ne 4096 -or
    $pcNormalPattern -ne "PASS")
{
    throw "A14_NB3F2_PC_NORMAL_VALIDATION_FAIL"
}

if ($bpConnected -ne "YES" -or $pcBpAccepted -ne "YES") {
    throw "A14_NB3F2_BACKPRESSURE_CONNECT_FAIL"
}

if ($bpSuccessfulWrites -lt 1 -or $bpBytes -lt 1024) {
    throw "A14_NB3F2_BACKPRESSURE_NOT_REACHED_AFTER_PROGRESS"
}

if ($bpTimeoutObserved -ne "YES" -or $bpTimeoutReturn -ne 0) {
    throw "A14_NB3F2_TIMEOUT_NOT_OBSERVED"
}

if ($writeTimeoutMs -ne 250) {
    throw "A14_NB3F2_WRITE_TIMEOUT_CONFIG_INVALID=$writeTimeoutMs"
}

if ($bpTimeoutHoldUs -lt 150000 -or $bpTimeoutHoldUs -gt 450000) {
    throw "A14_NB3F2_TIMEOUT_HOLD_OUT_OF_RANGE=$bpTimeoutHoldUs"
}

if ($bpConnectedAfter -ne "YES") {
    throw "A14_NB3F2_CONNECTION_NOT_ALIVE_AFTER_TIMEOUT"
}

if ($pcBpRcvbuf -le 0) {
    throw "A14_NB3F2_PC_RCVBUF_INVALID=$pcBpRcvbuf"
}

if ($spiLockErrors -ne 0) {
    throw "A14_NB3F2_SPI_LOCK_ERRORS=$spiLockErrors"
}

if ($runDurationMs -le 0 -or $runDurationMs -gt 10000) {
    throw "A14_NB3F2_RUN_DURATION_INVALID=$runDurationMs"
}

Write-Host ""
Write-Host "OBSERVACION FISICA REQUERIDA: mira la TFT durante NB3-F2."
$answer = ""

while ($answer -notin @("S", "N")) {
    $answer = (Read-Host 'Aparecio el diagnostico visual "SPI" durante NB3-F2? (S/N)').Trim().ToUpperInvariant()
}

$visualEvents = if ($answer -eq "S") { 1 } else { 0 }
Write-Host "VISUAL_SPI_EVENTS=$visualEvents"

if ($visualEvents -ne 0) {
    throw "A14_NB3F2_VISUAL_SPI_FAIL"
}

Assert-G2ProtectedArtifacts

$dirtyFinal = @(Get-G2TrackedDirtyPaths)
$stagedFinal = @(& git -C $script:G2RepoRoot diff --cached --name-only)

Write-Host "TRACKED_DIRTY_COUNT_FINAL=$($dirtyFinal.Count)"
$dirtyFinal | ForEach-Object { Write-Host "DIRTY_FINAL=$_" }
Write-Host "STAGED_COUNT_FINAL=$($stagedFinal.Count)"

if ($dirtyFinal.Count -ne $expectedDirty.Count -or $stagedFinal.Count -ne 0) {
    throw "A14_NB3F2_FINAL_WORKTREE_INVALID"
}

for ($i = 0; $i -lt $expectedDirty.Count; ++$i) {
    if ($dirtyFinal[$i] -ne $expectedDirty[$i]) {
        throw "A14_NB3F2_FINAL_DIRTY_PATH_INVALID=$($dirtyFinal[$i])"
    }
}

Write-Host "NB3_F2_LEGACY_TCP_NORMAL_WRITE=PASS"
Write-Host "NB3_F2_LEGACY_TCP_NORMAL_PAYLOAD=PASS"
Write-Host "NB3_F2_BACKPRESSURE_CREATED=PASS"
Write-Host "NB3_F2_BACKPRESSURE_PROGRESS_BEFORE_TIMEOUT=PASS"
Write-Host "NB3_F2_WRITE_TIMEOUT_250MS=PASS"
Write-Host "NB3_F2_WRITE_RETURNED_ZERO=PASS"
Write-Host "NB3_F2_CONNECTION_ALIVE_AFTER_TIMEOUT=PASS"
Write-Host "NB3_F2_SPI_LOCK_ERRORS=ZERO"
Write-Host "NB3_F2_VISUAL_SPI=PASS"
Write-Host "NB3_TCP_LEGACY_SEND_BOUNDS=PHYSICAL_CLOSED_PASS"
Write-Host "A14_NB3_F2_LEGACY_TCP_SEND_PHYSICAL=PASS"
