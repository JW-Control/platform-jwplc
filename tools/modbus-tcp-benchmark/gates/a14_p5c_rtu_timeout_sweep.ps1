param(
    [string]$MasterPort = "COM14",
    [string]$SlavePort = "COM4",
    [double]$DurationS = 15.0
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"
. (Join-Path $PSScriptRoot "common.ps1")

function Invoke-NativeToLog {
    param([string]$FilePath, [string[]]$Arguments, [string]$LogPath)
    $previousPreference = $ErrorActionPreference
    try {
        $ErrorActionPreference = "Continue"
        & $FilePath @Arguments *> $LogPath
        return [int]$LASTEXITCODE
    }
    finally { $ErrorActionPreference = $previousPreference }
}

function Get-LogValue {
    param([string]$Text, [string]$Key)
    $pattern = "(?m)^" + [regex]::Escape($Key) + "=(.*)\r?$"
    $matches = @([regex]::Matches($Text, $pattern))
    if ($matches.Count -ne 1) {
        throw ("P5C_LOG_KEY_COUNT_{0}={1}" -f $Key, $matches.Count)
    }
    return $matches[0].Groups[1].Value.Trim()
}

Write-Host "============================================================"
Write-Host " A14 P5-C - RTU TIMEOUT CALIBRATION SWEEP"
Write-Host " FULL RUNTIME / PERIOD 20 MS / NO TCP LOAD"
Write-Host "============================================================"

Assert-G2Branch
$head = Get-G2Head
$spiHz = Get-G2SpiHz
$dirty = @(Get-G2TrackedDirtyPaths)
$staged = @(& git -C $script:G2RepoRoot diff --cached --name-only)
if ($LASTEXITCODE -ne 0) { throw "P5C_CACHED_DIFF_FAILED" }

Write-Host "HEAD=$head"
Write-Host "W5500_SPI_HZ=$spiHz"
Write-Host "TRACKED_DIRTY_COUNT=$($dirty.Count)"
Write-Host "STAGED_COUNT=$($staged.Count)"
Write-Host "MASTER_PORT=$MasterPort"
Write-Host "SLAVE_PORT=$SlavePort"
Write-Host ("DURATION_PER_VARIANT_S={0:F0}" -f $DurationS)
Write-Host "RTU_PERIOD_MS=20"
Write-Host "TIMEOUT_VARIANTS_MS=15,25,35,50"
Write-Host "MIN_HEADROOM_MS=5"
Write-Host "TCP_LOAD=NO"
Write-Host "FULL_RUNTIME_PERIPHERALS=YES"
Write-Host "PRODUCT_SOURCE_MUTATION=NO"

if ($spiHz -ne 26000000) { throw "P5C_EXPECTED_26MHZ" }
if ($dirty.Count -ne 0) { throw "P5C_TRACKED_TREE_NOT_CLEAN" }
if ($staged.Count -ne 0) { throw "P5C_INDEX_NOT_CLEAN" }
if ($DurationS -lt 10.0) { throw "P5C_DURATION_TOO_SHORT" }
Assert-G2ProtectedArtifacts

$ports = @([System.IO.Ports.SerialPort]::GetPortNames() | Sort-Object)
Write-Host "COM_PORTS=$($ports -join ',')"
if ($MasterPort -notin $ports) { throw "P5C_MASTER_COM_NOT_PRESENT" }
if ($SlavePort -notin $ports) { throw "P5C_SLAVE_COM_NOT_PRESENT" }

$pythonCommand = Get-Command python.exe -ErrorAction SilentlyContinue
if ($null -eq $pythonCommand) { $pythonCommand = Get-Command python -ErrorAction SilentlyContinue }
if ($null -eq $pythonCommand) { throw "P5C_PYTHON_NOT_FOUND" }
$pythonExe = $pythonCommand.Source

$arduinoCli = "C:\Program Files\Arduino PLC IDE Tools\arduino-cli.exe"
if (-not (Test-Path -LiteralPath $arduinoCli)) { throw "P5C_ARDUINO_CLI_NOT_FOUND" }

$masterSourceDir = Get-G2Path "tools/modbus-tcp-benchmark/firmware/a14_p5_full_runtime_master"
$slaveDir = Get-G2Path "tools/modbus-tcp-benchmark/firmware/a14_p5_rtu_slave"
$patcher = Join-Path $PSScriptRoot "a14_p5c_rtu_timeout_patch.py"
$probe = Get-G2Path "tools/modbus-tcp-benchmark/pc/a14_p5c_rtu_timeout_probe.py"
$repoLibrariesRoot = Get-G2Path "JWPLC/2.1.0/libraries"
$fqbn = "jwplc_local:esp32:jwplcbasic"

foreach ($required in @($masterSourceDir, $slaveDir, $patcher, $probe)) {
    if (-not (Test-Path -LiteralPath $required)) { throw "P5C_REQUIRED_FILE_MISSING=$required" }
}

$timestamp = Get-Date -Format "yyyyMMdd_HHmmss"
$tempRoot = Join-Path $env:TEMP ("jwplc_a14_p5c_timeout_{0}" -f $timestamp)
$slaveBuild = Join-Path $tempRoot "build_slave"
$slaveCompileLog = Join-Path $tempRoot "compile_slave.log"
$slaveUploadLog = Join-Path $tempRoot "upload_slave.log"
New-Item -ItemType Directory -Force -Path $tempRoot | Out-Null
New-Item -ItemType Directory -Force -Path $slaveBuild | Out-Null
Write-Host "TEMP_ROOT=$tempRoot"

Write-Host ""
Write-Host "=== PYTHON SYNTAX PRECHECK ==="
$pyCompileLog = Join-Path $tempRoot "py_compile.log"
$pyCompileArgs = @("-m","py_compile",$patcher,$probe)
$pyCompileExit = Invoke-NativeToLog -FilePath $pythonExe -Arguments $pyCompileArgs -LogPath $pyCompileLog
Write-Host "PY_COMPILE_EXIT=$pyCompileExit"
if ($pyCompileExit -ne 0) {
    Get-Content -LiteralPath $pyCompileLog -Tail 200 | ForEach-Object { Write-Host $_ }
    throw "P5C_PYTHON_SYNTAX_FAILED"
}

Write-Host ""
Write-Host "=== COMPILE + UPLOAD SLAVE ONCE ==="
$slaveCompileArgs = @("compile","--fqbn",$fqbn,"--build-path",$slaveBuild,"--libraries",$repoLibrariesRoot,$slaveDir)
$slaveCompileExit = Invoke-NativeToLog -FilePath $arduinoCli -Arguments $slaveCompileArgs -LogPath $slaveCompileLog
Write-Host "SLAVE_COMPILE_EXIT=$slaveCompileExit"
if ($slaveCompileExit -ne 0) {
    Get-Content -LiteralPath $slaveCompileLog -Tail 220 | ForEach-Object { Write-Host $_ }
    throw "P5C_SLAVE_COMPILE_FAILED"
}
$slaveUploadArgs = @("upload","--fqbn",$fqbn,"--port",$SlavePort,"--input-dir",$slaveBuild,$slaveDir)
$slaveUploadExit = Invoke-NativeToLog -FilePath $arduinoCli -Arguments $slaveUploadArgs -LogPath $slaveUploadLog
Write-Host "SLAVE_UPLOAD_EXIT=$slaveUploadExit"
if ($slaveUploadExit -ne 0) {
    Get-Content -LiteralPath $slaveUploadLog -Tail 220 | ForEach-Object { Write-Host $_ }
    throw "P5C_SLAVE_UPLOAD_FAILED"
}
Start-Sleep -Seconds 2

$variants = @(15,25,35,50)
$results = @()

foreach ($timeoutMs in $variants) {
    Write-Host ""
    Write-Host "============================================================"
    Write-Host " P5-C VARIANT TIMEOUT=$timeoutMs ms"
    Write-Host "============================================================"

    $variantRoot = Join-Path $tempRoot ("t" + $timeoutMs)
    $masterDir = Join-Path $variantRoot "a14_p5_full_runtime_master"
    $masterSketch = Join-Path $masterDir "a14_p5_full_runtime_master.ino"
    $buildPath = Join-Path $variantRoot "build"
    $patchLog = Join-Path $variantRoot "patch.log"
    $compileLog = Join-Path $variantRoot "compile.log"
    $uploadLog = Join-Path $variantRoot "upload.log"
    $probeLog = Join-Path $variantRoot "probe.log"

    New-Item -ItemType Directory -Force -Path $variantRoot | Out-Null
    Copy-Item -LiteralPath $masterSourceDir -Destination $masterDir -Recurse
    New-Item -ItemType Directory -Force -Path $buildPath | Out-Null

    $patchArgs = @($patcher,"--sketch",$masterSketch,"--timeout-ms",$timeoutMs.ToString())
    $patchExit = Invoke-NativeToLog -FilePath $pythonExe -Arguments $patchArgs -LogPath $patchLog
    Write-Host "P5C_PATCH_EXIT=$patchExit"
    if ($patchExit -ne 0) {
        Get-Content -LiteralPath $patchLog -Tail 160 | ForEach-Object { Write-Host $_ }
        throw "P5C_PATCH_FAILED_T$timeoutMs"
    }
    Get-Content -LiteralPath $patchLog | ForEach-Object { Write-Host $_ }

    $compileArgs = @("compile","--fqbn",$fqbn,"--build-path",$buildPath,"--libraries",$repoLibrariesRoot,$masterDir)
    $compileExit = Invoke-NativeToLog -FilePath $arduinoCli -Arguments $compileArgs -LogPath $compileLog
    Write-Host "P5C_MASTER_COMPILE_EXIT_T${timeoutMs}=$compileExit"
    if ($compileExit -ne 0) {
        Invoke-G2CompileFinishedSound -Success $false
        Get-Content -LiteralPath $compileLog -Tail 240 | ForEach-Object { Write-Host $_ }
        throw "P5C_MASTER_COMPILE_FAILED_T$timeoutMs"
    }

    $uploadArgs = @("upload","--fqbn",$fqbn,"--port",$MasterPort,"--input-dir",$buildPath,$masterDir)
    $uploadExit = Invoke-NativeToLog -FilePath $arduinoCli -Arguments $uploadArgs -LogPath $uploadLog
    Write-Host "P5C_MASTER_UPLOAD_EXIT_T${timeoutMs}=$uploadExit"
    if ($uploadExit -ne 0) {
        Get-Content -LiteralPath $uploadLog -Tail 220 | ForEach-Object { Write-Host $_ }
        throw "P5C_MASTER_UPLOAD_FAILED_T$timeoutMs"
    }

    Start-Sleep -Seconds 2

    $durationText = $DurationS.ToString([System.Globalization.CultureInfo]::InvariantCulture)
    $probeArgs = @($probe,"--master-serial",$MasterPort,"--slave-serial",$SlavePort,"--duration",$durationText,"--expected-timeout-ms",$timeoutMs.ToString())
    $probeExit = Invoke-NativeToLog -FilePath $pythonExe -Arguments $probeArgs -LogPath $probeLog
    Write-Host "P5C_PROBE_EXIT_T${timeoutMs}=$probeExit"
    Get-Content -LiteralPath $probeLog | ForEach-Object { Write-Host $_ }
    if ($probeExit -eq 2) { throw "P5C_PROBE_HARNESS_FAILED_T$timeoutMs" }

    $probeText = [System.IO.File]::ReadAllText($probeLog)
    $candidatePass = Get-LogValue -Text $probeText -Key "P5C_CANDIDATE_PASS"
    $achievedHz = Get-LogValue -Text $probeText -Key "P5C_ACHIEVED_HZ"
    $timeouts = Get-LogValue -Text $probeText -Key "P5C_TIMEOUTS"
    $failed = Get-LogValue -Text $probeText -Key "P5C_FAILED"
    $serviceGap = Get-LogValue -Text $probeText -Key "P5C_RTU_SERVICE_GAP_MAX_US"
    $loopGap = Get-LogValue -Text $probeText -Key "P5C_LOOP_GAP_MAX_US"
    $sdVerifyMax = Get-LogValue -Text $probeText -Key "P5C_SD_VERIFY_MAX_US"
    $headroom = Get-LogValue -Text $probeText -Key "P5C_TIMEOUT_HEADROOM_MS"
    $periodsSkipped = Get-LogValue -Text $probeText -Key "P5C_PERIODS_SKIPPED"

    $results += [PSCustomObject]@{
        TimeoutMs = $timeoutMs
        CandidatePass = $candidatePass
        AchievedHz = $achievedHz
        Timeouts = $timeouts
        Failed = $failed
        ServiceGapUs = $serviceGap
        LoopGapUs = $loopGap
        SdVerifyMaxUs = $sdVerifyMax
        HeadroomMs = $headroom
        PeriodsSkipped = $periodsSkipped
    }
}

Invoke-G2CompileFinishedSound -Success $true

Write-Host ""
Write-Host "============================================================"
Write-Host " P5-C SWEEP SUMMARY"
Write-Host "============================================================"

foreach ($r in $results) {
    Write-Host ("P5C_SUMMARY TIMEOUT_MS={0} PASS={1} HZ={2} TIMEOUTS={3} FAILED={4} SERVICE_GAP_US={5} LOOP_GAP_US={6} SD_VERIFY_MAX_US={7} HEADROOM_MS={8} PERIODS_SKIPPED={9}" -f $r.TimeoutMs,$r.CandidatePass,$r.AchievedHz,$r.Timeouts,$r.Failed,$r.ServiceGapUs,$r.LoopGapUs,$r.SdVerifyMaxUs,$r.HeadroomMs,$r.PeriodsSkipped)
}

$passing = @($results | Where-Object { $_.CandidatePass -eq "YES" } | Sort-Object TimeoutMs)

if ($passing.Count -gt 0) {
    $selected = $passing[0]
    Write-Host "P5C_SELECTED_TIMEOUT_MS=$($selected.TimeoutMs)"
    Write-Host "P5C_SELECTION_RULE=LOWEST_ZERO_ERROR_45TO52HZ_WITH_5MS_HEADROOM"
    Write-Host "A14_P5C_RTU_TIMEOUT_CALIBRATION=PASS"
}
else {
    Write-Host "P5C_SELECTED_TIMEOUT_MS=NONE"
    Write-Host "P5C_SELECTION_RULE=LOWEST_ZERO_ERROR_45TO52HZ_WITH_5MS_HEADROOM"
    Write-Host "A14_P5C_RTU_TIMEOUT_CALIBRATION=NO_SAFE_CANDIDATE"
}

Write-Host ""
Write-Host "=== FINAL PRODUCT INVARIANTS ==="
Assert-G2ProtectedArtifacts
$finalHz = Get-G2SpiHz
$finalDirty = @(Get-G2TrackedDirtyPaths)
$finalStaged = @(& git -C $script:G2RepoRoot diff --cached --name-only)
Write-Host "FINAL_SPI_HZ=$finalHz"
Write-Host "TRACKED_DIRTY_FINAL=$($finalDirty.Count)"
Write-Host "STAGED_COUNT_FINAL=$($finalStaged.Count)"
if ($finalHz -ne 26000000) { throw "P5C_FINAL_SPI_CHANGED" }
if ($finalDirty.Count -ne 0) { throw "P5C_PRODUCT_TREE_DIRTY" }
if ($finalStaged.Count -ne 0) { throw "P5C_PRODUCT_INDEX_DIRTY" }

Write-Host "A14_P5C_PRODUCT_SOURCE_MUTATION=NO"
Write-Host "NEXT_ACTION=RETURN_OUTPUT_TO_CHAT_FOR_TIMEOUT_DECISION"

if ($passing.Count -eq 0) { exit 1 }
exit 0
