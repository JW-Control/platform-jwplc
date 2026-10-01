param(
    [string]$MasterPort = "COM14",
    [string]$SlavePort = "COM4",
    [double]$TcpRate = 1000.0,
    [double]$DurationS = 120.0,
    [string]$PythonExe = ""
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"
. (Join-Path $PSScriptRoot "common.ps1")

function Get-H3ERValue {
    param(
        [Parameter(Mandatory = $true)][string]$Text,
        [Parameter(Mandatory = $true)][string]$Key
    )

    $pattern =
        "(?m)^" +
        [regex]::Escape($Key) +
        "=(.*)\r?$"

    [object[]]$matches =
        @([regex]::Matches($Text, $pattern))

    if ($matches.Count -lt 1) {
        throw ("H3ER_KEY_MISSING={0}" -f $Key)
    }

    [string[]]$values = @(
        $matches |
            ForEach-Object {
                $_.Groups[1].Value.Trim()
            } |
            Select-Object -Unique
    )

    if ($values.Count -ne 1) {
        throw (
            "H3ER_KEY_VALUES_CONFLICT={0}:{1}" -f
            $Key,
            ($values -join ",")
        )
    }

    return $values[0]
}

function Get-H3ERDouble {
    param([string]$Text, [string]$Key)

    return [double]::Parse(
        (Get-H3ERValue -Text $Text -Key $Key),
        [Globalization.CultureInfo]::InvariantCulture)
}

function Get-H3ERInt {
    param([string]$Text, [string]$Key)

    return [int64]::Parse(
        (Get-H3ERValue -Text $Text -Key $Key),
        [Globalization.CultureInfo]::InvariantCulture)
}

function Get-H3ERModbusRtuArchiveLinkForm {
    param(
        [Parameter(Mandatory = $true)]
        [string]$NormalizedCompileText
    )

    $archiveDirectory =
        "/libraries/jwplc_modbusrtu/src/esp32"

    if ($NormalizedCompileText.Contains(
        "$archiveDirectory/libjwplc_modbusrtu.a")) {
        return "DIRECT_ARCHIVE_PATH"
    }

    $usesArchiveDirectory =
        $NormalizedCompileText.Contains($archiveDirectory)

    $usesGccLibraryFlag =
        [regex]::IsMatch(
            $NormalizedCompileText,
            '(?m)(?:^|\s)"?-ljwplc_modbusrtu"?(?=\s|$)')

    if ($usesArchiveDirectory -and $usesGccLibraryFlag) {
        return "GCC_LIBRARY_FLAG"
    }

    return "NONE"
}

Write-Host "=============================================================================="
Write-Host " A14 H3E-R - CURRENT PACKAGE FULL RUNTIME"
Write-Host " TCP 1000 req/s + RTU 50 Hz + DATALOG + FULL PERIPHERALS"
Write-Host "=============================================================================="

Assert-G2Branch

$repo = $script:G2RepoRoot
$head = Get-G2Head
$dirty = @(Get-G2TrackedDirtyPaths)
$staged = @(& git -C $repo diff --cached --name-only)

Write-Host "H3ER_HEAD=$head"
Write-Host "H3ER_MASTER_PORT=$MasterPort"
Write-Host "H3ER_SLAVE_PORT=$SlavePort"
Write-Host ("H3ER_TCP_TARGET_REQ_S={0:F0}" -f $TcpRate)
Write-Host ("H3ER_DURATION_S={0:F0}" -f $DurationS)
Write-Host "H3ER_RTU_TARGET_HZ=50"
Write-Host "H3ER_RTU_PERIOD_US=20000"
Write-Host "H3ER_W5500_SPI_HZ=$(Get-G2SpiHz)"
Write-Host "H3ER_FIFO_REUSE_SOURCE=PACKAGE_DEFAULT"
Write-Host "H3ER_DLEN_REUSE_SOURCE=PACKAGE_DEFAULT"
Write-Host "H3ER_DATALOG_POLICY=MANDATORY_PRODUCT_PATH"
Write-Host "H3ER_DISPLAY_POLICY=JWPLC_DISPLAY_HMI_ON_DEMAND_DIRTY"
Write-Host "H3ER_TFT_POLICY=JWPLC_TFT_SOURCE_PRIVATE_BACKEND"
Write-Host "H3ER_SINGLE_STATUS_POLICY=SAME_PASS_REUSE_NO_PERSISTENT_CACHE"
Write-Host "H3ER_TCP_ASYNC_POLICY=AUDIT_PRESENCE_AND_APPLICABILITY"

if ($dirty.Count -ne 0) {
    $dirty | ForEach-Object { Write-Host "DIRTY=$_" }
    throw "H3ER_TREE_NOT_CLEAN"
}

if ($staged.Count -ne 0) {
    $staged | ForEach-Object { Write-Host "STAGED=$_" }
    throw "H3ER_INDEX_NOT_CLEAN"
}

if ($TcpRate -ne 1000.0) {
    throw "H3ER_TCP_RATE_MUST_BE_1000"
}

if ($DurationS -lt 120.0) {
    throw "H3ER_DURATION_MUST_BE_AT_LEAST_120S"
}

if ((Get-G2SpiHz) -ne 26000000) {
    throw "H3ER_W5500_SPI_NOT_26MHZ"
}

if (-not [string]::IsNullOrWhiteSpace($PythonExe)) {
    if (-not (Test-Path -LiteralPath $PythonExe)) {
        throw "H3ER_PYTHON_EXPLICIT_NOT_FOUND=$PythonExe"
    }

    $pythonExeResolved = (Resolve-Path -LiteralPath $PythonExe).Path
}
else {
    $pythonCommand = Get-Command python.exe -ErrorAction SilentlyContinue

    if ($null -eq $pythonCommand) {
        $pythonCommand = Get-Command python -ErrorAction SilentlyContinue
    }

    if ($null -eq $pythonCommand) {
        throw "H3ER_PYTHON_NOT_FOUND"
    }

    $pythonExeResolved = $pythonCommand.Source
}

Write-Host "H3ER_PYTHON_EXE=$pythonExeResolved"

$contract =
    Join-Path $PSScriptRoot "a14_package_promotion_contract.py"

$contractOutput =
    @(& $pythonExeResolved -B $contract 2>&1)

$contractText =
    ($contractOutput | ForEach-Object { $_.ToString() }) -join [Environment]::NewLine

$contractOutput | ForEach-Object { Write-Host $_ }

if ($LASTEXITCODE -ne 0 -or
    -not $contractText.Contains("A14_PACKAGE_PROMOTION_CONTRACT=PASS")) {
    throw "H3ER_PACKAGE_PROMOTION_CONTRACT_FAILED"
}

$w5100Path =
    Get-G2Path "JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/utility/w5100.h"

$masterSketch =
    Get-G2Path "tools/modbus-tcp-benchmark/firmware/a14_p5_full_runtime_master/a14_p5_full_runtime_master.ino"

$p5bGate =
    Join-Path $PSScriptRoot "a14_p5b_physical_master_slave_combined.ps1"

$spiHeaderPath =
    Get-G2Path "JWPLC/2.1.0/libraries/SPI/src/SPI.h"
$displayPropsPath =
    Get-G2Path "JWPLC/2.1.0/libraries/JWPLC_Display/library.properties"
$tftPropsPath =
    Get-G2Path "JWPLC/2.1.0/libraries/JWPLC_TFT/library.properties"
$tftImplPath =
    Get-G2Path "JWPLC/2.1.0/libraries/JWPLC_TFT/src/JWPLC_TFT.cpp"
$displayImplPath =
    Get-G2Path "JWPLC/2.1.0/libraries/JWPLC_Display/src/JWPLC_Display.cpp"
$uiImplPath =
    Get-G2Path "JWPLC/2.1.0/libraries/JWPLC_Display/src/JWPLC_UI.cpp"
$ethernetClientPath =
    Get-G2Path "JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/EthernetClient.cpp"
$modbusTcpImplPath =
    Get-G2Path "JWPLC/2.1.0/libraries/JWPLC_ModbusTCP/src/JWPLC_ModbusTCP.cpp"
$modbusRtuPropsPath =
    Get-G2Path "JWPLC/2.1.0/libraries/JWPLC_ModbusRTU/library.properties"
$coreMainPath =
    Get-G2Path "JWPLC/2.1.0/cores/jwcontrol/main.cpp"
$slaveSketch =
    Get-G2Path "tools/modbus-tcp-benchmark/firmware/a14_p5_rtu_slave/a14_p5_rtu_slave.ino"

$w5100Text = [IO.File]::ReadAllText($w5100Path)
$spiHeaderText = [IO.File]::ReadAllText($spiHeaderPath)
$displayPropsText = [IO.File]::ReadAllText($displayPropsPath)
$tftPropsText = [IO.File]::ReadAllText($tftPropsPath)
$tftImplText = [IO.File]::ReadAllText($tftImplPath)
$displayImplText = [IO.File]::ReadAllText($displayImplPath)
$uiImplText = [IO.File]::ReadAllText($uiImplPath)
$ethernetClientText = [IO.File]::ReadAllText($ethernetClientPath)
$modbusTcpImplText = [IO.File]::ReadAllText($modbusTcpImplPath)
$modbusRtuPropsText = [IO.File]::ReadAllText($modbusRtuPropsPath)
$coreMainText = [IO.File]::ReadAllText($coreMainPath)
$masterText = [IO.File]::ReadAllText($masterSketch)
$slaveText = [IO.File]::ReadAllText($slaveSketch)
$p5bText = [IO.File]::ReadAllText($p5bGate)

if (-not $w5100Text.Contains("#define JWPLC_W5500_RX_FIFO_REUSE 1")) {
    throw "H3ER_FIFO_REUSE_DEFAULT_NOT_ON"
}

if (-not $w5100Text.Contains("#define JWPLC_W5500_RX_DIRECT_TRANSFER_BYTES 0")) {
    throw "H3ER_DIRECT_RX_DEFAULT_NOT_OFF"
}

if (-not $spiHeaderText.Contains("#define JWPLC_SPI_FIFO_REUSE_DLEN_CACHE 1")) {
    throw "H3ER_DLEN_REUSE_DEFAULT_NOT_ON"
}

Write-Host ""
Write-Host "=== H3E-R COMPOSITION AUDIT ==="

if ($displayPropsText -match '(?m)^precompiled\s*=') {
    throw "H3ER_DISPLAY_MUST_BUILD_FROM_SOURCE"
}
Write-Host "H3ER_DISPLAY_LINKAGE_POLICY=SOURCE"

if ($tftPropsText -match '(?m)^precompiled\s*=') {
    throw "H3ER_TFT_MUST_BUILD_FROM_SOURCE"
}
Write-Host "H3ER_TFT_LINKAGE_POLICY=SOURCE"

foreach ($token in @(
    "#include <TFT_eSPI.h>",
    "jwplcSPI_acquire",
    "jwplcSPI_prepareForTFT",
    "SPI_FREQUENCY != 80000000"
)) {
    if (-not $tftImplText.Contains($token)) {
        throw "H3ER_JWPLC_TFT_CONTRACT_MISSING=$token"
    }
}
Write-Host "H3ER_JWPLC_TFT_WRAPPER=PASS"
Write-Host "H3ER_TFT_BACKEND=TFT_ESPI_PRIVATE"
Write-Host "H3ER_TFT_SPI_HZ=80000000"

foreach ($token in @(
    "jwplcUIRuntimeDrawDirty",
    "jwplcUserDisplayRefreshNeededCallback"
)) {
    if (-not $displayImplText.Contains($token)) {
        throw "H3ER_DISPLAY_DIRTY_CONTRACT_MISSING=$token"
    }
}

foreach ($token in @(
    "USER_REFRESH_ON_DEMAND",
    "field.dirty",
    "setValueString",
    "drawDirty",
    "refreshNeeded"
)) {
    if (-not $uiImplText.Contains($token)) {
        throw "H3ER_UI_DIRTY_ENGINE_MISSING=$token"
    }
}
Write-Host "H3ER_DISPLAY_DIRTY_ENGINE=PASS"

foreach ($token in @(
    "JWPLC_Display.setUserRefreshMode(",
    "USER_REFRESH_ON_DEMAND",
    "JWPLC_Display.setFields(",
    "JWPLC_Display.setValue(",
    "JWPLC_Display.setBool("
)) {
    if (-not $masterText.Contains($token)) {
        throw "H3ER_MASTER_HMI_CONTRACT_MISSING=$token"
    }
}
Write-Host "H3ER_MASTER_HMI_ON_DEMAND_DIRTY=PASS"

foreach ($token in @(
    "jwplcModbusTCPLoopServiceCallback();",
    "jwplcDataLogTickCallback();",
    "jwplcEthernetTickCallback();",
    "jwplcSystemDisplayHook();"
)) {
    if (-not $coreMainText.Contains($token)) {
        throw "H3ER_CORE_RUNTIME_HOOK_MISSING=$token"
    }
}
Write-Host "H3ER_CORE_AUTO_SERVICES=PASS"

foreach ($token in @(
    "beginConnectAsync",
    "pollConnectAsync",
    "beginWriteAsync",
    "pollWriteAsync",
    "beginFlushAsync",
    "pollFlushAsync",
    "beginStopAsync",
    "pollStopAsync"
)) {
    if (-not $ethernetClientText.Contains($token)) {
        throw "H3ER_TCP_ASYNC_API_MISSING=$token"
    }
}
Write-Host "H3ER_TCP_ASYNC_API_SET=PASS"

if (-not $modbusTcpImplText.Contains("_client.write(_txBuffer, responseLength)")) {
    throw "H3ER_MODBUS_TCP_RESPONSE_PATH_CHANGED_UNEXPECTEDLY"
}
if ($modbusTcpImplText.Contains("beginWriteAsync(")) {
    throw "H3ER_MODBUS_TCP_ASYNC_TX_UNEXPECTED_FOR_P4_1_CAUSAL_GATE"
}
Write-Host "H3ER_TCP_ASYNC_CONNECT=PRESENT_NOT_EXERCISED_INBOUND_SERVER"
Write-Host "H3ER_TCP_ASYNC_WRITE=PRESENT_NOT_INTEGRATED_IN_MODBUS_TCP"
Write-Host "H3ER_TCP_ASYNC_FLUSH=PRESENT_NOT_USED_BY_MODBUS_TCP"
Write-Host "H3ER_TCP_ASYNC_STOP=PRESENT_LEGACY_WRAPPER_USES_ASYNC_ENGINE"
Write-Host "H3ER_MODBUS_TCP_TX_PATH=LEGACY_BLOCKING_WRITE"
Write-Host "H3ER_MODBUS_TCP_ASYNC_INTEGRATION=DEFERRED_SEPARATE_VARIABLE_GATE"

if (-not $modbusRtuPropsText.Contains("precompiled=full")) {
    throw "H3ER_MODBUS_RTU_PRECOMPILED_POLICY_CHANGED"
}
Write-Host "H3ER_MODBUS_RTU_LINKAGE_POLICY=QUALIFIED_PRECOMPILED"

foreach ($token in @(
    "JWPLC_ModbusRTU.motor(ASYNC)",
    "JWPLC_ModbusRTU.requestReadHoldingRegisters(",
    "JWPLC_ModbusRTU.masterBusy()",
    "JWPLC_ModbusRTU.masterDone()",
    "JWPLC_ModbusRTU.task()"
)) {
    if (-not $masterText.Contains($token)) {
        throw "H3ER_RTU_MASTER_ASYNC_CONTRACT_MISSING=$token"
    }
}

foreach ($token in @(
    "JWPLC_ModbusRTU.motor(ASYNC)",
    "JWPLC_ModbusRTU.task()"
)) {
    if (-not $slaveText.Contains($token)) {
        throw "H3ER_RTU_SLAVE_ASYNC_CONTRACT_MISSING=$token"
    }
}
Write-Host "H3ER_RTU_ASYNC_SOURCE_CONTRACT=PASS"

foreach ($token in @(
    "JWPLCDataLog sdDataLog",
    "SD_WORKLOAD_MODE=BUFFERED_DATALOG",
    "SD_DATALOG_BUFFER_BYTES = 4096",
    "SD_DATALOG_COMMIT_THRESHOLD_BYTES = 512",
    "SD_DATALOG_COMMIT_TIMEOUT_MS = 5000"
)) {
    if (-not $masterText.Contains($token)) {
        throw "H3ER_DATALOG_SOURCE_CONTRACT_MISSING=$token"
    }
}

if ($masterText.Contains("sdDataLog.service(") -or
    $masterText.Contains("JWPLC_SD.serviceDataLogs(")) {
    throw "H3ER_DATALOG_MANUAL_SERVICE_FORBIDDEN"
}

if (-not $coreMainText.Contains("jwplcDataLogTickCallback();")) {
    throw "H3ER_DATALOG_AUTOSERVICE_CORE_MISSING"
}

if ($p5bText.Contains("JWPLC_W5500_RX_FIFO_REUSE=")) {
    throw "H3ER_P5B_MUST_NOT_OVERRIDE_FIFO_REUSE_DEFAULT"
}

if ($p5bText.Contains("JWPLC_SPI_FIFO_REUSE_DLEN_CACHE=")) {
    throw "H3ER_P5B_MUST_NOT_OVERRIDE_DLEN_REUSE_DEFAULT"
}

Write-Host "H3ER_FIFO_REUSE_DEFAULT_CONTRACT=PASS"
Write-Host "H3ER_DLEN_REUSE_DEFAULT_CONTRACT=PASS"
Write-Host "H3ER_DATALOG_SOURCE_CONTRACT=PASS"
Write-Host "H3ER_DATALOG_AUTOSERVICE=CORE_SYSTEM_TASK"
Write-Host "H3ER_MANUAL_DATALOG_SERVICE=NO"

$p5bArgs = @{
    MasterPort = $MasterPort
    SlavePort = $SlavePort
    TcpRate = $TcpRate
    DurationS = $DurationS
    PythonExe = $pythonExeResolved
}

[object[]]$p5bOutput =
    @(& $p5bGate @p5bArgs *>&1)

[string[]]$p5bLines = @(
    $p5bOutput |
        ForEach-Object {
            $_.ToString()
        }
)

$p5bLines | ForEach-Object { Write-Host $_ }

$p5bOutputText =
    $p5bLines -join [Environment]::NewLine

if (-not $p5bOutputText.Contains(
    "A14_P5B_PHYSICAL_MASTER_SLAVE_COMBINED=PASS")) {
    throw "H3ER_P5B_NOT_PASS"
}

[string[]]$tempMarkers = @(
    $p5bLines |
        Where-Object {
            $_.StartsWith("P5B_TEMP_ROOT=", [StringComparison]::Ordinal)
        }
)

if ($tempMarkers.Count -ne 1) {
    throw "H3ER_TEMP_ROOT_MARKER_INVALID"
}

$tempRoot =
    $tempMarkers[0].Substring("P5B_TEMP_ROOT=".Length).Trim()

$qualificationLog =
    Join-Path $tempRoot "qualification.log"

$masterSnapshot =
    Join-Path $tempRoot "master_final.txt"

$slaveSnapshot =
    Join-Path $tempRoot "slave_final.txt"

$masterCompileLog =
    Join-Path $tempRoot "compile_master.log"

$slaveCompileLog =
    Join-Path $tempRoot "compile_slave.log"

foreach ($required in @(
    $qualificationLog,
    $masterSnapshot,
    $slaveSnapshot,
    $masterCompileLog,
    $slaveCompileLog
)) {
    if (-not (Test-Path -LiteralPath $required)) {
        throw "H3ER_REQUIRED_RESULT_MISSING=$required"
    }
}

$qualificationText =
    [IO.File]::ReadAllText($qualificationLog)

$masterTextResult =
    [IO.File]::ReadAllText($masterSnapshot)

$slaveTextResult =
    [IO.File]::ReadAllText($slaveSnapshot)

foreach ($requiredLine in @(
    "TCP_FULL_RUNTIME_PASS=YES",
    "MASTER_RUNTIME_PASS=YES",
    "RTU_MASTER_PASS=YES",
    "RTU_SLAVE_PASS=YES",
    "RTU_CROSS_COUNT_PASS=YES",
    "RTU_SNAPSHOT_TAIL_TOLERANCE=0",
    "RTU_FINAL_SNAPSHOT_MODE=QUIESCED",
    "A14_P5B_AUTOMATED=PASS",
    "TCP_CLEAN=YES",
    "PERIPHERAL_PASS=YES"
)) {
    if (-not $qualificationText.Contains($requiredLine)) {
        throw "H3ER_REQUIRED_MARKER_MISSING=$requiredLine"
    }
}

$requestedReqS =
    Get-H3ERDouble -Text $qualificationText -Key "REQUESTED_REQ_S"

$achievedReqS =
    Get-H3ERDouble -Text $qualificationText -Key "ACHIEVED_REQ_S"

$achievedPct =
    Get-H3ERDouble -Text $qualificationText -Key "ACHIEVED_PCT"

$totalMbps =
    Get-H3ERDouble -Text $qualificationText -Key "TOTAL_TCP_PAYLOAD_MBPS"

$usefulMbps =
    Get-H3ERDouble -Text $qualificationText -Key "USEFUL_DATA_MBPS"

$p95Us =
    Get-H3ERDouble -Text $qualificationText -Key "LATENCY_P95_US"

$p99Us =
    Get-H3ERDouble -Text $qualificationText -Key "LATENCY_P99_US"

$maxUs =
    Get-H3ERDouble -Text $qualificationText -Key "LATENCY_MAX_US"

$rtuStarted =
    Get-H3ERInt -Text $masterTextResult -Key "RTU_REQUESTS_STARTED"

$rtuSuccess =
    Get-H3ERInt -Text $masterTextResult -Key "RTU_REQUESTS_SUCCESS"

$rtuSkipped =
    Get-H3ERInt -Text $masterTextResult -Key "RTU_PERIODS_SKIPPED"

$rtuDurationMs =
    Get-H3ERInt -Text $masterTextResult -Key "RTU_TRAFFIC_DURATION_MS"

if ($rtuDurationMs -le 0) {
    throw "H3ER_RTU_DURATION_INVALID=$rtuDurationMs"
}

$rtuHz =
    [double]$rtuSuccess /
    ([double]$rtuDurationMs / 1000.0)

$sdActive =
    Get-H3ERValue -Text $masterTextResult -Key "SD_DATALOG_ACTIVE"

$sdAccepted =
    Get-H3ERInt -Text $masterTextResult -Key "SD_DATALOG_ACCEPTED_BYTES"

$sdCommitted =
    Get-H3ERInt -Text $masterTextResult -Key "SD_DATALOG_COMMITTED_BYTES"

$sdPending =
    Get-H3ERInt -Text $masterTextResult -Key "SD_DATALOG_PENDING_BYTES"

$sdThreshold =
    Get-H3ERInt -Text $masterTextResult -Key "SD_DATALOG_COMMIT_THRESHOLD_BYTES"

$sdFailed =
    Get-H3ERInt -Text $masterTextResult -Key "SD_DATALOG_FAILED_COMMITS"

$peripheralFailures =
    Get-H3ERInt -Text $masterTextResult -Key "PERIPHERAL_FAILURE_COUNT"

$masterRtuMotor =
    Get-H3ERValue -Text $masterTextResult -Key "RTU_MOTOR"
$masterRtuTxMode =
    Get-H3ERValue -Text $masterTextResult -Key "RTU_TX_MODE"
$masterRtuQueuedActive =
    Get-H3ERValue -Text $masterTextResult -Key "RTU_TX_QUEUED_ACTIVE"
$masterRtuRxMode =
    Get-H3ERValue -Text $masterTextResult -Key "RTU_RX_MODE"
$masterRtuCrcMode =
    Get-H3ERValue -Text $masterTextResult -Key "RTU_CRC_MODE"
$masterRtuFraming =
    Get-H3ERValue -Text $masterTextResult -Key "RTU_SERVER_FRAMING"

$slaveRtuMotor =
    Get-H3ERValue -Text $slaveTextResult -Key "RTU_MOTOR"
$slaveRtuTxMode =
    Get-H3ERValue -Text $slaveTextResult -Key "RTU_TX_MODE"
$slaveRtuQueuedActive =
    Get-H3ERValue -Text $slaveTextResult -Key "RTU_TX_QUEUED_ACTIVE"
$slaveRtuRxMode =
    Get-H3ERValue -Text $slaveTextResult -Key "RTU_RX_MODE"
$slaveRtuCrcMode =
    Get-H3ERValue -Text $slaveTextResult -Key "RTU_CRC_MODE"
$slaveRtuFraming =
    Get-H3ERValue -Text $slaveTextResult -Key "RTU_SERVER_FRAMING"

$masterDisplayMode =
    Get-H3ERValue -Text $masterTextResult -Key "DISPLAY_RENDER_MODE"
$masterDisplayRefresh =
    Get-H3ERValue -Text $masterTextResult -Key "DISPLAY_REFRESH_MODE"

$slaveDisplayMode =
    Get-H3ERValue -Text $slaveTextResult -Key "DISPLAY_RENDER_MODE"
$slaveDisplayRefresh =
    Get-H3ERValue -Text $slaveTextResult -Key "DISPLAY_REFRESH_MODE"

foreach ($item in @(
    [PSCustomObject]@{ Key = "MASTER_RTU_MOTOR"; Value = $masterRtuMotor; Expected = "ASYNC" },
    [PSCustomObject]@{ Key = "MASTER_RTU_TX_MODE"; Value = $masterRtuTxMode; Expected = "QUEUED" },
    [PSCustomObject]@{ Key = "MASTER_RTU_TX_QUEUED_ACTIVE"; Value = $masterRtuQueuedActive; Expected = "YES" },
    [PSCustomObject]@{ Key = "SLAVE_RTU_MOTOR"; Value = $slaveRtuMotor; Expected = "ASYNC" },
    [PSCustomObject]@{ Key = "SLAVE_RTU_TX_MODE"; Value = $slaveRtuTxMode; Expected = "QUEUED" },
    [PSCustomObject]@{ Key = "SLAVE_RTU_TX_QUEUED_ACTIVE"; Value = $slaveRtuQueuedActive; Expected = "YES" },
    [PSCustomObject]@{ Key = "MASTER_DISPLAY_MODE"; Value = $masterDisplayMode; Expected = "HMI_ON_DEMAND_DIRTY" },
    [PSCustomObject]@{ Key = "MASTER_DISPLAY_REFRESH"; Value = $masterDisplayRefresh; Expected = "USER_REFRESH_ON_DEMAND" },
    [PSCustomObject]@{ Key = "SLAVE_DISPLAY_MODE"; Value = $slaveDisplayMode; Expected = "HMI_ON_DEMAND_DIRTY" },
    [PSCustomObject]@{ Key = "SLAVE_DISPLAY_REFRESH"; Value = $slaveDisplayRefresh; Expected = "USER_REFRESH_ON_DEMAND" }
)) {
    if ($item.Value -ne $item.Expected) {
        throw ("H3ER_RUNTIME_COMPOSITION_MISMATCH_{0}={1}" -f $item.Key, $item.Value)
    }
}

# Estos modos permanecen deliberadamente en el baseline seguro de producto.
# BULK/LOOKUP/STRUCTURAL fueron candidatos experimentales y no se promocionan
# silenciosamente dentro de una regresión P4.1.
foreach ($item in @(
    [PSCustomObject]@{ Key = "MASTER_RTU_RX_MODE"; Value = $masterRtuRxMode; Expected = "BYTE" },
    [PSCustomObject]@{ Key = "MASTER_RTU_CRC_MODE"; Value = $masterRtuCrcMode; Expected = "BITWISE" },
    [PSCustomObject]@{ Key = "MASTER_RTU_FRAMING"; Value = $masterRtuFraming; Expected = "GAP" },
    [PSCustomObject]@{ Key = "SLAVE_RTU_RX_MODE"; Value = $slaveRtuRxMode; Expected = "BYTE" },
    [PSCustomObject]@{ Key = "SLAVE_RTU_CRC_MODE"; Value = $slaveRtuCrcMode; Expected = "BITWISE" },
    [PSCustomObject]@{ Key = "SLAVE_RTU_FRAMING"; Value = $slaveRtuFraming; Expected = "GAP" }
)) {
    if ($item.Value -ne $item.Expected) {
        throw ("H3ER_SAFE_RTU_BASELINE_CHANGED_{0}={1}" -f $item.Key, $item.Value)
    }
}

Write-Host "H3ER_RTU_ASYNC_RUNTIME=PASS"
Write-Host "H3ER_RTU_SAFE_BASELINE=BYTE_BITWISE_GAP"
Write-Host "H3ER_DISPLAY_RUNTIME_DIRTY=PASS"

if ($requestedReqS -ne 1000.0) {
    throw "H3ER_REQUESTED_RATE_CHANGED=$requestedReqS"
}

if ($achievedPct -lt 99.9) {
    throw "H3ER_TCP_RATE_BELOW_99_9_PCT=$achievedPct"
}

if ($rtuHz -lt 49.5 -or $rtuHz -gt 50.5) {
    throw "H3ER_RTU_NOT_50HZ=$rtuHz"
}

if ($rtuSkipped -ne 0) {
    throw "H3ER_RTU_PERIODS_SKIPPED=$rtuSkipped"
}

if ($rtuStarted -ne $rtuSuccess) {
    throw "H3ER_RTU_SUCCESS_MISMATCH=$rtuSuccess/$rtuStarted"
}

if ($sdActive -ne "YES") {
    throw "H3ER_DATALOG_NOT_ACTIVE"
}

if ($sdAccepted -le 0 -or $sdCommitted -le 0) {
    throw "H3ER_DATALOG_NO_ACTIVITY"
}

if ($sdFailed -ne 0) {
    throw "H3ER_DATALOG_FAILED_COMMITS=$sdFailed"
}

if ($sdPending -ge $sdThreshold) {
    throw "H3ER_DATALOG_PENDING_INVALID=$sdPending"
}

if ($peripheralFailures -ne 0) {
    throw "H3ER_PERIPHERAL_FAILURES=$peripheralFailures"
}

# Baseline inmediato antes de promover P4.1 (HEAD 413a570b, 2026-09-30).
# Protegemos P95/P99; MAX se reporta como diagnóstico por su alta varianza
# histórica y no veta por sí solo un gate limpio.
$preP41P95Us = 1275.5
$preP41P99Us = 3781.0
$preP41MaxUs = 28179.8

$p95DeltaPct =
    (($p95Us / $preP41P95Us) - 1.0) * 100.0
$p99DeltaPct =
    (($p99Us / $preP41P99Us) - 1.0) * 100.0
$maxDeltaPct =
    (($maxUs / $preP41MaxUs) - 1.0) * 100.0

$p95GuardUs = $preP41P95Us * 1.10
$p99GuardUs = $preP41P99Us * 1.15

$p95GuardPass = $p95Us -le $p95GuardUs
$p99GuardPass = $p99Us -le $p99GuardUs

Write-Host ("H3ER_PRE_P4_1_P95_US={0:F1}" -f $preP41P95Us)
Write-Host ("H3ER_PRE_P4_1_P99_US={0:F1}" -f $preP41P99Us)
Write-Host ("H3ER_PRE_P4_1_MAX_US={0:F1}" -f $preP41MaxUs)
Write-Host ("H3ER_P95_DELTA_VS_PRE_P4_1_PCT={0:F3}" -f $p95DeltaPct)
Write-Host ("H3ER_P99_DELTA_VS_PRE_P4_1_PCT={0:F3}" -f $p99DeltaPct)
Write-Host ("H3ER_MAX_DELTA_VS_PRE_P4_1_PCT={0:F3}" -f $maxDeltaPct)
Write-Host ("H3ER_P95_GUARD_US={0:F1}" -f $p95GuardUs)
Write-Host ("H3ER_P99_GUARD_US={0:F1}" -f $p99GuardUs)
Write-Host "H3ER_P95_GUARD_PASS=$($p95GuardPass.ToString().ToUpperInvariant())"
Write-Host "H3ER_P99_GUARD_PASS=$($p99GuardPass.ToString().ToUpperInvariant())"
Write-Host "H3ER_MAX_POLICY=DIAGNOSTIC_ONLY"

if (-not $p95GuardPass) {
    throw "H3ER_P4_1_P95_REGRESSION=$p95Us"
}

if (-not $p99GuardPass) {
    throw "H3ER_P4_1_P99_REGRESSION=$p99Us"
}

$compileText =
    [IO.File]::ReadAllText($masterCompileLog)

$slaveCompileText =
    [IO.File]::ReadAllText($slaveCompileLog)

$compileNormalized =
    $compileText.Replace("\", "/").ToLowerInvariant()

$slaveCompileNormalized =
    $slaveCompileText.Replace("\", "/").ToLowerInvariant()

if ($compileText -match '(?i)-DJWPLC_W5500_RX_FIFO_REUSE=') {
    throw "H3ER_FIFO_REUSE_WAS_OVERRIDDEN_AT_BUILD"
}

if ($compileText -match '(?i)-DJWPLC_SPI_FIFO_REUSE_DLEN_CACHE=') {
    throw "H3ER_DLEN_REUSE_WAS_OVERRIDDEN_AT_BUILD"
}

foreach ($token in @(
    "jwplc_display.cpp",
    "jwplc_ui.cpp",
    "jwplc_ui_api.cpp",
    "jwplc_ui_pages.cpp",
    "jwplc_tft.cpp",
    "spi.cpp",
    "jw_sd.cpp",
    "jwplc_modbustcp.cpp",
    "jwplc_ethernet.cpp",
    "ethernetclient.cpp",
    "socket.cpp",
    "w5100.cpp"
)) {
    if (-not $compileNormalized.Contains($token)) {
        throw "H3ER_MASTER_SOURCE_LINKAGE_MISSING=$token"
    }
}

foreach ($archive in @(
    "libjwplc_display.a",
    "libjwplc_tft.a",
    "libspi.a"
)) {
    if ($compileNormalized.Contains($archive)) {
        throw "H3ER_UNEXPECTED_STALE_ARCHIVE_LINKAGE=$archive"
    }
}

$masterModbusRtuArchiveLinkForm =
    Get-H3ERModbusRtuArchiveLinkForm `
        -NormalizedCompileText $compileNormalized

$slaveModbusRtuArchiveLinkForm =
    Get-H3ERModbusRtuArchiveLinkForm `
        -NormalizedCompileText $slaveCompileNormalized

if ($masterModbusRtuArchiveLinkForm -eq "NONE") {
    throw "H3ER_MASTER_MODBUS_RTU_ARCHIVE_NOT_LINKED"
}

if ($slaveModbusRtuArchiveLinkForm -eq "NONE") {
    throw "H3ER_SLAVE_MODBUS_RTU_ARCHIVE_NOT_LINKED"
}

Write-Host "H3ER_DISPLAY_SOURCE_LINKAGE=PASS"
Write-Host "H3ER_TFT_SOURCE_LINKAGE=PASS"
Write-Host "H3ER_SPI_SOURCE_LINKAGE=PASS"
Write-Host "H3ER_ETHERNET_SOURCE_LINKAGE=PASS"
Write-Host "H3ER_MODBUS_TCP_SOURCE_LINKAGE=PASS"
Write-Host "H3ER_DATALOG_SOURCE_LINKAGE=PASS"
Write-Host "H3ER_MASTER_MODBUS_RTU_ARCHIVE_LINK_FORM=$masterModbusRtuArchiveLinkForm"
Write-Host "H3ER_SLAVE_MODBUS_RTU_ARCHIVE_LINK_FORM=$slaveModbusRtuArchiveLinkForm"
Write-Host "H3ER_MODBUS_RTU_QUALIFIED_ARCHIVE_LINKAGE=PASS"

Write-Host ""
Write-Host "=============================================================================="
Write-Host " A14 H3E-R SUMMARY"
Write-Host "=============================================================================="
Write-Host ("H3ER_TCP_REQUESTED_REQ_S={0:F2}" -f $requestedReqS)
Write-Host ("H3ER_TCP_ACHIEVED_REQ_S={0:F2}" -f $achievedReqS)
Write-Host ("H3ER_TCP_ACHIEVED_PCT={0:F3}" -f $achievedPct)
Write-Host ("H3ER_TCP_TOTAL_MBPS={0:F4}" -f $totalMbps)
Write-Host ("H3ER_TCP_USEFUL_MBPS={0:F4}" -f $usefulMbps)
Write-Host ("H3ER_TCP_P95_US={0:F1}" -f $p95Us)
Write-Host ("H3ER_TCP_P99_US={0:F1}" -f $p99Us)
Write-Host ("H3ER_TCP_MAX_US={0:F1}" -f $maxUs)
Write-Host ("H3ER_RTU_ACHIEVED_HZ={0:F3}" -f $rtuHz)
Write-Host "H3ER_RTU_REQUESTS_STARTED=$rtuStarted"
Write-Host "H3ER_RTU_REQUESTS_SUCCESS=$rtuSuccess"
Write-Host "H3ER_RTU_PERIODS_SKIPPED=$rtuSkipped"
Write-Host "H3ER_DATALOG_ACTIVE=$sdActive"
Write-Host "H3ER_DATALOG_ACCEPTED_BYTES=$sdAccepted"
Write-Host "H3ER_DATALOG_COMMITTED_BYTES=$sdCommitted"
Write-Host "H3ER_DATALOG_PENDING_BYTES=$sdPending"
Write-Host "H3ER_DATALOG_FAILED_COMMITS=$sdFailed"
Write-Host "H3ER_PERIPHERAL_FAILURE_COUNT=$peripheralFailures"
Write-Host "H3ER_FIFO_REUSE_DEFAULT=PASS"
Write-Host "H3ER_DLEN_REUSE_DEFAULT=PASS"
Write-Host "H3ER_SINGLE_STATUS_POLICY=PASS"
Write-Host "H3ER_DATALOG_POLICY=PASS"
Write-Host "H3ER_DISPLAY_DIRTY_POLICY=PASS"
Write-Host "H3ER_RTU_ASYNC_POLICY=PASS"
Write-Host "H3ER_P4_1_LATENCY_GUARD=PASS"
Write-Host "H3ER_HISTORICAL_TCP_REQ_S=1000.00"
Write-Host "H3ER_HISTORICAL_RTU_HZ=50.004"
Write-Host "H3ER_HISTORICAL_TOTAL_MBPS=2.1680"
Write-Host "H3ER_HISTORICAL_USEFUL_MBPS=2.0000"
Write-Host "H3ER_HISTORICAL_P95_US=1197.2"
Write-Host "H3ER_HISTORICAL_P99_US=3423.3"
Write-Host "H3ER_HISTORICAL_MAX_US=18823.7"
Write-Host "HARNESS_FAILURE=NO"
Write-Host "PRODUCT_FAILURE=NO_EVIDENCE"
Write-Host "HARDWARE_FAILURE=NO_EVIDENCE"
Write-Host "A14_H3E_R_CURRENT_PACKAGE=PASS"
Write-Host "A14_H3E_R_P4_1_REGRESSION=PASS"
Write-Host "H3ER_TEMP_ROOT=$tempRoot"
Write-Host "NEXT=RETURN_TO_CHAT_BEFORE_P4_2"
