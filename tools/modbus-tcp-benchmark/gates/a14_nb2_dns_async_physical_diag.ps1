param(
    [string]$SerialPort = "COM14",
    [string]$DutIp = "192.168.0.31"
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"
. (Join-Path $PSScriptRoot "common.ps1")

function Invoke-NB2NativeToLog {
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
    param([string]$Text, [string]$Key)

    $pattern = "(?m)^" + [regex]::Escape($Key) + "=(.*)\r?$"
    $matches = @([regex]::Matches($Text, $pattern))
    if ($matches.Count -ne 1) {
        throw "A14_NB2C_KEY_COUNT_$($Key)=$($matches.Count)"
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

Write-Host "============================================================"
Write-Host " A14 NB2-C - COOPERATIVE DNS PHYSICAL DIAGNOSTIC"
Write-Host "============================================================"

Assert-G2Branch

$dnsHeaderRelative = "JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/Dns.h"
$dnsCppRelative = "JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/Dns.cpp"
$clientHeaderRelative = "JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/JWPLC_W5x00_Ethernet.h"
$clientCppRelative = "JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/EthernetClient.cpp"
$probeRelative = "tools/modbus-tcp-benchmark/firmware/a14_nb2_dns_async_probe/a14_nb2_dns_async_probe.ino"
$clientRelative = "tools/modbus-tcp-benchmark/gates/a14_nb2_dns_async_physical_client.py"

$expectedDnsHeaderSha256 = "EF6C5392C0CBFB8545BFF8EB4D30E3FEDAC005725DF292418A5CF974BE6F6FD2"
$expectedDnsCppSha256 = "3DE3CD1A14BD07C1FE3B0725941B2137A5109B43647AA3E10B3EF6A1E6EEC5F5"
$expectedClientHeaderSha256 = "3E11094D68873D81F264F8AD97C1D55867F5399E28613061BC38484CD7DE614B"
$expectedClientCppSha256 = "93B71C92E298053425FF03D750632A30102BE743E1C8B1FDAB800214EABED4B2"
$expectedRawSha256 = "9A0C6CF27E44A61DC97686FB1A769EBBBB34D036CDF8D0CA0EEAB597E699AB6B"

$expectedDirty = @(
    $dnsHeaderRelative,
    $dnsCppRelative,
    $clientHeaderRelative,
    $clientCppRelative,
    $script:G2SpiHeaderRelative,
    $script:G2RawFirmwareRelative
) | Sort-Object

$dirty = @(Get-G2TrackedDirtyPaths)

Write-Host "HEAD=$(Get-G2Head)"
Write-Host "EFFECTIVE_SPI_HZ=$(Get-G2SpiHz)"
Write-Host "SUCCESS_TIMEOUT_MS=500"
Write-Host "SILENT_TIMEOUT_MS_PER_WINDOW=150"
Write-Host "SILENT_TIMEOUT_WINDOWS=3"
Write-Host "EXPECTED_SILENT_TOTAL_MS_APPROX=450"
Write-Host "EXPECTED_VALID_IP=10.20.30.40"
Write-Host "TRACKED_DIRTY_COUNT=$($dirty.Count)"
$dirty | ForEach-Object { Write-Host "DIRTY=$_" }

if ($dirty.Count -ne $expectedDirty.Count) {
    throw "A14_NB2C_DIRTY_COUNT_INVALID=$($dirty.Count)"
}
for ($i = 0; $i -lt $expectedDirty.Count; ++$i) {
    if ($dirty[$i] -ne $expectedDirty[$i]) {
        throw "A14_NB2C_DIRTY_PATH_INVALID=$($dirty[$i])"
    }
}

$staged = @(& git -C $script:G2RepoRoot diff --cached --name-only)
if ($LASTEXITCODE -ne 0 -or $staged.Count -ne 0) {
    throw "A14_NB2C_INDEX_NOT_CLEAN"
}
Write-Host "STAGED_COUNT=0"

Assert-G2ProtectedArtifacts

if ((Get-G2Sha256 $dnsHeaderRelative) -ne $expectedDnsHeaderSha256) {
    throw "A14_NB2C_DNS_HEADER_HASH_MISMATCH"
}
if ((Get-G2Sha256 $dnsCppRelative) -ne $expectedDnsCppSha256) {
    throw "A14_NB2C_DNS_CPP_HASH_MISMATCH"
}
if ((Get-G2Sha256 $clientHeaderRelative) -ne $expectedClientHeaderSha256) {
    throw "A14_NB2C_CLIENT_HEADER_HASH_MISMATCH"
}
if ((Get-G2Sha256 $clientCppRelative) -ne $expectedClientCppSha256) {
    throw "A14_NB2C_CLIENT_CPP_HASH_MISMATCH"
}
if ((Get-G2Sha256 $script:G2RawFirmwareRelative) -ne $expectedRawSha256) {
    throw "A14_NB2C_RAW_HASH_MISMATCH"
}

$arduinoCli = "C:\Program Files\Arduino PLC IDE Tools\arduino-cli.exe"
if (-not (Test-Path -LiteralPath $arduinoCli)) {
    throw "A14_NB2C_ARDUINO_CLI_NOT_FOUND=$arduinoCli"
}

$pythonCommand = Get-Command python.exe -ErrorAction SilentlyContinue
if ($null -eq $pythonCommand) {
    $pythonCommand = Get-Command python -ErrorAction Stop
}
$pythonExe = $pythonCommand.Source

$fqbn = "jwplc_local:esp32:jwplcbasic"
$librariesRoot = Get-G2Path "JWPLC/2.1.0/libraries"
$expectedEthernetLibrary = Join-Path $librariesRoot "JWPLC_Ethernet"
$probePath = Get-G2Path $probeRelative
$probeDir = Split-Path -Parent $probePath
$clientPath = Get-G2Path $clientRelative

$timestamp = Get-Date -Format "yyyyMMdd_HHmmss"
$tempRoot = Join-Path $env:TEMP ("jwplc_a14_nb2c_dns_{0}" -f $timestamp)
$buildPath = Join-Path $tempRoot "build"
$compileLog = Join-Path $tempRoot "compile.log"
$uploadLog = Join-Path $tempRoot "upload.log"
$clientLog = Join-Path $tempRoot "client.log"

New-Item -ItemType Directory -Force -Path $buildPath | Out-Null

Write-Host "ARDUINO_CLI=$arduinoCli"
Write-Host "PYTHON=$pythonExe"
Write-Host "FQBN=$fqbn"
Write-Host "PROBE=$probePath"
Write-Host "CLIENT=$clientPath"
Write-Host "TEMP_ROOT=$tempRoot"
Write-Host "SOURCE_MUTATION=NO"

Write-Host ""
Write-Host "=== COMPILE DNS PHYSICAL PROBE ==="

$compileArgs = @(
    "compile",
    "--fqbn", $fqbn,
    "--build-path", $buildPath,
    "--libraries", $librariesRoot,
    $probeDir
)

$compileExit = Invoke-NB2NativeToLog -FilePath $arduinoCli -Arguments $compileArgs -LogPath $compileLog

Write-Host "COMPILE_EXIT=$compileExit"
Write-Host "COMPILE_LOG=$compileLog"

if ($compileExit -ne 0) {
    Invoke-G2CompileFinishedSound -Success $false
    Get-Content -LiteralPath $compileLog -Tail 120 | ForEach-Object { Write-Host $_ }
    throw "A14_NB2C_COMPILE_FAILED"
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
    throw "A14_NB2C_WRONG_ETHERNET_LIBRARY"
}

$binCount = @(
    Get-ChildItem -LiteralPath $buildPath -Recurse -File -Filter "*.bin"
).Count
Write-Host "BIN_COUNT=$binCount"

if ($binCount -lt 1) {
    Invoke-G2CompileFinishedSound -Success $false
    throw "A14_NB2C_NO_BIN"
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

$uploadExit = Invoke-NB2NativeToLog -FilePath $arduinoCli -Arguments $uploadArgs -LogPath $uploadLog

Write-Host "UPLOAD_EXIT=$uploadExit"
Write-Host "UPLOAD_LOG=$uploadLog"

if ($uploadExit -ne 0) {
    Get-Content -LiteralPath $uploadLog -Tail 120 | ForEach-Object { Write-Host $_ }
    throw "A14_NB2C_UPLOAD_FAILED"
}

Write-Host ""
Write-Host "=== RUN LOCAL DNS RESPONDER + SERIAL CAPTURE ==="

Start-Sleep -Milliseconds 600

$clientArgs = @(
    $clientPath,
    "--dut", $DutIp,
    "--serial", $SerialPort,
    "--timeout-s", "8"
)

$clientExit = Invoke-NB2NativeToLog -FilePath $pythonExe -Arguments $clientArgs -LogPath $clientLog

Write-Host "CLIENT_EXIT=$clientExit"
Write-Host "CLIENT_LOG=$clientLog"
Get-Content -LiteralPath $clientLog | ForEach-Object { Write-Host $_ }

if ($clientExit -ne 0) {
    throw "A14_NB2C_CLIENT_FAILED=$clientExit"
}

$logText = [System.IO.File]::ReadAllText($clientLog)

$resultCode = Get-LogInt64 -Text $logText -Key "RESULT_CODE"
$probeFailed = Get-LogValue -Text $logText -Key "PROBE_FAILED"
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

Write-Host ""
Write-Host "=== NB2-C PHYSICAL SUMMARY ==="
Write-Host "SUCCESS_RESULT_IP=$successIp"
Write-Host "SUCCESS_DURATION_MS=$successDurationMs"
Write-Host "SUCCESS_POLL_COUNT=$successPollCount"
Write-Host "SUCCESS_POLL_PENDING_COUNT=$successPendingCount"
Write-Host "TIMEOUT_DURATION_MS=$timeoutDurationMs"
Write-Host "TIMEOUT_POLL_COUNT=$timeoutPollCount"
Write-Host "TIMEOUT_POLL_PENDING_COUNT=$timeoutPendingCount"
Write-Host "DNS_BEGIN_HOLD_MAX_US=$beginHoldMaxUs"
Write-Host "DNS_POLL_HOLD_MAX_US=$pollHoldMaxUs"
Write-Host "LOOP_GAP_MAX_US=$loopGapMaxUs"
Write-Host "SPI_LOCK_ERRORS=$spiLockErrors"
Write-Host "DNS_VALID_QUERY_COUNT=$validQueries"
Write-Host "DNS_TIMEOUT_QUERY_COUNT=$timeoutQueries"
Write-Host "DNS_OTHER_QUERY_COUNT=$otherQueries"

if ($resultCode -ne 1 -or $probeFailed -ne "NO") {
    throw "A14_NB2C_PROBE_FAIL"
}
if ($successIp -ne "10.20.30.40") {
    throw "A14_NB2C_VALID_IP_MISMATCH=$successIp"
}
if ($successPollCount -lt 1) {
    throw "A14_NB2C_SUCCESS_POLL_NOT_OBSERVED"
}
if ($timeoutPollCount -lt 2 -or $timeoutPendingCount -lt 1) {
    throw "A14_NB2C_TIMEOUT_PENDING_NOT_OBSERVED"
}
if ($timeoutDurationMs -lt 350 -or $timeoutDurationMs -gt 900) {
    throw "A14_NB2C_TIMEOUT_DURATION_INVALID=$timeoutDurationMs"
}
if ($pollHoldMaxUs -gt 5000) {
    throw "A14_NB2C_POLL_HOLD_HIGH=$pollHoldMaxUs"
}
if ($loopGapMaxUs -gt 10000) {
    throw "A14_NB2C_LOOP_GAP_HIGH=$loopGapMaxUs"
}
if ($spiLockErrors -ne 0) {
    throw "A14_NB2C_SPI_LOCK_ERRORS=$spiLockErrors"
}
if ($validQueries -lt 1 -or $timeoutQueries -lt 1) {
    throw "A14_NB2C_DNS_QUERY_COUNTS_INVALID"
}
if ($otherQueries -ne 0) {
    throw "A14_NB2C_UNEXPECTED_DNS_QUERIES=$otherQueries"
}

Write-Host ""
Write-Host "OBSERVACION FISICA REQUERIDA: mira la TFT durante NB2-C."
$answer = ""

while ($answer -notin @("S", "N")) {
    $answer = (Read-Host 'Aparecio el diagnostico visual "SPI" durante NB2-C? (S/N)').Trim().ToUpperInvariant()
}

$visualEvents = if ($answer -eq "S") { 1 } else { 0 }
Write-Host "VISUAL_SPI_EVENTS=$visualEvents"

if ($visualEvents -ne 0) {
    throw "A14_NB2C_VISUAL_SPI_FAIL"
}

Assert-G2ProtectedArtifacts

$dirtyFinal = @(Get-G2TrackedDirtyPaths)
$stagedFinal = @(& git -C $script:G2RepoRoot diff --cached --name-only)

Write-Host "TRACKED_DIRTY_COUNT_FINAL=$($dirtyFinal.Count)"
Write-Host "STAGED_COUNT_FINAL=$($stagedFinal.Count)"

if ($dirtyFinal.Count -ne $expectedDirty.Count -or $stagedFinal.Count -ne 0) {
    throw "A14_NB2C_FINAL_WORKTREE_INVALID"
}

Write-Host "NB2_DNS_VALID_RESOLUTION=PASS"
Write-Host "NB2_DNS_TIMEOUT_PENDING=REPRODUCED"
Write-Host "NB2_DNS_ASYNC_POLL_COOPERATIVE=PASS"
Write-Host "NB2_DNS_UDP_SEND_SYNC_DEPENDENCY=PENDING_NB3"
Write-Host "NB2_VISUAL_SPI=PASS"
Write-Host "A14_NB2_DNS_ASYNC_PHYSICAL_DIAG=PASS"
