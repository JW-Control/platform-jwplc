param(
    [int]$MHz = 26,
    [int]$ExpectedChunks = 8,
    [int]$TimeoutMs = 200,
    [string]$SerialPort = "COM14",
    [string]$DutIp = "192.168.0.31",
    [double]$DurationSeconds = 2.0,
    [int]$Pairs = 12
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"
. (Join-Path $PSScriptRoot "common.ps1")

$expectedSourceFirmwareSha256 = "08E16E60F008371416859B63F93D7C8B7D2FB88325FB7281AC6C5C72983FEA8F"
$oneSecondMinUs = 900000
$oneSecondMaxUs = 1100000
$shiftedMinUs = [int64]([Math]::Max(50000, $TimeoutMs * 1000 - 50000))
$shiftedMaxUs = [int64]($TimeoutMs * 1000 + 100000)

function Invoke-NativeToLog {
    param([string]$FilePath, [string[]]$Arguments, [string]$LogPath)
    & $FilePath @Arguments *> $LogPath
    return [int]$LASTEXITCODE
}

function Get-LogValue {
    param([string]$Text, [string]$Key)
    $pattern = "(?m)^" + [regex]::Escape($Key) + "=(.*)\r?$"
    $matches = @([regex]::Matches($Text, $pattern))
    if ($matches.Count -ne 1) { throw "G3_AB_LOG_KEY_COUNT_${Key}=$($matches.Count)" }
    return $matches[0].Groups[1].Value.Trim()
}

function Get-LogInt64 {
    param([string]$Text, [string]$Key)
    return [int64]::Parse((Get-LogValue -Text $Text -Key $Key), [System.Globalization.CultureInfo]::InvariantCulture)
}

function Get-ChunkCountFromText {
    param([string]$Text)
    $pattern = '(?m)^\s*static constexpr uint8_t TCP_RX_MAX_CHUNKS_PER_LOCK = (\d+);\s*$'
    $matches = @([regex]::Matches($Text, $pattern))
    if ($matches.Count -ne 1) { throw "G3_AB_CHUNK_DECLARATION_COUNT=$($matches.Count)" }
    return [int]$matches[0].Groups[1].Value
}

Write-Host "============================================================"
Write-Host " G3 STOP TIMEOUT A/B DIAGNOSTIC"
Write-Host "============================================================"

Assert-G2Branch
Assert-G2AllowedMHz -MHz $MHz
if ($TimeoutMs -lt 1 -or $DurationSeconds -le 0 -or $Pairs -lt 1) { throw "G3_AB_INVALID_ARGUMENTS" }

$head = Get-G2Head
$targetHz = [int64]$MHz * 1000000
$effectiveHz = Get-G2SpiHz
$sourceFirmwarePath = Get-G2Path $script:G2RawFirmwareRelative
$sourceText = [System.IO.File]::ReadAllText($sourceFirmwarePath)
$effectiveChunks = Get-ChunkCountFromText -Text $sourceText

Write-Host "HEAD=$head"
Write-Host "TARGET_MHZ=$MHz"
Write-Host "EFFECTIVE_SPI_HZ=$effectiveHz"
Write-Host "EXPECTED_CHUNKS=$ExpectedChunks"
Write-Host "EFFECTIVE_CHUNKS=$effectiveChunks"
Write-Host "CONTROL_TIMEOUT_MS=1000"
Write-Host "DIAG_TIMEOUT_MS=$TimeoutMs"
Write-Host "PAIRS=$Pairs"
Write-Host "DURATION_S=$DurationSeconds"
Write-Host "ONE_SECOND_SIGNATURE_MIN_US=$oneSecondMinUs"
Write-Host "ONE_SECOND_SIGNATURE_MAX_US=$oneSecondMaxUs"
Write-Host "SHIFTED_SIGNATURE_MIN_US=$shiftedMinUs"
Write-Host "SHIFTED_SIGNATURE_MAX_US=$shiftedMaxUs"
Write-Host "SOURCE_MUTATION=NO"
Write-Host "TEMP_SKETCH_MUTATION=YES"
Write-Host "SNAPSHOT_SOURCE=FROZEN_RUNNER_EXACT_FINAL"

if ($effectiveHz -ne $targetHz) { throw "G3_AB_SPI_FREQUENCY_MISMATCH" }
if ($effectiveChunks -ne $ExpectedChunks) { throw "G3_AB_CHUNKS_MISMATCH" }

$dirty = @(Get-G2TrackedDirtyPaths)
$expectedDirty = @($script:G2SpiHeaderRelative, $script:G2RawFirmwareRelative) | Sort-Object
Write-Host "TRACKED_DIRTY_COUNT=$($dirty.Count)"
$dirty | ForEach-Object { Write-Host "DIRTY=$_" }
if ($dirty.Count -ne 2) { throw "G3_AB_DIRTY_COUNT_INVALID" }
for ($i = 0; $i -lt 2; ++$i) {
    if ($dirty[$i] -ne $expectedDirty[$i]) { throw "G3_AB_DIRTY_PATH_INVALID=$($dirty[$i])" }
}

$staged = @(& git -C $script:G2RepoRoot diff --cached --name-only)
if ($LASTEXITCODE -ne 0 -or $staged.Count -ne 0) { throw "G3_AB_INDEX_NOT_CLEAN" }
Write-Host "STAGED_COUNT=0"

Assert-G2ProtectedArtifacts
$sourceHash = Get-G2Sha256 $script:G2RawFirmwareRelative
$runnerHash = Get-G2Sha256 $script:G2RawRunnerRelative
Write-Host "G3_SOURCE_FIRMWARE_SHA256=$sourceHash"
Write-Host "RAW_RUNNER_SHA256=$runnerHash"
if ($sourceHash -ne $expectedSourceFirmwareSha256) { throw "G3_AB_SOURCE_FIRMWARE_HASH_MISMATCH" }
if ($runnerHash -ne $script:G2RawRunnerSha256) { throw "G3_AB_RUNNER_HASH_MISMATCH" }

$arduinoCli = "C:\Program Files\Arduino PLC IDE Tools\arduino-cli.exe"
if (-not (Test-Path -LiteralPath $arduinoCli)) { throw "G3_AB_ARDUINO_CLI_NOT_FOUND" }
$pythonCommand = Get-Command python.exe -ErrorAction SilentlyContinue
if ($null -eq $pythonCommand) { $pythonCommand = Get-Command python -ErrorAction SilentlyContinue }
if ($null -eq $pythonCommand) { throw "G3_AB_PYTHON_NOT_FOUND" }
$pythonExe = $pythonCommand.Source
$bridgePath = Join-Path $PSScriptRoot "g2_runner_snapshot_bridge.py"
if (-not (Test-Path -LiteralPath $bridgePath)) { throw "G3_AB_BRIDGE_NOT_FOUND" }

$timestamp = Get-Date -Format "yyyyMMdd_HHmmss"
$tempRoot = Join-Path $env:TEMP ("jwplc_g3_stop_timeout_ab_{0}mhz_{1}chunks_{2}ms_{3}" -f $MHz, $ExpectedChunks, $TimeoutMs, $timestamp)
$tempSketchDir = Join-Path $tempRoot "eth14_raw_transport_server"
$buildPath = Join-Path $tempRoot "build"
$compileLog = Join-Path $tempRoot "compile.log"
$uploadLog = Join-Path $tempRoot "upload.log"
New-Item -ItemType Directory -Force -Path $tempSketchDir | Out-Null
New-Item -ItemType Directory -Force -Path $buildPath | Out-Null

$sourceSketchDir = Split-Path -Parent $sourceFirmwarePath
Get-ChildItem -LiteralPath $sourceSketchDir -File | Copy-Item -Destination $tempSketchDir
$tempFirmwarePath = Join-Path $tempSketchDir "eth14_raw_transport_server.ino"
if (-not (Test-Path -LiteralPath $tempFirmwarePath)) { throw "G3_AB_TEMP_FIRMWARE_MISSING" }

$tempText = [System.IO.File]::ReadAllText($tempFirmwarePath)
$anchor = "    tcpClient = tcpServer.accept();`r`n`r`n    if (tcpClient)`r`n    {"
if ($tempText.IndexOf($anchor, [System.StringComparison]::Ordinal) -lt 0) {
    $anchor = "    tcpClient = tcpServer.accept();`n`n    if (tcpClient)`n    {"
}
$anchorCount = ([regex]::Matches($tempText, [regex]::Escape($anchor))).Count
Write-Host "TIMEOUT_INJECTION_ANCHOR_COUNT=$anchorCount"
if ($anchorCount -ne 1) { throw "G3_AB_TIMEOUT_INJECTION_ANCHOR_COUNT=$anchorCount" }

$replacement = $anchor + "`n        tcpClient.setConnectionTimeout($TimeoutMs);"
if ($anchor.Contains("`r`n")) {
    $replacement = $anchor + "`r`n        tcpClient.setConnectionTimeout($TimeoutMs);"
}
$tempText = $tempText.Replace($anchor, $replacement)
$utf8NoBom = New-Object -TypeName System.Text.UTF8Encoding -ArgumentList $false
[System.IO.File]::WriteAllText($tempFirmwarePath, $tempText, $utf8NoBom)

$patchedText = [System.IO.File]::ReadAllText($tempFirmwarePath)
$timeoutPattern = '(?m)^\s*tcpClient\.setConnectionTimeout\(' + $TimeoutMs + '\);\s*$'
$timeoutMatches = @([regex]::Matches($patchedText, $timeoutPattern))
Write-Host "TEMP_TIMEOUT_DECLARATION_COUNT=$($timeoutMatches.Count)"
if ($timeoutMatches.Count -ne 1) { throw "G3_AB_TEMP_TIMEOUT_DECLARATION_COUNT=$($timeoutMatches.Count)" }
if ((Get-ChunkCountFromText -Text $patchedText) -ne $ExpectedChunks) { throw "G3_AB_TEMP_CHUNKS_CHANGED" }

$tempStream = [System.IO.File]::OpenRead($tempFirmwarePath)
try {
    $sha = [System.Security.Cryptography.SHA256]::Create()
    try { $bytes = $sha.ComputeHash($tempStream) } finally { $sha.Dispose() }
} finally { $tempStream.Dispose() }
$tempHash = ([System.BitConverter]::ToString($bytes)).Replace("-", "").ToUpperInvariant()
Write-Host "TEMP_DIAG_FIRMWARE_SHA256=$tempHash"
Write-Host "TEMP_ROOT=$tempRoot"

$librariesRoot = Get-G2Path "JWPLC/2.1.0/libraries"
$expectedEthernetLibrary = Join-Path $librariesRoot "JWPLC_Ethernet"
$fqbn = "jwplc_local:esp32:jwplcbasic"

Write-Host ""
Write-Host "=== COMPILE TEMP DIAGNOSTIC FIRMWARE ==="
$compileArgs = @("compile", "--fqbn", $fqbn, "--build-path", $buildPath, "--libraries", $librariesRoot, $tempSketchDir)
$compileExit = Invoke-NativeToLog -FilePath $arduinoCli -Arguments $compileArgs -LogPath $compileLog
Write-Host "COMPILE_EXIT=$compileExit"
Write-Host "COMPILE_LOG=$compileLog"
if ($compileExit -ne 0) {
    Get-Content -LiteralPath $compileLog -Tail 100 | ForEach-Object { Write-Host $_ }
    throw "G3_AB_COMPILE_FAILED"
}
$compileText = [System.IO.File]::ReadAllText($compileLog)
$ethernetLibraryUsed = $compileText.IndexOf($expectedEthernetLibrary, [System.StringComparison]::OrdinalIgnoreCase) -ge 0
Write-Host "REPO_ETHERNET_LIBRARY_USED=$ethernetLibraryUsed"
if (-not $ethernetLibraryUsed) { throw "G3_AB_WRONG_ETHERNET_LIBRARY" }

Write-Host ""
Write-Host "=== UPLOAD TEMP DIAGNOSTIC FIRMWARE ==="
$uploadArgs = @("upload", "--fqbn", $fqbn, "--port", $SerialPort, "--input-dir", $buildPath, $tempSketchDir)
$uploadExit = Invoke-NativeToLog -FilePath $arduinoCli -Arguments $uploadArgs -LogPath $uploadLog
Write-Host "UPLOAD_EXIT=$uploadExit"
Write-Host "UPLOAD_LOG=$uploadLog"
if ($uploadExit -ne 0) {
    Get-Content -LiteralPath $uploadLog -Tail 100 | ForEach-Object { Write-Host $_ }
    throw "G3_AB_UPLOAD_FAILED"
}

$durationText = $DurationSeconds.ToString([System.Globalization.CultureInfo]::InvariantCulture)
$oneSecondRx = 0
$oneSecondTx = 0
$shiftedRx = 0
$shiftedTx = 0
$rxMaxOverall = 0L
$txMaxOverall = 0L

function Invoke-DiagMode {
    param([int]$Pair, [string]$CliMode, [string]$Key)
    $logPath = Join-Path $tempRoot ("pair{0:D2}_{1}.log" -f $Pair, $Key.ToLowerInvariant())
    $args = @($bridgePath, "--host", $DutIp, "--serial", $SerialPort, "--duration", $durationText, "--mode", $CliMode, "--tcp-chunk", "4096", "--udp-payload", "1472")
    $exitCode = Invoke-NativeToLog -FilePath $pythonExe -Arguments $args -LogPath $logPath
    if ($exitCode -ne 0) {
        Get-Content -LiteralPath $logPath -Tail 100 | ForEach-Object { Write-Host $_ }
        throw "G3_AB_RUNNER_FAILED_${Key}_PAIR_$Pair"
    }
    $text = [System.IO.File]::ReadAllText($logPath)
    if ((Get-LogValue -Text $text -Key "RAW_BENCH_FUNCTIONAL_PASS") -ne "YES") { throw "G3_AB_FUNCTIONAL_FAIL_${Key}_PAIR_$Pair" }
    if ((Get-LogValue -Text $text -Key "G2_FINAL_SNAPSHOT_PRESENT") -ne "YES") { throw "G3_AB_SNAPSHOT_MISSING_${Key}_PAIR_$Pair" }
    foreach ($errorKey in @("TRANSPORT_ERRORS", "UDP_SPI_LOCK_ERRORS", "TCP_SPI_LOCK_ERRORS")) {
        $v = Get-LogInt64 -Text $text -Key ("G2_FINAL_{0}" -f $errorKey)
        if ($v -ne 0) { throw "G3_AB_${errorKey}_${Key}_PAIR_${Pair}=$v" }
    }
    $holdAvgUs = Get-LogInt64 -Text $text -Key "G2_FINAL_TCP_SPI_HOLD_AVG_US"
    $holdMaxUs = Get-LogInt64 -Text $text -Key "G2_FINAL_TCP_SPI_HOLD_MAX_US"
    $loopGapMaxUs = Get-LogInt64 -Text $text -Key "G2_FINAL_LOOP_GAP_MAX_US"
    $pcMbps = Get-LogValue -Text $text -Key ("SUMMARY_{0}_PC_MBPS" -f $Key)
    $oneSecond = ($holdMaxUs -ge $oneSecondMinUs -and $holdMaxUs -le $oneSecondMaxUs)
    $shifted = ($holdMaxUs -ge $shiftedMinUs -and $holdMaxUs -le $shiftedMaxUs)
    Write-Host ("PAIR={0} MODE={1} PC_MBPS={2} HOLD_AVG_US={3} HOLD_MAX_US={4} LOOP_GAP_MAX_US={5} ONE_SECOND_SIGNATURE={6} SHIFTED_TIMEOUT_SIGNATURE={7}" -f $Pair, $Key, $pcMbps, $holdAvgUs, $holdMaxUs, $loopGapMaxUs, $(if ($oneSecond) {"YES"} else {"NO"}), $(if ($shifted) {"YES"} else {"NO"}))
    return [pscustomobject]@{ HoldMaxUs=$holdMaxUs; OneSecond=$oneSecond; Shifted=$shifted }
}

Write-Host ""
Write-Host "=== REPEATED TCP_RX -> TCP_TX PAIRS WITH DIAG TIMEOUT ==="
for ($pair = 1; $pair -le $Pairs; ++$pair) {
    $rx = Invoke-DiagMode -Pair $pair -CliMode "tcp-rx" -Key "TCP_RX"
    if ($rx.HoldMaxUs -gt $rxMaxOverall) { $rxMaxOverall = $rx.HoldMaxUs }
    if ($rx.OneSecond) { ++$oneSecondRx }
    if ($rx.Shifted) { ++$shiftedRx }
    Start-Sleep -Milliseconds 150

    $tx = Invoke-DiagMode -Pair $pair -CliMode "tcp-tx" -Key "TCP_TX"
    if ($tx.HoldMaxUs -gt $txMaxOverall) { $txMaxOverall = $tx.HoldMaxUs }
    if ($tx.OneSecond) { ++$oneSecondTx }
    if ($tx.Shifted) { ++$shiftedTx }
    Start-Sleep -Milliseconds 150
}

Write-Host ""
Write-Host "=== A/B DIAGNOSTIC SUMMARY ==="
Write-Host "PAIRS_COMPLETED=$Pairs"
Write-Host "TCP_RX_HOLD_MAX_US_OVERALL=$rxMaxOverall"
Write-Host "TCP_TX_HOLD_MAX_US_OVERALL=$txMaxOverall"
Write-Host "TCP_RX_ONE_SECOND_SIGNATURE_COUNT=$oneSecondRx"
Write-Host "TCP_TX_ONE_SECOND_SIGNATURE_COUNT=$oneSecondTx"
Write-Host "TCP_RX_SHIFTED_TIMEOUT_SIGNATURE_COUNT=$shiftedRx"
Write-Host "TCP_TX_SHIFTED_TIMEOUT_SIGNATURE_COUNT=$shiftedTx"

Write-Host ""
Write-Host "OBSERVACION FISICA REQUERIDA: mira la TFT durante el A/B."
$visualAnswer = ""
while ($visualAnswer -notin @("S", "N")) {
    $visualAnswer = (Read-Host 'Aparecio el diagnostico visual "SPI" durante el A/B? (S/N)').Trim().ToUpperInvariant()
}
$visualEvents = $(if ($visualAnswer -eq "S") { 1 } else { 0 })
Write-Host "VISUAL_SPI_EVENTS=$visualEvents"

Assert-G2ProtectedArtifacts
if ((Get-G2Sha256 $script:G2RawFirmwareRelative) -ne $expectedSourceFirmwareSha256) { throw "G3_AB_SOURCE_FIRMWARE_CHANGED" }
if ((Get-G2Sha256 $script:G2RawRunnerRelative) -ne $script:G2RawRunnerSha256) { throw "G3_AB_RUNNER_CHANGED" }
$dirtyAfter = @(Get-G2TrackedDirtyPaths)
if ($dirtyAfter.Count -ne 2) { throw "G3_AB_DIRTY_COUNT_CHANGED" }
for ($i = 0; $i -lt 2; ++$i) {
    if ($dirtyAfter[$i] -ne $expectedDirty[$i]) { throw "G3_AB_DIRTY_PATH_CHANGED=$($dirtyAfter[$i])" }
}

if (($shiftedRx + $shiftedTx) -gt 0 -and ($oneSecondRx + $oneSecondTx) -eq 0) {
    Write-Host "G3_STOP_TIMEOUT_AB=SHIFTED_TO_DIAG_TIMEOUT"
} elseif (($oneSecondRx + $oneSecondTx) -gt 0) {
    Write-Host "G3_STOP_TIMEOUT_AB=ONE_SECOND_PERSISTS"
} else {
    Write-Host "G3_STOP_TIMEOUT_AB=NO_SIGNATURE_REPRODUCED"
}
Write-Host "G3_STOP_TIMEOUT_AB_DIAG=COMPLETE"
