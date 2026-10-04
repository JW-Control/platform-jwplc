param(
    [string]$MasterPort = "COM14",
    [string]$SlavePort = "COM4",
    [double]$CaseDurationS = 300.0,
    [double]$BucketSeconds = 60.0
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"
. (Join-Path $PSScriptRoot "common.ps1")

Write-Host "============================================================"
Write-Host " A14 RTU-H3C.6 - FC03 RESPONSE-LENGTH MATRIX"
Write-Host "============================================================"

Assert-G2Branch

if ($CaseDurationS -lt 300.0) { throw "RTUH3C6_CASE_DURATION_MUST_BE_AT_LEAST_300S" }
if ($BucketSeconds -le 0.0) { throw "RTUH3C6_BUCKET_SECONDS_INVALID" }

$expectedCoreHash = "4BFF8C8241DA2E8BD0E1BBA99835ADDF91B9085A05C4DFBD05C339B824794566"
$expectedArchiveHash = "444BE3A04079A579252B2737FE6070E00ADCA949FD176880588FE69561B2A79F"
$archiveRelative = "JWPLC/2.1.0/libraries/JWPLC_ModbusRTU/src/esp32/libJWPLC_ModbusRTU.a"
$archivePath = Get-G2Path $archiveRelative
$masterSketch = Get-G2Path "tools/modbus-tcp-benchmark/firmware/a14_p5_full_runtime_master/a14_p5_full_runtime_master.ino"
$runner = Get-G2Path "tools/modbus-tcp-benchmark/pc/a14_rtuh3c6_fc03_response_length_case.py"
$p5bGate = Join-Path $PSScriptRoot "a14_p5b_physical_master_slave_combined.ps1"

foreach ($required in @($archivePath,$masterSketch,$runner,$p5bGate)) {
    if (-not (Test-Path -LiteralPath $required)) { throw "RTUH3C6_REQUIRED_PATH_MISSING=$required" }
}

$dirty = @(Get-G2TrackedDirtyPaths)
$staged = @(& git -C $script:G2RepoRoot diff --cached --name-only)
if ($dirty.Count -ne 1 -or $dirty[0].Replace("\", "/") -ne $script:G2CoreRelative) {
    throw "RTUH3C6_EXPECTED_ONLY_DIRTY_CORE_A"
}
if ($staged.Count -ne 0) { throw "RTUH3C6_INDEX_NOT_CLEAN" }

$coreHash = Get-G2Sha256 $script:G2CoreRelative
$archiveHashBefore = Get-G2Sha256 $archiveRelative
if ($coreHash -ne $expectedCoreHash) { throw "RTUH3C6_UNEXPECTED_CORE_HASH" }
if ($archiveHashBefore -ne $expectedArchiveHash) { throw "RTUH3C6_UNEXPECTED_ARCHIVE_HASH" }

$masterText = [IO.File]::ReadAllText($masterSketch)
if (
    -not $masterText.Contains('RTU_RX_FIFO_FULL=9') -or
    -not $masterText.Contains('RTU_READ_PROFILE=Q1') -or
    -not $masterText.Contains('RTU_READ_PROFILE=Q2') -or
    -not $masterText.Contains('RTU_READ_PROFILE=Q4') -or
    -not $masterText.Contains('RTU_READ_PROFILE=Q8') -or
    -not $masterText.Contains('RTU_READ_QUANTITY=') -or
    -not $masterText.Contains('RTU_EXPECTED_RESPONSE_BYTES=')
) {
    throw "RTUH3C6_SOURCE_CONTRACT_FAILED"
}

$pythonCommand = Get-Command python.exe -ErrorAction SilentlyContinue
if ($null -eq $pythonCommand) { $pythonCommand = Get-Command python -ErrorAction SilentlyContinue }
if ($null -eq $pythonCommand) { throw "RTUH3C6_PYTHON_NOT_FOUND" }
$pythonExe = $pythonCommand.Source
& $pythonExe -m py_compile $runner
if ($LASTEXITCODE -ne 0) { throw "RTUH3C6_PYTHON_SYNTAX_FAILED" }

Write-Host "HEAD=$(Get-G2Head)"
Write-Host "MASTER_RX_FIFO_FULL=9"
Write-Host "SLAVE_RX_FIFO_FULL=8"
Write-Host "MATRIX_QUANTITIES=1,2,4,8"
Write-Host "CASE_DURATION_S=$CaseDurationS"
Write-Host "TCP=500"
Write-Host "RTUH3C6_PREFLIGHT=PASS"

$tempRoot = Join-Path $env:TEMP ("jwplc_a14_rtuh3c6_matrix_{0}" -f (Get-Date -Format "yyyyMMdd_HHmmss"))
New-Item -ItemType Directory -Force -Path $tempRoot | Out-Null
$archiveBackup = Join-Path $tempRoot "libJWPLC_ModbusRTU.before.a"
$setupLog = Join-Path $tempRoot "setup_source.log"
Copy-Item -LiteralPath $archivePath -Destination $archiveBackup -Force

$archiveHidden = $false
$dutIp = $null
try {
    Remove-Item -LiteralPath $archivePath -Force
    $archiveHidden = $true

    Write-Host ""
    Write-Host "=== FORCE FRESH SOURCE COMPILE / UPLOAD H3C6 ==="

    & $p5bGate -MasterPort $MasterPort -SlavePort $SlavePort -SetupOnly -AllowDirtyCoreCandidate -AllowMissingModbusRtuArchiveCandidate *>&1 |
        Tee-Object -FilePath $setupLog

    $setupText = [IO.File]::ReadAllText($setupLog)
    $ipMatch = [regex]::Match($setupText, "(?m)^P5B_SETUP_ONLY_MASTER_IP=(.+?)\r?$")
    if (-not $ipMatch.Success) { throw "RTUH3C6_SETUP_IP_MISSING" }
    $dutIp = $ipMatch.Groups[1].Value.Trim()
}
finally {
    if ($archiveHidden) {
        Copy-Item -LiteralPath $archiveBackup -Destination $archivePath -Force
    }
}

if ((Get-G2Sha256 $archiveRelative) -ne $archiveHashBefore) {
    throw "RTUH3C6_ARCHIVE_RESTORE_HASH_MISMATCH"
}

$durationText = $CaseDurationS.ToString([Globalization.CultureInfo]::InvariantCulture)
$bucketText = $BucketSeconds.ToString([Globalization.CultureInfo]::InvariantCulture)

foreach ($quantity in @(1,2,4,8)) {
    Write-Host ""
    Write-Host "=== H3C6 CASE Q$quantity ==="
    $caseLog = Join-Path $tempRoot ("rtuh3c6_q{0}.log" -f $quantity)

    $previousPreference = $ErrorActionPreference
    try {
        $ErrorActionPreference = "Continue"
        & $pythonExe -u $runner --master-serial $MasterPort --slave-serial $SlavePort --host $dutIp --port 502 --duration $durationText --bucket-seconds $bucketText --rtu-quantity $quantity 2>&1 |
            Tee-Object -FilePath $caseLog
        $runExit = [int]$LASTEXITCODE
    }
    finally {
        $ErrorActionPreference = $previousPreference
    }

    Write-Host "RTUH3C6_Q$($quantity)_RUNNER_EXIT=$runExit"
    Write-Host "RTUH3C6_Q$($quantity)_RUNNER_LOG=$caseLog"
    if ($runExit -ne 0) { throw "RTUH3C6_Q$($quantity)_RUN_FAILED" }
}

Write-Host "RTUH3C6_MATRIX_RUNTIME=PASS"

$masterAnswer = Read-Host "MASTER COM14 estable y sin parpadeo/cortes visibles? (S/N)"
$slaveAnswer = Read-Host "SLAVE COM4 estable y sin parpadeo/cortes visibles? (S/N)"
if ($masterAnswer.Trim().ToUpper() -ne "S" -or $slaveAnswer.Trim().ToUpper() -ne "S") {
    throw "RTUH3C6_TFT_PHYSICAL_REVIEW"
}

$finalCoreHash = Get-G2Sha256 $script:G2CoreRelative
$finalArchiveHash = Get-G2Sha256 $archiveRelative
$finalDirty = @(Get-G2TrackedDirtyPaths)
$finalStaged = @(& git -C $script:G2RepoRoot diff --cached --name-only)

if ($finalCoreHash -ne $expectedCoreHash) { throw "RTUH3C6_CORE_HASH_CHANGED" }
if ($finalArchiveHash -ne $archiveHashBefore) { throw "RTUH3C6_ARCHIVE_CHANGED" }
if ($finalDirty.Count -ne 1 -or $finalDirty[0].Replace("\", "/") -ne $script:G2CoreRelative) { throw "RTUH3C6_FINAL_DIRTY_SCOPE_INVALID" }
if ($finalStaged.Count -ne 0) { throw "RTUH3C6_FINAL_INDEX_NOT_CLEAN" }

Write-Host "A14_RTU_H3C6_FC03_LENGTH_MATRIX_GATE=PASS"
Write-Host "NEXT=RETURN_OUTPUT_TO_CHAT_FOR_H3C_GENERALIZATION_DECISION"
