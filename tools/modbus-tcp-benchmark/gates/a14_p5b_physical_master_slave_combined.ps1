param(
    [string]$MasterPort = "COM14",
    [string]$SlavePort = "COM4",
    [double]$TcpRate = 1000.0,
    [double]$DurationS = 60.0
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

function Get-LogValue {
    param([string]$Text, [string]$Key)

    $pattern = "(?m)^" + [regex]::Escape($Key) + "=(.*)\r?$"
    $matches = @([regex]::Matches($Text, $pattern))

    if ($matches.Count -ne 1) {
        throw ("P5B_LOG_KEY_COUNT_{0}={1}" -f $Key, $matches.Count)
    }

    return $matches[0].Groups[1].Value.Trim()
}

Write-Host "============================================================"
Write-Host " A14 P5-B - PHYSICAL MASTER + SLAVE COMBINED"
Write-Host " TCP1000 + RTU50 + FULL RUNTIME + HMI DIRTY"
Write-Host "============================================================"

Assert-G2Branch

$head = Get-G2Head
$spiHz = Get-G2SpiHz
$dirty = @(Get-G2TrackedDirtyPaths)
$staged = @(& git -C $script:G2RepoRoot diff --cached --name-only)

if ($LASTEXITCODE -ne 0) {
    throw "P5B_CACHED_DIFF_FAILED"
}

Write-Host "HEAD=$head"
Write-Host "W5500_SPI_HZ=$spiHz"
Write-Host "TRACKED_DIRTY_COUNT=$($dirty.Count)"
Write-Host "STAGED_COUNT=$($staged.Count)"
Write-Host "MASTER_PORT=$MasterPort"
Write-Host "SLAVE_PORT=$SlavePort"
Write-Host ("TCP_TARGET_REQ_S={0:F0}" -f $TcpRate)
Write-Host ("DURATION_S={0:F0}" -f $DurationS)
Write-Host "RTU_TARGET_HZ=50"
Write-Host "RTU_SLAVE_ID=2"
Write-Host "RTU_BAUD=115200"
Write-Host "RTU_CONFIG=8N1"
Write-Host "DISPLAY_API=USER_REFRESH_ON_DEMAND_DIRTY"
Write-Host "PRODUCT_SOURCE_MUTATION=NO"

if ($spiHz -ne 26000000) {
    throw "P5B_EXPECTED_26MHZ"
}

if ($dirty.Count -ne 0) {
    $dirty | ForEach-Object { Write-Host "DIRTY=$_" }
    throw "P5B_TRACKED_TREE_NOT_CLEAN"
}

if ($staged.Count -ne 0) {
    $staged | ForEach-Object { Write-Host "STAGED=$_" }
    throw "P5B_INDEX_NOT_CLEAN"
}

if ($TcpRate -le 0) {
    throw "P5B_TCP_RATE_INVALID"
}

if ($DurationS -lt 30.0) {
    throw "P5B_DURATION_TOO_SHORT"
}

Assert-G2ProtectedArtifacts

$ports = @([System.IO.Ports.SerialPort]::GetPortNames() | Sort-Object)
Write-Host "COM_PORTS=$($ports -join ',')"

if ($MasterPort -notin $ports) {
    throw "P5B_MASTER_COM_NOT_PRESENT"
}

if ($SlavePort -notin $ports) {
    throw "P5B_SLAVE_COM_NOT_PRESENT"
}

$pythonCommand = Get-Command python.exe -ErrorAction SilentlyContinue

if ($null -eq $pythonCommand) {
    $pythonCommand = Get-Command python -ErrorAction SilentlyContinue
}

if ($null -eq $pythonCommand) {
    throw "P5B_PYTHON_NOT_FOUND"
}

$pythonExe = $pythonCommand.Source

$arduinoCli = "C:\Program Files\Arduino PLC IDE Tools\arduino-cli.exe"

if (-not (Test-Path -LiteralPath $arduinoCli)) {
    throw "P5B_ARDUINO_CLI_NOT_FOUND"
}

$masterDir = Get-G2Path "tools/modbus-tcp-benchmark/firmware/a14_p5_full_runtime_master"
$slaveDir = Get-G2Path "tools/modbus-tcp-benchmark/firmware/a14_p5_rtu_slave"
$masterSketch = Join-Path $masterDir "a14_p5_full_runtime_master.ino"
$slaveSketch = Join-Path $slaveDir "a14_p5_rtu_slave.ino"
$resolver = Join-Path $PSScriptRoot "a14_p5_resolve_full_runtime_ip.py"
$qualification = Get-G2Path "tools/modbus-tcp-benchmark/pc/a14_p5b_master_slave_qualification.py"

foreach ($required in @($masterSketch, $slaveSketch, $resolver, $qualification)) {
    if (-not (Test-Path -LiteralPath $required)) {
        throw "P5B_REQUIRED_FILE_MISSING=$required"
    }
}

$timestamp = Get-Date -Format "yyyyMMdd_HHmmss"
$tempRoot = Join-Path $env:TEMP ("jwplc_a14_p5b_{0}" -f $timestamp)
$masterBuild = Join-Path $tempRoot "build_master"
$slaveBuild = Join-Path $tempRoot "build_slave"
$masterCompileLog = Join-Path $tempRoot "compile_master.log"
$slaveCompileLog = Join-Path $tempRoot "compile_slave.log"
$masterUploadLog = Join-Path $tempRoot "upload_master.log"
$slaveUploadLog = Join-Path $tempRoot "upload_slave.log"
$resolverLog = Join-Path $tempRoot "resolver.log"
$qualificationLog = Join-Path $tempRoot "qualification.log"
$csvPath = Join-Path $tempRoot "tcp_result.csv"
$masterSnapshot = Join-Path $tempRoot "master_final.txt"
$slaveSnapshot = Join-Path $tempRoot "slave_final.txt"

New-Item -ItemType Directory -Force -Path $tempRoot | Out-Null
New-Item -ItemType Directory -Force -Path $masterBuild | Out-Null
New-Item -ItemType Directory -Force -Path $slaveBuild | Out-Null

Write-Host "TEMP_ROOT=$tempRoot"
Write-Host "MASTER_SKETCH=$masterSketch"
Write-Host "SLAVE_SKETCH=$slaveSketch"

Write-Host ""
Write-Host "=== STATIC PREFLIGHT ==="

$pyCompileLog = Join-Path $tempRoot "py_compile.log"
$pyCompileArgs = @("-m", "py_compile", $resolver, $qualification)
$pyCompileExit = Invoke-NativeToLog -FilePath $pythonExe -Arguments $pyCompileArgs -LogPath $pyCompileLog

Write-Host "PY_COMPILE_EXIT=$pyCompileExit"

if ($pyCompileExit -ne 0) {
    Get-Content -LiteralPath $pyCompileLog -Tail 200 | ForEach-Object { Write-Host $_ }
    throw "P5B_PYTHON_SYNTAX_FAILED"
}

$masterText = [System.IO.File]::ReadAllText($masterSketch)
$slaveText = [System.IO.File]::ReadAllText($slaveSketch)

$sourceChecks = @(
    [PSCustomObject]@{ Label = "MASTER_HMI_ON_DEMAND"; Pass = $masterText.Contains("USER_REFRESH_ON_DEMAND") },
    [PSCustomObject]@{ Label = "MASTER_RTUMASTER"; Pass = $masterText.Contains("RTU_ROLE=MASTER") },
    [PSCustomObject]@{ Label = "MASTER_SLAVE2"; Pass = $masterText.Contains("RTU_TARGET_SLAVE_ID = 2") },
    [PSCustomObject]@{ Label = "MASTER_AUTO_RTU"; Pass = $masterText.Contains("RTU_TRAFFIC_AUTO_START=") },
    [PSCustomObject]@{ Label = "MASTER_COMPACT_PREFLIGHT"; Pass = $masterText.Contains("A14_P5_PREFLIGHT=END") },
    [PSCustomObject]@{ Label = "MASTER_ETH_SNAPSHOT"; Pass = $masterText.Contains("COMBINED_RUNTIME_READY=") },
    [PSCustomObject]@{ Label = "SLAVE_HMI_ON_DEMAND"; Pass = $slaveText.Contains("USER_REFRESH_ON_DEMAND") },
    [PSCustomObject]@{ Label = "SLAVE_ID2"; Pass = $slaveText.Contains("SLAVE_ID = 2") },
    [PSCustomObject]@{ Label = "SLAVE_VERIFY_MAGIC"; Pass = $slaveText.Contains("VERIFY_MAGIC = 0x55AA") }
)

foreach ($check in $sourceChecks) {
    Write-Host "$($check.Label)=$($check.Pass)"

    if (-not $check.Pass) {
        throw "P5B_SOURCE_CONTRACT_FAILED_$($check.Label)"
    }
}

if ($masterText.Contains(".fillScreen(")) {
    throw "P5B_MASTER_DIRECT_FILL_SCREEN_REMAINS"
}

if ($slaveText.Contains(".fillScreen(")) {
    throw "P5B_SLAVE_DIRECT_FILL_SCREEN_REMAINS"
}

Write-Host "P5B_SOURCE_CONTRACT=PASS"

$repoLibrariesRoot = Get-G2Path "JWPLC/2.1.0/libraries"
$fqbn = "jwplc_local:esp32:jwplcbasic"

Write-Host ""
Write-Host "=== COMPILE SLAVE ==="

$slaveCompileArgs = @("compile", "--fqbn", $fqbn, "--build-path", $slaveBuild, "--libraries", $repoLibrariesRoot, $slaveDir)
$slaveCompileExit = Invoke-NativeToLog -FilePath $arduinoCli -Arguments $slaveCompileArgs -LogPath $slaveCompileLog

Write-Host "SLAVE_COMPILE_EXIT=$slaveCompileExit"
Write-Host "SLAVE_COMPILE_LOG=$slaveCompileLog"

if ($slaveCompileExit -ne 0) {
    Invoke-G2CompileFinishedSound -Success $false
    Get-Content -LiteralPath $slaveCompileLog -Tail 240 | ForEach-Object { Write-Host $_ }
    throw "P5B_SLAVE_COMPILE_FAILED"
}

$slaveBinCount = @(Get-ChildItem -LiteralPath $slaveBuild -Recurse -File -Filter "*.bin").Count
Write-Host "SLAVE_BIN_COUNT=$slaveBinCount"

if ($slaveBinCount -lt 1) {
    throw "P5B_SLAVE_BIN_MISSING"
}

Write-Host ""
Write-Host "=== COMPILE MASTER ==="

$masterCompileArgs = @("compile", "--fqbn", $fqbn, "--build-path", $masterBuild, "--libraries", $repoLibrariesRoot, $masterDir)
$masterCompileExit = Invoke-NativeToLog -FilePath $arduinoCli -Arguments $masterCompileArgs -LogPath $masterCompileLog

Write-Host "MASTER_COMPILE_EXIT=$masterCompileExit"
Write-Host "MASTER_COMPILE_LOG=$masterCompileLog"

if ($masterCompileExit -ne 0) {
    Invoke-G2CompileFinishedSound -Success $false
    Get-Content -LiteralPath $masterCompileLog -Tail 240 | ForEach-Object { Write-Host $_ }
    throw "P5B_MASTER_COMPILE_FAILED"
}

$masterBinCount = @(Get-ChildItem -LiteralPath $masterBuild -Recurse -File -Filter "*.bin").Count
Write-Host "MASTER_BIN_COUNT=$masterBinCount"

if ($masterBinCount -lt 1) {
    throw "P5B_MASTER_BIN_MISSING"
}

Invoke-G2CompileFinishedSound -Success $true

Write-Host ""
Write-Host "=== UPLOAD SLAVE $SlavePort ==="

$slaveUploadArgs = @("upload", "--fqbn", $fqbn, "--port", $SlavePort, "--input-dir", $slaveBuild, $slaveDir)
$slaveUploadExit = Invoke-NativeToLog -FilePath $arduinoCli -Arguments $slaveUploadArgs -LogPath $slaveUploadLog

Write-Host "SLAVE_UPLOAD_EXIT=$slaveUploadExit"

if ($slaveUploadExit -ne 0) {
    Get-Content -LiteralPath $slaveUploadLog -Tail 220 | ForEach-Object { Write-Host $_ }
    throw "P5B_SLAVE_UPLOAD_FAILED"
}

Start-Sleep -Seconds 2

Write-Host ""
Write-Host "=== UPLOAD MASTER $MasterPort ==="

$masterUploadArgs = @("upload", "--fqbn", $fqbn, "--port", $MasterPort, "--input-dir", $masterBuild, $masterDir)
$masterUploadExit = Invoke-NativeToLog -FilePath $arduinoCli -Arguments $masterUploadArgs -LogPath $masterUploadLog

Write-Host "MASTER_UPLOAD_EXIT=$masterUploadExit"

if ($masterUploadExit -ne 0) {
    Get-Content -LiteralPath $masterUploadLog -Tail 220 | ForEach-Object { Write-Host $_ }
    throw "P5B_MASTER_UPLOAD_FAILED"
}

Start-Sleep -Seconds 2

Write-Host ""
Write-Host "=== PHYSICAL READY PREFLIGHT ==="
Write-Host "MICROSD=REQUIRED_INSERTED"
Write-Host "RS485_MASTER_TO_SLAVE2=REQUIRED_CONNECTED"
Write-Host "ETHERNET_MASTER=REQUIRED_CONNECTED"

$resolverArgs = @($resolver, "--serial", $MasterPort, "--timeout", "45")
$resolverExit = Invoke-NativeToLog -FilePath $pythonExe -Arguments $resolverArgs -LogPath $resolverLog

Write-Host "P5B_RESOLVER_EXIT=$resolverExit"

if ($resolverExit -ne 0) {
    Get-Content -LiteralPath $resolverLog -Tail 320 | ForEach-Object { Write-Host $_ }
    throw "P5B_PHYSICAL_PREFLIGHT_FAILED"
}

$resolverText = [System.IO.File]::ReadAllText($resolverLog)
$dutIp = Get-LogValue -Text $resolverText -Key "P5_DUT_IP_EFFECTIVE"

Get-Content -LiteralPath $resolverLog | ForEach-Object { Write-Host $_ }

Write-Host ""
Write-Host "P5B_MASTER_IP=$dutIp"
Write-Host "P5B_PREFLIGHT_SD=PASS"
Write-Host "P5B_PREFLIGHT_ETHERNET=PASS"
Write-Host "P5B_PREFLIGHT_RTU_SLAVE2=PASS"
Write-Host "P5B_PREFLIGHT_HMI_DIRTY=PASS"
Write-Host "P5B_PREFLIGHT_MODE=COMPACT_QUIET"

Write-Host ""
Write-Host "============================================================"
Write-Host " START 60S COMBINED WINDOW"
Write-Host "============================================================"

$rateText = $TcpRate.ToString([System.Globalization.CultureInfo]::InvariantCulture)
$durationText = $DurationS.ToString([System.Globalization.CultureInfo]::InvariantCulture)

$qualificationArgs = @(
    $qualification,
    "--master-serial", $MasterPort,
    "--slave-serial", $SlavePort,
    "--host", $dutIp,
    "--port", "502",
    "--rate", $rateText,
    "--duration", $durationText,
    "--csv", $csvPath,
    "--master-snapshot-out", $masterSnapshot,
    "--slave-snapshot-out", $slaveSnapshot
)

$qualificationExit = Invoke-NativeToLog -FilePath $pythonExe -Arguments $qualificationArgs -LogPath $qualificationLog

Write-Host "P5B_QUALIFICATION_EXIT=$qualificationExit"
Write-Host "P5B_QUALIFICATION_LOG=$qualificationLog"
Write-Host ""
Get-Content -LiteralPath $qualificationLog | ForEach-Object { Write-Host $_ }

if ($qualificationExit -ne 0) {
    throw "P5B_AUTOMATED_QUALIFICATION_FAILED"
}

Write-Host ""
Write-Host "============================================================"
Write-Host " PHYSICAL TFT OBSERVATION"
Write-Host "============================================================"
Write-Host "MASTER COM14 esperado:"
Write-Host " - MASTER visible"
Write-Host " - TCP OK y RTU OK avanzan"
Write-Host " - RTU FAIL permanece en 0"
Write-Host " - SD=OK / ETH=UP"
Write-Host " - sin parpadeo de pantalla completa"
Write-Host ""
Write-Host "SLAVE COM4 esperado:"
Write-Host " - SLAVE 2 visible"
Write-Host " - RX/TX/OK avanzan"
Write-Host " - CRC permanece en 0"
Write-Host " - sin parpadeo de pantalla completa"

$masterAnswer = Read-Host "¿MASTER COM14 estable y sin parpadeo/cortes visibles? (S/N)"
$slaveAnswer = Read-Host "¿SLAVE COM4 estable y sin parpadeo/cortes visibles? (S/N)"

$masterPhysical = $masterAnswer.Trim().ToUpper() -eq "S"
$slavePhysical = $slaveAnswer.Trim().ToUpper() -eq "S"
$physicalPass = $masterPhysical -and $slavePhysical

Write-Host "MASTER_TFT_PHYSICAL_PASS=$masterPhysical"
Write-Host "SLAVE_TFT_PHYSICAL_PASS=$slavePhysical"
Write-Host "TFT_PHYSICAL_PASS=$physicalPass"

if (-not $physicalPass) {
    throw "P5B_TFT_PHYSICAL_REVIEW"
}

Write-Host ""
Write-Host "=== FINAL PRODUCT INVARIANTS ==="

Assert-G2ProtectedArtifacts

$finalHz = Get-G2SpiHz
$finalDirty = @(Get-G2TrackedDirtyPaths)
$finalStaged = @(& git -C $script:G2RepoRoot diff --cached --name-only)

if ($LASTEXITCODE -ne 0) {
    throw "P5B_FINAL_CACHED_DIFF_FAILED"
}

Write-Host "FINAL_SPI_HZ=$finalHz"
Write-Host "TRACKED_DIRTY_FINAL=$($finalDirty.Count)"
Write-Host "STAGED_COUNT_FINAL=$($finalStaged.Count)"

if ($finalHz -ne 26000000) {
    throw "P5B_FINAL_SPI_CHANGED"
}

if ($finalDirty.Count -ne 0) {
    throw "P5B_PRODUCT_TREE_DIRTY"
}

if ($finalStaged.Count -ne 0) {
    throw "P5B_PRODUCT_INDEX_DIRTY"
}

Write-Host ""
Write-Host "============================================================"
Write-Host " A14 P5-B FINAL SUMMARY"
Write-Host "============================================================"
Write-Host "A14_P5B_TCP_TARGET_REQ_S=$rateText"
Write-Host "A14_P5B_RTU_TARGET_HZ=50"
Write-Host "A14_P5B_MASTER_PORT=$MasterPort"
Write-Host "A14_P5B_SLAVE_PORT=$SlavePort"
Write-Host "A14_P5B_SLAVE_ID=2"
Write-Host "A14_P5B_DISPLAY_API=USER_REFRESH_ON_DEMAND_DIRTY"
Write-Host "A14_P5B_MICROSD=QUALIFIED"
Write-Host "A14_P5B_MASTER_SLAVE_CROSS_COUNT=QUALIFIED"
Write-Host "A14_P5B_TFT_PHYSICAL=PASS"
Write-Host "A14_P5B_PRODUCT_SOURCE_MUTATION=NO"
Write-Host "A14_P5B_PHYSICAL_MASTER_SLAVE_COMBINED=PASS"
Write-Host "P5B_TEMP_ROOT=$tempRoot"
Write-Host "NEXT=RETURN_OUTPUT_TO_CHAT_FOR_P5_CLOSURE_DECISION"
