param(
    [string]$SerialPort = "COM14",
    [double]$DurationSeconds = 5.0,
    [int]$Runs = 3
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

. (Join-Path $PSScriptRoot "common.ps1")

function Get-UniqueLogValue {
    param(
        [string]$Text,
        [string]$Key
    )

    $pattern = "(?m)^" + [regex]::Escape($Key) + "=(.*)\r?$"
    $matches = @([regex]::Matches($Text, $pattern))

    if ($matches.Count -ne 1) {
        throw ("H4A01_LOG_KEY_COUNT_{0}={1}" -f $Key, $matches.Count)
    }

    return $matches[0].Groups[1].Value.Trim()
}

function Get-UniqueLogDouble {
    param(
        [string]$Text,
        [string]$Key
    )

    return [double]::Parse(
        (Get-UniqueLogValue -Text $Text -Key $Key),
        [System.Globalization.CultureInfo]::InvariantCulture
    )
}

function Assert-ExactSha256 {
    param(
        [string]$RelativePath,
        [string]$Expected,
        [string]$Label
    )

    $actual = Get-G2Sha256 $RelativePath
    Write-Host "$Label=$actual"

    if ($actual -ne $Expected) {
        throw ($Label + "_MISMATCH")
    }
}

Write-Host "============================================================"
Write-Host " A14 H4A0.1 - EXACT P3K UDP FAST PATH REPLAY POST-H3E"
Write-Host " BATCH2 + INT + FUSED + COMMIT2 + R1"
Write-Host "============================================================"

Assert-G2Branch

if ($DurationSeconds -le 0) {
    throw "H4A01_DURATION_INVALID"
}

if ($Runs -ne 3) {
    throw "H4A01_RUNS_MUST_BE_3"
}

$head = Get-G2Head
$spiHz = Get-G2SpiHz
$dirty = @(Get-G2TrackedDirtyPaths)
$staged = @(& git -C $script:G2RepoRoot diff --cached --name-only)
$untracked = @(& git -C $script:G2RepoRoot ls-files --others --exclude-standard)

if ($LASTEXITCODE -ne 0) {
    throw "H4A01_GIT_QUERY_FAILED"
}

$productUntracked = @(
    $untracked |
        ForEach-Object { $_.Replace("\", "/") } |
        Where-Object {
            $_.StartsWith("JWPLC/2.1.0/") -or
            $_.StartsWith("tools/modbus-tcp-benchmark/firmware/")
        }
)

Write-Host "HEAD=$head"
Write-Host "W5500_SPI_HZ=$spiHz"
Write-Host "TRACKED_DIRTY_COUNT=$($dirty.Count)"
Write-Host "STAGED_COUNT=$($staged.Count)"
Write-Host "UNTRACKED_COUNT=$($untracked.Count)"
Write-Host "UNTRACKED_PRODUCT_COUNT=$($productUntracked.Count)"
Write-Host "SERIAL_PORT=$SerialPort"
Write-Host ("DURATION_S={0:F1}" -f $DurationSeconds)
Write-Host "RUNS=$Runs"
Write-Host "PAYLOAD_BYTES=1016"
Write-Host "SOCKET_TOPOLOGY=8x2KB"
Write-Host "CANDIDATE=BATCH2_INT_FUSED_COMMIT2_R1"
Write-Host "ETH_INT_PIN=GPIO15"
Write-Host "PRODUCT_SOURCE_MUTATION=NO"
Write-Host "DIAGNOSTIC_COPY_ONLY=YES"
Write-Host "POST_H3E_PACKAGE=YES"

if ($spiHz -ne 26000000) {
    throw "H4A01_EXPECTED_26MHZ"
}

if ($dirty.Count -ne 0) {
    $dirty | ForEach-Object { Write-Host "DIRTY=$_" }
    throw "H4A01_TRACKED_TREE_NOT_CLEAN"
}

if ($staged.Count -ne 0) {
    $staged | ForEach-Object { Write-Host "STAGED=$_" }
    throw "H4A01_INDEX_NOT_CLEAN"
}

if ($productUntracked.Count -ne 0) {
    $productUntracked | ForEach-Object { Write-Host "UNTRACKED_PRODUCT=$_" }
    throw "H4A01_UNTRACKED_PRODUCT_SOURCE_FOUND"
}

Assert-G2ProtectedArtifacts

Assert-ExactSha256 -RelativePath "JWPLC/2.1.0/libraries/JWPLC_Display/src/esp32/libJWPLC_Display.a" -Expected "52B9BC617FACB77705161B4F07E6D45571043E4473934EFE19A1F5444BB5D986" -Label "H4A01_DISPLAY_SHA256"
Assert-ExactSha256 -RelativePath "JWPLC/2.1.0/libraries/JWPLC_TFT/src/esp32/libJWPLC_TFT.a" -Expected "5D860A131811DD9A7EB6FA55F5674B1D78B0DE7DFAF8748CE18A60CEED2D3738" -Label "H4A01_TFT_SHA256"
Assert-ExactSha256 -RelativePath "JWPLC/2.1.0/libraries/JWPLC_ModbusRTU/src/esp32/libJWPLC_ModbusRTU.a" -Expected "486BE38AE088B94898E516FFBC125855F22C2EC5EE8A6FA9E10F35D7CAC3A3BE" -Label "H4A01_MODBUS_RTU_SHA256"

$child = Join-Path $PSScriptRoot "a14_p3k_tcp_udp_same_session_parity.ps1"

if (-not (Test-Path -LiteralPath $child)) {
    throw "H4A01_P3K_CHILD_MISSING"
}

$timestamp = Get-Date -Format "yyyyMMdd_HHmmss"
$tempRoot = Join-Path $env:TEMP ("jwplc_a14_h4a01_{0}" -f $timestamp)
$childLog = Join-Path $tempRoot "p3k_post_h3e_replay.log"

New-Item -ItemType Directory -Force -Path $tempRoot | Out-Null

Write-Host "H4A01_TEMP_ROOT=$tempRoot"
Write-Host "H4A01_CHILD_GATE=$child"

Write-Host ""
Write-Host "=== EXACT P3K REPLAY ON CURRENT POST-H3E PACKAGE ==="

$durationText = $DurationSeconds.ToString(
    [System.Globalization.CultureInfo]::InvariantCulture
)

$childArgs = @(
    "-NoLogo",
    "-NoProfile",
    "-ExecutionPolicy", "Bypass",
    "-File", $child,
    "-SerialPort", $SerialPort,
    "-DurationSeconds", $durationText,
    "-Runs", $Runs.ToString()
)

$previousPreference = $ErrorActionPreference

try {
    $ErrorActionPreference = "Continue"
    & powershell.exe @childArgs 2>&1 | Tee-Object -FilePath $childLog
    $childExit = [int]$LASTEXITCODE
}
finally {
    $ErrorActionPreference = $previousPreference
}

Write-Host "H4A01_CHILD_EXIT=$childExit"
Write-Host "H4A01_CHILD_LOG=$childLog"

if ($childExit -ne 0) {
    throw "H4A01_P3K_REPLAY_FAILED"
}

$childText = [System.IO.File]::ReadAllText($childLog)

if ((Get-UniqueLogValue -Text $childText -Key "A14_P3K_SAME_SESSION_TCP_UDP_PARITY") -ne "PASS") {
    throw "H4A01_P3K_FINAL_MARKER_MISSING"
}

if ((Get-UniqueLogValue -Text $childText -Key "A14_P3K_PRODUCT_SOURCE_MUTATION") -ne "NO") {
    throw "H4A01_PRODUCT_MUTATION_CONTRACT_FAILED"
}

if ((Get-UniqueLogValue -Text $childText -Key "A14_P3K_DIAGNOSTIC_COPY_ONLY") -ne "YES") {
    throw "H4A01_DIAGNOSTIC_COPY_CONTRACT_FAILED"
}

$udpMedian = Get-UniqueLogDouble -Text $childText -Key "P3K_UDP_MEDIAN_MBPS"
$tcpMedian = Get-UniqueLogDouble -Text $childText -Key "P3K_TCP_MEDIAN_MBPS"
$udpVsTcp = Get-UniqueLogDouble -Text $childText -Key "P3K_AGGREGATE_UDP_VS_TCP_PCT"
$interpretation = Get-UniqueLogValue -Text $childText -Key "P3K_INTERPRETATION"

$historicalUdp = 13.866349
$historicalTcp = 13.412507
$h4a0LegacyUdp = 11.052889
$h4a0LegacyTcp = 13.577425

$udpVsHistoricalPct = (($udpMedian / $historicalUdp) - 1.0) * 100.0
$tcpVsHistoricalPct = (($tcpMedian / $historicalTcp) - 1.0) * 100.0
$udpVsLegacyPct = (($udpMedian / $h4a0LegacyUdp) - 1.0) * 100.0
$tcpVsLegacyPct = (($tcpMedian / $h4a0LegacyTcp) - 1.0) * 100.0

$udpRecoveryPct = ($udpMedian / $historicalUdp) * 100.0
$recovered97 = $udpRecoveryPct -ge 97.0
$udpBeatsLegacy = $udpMedian -gt $h4a0LegacyUdp

Write-Host ""
Write-Host "============================================================"
Write-Host " H4A0.1 POST-H3E REPLAY COMPARISON"
Write-Host "============================================================"
Write-Host ("H4A01_UDP_MEDIAN_MBPS={0:F6}" -f $udpMedian)
Write-Host ("H4A01_TCP_MEDIAN_MBPS={0:F6}" -f $tcpMedian)
Write-Host ("H4A01_UDP_VS_TCP_PCT={0:F2}" -f $udpVsTcp)
Write-Host "H4A01_P3K_INTERPRETATION=$interpretation"
Write-Host ("H4A01_HISTORICAL_P3K_UDP_MBPS={0:F6}" -f $historicalUdp)
Write-Host ("H4A01_HISTORICAL_P3K_TCP_MBPS={0:F6}" -f $historicalTcp)
Write-Host ("H4A01_H4A0_LEGACY_UDP_MBPS={0:F6}" -f $h4a0LegacyUdp)
Write-Host ("H4A01_H4A0_LEGACY_TCP_MBPS={0:F6}" -f $h4a0LegacyTcp)
Write-Host ("H4A01_UDP_VS_HISTORICAL_P3K_PCT={0:F2}" -f $udpVsHistoricalPct)
Write-Host ("H4A01_TCP_VS_HISTORICAL_P3K_PCT={0:F2}" -f $tcpVsHistoricalPct)
Write-Host ("H4A01_UDP_VS_H4A0_LEGACY_PCT={0:F2}" -f $udpVsLegacyPct)
Write-Host ("H4A01_TCP_VS_H4A0_LEGACY_PCT={0:F2}" -f $tcpVsLegacyPct)
Write-Host ("H4A01_UDP_RECOVERY_OF_HISTORICAL_PCT={0:F2}" -f $udpRecoveryPct)
Write-Host "H4A01_UDP_RECOVERY_GE_97PCT=$recovered97"
Write-Host "H4A01_UDP_BEATS_CURRENT_LEGACY=$udpBeatsLegacy"

Write-Host ""
Write-Host "============================================================"
Write-Host " PHYSICAL TFT OBSERVATION"
Write-Host "============================================================"

$tftAnswer = Read-Host "¿TFT COM14 estable y operativo durante H4A0.1? (S/N)"
$tftPhysical = $tftAnswer.Trim().ToUpper() -eq "S"

Write-Host "H4A01_TFT_PHYSICAL_PASS=$tftPhysical"

if (-not $tftPhysical) {
    throw "H4A01_TFT_PHYSICAL_REVIEW"
}

$finalDirty = @(Get-G2TrackedDirtyPaths)
$finalStaged = @(& git -C $script:G2RepoRoot diff --cached --name-only)

Write-Host "H4A01_FINAL_TRACKED_DIRTY_COUNT=$($finalDirty.Count)"
Write-Host "H4A01_FINAL_STAGED_COUNT=$($finalStaged.Count)"

if ($finalDirty.Count -ne 0) {
    throw "H4A01_REPOSITORY_MUTATED"
}

if ($finalStaged.Count -ne 0) {
    throw "H4A01_INDEX_MUTATED"
}

Assert-G2ProtectedArtifacts

Write-Host "H4A01_PRODUCT_SOURCE_MUTATION=NO"
Write-Host "H4A01_DIAGNOSTIC_COPY_ONLY=YES"
Write-Host "A14_H4A01_P3K_POST_H3E_REPLAY=PASS"

if ($recovered97 -and $udpBeatsLegacy) {
    Write-Host "H4A01_UDP_FAST_PATH_RECOVERY=PASS"
    Write-Host "NEXT=DESIGN_PRODUCTIZABLE_ADDITIVE_OR_INTERNAL_UDP_FAST_PATH"
}
else {
    Write-Host "H4A01_UDP_FAST_PATH_RECOVERY=REVIEW"
    Write-Host "NEXT=RUN_COMPONENT_ABLATION_BATCH2_INT_FUSED_COMMIT2_R1"
}
