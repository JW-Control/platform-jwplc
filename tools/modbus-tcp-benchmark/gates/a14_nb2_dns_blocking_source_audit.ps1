param()

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"
. (Join-Path $PSScriptRoot "common.ps1")

Write-Host "============================================================"
Write-Host " A14 NB2-A - DNS BLOCKING SOURCE AUDIT"
Write-Host "============================================================"

Assert-G2Branch

$dnsHeaderRelative = "JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/Dns.h"
$dnsCppRelative = "JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/Dns.cpp"
$clientCppRelative = "JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/EthernetClient.cpp"

$dirty = @(Get-G2TrackedDirtyPaths)
$expectedDirty = @(
    "JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/JWPLC_W5x00_Ethernet.h",
    "JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/EthernetClient.cpp",
    $script:G2SpiHeaderRelative,
    $script:G2RawFirmwareRelative
) | Sort-Object

Write-Host "HEAD=$(Get-G2Head)"
Write-Host "TRACKED_DIRTY_COUNT=$($dirty.Count)"
$dirty | ForEach-Object { Write-Host "DIRTY=$_" }

if ($dirty.Count -ne $expectedDirty.Count) {
    throw "A14_NB2A_DIRTY_COUNT_INVALID=$($dirty.Count)"
}
for ($i = 0; $i -lt $expectedDirty.Count; ++$i) {
    if ($dirty[$i] -ne $expectedDirty[$i]) {
        throw "A14_NB2A_DIRTY_PATH_INVALID=$($dirty[$i])"
    }
}

$staged = @(& git -C $script:G2RepoRoot diff --cached --name-only)
if ($LASTEXITCODE -ne 0 -or $staged.Count -ne 0) {
    throw "A14_NB2A_INDEX_NOT_CLEAN"
}
Write-Host "STAGED_COUNT=0"

Assert-G2ProtectedArtifacts

foreach ($relative in @($dnsHeaderRelative, $dnsCppRelative)) {
    & git -C $script:G2RepoRoot diff --quiet -- $relative
    if ($LASTEXITCODE -ne 0) {
        throw "A14_NB2A_DNS_SOURCE_DIRTY=$relative"
    }
}
Write-Host "DNS_SOURCE_TRACKED_CLEAN=YES"

$dnsHeaderText = [System.IO.File]::ReadAllText((Get-G2Path $dnsHeaderRelative))
$dnsCppText = [System.IO.File]::ReadAllText((Get-G2Path $dnsCppRelative))
$clientCppText = [System.IO.File]::ReadAllText((Get-G2Path $clientCppRelative))

function Count-Regex {
    param(
        [string]$Text,
        [string]$Pattern
    )
    return ([regex]::Matches($Text, $Pattern)).Count
}

$defaultDnsTimeoutCount = Count-Regex $dnsHeaderText 'timeout\s*=\s*5000'
$processResponseDelay50Count = Count-Regex $dnsCppText 'delay\s*\(\s*50\s*\)'
$processResponseWaitLoopCount = Count-Regex $dnsCppText 'while\s*\(\s*iUdp\.parsePacket\(\)\s*<=\s*0\s*\)'
$waitRetryLimitCount = Count-Regex $dnsCppText 'wait_retries\s*<\s*3'
$processResponseCallCount = Count-Regex $dnsCppText 'ProcessResponse\s*\(\s*timeout\s*,\s*aResult\s*\)'
$dnsStopCount = Count-Regex $dnsCppText 'iUdp\.stop\s*\(\s*\)'
$clientHostnameDnsCallCount = Count-Regex $clientCppText 'dns\.getHostByName\s*\(\s*host\s*,\s*remote_addr\s*\)'
$clientHostnameDnsTimeoutForwardCount = Count-Regex $clientCppText 'dns\.getHostByName\s*\(\s*host\s*,\s*remote_addr\s*,'
$clientHostnameTodoTimeoutCount = Count-Regex $clientCppText 'TODO:\s*use\s+_timeout'

Write-Host "DNS_DEFAULT_TIMEOUT_5000_COUNT=$defaultDnsTimeoutCount"
Write-Host "DNS_PROCESS_RESPONSE_DELAY_50_COUNT=$processResponseDelay50Count"
Write-Host "DNS_PROCESS_RESPONSE_WAIT_LOOP_COUNT=$processResponseWaitLoopCount"
Write-Host "DNS_WAIT_RETRY_LIMIT_3_COUNT=$waitRetryLimitCount"
Write-Host "DNS_PROCESS_RESPONSE_CALL_COUNT=$processResponseCallCount"
Write-Host "DNS_UDP_STOP_COUNT=$dnsStopCount"
Write-Host "CLIENT_HOSTNAME_DNS_CALL_COUNT=$clientHostnameDnsCallCount"
Write-Host "CLIENT_HOSTNAME_DNS_TIMEOUT_FORWARD_COUNT=$clientHostnameDnsTimeoutForwardCount"
Write-Host "CLIENT_HOSTNAME_TODO_USE_TIMEOUT_COUNT=$clientHostnameTodoTimeoutCount"

if ($defaultDnsTimeoutCount -ne 1) {
    throw "A14_NB2A_DEFAULT_DNS_TIMEOUT_NOT_FOUND"
}
if ($processResponseDelay50Count -ne 1) {
    throw "A14_NB2A_DELAY50_COUNT_INVALID=$processResponseDelay50Count"
}
if ($processResponseWaitLoopCount -ne 1) {
    throw "A14_NB2A_WAIT_LOOP_COUNT_INVALID=$processResponseWaitLoopCount"
}
if ($waitRetryLimitCount -ne 1) {
    throw "A14_NB2A_RETRY_LIMIT_COUNT_INVALID=$waitRetryLimitCount"
}
if ($processResponseCallCount -ne 1) {
    throw "A14_NB2A_PROCESS_RESPONSE_CALL_COUNT_INVALID=$processResponseCallCount"
}
if ($clientHostnameDnsCallCount -ne 1) {
    throw "A14_NB2A_CLIENT_DNS_CALL_COUNT_INVALID=$clientHostnameDnsCallCount"
}
if ($clientHostnameDnsTimeoutForwardCount -ne 0) {
    throw "A14_NB2A_CLIENT_ALREADY_FORWARDS_TIMEOUT"
}
if ($clientHostnameTodoTimeoutCount -lt 1) {
    throw "A14_NB2A_CLIENT_TIMEOUT_TODO_NOT_FOUND"
}

$defaultDnsTimeoutMs = 5000
$maxWaitAttempts = 3
$nominalWorstWaitMs = $defaultDnsTimeoutMs * $maxWaitAttempts

Write-Host "DNS_DEFAULT_TIMEOUT_MS=$defaultDnsTimeoutMs"
Write-Host "DNS_WAIT_ATTEMPTS_MAX=$maxWaitAttempts"
Write-Host "DNS_NOMINAL_WORST_WAIT_MS=$nominalWorstWaitMs"
Write-Host "DNS_POLL_SLEEP_MS=50"
Write-Host "CLIENT_CONNECTION_TIMEOUT_CONTROLS_DNS=NO"

Assert-G2ProtectedArtifacts

$dirtyFinal = @(Get-G2TrackedDirtyPaths)
$stagedFinal = @(& git -C $script:G2RepoRoot diff --cached --name-only)
Write-Host "TRACKED_DIRTY_COUNT_FINAL=$($dirtyFinal.Count)"
Write-Host "STAGED_COUNT_FINAL=$($stagedFinal.Count)"

if ($dirtyFinal.Count -ne $expectedDirty.Count -or $stagedFinal.Count -ne 0) {
    throw "A14_NB2A_FINAL_WORKTREE_INVALID"
}

Write-Host "NB2_DNS_BLOCKING_WAIT=CONFIRMED_BY_SOURCE"
Write-Host "NB2_DNS_DEFAULT_TIMEOUT_MS=5000"
Write-Host "NB2_DNS_MAX_WAIT_ATTEMPTS=3"
Write-Host "NB2_DNS_NOMINAL_WORST_WAIT_MS=15000"
Write-Host "NB2_CLIENT_TIMEOUT_NOT_FORWARDED_TO_DNS=CONFIRMED"
Write-Host "A14_NB2_DNS_BLOCKING_SOURCE_AUDIT=PASS"
