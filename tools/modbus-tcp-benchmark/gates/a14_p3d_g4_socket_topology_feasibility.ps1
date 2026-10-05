param(
    [int]$UdpPayloadBytes = 1472
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"
. (Join-Path $PSScriptRoot "common.ps1")

$expectedW5100HHash = "9A833532C44E0CFCD66429A764E8BDBF62A8871838B0355565BC0A63043455FC"
$expectedW5100CppHash = "9F94AAC1BB25966C18EDFD6A5C5D5908A9DBD4CF11B6E9BC099E10B28CEF272F"
$expectedRawHash = "9A0C6CF27E44A61DC97686FB1A769EBBBB34D036CDF8D0CA0EEAB597E699AB6B"

function Require-RegexCount {
    param(
        [string]$Text,
        [string]$Pattern,
        [int]$Expected,
        [string]$Label
    )

    $count = @([regex]::Matches(
        $Text,
        $Pattern,
        [System.Text.RegularExpressions.RegexOptions]::Multiline
    )).Count

    Write-Host "$Label=$count"

    if ($count -ne $Expected) {
        throw ("{0}_EXPECTED_{1}_GOT_{2}" -f $Label, $Expected, $count)
    }
}

Write-Host "============================================================"
Write-Host " A14 P3D - G4 SOCKET TOPOLOGY FEASIBILITY AUDIT"
Write-Host "============================================================"

Assert-G2Branch

$head = Get-G2Head
$effectiveHz = Get-G2SpiHz
$dirty = @(Get-G2TrackedDirtyPaths)
$staged = @(& git -C $script:G2RepoRoot diff --cached --name-only)

if ($LASTEXITCODE -ne 0) {
    throw "P3D_CACHED_DIFF_FAILED"
}

Write-Host "HEAD=$head"
Write-Host "EFFECTIVE_SPI_HZ=$effectiveHz"
Write-Host "TRACKED_DIRTY_COUNT=$($dirty.Count)"
Write-Host "STAGED_COUNT=$($staged.Count)"
Write-Host "SOURCE_MUTATION=NO"
Write-Host "UPLOAD=NO"
Write-Host "BENCHMARK=NO"

if ($effectiveHz -ne 26000000) { throw "P3D_EXPECTED_26MHZ" }
if ($dirty.Count -ne 0) { throw "P3D_TRACKED_TREE_NOT_CLEAN" }
if ($staged.Count -ne 0) { throw "P3D_INDEX_NOT_CLEAN" }

Assert-G2ProtectedArtifacts

$headerRelative = "JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/JWPLC_W5x00_Ethernet.h"
$w5100HRelative = $script:G2SpiHeaderRelative
$w5100CppRelative = "JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/utility/w5100.cpp"

$headerPath = Get-G2Path $headerRelative
$w5100HPath = Get-G2Path $w5100HRelative
$w5100CppPath = Get-G2Path $w5100CppRelative

$w5100HHash = Get-G2Sha256 $w5100HRelative
$w5100CppHash = Get-G2Sha256 $w5100CppRelative
$rawHash = Get-G2Sha256 $script:G2RawFirmwareRelative

Write-Host "W5100_H_SHA256=$w5100HHash"
Write-Host "W5100_CPP_SHA256=$w5100CppHash"
Write-Host "RAW_FIRMWARE_SHA256=$rawHash"

if ($w5100HHash -ne $expectedW5100HHash) { throw "P3D_W5100_H_HASH_MISMATCH" }
if ($w5100CppHash -ne $expectedW5100CppHash) { throw "P3D_W5100_CPP_HASH_MISMATCH" }
if ($rawHash -ne $expectedRawHash) { throw "P3D_RAW_HASH_MISMATCH" }

$headerText = [System.IO.File]::ReadAllText($headerPath)
$w5100HText = [System.IO.File]::ReadAllText($w5100HPath)
$w5100CppText = [System.IO.File]::ReadAllText($w5100CppPath)

$patternMaxSock8 = '^(?!\s*//)\s*#define\s+MAX_SOCK_NUM\s+8\s*$'
$patternCommentLarge = '^\s*//\s*#define\s+ETHERNET_LARGE_BUFFERS\s*$'
$patternActiveLarge = '^(?!\s*//)\s*#define\s+ETHERNET_LARGE_BUFFERS\s*$'
$patternSsize2048 = '^\s*static\s+const\s+uint16_t\s+SSIZE\s*=\s*2048\s*;\s*$'
$patternLarge4 = '#elif\s+MAX_SOCK_NUM\s*<=\s*4\s*\r?\n\s*SSIZE\s*=\s*4096\s*;'
$patternUniformLoop = 'for\s*\(i=0;\s*i<MAX_SOCK_NUM;\s*i\+\+\)\s*\{\s*\r?\n\s*writeSnRX_SIZE\(i,\s*SSIZE\s*>>\s*10\);\s*\r?\n\s*writeSnTX_SIZE\(i,\s*SSIZE\s*>>\s*10\);'

Require-RegexCount -Text $headerText -Pattern $patternMaxSock8 -Expected 1 -Label "ACTIVE_MAX_SOCK_NUM_8_COUNT"
Require-RegexCount -Text $headerText -Pattern $patternCommentLarge -Expected 1 -Label "COMMENTED_LARGE_BUFFERS_DEFINE_COUNT"
Require-RegexCount -Text $headerText -Pattern $patternActiveLarge -Expected 0 -Label "ACTIVE_LARGE_BUFFERS_DEFINE_COUNT"
Require-RegexCount -Text $w5100HText -Pattern $patternSsize2048 -Expected 1 -Label "DEFAULT_SSIZE_2048_COUNT"
Require-RegexCount -Text $w5100CppText -Pattern $patternLarge4 -Expected 2 -Label "LARGE_BUFFER_4SOCKET_4096_BRANCH_COUNT"
Require-RegexCount -Text $w5100CppText -Pattern $patternUniformLoop -Expected 2 -Label "UNIFORM_ACTIVE_SOCKET_SIZE_LOOP_COUNT"

if ($UdpPayloadBytes -le 0) {
    throw "P3D_UDP_PAYLOAD_INVALID"
}

$currentSocketCount = 8
$currentRxBytesPerSocket = 2048
$currentTxBytesPerSocket = 2048
$large4SocketCount = 4
$large4RxBytesPerSocket = 4096
$large4TxBytesPerSocket = 4096
$udpInternalHeaderBytes = 8
$udpRecordBytes = $UdpPayloadBytes + $udpInternalHeaderBytes

$currentDatagramsPerRxSocket =
    [int][math]::Floor(
        $currentRxBytesPerSocket /
        [double]$udpRecordBytes
    )

$large4DatagramsPerRxSocket =
    [int][math]::Floor(
        $large4RxBytesPerSocket /
        [double]$udpRecordBytes
    )

$currentRxTotalBytes = $currentSocketCount * $currentRxBytesPerSocket
$currentTxTotalBytes = $currentSocketCount * $currentTxBytesPerSocket
$large4RxTotalBytes = $large4SocketCount * $large4RxBytesPerSocket
$large4TxTotalBytes = $large4SocketCount * $large4TxBytesPerSocket
$socketCountDelta = $large4SocketCount - $currentSocketCount

Write-Host ""
Write-Host "=== CURRENT W5500 TOPOLOGY ==="
Write-Host "CURRENT_MAX_SOCK_NUM=$currentSocketCount"
Write-Host "CURRENT_RX_BYTES_PER_SOCKET=$currentRxBytesPerSocket"
Write-Host "CURRENT_TX_BYTES_PER_SOCKET=$currentTxBytesPerSocket"
Write-Host "CURRENT_TOTAL_RX_BYTES=$currentRxTotalBytes"
Write-Host "CURRENT_TOTAL_TX_BYTES=$currentTxTotalBytes"
Write-Host "ETHERNET_LARGE_BUFFERS_ACTIVE=NO"
Write-Host "CURRENT_INIT_USES_UNIFORM_SSIZE_FOR_ALL_ACTIVE_SOCKETS=YES"

Write-Host ""
Write-Host "=== UDP CAPACITY ==="
Write-Host "UDP_PAYLOAD_BYTES=$UdpPayloadBytes"
Write-Host "UDP_INTERNAL_HEADER_BYTES=$udpInternalHeaderBytes"
Write-Host "UDP_W5500_RECORD_BYTES=$udpRecordBytes"
Write-Host "CURRENT_COMPLETE_DATAGRAMS_PER_RX_SOCKET=$currentDatagramsPerRxSocket"
Write-Host "LARGE4_COMPLETE_DATAGRAMS_PER_RX_SOCKET=$large4DatagramsPerRxSocket"

Write-Host ""
Write-Host "=== EXISTING UNIFORM 4KB OPTION ==="
Write-Host "G4_UNIFORM_4K_MAX_SOCK_NUM=$large4SocketCount"
Write-Host "G4_UNIFORM_4K_RX_BYTES_PER_SOCKET=$large4RxBytesPerSocket"
Write-Host "G4_UNIFORM_4K_TX_BYTES_PER_SOCKET=$large4TxBytesPerSocket"
Write-Host "G4_UNIFORM_4K_TOTAL_RX_BYTES=$large4RxTotalBytes"
Write-Host "G4_UNIFORM_4K_TOTAL_TX_BYTES=$large4TxTotalBytes"
Write-Host "G4_UNIFORM_4K_SOCKET_COUNT_DELTA=$socketCountDelta"
Write-Host "G4_UNIFORM_4K_REQUIRES_REDUCING_MAX_SOCK_NUM_TO_4=YES"
Write-Host "G4_UNIFORM_4K_REQUIRES_ETHERNET_LARGE_BUFFERS=YES"
Write-Host "G4_TARGETED_PER_SOCKET_RX_ALLOCATION_SUPPORTED_BY_CURRENT_INIT=NO"

if ($currentDatagramsPerRxSocket -ne 1) {
    throw "P3D_CURRENT_UDP_CAPACITY_UNEXPECTED"
}

if ($large4DatagramsPerRxSocket -lt 2) {
    throw "P3D_4K_UDP_CAPACITY_UNEXPECTED"
}

if ($currentRxTotalBytes -ne 16384 -or $large4RxTotalBytes -ne 16384) {
    throw "P3D_RX_TOTAL_BUDGET_UNEXPECTED"
}

if ($currentTxTotalBytes -ne 16384 -or $large4TxTotalBytes -ne 16384) {
    throw "P3D_TX_TOTAL_BUDGET_UNEXPECTED"
}

Write-Host ""
Write-Host "P3D_EVIDENCE_2KB_LIMITS_1472B_UDP_TO_ONE_COMPLETE_DATAGRAM=YES"
Write-Host "P3D_EXISTING_4KB_MODE_DOUBLES_DATAGRAM_CAPACITY=YES"
Write-Host "P3D_EXISTING_4KB_MODE_HALVES_SOCKET_COUNT=YES"
Write-Host "P3D_UNIFORM_4KB_PRODUCT_CHANGE_SAFE_WITHOUT_CONCURRENCY_TEST=NO"
Write-Host "P3D_DECISION=DEFER_UNIFORM_4K_UNTIL_CONCURRENCY_QUALIFIED"
Write-Host "A14_P3D_G4_SOCKET_TOPOLOGY_FEASIBILITY=PASS"
Write-Host "NEXT_ACTION=RETURN_OUTPUT_TO_CHAT_FOR_G4_GO_NO_GO"
