param(
    [string]$SerialPort = "COM14",
    [double]$DurationSeconds = 3.0
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"
. (Join-Path $PSScriptRoot "common.ps1")

$expectedW5100Hash30 = "B95324195AD1ACDE923734826DCD36B2266B6829C2B9D828765B4944B59E9993"
$expectedW5100Hash26 = "9A833532C44E0CFCD66429A764E8BDBF62A8871838B0355565BC0A63043455FC"
$expectedRawFirmwareHash = "9A0C6CF27E44A61DC97686FB1A769EBBBB34D036CDF8D0CA0EEAB597E699AB6B"
$expectedRunnerHash = "FD25997CFE777B4FF50BBC10EFE555BD6B7A8A8DCE9D4ADD1E7D2DBA6D52470F"

function Invoke-A14NativeToLog {
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

function Get-A14LogValue {
    param([string]$Text, [string]$Key)
    $pattern = "(?m)^" + [regex]::Escape($Key) + "=(.*)\r?$"
    $matches = @([regex]::Matches($Text, $pattern))
    if ($matches.Count -ne 1) {
        throw ("A14_G2_RECOVERY_LOG_KEY_COUNT_{0}={1}" -f $Key, $matches.Count)
    }
    return $matches[0].Groups[1].Value.Trim()
}

function Resolve-A14DutIp {
    param([string]$PythonExe, [string]$ResolverPath, [string]$Port, [string]$LogPath)
    $resolverArgs = @($ResolverPath, "--serial", $Port, "--timeout", "15")
    $exitCode = Invoke-A14NativeToLog -FilePath $PythonExe -Arguments $resolverArgs -LogPath $LogPath
    Write-Host "RECOVERY_IP_RESOLVER_EXIT=$exitCode"
    Write-Host "RECOVERY_IP_RESOLVER_LOG=$LogPath"
    if ($exitCode -ne 0) {
        Get-Content -LiteralPath $LogPath -Tail 140 | ForEach-Object { Write-Host $_ }
        throw "A14_G2_26_RECOVERY_IP_RESOLVER_FAILED"
    }
    $text = [System.IO.File]::ReadAllText($LogPath)
    $ready = Get-A14LogValue -Text $text -Key "DUT_SERIAL_READY"
    $ip = Get-A14LogValue -Text $text -Key "DUT_IP_EFFECTIVE"
    $source = Get-A14LogValue -Text $text -Key "DUT_IP_SOURCE"
    Write-Host "RECOVERY_DUT_SERIAL_READY=$ready"
    Write-Host "RECOVERY_DUT_IP_EFFECTIVE=$ip"
    Write-Host "RECOVERY_DUT_IP_SOURCE=$source"
    if ($ready -ne "YES" -or $source -ne "RAW_SERIAL_SNAPSHOT") {
        throw "A14_G2_26_RECOVERY_IP_STATE_INVALID"
    }
    return $ip
}

Write-Host "============================================================"
Write-Host " A14 G2 - ROLLBACK 30 -> 26 MHz / RECOVERY"
Write-Host "============================================================"

Assert-G2Branch
$head = Get-G2Head
$currentHz = Get-G2SpiHz
$dirtyBefore = @(Get-G2TrackedDirtyPaths)
$stagedBefore = @(& git -C $script:G2RepoRoot diff --cached --name-only)
if ($LASTEXITCODE -ne 0) { throw "A14_G2_RECOVERY_CACHED_DIFF_FAILED" }

Write-Host "HEAD=$head"
Write-Host "CURRENT_SPI_HZ=$currentHz"
Write-Host "TRACKED_DIRTY_BEFORE=$($dirtyBefore.Count)"
$dirtyBefore | ForEach-Object { Write-Host "DIRTY_BEFORE=$_" }
Write-Host "STAGED_COUNT_BEFORE=$($stagedBefore.Count)"

if ($currentHz -ne 30000000) { throw "A14_G2_RECOVERY_EXPECTED_30MHZ" }
if ($dirtyBefore.Count -ne 1 -or $dirtyBefore[0] -ne $script:G2SpiHeaderRelative) { throw "A14_G2_RECOVERY_UNEXPECTED_DIRTY_STATE" }
if ($stagedBefore.Count -ne 0) { throw "A14_G2_RECOVERY_INDEX_NOT_CLEAN" }

Assert-G2ProtectedArtifacts
$w5100Hash30 = Get-G2Sha256 $script:G2SpiHeaderRelative
$firmwareHash = Get-G2Sha256 $script:G2RawFirmwareRelative
$runnerHash = Get-G2Sha256 $script:G2RawRunnerRelative
Write-Host "W5100_H_SHA256_30=$w5100Hash30"
Write-Host "RAW_FIRMWARE_SHA256=$firmwareHash"
Write-Host "RAW_RUNNER_SHA256=$runnerHash"
if ($w5100Hash30 -ne $expectedW5100Hash30) { throw "A14_G2_RECOVERY_30MHZ_HASH_MISMATCH" }
if ($firmwareHash -ne $expectedRawFirmwareHash) { throw "A14_G2_RECOVERY_RAW_FIRMWARE_HASH_MISMATCH" }
if ($runnerHash -ne $expectedRunnerHash) { throw "A14_G2_RECOVERY_RAW_RUNNER_HASH_MISMATCH" }

Write-Host ""
Write-Host "=== ROLLBACK SOURCE TO 26 MHz ==="
Set-G2SpiHz -Hz 26000000
$effectiveHz = Get-G2SpiHz
$w5100Hash26 = Get-G2Sha256 $script:G2SpiHeaderRelative
$dirtyAfterRollback = @(Get-G2TrackedDirtyPaths)
Write-Host "EFFECTIVE_SPI_HZ=$effectiveHz"
Write-Host "W5100_H_SHA256_26=$w5100Hash26"
Write-Host "TRACKED_DIRTY_AFTER_ROLLBACK=$($dirtyAfterRollback.Count)"
if ($effectiveHz -ne 26000000) { throw "A14_G2_RECOVERY_26MHZ_NOT_EFFECTIVE" }
if ($w5100Hash26 -ne $expectedW5100Hash26) { throw "A14_G2_RECOVERY_26MHZ_HASH_MISMATCH" }
if ($dirtyAfterRollback.Count -ne 0) {
    $dirtyAfterRollback | ForEach-Object { Write-Host "DIRTY_AFTER_ROLLBACK=$_" }
    throw "A14_G2_RECOVERY_TREE_NOT_CLEAN_AT_26"
}

$pythonCommand = Get-Command python.exe -ErrorAction SilentlyContinue
if ($null -eq $pythonCommand) { $pythonCommand = Get-Command python -ErrorAction SilentlyContinue }
if ($null -eq $pythonCommand) { throw "A14_G2_RECOVERY_PYTHON_NOT_FOUND" }
$pythonExe = $pythonCommand.Source
$arduinoCli = "C:\Program Files\Arduino PLC IDE Tools\arduino-cli.exe"
if (-not (Test-Path -LiteralPath $arduinoCli)) { throw "A14_G2_RECOVERY_ARDUINO_CLI_NOT_FOUND" }

$resolverPath = Join-Path $PSScriptRoot "a14_nb3_resolve_dut_ip.py"
$bridgePath = Join-Path $PSScriptRoot "g2_runner_snapshot_bridge.py"
$firmwarePath = Get-G2Path $script:G2RawFirmwareRelative
$sketchDir = Split-Path -Parent $firmwarePath
$librariesRoot = Get-G2Path "JWPLC/2.1.0/libraries"
$expectedEthernetLibrary = Join-Path $librariesRoot "JWPLC_Ethernet"
$fqbn = "jwplc_local:esp32:jwplcbasic"
$timestamp = Get-Date -Format "yyyyMMdd_HHmmss"
$tempRoot = Join-Path $env:TEMP ("jwplc_a14_g2_26mhz_recovery_{0}" -f $timestamp)
$buildPath = Join-Path $tempRoot "build"
$compileLog = Join-Path $tempRoot "compile.log"
$uploadLog = Join-Path $tempRoot "upload.log"
$ipLog = Join-Path $tempRoot "recovery_ip.log"
New-Item -ItemType Directory -Force -Path $buildPath | Out-Null

Write-Host "PYTHON=$pythonExe"
Write-Host "ARDUINO_CLI=$arduinoCli"
Write-Host "TEMP_ROOT=$tempRoot"

Write-Host ""
Write-Host "=== PYTHON SYNTAX PRECHECK ==="
$pyArgs = @("-m", "py_compile", $resolverPath, $bridgePath)
$pyExit = Invoke-A14NativeToLog -FilePath $pythonExe -Arguments $pyArgs -LogPath (Join-Path $tempRoot "py_compile.log")
Write-Host "PY_COMPILE_EXIT=$pyExit"
if ($pyExit -ne 0) { throw "A14_G2_RECOVERY_PYTHON_SYNTAX_FAILED" }

Write-Host ""
Write-Host "=== COMPILE 26 MHz RAW FIRMWARE ==="
$compileArgs = @("compile", "--fqbn", $fqbn, "--build-path", $buildPath, "--libraries", $librariesRoot, $sketchDir)
$compileExit = Invoke-A14NativeToLog -FilePath $arduinoCli -Arguments $compileArgs -LogPath $compileLog
Write-Host "COMPILE_EXIT=$compileExit"
Write-Host "COMPILE_LOG=$compileLog"
if ($compileExit -ne 0) {
    Invoke-G2CompileFinishedSound -Success $false
    Get-Content -LiteralPath $compileLog -Tail 120 | ForEach-Object { Write-Host $_ }
    throw "A14_G2_RECOVERY_COMPILE_FAILED"
}
$compileText = [System.IO.File]::ReadAllText($compileLog)
$repoEthernetUsed = $compileText.IndexOf($expectedEthernetLibrary, [System.StringComparison]::OrdinalIgnoreCase) -ge 0
Write-Host "REPO_ETHERNET_LIBRARY_USED=$repoEthernetUsed"
if (-not $repoEthernetUsed) { throw "A14_G2_RECOVERY_WRONG_ETHERNET_LIBRARY" }
$binCount = @(Get-ChildItem -LiteralPath $buildPath -Recurse -File -Filter "*.bin").Count
Write-Host "BIN_COUNT=$binCount"
if ($binCount -lt 1) { throw "A14_G2_RECOVERY_BIN_MISSING" }
Invoke-G2CompileFinishedSound -Success $true

Write-Host ""
Write-Host "=== UPLOAD 26 MHz RAW FIRMWARE ==="
$uploadArgs = @("upload", "--fqbn", $fqbn, "--port", $SerialPort, "--input-dir", $buildPath, $sketchDir)
$uploadExit = Invoke-A14NativeToLog -FilePath $arduinoCli -Arguments $uploadArgs -LogPath $uploadLog
Write-Host "UPLOAD_EXIT=$uploadExit"
Write-Host "UPLOAD_LOG=$uploadLog"
if ($uploadExit -ne 0) {
    Get-Content -LiteralPath $uploadLog -Tail 120 | ForEach-Object { Write-Host $_ }
    throw "A14_G2_RECOVERY_UPLOAD_FAILED"
}
Start-Sleep -Seconds 2

Write-Host ""
Write-Host "=== VERIFY ETHERNET RECOVERY @ 26 MHz ==="
$dutIp = Resolve-A14DutIp -PythonExe $pythonExe -ResolverPath $resolverPath -Port $SerialPort -LogPath $ipLog

Write-Host ""
Write-Host "=== SHORT FOUR-MODE RECOVERY SMOKE ==="
$durationText = $DurationSeconds.ToString([System.Globalization.CultureInfo]::InvariantCulture)
$modeMap = @(
    [pscustomobject]@{ Cli = "tcp-rx"; Key = "TCP_RX" },
    [pscustomobject]@{ Cli = "tcp-tx"; Key = "TCP_TX" },
    [pscustomobject]@{ Cli = "udp-rx"; Key = "UDP_RX" },
    [pscustomobject]@{ Cli = "udp-tx"; Key = "UDP_TX" }
)

foreach ($mode in $modeMap) {
    $runLog = Join-Path $tempRoot ("{0}.log" -f $mode.Key.ToLowerInvariant())
    $runArgs = @($bridgePath, "--host", $dutIp, "--serial", $SerialPort, "--duration", $durationText, "--mode", $mode.Cli, "--tcp-chunk", "4096", "--udp-payload", "1472")
    $runExit = Invoke-A14NativeToLog -FilePath $pythonExe -Arguments $runArgs -LogPath $runLog
    Write-Host "MODE=$($mode.Key) RUNNER_EXIT=$runExit LOG=$runLog"
    if ($runExit -ne 0) {
        Get-Content -LiteralPath $runLog -Tail 140 | ForEach-Object { Write-Host $_ }
        throw "A14_G2_RECOVERY_RUNNER_FAILED_$($mode.Key)"
    }
    $runText = [System.IO.File]::ReadAllText($runLog)
    $functionalPass = Get-A14LogValue -Text $runText -Key "RAW_BENCH_FUNCTIONAL_PASS"
    $snapshotPresent = Get-A14LogValue -Text $runText -Key "G2_FINAL_SNAPSHOT_PRESENT"
    $finalReady = Get-A14LogValue -Text $runText -Key "G2_FINAL_ETH_READY"
    $finalLink = Get-A14LogValue -Text $runText -Key "G2_FINAL_ETH_LINK"
    $finalIp = Get-A14LogValue -Text $runText -Key "G2_FINAL_IP"
    $pcMbps = Get-A14LogValue -Text $runText -Key ("SUMMARY_{0}_PC_MBPS" -f $mode.Key)
    $dutMbps = Get-A14LogValue -Text $runText -Key ("SUMMARY_{0}_DUT_MBPS" -f $mode.Key)
    if ($functionalPass -ne "YES") { throw "A14_G2_RECOVERY_FUNCTIONAL_FAIL_$($mode.Key)" }
    if ($snapshotPresent -ne "YES") { throw "A14_G2_RECOVERY_SNAPSHOT_MISSING_$($mode.Key)" }
    if ($finalReady -ne "YES" -or $finalLink -ne "UP" -or $finalIp -ne $dutIp) { throw "A14_G2_RECOVERY_FINAL_STATE_INVALID_$($mode.Key)" }
    Write-Host ("RUN MODE={0} PC_MBPS={1} DUT_MBPS={2} PASS=YES" -f $mode.Key, $pcMbps, $dutMbps)
}

Assert-G2ProtectedArtifacts
$finalHz = Get-G2SpiHz
$finalDirty = @(Get-G2TrackedDirtyPaths)
$finalStaged = @(& git -C $script:G2RepoRoot diff --cached --name-only)
Write-Host ""
Write-Host "=== FINAL STATE ==="
Write-Host "FINAL_SPI_HZ=$finalHz"
Write-Host "TRACKED_DIRTY_FINAL=$($finalDirty.Count)"
Write-Host "STAGED_COUNT_FINAL=$($finalStaged.Count)"
Write-Host "RECOVERY_IP=$dutIp"
if ($finalHz -ne 26000000) { throw "A14_G2_RECOVERY_FINAL_FREQ_INVALID" }
if ($finalDirty.Count -ne 0) { throw "A14_G2_RECOVERY_FINAL_TREE_NOT_CLEAN" }
if ($finalStaged.Count -ne 0) { throw "A14_G2_RECOVERY_FINAL_INDEX_NOT_CLEAN" }

Write-Host ""
Write-Host "A14_G2_30MHZ_STATUS=REJECTED_CURRENT_HW_CONFIG"
Write-Host "A14_G2_26MHZ_RECOVERY=PASS"
Write-Host "A14_G2_26MHZ_FREEZE_CANDIDATE=YES"
Write-Host "A14_G2_ROLLBACK_30_TO_26_RECOVERY=PASS"
Write-Host "NEXT_ACTION=RETURN_OUTPUT_TO_CHAT"
