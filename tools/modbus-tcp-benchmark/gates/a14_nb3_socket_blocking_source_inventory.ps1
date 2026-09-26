param()

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"
. (Join-Path $PSScriptRoot "common.ps1")

function Get-FunctionBlock {
    param(
        [Parameter(Mandatory = $true)][string]$Text,
        [Parameter(Mandatory = $true)][string]$Signature
    )

    $searchOffset = 0
    $start = -1
    $braceStart = -1

    while ($true) {
        $candidate = $Text.IndexOf(
            $Signature,
            $searchOffset,
            [System.StringComparison]::Ordinal
        )

        if ($candidate -lt 0) {
            break
        }

        $cursor = $candidate + $Signature.Length

        while ($cursor -lt $Text.Length -and
               [char]::IsWhiteSpace($Text[$cursor])) {
            ++$cursor
        }

        if ($cursor -lt $Text.Length -and
            $Text[$cursor] -eq "{") {
            $start = $candidate
            $braceStart = $cursor
            break
        }

        # Skip forward declarations and any non-definition occurrence.
        $searchOffset = $candidate + $Signature.Length
    }

    if ($start -lt 0 -or $braceStart -lt 0) {
        throw "A14_NB3A_FUNCTION_DEFINITION_NOT_FOUND=$Signature"
    }

    $depth = 0
    for ($i = $braceStart; $i -lt $Text.Length; ++$i) {
        $ch = $Text[$i]

        if ($ch -eq "{") {
            ++$depth
        }
        elseif ($ch -eq "}") {
            --$depth
            if ($depth -eq 0) {
                return $Text.Substring(
                    $start,
                    $i - $start + 1
                )
            }
        }
    }

    throw "A14_NB3A_FUNCTION_END_NOT_FOUND=$Signature"
}

function Count-Literal {
    param(
        [Parameter(Mandatory = $true)][string]$Text,
        [Parameter(Mandatory = $true)][string]$Needle
    )

    if ([string]::IsNullOrEmpty($Needle)) { return 0 }

    $count = 0
    $offset = 0
    while ($true) {
        $index = $Text.IndexOf($Needle, $offset, [System.StringComparison]::Ordinal)
        if ($index -lt 0) { break }
        ++$count
        $offset = $index + $Needle.Length
    }
    return $count
}

Write-Host "============================================================"
Write-Host " A14 NB3-A - UDP / SOCKET BLOCKING SOURCE INVENTORY"
Write-Host "============================================================"

Assert-G2Branch

$socketRelative = "JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/socket.cpp"
$udpRelative = "JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/EthernetUdp.cpp"
$w5100CppRelative = "JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/utility/w5100.cpp"
$asyncTxRelative = "JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/jwplc_ethernet_async_tx.cpp"

$expectedDirty = @(
    "JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/Dns.cpp",
    "JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/Dns.h",
    "JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/EthernetClient.cpp",
    "JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/JWPLC_W5x00_Ethernet.h",
    $script:G2SpiHeaderRelative,
    $script:G2RawFirmwareRelative
) | Sort-Object

$dirty = @(Get-G2TrackedDirtyPaths)
Write-Host "HEAD=$(Get-G2Head)"
Write-Host "EFFECTIVE_SPI_HZ=$(Get-G2SpiHz)"
Write-Host "TRACKED_DIRTY_COUNT=$($dirty.Count)"
$dirty | ForEach-Object { Write-Host "DIRTY=$_" }

if ($dirty.Count -ne $expectedDirty.Count) { throw "A14_NB3A_DIRTY_COUNT_INVALID=$($dirty.Count)" }
for ($i = 0; $i -lt $expectedDirty.Count; ++$i) {
    if ($dirty[$i] -ne $expectedDirty[$i]) { throw "A14_NB3A_DIRTY_PATH_INVALID=$($dirty[$i])" }
}

$staged = @(& git -C $script:G2RepoRoot diff --cached --name-only)
if ($LASTEXITCODE -ne 0 -or $staged.Count -ne 0) { throw "A14_NB3A_INDEX_NOT_CLEAN" }
Write-Host "STAGED_COUNT=0"

foreach ($relative in @($socketRelative, $udpRelative, $w5100CppRelative, $asyncTxRelative)) {
    $diff = @(& git -C $script:G2RepoRoot diff --name-only -- $relative)
    if ($LASTEXITCODE -ne 0) { throw "A14_NB3A_DIFF_CHECK_FAILED=$relative" }
    if ($diff.Count -ne 0) { throw "A14_NB3A_SOURCE_ALREADY_DIRTY=$relative" }
}
Write-Host "NB3_AUDITED_SOURCES_TRACKED_CLEAN=YES"

Assert-G2ProtectedArtifacts

$socketHash = Get-G2Sha256 $socketRelative
$udpHash = Get-G2Sha256 $udpRelative
$w5100CppHash = Get-G2Sha256 $w5100CppRelative
$asyncTxHash = Get-G2Sha256 $asyncTxRelative

Write-Host "SOCKET_CPP_SHA256=$socketHash"
Write-Host "ETHERNET_UDP_CPP_SHA256=$udpHash"
Write-Host "W5100_CPP_SHA256=$w5100CppHash"
Write-Host "ASYNC_TX_CPP_SHA256=$asyncTxHash"

Write-Host "NB3_AUDITED_SOURCE_AUTHORITY=TRACKED_CLEAN_GIT_DIFF"
Write-Host "NB3_LOCAL_SHA256=EVIDENCE_ONLY"

$socketText = [System.IO.File]::ReadAllText((Get-G2Path $socketRelative))
$udpText = [System.IO.File]::ReadAllText((Get-G2Path $udpRelative))
$w5100CppText = [System.IO.File]::ReadAllText((Get-G2Path $w5100CppRelative))
$asyncTxText = [System.IO.File]::ReadAllText((Get-G2Path $asyncTxRelative))

$rxStableBlock = Get-FunctionBlock -Text $socketText -Signature "static uint16_t getSnRX_RSR(uint8_t s)"
$txStableBlock = Get-FunctionBlock -Text $socketText -Signature "static uint16_t getSnTX_FSR(uint8_t s)"
$socketSendBlock = Get-FunctionBlock -Text $socketText -Signature "uint16_t EthernetClass::socketSend(uint8_t s, const uint8_t * buf, uint16_t len)"
$socketSendUdpBlock = Get-FunctionBlock -Text $socketText -Signature "bool EthernetClass::socketSendUDP(uint8_t s)"
$parsePacketBlock = Get-FunctionBlock -Text $udpText -Signature "int EthernetUDP::parsePacket()"
$execCmdBlock = Get-FunctionBlock -Text $w5100CppText -Signature "void W5100Class::execCmdSn(SOCKET s, SockCMD _cmd)"
$asyncStableBlock = Get-FunctionBlock -Text $asyncTxText -Signature "uint16_t JWPLC_EthernetAsyncTx::readTxFreeStable(uint8_t socket)"

$rxStableWhileCount = Count-Literal -Text $rxStableBlock -Needle "while (1)"
$txStableWhileCount = Count-Literal -Text $txStableBlock -Needle "while (1)"
$tcpFreeWaitCount = Count-Literal -Text $socketSendBlock -Needle "} while (freesize < ret);"
$tcpSendOkWaitCount = Count-Literal -Text $socketSendBlock -Needle "while ( (W5100.readSnIR(s) & SnIR::SEND_OK) != SnIR::SEND_OK )"
$udpSendOkWaitCount = Count-Literal -Text $socketSendUdpBlock -Needle "while ( (W5100.readSnIR(s) & SnIR::SEND_OK) != SnIR::SEND_OK )"
$udpSendTimeoutCheckCount = Count-Literal -Text $socketSendUdpBlock -Needle "W5100.readSnIR(s) & SnIR::TIMEOUT"
$parseRemainingWhileCount = Count-Literal -Text $parsePacketBlock -Needle "while (_remaining)"
$parseReadFailureCommentCount = Count-Literal -Text $parsePacketBlock -Needle "loop endlessly"
$execCmdWaitCount = Count-Literal -Text $execCmdBlock -Needle "while (readSnCR(s)) ;"
$asyncStableWhileCount = Count-Literal -Text $asyncStableBlock -Needle "while (true)"

Write-Host ""
Write-Host "=== NB3-A INVENTORY ==="
Write-Host "SOCKET_RX_STABLE_WHILE1_COUNT=$rxStableWhileCount"
Write-Host "SOCKET_TX_STABLE_WHILE1_COUNT=$txStableWhileCount"
Write-Host "SOCKET_TCP_FREE_WAIT_LOOP_COUNT=$tcpFreeWaitCount"
Write-Host "SOCKET_TCP_SEND_OK_WAIT_LOOP_COUNT=$tcpSendOkWaitCount"
Write-Host "SOCKET_UDP_SEND_OK_WAIT_LOOP_COUNT=$udpSendOkWaitCount"
Write-Host "SOCKET_UDP_TIMEOUT_FLAG_CHECK_COUNT=$udpSendTimeoutCheckCount"
Write-Host "UDP_PARSE_REMAINING_WHILE_COUNT=$parseRemainingWhileCount"
Write-Host "UDP_PARSE_ENDLESS_RISK_COMMENT_COUNT=$parseReadFailureCommentCount"
Write-Host "W5100_EXEC_CMD_WAIT_LOOP_COUNT=$execCmdWaitCount"
Write-Host "ASYNC_TX_STABLE_WHILE_TRUE_COUNT=$asyncStableWhileCount"

if ($rxStableWhileCount -ne 1) { throw "A14_NB3A_RX_STABLE_LOOP_COUNT_INVALID=$rxStableWhileCount" }
if ($txStableWhileCount -ne 1) { throw "A14_NB3A_TX_STABLE_LOOP_COUNT_INVALID=$txStableWhileCount" }
if ($tcpFreeWaitCount -ne 1) { throw "A14_NB3A_TCP_FREE_WAIT_COUNT_INVALID=$tcpFreeWaitCount" }
if ($tcpSendOkWaitCount -ne 1) { throw "A14_NB3A_TCP_SEND_OK_WAIT_COUNT_INVALID=$tcpSendOkWaitCount" }
if ($udpSendOkWaitCount -ne 1) { throw "A14_NB3A_UDP_SEND_OK_WAIT_COUNT_INVALID=$udpSendOkWaitCount" }
if ($udpSendTimeoutCheckCount -ne 1) { throw "A14_NB3A_UDP_TIMEOUT_CHECK_COUNT_INVALID=$udpSendTimeoutCheckCount" }
if ($parseRemainingWhileCount -ne 1) { throw "A14_NB3A_PARSE_REMAINING_LOOP_COUNT_INVALID=$parseRemainingWhileCount" }
if ($parseReadFailureCommentCount -ne 1) { throw "A14_NB3A_PARSE_RISK_COMMENT_COUNT_INVALID=$parseReadFailureCommentCount" }
if ($execCmdWaitCount -ne 1) { throw "A14_NB3A_EXEC_CMD_WAIT_COUNT_INVALID=$execCmdWaitCount" }
if ($asyncStableWhileCount -ne 1) { throw "A14_NB3A_ASYNC_STABLE_LOOP_COUNT_INVALID=$asyncStableWhileCount" }

$inventoryCount = $rxStableWhileCount + $txStableWhileCount + $tcpFreeWaitCount + $tcpSendOkWaitCount + $udpSendOkWaitCount + $parseRemainingWhileCount + $execCmdWaitCount + $asyncStableWhileCount
Write-Host "NB3_BLOCKING_OR_UNBOUNDED_LOOP_INVENTORY_COUNT=$inventoryCount"
if ($inventoryCount -ne 8) { throw "A14_NB3A_INVENTORY_COUNT_INVALID=$inventoryCount" }

Write-Host ""
Write-Host "NB3_PRIORITY_1=W5100_EXEC_CMD_AND_STABLE_REGISTER_READS"
Write-Host "NB3_PRIORITY_2=UDP_SEND_COOPERATIVE_ENGINE"
Write-Host "NB3_PRIORITY_3=UDP_PARSE_REMAINING_HARDENING"
Write-Host "NB3_PRIORITY_4=LEGACY_TCP_SEND_BOUNDS"
Write-Host "NB3_DNS_BEGIN_HOLD_6033US=ATTRIBUTED_TO_UDP_SEND_PATH"
Write-Host "NB3_RUNTIME_POLICY=COOPERATIVE_PATHS_ONLY"
Write-Host "NB3_HASH_TYPE_CONFUSION=CORRECTED"
Write-Host "NB3_FUNCTION_DEFINITION_MATCHING=ENFORCED"
Write-Host "A14_NB3_SOCKET_BLOCKING_SOURCE_INVENTORY=PASS"
