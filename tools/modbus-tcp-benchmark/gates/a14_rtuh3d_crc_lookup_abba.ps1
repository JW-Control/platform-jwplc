param(
    [string]$MasterPort = "COM14",
    [string]$SlavePort = "COM4",
    [double]$CaseDurationS = 180.0,
    [double]$BucketSeconds = 60.0
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"
. (Join-Path $PSScriptRoot "common.ps1")

Write-Host "============================================================"
Write-Host " A14 RTU-H3D - CRC LOOKUP ABBA"
Write-Host "============================================================"

Assert-G2Branch

if ($CaseDurationS -lt 180.0) { throw "RTUH3D_CASE_DURATION_MUST_BE_AT_LEAST_180S" }
if ($BucketSeconds -le 0.0) { throw "RTUH3D_BUCKET_SECONDS_INVALID" }

$expectedCoreHash = "4BFF8C8241DA2E8BD0E1BBA99835ADDF91B9085A05C4DFBD05C339B824794566"
$expectedArchiveHash = "444BE3A04079A579252B2737FE6070E00ADCA949FD176880588FE69561B2A79F"
$archiveRelative = "JWPLC/2.1.0/libraries/JWPLC_ModbusRTU/src/esp32/libJWPLC_ModbusRTU.a"
$archivePath = Get-G2Path $archiveRelative
$rtuHeader = Get-G2Path "JWPLC/2.1.0/libraries/JWPLC_ModbusRTU/src/JWPLC_ModbusRTU.h"
$rtuCpp = Get-G2Path "JWPLC/2.1.0/libraries/JWPLC_ModbusRTU/src/JWPLC_ModbusRTU.cpp"
$masterSketch = Get-G2Path "tools/modbus-tcp-benchmark/firmware/a14_p5_full_runtime_master/a14_p5_full_runtime_master.ino"
$slaveSketch = Get-G2Path "tools/modbus-tcp-benchmark/firmware/a14_p5_rtu_slave/a14_p5_rtu_slave.ino"
$runner = Get-G2Path "tools/modbus-tcp-benchmark/pc/a14_rtuh3d_crc_lookup_case.py"
$p5bGate = Join-Path $PSScriptRoot "a14_p5b_physical_master_slave_combined.ps1"

foreach ($required in @($archivePath,$rtuHeader,$rtuCpp,$masterSketch,$slaveSketch,$runner,$p5bGate)) {
    if (-not (Test-Path -LiteralPath $required)) { throw "RTUH3D_REQUIRED_PATH_MISSING=$required" }
}

$dirty = @(Get-G2TrackedDirtyPaths)
$staged = @(& git -C $script:G2RepoRoot diff --cached --name-only)
if ($dirty.Count -ne 1 -or $dirty[0].Replace("\", "/") -ne $script:G2CoreRelative) {
    $dirty | ForEach-Object { Write-Host "DIRTY=$_" }
    throw "RTUH3D_EXPECTED_ONLY_DIRTY_CORE_A"
}
if ($staged.Count -ne 0) { throw "RTUH3D_INDEX_NOT_CLEAN" }

$coreHash = Get-G2Sha256 $script:G2CoreRelative
$archiveHashBefore = Get-G2Sha256 $archiveRelative
if ($coreHash -ne $expectedCoreHash) { throw "RTUH3D_UNEXPECTED_CORE_HASH" }
if ($archiveHashBefore -ne $expectedArchiveHash) { throw "RTUH3D_UNEXPECTED_ARCHIVE_HASH" }

$headerText = [IO.File]::ReadAllText($rtuHeader)
$cppText = [IO.File]::ReadAllText($rtuCpp)
$masterText = [IO.File]::ReadAllText($masterSketch)
$slaveText = [IO.File]::ReadAllText($slaveSketch)

$checks = @(
    [PSCustomObject]@{ Label="H3D_SELECTOR_API"; Pass=$headerText.Contains("setCrcLookupEnabled") -and $headerText.Contains("crcLookupEnabled") },
    [PSCustomObject]@{ Label="H3D_DEFAULT_BITWISE"; Pass=$cppText.Contains("_crcLookupEnabled = false") },
    [PSCustomObject]@{ Label="H3D_TABLE_256"; Pass=$cppText.Contains("JWPLC_MODBUS_CRC16_TABLE[256]") },
    [PSCustomObject]@{ Label="H3D_LOOKUP_IMPL"; Pass=$cppText.Contains("crc16Lookup") -and $cppText.Contains("JWPLC_MODBUS_CRC16_TABLE[index]") },
    [PSCustomObject]@{ Label="H3D_BITWISE_IMPL"; Pass=$cppText.Contains("crc16Bitwise") -and $cppText.Contains("0xA001") },
    [PSCustomObject]@{ Label="H3D_MASTER_MODE"; Pass=$masterText.Contains('RTU_CRC_MODE=BITWISE') -and $masterText.Contains('RTU_CRC_MODE=LOOKUP') },
    [PSCustomObject]@{ Label="H3D_SLAVE_MODE"; Pass=$slaveText.Contains('RTU_CRC_MODE=BITWISE') -and $slaveText.Contains('RTU_CRC_MODE=LOOKUP') },
    [PSCustomObject]@{ Label="H3D_MASTER_SELFTEST"; Pass=$masterText.Contains('RTU_CRC_SELFTEST=') -and $masterText.Contains("0x4B37U") },
    [PSCustomObject]@{ Label="H3D_SLAVE_SELFTEST"; Pass=$slaveText.Contains('RTU_CRC_SELFTEST=') -and $slaveText.Contains("0x4B37U") }
)
foreach ($check in $checks) {
    Write-Host "$($check.Label)=$($check.Pass)"
    if (-not $check.Pass) { throw "RTUH3D_SOURCE_CONTRACT_FAILED_$($check.Label)" }
}
Write-Host "RTUH3D_SOURCE_CONTRACT=PASS"

$pythonCommand = Get-Command python.exe -ErrorAction SilentlyContinue
if ($null -eq $pythonCommand) { $pythonCommand = Get-Command python -ErrorAction SilentlyContinue }
if ($null -eq $pythonCommand) { throw "RTUH3D_PYTHON_NOT_FOUND" }
$pythonExe = $pythonCommand.Source
& $pythonExe -m py_compile $runner
if ($LASTEXITCODE -ne 0) { throw "RTUH3D_PYTHON_SYNTAX_FAILED" }
Write-Host "RTUH3D_PYTHON_SYNTAX=PASS"

Write-Host "HEAD=$(Get-G2Head)"
Write-Host "MASTER_RX_FIFO_FULL=9"
Write-Host "SLAVE_RX_FIFO_FULL=8"
Write-Host "CRC_SEQUENCE=BITWISE,LOOKUP,LOOKUP,BITWISE"
Write-Host "CASE_DURATION_S=$CaseDurationS"
Write-Host "TCP=500"
Write-Host "RTUH3D_PREFLIGHT=PASS"

$tempRoot = Join-Path $env:TEMP ("jwplc_a14_rtuh3d_crc_abba_{0}" -f (Get-Date -Format "yyyyMMdd_HHmmss"))
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
    Write-Host "=== FORCE FRESH SOURCE COMPILE / UPLOAD H3D ==="

    & $p5bGate -MasterPort $MasterPort -SlavePort $SlavePort -SetupOnly -AllowDirtyCoreCandidate -AllowMissingModbusRtuArchiveCandidate *>&1 |
        Tee-Object -FilePath $setupLog

    $setupText = [IO.File]::ReadAllText($setupLog)
    $ipMatch = [regex]::Match($setupText, "(?m)^P5B_SETUP_ONLY_MASTER_IP=(.+?)\r?$")
    if (-not $ipMatch.Success) { throw "RTUH3D_SETUP_IP_MISSING" }
    $dutIp = $ipMatch.Groups[1].Value.Trim()
}
finally {
    if ($archiveHidden) {
        Copy-Item -LiteralPath $archiveBackup -Destination $archivePath -Force
    }
}

if ((Get-G2Sha256 $archiveRelative) -ne $archiveHashBefore) {
    throw "RTUH3D_ARCHIVE_RESTORE_HASH_MISMATCH"
}

$durationText = $CaseDurationS.ToString([Globalization.CultureInfo]::InvariantCulture)
$bucketText = $BucketSeconds.ToString([Globalization.CultureInfo]::InvariantCulture)
$sequence = @("BITWISE","LOOKUP","LOOKUP","BITWISE")
$bitwiseHz = New-Object System.Collections.Generic.List[double]
$lookupHz = New-Object System.Collections.Generic.List[double]

for ($index = 0; $index -lt $sequence.Count; $index++) {
    $mode = $sequence[$index]
    $runNumber = $index + 1
    Write-Host ""
    Write-Host "=== H3D RUN $runNumber MODE=$mode ==="

    $runLog = Join-Path $tempRoot ("rtuh3d_{0}_{1}.log" -f $runNumber,$mode.ToLowerInvariant())

    $previousPreference = $ErrorActionPreference
    try {
        $ErrorActionPreference = "Continue"
        & $pythonExe -u $runner --master-serial $MasterPort --slave-serial $SlavePort --host $dutIp --port 502 --duration $durationText --bucket-seconds $bucketText --crc-mode $mode 2>&1 |
            Tee-Object -FilePath $runLog
        $runExit = [int]$LASTEXITCODE
    }
    finally {
        $ErrorActionPreference = $previousPreference
    }

    Write-Host "RTUH3D_RUN$($runNumber)_EXIT=$runExit"
    Write-Host "RTUH3D_RUN$($runNumber)_LOG=$runLog"
    if ($runExit -ne 0) { throw "RTUH3D_RUN$($runNumber)_$($mode)_FAILED" }

    $runText = [IO.File]::ReadAllText($runLog)
    $hzMatch = [regex]::Match($runText, "(?m)^RTUH3D_RTU_HZ=([0-9.]+)\r?$")
    if (-not $hzMatch.Success) { throw "RTUH3D_RUN$($runNumber)_HZ_MISSING" }

    $hz = [double]::Parse(
        $hzMatch.Groups[1].Value,
        [Globalization.CultureInfo]::InvariantCulture)

    Write-Host ("RTUH3D_RUN{0}_MODE={1}" -f $runNumber,$mode)
    Write-Host ("RTUH3D_RUN{0}_RTU_HZ={1:F3}" -f $runNumber,$hz)

    if ($mode -eq "BITWISE") {
        $bitwiseHz.Add($hz)
    }
    else {
        $lookupHz.Add($hz)
    }
}

$bitwiseAvg = ($bitwiseHz | Measure-Object -Average).Average
$lookupAvg = ($lookupHz | Measure-Object -Average).Average
$gainPct = if ($bitwiseAvg -gt 0.0) {
    (($lookupAvg - $bitwiseAvg) / $bitwiseAvg) * 100.0
} else {
    0.0
}

Write-Host ""
Write-Host "=== H3D ABBA SUMMARY ==="
Write-Host ("RTUH3D_BITWISE_AVG_HZ={0:F3}" -f $bitwiseAvg)
Write-Host ("RTUH3D_LOOKUP_AVG_HZ={0:F3}" -f $lookupAvg)
Write-Host ("RTUH3D_LOOKUP_GAIN_PCT={0:F3}" -f $gainPct)
Write-Host "RTUH3D_ABBA_RUNTIME=PASS"

$masterAnswer = Read-Host "MASTER COM14 estable y sin parpadeo/cortes visibles? (S/N)"
$slaveAnswer = Read-Host "SLAVE COM4 estable y sin parpadeo/cortes visibles? (S/N)"
if ($masterAnswer.Trim().ToUpper() -ne "S" -or $slaveAnswer.Trim().ToUpper() -ne "S") {
    throw "RTUH3D_TFT_PHYSICAL_REVIEW"
}

$finalCoreHash = Get-G2Sha256 $script:G2CoreRelative
$finalArchiveHash = Get-G2Sha256 $archiveRelative
$finalDirty = @(Get-G2TrackedDirtyPaths)
$finalStaged = @(& git -C $script:G2RepoRoot diff --cached --name-only)

if ($finalCoreHash -ne $expectedCoreHash) { throw "RTUH3D_CORE_HASH_CHANGED" }
if ($finalArchiveHash -ne $archiveHashBefore) { throw "RTUH3D_ARCHIVE_CHANGED" }
if ($finalDirty.Count -ne 1 -or $finalDirty[0].Replace("\", "/") -ne $script:G2CoreRelative) { throw "RTUH3D_FINAL_DIRTY_SCOPE_INVALID" }
if ($finalStaged.Count -ne 0) { throw "RTUH3D_FINAL_INDEX_NOT_CLEAN" }

Write-Host "A14_RTU_H3D_CRC_LOOKUP_ABBA_GATE=PASS"
Write-Host "NEXT=RETURN_OUTPUT_TO_CHAT_FOR_CRC_LOOKUP_DECISION"
