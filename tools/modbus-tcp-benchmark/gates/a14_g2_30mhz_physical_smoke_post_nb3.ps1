param(
    [string]$SerialPort = "COM14",
    [double]$DurationSeconds = 3.0
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"
. (Join-Path $PSScriptRoot "common.ps1")

$expectedW5100Hash30 = "B95324195AD1ACDE923734826DCD36B2266B6829C2B9D828765B4944B59E9993"
$expectedRawFirmwareHash = "9A0C6CF27E44A61DC97686FB1A769EBBBB34D036CDF8D0CA0EEAB597E699AB6B"
$expectedRunnerHash = "FD25997CFE777B4FF50BBC10EFE555BD6B7A8A8DCE9D4ADD1E7D2DBA6D52470F"
$preferredHoldUs = [int64]5000
$hardHoldUs = [int64]10000

function Invoke-NativeToLog {
    param([string]$FilePath, [string[]]$Arguments, [string]$LogPath)
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
    param([string]$Text, [string]$Key)
    $pattern = "(?m)^" + [regex]::Escape($Key) + "=(.*)\r?$"
    $matches = @([regex]::Matches($Text, $pattern))
    if ($matches.Count -ne 1) {
        throw ("A14_G2_30_LOG_KEY_COUNT_{0}={1}" -f $Key, $matches.Count)
    }
    return $matches[0].Groups[1].Value.Trim()
}

function Get-LogInt64 {
    param([string]$Text, [string]$Key)
    return [int64]::Parse(
        (Get-LogValue -Text $Text -Key $Key),
        [System.Globalization.CultureInfo]::InvariantCulture
    )
}

function Resolve-DutIp {
    param([string]$PythonExe, [string]$ResolverPath, [string]$Port, [string]$LogPath, [string]$Phase)

    $exitCode = Invoke-NativeToLog -FilePath $PythonExe -Arguments @(
        $ResolverPath, "--serial", $Port, "--timeout", "15"
    ) -LogPath $LogPath

    Write-Host ("{0}_IP_RESOLVER_EXIT={1}" -f $Phase, $exitCode)
    Write-Host ("{0}_IP_RESOLVER_LOG={1}" -f $Phase, $LogPath)

    if ($exitCode -ne 0) {
        Get-Content -LiteralPath $LogPath -Tail 120 | ForEach-Object { Write-Host $_ }
        throw ("A14_G2_30_{0}_IP_RESOLVER_FAILED={1}" -f $Phase, $exitCode)
    }

    $text = [System.IO.File]::ReadAllText($LogPath)
    $ready = Get-LogValue -Text $text -Key "DUT_SERIAL_READY"
    $ip = Get-LogValue -Text $text -Key "DUT_IP_EFFECTIVE"
    $source = Get-LogValue -Text $text -Key "DUT_IP_SOURCE"

    Write-Host ("{0}_DUT_SERIAL_READY={1}" -f $Phase, $ready)
    Write-Host ("{0}_DUT_IP_EFFECTIVE={1}" -f $Phase, $ip)
    Write-Host ("{0}_DUT_IP_SOURCE={1}" -f $Phase, $source)

    if ($ready -ne "YES" -or $source -ne "RAW_SERIAL_SNAPSHOT") {
        throw ("A14_G2_30_{0}_IP_PREFLIGHT_INVALID" -f $Phase)
    }

    return $ip
}

function Assert-SourceState30 {
    Assert-G2ProtectedArtifacts

    $effectiveHz = Get-G2SpiHz
    Write-Host "EFFECTIVE_SPI_HZ=$effectiveHz"
    if ($effectiveHz -ne 30000000) {
        throw "A14_G2_30_EFFECTIVE_SPI_HZ_MISMATCH"
    }

    $w5100Hash = Get-G2Sha256 $script:G2SpiHeaderRelative
    $firmwareHash = Get-G2Sha256 $script:G2RawFirmwareRelative
    $runnerHash = Get-G2Sha256 $script:G2RawRunnerRelative

    Write-Host "W5100_H_SHA256=$w5100Hash"
    Write-Host "RAW_FIRMWARE_SHA256=$firmwareHash"
    Write-Host "RAW_RUNNER_SHA256=$runnerHash"

    if ($w5100Hash -ne $expectedW5100Hash30) { throw "A14_G2_30_W5100_HASH_MISMATCH" }
    if ($firmwareHash -ne $expectedRawFirmwareHash) { throw "A14_G2_30_RAW_FIRMWARE_HASH_MISMATCH" }
    if ($runnerHash -ne $expectedRunnerHash) { throw "A14_G2_30_RAW_RUNNER_HASH_MISMATCH" }

    $dirty = @(Get-G2TrackedDirtyPaths)
    Write-Host "TRACKED_DIRTY_COUNT=$($dirty.Count)"
    $dirty | ForEach-Object { Write-Host "DIRTY=$_" }

    if ($dirty.Count -ne 1 -or $dirty[0] -ne $script:G2SpiHeaderRelative) {
        throw "A14_G2_30_DIRTY_STATE_INVALID"
    }

    $staged = @(& git -C $script:G2RepoRoot diff --cached --name-only)
    if ($LASTEXITCODE -ne 0 -or $staged.Count -ne 0) {
        throw "A14_G2_30_INDEX_NOT_CLEAN"
    }
    Write-Host "STAGED_COUNT=0"
}

Write-Host "============================================================"
Write-Host " A14 G2 - 30 MHz PHYSICAL SMOKE POST-NB3"
Write-Host "============================================================"

Assert-G2Branch
Assert-SourceState30

if ($DurationSeconds -le 0) { throw "A14_G2_30_DURATION_INVALID" }

$head = Get-G2Head
Write-Host "HEAD=$head"
Write-Host "SERIAL_PORT=$SerialPort"
Write-Host "DURATION_S=$DurationSeconds"
Write-Host "HOLD_PREFERRED_US=$preferredHoldUs"
Write-Host "HOLD_HARD_LIMIT_US=$hardHoldUs"

$pythonCommand = Get-Command python.exe -ErrorAction SilentlyContinue
if ($null -eq $pythonCommand) { $pythonCommand = Get-Command python -ErrorAction SilentlyContinue }
if ($null -eq $pythonCommand) { throw "A14_G2_30_PYTHON_NOT_FOUND" }

$pythonExe = $pythonCommand.Source
$arduinoCli = "C:\Program Files\Arduino PLC IDE Tools\arduino-cli.exe"
if (-not (Test-Path -LiteralPath $arduinoCli)) { throw "A14_G2_30_ARDUINO_CLI_NOT_FOUND" }

$resolverPath = Join-Path $PSScriptRoot "a14_nb3_resolve_dut_ip.py"
$bridgePath = Join-Path $PSScriptRoot "g2_runner_snapshot_bridge.py"
$firmwarePath = Get-G2Path $script:G2RawFirmwareRelative
$sketchDir = Split-Path -Parent $firmwarePath
$librariesRoot = Get-G2Path "JWPLC/2.1.0/libraries"
$expectedEthernetLibrary = Join-Path $librariesRoot "JWPLC_Ethernet"
$fqbn = "jwplc_local:esp32:jwplcbasic"

$timestamp = Get-Date -Format "yyyyMMdd_HHmmss"
$tempRoot = Join-Path $env:TEMP ("jwplc_a14_g2_30mhz_smoke_{0}" -f $timestamp)
$buildPath = Join-Path $tempRoot "build"
New-Item -ItemType Directory -Force -Path $buildPath | Out-Null

Write-Host "PYTHON=$pythonExe"
Write-Host "IP_RESOLVER=$resolverPath"
Write-Host "TEMP_ROOT=$tempRoot"

$pyLog = Join-Path $tempRoot "py_compile.log"
$pyExit = Invoke-NativeToLog -FilePath $pythonExe -Arguments @(
    "-m", "py_compile", $resolverPath, $bridgePath
) -LogPath $pyLog
Write-Host "PY_COMPILE_EXIT=$pyExit"
if ($pyExit -ne 0) { throw "A14_G2_30_PYTHON_SYNTAX_FAILED" }

Write-Host ""
Write-Host "=== PRE-UPLOAD CONNECTIVITY PREFLIGHT ==="
$preUploadIp = Resolve-DutIp -PythonExe $pythonExe -ResolverPath $resolverPath -Port $SerialPort -LogPath (Join-Path $tempRoot "preupload_ip.log") -Phase "PREUPLOAD"

Write-Host ""
Write-Host "=== COMPILE 30 MHz ==="
$compileLog = Join-Path $tempRoot "compile.log"
$compileExit = Invoke-NativeToLog -FilePath $arduinoCli -Arguments @(
    "compile", "--fqbn", $fqbn,
    "--build-path", $buildPath,
    "--libraries", $librariesRoot,
    $sketchDir
) -LogPath $compileLog

Write-Host "COMPILE_EXIT=$compileExit"
Write-Host "COMPILE_LOG=$compileLog"

if ($compileExit -ne 0) {
    Invoke-G2CompileFinishedSound -Success $false
    Get-Content -LiteralPath $compileLog -Tail 120 | ForEach-Object { Write-Host $_ }
    throw "A14_G2_30_COMPILE_FAILED"
}

$compileText = [System.IO.File]::ReadAllText($compileLog)
$repoEthernetUsed = $compileText.IndexOf(
    $expectedEthernetLibrary,
    [System.StringComparison]::OrdinalIgnoreCase
) -ge 0

Write-Host "REPO_ETHERNET_LIBRARY_USED=$repoEthernetUsed"
if (-not $repoEthernetUsed) { throw "A14_G2_30_WRONG_ETHERNET_LIBRARY" }

$binCount = @(Get-ChildItem -LiteralPath $buildPath -Recurse -File -Filter "*.bin").Count
Write-Host "BIN_COUNT=$binCount"
if ($binCount -lt 1) { throw "A14_G2_30_BIN_MISSING" }

Invoke-G2CompileFinishedSound -Success $true
Assert-SourceState30

Write-Host ""
Write-Host "=== UPLOAD 30 MHz ==="
$uploadLog = Join-Path $tempRoot "upload.log"
$uploadExit = Invoke-NativeToLog -FilePath $arduinoCli -Arguments @(
    "upload", "--fqbn", $fqbn,
    "--port", $SerialPort,
    "--input-dir", $buildPath,
    $sketchDir
) -LogPath $uploadLog

Write-Host "UPLOAD_EXIT=$uploadExit"
Write-Host "UPLOAD_LOG=$uploadLog"

if ($uploadExit -ne 0) {
    Get-Content -LiteralPath $uploadLog -Tail 120 | ForEach-Object { Write-Host $_ }
    throw "A14_G2_30_UPLOAD_FAILED"
}

Start-Sleep -Seconds 2

Write-Host ""
Write-Host "=== POST-UPLOAD IP RESOLUTION ==="
$postUploadIp = Resolve-DutIp -PythonExe $pythonExe -ResolverPath $resolverPath -Port $SerialPort -LogPath (Join-Path $tempRoot "postupload_ip.log") -Phase "POSTUPLOAD"
$ipChanged = if ($postUploadIp -eq $preUploadIp) { "NO" } else { "YES" }
Write-Host "IP_CHANGED_AFTER_UPLOAD=$ipChanged"

$modeMap = @(
    [pscustomobject]@{ Cli = "tcp-rx"; Key = "TCP_RX" },
    [pscustomobject]@{ Cli = "tcp-tx"; Key = "TCP_TX" },
    [pscustomobject]@{ Cli = "udp-rx"; Key = "UDP_RX" },
    [pscustomobject]@{ Cli = "udp-tx"; Key = "UDP_TX" }
)

$durationText = $DurationSeconds.ToString([System.Globalization.CultureInfo]::InvariantCulture)
$summary = New-Object System.Collections.Generic.List[string]
$summary.Add("PREUPLOAD_IP=$preUploadIp")
$summary.Add("POSTUPLOAD_IP=$postUploadIp")
$summary.Add("DURATION_S=$durationText")
$reviewRequired = $false

Write-Host ""
Write-Host "=== FOUR-MODE EXACT SMOKE @ 30 MHz ==="

foreach ($mode in $modeMap) {
    $runLog = Join-Path $tempRoot ("{0}.log" -f $mode.Key.ToLowerInvariant())
    $runExit = Invoke-NativeToLog -FilePath $pythonExe -Arguments @(
        $bridgePath,
        "--host", $postUploadIp,
        "--serial", $SerialPort,
        "--duration", $durationText,
        "--mode", $mode.Cli,
        "--tcp-chunk", "4096",
        "--udp-payload", "1472"
    ) -LogPath $runLog

    Write-Host "MODE=$($mode.Key) RUNNER_EXIT=$runExit LOG=$runLog"

    if ($runExit -ne 0) {
        Get-Content -LiteralPath $runLog -Tail 140 | ForEach-Object { Write-Host $_ }
        throw "A14_G2_30_RUNNER_FAILED_$($mode.Key)"
    }

    $text = [System.IO.File]::ReadAllText($runLog)
    if ((Get-LogValue -Text $text -Key "RAW_BENCH_FUNCTIONAL_PASS") -ne "YES") {
        throw "A14_G2_30_FUNCTIONAL_FAIL_$($mode.Key)"
    }
    if ((Get-LogValue -Text $text -Key "G2_FINAL_SNAPSHOT_PRESENT") -ne "YES") {
        throw "A14_G2_30_SNAPSHOT_MISSING_$($mode.Key)"
    }
    if ((Get-LogValue -Text $text -Key "G2_FINAL_IP") -ne $postUploadIp) {
        throw "A14_G2_30_IP_MISMATCH_$($mode.Key)"
    }
    if ((Get-LogValue -Text $text -Key "G2_FINAL_ETH_LINK") -ne "UP") {
        throw "A14_G2_30_LINK_DOWN_$($mode.Key)"
    }

    foreach ($key in @(
        "G2_FINAL_TRANSPORT_ERRORS",
        "G2_FINAL_UDP_BEGIN_PACKET_ERRORS",
        "G2_FINAL_UDP_WRITE_ERRORS",
        "G2_FINAL_UDP_END_PACKET_ERRORS",
        "G2_FINAL_UDP_SPI_LOCK_ERRORS",
        "G2_FINAL_TCP_SPI_LOCK_ERRORS",
        "G2_FINAL_UDP_LAST_SHORT_WRITE_BYTES"
    )) {
        $value = Get-LogInt64 -Text $text -Key $key
        if ($value -ne 0) { throw "A14_G2_30_NONZERO_$key=$value" }
    }

    $pcMbps = Get-LogValue -Text $text -Key ("SUMMARY_{0}_PC_MBPS" -f $mode.Key)
    $dutMbps = Get-LogValue -Text $text -Key ("SUMMARY_{0}_DUT_MBPS" -f $mode.Key)
    $lossPct = Get-LogValue -Text $text -Key ("SUMMARY_{0}_LOSS_PERCENT" -f $mode.Key)
    $holdAvgUs = Get-LogInt64 -Text $text -Key "G2_FINAL_TCP_SPI_HOLD_AVG_US"
    $holdMaxUs = Get-LogInt64 -Text $text -Key "G2_FINAL_TCP_SPI_HOLD_MAX_US"
    $loopGapMaxUs = Get-LogInt64 -Text $text -Key "G2_FINAL_LOOP_GAP_MAX_US"

    $holdClass = "PASS"
    if ($holdMaxUs -gt $hardHoldUs) {
        $holdClass = "FAIL_10MS"
    }
    elseif ($holdMaxUs -gt $preferredHoldUs) {
        $holdClass = "REVIEW_5MS"
        $reviewRequired = $true
    }

    Write-Host ("RUN MODE={0} PC_MBPS={1} DUT_MBPS={2} LOSS_PCT={3} HOLD_AVG_US={4} HOLD_MAX_US={5} LOOP_GAP_MAX_US={6} HOLD_CLASS={7}" -f
        $mode.Key, $pcMbps, $dutMbps, $lossPct, $holdAvgUs, $holdMaxUs, $loopGapMaxUs, $holdClass)

    if ($holdMaxUs -gt $hardHoldUs) {
        throw "A14_G2_30_HOLD_HARD_LIMIT_EXCEEDED_$($mode.Key)=$holdMaxUs"
    }

    if ($mode.Key -eq "UDP_TX") {
        foreach ($key in @(
            "UDP_TX_SEQUENCE_DECODE_ERRORS",
            "UDP_TX_SEQUENCE_TOTAL_DUPLICATES",
            "UDP_TX_SEQUENCE_REORDERS",
            "UDP_TX_SEQUENCE_TOTAL_RANGE_MISSING",
            "UDP_TX_UNEXPECTED_SOURCE_PACKETS",
            "UDP_TX_WRONG_SIZE_FROM_DUT"
        )) {
            $value = Get-LogInt64 -Text $text -Key $key
            if ($value -ne 0) { throw "A14_G2_30_UDP_TX_INTEGRITY_FAIL_$key=$value" }
        }
    }

    $summary.Add(("{0}_PC_MBPS={1}" -f $mode.Key, $pcMbps))
    $summary.Add(("{0}_DUT_MBPS={1}" -f $mode.Key, $dutMbps))
    $summary.Add(("{0}_LOSS_PCT={1}" -f $mode.Key, $lossPct))
    $summary.Add(("{0}_HOLD_MAX_US={1}" -f $mode.Key, $holdMaxUs))
    $summary.Add(("{0}_LOOP_GAP_MAX_US={1}" -f $mode.Key, $loopGapMaxUs))
    $summary.Add(("{0}_HOLD_CLASS={1}" -f $mode.Key, $holdClass))
}

Assert-SourceState30

Write-Host ""
Write-Host "OBSERVACION FISICA: mira la TFT durante las cuatro corridas."
$visualAnswer = ""
while ($visualAnswer -notin @("S", "N")) {
    $visualAnswer = (Read-Host 'Aparecio el diagnostico visual "SPI" durante el gate? (S/N)').Trim().ToUpperInvariant()
}

$visualEvents = if ($visualAnswer -eq "S") { 1 } else { 0 }
Write-Host "VISUAL_SPI_EVENTS=$visualEvents"

$result = "PASS"
if ($visualEvents -ne 0) {
    $result = "FAIL"
}
elseif ($reviewRequired) {
    $result = "PASS_WITH_HOLD_REVIEW"
}

$summary.Add("VISUAL_SPI_EVENTS=$visualEvents")
$summary.Add("COMPILE_EXIT=$compileExit")
$summary.Add("UPLOAD_EXIT=$uploadExit")
$summary.Add("A14_G2_30MHZ_PHYSICAL_SMOKE=$result")
$summaryPath = Join-Path $tempRoot "summary.log"
$summary | Set-Content -LiteralPath $summaryPath -Encoding UTF8

Write-Host ""
Write-Host "============================================================"
Write-Host " A14 G2 30 MHz PHYSICAL SMOKE SUMMARY"
Write-Host "============================================================"
Write-Host "PREUPLOAD_IP=$preUploadIp"
Write-Host "POSTUPLOAD_IP=$postUploadIp"
Write-Host "COMPILE_EXIT=$compileExit"
Write-Host "UPLOAD_EXIT=$uploadExit"
Write-Host "FOUR_MODES_FUNCTIONAL_PASS=YES"
Write-Host "VISUAL_SPI_EVENTS=$visualEvents"
Write-Host "SUMMARY_LOG=$summaryPath"
Write-Host "A14_G2_30MHZ_PHYSICAL_SMOKE=$result"

if ($result -eq "FAIL") { exit 2 }
