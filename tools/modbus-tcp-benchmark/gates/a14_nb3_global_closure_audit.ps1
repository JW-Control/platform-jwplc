param()

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"
. (Join-Path $PSScriptRoot "common.ps1")

function Get-CppFunctionBlock {
    param(
        [Parameter(Mandatory = $true)][string]$Text,
        [Parameter(Mandatory = $true)][string]$SignaturePrefix
    )

    $searchOffset = 0

    while ($true) {
        $candidate = $Text.IndexOf(
            $SignaturePrefix,
            $searchOffset,
            [System.StringComparison]::Ordinal)

        if ($candidate -lt 0) { break }

        $cursor = $candidate + $SignaturePrefix.Length
        $closeParen = $Text.IndexOf(")", $cursor, [System.StringComparison]::Ordinal)
        $braceStart = $Text.IndexOf("{", $cursor, [System.StringComparison]::Ordinal)
        $semicolon = $Text.IndexOf(";", $cursor, [System.StringComparison]::Ordinal)

        $definition = (
            $closeParen -ge 0 -and
            $braceStart -gt $closeParen -and
            ($semicolon -lt 0 -or $braceStart -lt $semicolon)
        )

        if ($definition) {
            $depth = 0

            for ($i = $braceStart; $i -lt $Text.Length; ++$i) {
                if ($Text[$i] -eq "{") {
                    ++$depth
                }
                elseif ($Text[$i] -eq "}") {
                    --$depth

                    if ($depth -eq 0) {
                        return $Text.Substring(
                            $candidate,
                            $i - $candidate + 1)
                    }
                }
            }

            throw "A14_NB3_CLOSE_FUNCTION_END_NOT_FOUND=$SignaturePrefix"
        }

        $searchOffset = $candidate + $SignaturePrefix.Length
    }

    throw "A14_NB3_CLOSE_FUNCTION_DEFINITION_NOT_FOUND=$SignaturePrefix"
}

function Count-Regex {
    param(
        [Parameter(Mandatory = $true)][string]$Text,
        [Parameter(Mandatory = $true)][string]$Pattern
    )

    return ([regex]::Matches($Text, $Pattern)).Count
}

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

Write-Host "============================================================"
Write-Host " A14 NB3-G - GLOBAL CLOSURE AUDIT"
Write-Host "============================================================"

Assert-G2Branch

Write-Host "HEAD=$(Get-G2Head)"
Write-Host "EFFECTIVE_SPI_HZ=$(Get-G2SpiHz)"
Write-Host "UPLOAD=NO"
Write-Host "SOURCE_MUTATION=NO"

if ((Get-G2SpiHz) -ne 26000000) {
    throw "A14_NB3_CLOSE_SPI_FREQUENCY_MISMATCH"
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
    throw "A14_NB3_CLOSE_DIRTY_COUNT_INVALID=$($dirty.Count)"
}

for ($i = 0; $i -lt $expectedDirty.Count; ++$i) {
    if ($dirty[$i] -ne $expectedDirty[$i]) {
        throw "A14_NB3_CLOSE_DIRTY_PATH_INVALID=$($dirty[$i])"
    }
}

$staged = @(& git -C $script:G2RepoRoot diff --cached --name-only)
if ($LASTEXITCODE -ne 0 -or $staged.Count -ne 0) {
    throw "A14_NB3_CLOSE_INDEX_NOT_CLEAN"
}
Write-Host "STAGED_COUNT=0"

Assert-G2ProtectedArtifacts

foreach ($entry in $expectedHashes.GetEnumerator()) {
    $actual = Get-G2Sha256 $entry.Key
    Write-Host "CANDIDATE_SHA256=$($entry.Key)=$actual"

    if ($actual -ne $entry.Value) {
        throw "A14_NB3_CLOSE_HASH_MISMATCH=$($entry.Key)"
    }
}

$socketText = [System.IO.File]::ReadAllText(
    (Get-G2Path "JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/socket.cpp"))
$udpText = [System.IO.File]::ReadAllText(
    (Get-G2Path "JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/EthernetUdp.cpp"))
$w5100Text = [System.IO.File]::ReadAllText(
    (Get-G2Path "JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/utility/w5100.cpp"))
$asyncTxText = [System.IO.File]::ReadAllText(
    (Get-G2Path "JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/jwplc_ethernet_async_tx.cpp"))
$dnsText = [System.IO.File]::ReadAllText(
    (Get-G2Path "JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/Dns.cpp"))

$tcpSend = Get-CppFunctionBlock -Text $socketText -SignaturePrefix "uint16_t EthernetClass::socketSend("
$udpSend = Get-CppFunctionBlock -Text $socketText -SignaturePrefix "bool EthernetClass::socketSendUDP("
$udpBegin = Get-CppFunctionBlock -Text $socketText -SignaturePrefix "int EthernetClass::socketBeginSendUDP("
$udpPoll = Get-CppFunctionBlock -Text $socketText -SignaturePrefix "int EthernetClass::socketPollSendUDP("
$parsePacket = Get-CppFunctionBlock -Text $udpText -SignaturePrefix "int EthernetUDP::parsePacket("
$udpLegacyEndPacket = Get-CppFunctionBlock -Text $udpText -SignaturePrefix "int EthernetUDP::endPacket("
$w5100ExecLegacy = Get-CppFunctionBlock -Text $w5100Text -SignaturePrefix "void W5100Class::execCmdSn("
$w5100ExecChecked = Get-CppFunctionBlock -Text $w5100Text -SignaturePrefix "bool W5100Class::execCmdSnChecked("
$w5100TxStable = Get-CppFunctionBlock -Text $w5100Text -SignaturePrefix "bool W5100Class::readSnTX_FSRStable("
$w5100RxStable = Get-CppFunctionBlock -Text $w5100Text -SignaturePrefix "bool W5100Class::readSnRX_RSRStable("
$asyncStable = Get-CppFunctionBlock -Text $asyncTxText -SignaturePrefix "bool JWPLC_EthernetAsyncTx::readTxFreeStable("
$dnsBegin = Get-CppFunctionBlock -Text $dnsText -SignaturePrefix "int DNSClient::beginResolveAsync("
$dnsPoll = Get-CppFunctionBlock -Text $dnsText -SignaturePrefix "int DNSClient::pollResolveAsync("

$socketWhile1 = Count-Regex -Text $socketText -Pattern 'while\s*\(\s*1\s*\)'
$socketRxStableCall = Count-Regex -Text $socketText -Pattern 'W5100\.readSnRX_RSRStable\(s,\s*value\)'
$socketTxStableCall = Count-Regex -Text $socketText -Pattern 'W5100\.readSnTX_FSRStable\(s,\s*value\)'
$tcpOldFree = Count-Regex -Text $tcpSend -Pattern 'while\s*\(\s*freesize\s*<\s*ret\s*\)'
$tcpOldSendOk = Count-Regex -Text $tcpSend -Pattern 'while\s*\(\s*\(W5100\.readSnIR\(s\).*SEND_OK'
$tcpTimeoutLoops = Count-Regex -Text $tcpSend -Pattern 'millis\(\)\s*-\s*startedMs\)\s*<\s*timeoutMs'
$udpDirectSendOk = Count-Regex -Text $udpSend -Pattern 'W5100\.readSnIR\(s\).*SEND_OK'
$udpWrapperPoll = Count-Regex -Text $udpSend -Pattern 'socketPollSendUDP\(s\)'
$udpBeginChecked = Count-Regex -Text $udpBegin -Pattern 'execCmdSnChecked\(\s*s,\s*Sock_SEND,\s*1000\s*\)'
$udpPollSendOk = Count-Regex -Text $udpPoll -Pattern 'SnIR::SEND_OK'
$udpPollTimeout = Count-Regex -Text $udpPoll -Pattern 'SnIR::TIMEOUT'
$parseRemainingWhile = Count-Regex -Text $parsePacket -Pattern 'while\s*\(\s*_remaining\s*\)'
$parseDrainGuard = Count-Regex -Text $parsePacket -Pattern 'const\s+int\s+drained\s*=\s*read'
$legacyExecUnbounded = Count-Regex -Text $w5100ExecLegacy -Pattern 'while\s*\(\s*readSnCR\(s\)\s*\)'
$legacyExecDelegatesChecked = Count-Regex -Text $w5100ExecLegacy -Pattern 'execCmdSnChecked'
$checkedExecDeadline = Count-Regex -Text $w5100ExecChecked -Pattern 'timeoutUs'
$asyncWhileTrue = Count-Regex -Text $asyncStable -Pattern 'while\s*\(\s*true\s*\)'
$w5100TxStableBound = Count-Regex -Text $w5100TxStable -Pattern 'for\s*\([^;]+;\s*i\s*<\s*maxComparisons\s*;'
$w5100RxStableBound = Count-Regex -Text $w5100RxStable -Pattern 'for\s*\([^;]+;\s*i\s*<\s*maxComparisons\s*;'
$asyncStableDelegate = Count-Regex -Text $asyncStable -Pattern 'W5100\.readSnTX_FSRStable\(socket,\s*value\)'
$dnsSyncEndPacket = Count-Regex -Text $dnsBegin -Pattern '\.endPacket\('
$dnsAsyncBeginUdp = Count-Regex -Text $dnsBegin -Pattern 'beginEndPacketAsync\('
$dnsAsyncPollUdp = Count-Regex -Text $dnsPoll -Pattern 'pollEndPacketAsync\('

Write-Host ""
Write-Host "=== ORIGINAL NB3 RISK INVENTORY - CURRENT STATE ==="
Write-Host "NB3_CLOSE_SOCKET_STABLE_WHILE1_COUNT=$socketWhile1"
Write-Host "NB3_CLOSE_SOCKET_RX_STABLE_DELEGATE_COUNT=$socketRxStableCall"
Write-Host "NB3_CLOSE_SOCKET_TX_STABLE_DELEGATE_COUNT=$socketTxStableCall"
Write-Host "NB3_CLOSE_TCP_OLD_FREE_WAIT_COUNT=$tcpOldFree"
Write-Host "NB3_CLOSE_TCP_OLD_SEND_OK_WAIT_COUNT=$tcpOldSendOk"
Write-Host "NB3_CLOSE_TCP_TIMEOUT_CONDITION_COUNT=$tcpTimeoutLoops"
Write-Host "NB3_CLOSE_UDP_DIRECT_SEND_OK_WAIT_COUNT=$udpDirectSendOk"
Write-Host "NB3_CLOSE_UDP_LEGACY_WRAPPER_POLL_COUNT=$udpWrapperPoll"
Write-Host "NB3_CLOSE_UDP_BEGIN_CHECKED_COMMAND_COUNT=$udpBeginChecked"
Write-Host "NB3_CLOSE_UDP_BEGIN_CHECKED_COMMAND_MATCH=MULTILINE_AWARE"
Write-Host "NB3_CLOSE_UDP_POLL_SEND_OK_COUNT=$udpPollSendOk"
Write-Host "NB3_CLOSE_UDP_POLL_TIMEOUT_COUNT=$udpPollTimeout"
Write-Host "NB3_CLOSE_PARSE_REMAINING_WHILE_COUNT=$parseRemainingWhile"
Write-Host "NB3_CLOSE_PARSE_SINGLE_DRAIN_COUNT=$parseDrainGuard"
Write-Host "NB3_CLOSE_W5100_LEGACY_EXEC_UNBOUNDED_COUNT=$legacyExecUnbounded"
Write-Host "NB3_CLOSE_W5100_LEGACY_EXEC_DELEGATES_CHECKED_COUNT=$legacyExecDelegatesChecked"
Write-Host "NB3_CLOSE_W5100_CHECKED_TIMEOUT_MARKER_COUNT=$checkedExecDeadline"
Write-Host "NB3_CLOSE_W5100_TX_STABLE_BOUND_COUNT=$w5100TxStableBound"
Write-Host "NB3_CLOSE_W5100_RX_STABLE_BOUND_COUNT=$w5100RxStableBound"
Write-Host "NB3_CLOSE_ASYNC_STABLE_WHILE_TRUE_COUNT=$asyncWhileTrue"
Write-Host "NB3_CLOSE_ASYNC_STABLE_DELEGATE_COUNT=$asyncStableDelegate"
Write-Host "NB3_CLOSE_DNS_SYNC_END_PACKET_COUNT=$dnsSyncEndPacket"
Write-Host "NB3_CLOSE_DNS_ASYNC_UDP_BEGIN_COUNT=$dnsAsyncBeginUdp"
Write-Host "NB3_CLOSE_DNS_ASYNC_UDP_POLL_COUNT=$dnsAsyncPollUdp"

if ($socketWhile1 -ne 0) { throw "A14_NB3_CLOSE_SOCKET_STABLE_LOOP_REMAINS=$socketWhile1" }
if ($socketRxStableCall -ne 1) { throw "A14_NB3_CLOSE_RX_STABLE_DELEGATE_INVALID=$socketRxStableCall" }
if ($socketTxStableCall -ne 1) { throw "A14_NB3_CLOSE_TX_STABLE_DELEGATE_INVALID=$socketTxStableCall" }
if ($tcpOldFree -ne 0) { throw "A14_NB3_CLOSE_TCP_FREE_WAIT_REMAINS" }
if ($tcpOldSendOk -ne 0) { throw "A14_NB3_CLOSE_TCP_SEND_OK_WAIT_REMAINS" }
if ($tcpTimeoutLoops -lt 2) { throw "A14_NB3_CLOSE_TCP_TIMEOUT_BOUNDS_MISSING=$tcpTimeoutLoops" }
if ($udpDirectSendOk -ne 0) { throw "A14_NB3_CLOSE_UDP_DIRECT_SEND_OK_REMAINS" }
if ($udpWrapperPoll -lt 1) { throw "A14_NB3_CLOSE_UDP_WRAPPER_NOT_ON_COOPERATIVE_ENGINE" }
if ($udpBeginChecked -ne 1) { throw "A14_NB3_CLOSE_UDP_CHECKED_COMMAND_INVALID=$udpBeginChecked" }
if ($udpPollSendOk -lt 1 -or $udpPollTimeout -lt 1) { throw "A14_NB3_CLOSE_UDP_POLL_TERMINAL_FLAGS_INVALID" }
if ($parseRemainingWhile -ne 0) { throw "A14_NB3_CLOSE_PARSE_UNBOUNDED_LOOP_REMAINS" }
if ($parseDrainGuard -ne 1) { throw "A14_NB3_CLOSE_PARSE_DRAIN_GUARD_INVALID=$parseDrainGuard" }
if ($legacyExecUnbounded -ne 0) { throw "A14_NB3_CLOSE_W5100_LEGACY_EXEC_UNBOUNDED_REMAINS" }
if ($legacyExecDelegatesChecked -lt 1) { throw "A14_NB3_CLOSE_W5100_LEGACY_EXEC_NOT_DELEGATED" }
if ($checkedExecDeadline -lt 1) { throw "A14_NB3_CLOSE_W5100_CHECKED_TIMEOUT_MISSING" }
if ($w5100TxStableBound -ne 1) { throw "A14_NB3_CLOSE_W5100_TX_STABLE_BOUND_INVALID=$w5100TxStableBound" }
if ($w5100RxStableBound -ne 1) { throw "A14_NB3_CLOSE_W5100_RX_STABLE_BOUND_INVALID=$w5100RxStableBound" }
if ($asyncWhileTrue -ne 0) { throw "A14_NB3_CLOSE_ASYNC_STABLE_LOOP_REMAINS" }
if ($asyncStableDelegate -ne 1) { throw "A14_NB3_CLOSE_ASYNC_STABLE_DELEGATE_INVALID=$asyncStableDelegate" }
if ($dnsSyncEndPacket -ne 0) { throw "A14_NB3_CLOSE_DNS_SYNC_UDP_SEND_REMAINS" }
if ($dnsAsyncBeginUdp -ne 1 -or $dnsAsyncPollUdp -ne 1) { throw "A14_NB3_CLOSE_DNS_ASYNC_SEND_CONTRACT_INVALID" }

$arduinoCli = "C:\Program Files\Arduino PLC IDE Tools\arduino-cli.exe"
if (-not (Test-Path -LiteralPath $arduinoCli)) {
    throw "A14_NB3_CLOSE_ARDUINO_CLI_NOT_FOUND"
}

$fqbn = "jwplc_local:esp32:jwplcbasic"
$librariesRoot = Get-G2Path "JWPLC/2.1.0/libraries"
$expectedEthernetLibrary = Join-Path $librariesRoot "JWPLC_Ethernet"
$rawFirmware = Get-G2Path $script:G2RawFirmwareRelative
$rawSketchDir = Split-Path -Parent $rawFirmware

$timestamp = Get-Date -Format "yyyyMMdd_HHmmss"
$tempRoot = Join-Path $env:TEMP ("jwplc_a14_nb3_close_{0}" -f $timestamp)
$buildPath = Join-Path $tempRoot "build"
$compileLog = Join-Path $tempRoot "compile.log"

New-Item -ItemType Directory -Force -Path $buildPath | Out-Null

Write-Host ""
Write-Host "=== FINAL RAW CANDIDATE COMPILE ==="

$compileArgs = @(
    "compile",
    "--fqbn", $fqbn,
    "--build-path", $buildPath,
    "--libraries", $librariesRoot,
    $rawSketchDir
)

$previousPreference = $ErrorActionPreference
try {
    $ErrorActionPreference = "Continue"
    & $arduinoCli @compileArgs *> $compileLog
    $compileExit = [int]$LASTEXITCODE
}
finally {
    $ErrorActionPreference = $previousPreference
}

Write-Host "FINAL_RAW_COMPILE_EXIT=$compileExit"
Write-Host "FINAL_RAW_COMPILE_LOG=$compileLog"

if ($compileExit -ne 0) {
    Invoke-G2CompileFinishedSound -Success $false
    Get-Content -LiteralPath $compileLog -Tail 140 | ForEach-Object { Write-Host $_ }
    throw "A14_NB3_CLOSE_RAW_COMPILE_FAILED"
}

$compileText = [System.IO.File]::ReadAllText($compileLog)
$repoEthernetUsed = (
    $compileText.IndexOf(
        $expectedEthernetLibrary,
        [System.StringComparison]::OrdinalIgnoreCase
    ) -ge 0
)

Write-Host "FINAL_RAW_REPO_ETHERNET_LIBRARY_USED=$repoEthernetUsed"

if (-not $repoEthernetUsed) {
    throw "A14_NB3_CLOSE_WRONG_ETHERNET_LIBRARY"
}

$binCount = @(Get-ChildItem -LiteralPath $buildPath -Recurse -File -Filter "*.bin").Count
Write-Host "FINAL_RAW_BIN_COUNT=$binCount"

if ($binCount -lt 1) {
    throw "A14_NB3_CLOSE_BIN_MISSING"
}

Invoke-G2CompileFinishedSound -Success $true
Write-Host "COMPILE_FINISHED_SOUND=PLAYED"

Assert-G2ProtectedArtifacts

$dirtyFinal = @(Get-G2TrackedDirtyPaths)
$stagedFinal = @(& git -C $script:G2RepoRoot diff --cached --name-only)

Write-Host "TRACKED_DIRTY_COUNT_FINAL=$($dirtyFinal.Count)"
Write-Host "STAGED_COUNT_FINAL=$($stagedFinal.Count)"

if ($dirtyFinal.Count -ne $expectedDirty.Count -or $stagedFinal.Count -ne 0) {
    throw "A14_NB3_CLOSE_FINAL_WORKTREE_INVALID"
}

for ($i = 0; $i -lt $expectedDirty.Count; ++$i) {
    if ($dirtyFinal[$i] -ne $expectedDirty[$i]) {
        throw "A14_NB3_CLOSE_FINAL_DIRTY_PATH_INVALID=$($dirtyFinal[$i])"
    }
}

Write-Host ""
Write-Host "NB3_BOUNDED_W5100_PRIMITIVES=CLOSED_PASS"
Write-Host "NB3_UDP_SEND_COOPERATIVE_ENGINE=CLOSED_PASS"
Write-Host "NB3_UDP_PARSE_PACKET_HARDENING=CLOSED_PASS"
Write-Host "NB3_TCP_LEGACY_SEND_BOUNDS=CLOSED_PASS"
Write-Host "NB3_DNS_ASYNC_SEND_PATH=CLOSED_PASS"
Write-Host "NB3_PHYSICAL_EVIDENCE=NB3C_NB3D2_NB3D3_NB3E2_NB3E3_NB3F2"
Write-Host "NB3_UDP_RX_PERFORMANCE_OPTIMIZATION=PENDING_AFTER_NB3"
Write-Host "NB3_SPI_FREQUENCY=26000000"
Write-Host "NB3_GLOBAL_STATUS=CLOSED_PASS"
Write-Host "A14_NB3_GLOBAL_CLOSURE_AUDIT=PASS"
