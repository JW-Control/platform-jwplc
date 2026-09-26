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
        throw "A14_NB3D2_KEY_COUNT_$($Key)=$($matches.Count)"
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
Write-Host " A14 NB3-D2 - UDP SEND COOPERATIVE DNS PHYSICAL"
Write-Host "============================================================"

Assert-G2Branch

$baselineBeginHoldUs = 6033
$targetBeginHoldUs = 5000
$pollHoldLimitUs = 5000
$loopGapLimitUs = 10000

Write-Host "HEAD=$(Get-G2Head)"
Write-Host "EFFECTIVE_SPI_HZ=$(Get-G2SpiHz)"
Write-Host "SERIAL_PORT=$SerialPort"
Write-Host "DNS_BEGIN_HOLD_BASELINE_US=$baselineBeginHoldUs"
Write-Host "DNS_BEGIN_HOLD_TARGET_US=$targetBeginHoldUs"
Write-Host "DNS_POLL_HOLD_LIMIT_US=$pollHoldLimitUs"
Write-Host "LOOP_GAP_LIMIT_US=$loopGapLimitUs"

if ((Get-G2SpiHz) -ne 26000000) {
    throw "A14_NB3D2_SPI_FREQUENCY_MISMATCH"
}

$expectedHashes = [ordered]@{
    "JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/Dns.cpp" = "5D67E2FE8F2333A9A8AAD2A290A26DBAE92761DF11661C4305F8EC9D09604A60"
    "JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/Dns.h" = "B92718E94D549D60A7F2E648DAAD2A18DF4FAB94936BFEB279025911949AAFED"
    "JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/EthernetClient.cpp" = "93B71C92E298053425FF03D750632A30102BE743E1C8B1FDAB800214EABED4B2"
    "JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/EthernetUdp.cpp" = "90F37F3C3E7E4C0F8E0B4982E515AFCFE4A3A480084FBA3E6DCDF0D0DA58CAA1"
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
    throw "A14_NB3D2_DIRTY_COUNT_INVALID=$($dirty.Count)"
}

for ($i = 0; $i -lt $expectedDirty.Count; ++$i) {
    if ($dirty[$i] -ne $expectedDirty[$i]) {
        throw "A14_NB3D2_DIRTY_PATH_INVALID=$($dirty[$i])"
    }
}

$staged = @(& git -C $script:G2RepoRoot diff --cached --name-only)
if ($LASTEXITCODE -ne 0 -or $staged.Count -ne 0) {
    throw "A14_NB3D2_INDEX_NOT_CLEAN"
}
Write-Host "STAGED_COUNT=0"

Assert-G2ProtectedArtifacts

foreach ($entry in $expectedHashes.GetEnumerator()) {
    $actual = Get-G2Sha256 $entry.Key
    Write-Host "CANDIDATE_SHA256=$($entry.Key)=$actual"

    if ($actual -ne $entry.Value) {
        throw "A14_NB3D2_CANDIDATE_HASH_MISMATCH=$($entry.Key)"
    }
}

$arduinoCli = "C:\Program Files\Arduino PLC IDE Tools\arduino-cli.exe"
if (-not (Test-Path -LiteralPath $arduinoCli)) {
    throw "A14_NB3D2_ARDUINO_CLI_NOT_FOUND=$arduinoCli"
}

$pythonCommand = Get-Command python.exe -ErrorAction SilentlyContinue
if ($null -eq $pythonCommand) {
    $pythonCommand = Get-Command python -ErrorAction Stop
}
$pythonExe = $pythonCommand.Source

$fqbn = "jwplc_local:esp32:jwplcbasic"
$librariesRoot = Get-G2Path "JWPLC/2.1.0/libraries"
$expectedEthernetLibrary = Join-Path $librariesRoot "JWPLC_Ethernet"

$probeRelative = "tools/modbus-tcp-benchmark/firmware/a14_nb2_dns_async_probe/a14_nb2_dns_async_probe.ino"
$clientRelative = "tools/modbus-tcp-benchmark/gates/a14_nb3_udp_send_dns_physical_client.py"

$probePath = Get-G2Path $probeRelative
$probeDir = Split-Path -Parent $probePath
$clientPath = Get-G2Path $clientRelative

if (-not (Test-Path -LiteralPath $clientPath)) {
    throw "A14_NB3D2_CLIENT_NOT_FOUND"
}

$timestamp = Get-Date -Format "yyyyMMdd_HHmmss"
$tempRoot = Join-Path $env:TEMP ("jwplc_a14_nb3d2_dns_{0}" -f $timestamp)
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
    throw "A14_NB3D2_CLIENT_SYNTAX_FAILED"
}
Write-Host "CLIENT_SYNTAX=PASS"

Write-Host ""
Write-Host "=== COMPILE DNS PHYSICAL PROBE ==="

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
    throw "A14_NB3D2_COMPILE_FAILED"
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
    throw "A14_NB3D2_WRONG_ETHERNET_LIBRARY"
}

$binCount = @(
    Get-ChildItem -LiteralPath $buildPath -Recurse -File -Filter "*.bin"
).Count

Write-Host "BIN_COUNT=$binCount"

if ($binCount -lt 1) {
    Invoke-G2CompileFinishedSound -Success $false
    throw "A14_NB3D2_NO_BIN"
}

Invoke-G2CompileFinishedSound -Success $true
Write-Host "COMPILE_FINISHED_SOUND=PLAYED"

Write-Host ""
Write-Host "=== UPLOAD DNS PHYSICAL PROBE ==="

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
    throw "A14_NB3D2_UPLOAD_FAILED"
}

Write-Host ""
Write-Host "=== RUN DYNAMIC-IP DNS RESPONDER + SERIAL CAPTURE ==="

Start-Sleep -Milliseconds 600

$clientArgs = @(
    $clientPath,
    "--serial", $SerialPort,
    "--timeout-s", "8"
)

$clientExit = Invoke-NB3NativeToLog -FilePath $pythonExe -Arguments $clientArgs -LogPath $clientLog

Write-Host "CLIENT_EXIT=$clientExit"
Write-Host "CLIENT_LOG=$clientLog"
Get-Content -LiteralPath $clientLog | ForEach-Object { Write-Host $_ }

if ($clientExit -ne 0) {
    throw "A14_NB3D2_CLIENT_FAILED=$clientExit"
}

$logText = [System.IO.File]::ReadAllText($clientLog)

$resultCode = Get-LogInt64 -Text $logText -Key "RESULT_CODE"
$probeFailed = Get-LogValue -Text $logText -Key "PROBE_FAILED"
$dutIp = Get-LogValue -Text $logText -Key "DUT_IP_EFFECTIVE"
$successIp = Get-LogValue -Text $logText -Key "SUCCESS_RESULT_IP"
$successDurationMs = Get-LogInt64 -Text $logText -Key "SUCCESS_DURATION_MS"
$successPollCount = Get-LogInt64 -Text $logText -Key "SUCCESS_POLL_COUNT"
$successPendingCount = Get-LogInt64 -Text $logText -Key "SUCCESS_POLL_PENDING_COUNT"
$timeoutDurationMs = Get-LogInt64 -Text $logText -Key "TIMEOUT_DURATION_MS"
$timeoutPollCount = Get-LogInt64 -Text $logText -Key "TIMEOUT_POLL_COUNT"
$timeoutPendingCount = Get-LogInt64 -Text $logText -Key "TIMEOUT_POLL_PENDING_COUNT"
$beginHoldMaxUs = Get-LogInt64 -Text $logText -Key "DNS_BEGIN_HOLD_MAX_US"
$pollHoldMaxUs = Get-LogInt64 -Text $logText -Key "DNS_POLL_HOLD_MAX_US"
$loopGapMaxUs = Get-LogInt64 -Text $logText -Key "LOOP_GAP_MAX_US"
$spiLockErrors = Get-LogInt64 -Text $logText -Key "SPI_LOCK_ERRORS"
$validQueries = Get-LogInt64 -Text $logText -Key "DNS_VALID_QUERY_COUNT"
$timeoutQueries = Get-LogInt64 -Text $logText -Key "DNS_TIMEOUT_QUERY_COUNT"
$otherQueries = Get-LogInt64 -Text $logText -Key "DNS_OTHER_QUERY_COUNT"

$beginHoldReductionUs = $baselineBeginHoldUs - $beginHoldMaxUs
$beginHoldReductionPct = [math]::Round(
    (100.0 * $beginHoldReductionUs / $baselineBeginHoldUs),
    1
)

Write-Host ""
Write-Host "=== NB3-D2 PHYSICAL SUMMARY ==="
Write-Host "DUT_IP_EFFECTIVE=$dutIp"
Write-Host "SUCCESS_RESULT_IP=$successIp"
Write-Host "SUCCESS_DURATION_MS=$successDurationMs"
Write-Host "SUCCESS_POLL_COUNT=$successPollCount"
Write-Host "SUCCESS_POLL_PENDING_COUNT=$successPendingCount"
Write-Host "TIMEOUT_DURATION_MS=$timeoutDurationMs"
Write-Host "TIMEOUT_POLL_COUNT=$timeoutPollCount"
Write-Host "TIMEOUT_POLL_PENDING_COUNT=$timeoutPendingCount"
Write-Host "DNS_BEGIN_HOLD_BASELINE_US=$baselineBeginHoldUs"
Write-Host "DNS_BEGIN_HOLD_MAX_US=$beginHoldMaxUs"
Write-Host "DNS_BEGIN_HOLD_REDUCTION_US=$beginHoldReductionUs"
Write-Host "DNS_BEGIN_HOLD_REDUCTION_PCT=$beginHoldReductionPct"
Write-Host "DNS_POLL_HOLD_MAX_US=$pollHoldMaxUs"
Write-Host "LOOP_GAP_MAX_US=$loopGapMaxUs"
Write-Host "SPI_LOCK_ERRORS=$spiLockErrors"
Write-Host "DNS_VALID_QUERY_COUNT=$validQueries"
Write-Host "DNS_TIMEOUT_QUERY_COUNT=$timeoutQueries"
Write-Host "DNS_OTHER_QUERY_COUNT=$otherQueries"

if ($resultCode -ne 1 -or $probeFailed -ne "NO") {
    throw "A14_NB3D2_PROBE_FAIL"
}

if ($successIp -ne "10.20.30.40") {
    throw "A14_NB3D2_VALID_IP_MISMATCH=$successIp"
}

if ($successPollCount -lt 2 -or $successPendingCount -lt 1) {
    throw "A14_NB3D2_SUCCESS_COOPERATIVE_SEND_NOT_OBSERVED"
}

if ($timeoutPollCount -lt 2 -or $timeoutPendingCount -lt 1) {
    throw "A14_NB3D2_TIMEOUT_PENDING_NOT_OBSERVED"
}

if ($timeoutDurationMs -lt 350 -or $timeoutDurationMs -gt 900) {
    throw "A14_NB3D2_TIMEOUT_DURATION_INVALID=$timeoutDurationMs"
}

if ($beginHoldMaxUs -gt $targetBeginHoldUs) {
    throw "A14_NB3D2_BEGIN_HOLD_TARGET_EXCEEDED=$beginHoldMaxUs"
}

if ($beginHoldMaxUs -ge $baselineBeginHoldUs) {
    throw "A14_NB3D2_BEGIN_HOLD_NOT_IMPROVED=$beginHoldMaxUs"
}

if ($pollHoldMaxUs -gt $pollHoldLimitUs) {
    throw "A14_NB3D2_POLL_HOLD_HIGH=$pollHoldMaxUs"
}

if ($loopGapMaxUs -gt $loopGapLimitUs) {
    throw "A14_NB3D2_LOOP_GAP_HIGH=$loopGapMaxUs"
}

if ($spiLockErrors -ne 0) {
    throw "A14_NB3D2_SPI_LOCK_ERRORS=$spiLockErrors"
}

if ($validQueries -lt 1 -or $timeoutQueries -lt 1) {
    throw "A14_NB3D2_DNS_QUERY_COUNTS_INVALID"
}

if ($otherQueries -ne 0) {
    throw "A14_NB3D2_UNEXPECTED_DNS_QUERIES=$otherQueries"
}

Write-Host ""
Write-Host "OBSERVACION FISICA REQUERIDA: mira la TFT durante NB3-D2."
$answer = ""

while ($answer -notin @("S", "N")) {
    $answer = (Read-Host 'Aparecio el diagnostico visual "SPI" durante NB3-D2? (S/N)').Trim().ToUpperInvariant()
}

$visualEvents = if ($answer -eq "S") { 1 } else { 0 }
Write-Host "VISUAL_SPI_EVENTS=$visualEvents"

if ($visualEvents -ne 0) {
    throw "A14_NB3D2_VISUAL_SPI_FAIL"
}

Assert-G2ProtectedArtifacts

$dirtyFinal = @(Get-G2TrackedDirtyPaths)
$stagedFinal = @(& git -C $script:G2RepoRoot diff --cached --name-only)

Write-Host "TRACKED_DIRTY_COUNT_FINAL=$($dirtyFinal.Count)"
$dirtyFinal | ForEach-Object { Write-Host "DIRTY_FINAL=$_" }
Write-Host "STAGED_COUNT_FINAL=$($stagedFinal.Count)"

if ($dirtyFinal.Count -ne $expectedDirty.Count -or $stagedFinal.Count -ne 0) {
    throw "A14_NB3D2_FINAL_WORKTREE_INVALID"
}

for ($i = 0; $i -lt $expectedDirty.Count; ++$i) {
    if ($dirtyFinal[$i] -ne $expectedDirty[$i]) {
        throw "A14_NB3D2_FINAL_DIRTY_PATH_INVALID=$($dirtyFinal[$i])"
    }
}

Write-Host "NB3_DNS_DYNAMIC_IP=PASS"
Write-Host "NB3_DNS_VALID_RESOLUTION=PASS"
Write-Host "NB3_DNS_TIMEOUT_PENDING=PASS"
Write-Host "NB3_DNS_UDP_SEND_COOPERATIVE=PASS"
Write-Host "NB3_DNS_BEGIN_HOLD_TARGET_5MS=PASS"
Write-Host "NB3_DNS_BEGIN_HOLD_IMPROVED_FROM_6033US=PASS"
Write-Host "NB3_DNS_POLL_HOLD_5MS=PASS"
Write-Host "NB3_DNS_LOOP_GAP_10MS=PASS"
Write-Host "NB3_DNS_SPI_LOCK_ERRORS=ZERO"
Write-Host "NB3_DNS_VISUAL_SPI=PASS"
Write-Host "NB3_UDP_TX_REGRESSION=PENDING_NB3_D3"
Write-Host "A14_NB3_D2_UDP_SEND_DNS_PHYSICAL=PASS"
