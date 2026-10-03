param(
    [string]$MasterPort = "COM14",
    [string]$SlavePort = "COM4",
    [string]$PythonExe = "C:\Users\jeykc\AppData\Local\Programs\Python\Python311\python.exe",
    [double]$DurationS = 600.0,
    [string]$ResultRoot = ""
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

. (Join-Path $PSScriptRoot "common.ps1")

Assert-G2Branch

$repo = $script:G2RepoRoot
$head = Get-G2Head
$dirty = @(Get-G2TrackedDirtyPaths)
$staged = @(& git -C $repo diff --cached --name-only)

if ($dirty.Count -ne 0) {
    $dirty | ForEach-Object { Write-Host "DIRTY=$_" }
    throw "STRICT_CONFIRM_TRACKED_TREE_NOT_CLEAN"
}
if ($staged.Count -ne 0) {
    throw "STRICT_CONFIRM_INDEX_NOT_CLEAN"
}
if ($DurationS -lt 600.0) {
    throw "STRICT_CONFIRM_DURATION_LT_600"
}
if (-not (Test-Path -LiteralPath $PythonExe)) {
    throw "STRICT_CONFIRM_PYTHON_NOT_FOUND"
}

$runner = Get-G2Path "tools/modbus-tcp-benchmark/pc/a14_final_tcp1000_rtu100_strict_confirm.py"
$stamp = Get-Date -Format "yyyyMMdd_HHmmss"

if ([string]::IsNullOrWhiteSpace($ResultRoot)) {
    $ResultRoot = Join-Path $repo "tools\modbus-tcp-benchmark\results\a14_tcp1000_rtu100_strict_confirm_$stamp"
}

New-Item -ItemType Directory -Force -Path $ResultRoot | Out-Null

$sessionLog = Join-Path $ResultRoot "SESSION.log"
Start-Transcript -Path $sessionLog -Force | Out-Null

try {
    Write-Host "=============================================================================="
    Write-Host " ALPHA14 FINAL STRICT CONFIRMATION - TCP1000 + RTU100"
    Write-Host "=============================================================================="
    Write-Host "BRANCH=$(& git -C $repo branch --show-current)"
    Write-Host "HEAD=$head"
    Write-Host "MASTER_PORT=$MasterPort"
    Write-Host "SLAVE_PORT=$SlavePort"
    Write-Host "DURATION_S=$DurationS"
    Write-Host "TCP_TARGET_REQ_S=1000"
    Write-Host "RTU_TARGET_REQ_S=100"
    Write-Host "RTU_WORKLOAD=2DI_2DO_2AI_2AO"
    Write-Host "STRICT_TCP_THRESHOLD_PCT=99.9"
    Write-Host "RTU_TARGET_THRESHOLD_PCT=99.0"
    Write-Host "RTU_SKIPS_REQUIRED=0"
    Write-Host "FIRMWARE_POLICY=REUSE_LAST_SOURCE_FIRST_FULL_RUNTIME_IMAGE"
    Write-Host "HOST_POLICY=PC_IDLE_NO_GAME_NO_STREAMING_NO_DOWNLOADS"
    Write-Host "RESULT_ROOT=$ResultRoot"

    @(
        "DATE=$(Get-Date -Format o)"
        "BRANCH=$(& git -C $repo branch --show-current)"
        "HEAD=$head"
        "MASTER_PORT=$MasterPort"
        "SLAVE_PORT=$SlavePort"
        "DURATION_S=$DurationS"
        "TCP_TARGET_REQ_S=1000"
        "RTU_TARGET_REQ_S=100"
        "RTU_WORKLOAD=EXP_MIX_2DI_2DO_2AI_2AO"
        "STRICT_TCP_THRESHOLD_PCT=99.9"
        "RTU_TARGET_THRESHOLD_PCT=99.0"
        "RTU_SKIPS_REQUIRED=0"
        "FIRMWARE_POLICY=REUSE_LAST_SOURCE_FIRST_FULL_RUNTIME_IMAGE"
        "HOST_POLICY=IDLE_NO_GAME_NO_STREAMING_NO_DOWNLOADS"
    ) | Set-Content -LiteralPath (Join-Path $ResultRoot "MANIFEST.txt") -Encoding UTF8

    & git -C $repo diff --check *> (Join-Path $ResultRoot "git_diff_check.log")
    if ($LASTEXITCODE -ne 0) {
        throw "STRICT_CONFIRM_GIT_DIFF_CHECK_FAILED"
    }

    $escapedRunner = $runner.Replace("'", "''")
    $syntaxCode = "import ast,pathlib; ast.parse(pathlib.Path(r'$escapedRunner').read_text(encoding='utf-8'))"
    & $PythonExe -c $syntaxCode *> (Join-Path $ResultRoot "python_syntax.log")
    if ($LASTEXITCODE -ne 0) {
        throw "STRICT_CONFIRM_PYTHON_SYNTAX_FAILED"
    }

    $runnerLog = Join-Path $ResultRoot "runner.log"
    & $PythonExe -u $runner --master-serial $MasterPort --slave-serial $SlavePort --output-root $ResultRoot --duration $DurationS --tcp-target 1000 --rtu-rate 100 *> $runnerLog
    $runnerExit = $LASTEXITCODE

    Get-Content -LiteralPath $runnerLog -Tail 120 | ForEach-Object { Write-Host $_ }

    if ($runnerExit -ne 0) {
        throw "STRICT_CONFIRM_RUNNER_FAILED"
    }

    $finalStatusPath = Join-Path $ResultRoot "FINAL_STATUS.txt"
    if (-not (Test-Path -LiteralPath $finalStatusPath)) {
        throw "STRICT_CONFIRM_FINAL_STATUS_MISSING"
    }

    $finalStatusText = [IO.File]::ReadAllText($finalStatusPath)
    $strictPass = $finalStatusText.Contains("A14_TCP1000_RTU100_STRICT_CONFIRMATION=PASS_STRICT_CONFIRMED")
    Write-Host "STRICT_CONFIRM_RESULT=$(if ($strictPass) { 'PASS' } else { 'FAIL' })"

    if (-not $strictPass) {
        throw "STRICT_CONFIRM_CRITERIA_NOT_MET"
    }

    $finalHead = Get-G2Head
    $finalDirty = @(Get-G2TrackedDirtyPaths)
    $finalStaged = @(& git -C $repo diff --cached --name-only)

    if ($finalHead -ne $head) {
        throw "STRICT_CONFIRM_HEAD_CHANGED"
    }
    if ($finalDirty.Count -ne 0) {
        throw "STRICT_CONFIRM_TREE_DIRTY_AT_END"
    }
    if ($finalStaged.Count -ne 0) {
        throw "STRICT_CONFIRM_INDEX_DIRTY_AT_END"
    }

    @(
        "A14_FINAL_TCP1000_RTU100_GATE=PASS_STRICT_CONFIRMED"
        "INITIAL_HEAD=$head"
        "FINAL_HEAD=$finalHead"
        "RUNNER_EXIT=$runnerExit"
        "TRACKED_DIRTY_FINAL=$($finalDirty.Count)"
        "STAGED_FINAL=$($finalStaged.Count)"
        "RESULT_ROOT=$ResultRoot"
    ) | Set-Content -LiteralPath (Join-Path $ResultRoot "GATE_STATUS.txt") -Encoding UTF8

    Write-Host "A14_FINAL_TCP1000_RTU100_GATE=PASS_STRICT_CONFIRMED"
    Write-Host "RESULT_ROOT=$ResultRoot"
}
catch {
    @(
        "A14_FINAL_TCP1000_RTU100_GATE=FAIL"
        "ERROR=$($_.Exception.Message)"
        "INITIAL_HEAD=$head"
        "RESULT_ROOT=$ResultRoot"
    ) | Set-Content -LiteralPath (Join-Path $ResultRoot "GATE_STATUS.txt") -Encoding UTF8
    throw
}
finally {
    Stop-Transcript | Out-Null
}
