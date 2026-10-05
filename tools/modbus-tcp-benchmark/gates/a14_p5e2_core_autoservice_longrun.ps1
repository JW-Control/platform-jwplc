param(
    [string]$MasterPort = "COM14",
    [string]$SlavePort = "COM4",
    [double]$DurationS = 600.0
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
Write-Host " A14 P5-E2-R1 - CORE AUTOSERVICE 600S LONG-RUN"
Write-Host "============================================================"

Assert-G2Branch

$expectedCoreHash = "4BFF8C8241DA2E8BD0E1BBA99835ADDF91B9085A05C4DFBD05C339B824794566"
$head = Get-G2Head
$spiHz = Get-G2SpiHz
$dirty = @(Get-G2TrackedDirtyPaths)
$staged = @(& git -C $script:G2RepoRoot diff --cached --name-only)

Write-Host "HEAD=$head"
Write-Host "W5500_SPI_HZ=$spiHz"
Write-Host "MASTER_PORT=$MasterPort"
Write-Host "SLAVE_PORT=$SlavePort"
Write-Host ("DURATION_S={0:F0}" -f $DurationS)
Write-Host "BUCKET_SECONDS=60"
Write-Host "CANDIDATE=CORE_PRE_POST_LOOP_AUTOSERVICE"
Write-Host "EXPECTED_CORE_SHA256=$expectedCoreHash"
Write-Host "PRODUCT_ADOPTION=NOT_YET"

if ($spiHz -ne 26000000) {
    throw "P5E2R1_EXPECTED_26MHZ"
}

if ($DurationS -lt 600.0) {
    throw "P5E2R1_DURATION_MUST_BE_AT_LEAST_600S"
}

if ($staged.Count -ne 0) {
    $staged | ForEach-Object { Write-Host "STAGED=$_" }
    throw "P5E2R1_INDEX_NOT_CLEAN"
}

if (
    $dirty.Count -ne 1 -or
    $dirty[0].Replace("\", "/") -ne $script:G2CoreRelative
) {
    $dirty | ForEach-Object { Write-Host "DIRTY=$_" }
    throw "P5E2R1_EXPECTED_ONLY_DIRTY_CORE_A"
}

$coreHash = Get-G2Sha256 $script:G2CoreRelative
$sdHash = Get-G2Sha256 $script:G2SdRelative

Write-Host "CORE_A_SHA256=$coreHash"
Write-Host "LIBJW_SD_A_SHA256=$sdHash"

if ($coreHash -ne $expectedCoreHash) {
    throw "P5E2R1_UNEXPECTED_CANDIDATE_CORE_HASH"
}

if ($sdHash -ne $script:G2SdSha256) {
    throw "P5E2R1_SD_HASH_MISMATCH"
}

$mainPath = Get-G2Path "JWPLC/2.1.0/cores/jwcontrol/main.cpp"
$modbusPath = Get-G2Path "JWPLC/2.1.0/libraries/JWPLC_ModbusTCP/src/JWPLC_ModbusTCP.cpp"
$masterSketch = Get-G2Path "tools/modbus-tcp-benchmark/firmware/a14_p5_full_runtime_master/a14_p5_full_runtime_master.ino"
$p5bGate = Join-Path $PSScriptRoot "a14_p5b_physical_master_slave_combined.ps1"
$runner = Get-G2Path "tools/modbus-tcp-benchmark/pc/a14_p5e1_unpaced_fc03_ceiling.py"
$verifyScript = Get-G2Path "tools/build-speed-benchmark/Verify-JWPLCPrecompiledCore.ps1"
$arduinoCli = "C:\Program Files\Arduino PLC IDE Tools\arduino-cli.exe"

foreach ($required in @(
    $mainPath,
    $modbusPath,
    $masterSketch,
    $p5bGate,
    $runner,
    $verifyScript,
    $arduinoCli
)) {
    if (-not (Test-Path -LiteralPath $required)) {
        throw "P5E2R1_REQUIRED_PATH_MISSING=$required"
    }
}

$mainText = [System.IO.File]::ReadAllText($mainPath)
$modbusText = [System.IO.File]::ReadAllText($modbusPath)
$masterText = [System.IO.File]::ReadAllText($masterSketch)

$coreHookCalls = @(
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

Write-Host "P5E2R1_CORE_HOOK_CALLS=$coreHookCalls"
Write-Host "P5E2R1_LIBRARY_PROVIDER_COUNT=$providerCount"
Write-Host "P5E2R1_MASTER_MANUAL_TASK_CALLS=$manualTaskCalls"

if ($coreHookCalls -ne 2) {
    throw "P5E2R1_CORE_HOOK_CALL_COUNT_INVALID"
}

if ($providerCount -ne 1) {
    throw "P5E2R1_PROVIDER_COUNT_INVALID"
}

if ($manualTaskCalls -ne 0) {
    throw "P5E2R1_MANUAL_TCP_TASK_REAPPEARED"
}

$pythonCommand = Get-Command python.exe -ErrorAction SilentlyContinue
if ($null -eq $pythonCommand) {
    $pythonCommand = Get-Command python -ErrorAction SilentlyContinue
}
if ($null -eq $pythonCommand) {
    throw "P5E2R1_PYTHON_NOT_FOUND"
}
$pythonExe = $pythonCommand.Source

$timestamp = Get-Date -Format "yyyyMMdd_HHmmss"
$tempRoot = Join-Path $env:TEMP ("jwplc_a14_p5e2_r1_{0}" -f $timestamp)
$verifyLog = Join-Path $tempRoot "verify_core.log"
$setupLog = Join-Path $tempRoot "setup.log"
$runnerLog = Join-Path $tempRoot "unpaced_600s.log"

New-Item -ItemType Directory -Force -Path $tempRoot | Out-Null
Write-Host "TEMP_ROOT=$tempRoot"

Write-Host ""
Write-Host "=== VERIFY EXISTING CANDIDATE CORE.A ==="

$verifyArgs = @(
    "-NoLogo",
    "-NoProfile",
    "-ExecutionPolicy", "Bypass",
    "-File", $verifyScript,
    "-Target", "Basic",
    "-ArduinoCli", $arduinoCli
)

$verifyExit = Invoke-NativeToLog -FilePath "powershell.exe" -Arguments $verifyArgs -LogPath $verifyLog
Write-Host "P5E2R1_CORE_VERIFY_EXIT=$verifyExit"
Write-Host "P5E2R1_CORE_VERIFY_LOG=$verifyLog"

if ($verifyExit -ne 0) {
    Get-Content -LiteralPath $verifyLog -Tail 300 | ForEach-Object { Write-Host $_ }
    throw "P5E2R1_CORE_VERIFY_FAILED"
}

Write-Host ""
Write-Host "=== COMPILE / UPLOAD FULL RUNTIME WITH SAME CANDIDATE CORE ==="

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
Write-Host "P5E2R1_SETUP_EXIT=$setupExit"
Write-Host "P5E2R1_SETUP_LOG=$setupLog"

Get-Content -LiteralPath $setupLog | ForEach-Object { Write-Host $_ }

if ($setupExit -ne 0) {
    throw "P5E2R1_SETUP_FAILED"
}

$setupText = [System.IO.File]::ReadAllText($setupLog)
$ipMatches = @(
    [regex]::Matches(
        $setupText,
        "(?m)^P5B_SETUP_ONLY_MASTER_IP=(.+?)\r?$"
    )
)

if ($ipMatches.Count -ne 1) {
    throw "P5E2R1_SETUP_IP_COUNT_$($ipMatches.Count)"
}

$dutIp = $ipMatches[0].Groups[1].Value.Trim()
Write-Host "P5E2R1_DUT_IP=$dutIp"

Write-Host ""
Write-Host "=== RUN 600S UNPACED FULL-RUNTIME CANDIDATE ==="

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
Write-Host "P5E2R1_RUNNER_EXIT=$runnerExit"
Write-Host "P5E2R1_RUNNER_LOG=$runnerLog"
Write-Host ""

Get-Content -LiteralPath $runnerLog | ForEach-Object { Write-Host $_ }

if ($runnerExit -ne 0) {
    throw "P5E2R1_CHARACTERIZATION_FAILED"
}

$runnerText = [System.IO.File]::ReadAllText($runnerLog)

function Get-RunnerDouble {
    param([string]$Key)

    $m = [regex]::Match(
        $runnerText,
        "(?m)^" + [regex]::Escape($Key) + "=([0-9.]+)\r?$"
    )

    if (-not $m.Success) {
        throw "P5E2R1_MISSING_$Key"
    }

    return [double]::Parse(
        $m.Groups[1].Value,
        [System.Globalization.CultureInfo]::InvariantCulture
    )
}

$achieved = Get-RunnerDouble "P5E1_ACHIEVED_REQ_S"
$latAvg = Get-RunnerDouble "P5E1_LATENCY_AVG_US"
$latP95 = Get-RunnerDouble "P5E1_LATENCY_P95_US"
$latP99 = Get-RunnerDouble "P5E1_LATENCY_P99_US"
$latMax = Get-RunnerDouble "P5E1_LATENCY_MAX_US"

$bucketMatches = @(
    [regex]::Matches(
        $runnerText,
        "(?m)^P5E1_BUCKET INDEX=(\d+) START_S=([0-9.]+) END_S=([0-9.]+) OK=(\d+) REQ_S=([0-9.]+)\r?$"
    )
)

$expectedBuckets = [int][Math]::Ceiling($DurationS / 60.0)

Write-Host ""
Write-Host "P5E2R1_BUCKET_COUNT=$($bucketMatches.Count)"
Write-Host "P5E2R1_EXPECTED_BUCKET_COUNT=$expectedBuckets"

if ($bucketMatches.Count -ne $expectedBuckets) {
    throw "P5E2R1_BUCKET_COUNT_INVALID"
}

$bucketRates = @(
    foreach ($m in $bucketMatches) {
        [double]::Parse(
            $m.Groups[5].Value,
            [System.Globalization.CultureInfo]::InvariantCulture
        )
    }
)

$bucketMin = ($bucketRates | Measure-Object -Minimum).Minimum
$bucketMax = ($bucketRates | Measure-Object -Maximum).Maximum
$half = [int]($bucketRates.Count / 2)
$firstHalf = @($bucketRates[0..($half - 1)])
$secondHalf = @($bucketRates[$half..($bucketRates.Count - 1)])
$firstHalfAvg = ($firstHalf | Measure-Object -Average).Average
$secondHalfAvg = ($secondHalf | Measure-Object -Average).Average
$halfDriftPct = if ($firstHalfAvg -gt 0.0) {
    (($secondHalfAvg - $firstHalfAvg) / $firstHalfAvg) * 100.0
}
else {
    0.0
}

$target1000 = $achieved -ge 1000.0
$target1020 = $achieved -ge 1020.0
$target1050 = $achieved -ge 1050.0

Write-Host "P5E2R1_ACHIEVED_REQ_S=$($achieved.ToString('F3', [System.Globalization.CultureInfo]::InvariantCulture))"
Write-Host "P5E2R1_BUCKET_MIN_REQ_S=$($bucketMin.ToString('F3', [System.Globalization.CultureInfo]::InvariantCulture))"
Write-Host "P5E2R1_BUCKET_MAX_REQ_S=$($bucketMax.ToString('F3', [System.Globalization.CultureInfo]::InvariantCulture))"
Write-Host "P5E2R1_FIRST_HALF_AVG_REQ_S=$($firstHalfAvg.ToString('F3', [System.Globalization.CultureInfo]::InvariantCulture))"
Write-Host "P5E2R1_SECOND_HALF_AVG_REQ_S=$($secondHalfAvg.ToString('F3', [System.Globalization.CultureInfo]::InvariantCulture))"
Write-Host "P5E2R1_HALF_DRIFT_PCT=$($halfDriftPct.ToString('F3', [System.Globalization.CultureInfo]::InvariantCulture))"
Write-Host "P5E2R1_LATENCY_AVG_US=$($latAvg.ToString('F1', [System.Globalization.CultureInfo]::InvariantCulture))"
Write-Host "P5E2R1_LATENCY_P95_US=$($latP95.ToString('F1', [System.Globalization.CultureInfo]::InvariantCulture))"
Write-Host "P5E2R1_LATENCY_P99_US=$($latP99.ToString('F1', [System.Globalization.CultureInfo]::InvariantCulture))"
Write-Host "P5E2R1_LATENCY_MAX_US=$($latMax.ToString('F1', [System.Globalization.CultureInfo]::InvariantCulture))"
Write-Host "P5E2R1_TARGET_1000_PASS=$target1000"
Write-Host "P5E2R1_TARGET_1020_PASS=$target1020"
Write-Host "P5E2R1_TARGET_1050_PASS=$target1050"

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
    throw "P5E2R1_TFT_PHYSICAL_REVIEW"
}

Write-Host ""
Write-Host "=== FINAL CONTROLLED CANDIDATE STATE ==="

$finalCoreHash = Get-G2Sha256 $script:G2CoreRelative
$finalSdHash = Get-G2Sha256 $script:G2SdRelative
$finalDirty = @(Get-G2TrackedDirtyPaths)
$finalStaged = @(& git -C $script:G2RepoRoot diff --cached --name-only)
$finalHz = Get-G2SpiHz

Write-Host "CORE_A_SHA256=$finalCoreHash"
Write-Host "LIBJW_SD_A_SHA256=$finalSdHash"
Write-Host "FINAL_SPI_HZ=$finalHz"
Write-Host "TRACKED_DIRTY_FINAL=$($finalDirty.Count)"
Write-Host "STAGED_COUNT_FINAL=$($finalStaged.Count)"

if ($finalCoreHash -ne $expectedCoreHash) {
    throw "P5E2R1_CORE_HASH_CHANGED"
}

if ($finalSdHash -ne $script:G2SdSha256) {
    throw "P5E2R1_SD_HASH_CHANGED"
}

if ($finalHz -ne 26000000) {
    throw "P5E2R1_SPI_CHANGED"
}

if (
    $finalDirty.Count -ne 1 -or
    $finalDirty[0].Replace("\", "/") -ne $script:G2CoreRelative
) {
    throw "P5E2R1_FINAL_DIRTY_SCOPE_INVALID"
}

if ($finalStaged.Count -ne 0) {
    throw "P5E2R1_FINAL_INDEX_NOT_CLEAN"
}

Write-Host ""
Write-Host "============================================================"
Write-Host " A14 P5-E2-R1 FINAL SUMMARY"
Write-Host "============================================================"
Write-Host "A14_P5E2R1_CANDIDATE=CORE_PRE_POST_LOOP_AUTOSERVICE"
Write-Host "A14_P5E2R1_DURATION_S=$($DurationS.ToString('F0', [System.Globalization.CultureInfo]::InvariantCulture))"
Write-Host "A14_P5E2R1_BUCKET_SECONDS=60"
Write-Host "A14_P5E2R1_USER_MANUAL_TCP_TASK_REQUIRED=NO"
Write-Host "A14_P5E2R1_PUBLIC_API_CHANGE=NO"
Write-Host "A14_P5E2R1_CORE_SHA_CANDIDATE=$expectedCoreHash"
Write-Host "A14_P5E2R1_TARGET_1000_PASS=$target1000"
Write-Host "A14_P5E2R1_TARGET_1020_PASS=$target1020"
Write-Host "A14_P5E2R1_TARGET_1050_PASS=$target1050"
Write-Host "A14_P5E2R1_LONG_RUN=PASS_CHARACTERIZED"
Write-Host "A14_P5E2R1_PRODUCT_CANDIDATE_DIRTY_CORE_A=YES"
Write-Host "NEXT=RETURN_OUTPUT_TO_CHAT_FOR_ADOPTION_DECISION"
