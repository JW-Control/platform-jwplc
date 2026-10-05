param()

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"
. (Join-Path $PSScriptRoot "common.ps1")

Write-Host "============================================================"
Write-Host " A14 NB1-C1 - APPLY RAW BENCHMARK ASYNC TCP STOP"
Write-Host "============================================================"

Assert-G2Branch

$headerRelative = "JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/JWPLC_W5x00_Ethernet.h"
$cppRelative = "JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/EthernetClient.cpp"
$firmwareRelative = $script:G2RawFirmwareRelative
$firmwarePath = Get-G2Path $firmwareRelative

$expectedHeaderSha256 = "3E11094D68873D81F264F8AD97C1D55867F5399E28613061BC38484CD7DE614B"
$expectedCppSha256 = "93B71C92E298053425FF03D750632A30102BE743E1C8B1FDAB800214EABED4B2"
$expectedFirmwareBeforeSha256 = "08E16E60F008371416859B63F93D7C8B7D2FB88325FB7281AC6C5C72983FEA8F"

$dirtyBefore = @(Get-G2TrackedDirtyPaths)
$expectedDirty = @(
    $headerRelative,
    $cppRelative,
    $script:G2SpiHeaderRelative,
    $firmwareRelative
) | Sort-Object

Write-Host "HEAD=$(Get-G2Head)"
Write-Host "TRACKED_DIRTY_BEFORE=$($dirtyBefore.Count)"
$dirtyBefore | ForEach-Object { Write-Host "DIRTY_BEFORE=$_" }

if ($dirtyBefore.Count -ne $expectedDirty.Count) {
    throw "A14_NB1C1_DIRTY_COUNT_BEFORE_INVALID=$($dirtyBefore.Count)"
}
for ($i = 0; $i -lt $expectedDirty.Count; ++$i) {
    if ($dirtyBefore[$i] -ne $expectedDirty[$i]) {
        throw "A14_NB1C1_DIRTY_PATH_BEFORE_INVALID=$($dirtyBefore[$i])"
    }
}

$stagedBefore = @(& git -C $script:G2RepoRoot diff --cached --name-only)
if ($LASTEXITCODE -ne 0 -or $stagedBefore.Count -ne 0) {
    throw "A14_NB1C1_INDEX_NOT_CLEAN_BEFORE"
}
Write-Host "STAGED_COUNT_BEFORE=0"

Assert-G2ProtectedArtifacts

$headerHash = Get-G2Sha256 $headerRelative
$cppHash = Get-G2Sha256 $cppRelative
$firmwareHashBefore = Get-G2Sha256 $firmwareRelative
$runnerHash = Get-G2Sha256 $script:G2RawRunnerRelative
Write-Host "HEADER_SHA256=$headerHash"
Write-Host "CPP_SHA256=$cppHash"
Write-Host "FIRMWARE_SHA256_BEFORE=$firmwareHashBefore"
Write-Host "RAW_RUNNER_SHA256=$runnerHash"

if ($headerHash -ne $expectedHeaderSha256) { throw "A14_NB1C1_HEADER_HASH_MISMATCH" }
if ($cppHash -ne $expectedCppSha256) { throw "A14_NB1C1_CPP_HASH_MISMATCH" }
if ($firmwareHashBefore -ne $expectedFirmwareBeforeSha256) { throw "A14_NB1C1_FIRMWARE_BEFORE_HASH_MISMATCH" }
if ($runnerHash -ne $script:G2RawRunnerSha256) { throw "A14_NB1C1_RUNNER_HASH_MISMATCH" }

$text = [System.IO.File]::ReadAllText($firmwarePath)

$chunkPattern = '(?m)^[ \t]*static constexpr uint8_t TCP_RX_MAX_CHUNKS_PER_LOCK = (\d+);[ \t]*\r?$'
$chunkMatches = @([regex]::Matches($text, $chunkPattern))
if ($chunkMatches.Count -ne 1) { throw "A14_NB1C1_CHUNK_DECLARATION_COUNT=$($chunkMatches.Count)" }
$currentChunks = [int]$chunkMatches[0].Groups[1].Value
Write-Host "TCP_RX_CHUNKS=$currentChunks"
if ($currentChunks -ne 8) { throw "A14_NB1C1_EXPECTED_8_CHUNKS" }

$blockingStopPattern = '(?m)^[ \t]*tcpClient\.stop\(\);[ \t]*\r?$'
$blockingStopsBefore = @([regex]::Matches($text, $blockingStopPattern))
Write-Host "TCP_CLIENT_BLOCKING_STOP_COUNT_BEFORE=$($blockingStopsBefore.Count)"
if ($blockingStopsBefore.Count -ne 2) {
    throw "A14_NB1C1_BLOCKING_STOP_COUNT_BEFORE_INVALID=$($blockingStopsBefore.Count)"
}

$acceptPattern = '(?ms)^static void acceptTcpClient\(\)[ \t]*\r?\n\{.*?^\}[ \t]*\r?$'
$acceptRegex = [regex]::new($acceptPattern)
$acceptMatches = @($acceptRegex.Matches($text))
Write-Host "ACCEPT_TCP_CLIENT_ANCHOR_COUNT=$($acceptMatches.Count)"
if ($acceptMatches.Count -ne 1) {
    throw "A14_NB1C1_ACCEPT_ANCHOR_INVALID=$($acceptMatches.Count)"
}

$acceptReplacement = @'
static void acceptTcpClient()
{
    // El cierre TCP se avanza de forma cooperativa. Cada poll realiza solo
    // accesos breves al W5500 y retorna para liberar el ownership SPI.
    if (tcpClient.stopAsyncInProgress())
    {
        const int stopState =
            tcpClient.pollStopAsync();

        if (stopState == 0)
        {
            return;
        }
    }

    if (
        tcpClient &&
        tcpClient.connected()
    )
    {
        return;
    }

    if (tcpClient)
    {
        const int stopState =
            tcpClient.beginStopAsync();

        if (stopState == 0)
        {
            return;
        }
    }

    tcpClient = tcpServer.accept();

    if (tcpClient)
    {
        mode = MODE_IDLE;
        resetCounters();
    }
}
'@
$text = $acceptRegex.Replace($text, $acceptReplacement, 1)

$blockingStopsAfterAccept = @([regex]::Matches($text, $blockingStopPattern))
Write-Host "TCP_CLIENT_BLOCKING_STOP_COUNT_AFTER_ACCEPT=$($blockingStopsAfterAccept.Count)"
if ($blockingStopsAfterAccept.Count -ne 1) {
    throw "A14_NB1C1_BLOCKING_STOP_AFTER_ACCEPT_INVALID=$($blockingStopsAfterAccept.Count)"
}

$remainingStopRegex = [regex]::new($blockingStopPattern)
$text = $remainingStopRegex.Replace($text, "            (void)tcpClient.beginStopAsync();", 1)

$blockingStopsFinal = @([regex]::Matches($text, $blockingStopPattern))
$beginStopCount = ([regex]::Matches($text, [regex]::Escape("tcpClient.beginStopAsync()"))).Count
$pollStopCount = ([regex]::Matches($text, [regex]::Escape("tcpClient.pollStopAsync()"))).Count
$inProgressCount = ([regex]::Matches($text, [regex]::Escape("tcpClient.stopAsyncInProgress()"))).Count

Write-Host "TCP_CLIENT_BLOCKING_STOP_COUNT_AFTER=$($blockingStopsFinal.Count)"
Write-Host "TCP_CLIENT_BEGIN_STOP_ASYNC_COUNT=$beginStopCount"
Write-Host "TCP_CLIENT_POLL_STOP_ASYNC_COUNT=$pollStopCount"
Write-Host "TCP_CLIENT_STOP_ASYNC_IN_PROGRESS_COUNT=$inProgressCount"

if ($blockingStopsFinal.Count -ne 0) { throw "A14_NB1C1_BLOCKING_STOP_REMAINS" }
if ($beginStopCount -ne 2) { throw "A14_NB1C1_BEGIN_STOP_COUNT_INVALID=$beginStopCount" }
if ($pollStopCount -ne 1) { throw "A14_NB1C1_POLL_STOP_COUNT_INVALID=$pollStopCount" }
if ($inProgressCount -ne 1) { throw "A14_NB1C1_IN_PROGRESS_COUNT_INVALID=$inProgressCount" }

$utf8NoBom = New-Object -TypeName System.Text.UTF8Encoding -ArgumentList $false
[System.IO.File]::WriteAllText($firmwarePath, $text, $utf8NoBom)

& git -C $script:G2RepoRoot diff --check
if ($LASTEXITCODE -ne 0) { throw "A14_NB1C1_DIFF_CHECK_FAILED" }

$dirtyAfter = @(Get-G2TrackedDirtyPaths)
Write-Host "TRACKED_DIRTY_AFTER=$($dirtyAfter.Count)"
$dirtyAfter | ForEach-Object { Write-Host "DIRTY_AFTER=$_" }
if ($dirtyAfter.Count -ne $expectedDirty.Count) { throw "A14_NB1C1_DIRTY_COUNT_AFTER_INVALID=$($dirtyAfter.Count)" }
for ($i = 0; $i -lt $expectedDirty.Count; ++$i) {
    if ($dirtyAfter[$i] -ne $expectedDirty[$i]) { throw "A14_NB1C1_DIRTY_PATH_AFTER_INVALID=$($dirtyAfter[$i])" }
}

$stagedAfter = @(& git -C $script:G2RepoRoot diff --cached --name-only)
if ($LASTEXITCODE -ne 0 -or $stagedAfter.Count -ne 0) { throw "A14_NB1C1_INDEX_NOT_CLEAN_AFTER" }
Write-Host "STAGED_COUNT_AFTER=0"

$firmwareNumstat = @(& git -C $script:G2RepoRoot diff --numstat -- $firmwareRelative)
Write-Host "FIRMWARE_DIFF_NUMSTAT=$($firmwareNumstat -join ';')"
Write-Host "FIRMWARE_SHA256_AFTER=$(Get-G2Sha256 $firmwareRelative)"
Write-Host "HEADER_SHA256_FINAL=$(Get-G2Sha256 $headerRelative)"
Write-Host "CPP_SHA256_FINAL=$(Get-G2Sha256 $cppRelative)"
Write-Host "RAW_RUNNER_SHA256_FINAL=$(Get-G2Sha256 $script:G2RawRunnerRelative)"

if ((Get-G2Sha256 $headerRelative) -ne $expectedHeaderSha256) { throw "A14_NB1C1_HEADER_CHANGED" }
if ((Get-G2Sha256 $cppRelative) -ne $expectedCppSha256) { throw "A14_NB1C1_CPP_CHANGED" }
if ((Get-G2Sha256 $script:G2RawRunnerRelative) -ne $script:G2RawRunnerSha256) { throw "A14_NB1C1_RUNNER_CHANGED" }
Assert-G2ProtectedArtifacts

Write-Host "NB1_RAW_TCP_STOP_MODE=ASYNC"
Write-Host "NB1_CONNECTION_TIMEOUT_MS=1000_UNCHANGED"
Write-Host "NB1_SOURCE_MUTATION=RAW_BENCHMARK_ONLY"
Write-Host "A14_NB1_APPLY_RAW_ASYNC_STOP=PASS"
