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
Write-Host " A14 P5-E1 - FULL RUNTIME FC03/125 UNPACED CEILING"
Write-Host "============================================================"

Assert-G2Branch

$head = Get-G2Head
$spiHz = Get-G2SpiHz
$dirty = @(Get-G2TrackedDirtyPaths)
$staged = @(
    & git -C $script:G2RepoRoot diff --cached --name-only
)

if ($LASTEXITCODE -ne 0) {
    throw "P5E1_CACHED_DIFF_FAILED"
}

Write-Host "HEAD=$head"
Write-Host "W5500_SPI_HZ=$spiHz"
Write-Host "TRACKED_DIRTY_COUNT=$($dirty.Count)"
Write-Host "STAGED_COUNT=$($staged.Count)"
Write-Host "MASTER_PORT=$MasterPort"
Write-Host "SLAVE_PORT=$SlavePort"
Write-Host ("DURATION_S={0:F0}" -f $DurationS)
Write-Host "FC03_QUANTITY_REGISTERS=125"
Write-Host "TCP_PACING=NONE"
Write-Host "TCP_OUTSTANDING_REQUESTS=1"
Write-Host "PRODUCT_SOURCE_MUTATION=NO"

if ($spiHz -ne 26000000) {
    throw "P5E1_EXPECTED_26MHZ"
}

if ($dirty.Count -ne 0) {
    $dirty | ForEach-Object {
        Write-Host "DIRTY=$_"
    }

    throw "P5E1_TRACKED_TREE_NOT_CLEAN"
}

if ($staged.Count -ne 0) {
    $staged | ForEach-Object {
        Write-Host "STAGED=$_"
    }

    throw "P5E1_INDEX_NOT_CLEAN"
}

if ($DurationS -lt 30.0) {
    throw "P5E1_DURATION_TOO_SHORT"
}

Assert-G2ProtectedArtifacts

$pythonCommand = Get-Command python.exe -ErrorAction SilentlyContinue

if ($null -eq $pythonCommand) {
    $pythonCommand = Get-Command python -ErrorAction SilentlyContinue
}

if ($null -eq $pythonCommand) {
    throw "P5E1_PYTHON_NOT_FOUND"
}

$pythonExe = $pythonCommand.Source

$setupGate = Join-Path $PSScriptRoot "a14_p5b_physical_master_slave_combined.ps1"
$runner = Get-G2Path "tools/modbus-tcp-benchmark/pc/a14_p5e1_unpaced_fc03_ceiling.py"

foreach ($required in @($setupGate, $runner)) {
    if (-not (Test-Path -LiteralPath $required)) {
        throw "P5E1_REQUIRED_FILE_MISSING=$required"
    }
}

$timestamp = Get-Date -Format "yyyyMMdd_HHmmss"
$tempRoot = Join-Path $env:TEMP ("jwplc_a14_p5e1_{0}" -f $timestamp)
$setupLog = Join-Path $tempRoot "setup.log"
$runnerLog = Join-Path $tempRoot "unpaced.log"
$pyCompileLog = Join-Path $tempRoot "py_compile.log"

New-Item -ItemType Directory -Force -Path $tempRoot | Out-Null

Write-Host "TEMP_ROOT=$tempRoot"

Write-Host ""
Write-Host "=== PYTHON SYNTAX PREFLIGHT ==="

$pyCompileArgs = @(
    "-m",
    "py_compile",
    $runner
)

$pyCompileExit = Invoke-NativeToLog -FilePath $pythonExe -Arguments $pyCompileArgs -LogPath $pyCompileLog

Write-Host "P5E1_PY_COMPILE_EXIT=$pyCompileExit"

if ($pyCompileExit -ne 0) {
    Get-Content -LiteralPath $pyCompileLog -Tail 200 |
        ForEach-Object { Write-Host $_ }

    throw "P5E1_PYTHON_SYNTAX_FAILED"
}

Write-Host ""
Write-Host "=== REUSE P5-B SETUP-ONLY ==="

$setupArgs = @(
    "-NoLogo",
    "-NoProfile",
    "-ExecutionPolicy", "Bypass",
    "-File", $setupGate,
    "-MasterPort", $MasterPort,
    "-SlavePort", $SlavePort,
    "-SetupOnly"
)

$setupExit = Invoke-NativeToLog -FilePath "powershell.exe" -Arguments $setupArgs -LogPath $setupLog

Write-Host "P5E1_SETUP_EXIT=$setupExit"
Write-Host "P5E1_SETUP_LOG=$setupLog"

Get-Content -LiteralPath $setupLog |
    ForEach-Object { Write-Host $_ }

if ($setupExit -ne 0) {
    throw "P5E1_SETUP_FAILED"
}

$setupText = [System.IO.File]::ReadAllText($setupLog)
$ipMatches = @(
    [regex]::Matches(
        $setupText,
        "(?m)^P5B_SETUP_ONLY_MASTER_IP=(.+?)\r?$"
    )
)

if ($ipMatches.Count -ne 1) {
    throw "P5E1_SETUP_IP_COUNT_$($ipMatches.Count)"
}

$dutIp = $ipMatches[0].Groups[1].Value.Trim()

Write-Host ""
Write-Host "P5E1_DUT_IP=$dutIp"

Write-Host ""
Write-Host "=== RUN UNPACED FC03/125 CEILING ==="

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
    "--quantity", "125"
)

$runnerExit = Invoke-NativeToLog -FilePath $pythonExe -Arguments $runnerArgs -LogPath $runnerLog

Write-Host "P5E1_RUNNER_EXIT=$runnerExit"
Write-Host "P5E1_RUNNER_LOG=$runnerLog"
Write-Host ""

Get-Content -LiteralPath $runnerLog |
    ForEach-Object { Write-Host $_ }

if ($runnerExit -ne 0) {
    throw "P5E1_UNPACED_CHARACTERIZATION_FAILED"
}

$runnerText = [System.IO.File]::ReadAllText($runnerLog)

if (
    $runnerText -notmatch
    "(?m)^A14_P5E1_UNPACED_CEILING=PASS_CHARACTERIZED\r?$"
) {
    throw "P5E1_TERMINAL_PASS_MISSING"
}

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
    throw "P5E1_TFT_PHYSICAL_REVIEW"
}

Write-Host ""
Write-Host "=== FINAL PRODUCT INVARIANTS ==="

Assert-G2ProtectedArtifacts

$finalHz = Get-G2SpiHz
$finalDirty = @(Get-G2TrackedDirtyPaths)
$finalStaged = @(
    & git -C $script:G2RepoRoot diff --cached --name-only
)

if ($LASTEXITCODE -ne 0) {
    throw "P5E1_FINAL_CACHED_DIFF_FAILED"
}

Write-Host "FINAL_SPI_HZ=$finalHz"
Write-Host "TRACKED_DIRTY_FINAL=$($finalDirty.Count)"
Write-Host "STAGED_COUNT_FINAL=$($finalStaged.Count)"

if ($finalHz -ne 26000000) {
    throw "P5E1_FINAL_SPI_CHANGED"
}

if ($finalDirty.Count -ne 0) {
    throw "P5E1_PRODUCT_TREE_DIRTY"
}

if ($finalStaged.Count -ne 0) {
    throw "P5E1_PRODUCT_INDEX_DIRTY"
}

Write-Host ""
Write-Host "============================================================"
Write-Host " A14 P5-E1 FINAL SUMMARY"
Write-Host "============================================================"
Write-Host "A14_P5E1_PROFILE=FULL_RUNTIME_FC03_125_UNPACED"
Write-Host ("A14_P5E1_DURATION_S={0:F0}" -f $DurationS)
Write-Host "A14_P5E1_RTU_TARGET_HZ=50"
Write-Host "A14_P5E1_SD_WORKLOAD_MODE=BUFFERED_DATALOG"
Write-Host "A14_P5E1_W5500_SPI_HZ=26000000"
Write-Host "A14_P5E1_PRODUCT_SOURCE_MUTATION=NO"
Write-Host "A14_P5E1_UNPACED_CEILING=PASS"
Write-Host "NEXT=RETURN_OUTPUT_TO_CHAT_FOR_P5E2_DECISION"
