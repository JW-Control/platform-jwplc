param(
    [string]$MasterPort = "COM14",
    [string]$SlavePort = "COM4",
    [double]$DurationPerCaseS = 30.0
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

function Get-RunnerDouble {
    param([string]$Text, [string]$Key)

    $m = [regex]::Match(
        $Text,
        "(?m)^" + [regex]::Escape($Key) + "=([0-9.]+)\r?$"
    )

    if (-not $m.Success) {
        throw ("P5CAP1_MISSING_{0}" -f $Key)
    }

    return [double]::Parse(
        $m.Groups[1].Value,
        [System.Globalization.CultureInfo]::InvariantCulture
    )
}

function Get-RunnerText {
    param([string]$Text, [string]$Key)

    $m = [regex]::Match(
        $Text,
        "(?m)^" + [regex]::Escape($Key) + "=(.+?)\r?$"
    )

    if (-not $m.Success) {
        throw ("P5CAP1_MISSING_{0}" -f $Key)
    }

    return $m.Groups[1].Value.Trim()
}

Write-Host "============================================================"
Write-Host " A14 P5-CAP1 - INDUSTRIAL FULL-RUNTIME ETHERNET SWEEP"
Write-Host "============================================================"

Assert-G2Branch

$expectedCoreHash = "4BFF8C8241DA2E8BD0E1BBA99835ADDF91B9085A05C4DFBD05C339B824794566"
$quantities = @(1, 8, 16, 32, 64, 125)

if ($DurationPerCaseS -lt 30.0) {
    throw "P5CAP1_DURATION_PER_CASE_MUST_BE_AT_LEAST_30S"
}

$head = Get-G2Head
$spiHz = Get-G2SpiHz
$dirty = @(Get-G2TrackedDirtyPaths)
$staged = @(& git -C $script:G2RepoRoot diff --cached --name-only)

Write-Host "HEAD=$head"
Write-Host "W5500_SPI_HZ=$spiHz"
Write-Host "MASTER_PORT=$MasterPort"
Write-Host "SLAVE_PORT=$SlavePort"
Write-Host ("DURATION_PER_CASE_S={0:F0}" -f $DurationPerCaseS)
Write-Host "FC03_QUANTITIES=$($quantities -join ',')"
Write-Host "TCP_PACING=NONE"
Write-Host "TCP_OUTSTANDING_REQUESTS=1"
Write-Host "CANDIDATE=CORE_PRE_POST_LOOP_AUTOSERVICE"
Write-Host "EXPECTED_CORE_SHA256=$expectedCoreHash"

Write-Host ""
Write-Host "=== INDUSTRIAL ACTIVE WORKLOAD PROFILE ==="
Write-Host "DISPLAY_TELEMETRY_HZ=10"
Write-Host "IO_SAMPLE_HZ=50"
Write-Host "BUTTON_SAMPLE_HZ=50"
Write-Host "RTU_TARGET_HZ=50"
Write-Host "FRAM_CYCLES_HZ=4"
Write-Host "FRAM_BYTES_PER_CYCLE=96"
Write-Host "FRAM_PAYLOAD_BYTES_S=384"
Write-Host "RTC_CACHE_SAMPLE_HZ=4"
Write-Host "SD_RECORD_HZ=1"
Write-Host "SD_RECORD_BYTES=32"
Write-Host "SD_LOGICAL_BYTES_S=32"
Write-Host "SD_DATALOG_BUFFER_BYTES=4096"
Write-Host "SD_COMMIT_THRESHOLD_BYTES=512"
Write-Host "SD_COMMIT_TIMEOUT_MS=5000"
Write-Host "SD_VERIFY_HZ=0.2"
Write-Host "SPI_PROBE_HZ=10"

if ($spiHz -ne 26000000) {
    throw "P5CAP1_EXPECTED_26MHZ"
}

if ($staged.Count -ne 0) {
    $staged | ForEach-Object { Write-Host "STAGED=$_" }
    throw "P5CAP1_INDEX_NOT_CLEAN"
}

if (
    $dirty.Count -ne 1 -or
    $dirty[0].Replace("\", "/") -ne $script:G2CoreRelative
) {
    $dirty | ForEach-Object { Write-Host "DIRTY=$_" }
    throw "P5CAP1_EXPECTED_ONLY_DIRTY_CORE_A"
}

$coreHash = Get-G2Sha256 $script:G2CoreRelative
$sdHash = Get-G2Sha256 $script:G2SdRelative

Write-Host "CORE_A_SHA256=$coreHash"
Write-Host "LIBJW_SD_A_SHA256=$sdHash"

if ($coreHash -ne $expectedCoreHash) {
    throw "P5CAP1_UNEXPECTED_CANDIDATE_CORE_HASH"
}

if ($sdHash -ne $script:G2SdSha256) {
    throw "P5CAP1_SD_HASH_MISMATCH"
}

$p5bGate = Join-Path $PSScriptRoot "a14_p5b_physical_master_slave_combined.ps1"
$runner = Get-G2Path "tools/modbus-tcp-benchmark/pc/a14_p5e1_unpaced_fc03_ceiling.py"
$verifyScript = Get-G2Path "tools/build-speed-benchmark/Verify-JWPLCPrecompiledCore.ps1"
$arduinoCli = "C:\Program Files\Arduino PLC IDE Tools\arduino-cli.exe"

foreach ($required in @($p5bGate, $runner, $verifyScript, $arduinoCli)) {
    if (-not (Test-Path -LiteralPath $required)) {
        throw ("P5CAP1_REQUIRED_PATH_MISSING={0}" -f $required)
    }
}

$pythonCommand = Get-Command python.exe -ErrorAction SilentlyContinue
if ($null -eq $pythonCommand) {
    $pythonCommand = Get-Command python -ErrorAction SilentlyContinue
}
if ($null -eq $pythonCommand) {
    throw "P5CAP1_PYTHON_NOT_FOUND"
}
$pythonExe = $pythonCommand.Source

$timestamp = Get-Date -Format "yyyyMMdd_HHmmss"
$tempRoot = Join-Path $env:TEMP ("jwplc_a14_p5cap1_{0}" -f $timestamp)
$verifyLog = Join-Path $tempRoot "verify_core.log"
$setupLog = Join-Path $tempRoot "setup.log"

New-Item -ItemType Directory -Force -Path $tempRoot | Out-Null
Write-Host "TEMP_ROOT=$tempRoot"

Write-Host ""
Write-Host "=== VERIFY EXISTING CANDIDATE CORE.A ==="

$verifyArgs = @(
    "-NoLogo", "-NoProfile", "-ExecutionPolicy", "Bypass",
    "-File", $verifyScript,
    "-Target", "Basic",
    "-ArduinoCli", $arduinoCli
)

$verifyExit = Invoke-NativeToLog -FilePath "powershell.exe" -Arguments $verifyArgs -LogPath $verifyLog
Write-Host "P5CAP1_CORE_VERIFY_EXIT=$verifyExit"
Write-Host "P5CAP1_CORE_VERIFY_LOG=$verifyLog"

if ($verifyExit -ne 0) {
    Get-Content -LiteralPath $verifyLog -Tail 300 | ForEach-Object { Write-Host $_ }
    throw "P5CAP1_CORE_VERIFY_FAILED"
}

Write-Host ""
Write-Host "=== COMPILE / UPLOAD INDUSTRIAL FULL RUNTIME ==="

$setupArgs = @(
    "-NoLogo", "-NoProfile", "-ExecutionPolicy", "Bypass",
    "-File", $p5bGate,
    "-MasterPort", $MasterPort,
    "-SlavePort", $SlavePort,
    "-SetupOnly",
    "-AllowDirtyCoreCandidate"
)

$setupExit = Invoke-NativeToLog -FilePath "powershell.exe" -Arguments $setupArgs -LogPath $setupLog
Write-Host "P5CAP1_SETUP_EXIT=$setupExit"
Write-Host "P5CAP1_SETUP_LOG=$setupLog"

Get-Content -LiteralPath $setupLog | ForEach-Object { Write-Host $_ }

if ($setupExit -ne 0) {
    throw "P5CAP1_SETUP_FAILED"
}

$setupText = [System.IO.File]::ReadAllText($setupLog)
$ipMatch = [regex]::Match(
    $setupText,
    "(?m)^P5B_SETUP_ONLY_MASTER_IP=(.+?)\r?$"
)

if (-not $ipMatch.Success) {
    throw "P5CAP1_SETUP_IP_MISSING"
}

$dutIp = $ipMatch.Groups[1].Value.Trim()
Write-Host "P5CAP1_DUT_IP=$dutIp"

$rows = New-Object System.Collections.Generic.List[object]

foreach ($quantity in $quantities) {
    Write-Host ""
    Write-Host "------------------------------------------------------------"
    Write-Host (" P5-CAP1 CASE Q={0}" -f $quantity)
    Write-Host "------------------------------------------------------------"

    $caseLog = Join-Path $tempRoot ("q{0}.log" -f $quantity)
    $durationText = $DurationPerCaseS.ToString(
        [System.Globalization.CultureInfo]::InvariantCulture
    )

    $runnerArgs = @(
        $runner,
        "--master-serial", $MasterPort,
        "--slave-serial", $SlavePort,
        "--host", $dutIp,
        "--port", "502",
        "--duration", $durationText,
        "--quantity", $quantity.ToString(),
        "--bucket-seconds", $durationText
    )

    $runnerExit = Invoke-NativeToLog -FilePath $pythonExe -Arguments $runnerArgs -LogPath $caseLog
    Write-Host "P5CAP1_CASE_RUNNER_EXIT=$runnerExit"
    Write-Host "P5CAP1_CASE_LOG=$caseLog"

    Get-Content -LiteralPath $caseLog | ForEach-Object { Write-Host $_ }

    if ($runnerExit -ne 0) {
        throw ("P5CAP1_CASE_Q{0}_FAILED" -f $quantity)
    }

    $text = [System.IO.File]::ReadAllText($caseLog)

    $reqS = Get-RunnerDouble -Text $text -Key "P5E1_ACHIEVED_REQ_S"
    $useful = Get-RunnerDouble -Text $text -Key "P5E1_USEFUL_MBPS"
    $total = Get-RunnerDouble -Text $text -Key "P5E1_TOTAL_MODBUS_MBPS"
    $avg = Get-RunnerDouble -Text $text -Key "P5E1_LATENCY_AVG_US"
    $p95 = Get-RunnerDouble -Text $text -Key "P5E1_LATENCY_P95_US"
    $p99 = Get-RunnerDouble -Text $text -Key "P5E1_LATENCY_P99_US"
    $max = Get-RunnerDouble -Text $text -Key "P5E1_LATENCY_MAX_US"
    $rtuHz = Get-RunnerDouble -Text $text -Key "P5E1_RTU_ACHIEVED_HZ"
    $sdCommitted = Get-RunnerDouble -Text $text -Key "P5E1_SD_COMMITTED_BYTES"
    $tcpClean = Get-RunnerText -Text $text -Key "P5E1_TCP_CLEAN"
    $rtuPass = Get-RunnerText -Text $text -Key "P5E1_RTU_PASS"
    $runtimePass = Get-RunnerText -Text $text -Key "P5E1_RUNTIME_PASS"
    $peripheralFails = Get-RunnerDouble -Text $text -Key "P5E1_PERIPHERAL_FAILURE_COUNT"
    $sdFailed = Get-RunnerDouble -Text $text -Key "P5E1_SD_FAILED_COMMITS"

    if (
        $tcpClean -ne "YES" -or
        $rtuPass -ne "YES" -or
        $runtimePass -ne "YES" -or
        $peripheralFails -ne 0 -or
        $sdFailed -ne 0
    ) {
        throw ("P5CAP1_CASE_Q{0}_RUNTIME_NOT_CLEAN" -f $quantity)
    }

    $responseUsefulBytes = 2 * $quantity
    $responseAppBytes = 9 + $responseUsefulBytes
    $requestAppBytes = 12
    $transactionAppBytes = $requestAppBytes + $responseAppBytes

    $row = [PSCustomObject]@{
        Quantity = $quantity
        ReqS = $reqS
        UsefulMbps = $useful
        TotalMbps = $total
        AvgUs = $avg
        P95Us = $p95
        P99Us = $p99
        MaxUs = $max
        RtuHz = $rtuHz
        SdCommitted = $sdCommitted
        UsefulBytesPerResponse = $responseUsefulBytes
        TransactionAppBytes = $transactionAppBytes
    }

    $rows.Add($row)

    Write-Host (
        "P5CAP1_CASE Q={0} USEFUL_B_RESPONSE={1} APP_B_TRANSACTION={2} REQ_S={3:F3} USEFUL_MBPS={4:F4} TOTAL_MBPS={5:F4} AVG_US={6:F1} P95_US={7:F1} P99_US={8:F1} RTU_HZ={9:F3} SD_COMMITTED_B={10:F0}" -f
        $quantity,
        $responseUsefulBytes,
        $transactionAppBytes,
        $reqS,
        $useful,
        $total,
        $avg,
        $p95,
        $p99,
        $rtuHz,
        $sdCommitted
    )
}

$maxReqRow = $rows | Sort-Object ReqS -Descending | Select-Object -First 1
$maxUsefulRow = $rows | Sort-Object UsefulMbps -Descending | Select-Object -First 1
$maxTotalRow = $rows | Sort-Object TotalMbps -Descending | Select-Object -First 1

Write-Host ""
Write-Host "============================================================"
Write-Host " P5-CAP1 INDUSTRIAL ETHERNET CAPACITY SUMMARY"
Write-Host "============================================================"

foreach ($row in $rows) {
    Write-Host (
        "P5CAP1_SUMMARY Q={0} REQ_S={1:F3} USEFUL_MBPS={2:F4} TOTAL_MBPS={3:F4} AVG_US={4:F1} P95_US={5:F1} P99_US={6:F1}" -f
        $row.Quantity,
        $row.ReqS,
        $row.UsefulMbps,
        $row.TotalMbps,
        $row.AvgUs,
        $row.P95Us,
        $row.P99Us
    )
}

Write-Host "P5CAP1_MAX_REQ_S_Q=$($maxReqRow.Quantity)"
Write-Host "P5CAP1_MAX_REQ_S=$($maxReqRow.ReqS.ToString('F3', [System.Globalization.CultureInfo]::InvariantCulture))"
Write-Host "P5CAP1_MAX_USEFUL_MBPS_Q=$($maxUsefulRow.Quantity)"
Write-Host "P5CAP1_MAX_USEFUL_MBPS=$($maxUsefulRow.UsefulMbps.ToString('F4', [System.Globalization.CultureInfo]::InvariantCulture))"
Write-Host "P5CAP1_MAX_TOTAL_MODBUS_MBPS_Q=$($maxTotalRow.Quantity)"
Write-Host "P5CAP1_MAX_TOTAL_MODBUS_MBPS=$($maxTotalRow.TotalMbps.ToString('F4', [System.Globalization.CultureInfo]::InvariantCulture))"

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
    throw "P5CAP1_TFT_PHYSICAL_REVIEW"
}

Write-Host ""
Write-Host "=== FINAL CONTROLLED STATE ==="

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
    throw "P5CAP1_CORE_HASH_CHANGED"
}

if ($finalSdHash -ne $script:G2SdSha256) {
    throw "P5CAP1_SD_HASH_CHANGED"
}

if ($finalHz -ne 26000000) {
    throw "P5CAP1_SPI_CHANGED"
}

if (
    $finalDirty.Count -ne 1 -or
    $finalDirty[0].Replace("\", "/") -ne $script:G2CoreRelative
) {
    throw "P5CAP1_FINAL_DIRTY_SCOPE_INVALID"
}

if ($finalStaged.Count -ne 0) {
    throw "P5CAP1_FINAL_INDEX_NOT_CLEAN"
}

Write-Host "A14_P5CAP1_PROFILE=INDUSTRIAL_ACTIVE_FULL_RUNTIME"
Write-Host "A14_P5CAP1_CASES=$($quantities.Count)"
Write-Host "A14_P5CAP1_DURATION_PER_CASE_S=$($DurationPerCaseS.ToString('F0', [System.Globalization.CultureInfo]::InvariantCulture))"
Write-Host "A14_P5CAP1_CHARACTERIZATION=PASS"
Write-Host "NEXT=RETURN_OUTPUT_TO_CHAT_FOR_CAPACITY_INTERPRETATION"
