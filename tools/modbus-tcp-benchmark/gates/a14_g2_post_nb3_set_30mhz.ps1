param()

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"
. (Join-Path $PSScriptRoot "common.ps1")

$expectedHeadSourceHashes = [ordered]@{
    "JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/Dns.cpp" = "5D67E2FE8F2333A9A8AAD2A290A26DBAE92761DF11661C4305F8EC9D09604A60"
    "JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/Dns.h" = "B92718E94D549D60A7F2E648DAAD2A18DF4FAB94936BFEB279025911949AAFED"
    "JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/EthernetClient.cpp" = "6ADD2904893DA8C3A61C19FC91BF146C40038834458BC9F3084339462C9A9770"
    "JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/EthernetUdp.cpp" = "F5AE79FA9E9642A670D0AF23E029621711D2DD857B776A5208CF2866EDB496E5"
    "JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/JWPLC_W5x00_Ethernet.h" = "7A5923105CFEEF07CD4BACEB72854397B9E9389772F97DF6B9DF3AB5A4D0F15A"
    "JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/jwplc_ethernet_async_tx.cpp" = "CC8EEE779FC932C5A8FA0175ABBF0421AA8C95279B4260FADAC1276E7BD74E1C"
    "JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/jwplc_ethernet_async_tx.h" = "C1C5615DFF167F3572FBEA85CFFE1A7F5051732E2BD446387F1025A1D981FB47"
    "JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/socket.cpp" = "6E55F494F5E43B6BCF327F40C268AB6FF8F739331C96C328E9A0BEC0DA38654A"
    "JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/utility/w5100.cpp" = "9F94AAC1BB25966C18EDFD6A5C5D5908A9DBD4CF11B6E9BC099E10B28CEF272F"
    "tools/modbus-tcp-benchmark/firmware/eth14_raw_transport_server/eth14_raw_transport_server.ino" = "9A0C6CF27E44A61DC97686FB1A769EBBBB34D036CDF8D0CA0EEAB597E699AB6B"
}

$expectedW5100Hash26 = "9A833532C44E0CFCD66429A764E8BDBF62A8871838B0355565BC0A63043455FC"
$targetHz = [int64]30000000

function Assert-A14G2PostNB3SourceHashes {
    foreach ($entry in $expectedHeadSourceHashes.GetEnumerator()) {
        $actual = Get-G2Sha256 $entry.Key
        Write-Host "SOURCE_SHA256=$($entry.Key)=$actual"

        if ($actual -ne $entry.Value) {
            throw "A14_G2_POST_NB3_SOURCE_HASH_MISMATCH=$($entry.Key)"
        }
    }
}

Write-Host "============================================================"
Write-Host " A14 G2 - POST-NB3 SET 30 MHz"
Write-Host "============================================================"

Assert-G2Branch

$head = Get-G2Head
$currentHz = Get-G2SpiHz
$dirtyBefore = @(Get-G2TrackedDirtyPaths)
$stagedBefore = @(& git -C $script:G2RepoRoot diff --cached --name-only)

if ($LASTEXITCODE -ne 0) {
    throw "A14_G2_POST_NB3_CACHED_DIFF_FAILED"
}

Write-Host "HEAD=$head"
Write-Host "FROM_MHZ=26"
Write-Host "TO_MHZ=30"
Write-Host "CURRENT_SPI_HZ=$currentHz"
Write-Host "TRACKED_DIRTY_BEFORE=$($dirtyBefore.Count)"
Write-Host "STAGED_COUNT_BEFORE=$($stagedBefore.Count)"
Write-Host "UPLOAD=NO"
Write-Host "BENCHMARK=NO"
Write-Host "SOURCE_MUTATION=SPI_FREQUENCY_ONLY"

if ($currentHz -ne 26000000) {
    throw "A14_G2_POST_NB3_EXPECTED_26MHZ_HEAD"
}

if ($dirtyBefore.Count -ne 0) {
    $dirtyBefore | ForEach-Object { Write-Host "DIRTY_BEFORE=$_" }
    throw "A14_G2_POST_NB3_TREE_NOT_CLEAN"
}

if ($stagedBefore.Count -ne 0) {
    throw "A14_G2_POST_NB3_INDEX_NOT_CLEAN"
}

Assert-G2ProtectedArtifacts
Assert-A14G2PostNB3SourceHashes

$w5100HashBefore = Get-G2Sha256 $script:G2SpiHeaderRelative
Write-Host "W5100_H_SHA256_BEFORE=$w5100HashBefore"

if ($w5100HashBefore -ne $expectedW5100Hash26) {
    throw "A14_G2_POST_NB3_W5100_26MHZ_HASH_MISMATCH"
}

Write-Host ""
Write-Host "=== APPLY 30 MHz ==="
Set-G2SpiHz -Hz $targetHz

$effectiveHz = Get-G2SpiHz
Write-Host "EFFECTIVE_SPI_HZ=$effectiveHz"

if ($effectiveHz -ne $targetHz) {
    throw "A14_G2_POST_NB3_EFFECTIVE_SPI_HZ_MISMATCH"
}

$dirtyAfter = @(Get-G2TrackedDirtyPaths)
Write-Host "TRACKED_DIRTY_AFTER=$($dirtyAfter.Count)"
$dirtyAfter | ForEach-Object { Write-Host "DIRTY_AFTER=$_" }

if (
    $dirtyAfter.Count -ne 1 -or
    $dirtyAfter[0] -ne $script:G2SpiHeaderRelative
) {
    throw "A14_G2_POST_NB3_FINAL_DIRTY_STATE_INVALID"
}

$stagedAfter = @(& git -C $script:G2RepoRoot diff --cached --name-only)
if ($LASTEXITCODE -ne 0) {
    throw "A14_G2_POST_NB3_FINAL_CACHED_DIFF_FAILED"
}

Write-Host "STAGED_COUNT_AFTER=$($stagedAfter.Count)"

if ($stagedAfter.Count -ne 0) {
    throw "A14_G2_POST_NB3_FINAL_INDEX_NOT_CLEAN"
}

$numstat = @(
    & git -C $script:G2RepoRoot diff --numstat -- $script:G2SpiHeaderRelative
)

if ($LASTEXITCODE -ne 0 -or $numstat.Count -ne 1) {
    throw "A14_G2_POST_NB3_NUMSTAT_UNAVAILABLE"
}

Write-Host "SPI_DIFF_NUMSTAT=$($numstat[0])"
$parts = $numstat[0].Split([char]9)

if (
    $parts.Count -lt 3 -or
    $parts[0] -ne "1" -or
    $parts[1] -ne "1"
) {
    throw "A14_G2_POST_NB3_SPI_DIFF_NOT_SINGLE_LINE"
}

$previousPreference = $ErrorActionPreference
try {
    $ErrorActionPreference = "Continue"
    & git -C $script:G2RepoRoot diff --check
    $diffCheckExit = [int]$LASTEXITCODE
}
finally {
    $ErrorActionPreference = $previousPreference
}

Write-Host "GIT_DIFF_CHECK_EXIT=$diffCheckExit"

if ($diffCheckExit -ne 0) {
    throw "A14_G2_POST_NB3_DIFF_CHECK_FAILED"
}

Assert-G2ProtectedArtifacts
Assert-A14G2PostNB3SourceHashes

$w5100HashAfter = Get-G2Sha256 $script:G2SpiHeaderRelative
Write-Host "W5100_H_SHA256_30MHZ=$w5100HashAfter"

if ($w5100HashAfter -eq $expectedW5100Hash26) {
    throw "A14_G2_POST_NB3_W5100_HASH_DID_NOT_CHANGE"
}

Write-Host ""
Write-Host "A14_G2_POST_NB3_SOURCE_BASE=PASS"
Write-Host "A14_G2_26_TO_30_SINGLE_VARIABLE=PASS"
Write-Host "A14_G2_30MHZ_STATIC_STATE=PASS"
Write-Host "A14_G2_30MHZ_PREP=PASS"
Write-Host "NEXT_GATE=30MHZ_CONNECTIVITY_PREFLIGHT_COMPILE_UPLOAD_SMOKE"
