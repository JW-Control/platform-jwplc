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
Write-Host " A14 NB1-D2R - W5500 TX REGISTER SEMANTICS"
Write-Host "============================================================"
Assert-G2Branch

$headerRelative="JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/JWPLC_W5x00_Ethernet.h"
$cppRelative="JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/EthernetClient.cpp"
$firmwareRelative=$script:G2RawFirmwareRelative
$probeRelative="tools/modbus-tcp-benchmark/firmware/a14_nb1_tx_register_probe/a14_nb1_tx_register_probe.ino"
$clientRelative="tools/modbus-tcp-benchmark/gates/a14_nb1_tx_register_probe_client.py"

$expectedHeader="3E11094D68873D81F264F8AD97C1D55867F5399E28613061BC38484CD7DE614B"
$expectedCpp="93B71C92E298053425FF03D750632A30102BE743E1C8B1FDAB800214EABED4B2"
$expectedFirmware="9A0C6CF27E44A61DC97686FB1A769EBBBB34D036CDF8D0CA0EEAB597E699AB6B"

$dirty=@(Get-G2TrackedDirtyPaths)
$expectedDirty=@($headerRelative,$cppRelative,$script:G2SpiHeaderRelative,$firmwareRelative)|Sort-Object
Write-Host "HEAD=$(Get-G2Head)"
Write-Host "EFFECTIVE_SPI_HZ=$(Get-G2SpiHz)"
Write-Host "TRACKED_DIRTY_COUNT=$($dirty.Count)"
$dirty|ForEach-Object{Write-Host "DIRTY=$_"}
if($dirty.Count -ne 4){throw "A14_NB1D2R_DIRTY_COUNT_INVALID"}
for($i=0;$i -lt 4;++$i){if($dirty[$i] -ne $expectedDirty[$i]){throw "A14_NB1D2R_DIRTY_PATH_INVALID"}}
$staged=@(& git -C $script:G2RepoRoot diff --cached --name-only)
if($LASTEXITCODE -ne 0 -or $staged.Count -ne 0){throw "A14_NB1D2R_INDEX_NOT_CLEAN"}
Write-Host "STAGED_COUNT=0"

Assert-G2ProtectedArtifacts
if((Get-G2Sha256 $headerRelative)-ne $expectedHeader){throw "A14_NB1D2R_HEADER_HASH_MISMATCH"}
if((Get-G2Sha256 $cppRelative)-ne $expectedCpp){throw "A14_NB1D2R_CPP_HASH_MISMATCH"}
if((Get-G2Sha256 $firmwareRelative)-ne $expectedFirmware){throw "A14_NB1D2R_FIRMWARE_HASH_MISMATCH"}
if((Get-G2SpiHz)-ne 26000000){throw "A14_NB1D2R_SPI_MISMATCH"}

$cli="C:\Program Files\Arduino PLC IDE Tools\arduino-cli.exe"
$python=(Get-Command python.exe -ErrorAction SilentlyContinue)
if($null -eq $python){$python=Get-Command python -ErrorAction Stop}
$pythonExe=$python.Source
$fqbn="jwplc_local:esp32:jwplcbasic"
$libraries=Get-G2Path "JWPLC/2.1.0/libraries"
$probe=Get-G2Path $probeRelative
$probeDir=Split-Path -Parent $probe
$client=Get-G2Path $clientRelative
$temp=Join-Path $env:TEMP ("jwplc_a14_nb1_d2r_"+(Get-Date -Format "yyyyMMdd_HHmmss"))
$build=Join-Path $temp "build"; New-Item -ItemType Directory -Force -Path $build|Out-Null
$compileLog=Join-Path $temp "compile.log"; $uploadLog=Join-Path $temp "upload.log"; $clientLog=Join-Path $temp "client.log"

Write-Host "PROBE=$probe"
Write-Host "CLIENT=$client"
Write-Host "TEMP_ROOT=$temp"
Write-Host "SOURCE_MUTATION=NO"

$cargs=@("compile","--fqbn",$fqbn,"--build-path",$build,"--libraries",$libraries,$probeDir)
$ce=Invoke-NativeToLog $cli $cargs $compileLog
Write-Host "COMPILE_EXIT=$ce"
if($ce -ne 0){Get-Content $compileLog -Tail 120|ForEach-Object{Write-Host $_};throw "A14_NB1D2R_COMPILE_FAILED"}

$uargs=@("upload","--fqbn",$fqbn,"--port",$SerialPort,"--input-dir",$build,$probeDir)
$ue=Invoke-NativeToLog $cli $uargs $uploadLog
Write-Host "UPLOAD_EXIT=$ue"
if($ue -ne 0){Get-Content $uploadLog -Tail 120|ForEach-Object{Write-Host $_};throw "A14_NB1D2R_UPLOAD_FAILED"}

Start-Sleep -Milliseconds 600
$pargs=@($client,"--host",$DutIp,"--serial",$SerialPort,"--timeout-s","10")
$pe=Invoke-NativeToLog $pythonExe $pargs $clientLog
Write-Host "CLIENT_EXIT=$pe"
Get-Content $clientLog|ForEach-Object{Write-Host $_}
if($pe -ne 0){throw "A14_NB1D2R_CLIENT_FAILED=$pe"}

$text=[IO.File]::ReadAllText($clientLog)
function V([string]$k){$m=@([regex]::Matches($text,"(?m)^"+[regex]::Escape($k)+"=(.*)\r?$"));if($m.Count-ne1){throw "A14_NB1D2R_KEY_COUNT_$k=$($m.Count)"};return $m[0].Groups[1].Value.Trim()}
function I([string]$k){return [int64]::Parse((V $k),[Globalization.CultureInfo]::InvariantCulture)}

$result=I "RESULT_CODE"; $failed=V "PROBE_FAILED"; $rx=I "CLIENT_RX_BYTES"
$bwr=I "BEFORE_TX_WR"; $brd=I "BEFORE_TX_RD"; $bfsr=I "BEFORE_TX_FSR"
$awr=I "AFTER_BUFFER_TX_WR"; $ard=I "AFTER_BUFFER_TX_RD"; $afsr=I "AFTER_BUFFER_TX_FSR"
$swr=I "AFTER_SEND_TX_WR"; $srd=I "AFTER_SEND_TX_RD"; $sfsr=I "AFTER_SEND_TX_FSR"
$owr=I "SEND_OK_TX_WR"; $ord=I "SEND_OK_TX_RD"; $ofsr=I "SEND_OK_TX_FSR"
$elapsed=I "SEND_OK_ELAPSED_US"

Write-Host ""
Write-Host "=== NB1-D2R SUMMARY ==="
Write-Host "BEFORE_TX_WR=$bwr BEFORE_TX_RD=$brd BEFORE_TX_FSR=$bfsr"
Write-Host "AFTER_BUFFER_TX_WR=$awr AFTER_BUFFER_TX_RD=$ard AFTER_BUFFER_TX_FSR=$afsr"
Write-Host "AFTER_SEND_TX_WR=$swr AFTER_SEND_TX_RD=$srd AFTER_SEND_TX_FSR=$sfsr"
Write-Host "SEND_OK_TX_WR=$owr SEND_OK_TX_RD=$ord SEND_OK_TX_FSR=$ofsr"
Write-Host "SEND_OK_ELAPSED_US=$elapsed"
Write-Host "CLIENT_RX_BYTES=$rx"

if($result-ne1 -or $failed-ne"NO"){throw "A14_NB1D2R_PROBE_FAIL"}
if($rx-lt1024){throw "A14_NB1D2R_RX_TOO_SMALL=$rx"}
if((V "BEFORE_TX_FSR_STABLE")-ne"YES" -or (V "AFTER_BUFFER_TX_FSR_STABLE")-ne"YES" -or (V "AFTER_SEND_TX_FSR_STABLE")-ne"YES" -or (V "SEND_OK_TX_FSR_STABLE")-ne"YES"){throw "A14_NB1D2R_FSR_UNSTABLE"}

Assert-G2ProtectedArtifacts
Write-Host "A14_NB1_TX_REGISTER_SEMANTICS=CAPTURED"
Write-Host "A14_NB1_D2R=PASS"
