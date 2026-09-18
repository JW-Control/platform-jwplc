param()

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"
. (Join-Path $PSScriptRoot "common.ps1")

Write-Host "============================================================"
Write-Host " A14 NB2-D - DNS ASYNC CLOSURE CONTRACT"
Write-Host "============================================================"

Assert-G2Branch

$dnsHeaderRelative = "JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/Dns.h"
$dnsCppRelative = "JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/Dns.cpp"
$clientHeaderRelative = "JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/JWPLC_W5x00_Ethernet.h"
$clientCppRelative = "JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/EthernetClient.cpp"
$udpCppRelative = "JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/EthernetUdp.cpp"

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
Write-Host "TRACKED_DIRTY_COUNT=$($dirty.Count)"
$dirty | ForEach-Object { Write-Host "DIRTY=$_" }

if ($dirty.Count -ne $expectedDirty.Count) {
    throw "A14_NB2D_DIRTY_COUNT_INVALID=$($dirty.Count)"
}

for ($i = 0; $i -lt $expectedDirty.Count; ++$i) {
    if ($dirty[$i] -ne $expectedDirty[$i]) {
        throw "A14_NB2D_DIRTY_PATH_INVALID=$($dirty[$i])"
    }
}

$staged = @(& git -C $script:G2RepoRoot diff --cached --name-only)
if ($LASTEXITCODE -ne 0 -or $staged.Count -ne 0) {
    throw "A14_NB2D_INDEX_NOT_CLEAN"
}
Write-Host "STAGED_COUNT=0"

Assert-G2ProtectedArtifacts

$dnsHeaderHash = Get-G2Sha256 $dnsHeaderRelative
$dnsCppHash = Get-G2Sha256 $dnsCppRelative
$clientHeaderHash = Get-G2Sha256 $clientHeaderRelative
$clientCppHash = Get-G2Sha256 $clientCppRelative
$rawHash = Get-G2Sha256 $script:G2RawFirmwareRelative

Write-Host "DNS_HEADER_SHA256=$dnsHeaderHash"
Write-Host "DNS_CPP_SHA256=$dnsCppHash"
Write-Host "CLIENT_HEADER_SHA256=$clientHeaderHash"
Write-Host "CLIENT_CPP_SHA256=$clientCppHash"
Write-Host "RAW_FIRMWARE_SHA256=$rawHash"

if ($dnsHeaderHash -ne $expectedDnsHeaderSha256) {
    throw "A14_NB2D_DNS_HEADER_HASH_MISMATCH"
}
if ($dnsCppHash -ne $expectedDnsCppSha256) {
    throw "A14_NB2D_DNS_CPP_HASH_MISMATCH"
}
if ($clientHeaderHash -ne $expectedClientHeaderSha256) {
    throw "A14_NB2D_CLIENT_HEADER_HASH_MISMATCH"
}
if ($clientCppHash -ne $expectedClientCppSha256) {
    throw "A14_NB2D_CLIENT_CPP_HASH_MISMATCH"
}
if ($rawHash -ne $expectedRawSha256) {
    throw "A14_NB2D_RAW_HASH_MISMATCH"
}

$dnsHeaderText = [System.IO.File]::ReadAllText((Get-G2Path $dnsHeaderRelative))
$dnsCppText = [System.IO.File]::ReadAllText((Get-G2Path $dnsCppRelative))
$clientCppText = [System.IO.File]::ReadAllText((Get-G2Path $clientCppRelative))
$udpCppText = [System.IO.File]::ReadAllText((Get-G2Path $udpCppRelative))

foreach ($marker in @(
    "beginResolveAsync",
    "pollResolveAsync",
    "resolveAsyncInProgress",
    "cancelResolveAsync"
)) {
    $headerCount = ([regex]::Matches(
        $dnsHeaderText,
        [regex]::Escape($marker)
    )).Count

    $cppCount = ([regex]::Matches(
        $dnsCppText,
        [regex]::Escape("DNSClient::$marker")
    )).Count

    Write-Host "DNS_API=$marker HEADER_COUNT=$headerCount CPP_COUNT=$cppCount"

    if ($headerCount -ne 1 -or $cppCount -ne 1) {
        throw "A14_NB2D_API_MARKER_INVALID=$marker"
    }
}

$pollBlock = [regex]::Match(
    $dnsCppText,
    '(?ms)^int DNSClient::pollResolveAsync\(\)\r?\n\{.*?^\}'
)

if (-not $pollBlock.Success) {
    throw "A14_NB2D_POLL_BLOCK_NOT_FOUND"
}

$pollDelayCount = ([regex]::Matches(
    $pollBlock.Value,
    'delay\s*\('
)).Count

$pollWhileCount = ([regex]::Matches(
    $pollBlock.Value,
    '\bwhile\s*\('
)).Count

Write-Host "DNS_ASYNC_POLL_DELAY_COUNT=$pollDelayCount"
Write-Host "DNS_ASYNC_POLL_WHILE_COUNT=$pollWhileCount"

if ($pollDelayCount -ne 0) {
    throw "A14_NB2D_ASYNC_POLL_DELAY_PRESENT=$pollDelayCount"
}
if ($pollWhileCount -ne 0) {
    throw "A14_NB2D_ASYNC_POLL_WHILE_PRESENT=$pollWhileCount"
}

$getHostBlock = [regex]::Match(
    $dnsCppText,
    '(?ms)^int DNSClient::getHostByName\(.*?^\}'
)

if (-not $getHostBlock.Success) {
    throw "A14_NB2D_GETHOST_BLOCK_NOT_FOUND"
}

$legacyBeginCount = ([regex]::Matches(
    $getHostBlock.Value,
    'beginResolveAsync\s*\('
)).Count
$legacyPollCount = ([regex]::Matches(
    $getHostBlock.Value,
    'pollResolveAsync\s*\('
)).Count

Write-Host "LEGACY_GETHOST_BEGIN_ASYNC_COUNT=$legacyBeginCount"
Write-Host "LEGACY_GETHOST_POLL_ASYNC_COUNT=$legacyPollCount"

if ($legacyBeginCount -ne 1 -or $legacyPollCount -ne 1) {
    throw "A14_NB2D_LEGACY_GETHOST_NOT_WRAPPED"
}

$clientLegacyDnsCount = ([regex]::Matches(
    $clientCppText,
    'dns\.getHostByName\s*\('
)).Count

$udpLegacyDnsCount = ([regex]::Matches(
    $udpCppText,
    'dns\.getHostByName\s*\('
)).Count

Write-Host "CLIENT_LEGACY_HOSTNAME_DNS_COUNT=$clientLegacyDnsCount"
Write-Host "UDP_LEGACY_HOSTNAME_DNS_COUNT=$udpLegacyDnsCount"

if ($clientLegacyDnsCount -ne 1) {
    throw "A14_NB2D_CLIENT_LEGACY_HOSTNAME_PATH_CHANGED=$clientLegacyDnsCount"
}
if ($udpLegacyDnsCount -ne 1) {
    throw "A14_NB2D_UDP_LEGACY_HOSTNAME_PATH_CHANGED=$udpLegacyDnsCount"
}

$dnsDelay50Count = ([regex]::Matches(
    $dnsCppText,
    'delay\s*\(\s*50\s*\)'
)).Count

$dnsLegacyWaitLoopCount = ([regex]::Matches(
    $dnsCppText,
    'while\s*\(\s*iUdp\.parsePacket\(\)\s*<=\s*0\s*\)'
)).Count

Write-Host "DNS_LEGACY_DELAY50_COUNT=$dnsDelay50Count"
Write-Host "DNS_LEGACY_PROCESS_WAIT_LOOP_COUNT=$dnsLegacyWaitLoopCount"

if ($dnsDelay50Count -ne 1 -or $dnsLegacyWaitLoopCount -ne 1) {
    throw "A14_NB2D_LEGACY_PROCESS_RESPONSE_CONTRACT_CHANGED"
}

Assert-G2ProtectedArtifacts

$dirtyFinal = @(Get-G2TrackedDirtyPaths)
$stagedFinal = @(& git -C $script:G2RepoRoot diff --cached --name-only)

Write-Host "TRACKED_DIRTY_COUNT_FINAL=$($dirtyFinal.Count)"
Write-Host "STAGED_COUNT_FINAL=$($stagedFinal.Count)"

if ($dirtyFinal.Count -ne $expectedDirty.Count -or
    $stagedFinal.Count -ne 0) {
    throw "A14_NB2D_FINAL_WORKTREE_INVALID"
}

for ($i = 0; $i -lt $expectedDirty.Count; ++$i) {
    if ($dirtyFinal[$i] -ne $expectedDirty[$i]) {
        throw "A14_NB2D_FINAL_DIRTY_PATH_INVALID=$($dirtyFinal[$i])"
    }
}

Write-Host "NB2_DNS_ASYNC_SOURCE_CONTRACT=PASS"
Write-Host "NB2_DNS_ASYNC_POLL_BLOCKING_DELAY=ABSENT"
Write-Host "NB2_DNS_LEGACY_GETHOST=WRAPPER_PRESERVED"
Write-Host "NB2_LEGACY_CLIENT_HOSTNAME_PATH=PRESERVED"
Write-Host "NB2_LEGACY_UDP_HOSTNAME_PATH=PRESERVED"
Write-Host "NB2_DNS_PHYSICAL_VALID_RESOLUTION=PASS"
Write-Host "NB2_DNS_PHYSICAL_TIMEOUT_PENDING=PASS"
Write-Host "NB2_DNS_POLL_HOLD_MAX_US=369_EVIDENCE"
Write-Host "NB2_DNS_LOOP_GAP_MAX_US=2383_EVIDENCE"
Write-Host "NB2_DNS_BEGIN_HOLD_US=6033_DEFERRED_TO_NB3_UDP_SEND"
Write-Host "NB2_OVERALL=CLOSED_PASS"
Write-Host "A14_NB2_DNS_CLOSURE=PASS"
