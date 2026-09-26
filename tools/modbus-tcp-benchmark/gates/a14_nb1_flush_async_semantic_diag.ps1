param(
    [string]$SerialPort = "COM14",
    [string]$DutIp = "192.168.0.31"
)
Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"
. (Join-Path $PSScriptRoot "common.ps1")

function Invoke-NativeToLog {
    param([string]$FilePath,[string[]]$Arguments,[string]$LogPath)
    $old=$ErrorActionPreference
    try {
        $ErrorActionPreference="Continue"
        & $FilePath @Arguments *> $LogPath
        return [int]$LASTEXITCODE
    } finally {
        $ErrorActionPreference=$old
    }
}

Write-Host "============================================================"
Write-Host " A14 NB1-D2S - SEMANTIC TCP FLUSH ASYNC"
Write-Host "============================================================"
Assert-G2Branch

$header="JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/JWPLC_W5x00_Ethernet.h"
$cpp="JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/EthernetClient.cpp"
$raw=$script:G2RawFirmwareRelative
$probeRel="tools/modbus-tcp-benchmark/firmware/a14_nb1_flush_async_semantic_probe/a14_nb1_flush_async_semantic_probe.ino"
$clientRel="tools/modbus-tcp-benchmark/gates/a14_nb1_flush_async_semantic_client.py"
$expectedHeader="3E11094D68873D81F264F8AD97C1D55867F5399E28613061BC38484CD7DE614B"
$expectedCpp="93B71C92E298053425FF03D750632A30102BE743E1C8B1FDAB800214EABED4B2"
$expectedRaw="9A0C6CF27E44A61DC97686FB1A769EBBBB34D036CDF8D0CA0EEAB597E699AB6B"

$dirty=@(Get-G2TrackedDirtyPaths)
$expected=@($header,$cpp,$script:G2SpiHeaderRelative,$raw)|Sort-Object
Write-Host "HEAD=$(Get-G2Head)"
Write-Host "EFFECTIVE_SPI_HZ=$(Get-G2SpiHz)"
Write-Host "CONNECTION_TIMEOUT_MS=1000_UNCHANGED"
Write-Host "TARGET_CYCLES=20"
Write-Host "PAYLOAD_BYTES=1024"
Write-Host "FLUSH_SEMANTICS=WAIT_FOR_TX_FSR_FULL_AFTER_SEND"
Write-Host "TRACKED_DIRTY_COUNT=$($dirty.Count)"
$dirty|ForEach-Object{Write-Host "DIRTY=$_"}
if($dirty.Count -ne 4){throw "A14_NB1D2S_DIRTY_COUNT_INVALID"}
for($i=0;$i -lt 4;++$i){if($dirty[$i] -ne $expected[$i]){throw "A14_NB1D2S_DIRTY_PATH_INVALID"}}
$staged=@(& git -C $script:G2RepoRoot diff --cached --name-only)
if($LASTEXITCODE -ne 0 -or $staged.Count -ne 0){throw "A14_NB1D2S_INDEX_NOT_CLEAN"}
Write-Host "STAGED_COUNT=0"

Assert-G2ProtectedArtifacts
if((Get-G2Sha256 $header) -ne $expectedHeader){throw "A14_NB1D2S_HEADER_HASH_MISMATCH"}
if((Get-G2Sha256 $cpp) -ne $expectedCpp){throw "A14_NB1D2S_CPP_HASH_MISMATCH"}
if((Get-G2Sha256 $raw) -ne $expectedRaw){throw "A14_NB1D2S_RAW_HASH_MISMATCH"}
if((Get-G2SpiHz) -ne 26000000){throw "A14_NB1D2S_SPI_MISMATCH"}

$cli="C:\Program Files\Arduino PLC IDE Tools\arduino-cli.exe"
$python=(Get-Command python.exe -ErrorAction SilentlyContinue)
if($null -eq $python){$python=Get-Command python -ErrorAction Stop}
$pythonExe=$python.Source
$fqbn="jwplc_local:esp32:jwplcbasic"
$libraries=Get-G2Path "JWPLC/2.1.0/libraries"
$probe=Get-G2Path $probeRel
$probeDir=Split-Path -Parent $probe
$client=Get-G2Path $clientRel
$temp=Join-Path $env:TEMP ("jwplc_a14_nb1_d2s_"+(Get-Date -Format "yyyyMMdd_HHmmss"))
$build=Join-Path $temp "build";New-Item -ItemType Directory -Force -Path $build|Out-Null
$cl=Join-Path $temp "compile.log";$ul=Join-Path $temp "upload.log";$pl=Join-Path $temp "client.log"
Write-Host "PROBE=$probe"
Write-Host "CLIENT=$client"
Write-Host "TEMP_ROOT=$temp"
Write-Host "SOURCE_MUTATION=NO"

$cargs=@("compile","--fqbn",$fqbn,"--build-path",$build,"--libraries",$libraries,$probeDir)
$ce=Invoke-NativeToLog $cli $cargs $cl
Write-Host "COMPILE_EXIT=$ce"
if($ce -ne 0){Get-Content $cl -Tail 120|ForEach-Object{Write-Host $_};throw "A14_NB1D2S_COMPILE_FAILED"}

$compileText=[IO.File]::ReadAllText($cl)
$repoEth=(Get-G2Path "JWPLC/2.1.0/libraries/JWPLC_Ethernet")
if($compileText.IndexOf($repoEth,[StringComparison]::OrdinalIgnoreCase) -lt 0){throw "A14_NB1D2S_REPO_ETHERNET_NOT_USED"}
Write-Host "REPO_ETHERNET_LIBRARY_USED=True"

$uargs=@("upload","--fqbn",$fqbn,"--port",$SerialPort,"--input-dir",$build,$probeDir)
$ue=Invoke-NativeToLog $cli $uargs $ul
Write-Host "UPLOAD_EXIT=$ue"
if($ue -ne 0){Get-Content $ul -Tail 120|ForEach-Object{Write-Host $_};throw "A14_NB1D2S_UPLOAD_FAILED"}

Start-Sleep -Milliseconds 600
$pargs=@($client,"--host",$DutIp,"--serial",$SerialPort,"--timeout-s","12")
$pe=Invoke-NativeToLog $pythonExe $pargs $pl
Write-Host "CLIENT_EXIT=$pe"
Get-Content $pl|ForEach-Object{Write-Host $_}
if($pe -ne 0){throw "A14_NB1D2S_CLIENT_FAILED=$pe"}

$text=[IO.File]::ReadAllText($pl)
function V([string]$k){$m=@([regex]::Matches($text,"(?m)^"+[regex]::Escape($k)+"=(.*)\r?$"));if($m.Count -ne 1){throw "A14_NB1D2S_KEY_COUNT_$k=$($m.Count)"};return $m[0].Groups[1].Value.Trim()}
function I([string]$k){return [int64]::Parse((V $k),[Globalization.CultureInfo]::InvariantCulture)}

$result=I "RESULT_CODE";$failed=V "PROBE_FAILED";$cycles=I "CYCLES_COMPLETED"
$beginPending=I "FLUSH_BEGIN_PENDING_COUNT";$beginImmediate=I "FLUSH_BEGIN_IMMEDIATE_COUNT"
$pollCount=I "FLUSH_POLL_COUNT_TOTAL";$pendingPolls=I "FLUSH_POLL_PENDING_TOTAL";$timeouts=I "FLUSH_TIMEOUT_COUNT"
$durMin=I "FLUSH_DURATION_MIN_US";$durAvg=I "FLUSH_DURATION_AVG_US";$durMax=I "FLUSH_DURATION_MAX_US"
$pollHold=I "FLUSH_POLL_HOLD_MAX_US";$serviceHold=I "SERVICE_SPI_HOLD_MAX_US";$gap=I "LOOP_GAP_MAX_US";$lockErrors=I "SPI_LOCK_ERRORS"
$sendFsr=I "FIRST_AFTER_SEND_TX_FSR";$doneFsr=I "FIRST_FLUSH_DONE_TX_FSR";$rx=I "CLIENT_RX_BYTES"

Write-Host ""
Write-Host "=== NB1-D2S SUMMARY ==="
Write-Host "CYCLES_COMPLETED=$cycles"
Write-Host "FLUSH_BEGIN_PENDING_COUNT=$beginPending"
Write-Host "FLUSH_BEGIN_IMMEDIATE_COUNT=$beginImmediate"
Write-Host "FLUSH_POLL_COUNT_TOTAL=$pollCount"
Write-Host "FLUSH_POLL_PENDING_TOTAL=$pendingPolls"
Write-Host "FLUSH_TIMEOUT_COUNT=$timeouts"
Write-Host "FLUSH_DURATION_MIN_US=$durMin"
Write-Host "FLUSH_DURATION_AVG_US=$durAvg"
Write-Host "FLUSH_DURATION_MAX_US=$durMax"
Write-Host "FLUSH_POLL_HOLD_MAX_US=$pollHold"
Write-Host "SERVICE_SPI_HOLD_MAX_US=$serviceHold"
Write-Host "LOOP_GAP_MAX_US=$gap"
Write-Host "SPI_LOCK_ERRORS=$lockErrors"
Write-Host "FIRST_AFTER_SEND_TX_FSR=$sendFsr"
Write-Host "FIRST_FLUSH_DONE_TX_FSR=$doneFsr"
Write-Host "CLIENT_RX_BYTES=$rx"

if($result -ne 1 -or $failed -ne "NO"){throw "A14_NB1D2S_PROBE_FAIL"}
if($cycles -ne 20){throw "A14_NB1D2S_CYCLES_INVALID=$cycles"}
if($beginPending -lt 1){throw "A14_NB1D2S_PENDING_NOT_OBSERVED"}
if($timeouts -ne 0){throw "A14_NB1D2S_TIMEOUTS=$timeouts"}
if($pollHold -gt 5000){throw "A14_NB1D2S_POLL_HOLD_HIGH=$pollHold"}
if($serviceHold -gt 10000){throw "A14_NB1D2S_SERVICE_HOLD_HIGH=$serviceHold"}
if($gap -gt 10000){throw "A14_NB1D2S_LOOP_GAP_HIGH=$gap"}
if($lockErrors -ne 0){throw "A14_NB1D2S_SPI_LOCK_ERRORS=$lockErrors"}
if($sendFsr -ge 2048){throw "A14_NB1D2S_SEND_FSR_NOT_PENDING=$sendFsr"}
if($doneFsr -ne 2048){throw "A14_NB1D2S_DONE_FSR_NOT_FULL=$doneFsr"}
if($rx -lt 20480){throw "A14_NB1D2S_RX_TOO_SMALL=$rx"}

Write-Host ""
Write-Host "OBSERVACION FISICA REQUERIDA: mira la TFT durante NB1-D2S."
$answer=""
while($answer -notin @("S","N")){$answer=(Read-Host 'Aparecio el diagnostico visual "SPI" durante NB1-D2S? (S/N)').Trim().ToUpperInvariant()}
$visual=$(if($answer -eq "S"){1}else{0})
Write-Host "VISUAL_SPI_EVENTS=$visual"
if($visual -ne 0){throw "A14_NB1D2S_VISUAL_SPI_FAIL"}

Assert-G2ProtectedArtifacts
$dirtyFinal=@(Get-G2TrackedDirtyPaths);$stagedFinal=@(& git -C $script:G2RepoRoot diff --cached --name-only)
Write-Host "TRACKED_DIRTY_COUNT_FINAL=$($dirtyFinal.Count)"
Write-Host "STAGED_COUNT_FINAL=$($stagedFinal.Count)"
if($dirtyFinal.Count -ne 4 -or $stagedFinal.Count -ne 0){throw "A14_NB1D2S_FINAL_WORKTREE_INVALID"}

Write-Host "NB1_FLUSH_SEMANTICS=TX_FSR_FULL_AFTER_PEER_ACK"
Write-Host "NB1_FLUSH_ASYNC_PENDING=REPRODUCED"
Write-Host "NB1_FLUSH_ASYNC_COOPERATIVE=PASS"
Write-Host "NB1_FLUSH_TIMEOUT_MS=1000_PRESERVED"
Write-Host "NB1_VISUAL_SPI=PASS"
Write-Host "A14_NB1_FLUSH_ASYNC_SEMANTIC_DIAG=PASS"
