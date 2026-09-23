param(
    [string]$SerialPort = "COM14",
    [double]$DurationSeconds = 3.0
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"
. (Join-Path $PSScriptRoot "common.ps1")

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

function Get-LogValue {
    param(
        [Parameter(Mandatory = $true)][string]$Text,
        [Parameter(Mandatory = $true)][string]$Key
    )

    $pattern = "(?m)^" + [regex]::Escape($Key) + "=(.*)\r?$"
    $matches = @([regex]::Matches($Text, $pattern))

    if ($matches.Count -ne 1) {
        throw "A14_NB3D3_KEY_COUNT_$($Key)=$($matches.Count)"
    }

    return $matches[0].Groups[1].Value.Trim()
}

function Get-LogInt64 {
    param(
        [Parameter(Mandatory = $true)][string]$Text,
        [Parameter(Mandatory = $true)][string]$Key
    )

    return [int64]::Parse(
        (Get-LogValue -Text $Text -Key $Key),
        [System.Globalization.CultureInfo]::InvariantCulture
    )
}

function Get-LogDouble {
    param(
        [Parameter(Mandatory = $true)][string]$Text,
        [Parameter(Mandatory = $true)][string]$Key
    )

    return [double]::Parse(
        (Get-LogValue -Text $Text -Key $Key),
        [System.Globalization.CultureInfo]::InvariantCulture
    )
}

function Assert-ExactValue {
    param(
        [Parameter(Mandatory = $true)][string]$Text,
        [Parameter(Mandatory = $true)][string]$Key,
        [Parameter(Mandatory = $true)][string]$Expected
    )

    $actual = Get-LogValue -Text $Text -Key ("G2_FINAL_{0}" -f $Key)
    Write-Host "EXACT_$($Key)=$actual"

    if ($actual -ne $Expected) {
        throw "A14_NB3D3_EXACT_$($Key)_MISMATCH=$actual"
    }
}

function Assert-ExactZero {
    param(
        [Parameter(Mandatory = $true)][string]$Text,
        [Parameter(Mandatory = $true)][string]$Key
    )

    $value = Get-LogInt64 -Text $Text -Key ("G2_FINAL_{0}" -f $Key)
    Write-Host "EXACT_$($Key)=$value"

    if ($value -ne 0) {
        throw "A14_NB3D3_EXACT_$($Key)_NONZERO=$value"
    }
}

Write-Host "============================================================"
Write-Host " A14 NB3-D3 - UDP TX REGRESSION PHYSICAL"
Write-Host "============================================================"

Assert-G2Branch

$baselinePcMbps = 5.174312
$baselineDutMbps = 5.178235
$hardFloorRatio = 0.90
$hardFloorDutMbps = $baselineDutMbps * $hardFloorRatio

Write-Host "HEAD=$(Get-G2Head)"
Write-Host "EFFECTIVE_SPI_HZ=$(Get-G2SpiHz)"
Write-Host "SERIAL_PORT=$SerialPort"
Write-Host "DURATION_S=$DurationSeconds"
Write-Host "RUN_COUNT=3"
Write-Host "UDP_TX_BASELINE_PC_MBPS=$baselinePcMbps"
Write-Host "UDP_TX_BASELINE_DUT_MBPS=$baselineDutMbps"
Write-Host "UDP_TX_MEDIAN_HARD_FLOOR_RATIO=$hardFloorRatio"
Write-Host "UDP_TX_MEDIAN_HARD_FLOOR_DUT_MBPS=$hardFloorDutMbps"
Write-Host "HARD_HOLD_MAX_US=10000"
Write-Host "HARD_LOOP_GAP_MAX_US=15000"

if ((Get-G2SpiHz) -ne 26000000) {
    throw "A14_NB3D3_SPI_FREQUENCY_MISMATCH"
}

if ($DurationSeconds -le 0) {
    throw "A14_NB3D3_DURATION_MUST_BE_POSITIVE"
}

$expectedHashes = [ordered]@{
    "JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/Dns.cpp" = "5D67E2FE8F2333A9A8AAD2A290A26DBAE92761DF11661C4305F8EC9D09604A60"
    "JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/Dns.h" = "B92718E94D549D60A7F2E648DAAD2A18DF4FAB94936BFEB279025911949AAFED"
    "JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/EthernetClient.cpp" = "93B71C92E298053425FF03D750632A30102BE743E1C8B1FDAB800214EABED4B2"
    "JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/EthernetUdp.cpp" = "90F37F3C3E7E4C0F8E0B4982E515AFCFE4A3A480084FBA3E6DCDF0D0DA58CAA1"
    "JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/jwplc_ethernet_async_tx.cpp" = "CC8EEE779FC932C5A8FA0175ABBF0421AA8C95279B4260FADAC1276E7BD74E1C"
    "JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/jwplc_ethernet_async_tx.h" = "C1C5615DFF167F3572FBEA85CFFE1A7F5051732E2BD446387F1025A1D981FB47"
    "JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/JWPLC_W5x00_Ethernet.h" = "8CE510B0E3FED0E2CAFE962B623559AFBA02DCB88928D5C1A7C879263ED310A3"
    "JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/socket.cpp" = "AE404D279294C0A4ABEA0BE51EE6267EB7106B289C27CF46B3991048A3C6A2C2"
    "JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/utility/w5100.cpp" = "9F94AAC1BB25966C18EDFD6A5C5D5908A9DBD4CF11B6E9BC099E10B28CEF272F"
    "JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/utility/w5100.h" = "9A833532C44E0CFCD66429A764E8BDBF62A8871838B0355565BC0A63043455FC"
    "tools/modbus-tcp-benchmark/firmware/eth14_raw_transport_server/eth14_raw_transport_server.ino" = "9A0C6CF27E44A61DC97686FB1A769EBBBB34D036CDF8D0CA0EEAB597E699AB6B"
}

$expectedDirty = @($expectedHashes.Keys) | Sort-Object
$dirty = @(Get-G2TrackedDirtyPaths)

Write-Host "TRACKED_DIRTY_COUNT=$($dirty.Count)"
$dirty | ForEach-Object { Write-Host "DIRTY=$_" }

if ($dirty.Count -ne $expectedDirty.Count) {
    throw "A14_NB3D3_DIRTY_COUNT_INVALID=$($dirty.Count)"
}

for ($i = 0; $i -lt $expectedDirty.Count; ++$i) {
    if ($dirty[$i] -ne $expectedDirty[$i]) {
        throw "A14_NB3D3_DIRTY_PATH_INVALID=$($dirty[$i])"
    }
}

$staged = @(& git -C $script:G2RepoRoot diff --cached --name-only)
if ($LASTEXITCODE -ne 0 -or $staged.Count -ne 0) {
    throw "A14_NB3D3_INDEX_NOT_CLEAN"
}
Write-Host "STAGED_COUNT=0"

Assert-G2ProtectedArtifacts

foreach ($entry in $expectedHashes.GetEnumerator()) {
    $actual = Get-G2Sha256 $entry.Key
    Write-Host "CANDIDATE_SHA256=$($entry.Key)=$actual"

    if ($actual -ne $entry.Value) {
        throw "A14_NB3D3_CANDIDATE_HASH_MISMATCH=$($entry.Key)"
    }
}

$arduinoCli = "C:\Program Files\Arduino PLC IDE Tools\arduino-cli.exe"
if (-not (Test-Path -LiteralPath $arduinoCli)) {
    throw "A14_NB3D3_ARDUINO_CLI_NOT_FOUND=$arduinoCli"
}

$pythonCommand = Get-Command python.exe -ErrorAction SilentlyContinue
if ($null -eq $pythonCommand) {
    $pythonCommand = Get-Command python -ErrorAction Stop
}
$pythonExe = $pythonCommand.Source

$fqbn = "jwplc_local:esp32:jwplcbasic"
$librariesRoot = Get-G2Path "JWPLC/2.1.0/libraries"
$expectedEthernetLibrary = Join-Path $librariesRoot "JWPLC_Ethernet"
$rawFirmwarePath = Get-G2Path $script:G2RawFirmwareRelative
$rawSketchDir = Split-Path -Parent $rawFirmwarePath
$bridgePath = Join-Path $PSScriptRoot "g2_runner_snapshot_bridge.py"
$ipResolverPath = Join-Path $PSScriptRoot "a14_nb3_resolve_dut_ip.py"

foreach ($required in @($bridgePath, $ipResolverPath)) {
    if (-not (Test-Path -LiteralPath $required)) {
        throw "A14_NB3D3_REQUIRED_HELPER_NOT_FOUND=$required"
    }
}

$timestamp = Get-Date -Format "yyyyMMdd_HHmmss"
$tempRoot = Join-Path $env:TEMP ("jwplc_a14_nb3d3_udp_tx_{0}" -f $timestamp)
$buildPath = Join-Path $tempRoot "build"
$compileLog = Join-Path $tempRoot "compile.log"
$uploadLog = Join-Path $tempRoot "upload.log"
$resolverSyntaxLog = Join-Path $tempRoot "resolver_syntax.log"
$bridgeSyntaxLog = Join-Path $tempRoot "bridge_syntax.log"
$ipResolverLog = Join-Path $tempRoot "ip_resolver.log"

New-Item -ItemType Directory -Force -Path $buildPath | Out-Null

Write-Host "ARDUINO_CLI=$arduinoCli"
Write-Host "PYTHON=$pythonExe"
Write-Host "FQBN=$fqbn"
Write-Host "TEMP_ROOT=$tempRoot"
Write-Host "SOURCE_MUTATION=NO"

Write-Host ""
Write-Host "=== PYTHON HELPER SYNTAX PREFLIGHT ==="

$resolverSyntaxExit = Invoke-NB3NativeToLog -FilePath $pythonExe -Arguments @(
    "-m", "py_compile", $ipResolverPath
) -LogPath $resolverSyntaxLog

$bridgeSyntaxExit = Invoke-NB3NativeToLog -FilePath $pythonExe -Arguments @(
    "-m", "py_compile", $bridgePath
) -LogPath $bridgeSyntaxLog

Write-Host "IP_RESOLVER_SYNTAX_EXIT=$resolverSyntaxExit"
Write-Host "BRIDGE_SYNTAX_EXIT=$bridgeSyntaxExit"

if ($resolverSyntaxExit -ne 0 -or $bridgeSyntaxExit -ne 0) {
    throw "A14_NB3D3_PYTHON_HELPER_SYNTAX_FAILED"
}
Write-Host "PYTHON_HELPER_SYNTAX=PASS"

Write-Host ""
Write-Host "=== COMPILE RAW CANDIDATE ==="

$compileArgs = @(
    "compile",
    "--fqbn", $fqbn,
    "--build-path", $buildPath,
    "--libraries", $librariesRoot,
    $rawSketchDir
)

$compileExit = Invoke-NB3NativeToLog -FilePath $arduinoCli -Arguments $compileArgs -LogPath $compileLog

Write-Host "COMPILE_EXIT=$compileExit"
Write-Host "COMPILE_LOG=$compileLog"

if ($compileExit -ne 0) {
    Invoke-G2CompileFinishedSound -Success $false
    Get-Content -LiteralPath $compileLog -Tail 120 | ForEach-Object { Write-Host $_ }
    throw "A14_NB3D3_COMPILE_FAILED"
}

$compileText = [System.IO.File]::ReadAllText($compileLog)
$repoEthernetUsed = (
    $compileText.IndexOf(
        $expectedEthernetLibrary,
        [System.StringComparison]::OrdinalIgnoreCase
    ) -ge 0
)

Write-Host "REPO_ETHERNET_LIBRARY_USED=$repoEthernetUsed"

if (-not $repoEthernetUsed) {
    throw "A14_NB3D3_WRONG_ETHERNET_LIBRARY"
}

$binCount = @(
    Get-ChildItem -LiteralPath $buildPath -Recurse -File -Filter "*.bin"
).Count

Write-Host "BIN_COUNT=$binCount"

if ($binCount -lt 1) {
    Invoke-G2CompileFinishedSound -Success $false
    throw "A14_NB3D3_NO_BIN"
}

Invoke-G2CompileFinishedSound -Success $true
Write-Host "COMPILE_FINISHED_SOUND=PLAYED"

Write-Host ""
Write-Host "=== UPLOAD RAW CANDIDATE ==="

$uploadArgs = @(
    "upload",
    "--fqbn", $fqbn,
    "--port", $SerialPort,
    "--input-dir", $buildPath,
    $rawSketchDir
)

$uploadExit = Invoke-NB3NativeToLog -FilePath $arduinoCli -Arguments $uploadArgs -LogPath $uploadLog

Write-Host "UPLOAD_EXIT=$uploadExit"
Write-Host "UPLOAD_LOG=$uploadLog"

if ($uploadExit -ne 0) {
    Get-Content -LiteralPath $uploadLog -Tail 120 | ForEach-Object { Write-Host $_ }
    throw "A14_NB3D3_UPLOAD_FAILED"
}

Write-Host ""
Write-Host "=== RESOLVE EFFECTIVE DUT IP FROM SERIAL ==="

$ipResolverArgs = @(
    $ipResolverPath,
    "--serial", $SerialPort,
    "--timeout", "15"
)

$ipResolverExit = Invoke-NB3NativeToLog -FilePath $pythonExe -Arguments $ipResolverArgs -LogPath $ipResolverLog

Write-Host "IP_RESOLVER_EXIT=$ipResolverExit"
Write-Host "IP_RESOLVER_LOG=$ipResolverLog"

if ($ipResolverExit -ne 0) {
    Get-Content -LiteralPath $ipResolverLog -Tail 120 | ForEach-Object { Write-Host $_ }
    throw "A14_NB3D3_IP_RESOLVER_FAILED"
}

$ipResolverText = [System.IO.File]::ReadAllText($ipResolverLog)
$ipMatches = @([regex]::Matches(
    $ipResolverText,
    '(?m)^DUT_IP_EFFECTIVE=([0-9]{1,3}(?:\.[0-9]{1,3}){3})\r?$'
))

if ($ipMatches.Count -ne 1) {
    throw "A14_NB3D3_EFFECTIVE_IP_COUNT_INVALID=$($ipMatches.Count)"
}

$effectiveDutIp = $ipMatches[0].Groups[1].Value
Write-Host "DUT_IP_EFFECTIVE=$effectiveDutIp"

$durationText = $DurationSeconds.ToString(
    [System.Globalization.CultureInfo]::InvariantCulture
)

$dutRates = @()
$pcRates = @()
$holdValues = @()
$loopGapValues = @()

Write-Host ""
Write-Host "=== UDP TX THREE-RUN REGRESSION CHECK ==="

for ($run = 1; $run -le 3; ++$run) {
    $runLog = Join-Path $tempRoot ("udp_tx_run{0}.log" -f $run)

    $runnerArgs = @(
        $bridgePath,
        "--host", $effectiveDutIp,
        "--serial", $SerialPort,
        "--duration", $durationText,
        "--mode", "udp-tx",
        "--tcp-chunk", "4096",
        "--udp-payload", "1472"
    )

    $runExit = Invoke-NB3NativeToLog -FilePath $pythonExe -Arguments $runnerArgs -LogPath $runLog

    Write-Host "RUN=$run RUNNER_EXIT=$runExit LOG=$runLog"

    if ($runExit -ne 0) {
        Get-Content -LiteralPath $runLog -Tail 120 | ForEach-Object { Write-Host $_ }
        throw "A14_NB3D3_RUNNER_FAILED_RUN$run"
    }

    $runText = [System.IO.File]::ReadAllText($runLog)

    if ((Get-LogValue -Text $runText -Key "RAW_BENCH_FUNCTIONAL_PASS") -ne "YES") {
        throw "A14_NB3D3_FUNCTIONAL_FAIL_RUN$run"
    }

    if ((Get-LogValue -Text $runText -Key "DUT_READY") -ne "YES") {
        throw "A14_NB3D3_DUT_NOT_READY_RUN$run"
    }

    if ((Get-LogValue -Text $runText -Key "G2_FINAL_SNAPSHOT_PRESENT") -ne "YES") {
        throw "A14_NB3D3_FINAL_SNAPSHOT_MISSING_RUN$run"
    }

    Assert-ExactValue -Text $runText -Key "RAW_SERVER_READY" -Expected "YES"
    Assert-ExactValue -Text $runText -Key "ETH_READY" -Expected "YES"
    Assert-ExactValue -Text $runText -Key "ETH_LINK" -Expected "UP"
    Assert-ExactValue -Text $runText -Key "IP" -Expected $effectiveDutIp

    @(
        "TRANSPORT_ERRORS",
        "UDP_BEGIN_PACKET_ERRORS",
        "UDP_WRITE_ERRORS",
        "UDP_END_PACKET_ERRORS",
        "UDP_SPI_LOCK_ERRORS",
        "TCP_SPI_LOCK_ERRORS",
        "UDP_LAST_SHORT_WRITE_BYTES"
    ) | ForEach-Object {
        Assert-ExactZero -Text $runText -Key $_
    }

    @(
        "UDP_TX_SEQUENCE_DECODE_ERRORS",
        "UDP_TX_SEQUENCE_TOTAL_DUPLICATES",
        "UDP_TX_SEQUENCE_REORDERS",
        "UDP_TX_SEQUENCE_TOTAL_RANGE_MISSING",
        "UDP_TX_UNEXPECTED_SOURCE_PACKETS",
        "UDP_TX_WRONG_SIZE_FROM_DUT"
    ) | ForEach-Object {
        $integrityValue = Get-LogInt64 -Text $runText -Key $_
        Write-Host "RUN=$run $_=$integrityValue"

        if ($integrityValue -ne 0) {
            throw "A14_NB3D3_UDP_TX_INTEGRITY_FAIL_RUN$run_$($_)=$integrityValue"
        }
    }

    $sequenceCount = Get-LogInt64 -Text $runText -Key "UDP_TX_SEQUENCE_TOTAL_COUNT"
    $pcMbps = Get-LogDouble -Text $runText -Key "SUMMARY_UDP_TX_PC_MBPS"
    $dutMbps = Get-LogDouble -Text $runText -Key "SUMMARY_UDP_TX_DUT_MBPS"
    $holdMaxUs = Get-LogInt64 -Text $runText -Key "G2_FINAL_TCP_SPI_HOLD_MAX_US"
    $loopGapMaxUs = Get-LogInt64 -Text $runText -Key "G2_FINAL_LOOP_GAP_MAX_US"

    Write-Host "RUN=$run UDP_TX_SEQUENCE_TOTAL_COUNT=$sequenceCount"
    Write-Host "RUN=$run PC_MBPS=$pcMbps DUT_MBPS=$dutMbps HOLD_MAX_US=$holdMaxUs LOOP_GAP_MAX_US=$loopGapMaxUs"

    if ($sequenceCount -le 0) {
        throw "A14_NB3D3_UDP_TX_SEQUENCE_EMPTY_RUN$run"
    }

    if ($holdMaxUs -gt 10000) {
        throw "A14_NB3D3_HARD_HOLD_REGRESSION_RUN$run=$holdMaxUs"
    }

    if ($loopGapMaxUs -gt 15000) {
        throw "A14_NB3D3_LOOP_GAP_REGRESSION_RUN$run=$loopGapMaxUs"
    }

    $pcRates += $pcMbps
    $dutRates += $dutMbps
    $holdValues += $holdMaxUs
    $loopGapValues += $loopGapMaxUs

    Write-Host "NB3_D3_RUN_$run=PASS"
    Start-Sleep -Milliseconds 400
}

$sortedPc = @($pcRates | Sort-Object)
$sortedDut = @($dutRates | Sort-Object)
$medianPc = [double]$sortedPc[1]
$medianDut = [double]$sortedDut[1]
$maxHold = [int64](($holdValues | Measure-Object -Maximum).Maximum)
$maxLoopGap = [int64](($loopGapValues | Measure-Object -Maximum).Maximum)
$dutRatio = $medianDut / $baselineDutMbps
$dutPct = [math]::Round($dutRatio * 100.0, 1)

Write-Host ""
Write-Host "=== NB3-D3 SUMMARY ==="
Write-Host "UDP_TX_BASELINE_PC_MBPS=$baselinePcMbps"
Write-Host "UDP_TX_BASELINE_DUT_MBPS=$baselineDutMbps"
Write-Host "UDP_TX_MEDIAN_PC_MBPS=$medianPc"
Write-Host "UDP_TX_MEDIAN_DUT_MBPS=$medianDut"
Write-Host "UDP_TX_MEDIAN_DUT_BASELINE_PCT=$dutPct"
Write-Host "UDP_TX_MAX_HOLD_US=$maxHold"
Write-Host "UDP_TX_MAX_LOOP_GAP_US=$maxLoopGap"

if ($medianDut -lt $hardFloorDutMbps) {
    throw "A14_NB3D3_THROUGHPUT_REGRESSION median=$medianDut floor=$hardFloorDutMbps"
}

Write-Host ""
Write-Host "OBSERVACION FISICA REQUERIDA: mira la TFT durante NB3-D3."
$visualAnswer = ""

while ($visualAnswer -notin @("S", "N")) {
    $visualAnswer = (Read-Host 'Aparecio el diagnostico visual "SPI" durante NB3-D3? (S/N)').Trim().ToUpperInvariant()
}

$visualEvents = if ($visualAnswer -eq "S") { 1 } else { 0 }
Write-Host "VISUAL_SPI_EVENTS=$visualEvents"

if ($visualEvents -ne 0) {
    throw "A14_NB3D3_VISUAL_SPI_FAIL"
}

Assert-G2ProtectedArtifacts

$dirtyFinal = @(Get-G2TrackedDirtyPaths)
$stagedFinal = @(& git -C $script:G2RepoRoot diff --cached --name-only)

Write-Host "TRACKED_DIRTY_COUNT_FINAL=$($dirtyFinal.Count)"
$dirtyFinal | ForEach-Object { Write-Host "DIRTY_FINAL=$_" }
Write-Host "STAGED_COUNT_FINAL=$($stagedFinal.Count)"

if ($dirtyFinal.Count -ne $expectedDirty.Count -or $stagedFinal.Count -ne 0) {
    throw "A14_NB3D3_FINAL_WORKTREE_INVALID"
}

for ($i = 0; $i -lt $expectedDirty.Count; ++$i) {
    if ($dirtyFinal[$i] -ne $expectedDirty[$i]) {
        throw "A14_NB3D3_FINAL_DIRTY_PATH_INVALID=$($dirtyFinal[$i])"
    }
}

Write-Host "NB3_D3_UDP_TX_RUNS=3_PASS"
Write-Host "NB3_D3_UDP_TX_INTEGRITY=PASS"
Write-Host "NB3_D3_UDP_TX_TRANSPORT_ERRORS=ZERO"
Write-Host "NB3_D3_UDP_TX_SPI_LOCK_ERRORS=ZERO"
Write-Host "NB3_D3_UDP_TX_HOLD_10MS=PASS"
Write-Host "NB3_D3_UDP_TX_LOOP_GAP_15MS=PASS"
Write-Host "NB3_D3_UDP_TX_MEDIAN_GE_90PCT_BASELINE=PASS"
Write-Host "NB3_D3_VISUAL_SPI=PASS"
Write-Host "NB3_UDP_SEND_COOPERATIVE_ENGINE=PHYSICAL_CLOSED_PASS"
Write-Host "A14_NB3_D3_UDP_TX_REGRESSION_PHYSICAL=PASS"
