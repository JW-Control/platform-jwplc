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
        throw "A14_NB3E2_KEY_COUNT_$($Key)=$($matches.Count)"
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
Write-Host " A14 NB3-E2 - UDP parsePacket PARTIAL-DRAIN PHYSICAL"
Write-Host "============================================================"

Assert-G2Branch

Write-Host "HEAD=$(Get-G2Head)"
Write-Host "EFFECTIVE_SPI_HZ=$(Get-G2SpiHz)"
Write-Host "SERIAL_PORT=$SerialPort"
Write-Host "EXPECTED_PARTIAL_PACKET_BYTES=256"
Write-Host "EXPECTED_PARTIAL_READ_BYTES=8"
Write-Host "EXPECTED_REMAINING_BEFORE_DRAIN=248"
Write-Host "DRAIN_HOLD_LIMIT_US=5000"
Write-Host "LOOP_GAP_LIMIT_US=10000"

if ((Get-G2SpiHz) -ne 26000000) {
    throw "A14_NB3E2_SPI_FREQUENCY_MISMATCH"
}

$expectedHashes = [ordered]@{
    "JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/Dns.cpp" = "5D67E2FE8F2333A9A8AAD2A290A26DBAE92761DF11661C4305F8EC9D09604A60"
    "JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/Dns.h" = "B92718E94D549D60A7F2E648DAAD2A18DF4FAB94936BFEB279025911949AAFED"
    "JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/EthernetClient.cpp" = "93B71C92E298053425FF03D750632A30102BE743E1C8B1FDAB800214EABED4B2"
    "JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/EthernetUdp.cpp" = "F5AE79FA9E9642A670D0AF23E029621711D2DD857B776A5208CF2866EDB496E5"
    "JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/jwplc_ethernet_async_tx.cpp" = "CC8EEE779FC932C5A8FA0175ABBF0421AA8C95279B4260FADAC1276E7BD74E1C"
    "JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/jwplc_ethernet_async_tx.h" = "C1C5615DFF167F3572FBEA85CFFE1A7F5051732E2BD446387F1025A1D981FB47"
    "JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/JWPLC_W5x00_Ethernet.h" = "8CE510B0E3FED0E2CAFE962B623559AFBA02DCB88928D5C1A7C879263ED310A3"
    "JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/socket.cpp" = "AE404D279294C0A4ABEA0BE51EE6267EB7106B289C27CF46B3991048A3C6A2C2"
    "JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/utility/w5100.cpp" = "9F94AAC1BB25966C18EDFD6A5C5D5908A9DBD4CF11B6E9BC099E10B28CEF272F"
    "JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/utility/w5100.h" = "9A833532C44E0CFCD66429A764E8BDBF62A8871838B0355565BC0A63043455FC"
    "tools/modbus-tcp-benchmark/firmware/eth14_raw_transport_server/eth14_raw_transport_server.ino" = "9A0C6CF27E44A61DC97686FB1A769EBBBB34D036CDF8D0CA0EEAB597E699AB6B"
}

$expectedDirty = @($expectedHashes.Keys) | Sort-Object
$dirty = @(Get-G2TrackedDirtyPaths)

Write-Host "TRACKED_DIRTY_COUNT=$($dirty.Count)"
$dirty | ForEach-Object { Write-Host "DIRTY=$_" }

if ($dirty.Count -ne $expectedDirty.Count) {
    throw "A14_NB3E2_DIRTY_COUNT_INVALID=$($dirty.Count)"
}

for ($i = 0; $i -lt $expectedDirty.Count; ++$i) {
    if ($dirty[$i] -ne $expectedDirty[$i]) {
        throw "A14_NB3E2_DIRTY_PATH_INVALID=$($dirty[$i])"
    }
}

$staged = @(& git -C $script:G2RepoRoot diff --cached --name-only)
if ($LASTEXITCODE -ne 0 -or $staged.Count -ne 0) {
    throw "A14_NB3E2_INDEX_NOT_CLEAN"
}
Write-Host "STAGED_COUNT=0"

Assert-G2ProtectedArtifacts

foreach ($entry in $expectedHashes.GetEnumerator()) {
    $actual = Get-G2Sha256 $entry.Key
    Write-Host "CANDIDATE_SHA256=$($entry.Key)=$actual"

    if ($actual -ne $entry.Value) {
        throw "A14_NB3E2_CANDIDATE_HASH_MISMATCH=$($entry.Key)"
    }
}

$arduinoCli = "C:\Program Files\Arduino PLC IDE Tools\arduino-cli.exe"
if (-not (Test-Path -LiteralPath $arduinoCli)) {
    throw "A14_NB3E2_ARDUINO_CLI_NOT_FOUND=$arduinoCli"
}

$pythonCommand = Get-Command python.exe -ErrorAction SilentlyContinue
if ($null -eq $pythonCommand) {
    $pythonCommand = Get-Command python -ErrorAction Stop
}
$pythonExe = $pythonCommand.Source

$fqbn = "jwplc_local:esp32:jwplcbasic"
$librariesRoot = Get-G2Path "JWPLC/2.1.0/libraries"
$expectedEthernetLibrary = Join-Path $librariesRoot "JWPLC_Ethernet"

$probeRelative = "tools/modbus-tcp-benchmark/firmware/a14_nb3_udp_parse_packet_probe/a14_nb3_udp_parse_packet_probe.ino"
$clientRelative = "tools/modbus-tcp-benchmark/gates/a14_nb3_udp_parse_packet_physical_client.py"

$probePath = Get-G2Path $probeRelative
$probeDir = Split-Path -Parent $probePath
$clientPath = Get-G2Path $clientRelative

if (-not (Test-Path -LiteralPath $probePath)) {
    throw "A14_NB3E2_PROBE_NOT_FOUND"
}

if (-not (Test-Path -LiteralPath $clientPath)) {
    throw "A14_NB3E2_CLIENT_NOT_FOUND"
}

$timestamp = Get-Date -Format "yyyyMMdd_HHmmss"
$tempRoot = Join-Path $env:TEMP ("jwplc_a14_nb3e2_parse_packet_{0}" -f $timestamp)
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
    Get-Content -LiteralPath $clientSyntaxLog -Tail 80 | ForEach-Object { Write-Host $_ }
    throw "A14_NB3E2_CLIENT_SYNTAX_FAILED"
}
Write-Host "CLIENT_SYNTAX=PASS"

Write-Host ""
Write-Host "=== COMPILE UDP parsePacket PHYSICAL PROBE ==="

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
    Get-Content -LiteralPath $compileLog -Tail 120 | ForEach-Object { Write-Host $_ }
    throw "A14_NB3E2_COMPILE_FAILED"
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
    throw "A14_NB3E2_WRONG_ETHERNET_LIBRARY"
}

$binCount = @(Get-ChildItem -LiteralPath $buildPath -Recurse -File -Filter "*.bin").Count
Write-Host "BIN_COUNT=$binCount"

if ($binCount -lt 1) {
    Invoke-G2CompileFinishedSound -Success $false
    throw "A14_NB3E2_NO_BIN"
}

Invoke-G2CompileFinishedSound -Success $true
Write-Host "COMPILE_FINISHED_SOUND=PLAYED"

Write-Host ""
Write-Host "=== UPLOAD UDP parsePacket PHYSICAL PROBE ==="

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
    Get-Content -LiteralPath $uploadLog -Tail 120 | ForEach-Object { Write-Host $_ }
    throw "A14_NB3E2_UPLOAD_FAILED"
}

Write-Host ""
Write-Host "=== RUN PARTIAL-DRAIN + RECOVERY PHYSICAL SCENARIO ==="

Start-Sleep -Milliseconds 300

$clientArgs = @(
    $clientPath,
    "--serial", $SerialPort,
    "--timeout-s", "8",
    "--udp-port", "5003"
)

$clientExit = Invoke-NB3NativeToLog -FilePath $pythonExe -Arguments $clientArgs -LogPath $clientLog

Write-Host "CLIENT_EXIT=$clientExit"
Write-Host "CLIENT_LOG=$clientLog"
Get-Content -LiteralPath $clientLog | ForEach-Object { Write-Host $_ }

if ($clientExit -ne 0) {
    throw "A14_NB3E2_CLIENT_FAILED=$clientExit"
}

$logText = [System.IO.File]::ReadAllText($clientLog)

$resultCode = Get-LogInt64 -Text $logText -Key "RESULT_CODE"
$probeFailed = Get-LogValue -Text $logText -Key "PROBE_FAILED"
$dutIp = Get-LogValue -Text $logText -Key "DUT_IP_EFFECTIVE"
$partialSent = Get-LogInt64 -Text $logText -Key "PARTIAL_SENT_BYTES"
$nextSent = Get-LogInt64 -Text $logText -Key "NEXT_SENT_BYTES"
$partialPacketSize = Get-LogInt64 -Text $logText -Key "PARTIAL_PACKET_SIZE"
$partialReadBytes = Get-LogInt64 -Text $logText -Key "PARTIAL_READ_BYTES"
$remainingBefore = Get-LogInt64 -Text $logText -Key "PARTIAL_REMAINING_BEFORE_DRAIN"
$drainParseReturn = Get-LogInt64 -Text $logText -Key "DRAIN_PARSE_RETURN"
$remainingAfter = Get-LogInt64 -Text $logText -Key "DRAIN_REMAINING_AFTER"
$drainHoldUs = Get-LogInt64 -Text $logText -Key "DRAIN_HOLD_US"
$nextRecovered = Get-LogValue -Text $logText -Key "NEXT_PACKET_RECOVERED"
$spiLockErrors = Get-LogInt64 -Text $logText -Key "SPI_LOCK_ERRORS"
$loopGapMaxUs = Get-LogInt64 -Text $logText -Key "LOOP_GAP_MAX_US"
$clientPass = Get-LogValue -Text $logText -Key "NB3_E2_CLIENT_PASS"

Write-Host ""
Write-Host "=== NB3-E2 PHYSICAL SUMMARY ==="
Write-Host "DUT_IP_EFFECTIVE=$dutIp"
Write-Host "PARTIAL_SENT_BYTES=$partialSent"
Write-Host "PARTIAL_PACKET_SIZE=$partialPacketSize"
Write-Host "PARTIAL_READ_BYTES=$partialReadBytes"
Write-Host "PARTIAL_REMAINING_BEFORE_DRAIN=$remainingBefore"
Write-Host "DRAIN_PARSE_RETURN=$drainParseReturn"
Write-Host "DRAIN_REMAINING_AFTER=$remainingAfter"
Write-Host "DRAIN_HOLD_US=$drainHoldUs"
Write-Host "NEXT_SENT_BYTES=$nextSent"
Write-Host "NEXT_PACKET_RECOVERED=$nextRecovered"
Write-Host "SPI_LOCK_ERRORS=$spiLockErrors"
Write-Host "LOOP_GAP_MAX_US=$loopGapMaxUs"

if ($resultCode -ne 1 -or $probeFailed -ne "NO") {
    throw "A14_NB3E2_PROBE_FAIL"
}

if ($clientPass -ne "YES") {
    throw "A14_NB3E2_CLIENT_PASS_MISSING"
}

if ($partialSent -ne 256 -or $partialPacketSize -ne 256) {
    throw "A14_NB3E2_PARTIAL_PACKET_SIZE_INVALID sent=$partialSent parsed=$partialPacketSize"
}

if ($partialReadBytes -ne 8 -or $remainingBefore -ne 248) {
    throw "A14_NB3E2_PARTIAL_REMAINING_INVALID read=$partialReadBytes remaining=$remainingBefore"
}

if ($drainParseReturn -ne 0 -or $remainingAfter -ne 0) {
    throw "A14_NB3E2_DRAIN_NOT_COMPLETED return=$drainParseReturn remaining=$remainingAfter"
}

if ($drainHoldUs -gt 5000) {
    throw "A14_NB3E2_DRAIN_HOLD_HIGH=$drainHoldUs"
}

if ($nextSent -ne 4 -or $nextRecovered -ne "YES") {
    throw "A14_NB3E2_NEXT_PACKET_RECOVERY_FAIL"
}

if ($spiLockErrors -ne 0) {
    throw "A14_NB3E2_SPI_LOCK_ERRORS=$spiLockErrors"
}

if ($loopGapMaxUs -gt 10000) {
    throw "A14_NB3E2_LOOP_GAP_HIGH=$loopGapMaxUs"
}

Write-Host ""
Write-Host "OBSERVACION FISICA REQUERIDA: mira la TFT durante NB3-E2."
$answer = ""

while ($answer -notin @("S", "N")) {
    $answer = (Read-Host 'Aparecio el diagnostico visual "SPI" durante NB3-E2? (S/N)').Trim().ToUpperInvariant()
}

$visualEvents = if ($answer -eq "S") { 1 } else { 0 }
Write-Host "VISUAL_SPI_EVENTS=$visualEvents"

if ($visualEvents -ne 0) {
    throw "A14_NB3E2_VISUAL_SPI_FAIL"
}

Assert-G2ProtectedArtifacts

$dirtyFinal = @(Get-G2TrackedDirtyPaths)
$stagedFinal = @(& git -C $script:G2RepoRoot diff --cached --name-only)

Write-Host "TRACKED_DIRTY_COUNT_FINAL=$($dirtyFinal.Count)"
$dirtyFinal | ForEach-Object { Write-Host "DIRTY_FINAL=$_" }
Write-Host "STAGED_COUNT_FINAL=$($stagedFinal.Count)"

if ($dirtyFinal.Count -ne $expectedDirty.Count -or $stagedFinal.Count -ne 0) {
    throw "A14_NB3E2_FINAL_WORKTREE_INVALID"
}

for ($i = 0; $i -lt $expectedDirty.Count; ++$i) {
    if ($dirtyFinal[$i] -ne $expectedDirty[$i]) {
        throw "A14_NB3E2_FINAL_DIRTY_PATH_INVALID=$($dirtyFinal[$i])"
    }
}

Write-Host "NB3_E2_PARTIAL_PACKET_CREATED=PASS"
Write-Host "NB3_E2_REMAINING_248_CONFIRMED=PASS"
Write-Host "NB3_E2_SINGLE_DRAIN_COMPLETED=PASS"
Write-Host "NB3_E2_PARSE_RETURNED_TO_LOOP=PASS"
Write-Host "NB3_E2_NEXT_PACKET_RECOVERY=PASS"
Write-Host "NB3_E2_DRAIN_HOLD_5MS=PASS"
Write-Host "NB3_E2_LOOP_GAP_10MS=PASS"
Write-Host "NB3_E2_SPI_LOCK_ERRORS=ZERO"
Write-Host "NB3_E2_VISUAL_SPI=PASS"
Write-Host "NB3_UDP_RX_THROUGHPUT_REGRESSION=PENDING_NB3_E3"
Write-Host "A14_NB3_E2_UDP_PARSE_PACKET_PHYSICAL=PASS"
