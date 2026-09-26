param(
    [string]$MasterPort = "COM14",
    [string]$SlavePort = "COM4",
    [double]$DurationS = 600.0,
    [double]$BucketSeconds = 60.0
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
    finally {
        $ErrorActionPreference = $previousPreference
    }
}

Write-Host "============================================================"
Write-Host " A14 RTU-H3C1.1 - STRUCTURAL TAIL TIMING DIAGNOSTIC"
Write-Host "============================================================"

Assert-G2Branch

$expectedCoreHash = "4BFF8C8241DA2E8BD0E1BBA99835ADDF91B9085A05C4DFBD05C339B824794566"
$expectedArchiveHash = "444BE3A04079A579252B2737FE6070E00ADCA949FD176880588FE69561B2A79F"
$archiveRelative = "JWPLC/2.1.0/libraries/JWPLC_ModbusRTU/src/esp32/libJWPLC_ModbusRTU.a"
$archivePath = Get-G2Path $archiveRelative
$rs485Header = Get-G2Path "JWPLC/2.1.0/libraries/JWPLC_RS485/src/JWPLC_RS485.h"
$rs485Cpp = Get-G2Path "JWPLC/2.1.0/libraries/JWPLC_RS485/src/JWPLC_RS485.cpp"
$rtuHeader = Get-G2Path "JWPLC/2.1.0/libraries/JWPLC_ModbusRTU/src/JWPLC_ModbusRTU.h"
$rtuCpp = Get-G2Path "JWPLC/2.1.0/libraries/JWPLC_ModbusRTU/src/JWPLC_ModbusRTU.cpp"
$masterSketch = Get-G2Path "tools/modbus-tcp-benchmark/firmware/a14_p5_full_runtime_master/a14_p5_full_runtime_master.ino"
$slaveSketch = Get-G2Path "tools/modbus-tcp-benchmark/firmware/a14_p5_rtu_slave/a14_p5_rtu_slave.ino"
$runner = Get-G2Path "tools/modbus-tcp-benchmark/pc/a14_rtuh3c1_tail_timing_diagnostic.py"
$p5bGate = Join-Path $PSScriptRoot "a14_p5b_physical_master_slave_combined.ps1"

foreach ($required in @($archivePath,$rs485Header,$rs485Cpp,$rtuHeader,$rtuCpp,$masterSketch,$slaveSketch,$runner,$p5bGate)) {
    if (-not (Test-Path -LiteralPath $required)) { throw "RTUH3C111_REQUIRED_PATH_MISSING=$required" }
}
if ($DurationS -lt 600.0) { throw "RTUH3C111_DURATION_MUST_BE_AT_LEAST_600S" }
if ($BucketSeconds -le 0.0) { throw "RTUH3C111_BUCKET_SECONDS_INVALID" }

$dirty = @(Get-G2TrackedDirtyPaths)
$staged = @(& git -C $script:G2RepoRoot diff --cached --name-only)
if ($dirty.Count -ne 1 -or $dirty[0].Replace("\", "/") -ne $script:G2CoreRelative) {
    $dirty | ForEach-Object { Write-Host "DIRTY=$_" }
    throw "RTUH3C111_EXPECTED_ONLY_DIRTY_CORE_A"
}
if ($staged.Count -ne 0) { throw "RTUH3C111_INDEX_NOT_CLEAN" }

$coreHash = Get-G2Sha256 $script:G2CoreRelative
$archiveHashBefore = Get-G2Sha256 $archiveRelative
if ($coreHash -ne $expectedCoreHash) { throw "RTUH3C111_UNEXPECTED_CORE_HASH" }
if ($archiveHashBefore -ne $expectedArchiveHash) { throw "RTUH3C111_UNEXPECTED_ARCHIVE_HASH" }

Write-Host "HEAD=$(Get-G2Head)"
Write-Host "CORE_A_SHA256=$coreHash"
Write-Host "MODBUS_RTU_ARCHIVE_SHA256_BEFORE=$archiveHashBefore"
Write-Host "W5500_SPI_HZ=$(Get-G2SpiHz)"
Write-Host "BAUD=500000"
Write-Host "FRAME_GAP_US=100"
Write-Host "RX_FIFO_FULL=1"
Write-Host "CLOCK=APB_FORCED"
Write-Host "RX_MODE=BULK"
Write-Host "TCP=500"
Write-Host "DURATION_S=$DurationS"
Write-Host "BUCKET_SECONDS=$BucketSeconds"
Write-Host "RTU_STRUCTURAL_FLOOR_HZ=650"
Write-Host "PRODUCT_BULK_RX_DEFAULT_MUTATION=NO"

$rs485HeaderText = [IO.File]::ReadAllText($rs485Header)
$rs485CppText = [IO.File]::ReadAllText($rs485Cpp)
$rtuHeaderText = [IO.File]::ReadAllText($rtuHeader)
$rtuCppText = [IO.File]::ReadAllText($rtuCpp)
$masterText = [IO.File]::ReadAllText($masterSketch)
$slaveText = [IO.File]::ReadAllText($slaveSketch)

$checks = @(
    [PSCustomObject]@{ Label="RS485_BULK_API"; Pass=$rs485HeaderText.Contains("size_t readAvailable(uint8_t *buffer, size_t maxSize)") },
    [PSCustomObject]@{ Label="RS485_BULK_IMPL"; Pass=$rs485CppText.Contains("_serial->read(buffer, toRead)") },
    [PSCustomObject]@{ Label="RTU_BULK_API"; Pass=$rtuHeaderText.Contains("void setBulkRxEnabled(bool enabled)") -and $rtuHeaderText.Contains("bool bulkRxEnabled() const") },
    [PSCustomObject]@{ Label="RTU_BULK_DEFAULT_FALSE"; Pass=$rtuCppText.Contains("_bulkRxEnabled(false)") },
    [PSCustomObject]@{ Label="RTU_BULK_READ_PATH"; Pass=$rtuCppText.Contains("JWPLC_RS485.readAvailable(") },
    [PSCustomObject]@{ Label="MASTER_RX_MODE"; Pass=$masterText.Contains('Serial.print("RTU_RX_MODE=")') -and $masterText.Contains('Serial.println("RTU_RX_MODE=BULK")') -and $masterText.Contains('Serial.println("RTU_RX_MODE=BYTE")') },
    [PSCustomObject]@{ Label="SLAVE_RX_MODE"; Pass=$slaveText.Contains('Serial.print("RTU_RX_MODE=")') -and $slaveText.Contains('Serial.println("RTU_RX_MODE=BULK")') -and $slaveText.Contains('Serial.println("RTU_RX_MODE=BYTE")') },
    [PSCustomObject]@{ Label="H3C1_TXN_LATENCY"; Pass=$masterText.Contains('Serial.print("RTU_TRANSACTION_MAX_US=")') -and $masterText.Contains('Serial.print("RTU_TRANSACTIONS_OVER_20MS=")') },
    [PSCustomObject]@{ Label="H3C1_FAILURE_LATENCY"; Pass=$masterText.Contains('Serial.print("RTU_LAST_FAILURE_DURATION_US=")') -and $masterText.Contains('Serial.print("RTU_LAST_FAILURE_RESULT=")') },
    [PSCustomObject]@{ Label="H3C1_STATS_RX_BYTES"; Pass=$rtuHeaderText.Contains("uint64_t rxBytes;") -and $rtuCppText.Contains("_stats.rxBytes") },
    [PSCustomObject]@{ Label="H3C1_STATS_TX_BYTES"; Pass=$rtuHeaderText.Contains("uint64_t txBytes;") -and $rtuCppText.Contains("_stats.txBytes") },
    [PSCustomObject]@{ Label="H3C1_SERVER_DISCARDS"; Pass=$rtuHeaderText.Contains("serverDiscardedTails") -and $rtuHeaderText.Contains("serverDiscardedBytes") -and $rtuCppText.Contains("_stats.serverDiscardedTails++") },
    [PSCustomObject]@{ Label="H3C1_MASTER_SNAPSHOT"; Pass=$masterText.Contains('Serial.print("RTU_RX_BYTES=")') -and $masterText.Contains('Serial.print("RTU_TX_BYTES=")') },
    [PSCustomObject]@{ Label="H3C1_SLAVE_SNAPSHOT"; Pass=$slaveText.Contains('Serial.print("RTU_SERVER_DISCARDED_TAILS=")') -and $slaveText.Contains('Serial.print("RTU_SERVER_DISCARDED_BYTES=")') },
    [PSCustomObject]@{ Label="H3C1_API"; Pass=$rtuHeaderText.Contains("setEarlyServerDispatchEnabled") -and $rtuHeaderText.Contains("earlyServerDispatchEnabled") },
    [PSCustomObject]@{ Label="H3C1_DEFAULT_FALSE"; Pass=$rtuCppText.Contains("_earlyServerDispatchEnabled(false)") },
    [PSCustomObject]@{ Label="H3C1_PARTIAL_HOLD"; Pass=$rtuCppText.Contains("JWPLC_MODBUS_FAST_PARTIAL_HOLD_US = 1750UL") -and $rtuCppText.Contains("preservePartial") },
    [PSCustomObject]@{ Label="H3C1_EARLY_PARSE"; Pass=$rtuCppText.Contains("if (_rxLength >= expectedLength)") -and $rtuCppText.Contains("parseNow = true") },
    [PSCustomObject]@{ Label="H3C1_MASTER_COMMAND"; Pass=$masterText.Contains('RTU_SERVER_FRAMING=STRUCTURAL') -and $masterText.Contains('RTU_SERVER_FRAMING=GAP') },
    [PSCustomObject]@{ Label="H3C1_SLAVE_COMMAND"; Pass=$slaveText.Contains('RTU_SERVER_FRAMING=STRUCTURAL') -and $slaveText.Contains('RTU_SERVER_FRAMING=GAP') },
    [PSCustomObject]@{ Label="H3C1_TAIL_STATS"; Pass=$rtuHeaderText.Contains("serverDiscardedLastLength") -and $rtuHeaderText.Contains("serverDiscardedMaxAgeUs") -and $rtuHeaderText.Contains("serverDiscardedLen4") },
    [PSCustomObject]@{ Label="H3C1_TAIL_IMPL"; Pass=$rtuCppText.Contains("_stats.serverDiscardedLastAgeUs = frameAgeUs") -and $rtuCppText.Contains("_stats.serverDiscardedLen4++") },
    [PSCustomObject]@{ Label="H3C1_TAIL_SNAPSHOT"; Pass=$slaveText.Contains('RTU_SERVER_DISCARDED_MAX_AGE_US=') -and $slaveText.Contains('RTU_SERVER_DISCARDED_LEN4=') }
)
foreach ($check in $checks) {
    Write-Host "$($check.Label)=$($check.Pass)"
    if (-not $check.Pass) { throw "RTUH3C111_SOURCE_CONTRACT_FAILED_$($check.Label)" }
}
Write-Host "RTUH3C111_SOURCE_CONTRACT=PASS"

$pythonCommand = Get-Command python.exe -ErrorAction SilentlyContinue
if ($null -eq $pythonCommand) { $pythonCommand = Get-Command python -ErrorAction SilentlyContinue }
if ($null -eq $pythonCommand) { throw "RTUH3C111_PYTHON_NOT_FOUND" }
$pythonExe = $pythonCommand.Source
& $pythonExe -m py_compile $runner
if ($LASTEXITCODE -ne 0) { throw "RTUH3C111_PYTHON_SYNTAX_FAILED" }
Write-Host "RTUH3C111_PYTHON_SYNTAX=PASS"

$tempRoot = Join-Path $env:TEMP ("jwplc_a14_rtuh3c1_tailtiming_{0}" -f (Get-Date -Format "yyyyMMdd_HHmmss"))
$archiveBackup = Join-Path $tempRoot "libJWPLC_ModbusRTU.before.a"
$setupLog = Join-Path $tempRoot "setup_source.log"
$runLog = Join-Path $tempRoot "rtuh3c1_tailtiming.log"
New-Item -ItemType Directory -Force -Path $tempRoot | Out-Null
Copy-Item -LiteralPath $archivePath -Destination $archiveBackup -Force

$archiveHidden = $false
$dutIp = $null
try {
    Remove-Item -LiteralPath $archivePath -Force
    $archiveHidden = $true
    Write-Host ""
    Write-Host "=== FORCE FRESH SOURCE COMPILE / UPLOAD H3C1 ==="

    $setupArgs = @(
        "-NoLogo","-NoProfile","-ExecutionPolicy","Bypass",
        "-File",$p5bGate,
        "-MasterPort",$MasterPort,
        "-SlavePort",$SlavePort,
        "-SetupOnly",
        "-AllowDirtyCoreCandidate",
        "-AllowMissingModbusRtuArchiveCandidate"
    )

    $setupExit = Invoke-NativeToLog -FilePath "powershell.exe" -Arguments $setupArgs -LogPath $setupLog
    Write-Host "RTUH3C111_SETUP_EXIT=$setupExit"
    Get-Content -LiteralPath $setupLog | ForEach-Object { Write-Host $_ }
    if ($setupExit -ne 0) { throw "RTUH3C111_SETUP_FAILED" }

    $setupText = [IO.File]::ReadAllText($setupLog)
    $tempRootMatch = [regex]::Match($setupText, "(?m)^TEMP_ROOT=(.+?)\r?$")
    $ipMatch = [regex]::Match($setupText, "(?m)^P5B_SETUP_ONLY_MASTER_IP=(.+?)\r?$")
    if (-not $tempRootMatch.Success -or -not $ipMatch.Success) { throw "RTUH3C111_SETUP_METADATA_MISSING" }

    $p5bTempRoot = $tempRootMatch.Groups[1].Value.Trim()
    $dutIp = $ipMatch.Groups[1].Value.Trim()

    foreach ($buildName in @("build_master","build_slave")) {
        $buildPath = Join-Path $p5bTempRoot $buildName
        foreach ($objectPattern in @("JWPLC_ModbusRTU.cpp.o*","JWPLC_RS485.cpp.o*")) {
            $objects = @(Get-ChildItem -LiteralPath $buildPath -Recurse -File | Where-Object { $_.Name -like $objectPattern })
            if ($objects.Count -lt 1) { throw ("RTUH3C111_SOURCE_OBJECT_MISSING={0}:{1}" -f $buildPath,$objectPattern) }
            Write-Host ("RTUH3C111_SOURCE_OBJECT={0}" -f $objects[0].FullName)
        }
    }
    Write-Host "RTUH3C111_FRESH_SOURCE_COMPILE=PASS"
}
finally {
    if ($archiveHidden) { Copy-Item -LiteralPath $archiveBackup -Destination $archivePath -Force }
}

$archiveHashAfter = Get-G2Sha256 $archiveRelative
Write-Host "MODBUS_RTU_ARCHIVE_SHA256_AFTER=$archiveHashAfter"
if ($archiveHashAfter -ne $archiveHashBefore) { throw "RTUH3C111_ARCHIVE_RESTORE_HASH_MISMATCH" }

$dirtyAfterRestore = @(Get-G2TrackedDirtyPaths)
if ($dirtyAfterRestore.Count -ne 1 -or $dirtyAfterRestore[0].Replace("\", "/") -ne $script:G2CoreRelative) {
    $dirtyAfterRestore | ForEach-Object { Write-Host "DIRTY=$_" }
    throw "RTUH3C111_DIRTY_SCOPE_AFTER_RESTORE_INVALID"
}

Write-Host ""
Write-Host "=== PHYSICAL RTU-H3C1 TAIL TIMING DIAGNOSTIC ==="
$durationText = $DurationS.ToString([Globalization.CultureInfo]::InvariantCulture)
$bucketText = $BucketSeconds.ToString([Globalization.CultureInfo]::InvariantCulture)
$previousPreference = $ErrorActionPreference
try {
    $ErrorActionPreference = "Continue"
    & $pythonExe -u $runner --master-serial $MasterPort --slave-serial $SlavePort --host $dutIp --port 502 --duration $durationText --bucket-seconds $bucketText 2>&1 | Tee-Object -FilePath $runLog
    $runExit = [int]$LASTEXITCODE
}
finally {
    $ErrorActionPreference = $previousPreference
}

Write-Host "RTUH3C111_RUNNER_EXIT=$runExit"
Write-Host "RTUH3C111_RUNNER_LOG=$runLog"
if ($runExit -ne 0) { throw "RTUH3C111_RUN_FAILED" }

Write-Host ""
Write-Host "============================================================"
Write-Host " PHYSICAL TFT OBSERVATION"
Write-Host "============================================================"
$masterAnswer = Read-Host "MASTER COM14 estable y sin parpadeo/cortes visibles? (S/N)"
$slaveAnswer = Read-Host "SLAVE COM4 estable y sin parpadeo/cortes visibles? (S/N)"
$masterPhysical = $masterAnswer.Trim().ToUpper() -eq "S"
$slavePhysical = $slaveAnswer.Trim().ToUpper() -eq "S"
Write-Host "MASTER_TFT_PHYSICAL_PASS=$masterPhysical"
Write-Host "SLAVE_TFT_PHYSICAL_PASS=$slavePhysical"
if (-not ($masterPhysical -and $slavePhysical)) { throw "RTUH3C111_TFT_PHYSICAL_REVIEW" }

$finalCoreHash = Get-G2Sha256 $script:G2CoreRelative
$finalArchiveHash = Get-G2Sha256 $archiveRelative
$finalDirty = @(Get-G2TrackedDirtyPaths)
$finalStaged = @(& git -C $script:G2RepoRoot diff --cached --name-only)

Write-Host ""
Write-Host "=== FINAL CONTROLLED STATE ==="
Write-Host "CORE_A_SHA256=$finalCoreHash"
Write-Host "MODBUS_RTU_ARCHIVE_SHA256=$finalArchiveHash"
Write-Host "TRACKED_DIRTY_FINAL=$($finalDirty.Count)"
Write-Host "STAGED_COUNT_FINAL=$($finalStaged.Count)"

if ($finalCoreHash -ne $expectedCoreHash) { throw "RTUH3C111_CORE_HASH_CHANGED" }
if ($finalArchiveHash -ne $archiveHashBefore) { throw "RTUH3C111_ARCHIVE_CHANGED" }
if ($finalDirty.Count -ne 1 -or $finalDirty[0].Replace("\", "/") -ne $script:G2CoreRelative) { throw "RTUH3C111_FINAL_DIRTY_SCOPE_INVALID" }
if ($finalStaged.Count -ne 0) { throw "RTUH3C111_FINAL_INDEX_NOT_CLEAN" }

Write-Host "A14_RTU_H3C1_TAIL_TIMING_GATE=PASS"
Write-Host "NEXT=RETURN_OUTPUT_TO_CHAT_FOR_TAIL_TIMING_DECISION"
