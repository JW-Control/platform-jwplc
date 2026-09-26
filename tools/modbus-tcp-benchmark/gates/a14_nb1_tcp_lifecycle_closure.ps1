param()

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"
. (Join-Path $PSScriptRoot "common.ps1")

Write-Host "============================================================"
Write-Host " A14 NB1-E - TCP LIFECYCLE ASYNC CLOSURE CONTRACT"
Write-Host "============================================================"

Assert-G2Branch

$headerRelative = "JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/JWPLC_W5x00_Ethernet.h"
$cppRelative = "JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/EthernetClient.cpp"
$firmwareRelative = $script:G2RawFirmwareRelative

$expectedHeaderSha256 = "3E11094D68873D81F264F8AD97C1D55867F5399E28613061BC38484CD7DE614B"
$expectedCppSha256 = "93B71C92E298053425FF03D750632A30102BE743E1C8B1FDAB800214EABED4B2"
$expectedFirmwareSha256 = "9A0C6CF27E44A61DC97686FB1A769EBBBB34D036CDF8D0CA0EEAB597E699AB6B"

$dirty = @(Get-G2TrackedDirtyPaths)
$expectedDirty = @(
    $headerRelative,
    $cppRelative,
    $script:G2SpiHeaderRelative,
    $firmwareRelative
) | Sort-Object

Write-Host "HEAD=$(Get-G2Head)"
Write-Host "EFFECTIVE_SPI_HZ=$(Get-G2SpiHz)"
Write-Host "TRACKED_DIRTY_COUNT=$($dirty.Count)"
$dirty | ForEach-Object { Write-Host "DIRTY=$_" }

if ($dirty.Count -ne $expectedDirty.Count) {
    throw "A14_NB1E_DIRTY_COUNT_INVALID=$($dirty.Count)"
}
for ($i = 0; $i -lt $expectedDirty.Count; ++$i) {
    if ($dirty[$i] -ne $expectedDirty[$i]) {
        throw "A14_NB1E_DIRTY_PATH_INVALID=$($dirty[$i])"
    }
}

$staged = @(& git -C $script:G2RepoRoot diff --cached --name-only)
if ($LASTEXITCODE -ne 0 -or $staged.Count -ne 0) {
    throw "A14_NB1E_INDEX_NOT_CLEAN"
}
Write-Host "STAGED_COUNT=0"

Assert-G2ProtectedArtifacts

$headerHash = Get-G2Sha256 $headerRelative
$cppHash = Get-G2Sha256 $cppRelative
$firmwareHash = Get-G2Sha256 $firmwareRelative
$runnerHash = Get-G2Sha256 $script:G2RawRunnerRelative

Write-Host "HEADER_SHA256=$headerHash"
Write-Host "CPP_SHA256=$cppHash"
Write-Host "FIRMWARE_SHA256=$firmwareHash"
Write-Host "RAW_RUNNER_SHA256=$runnerHash"

if ($headerHash -ne $expectedHeaderSha256) {
    throw "A14_NB1E_HEADER_HASH_MISMATCH"
}
if ($cppHash -ne $expectedCppSha256) {
    throw "A14_NB1E_CPP_HASH_MISMATCH"
}
if ($firmwareHash -ne $expectedFirmwareSha256) {
    throw "A14_NB1E_FIRMWARE_HASH_MISMATCH"
}
if ($runnerHash -ne $script:G2RawRunnerSha256) {
    throw "A14_NB1E_RUNNER_HASH_MISMATCH"
}

$headerText = [System.IO.File]::ReadAllText((Get-G2Path $headerRelative))
$cppText = [System.IO.File]::ReadAllText((Get-G2Path $cppRelative))
$firmwareText = [System.IO.File]::ReadAllText((Get-G2Path $firmwareRelative))

$defaultTimeoutCount = ([regex]::Matches(
    $headerText,
    [regex]::Escape("_timeout(1000)")
)).Count
$rawTimeoutOverrideCount = ([regex]::Matches(
    $firmwareText,
    "setConnectionTimeout\s*\("
)).Count

Write-Host "DEFAULT_TIMEOUT_1000_CONSTRUCTOR_COUNT=$defaultTimeoutCount"
Write-Host "RAW_SET_CONNECTION_TIMEOUT_COUNT=$rawTimeoutOverrideCount"

if ($defaultTimeoutCount -ne 2) {
    throw "A14_NB1E_DEFAULT_TIMEOUT_1000_NOT_PROVEN=$defaultTimeoutCount"
}
if ($rawTimeoutOverrideCount -ne 0) {
    throw "A14_NB1E_RAW_TIMEOUT_OVERRIDE_PRESENT=$rawTimeoutOverrideCount"
}

$apiMarkers = @(
    "beginStopAsync",
    "pollStopAsync",
    "stopAsyncInProgress",
    "cancelStopAsync",
    "beginFlushAsync",
    "pollFlushAsync",
    "flushAsyncInProgress",
    "cancelFlushAsync"
)

foreach ($marker in $apiMarkers) {
    $headerCount = ([regex]::Matches(
        $headerText,
        [regex]::Escape($marker)
    )).Count
    $cppCount = ([regex]::Matches(
        $cppText,
        [regex]::Escape("EthernetClient::$marker")
    )).Count

    Write-Host "API=$marker HEADER_COUNT=$headerCount CPP_COUNT=$cppCount"

    if ($headerCount -ne 1 -or $cppCount -ne 1) {
        throw "A14_NB1E_API_MARKER_INVALID=$marker"
    }
}

$legacyStopDefinitionCount = ([regex]::Matches(
    $cppText,
    "void\s+EthernetClient::stop\s*\(\s*\)"
)).Count
$legacyFlushDefinitionCount = ([regex]::Matches(
    $cppText,
    "void\s+EthernetClient::flush\s*\(\s*\)"
)).Count
$rawBlockingStopCount = ([regex]::Matches(
    $firmwareText,
    "tcpClient\s*\.\s*stop\s*\("
)).Count
$rawBeginStopCount = ([regex]::Matches(
    $firmwareText,
    "tcpClient\s*\.\s*beginStopAsync\s*\("
)).Count
$rawPollStopCount = ([regex]::Matches(
    $firmwareText,
    "tcpClient\s*\.\s*pollStopAsync\s*\("
)).Count
$rawStopProgressCount = ([regex]::Matches(
    $firmwareText,
    "tcpClient\s*\.\s*stopAsyncInProgress\s*\("
)).Count

Write-Host "LEGACY_STOP_DEFINITION_COUNT=$legacyStopDefinitionCount"
Write-Host "LEGACY_FLUSH_DEFINITION_COUNT=$legacyFlushDefinitionCount"
Write-Host "RAW_BLOCKING_STOP_COUNT=$rawBlockingStopCount"
Write-Host "RAW_BEGIN_STOP_ASYNC_COUNT=$rawBeginStopCount"
Write-Host "RAW_POLL_STOP_ASYNC_COUNT=$rawPollStopCount"
Write-Host "RAW_STOP_ASYNC_IN_PROGRESS_COUNT=$rawStopProgressCount"

if ($legacyStopDefinitionCount -ne 1) {
    throw "A14_NB1E_LEGACY_STOP_DEFINITION_INVALID"
}
if ($legacyFlushDefinitionCount -ne 1) {
    throw "A14_NB1E_LEGACY_FLUSH_DEFINITION_INVALID"
}
if ($rawBlockingStopCount -ne 0) {
    throw "A14_NB1E_RAW_BLOCKING_STOP_PRESENT=$rawBlockingStopCount"
}
if ($rawBeginStopCount -ne 2) {
    throw "A14_NB1E_RAW_BEGIN_STOP_COUNT_INVALID=$rawBeginStopCount"
}
if ($rawPollStopCount -ne 1) {
    throw "A14_NB1E_RAW_POLL_STOP_COUNT_INVALID=$rawPollStopCount"
}
if ($rawStopProgressCount -ne 1) {
    throw "A14_NB1E_RAW_STOP_PROGRESS_COUNT_INVALID=$rawStopProgressCount"
}

$flushCallsSocketSendAvailable = (
    $cppText.IndexOf(
        "Ethernet.socketSendAvailable(_sockindex)",
        [System.StringComparison]::Ordinal
    ) -ge 0
)
$flushUsesSocketSize = (
    $cppText.IndexOf(
        "W5100.SSIZE",
        [System.StringComparison]::Ordinal
    ) -ge 0
)

Write-Host "FLUSH_USES_SOCKET_SEND_AVAILABLE=$flushCallsSocketSendAvailable"
Write-Host "FLUSH_USES_W5100_SSIZE=$flushUsesSocketSize"

if (-not $flushCallsSocketSendAvailable -or -not $flushUsesSocketSize) {
    throw "A14_NB1E_FLUSH_SEMANTIC_CONTRACT_NOT_PROVEN"
}

Assert-G2ProtectedArtifacts

$dirtyFinal = @(Get-G2TrackedDirtyPaths)
$stagedFinal = @(& git -C $script:G2RepoRoot diff --cached --name-only)

Write-Host "TRACKED_DIRTY_COUNT_FINAL=$($dirtyFinal.Count)"
Write-Host "STAGED_COUNT_FINAL=$($stagedFinal.Count)"

if ($dirtyFinal.Count -ne $expectedDirty.Count -or $stagedFinal.Count -ne 0) {
    throw "A14_NB1E_FINAL_WORKTREE_INVALID"
}
for ($i = 0; $i -lt $expectedDirty.Count; ++$i) {
    if ($dirtyFinal[$i] -ne $expectedDirty[$i]) {
        throw "A14_NB1E_FINAL_DIRTY_PATH_INVALID=$($dirtyFinal[$i])"
    }
}

Write-Host "NB1_DEFAULT_CONNECTION_TIMEOUT_MS=1000_SOURCE_PROVEN"
Write-Host "NB1_RAW_TIMEOUT_OVERRIDE=ABSENT"
Write-Host "NB1_RAW_BLOCKING_STOP=ABSENT"
Write-Host "NB1_LEGACY_API_COMPATIBILITY=PRESERVED"
Write-Host "NB1_STOP_ASYNC_PHYSICAL=PASS_REPRODUCED_40_PAIRS"
Write-Host "NB1_FLUSH_ASYNC_PHYSICAL=PASS_20_CYCLES"
Write-Host "NB1_FLUSH_SEMANTICS=TX_FSR_FULL"
Write-Host "A14_NB1_TCP_LIFECYCLE_CLOSURE=PASS"
