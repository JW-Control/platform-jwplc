param(
    [Parameter(Mandatory = $true)]
    [int]$MHz,

    [int]$ExpectedChunks = 8,
    [string]$SerialPort = "COM14",
    [string]$DutIp = "192.168.0.31",
    [double]$DurationSeconds = 3.0
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"
. (Join-Path $PSScriptRoot "common.ps1")

$g3PreferredHoldUs = 5000
$g3HardHoldUs = 10000
$g3ExpectedFirmwareSha256 = "08E16E60F008371416859B63F93D7C8B7D2FB88325FB7281AC6C5C72983FEA8F"

function Invoke-G3NativeToLog {
    param(
        [Parameter(Mandatory = $true)]
        [string]$FilePath,
        [Parameter(Mandatory = $true)]
        [string[]]$Arguments,
        [Parameter(Mandatory = $true)]
        [string]$LogPath
    )

    & $FilePath @Arguments *> $LogPath
    return [int]$LASTEXITCODE
}

function Get-G3LogValue {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Text,
        [Parameter(Mandatory = $true)]
        [string]$Key
    )

    $pattern = "(?m)^" + [regex]::Escape($Key) + "=(.*)\r?$"
    $matches = @([regex]::Matches($Text, $pattern))

    if ($matches.Count -ne 1) {
        throw "G3_LOG_KEY_COUNT_${Key}=$($matches.Count)"
    }

    return $matches[0].Groups[1].Value.Trim()
}

function Get-G3LogInt64 {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Text,
        [Parameter(Mandatory = $true)]
        [string]$Key
    )

    return [int64]::Parse(
        (Get-G3LogValue -Text $Text -Key $Key),
        [System.Globalization.CultureInfo]::InvariantCulture
    )
}

function Assert-G3ExactValue {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Text,
        [Parameter(Mandatory = $true)]
        [string]$Key,
        [Parameter(Mandatory = $true)]
        [string]$Expected
    )

    $actual = Get-G3LogValue -Text $Text -Key ("G2_FINAL_{0}" -f $Key)
    Write-Host "EXACT_${Key}=$actual"

    if ($actual -ne $Expected) {
        throw "G3_EXACT_${Key}_MISMATCH=$actual"
    }
}

function Assert-G3ExactZero {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Text,
        [Parameter(Mandatory = $true)]
        [string]$Key
    )

    $value = Get-G3LogInt64 -Text $Text -Key ("G2_FINAL_{0}" -f $Key)
    Write-Host "EXACT_${Key}=$value"

    if ($value -ne 0) {
        throw "G3_EXACT_${Key}_NONZERO=$value"
    }
}

function Get-G3ChunkCount {
    $firmwarePath = Get-G2Path $script:G2RawFirmwareRelative
    $text = [System.IO.File]::ReadAllText($firmwarePath)
    $pattern = '(?m)^\s*static constexpr uint8_t TCP_RX_MAX_CHUNKS_PER_LOCK = (\d+);\s*$'
    $matches = @([regex]::Matches($text, $pattern))

    Write-Host "TCP_RX_CHUNK_DECLARATION_COUNT=$($matches.Count)"

    if ($matches.Count -ne 1) {
        throw "G3_TCP_RX_CHUNK_DECLARATION_COUNT=$($matches.Count)"
    }

    return [int]$matches[0].Groups[1].Value
}

function Get-G3HoldClass {
    param(
        [Parameter(Mandatory = $true)]
        [int64]$HoldMaxUs
    )

    if ($HoldMaxUs -le $g3PreferredHoldUs) {
        return "PASS_PREFERRED"
    }

    if ($HoldMaxUs -le $g3HardHoldUs) {
        return "REVIEW"
    }

    return "FAIL"
}

Write-Host "============================================================"
Write-Host " G3 PHYSICAL SMOKE - 8 CHUNKS"
Write-Host "============================================================"

Assert-G2Branch
Assert-G2AllowedMHz -MHz $MHz

if ($DurationSeconds -le 0) {
    throw "G3_DURATION_MUST_BE_POSITIVE"
}

if ($ExpectedChunks -lt 1) {
    throw "G3_EXPECTED_CHUNKS_INVALID"
}

$head = Get-G2Head
$targetHz = [int64]$MHz * 1000000
$effectiveHz = Get-G2SpiHz
$effectiveChunks = Get-G3ChunkCount

Write-Host "HEAD=$head"
Write-Host "TARGET_MHZ=$MHz"
Write-Host "TARGET_SPI_HZ=$targetHz"
Write-Host "EFFECTIVE_SPI_HZ=$effectiveHz"
Write-Host "EXPECTED_CHUNKS=$ExpectedChunks"
Write-Host "EFFECTIVE_CHUNKS=$effectiveChunks"
Write-Host "SERIAL_PORT=$SerialPort"
Write-Host "DUT_IP=$DutIp"
Write-Host "SMOKE_DURATION_S=$DurationSeconds"
Write-Host "HOLD_PREFERRED_MAX_US=$g3PreferredHoldUs"
Write-Host "HOLD_HARD_MAX_US=$g3HardHoldUs"
Write-Host "SNAPSHOT_SOURCE=FROZEN_RUNNER_EXACT_FINAL"

if ($effectiveHz -ne $targetHz) {
    throw "G3_EFFECTIVE_SPI_HZ_MISMATCH"
}

if ($effectiveChunks -ne $ExpectedChunks) {
    throw "G3_EFFECTIVE_CHUNKS_MISMATCH"
}

Write-Host ""
Write-Host "=== STATIC PRECHECK ==="

$dirtyBefore = @(Get-G2TrackedDirtyPaths)
$expectedDirty = @(
    $script:G2SpiHeaderRelative,
    $script:G2RawFirmwareRelative
) | Sort-Object

Write-Host "TRACKED_DIRTY_COUNT=$($dirtyBefore.Count)"
$dirtyBefore | ForEach-Object { Write-Host "DIRTY=$_" }

if ($dirtyBefore.Count -ne 2) {
    throw "G3_DIRTY_COUNT_INVALID=$($dirtyBefore.Count)"
}

for ($i = 0; $i -lt $expectedDirty.Count; ++$i) {
    if ($dirtyBefore[$i] -ne $expectedDirty[$i]) {
        throw "G3_DIRTY_PATH_INVALID=$($dirtyBefore[$i])"
    }
}

$stagedBefore = @(& git -C $script:G2RepoRoot diff --cached --name-only)
if ($LASTEXITCODE -ne 0) {
    throw "G3_CACHED_DIFF_FAILED"
}
Write-Host "STAGED_COUNT=$($stagedBefore.Count)"
if ($stagedBefore.Count -ne 0) {
    throw "G3_INDEX_NOT_CLEAN"
}

$spiNumstat = @(& git -C $script:G2RepoRoot diff --numstat -- $script:G2SpiHeaderRelative)
$firmwareNumstat = @(& git -C $script:G2RepoRoot diff --numstat -- $script:G2RawFirmwareRelative)

if ($spiNumstat.Count -ne 1 -or $spiNumstat[0] -notmatch '^1\s+1\s+') {
    throw "G3_SPI_DIFF_NOT_1_1"
}
if ($firmwareNumstat.Count -ne 1 -or $firmwareNumstat[0] -notmatch '^1\s+1\s+') {
    throw "G3_FIRMWARE_DIFF_NOT_1_1"
}

Write-Host "SPI_DIFF_NUMSTAT=$($spiNumstat[0])"
Write-Host "FIRMWARE_DIFF_NUMSTAT=$($firmwareNumstat[0])"

& git -C $script:G2RepoRoot diff --check
if ($LASTEXITCODE -ne 0) {
    throw "G3_DIFF_CHECK_FAILED"
}

Assert-G2ProtectedArtifacts

$runnerHash = Get-G2Sha256 $script:G2RawRunnerRelative
$firmwareHash = Get-G2Sha256 $script:G2RawFirmwareRelative
Write-Host "G3_FIRMWARE_SHA256=$firmwareHash"
Write-Host "RAW_RUNNER_SHA256=$runnerHash"

if ($firmwareHash -ne $g3ExpectedFirmwareSha256) {
    throw "G3_FIRMWARE_HASH_MISMATCH"
}
if ($runnerHash -ne $script:G2RawRunnerSha256) {
    throw "G3_RAW_RUNNER_HASH_MISMATCH"
}

$arduinoCli = "C:\Program Files\Arduino PLC IDE Tools\arduino-cli.exe"
if (-not (Test-Path -LiteralPath $arduinoCli)) {
    throw "G3_ARDUINO_CLI_NOT_FOUND=$arduinoCli"
}

$pythonCommand = Get-Command python.exe -ErrorAction SilentlyContinue
if ($null -eq $pythonCommand) {
    $pythonCommand = Get-Command python -ErrorAction SilentlyContinue
}
if ($null -eq $pythonCommand) {
    throw "G3_PYTHON_NOT_FOUND"
}

$pythonExe = $pythonCommand.Source
$fqbn = "jwplc_local:esp32:jwplcbasic"
$firmwarePath = Get-G2Path $script:G2RawFirmwareRelative
$sketchDir = Split-Path -Parent $firmwarePath
$bridgePath = Join-Path $PSScriptRoot "g2_runner_snapshot_bridge.py"
$librariesRoot = Get-G2Path "JWPLC/2.1.0/libraries"
$expectedEthernetLibrary = Join-Path $librariesRoot "JWPLC_Ethernet"

if (-not (Test-Path -LiteralPath $bridgePath)) {
    throw "G3_SNAPSHOT_BRIDGE_NOT_FOUND"
}

$timestamp = Get-Date -Format "yyyyMMdd_HHmmss"
$tempRoot = Join-Path $env:TEMP ("jwplc_g3_{0}mhz_{1}chunks_smoke_{2}" -f $MHz, $ExpectedChunks, $timestamp)
$buildPath = Join-Path $tempRoot "build"
$compileLog = Join-Path $tempRoot "compile.log"
$uploadLog = Join-Path $tempRoot "upload.log"
$summaryLog = Join-Path $tempRoot "summary.log"
New-Item -ItemType Directory -Force -Path $buildPath | Out-Null

Write-Host "ARDUINO_CLI=$arduinoCli"
Write-Host "PYTHON=$pythonExe"
Write-Host "FQBN=$fqbn"
Write-Host "BRIDGE=$bridgePath"
Write-Host "TEMP_ROOT=$tempRoot"
Write-Host "BUILD_PATH=$buildPath"

Write-Host ""
Write-Host "=== COMPILE G3 FIRMWARE ==="
$compileArgs = @(
    "compile",
    "--fqbn", $fqbn,
    "--build-path", $buildPath,
    "--libraries", $librariesRoot,
    $sketchDir
)
$compileExit = Invoke-G3NativeToLog -FilePath $arduinoCli -Arguments $compileArgs -LogPath $compileLog
Write-Host "COMPILE_EXIT=$compileExit"
Write-Host "COMPILE_LOG=$compileLog"
if ($compileExit -ne 0) {
    Get-Content -LiteralPath $compileLog -Tail 80 | ForEach-Object { Write-Host $_ }
    throw "G3_COMPILE_FAILED"
}

$compileText = [System.IO.File]::ReadAllText($compileLog)
$ethernetLibraryUsed = (
    $compileText.IndexOf($expectedEthernetLibrary, [System.StringComparison]::OrdinalIgnoreCase) -ge 0
)
Write-Host "REPO_ETHERNET_LIBRARY_USED=$ethernetLibraryUsed"
if (-not $ethernetLibraryUsed) {
    throw "G3_COMPILE_DID_NOT_USE_REPO_ETHERNET_LIBRARY"
}

$binCount = @(Get-ChildItem -LiteralPath $buildPath -Recurse -File -Filter "*.bin").Count
Write-Host "BIN_COUNT=$binCount"
if ($binCount -lt 1) {
    throw "G3_COMPILE_NO_BIN_OUTPUT"
}

Assert-G2ProtectedArtifacts
if ((Get-G2Sha256 $script:G2RawFirmwareRelative) -ne $g3ExpectedFirmwareSha256) {
    throw "G3_FIRMWARE_CHANGED_DURING_COMPILE"
}

Write-Host ""
Write-Host "=== UPLOAD G3 FIRMWARE ==="
$uploadArgs = @(
    "upload",
    "--fqbn", $fqbn,
    "--port", $SerialPort,
    "--input-dir", $buildPath,
    $sketchDir
)
$uploadExit = Invoke-G3NativeToLog -FilePath $arduinoCli -Arguments $uploadArgs -LogPath $uploadLog
Write-Host "UPLOAD_EXIT=$uploadExit"
Write-Host "UPLOAD_LOG=$uploadLog"
if ($uploadExit -ne 0) {
    Get-Content -LiteralPath $uploadLog -Tail 80 | ForEach-Object { Write-Host $_ }
    throw "G3_UPLOAD_FAILED"
}

$modeMap = @(
    [pscustomobject]@{ Cli = "tcp-rx"; Key = "TCP_RX" },
    [pscustomobject]@{ Cli = "tcp-tx"; Key = "TCP_TX" },
    [pscustomobject]@{ Cli = "udp-rx"; Key = "UDP_RX" },
    [pscustomobject]@{ Cli = "udp-tx"; Key = "UDP_TX" }
)

$durationText = $DurationSeconds.ToString([System.Globalization.CultureInfo]::InvariantCulture)
$summaryLines = New-Object System.Collections.Generic.List[string]
$summaryLines.Add("G3_PHYSICAL_SMOKE_MHZ=$MHz")
$summaryLines.Add("G3_TCP_RX_CHUNKS=$ExpectedChunks")
$summaryLines.Add("HEAD=$head")
$summaryLines.Add("DURATION_S=$durationText")
$summaryLines.Add("HOLD_PREFERRED_MAX_US=$g3PreferredHoldUs")
$summaryLines.Add("HOLD_HARD_MAX_US=$g3HardHoldUs")

$reviewRequired = $false
$hardHoldFail = $false

Write-Host ""
Write-Host "=== FOUR-MODE G3 EXACT SMOKE ==="

foreach ($mode in $modeMap) {
    $runLog = Join-Path $tempRoot ("{0}.log" -f $mode.Key.ToLowerInvariant())
    $runnerArgs = @(
        $bridgePath,
        "--host", $DutIp,
        "--serial", $SerialPort,
        "--duration", $durationText,
        "--mode", $mode.Cli,
        "--tcp-chunk", "4096",
        "--udp-payload", "1472"
    )

    $runExit = Invoke-G3NativeToLog -FilePath $pythonExe -Arguments $runnerArgs -LogPath $runLog
    Write-Host "MODE=$($mode.Key) RUNNER_EXIT=$runExit LOG=$runLog"

    if ($runExit -ne 0) {
        Get-Content -LiteralPath $runLog -Tail 120 | ForEach-Object { Write-Host $_ }
        throw "G3_RUNNER_FAILED_$($mode.Key)"
    }

    $runText = [System.IO.File]::ReadAllText($runLog)

    if ((Get-G3LogValue -Text $runText -Key "RAW_BENCH_FUNCTIONAL_PASS") -ne "YES") {
        throw "G3_FUNCTIONAL_FAIL_$($mode.Key)"
    }
    if ((Get-G3LogValue -Text $runText -Key "DUT_READY") -ne "YES") {
        throw "G3_DUT_NOT_READY_$($mode.Key)"
    }
    if ((Get-G3LogValue -Text $runText -Key "G2_FINAL_SNAPSHOT_PRESENT") -ne "YES") {
        throw "G3_EXACT_SNAPSHOT_MISSING_$($mode.Key)"
    }

    Assert-G3ExactValue -Text $runText -Key "RAW_SERVER_READY" -Expected "YES"
    Assert-G3ExactValue -Text $runText -Key "ETH_READY" -Expected "YES"
    Assert-G3ExactValue -Text $runText -Key "ETH_LINK" -Expected "UP"
    Assert-G3ExactValue -Text $runText -Key "IP" -Expected $DutIp

    @(
        "TRANSPORT_ERRORS",
        "UDP_BEGIN_PACKET_ERRORS",
        "UDP_WRITE_ERRORS",
        "UDP_END_PACKET_ERRORS",
        "UDP_SPI_LOCK_ERRORS",
        "TCP_SPI_LOCK_ERRORS",
        "UDP_LAST_SHORT_WRITE_BYTES"
    ) | ForEach-Object {
        Assert-G3ExactZero -Text $runText -Key $_
    }

    $pcMbps = Get-G3LogValue -Text $runText -Key ("SUMMARY_{0}_PC_MBPS" -f $mode.Key)
    $dutMbps = Get-G3LogValue -Text $runText -Key ("SUMMARY_{0}_DUT_MBPS" -f $mode.Key)
    $holdAvgUs = Get-G3LogInt64 -Text $runText -Key "G2_FINAL_TCP_SPI_HOLD_AVG_US"
    $holdMaxUs = Get-G3LogInt64 -Text $runText -Key "G2_FINAL_TCP_SPI_HOLD_MAX_US"
    $loopGapMaxUs = Get-G3LogInt64 -Text $runText -Key "G2_FINAL_LOOP_GAP_MAX_US"
    $holdClass = Get-G3HoldClass -HoldMaxUs $holdMaxUs

    Write-Host "EXACT_TCP_SPI_HOLD_AVG_US=$holdAvgUs"
    Write-Host "EXACT_TCP_SPI_HOLD_MAX_US=$holdMaxUs"
    Write-Host "EXACT_LOOP_GAP_MAX_US=$loopGapMaxUs"
    Write-Host "HOLD_CLASS=$holdClass"

    if ($holdClass -eq "REVIEW") {
        $reviewRequired = $true
    }
    elseif ($holdClass -eq "FAIL") {
        $hardHoldFail = $true
    }

    if ($mode.Key -eq "UDP_TX") {
        @(
            "UDP_TX_SEQUENCE_DECODE_ERRORS",
            "UDP_TX_SEQUENCE_TOTAL_DUPLICATES",
            "UDP_TX_SEQUENCE_REORDERS",
            "UDP_TX_SEQUENCE_TOTAL_RANGE_MISSING",
            "UDP_TX_UNEXPECTED_SOURCE_PACKETS",
            "UDP_TX_WRONG_SIZE_FROM_DUT"
        ) | ForEach-Object {
            $value = Get-G3LogInt64 -Text $runText -Key $_
            Write-Host "$_=$value"
            if ($value -ne 0) {
                throw "G3_UDP_TX_INTEGRITY_FAIL_${_}=$value"
            }
        }

        $sequenceCount = Get-G3LogInt64 -Text $runText -Key "UDP_TX_SEQUENCE_TOTAL_COUNT"
        Write-Host "UDP_TX_SEQUENCE_TOTAL_COUNT=$sequenceCount"
        if ($sequenceCount -le 0) {
            throw "G3_UDP_TX_SEQUENCE_EMPTY"
        }
    }

    if ($mode.Key -eq "UDP_RX") {
        Write-Host ("RUN {0} PC_OFFERED_MBPS={1} DUT_MBPS={2} HOLD_MAX_US={3} HOLD_CLASS={4}" -f $mode.Key, $pcMbps, $dutMbps, $holdMaxUs, $holdClass)
    }
    else {
        Write-Host ("RUN {0} PC_MBPS={1} DUT_MBPS={2} HOLD_MAX_US={3} HOLD_CLASS={4}" -f $mode.Key, $pcMbps, $dutMbps, $holdMaxUs, $holdClass)
    }

    $summaryLines.Add(("{0}_PC_MBPS={1}" -f $mode.Key, $pcMbps))
    $summaryLines.Add(("{0}_DUT_MBPS={1}" -f $mode.Key, $dutMbps))
    $summaryLines.Add(("{0}_TCP_SPI_HOLD_AVG_US={1}" -f $mode.Key, $holdAvgUs))
    $summaryLines.Add(("{0}_TCP_SPI_HOLD_MAX_US={1}" -f $mode.Key, $holdMaxUs))
    $summaryLines.Add(("{0}_HOLD_CLASS={1}" -f $mode.Key, $holdClass))
}

Write-Host ""
Write-Host "=== FINAL INVARIANTS ==="
Assert-G2ProtectedArtifacts

$finalFirmwareHash = Get-G2Sha256 $script:G2RawFirmwareRelative
$finalRunnerHash = Get-G2Sha256 $script:G2RawRunnerRelative
Write-Host "G3_FIRMWARE_SHA256=$finalFirmwareHash"
Write-Host "RAW_RUNNER_SHA256=$finalRunnerHash"

if ($finalFirmwareHash -ne $g3ExpectedFirmwareSha256) {
    throw "G3_FINAL_FIRMWARE_HASH_MISMATCH"
}
if ($finalRunnerHash -ne $script:G2RawRunnerSha256) {
    throw "G3_FINAL_RUNNER_HASH_MISMATCH"
}

$dirtyAfter = @(Get-G2TrackedDirtyPaths)
Write-Host "TRACKED_DIRTY_COUNT_FINAL=$($dirtyAfter.Count)"
$dirtyAfter | ForEach-Object { Write-Host "DIRTY_FINAL=$_" }
if ($dirtyAfter.Count -ne 2) {
    throw "G3_FINAL_DIRTY_COUNT_INVALID"
}
for ($i = 0; $i -lt $expectedDirty.Count; ++$i) {
    if ($dirtyAfter[$i] -ne $expectedDirty[$i]) {
        throw "G3_FINAL_DIRTY_PATH_INVALID=$($dirtyAfter[$i])"
    }
}

$stagedAfter = @(& git -C $script:G2RepoRoot diff --cached --name-only)
if ($LASTEXITCODE -ne 0) {
    throw "G3_FINAL_CACHED_DIFF_FAILED"
}
Write-Host "STAGED_COUNT_FINAL=$($stagedAfter.Count)"
if ($stagedAfter.Count -ne 0) {
    throw "G3_FINAL_INDEX_NOT_CLEAN"
}

Write-Host ""
Write-Host "OBSERVACION FISICA REQUERIDA: mira la TFT durante los cuatro modos."
$visualAnswer = ""
while ($visualAnswer -notin @("S", "N")) {
    $visualAnswer = (Read-Host 'Aparecio el diagnostico visual "SPI" durante G3? (S/N)').Trim().ToUpperInvariant()
}

$visualEvents = 0
if ($visualAnswer -eq "S") {
    $visualEvents = 1
}
Write-Host "VISUAL_SPI_EVENTS=$visualEvents"
$summaryLines.Add("VISUAL_SPI_EVENTS=$visualEvents")

$result = "PASS_PREFERRED"
if ($visualEvents -ne 0 -or $hardHoldFail) {
    $result = "FAIL"
}
elseif ($reviewRequired) {
    $result = "REVIEW"
}

$summaryLines.Add("G3_PHYSICAL_SMOKE=$result")
$summaryLines.Add("TEMP_ROOT=$tempRoot")
$summaryLines | Set-Content -LiteralPath $summaryLog -Encoding UTF8

Write-Host ""
Write-Host "============================================================"
Write-Host " G3 PHYSICAL SMOKE SUMMARY"
Write-Host "============================================================"
Write-Host "MHZ=$MHz"
Write-Host "TCP_RX_CHUNKS=$ExpectedChunks"
Write-Host "COMPILE_EXIT=$compileExit"
Write-Host "UPLOAD_EXIT=$uploadExit"
Write-Host "HOLD_PREFERRED_MAX_US=$g3PreferredHoldUs"
Write-Host "HOLD_HARD_MAX_US=$g3HardHoldUs"
Write-Host "VISUAL_SPI_EVENTS=$visualEvents"
Write-Host "SUMMARY_LOG=$summaryLog"
Write-Host "G3_PHYSICAL_SMOKE=$result"

if ($result -eq "FAIL") {
    exit 2
}
if ($result -eq "REVIEW") {
    exit 3
}
