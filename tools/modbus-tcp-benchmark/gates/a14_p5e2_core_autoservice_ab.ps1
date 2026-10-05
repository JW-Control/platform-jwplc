param(
    [string]$MasterPort = "COM14",
    [string]$SlavePort = "COM4",
    [double]$DurationS = 60.0
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

. (Join-Path $PSScriptRoot "common.ps1")

function Invoke-NativeToLog {
    param(
        [string]$FilePath,
        [string[]]$Arguments,
        [string]$LogPath
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

Write-Host "============================================================"
Write-Host " A14 P5-E2 - PACKAGE-CORE MODBUS TCP AUTOSERVICE"
Write-Host "============================================================"

Assert-G2Branch

$head = Get-G2Head
$spiHz = Get-G2SpiHz
$dirty = @(Get-G2TrackedDirtyPaths)
$staged = @(& git -C $script:G2RepoRoot diff --cached --name-only)

Write-Host "HEAD=$head"
Write-Host "W5500_SPI_HZ=$spiHz"
Write-Host "TRACKED_DIRTY_BEFORE=$($dirty.Count)"
Write-Host "STAGED_BEFORE=$($staged.Count)"
Write-Host "MASTER_PORT=$MasterPort"
Write-Host "SLAVE_PORT=$SlavePort"
Write-Host ("DURATION_S={0:F0}" -f $DurationS)
Write-Host "PUBLIC_API_CHANGE=NO"
Write-Host "USER_MANUAL_TCP_TASK_REQUIRED=NO"
Write-Host "CANDIDATE=CORE_PRE_POST_LOOP_AUTOSERVICE"
Write-Host "PRODUCT_ADOPTION=NOT_YET"

if ($spiHz -ne 26000000) {
    throw "P5E2_EXPECTED_26MHZ"
}

if ($dirty.Count -ne 0) {
    $dirty | ForEach-Object { Write-Host "DIRTY=$_" }
    throw "P5E2_TREE_NOT_CLEAN_BEFORE_REBUILD"
}

if ($staged.Count -ne 0) {
    throw "P5E2_INDEX_NOT_CLEAN_BEFORE_REBUILD"
}

if ($DurationS -lt 30.0) {
    throw "P5E2_DURATION_TOO_SHORT"
}

Assert-G2ProtectedArtifacts

$mainPath = Get-G2Path "JWPLC/2.1.0/cores/jwcontrol/main.cpp"
$modbusPath = Get-G2Path "JWPLC/2.1.0/libraries/JWPLC_ModbusTCP/src/JWPLC_ModbusTCP.cpp"
$masterSketch = Get-G2Path "tools/modbus-tcp-benchmark/firmware/a14_p5_full_runtime_master/a14_p5_full_runtime_master.ino"
$buildScript = Get-G2Path "tools/build-speed-benchmark/Build-JWPLCPrecompiledCore.ps1"
$verifyScript = Get-G2Path "tools/build-speed-benchmark/Verify-JWPLCPrecompiledCore.ps1"
$p5bGate = Join-Path $PSScriptRoot "a14_p5b_physical_master_slave_combined.ps1"
$runner = Get-G2Path "tools/modbus-tcp-benchmark/pc/a14_p5e1_unpaced_fc03_ceiling.py"
$arduinoCli = "C:\Program Files\Arduino PLC IDE Tools\arduino-cli.exe"

foreach ($required in @(
    $mainPath,
    $modbusPath,
    $masterSketch,
    $buildScript,
    $verifyScript,
    $p5bGate,
    $runner,
    $arduinoCli
)) {
    if (-not (Test-Path -LiteralPath $required)) {
        throw "P5E2_REQUIRED_PATH_MISSING=$required"
    }
}

$mainText = [System.IO.File]::ReadAllText($mainPath)
$modbusText = [System.IO.File]::ReadAllText($modbusPath)
$masterText = [System.IO.File]::ReadAllText($masterSketch)

$mainHookCalls = @(
    [regex]::Matches(
        $mainText,
        [regex]::Escape("jwplcModbusTCPLoopServiceCallback();")
    )
).Count

$providerCount = @(
    [regex]::Matches(
        $modbusText,
        [regex]::Escape('extern "C" void jwplcModbusTCPLoopServiceCallback(void)')
    )
).Count

$manualTaskCalls = @(
    [regex]::Matches(
        $masterText,
        [regex]::Escape("JWPLC_ModbusTCP.task();")
    )
).Count

Write-Host "P5E2_CORE_HOOK_CALLS=$mainHookCalls"
Write-Host "P5E2_LIBRARY_PROVIDER_COUNT=$providerCount"
Write-Host "P5E2_MASTER_MANUAL_TASK_CALLS=$manualTaskCalls"

if ($mainHookCalls -ne 2) {
    throw "P5E2_CORE_HOOK_CALL_COUNT_INVALID"
}

if ($providerCount -ne 1) {
    throw "P5E2_LIBRARY_PROVIDER_COUNT_INVALID"
}

if ($manualTaskCalls -ne 0) {
    throw "P5E2_MASTER_STILL_HAS_MANUAL_TCP_TASK"
}

$pythonCommand = Get-Command python.exe -ErrorAction SilentlyContinue

if ($null -eq $pythonCommand) {
    $pythonCommand = Get-Command python -ErrorAction SilentlyContinue
}

if ($null -eq $pythonCommand) {
    throw "P5E2_PYTHON_NOT_FOUND"
}

$pythonExe = $pythonCommand.Source
$timestamp = Get-Date -Format "yyyyMMdd_HHmmss"
$tempRoot = Join-Path $env:TEMP ("jwplc_a14_p5e2_core_{0}" -f $timestamp)
$buildLog = Join-Path $tempRoot "rebuild_core.log"
$verifyLog = Join-Path $tempRoot "verify_core.log"
$setupLog = Join-Path $tempRoot "setup.log"
$runnerLog = Join-Path $tempRoot "unpaced.log"

New-Item -ItemType Directory -Force -Path $tempRoot | Out-Null

Write-Host "TEMP_ROOT=$tempRoot"

Write-Host ""
Write-Host "=== REBUILD CANDIDATE CORE.A ==="

$buildArgs = @(
    "-NoLogo",
    "-NoProfile",
    "-ExecutionPolicy", "Bypass",
    "-File", $buildScript,
    "-ArduinoCli", $arduinoCli,
    "-Targets", "Basic"
)

$buildExit = Invoke-NativeToLog -FilePath "powershell.exe" -Arguments $buildArgs -LogPath $buildLog

Write-Host "P5E2_CORE_REBUILD_EXIT=$buildExit"
Write-Host "P5E2_CORE_REBUILD_LOG=$buildLog"

if ($buildExit -ne 0) {
    Get-Content -LiteralPath $buildLog -Tail 300 | ForEach-Object { Write-Host $_ }
    throw "P5E2_CORE_REBUILD_FAILED"
}

$coreHashCandidate = Get-G2Sha256 $script:G2CoreRelative
Write-Host "P5E2_CORE_SHA_CANDIDATE=$coreHashCandidate"

if ($coreHashCandidate -eq $script:G2CoreSha256) {
    throw "P5E2_CORE_SHA_DID_NOT_CHANGE"
}

$dirtyAfterBuild = @(Get-G2TrackedDirtyPaths)
$stagedAfterBuild = @(& git -C $script:G2RepoRoot diff --cached --name-only)

Write-Host "P5E2_DIRTY_AFTER_REBUILD=$($dirtyAfterBuild.Count)"
$dirtyAfterBuild | ForEach-Object { Write-Host "P5E2_DIRTY=$_" }

if (
    $dirtyAfterBuild.Count -ne 1 -or
    $dirtyAfterBuild[0].Replace("\", "/") -ne $script:G2CoreRelative
) {
    throw "P5E2_CORE_REBUILD_DIRTY_SCOPE_INVALID"
}

if ($stagedAfterBuild.Count -ne 0) {
    throw "P5E2_CORE_REBUILD_STAGED_UNEXPECTED"
}

Write-Host ""
Write-Host "=== VERIFY PRECOMPILED CORE PATH ==="

$verifyArgs = @(
    "-NoLogo",
    "-NoProfile",
    "-ExecutionPolicy", "Bypass",
    "-File", $verifyScript,
    "-Target", "Basic",
    "-ArduinoCli", $arduinoCli
)

$verifyExit = Invoke-NativeToLog -FilePath "powershell.exe" -Arguments $verifyArgs -LogPath $verifyLog

Write-Host "P5E2_CORE_VERIFY_EXIT=$verifyExit"
Write-Host "P5E2_CORE_VERIFY_LOG=$verifyLog"

if ($verifyExit -ne 0) {
    Get-Content -LiteralPath $verifyLog -Tail 300 | ForEach-Object { Write-Host $_ }
    throw "P5E2_CORE_VERIFY_FAILED"
}

Write-Host ""
Write-Host "=== COMPILE / UPLOAD FULL RUNTIME WITH CANDIDATE CORE ==="

$setupArgs = @(
    "-NoLogo",
    "-NoProfile",
    "-ExecutionPolicy", "Bypass",
    "-File", $p5bGate,
    "-MasterPort", $MasterPort,
    "-SlavePort", $SlavePort,
    "-SetupOnly",
    "-AllowDirtyCoreCandidate"
)

$setupExit = Invoke-NativeToLog -FilePath "powershell.exe" -Arguments $setupArgs -LogPath $setupLog

Write-Host "P5E2_SETUP_EXIT=$setupExit"
Write-Host "P5E2_SETUP_LOG=$setupLog"

Get-Content -LiteralPath $setupLog | ForEach-Object { Write-Host $_ }

if ($setupExit -ne 0) {
    throw "P5E2_SETUP_FAILED"
}

$setupText = [System.IO.File]::ReadAllText($setupLog)
$ipMatches = @(
    [regex]::Matches(
        $setupText,
        "(?m)^P5B_SETUP_ONLY_MASTER_IP=(.+?)\r?$"
    )
)

if ($ipMatches.Count -ne 1) {
    throw "P5E2_SETUP_IP_COUNT_$($ipMatches.Count)"
}

$dutIp = $ipMatches[0].Groups[1].Value.Trim()
Write-Host "P5E2_DUT_IP=$dutIp"

Write-Host ""
Write-Host "=== RUN UNPACED FULL-RUNTIME CANDIDATE ==="

$durationText = $DurationS.ToString(
    [System.Globalization.CultureInfo]::InvariantCulture
)

$runnerArgs = @(
    $runner,
    "--master-serial", $MasterPort,
    "--slave-serial", $SlavePort,
    "--host", $dutIp,
    "--port", "502",
    "--duration", $durationText,
    "--quantity", "125",
    "--bucket-seconds", "60"
)

$runnerExit = Invoke-NativeToLog -FilePath $pythonExe -Arguments $runnerArgs -LogPath $runnerLog

Write-Host "P5E2_RUNNER_EXIT=$runnerExit"
Write-Host "P5E2_RUNNER_LOG=$runnerLog"
Write-Host ""

Get-Content -LiteralPath $runnerLog | ForEach-Object { Write-Host $_ }

if ($runnerExit -ne 0) {
    throw "P5E2_CANDIDATE_CHARACTERIZATION_FAILED"
}

$runnerText = [System.IO.File]::ReadAllText($runnerLog)
$rateMatch = [regex]::Match(
    $runnerText,
    "(?m)^P5E1_ACHIEVED_REQ_S=([0-9.]+)\r?$"
)

if (-not $rateMatch.Success) {
    throw "P5E2_ACHIEVED_RATE_MISSING"
}

$achieved = [double]::Parse(
    $rateMatch.Groups[1].Value,
    [System.Globalization.CultureInfo]::InvariantCulture
)

$target1020 = $achieved -ge 1020.0
$target1050 = $achieved -ge 1050.0

Write-Host ""
Write-Host "P5E2_ACHIEVED_REQ_S=$($achieved.ToString('F3', [System.Globalization.CultureInfo]::InvariantCulture))"
Write-Host "P5E2_TARGET_1020_PASS=$target1020"
Write-Host "P5E2_TARGET_1050_PASS=$target1050"

Write-Host ""
Write-Host "============================================================"
Write-Host " PHYSICAL TFT OBSERVATION"
Write-Host "============================================================"

$masterAnswer = Read-Host "¿MASTER COM14 estable y sin parpadeo/cortes visibles? (S/N)"
$slaveAnswer = Read-Host "¿SLAVE COM4 estable y sin parpadeo/cortes visibles? (S/N)"

$masterPhysical = $masterAnswer.Trim().ToUpper() -eq "S"
$slavePhysical = $slaveAnswer.Trim().ToUpper() -eq "S"
$physicalPass = $masterPhysical -and $slavePhysical

Write-Host "MASTER_TFT_PHYSICAL_PASS=$masterPhysical"
Write-Host "SLAVE_TFT_PHYSICAL_PASS=$slavePhysical"
Write-Host "TFT_PHYSICAL_PASS=$physicalPass"

if (-not $physicalPass) {
    throw "P5E2_TFT_PHYSICAL_REVIEW"
}

Write-Host ""
Write-Host "=== FINAL CONTROLLED CANDIDATE STATE ==="

$finalCoreHash = Get-G2Sha256 $script:G2CoreRelative
$finalSdHash = Get-G2Sha256 $script:G2SdRelative
$finalDirty = @(Get-G2TrackedDirtyPaths)
$finalStaged = @(& git -C $script:G2RepoRoot diff --cached --name-only)
$finalHz = Get-G2SpiHz

Write-Host "P5E2_CORE_SHA_FINAL=$finalCoreHash"
Write-Host "LIBJW_SD_A_SHA256=$finalSdHash"
Write-Host "FINAL_SPI_HZ=$finalHz"
Write-Host "TRACKED_DIRTY_FINAL=$($finalDirty.Count)"
Write-Host "STAGED_COUNT_FINAL=$($finalStaged.Count)"

if ($finalCoreHash -ne $coreHashCandidate) {
    throw "P5E2_CANDIDATE_CORE_CHANGED_DURING_RUN"
}

if ($finalSdHash -ne $script:G2SdSha256) {
    throw "P5E2_SD_ARTIFACT_CHANGED"
}

if ($finalHz -ne 26000000) {
    throw "P5E2_SPI_CHANGED"
}

if (
    $finalDirty.Count -ne 1 -or
    $finalDirty[0].Replace("\", "/") -ne $script:G2CoreRelative
) {
    throw "P5E2_FINAL_DIRTY_SCOPE_INVALID"
}

if ($finalStaged.Count -ne 0) {
    throw "P5E2_FINAL_INDEX_NOT_CLEAN"
}

Write-Host ""
Write-Host "============================================================"
Write-Host " A14 P5-E2 FINAL SUMMARY"
Write-Host "============================================================"
Write-Host "A14_P5E2_CANDIDATE=CORE_PRE_POST_LOOP_AUTOSERVICE"
Write-Host "A14_P5E2_USER_MANUAL_TCP_TASK_REQUIRED=NO"
Write-Host "A14_P5E2_PUBLIC_API_CHANGE=NO"
Write-Host "A14_P5E2_CORE_SHA_CANDIDATE=$coreHashCandidate"
Write-Host "A14_P5E2_TARGET_1020_PASS=$target1020"
Write-Host "A14_P5E2_TARGET_1050_PASS=$target1050"
Write-Host "A14_P5E2_CHARACTERIZATION=PASS"
Write-Host "A14_P5E2_PRODUCT_CANDIDATE_DIRTY_CORE_A=YES"
Write-Host "NEXT=RETURN_OUTPUT_TO_CHAT_BEFORE_ADOPTING_CORE_A"
