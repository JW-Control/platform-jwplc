param(
 [string]$MasterPort="COM14",
 [string]$SlavePort="COM4",
 [double]$DurationPerCaseS=60.0
)
Set-StrictMode -Version Latest
$ErrorActionPreference="Stop"
. (Join-Path $PSScriptRoot "common.ps1")

function Invoke-NativeToLog {
 param([string]$FilePath,[string[]]$Arguments,[string]$LogPath)
 $old=$ErrorActionPreference
 try { $ErrorActionPreference="Continue"; & $FilePath @Arguments *> $LogPath; return [int]$LASTEXITCODE }
 finally { $ErrorActionPreference=$old }
}

Write-Host "============================================================"
Write-Host " A14 RTU-H3A - RX FIFO THRESHOLD SWEEP"
Write-Host "============================================================"
Assert-G2Branch

$expectedCoreHash="4BFF8C8241DA2E8BD0E1BBA99835ADDF91B9085A05C4DFBD05C339B824794566"
$archiveRelative="JWPLC/2.1.0/libraries/JWPLC_ModbusRTU/src/esp32/libJWPLC_ModbusRTU.a"
$archivePath=Get-G2Path $archiveRelative
$masterSketch=Get-G2Path "tools/modbus-tcp-benchmark/firmware/a14_p5_full_runtime_master/a14_p5_full_runtime_master.ino"
$slaveSketch=Get-G2Path "tools/modbus-tcp-benchmark/firmware/a14_p5_rtu_slave/a14_p5_rtu_slave.ino"
$runner=Get-G2Path "tools/modbus-tcp-benchmark/pc/a14_rtuh3a_rx_fifo_sweep.py"
$p5bGate=Join-Path $PSScriptRoot "a14_p5b_physical_master_slave_combined.ps1"

foreach($p in @($archivePath,$masterSketch,$slaveSketch,$runner,$p5bGate)){if(-not(Test-Path -LiteralPath $p)){throw "RTUH3A_REQUIRED_PATH_MISSING=$p"}}
if($DurationPerCaseS -lt 30){throw "RTUH3A_DURATION_TOO_SHORT"}

$dirty=@(Get-G2TrackedDirtyPaths)
$staged=@(& git -C $script:G2RepoRoot diff --cached --name-only)
if($dirty.Count -ne 1 -or $dirty[0].Replace("\","/") -ne $script:G2CoreRelative){throw "RTUH3A_EXPECTED_ONLY_DIRTY_CORE_A"}
if($staged.Count -ne 0){throw "RTUH3A_INDEX_NOT_CLEAN"}

$coreHash=Get-G2Sha256 $script:G2CoreRelative
$archiveHashBefore=Get-G2Sha256 $archiveRelative
if($coreHash -ne $expectedCoreHash){throw "RTUH3A_UNEXPECTED_CORE_HASH"}

Write-Host "HEAD=$(Get-G2Head)"
Write-Host "CORE_A_SHA256=$coreHash"
Write-Host "MODBUS_RTU_ARCHIVE_SHA256_BEFORE=$archiveHashBefore"
Write-Host "W5500_SPI_HZ=$(Get-G2SpiHz)"
Write-Host "BAUD=500000"
Write-Host "FRAME_GAP_US=100"
Write-Host "CLOCK=APB_FORCED"
Write-Host "FIFO_BYTES=120,32,16,8,1"
Write-Host "TCP_CASES=500,OFF"
Write-Host "DURATION_PER_CASE_S=$DurationPerCaseS"
Write-Host "PRODUCT_FIFO_DEFAULT_MUTATION=NO"

foreach($pair in @(@("MASTER",[IO.File]::ReadAllText($masterSketch)),@("SLAVE",[IO.File]::ReadAllText($slaveSketch)))){
 foreach($needle in @("RTU_RX_FIFO_FULL=","setRxFIFOFull(fifoBytes)","setRtuRxFifoFull(120U)","setRtuRxFifoFull(1U)")){
  if(-not $pair[1].Contains($needle)){throw "RTUH3A_SOURCE_CONTRACT_MISSING=$($pair[0]):$needle"}
 }
}

$py=Get-Command python.exe -ErrorAction SilentlyContinue
if($null -eq $py){$py=Get-Command python -ErrorAction SilentlyContinue}
if($null -eq $py){throw "RTUH3A_PYTHON_NOT_FOUND"}
$pythonExe=$py.Source
& $pythonExe -m py_compile $runner
if($LASTEXITCODE -ne 0){throw "RTUH3A_PYTHON_SYNTAX_FAILED"}
Write-Host "RTUH3A_PYTHON_SYNTAX=PASS"

$tempRoot=Join-Path $env:TEMP ("jwplc_a14_rtuh3a_{0}" -f (Get-Date -Format "yyyyMMdd_HHmmss"))
$backup=Join-Path $tempRoot "libJWPLC_ModbusRTU.before.a"
$setupLog=Join-Path $tempRoot "setup_source.log"
$runLog=Join-Path $tempRoot "rtuh3a.log"
New-Item -ItemType Directory -Force -Path $tempRoot|Out-Null
Copy-Item -LiteralPath $archivePath -Destination $backup -Force

$hidden=$false
try{
 Remove-Item -LiteralPath $archivePath -Force
 $hidden=$true
 Write-Host "=== FORCE FRESH SOURCE COMPILE / UPLOAD H3A ==="
 $args=@("-NoLogo","-NoProfile","-ExecutionPolicy","Bypass","-File",$p5bGate,"-MasterPort",$MasterPort,"-SlavePort",$SlavePort,"-SetupOnly","-AllowDirtyCoreCandidate","-AllowMissingModbusRtuArchiveCandidate")
 $exit=Invoke-NativeToLog -FilePath "powershell.exe" -Arguments $args -LogPath $setupLog
 Write-Host "RTUH3A_SETUP_EXIT=$exit"
 Get-Content -LiteralPath $setupLog|ForEach-Object{Write-Host $_}
 if($exit -ne 0){throw "RTUH3A_SETUP_FAILED"}
 $txt=[IO.File]::ReadAllText($setupLog)
 $root=[regex]::Match($txt,"(?m)^TEMP_ROOT=(.+?)\r?$")
 $ip=[regex]::Match($txt,"(?m)^P5B_SETUP_ONLY_MASTER_IP=(.+?)\r?$")
 if(-not $root.Success -or -not $ip.Success){throw "RTUH3A_SETUP_METADATA_MISSING"}
 $p5root=$root.Groups[1].Value.Trim()
 $dutIp=$ip.Groups[1].Value.Trim()
 foreach($build in @("build_master","build_slave")){
  $bp=Join-Path $p5root $build
  foreach($pat in @("JWPLC_ModbusRTU.cpp.o*","JWPLC_RS485.cpp.o*")){
   $objs=@(Get-ChildItem -LiteralPath $bp -Recurse -File|Where-Object{$_.Name -like $pat})
   if($objs.Count -lt 1){throw "RTUH3A_SOURCE_OBJECT_MISSING=$bp:$pat"}
   Write-Host "RTUH3A_SOURCE_OBJECT=$($objs[0].FullName)"
  }
 }
 Write-Host "RTUH3A_FRESH_SOURCE_COMPILE=PASS"
}
finally{if($hidden){Copy-Item -LiteralPath $backup -Destination $archivePath -Force}}

$archiveHashAfter=Get-G2Sha256 $archiveRelative
Write-Host "MODBUS_RTU_ARCHIVE_SHA256_AFTER=$archiveHashAfter"
if($archiveHashAfter -ne $archiveHashBefore){throw "RTUH3A_ARCHIVE_RESTORE_HASH_MISMATCH"}

$dirty=@(Get-G2TrackedDirtyPaths)
if($dirty.Count -ne 1 -or $dirty[0].Replace("\","/") -ne $script:G2CoreRelative){throw "RTUH3A_DIRTY_SCOPE_AFTER_RESTORE_INVALID"}

Write-Host "=== PHYSICAL RTU-H3A SWEEP ==="
$duration=$DurationPerCaseS.ToString([Globalization.CultureInfo]::InvariantCulture)
$old=$ErrorActionPreference
try{
 $ErrorActionPreference="Continue"
 & $pythonExe -u $runner --master-serial $MasterPort --slave-serial $SlavePort --host $dutIp --port 502 --duration-per-case $duration 2>&1|Tee-Object -FilePath $runLog
 $runExit=[int]$LASTEXITCODE
}finally{$ErrorActionPreference=$old}
Write-Host "RTUH3A_RUNNER_EXIT=$runExit"
Write-Host "RTUH3A_RUNNER_LOG=$runLog"
if($runExit -ne 0){throw "RTUH3A_RUN_FAILED"}

Write-Host "============================================================"
Write-Host " PHYSICAL TFT OBSERVATION"
Write-Host "============================================================"
$m=(Read-Host "MASTER COM14 estable y sin parpadeo/cortes visibles? (S/N)").Trim().ToUpper() -eq "S"
$s=(Read-Host "SLAVE COM4 estable y sin parpadeo/cortes visibles? (S/N)").Trim().ToUpper() -eq "S"
Write-Host "MASTER_TFT_PHYSICAL_PASS=$m"
Write-Host "SLAVE_TFT_PHYSICAL_PASS=$s"
if(-not($m -and $s)){throw "RTUH3A_TFT_PHYSICAL_REVIEW"}

$finalCore=Get-G2Sha256 $script:G2CoreRelative
$finalArchive=Get-G2Sha256 $archiveRelative
$finalDirty=@(Get-G2TrackedDirtyPaths)
$finalStaged=@(& git -C $script:G2RepoRoot diff --cached --name-only)
Write-Host "CORE_A_SHA256=$finalCore"
Write-Host "MODBUS_RTU_ARCHIVE_SHA256=$finalArchive"
Write-Host "TRACKED_DIRTY_FINAL=$($finalDirty.Count)"
Write-Host "STAGED_COUNT_FINAL=$($finalStaged.Count)"
if($finalCore -ne $expectedCoreHash){throw "RTUH3A_CORE_HASH_CHANGED"}
if($finalArchive -ne $archiveHashBefore){throw "RTUH3A_ARCHIVE_CHANGED"}
if($finalDirty.Count -ne 1 -or $finalDirty[0].Replace("\","/") -ne $script:G2CoreRelative){throw "RTUH3A_FINAL_DIRTY_SCOPE_INVALID"}
if($finalStaged.Count -ne 0){throw "RTUH3A_FINAL_INDEX_NOT_CLEAN"}
Write-Host "A14_RTU_H3A_RX_FIFO_SWEEP_GATE=PASS"
Write-Host "NEXT=RETURN_OUTPUT_TO_CHAT_BEFORE_BULK_RX_DECISION"
