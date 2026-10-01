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

function Convert-H3ERDouble {
    param([Parameter(Mandatory = $true)][string]$Value)

    return [double]::Parse(
        $Value,
        [Globalization.CultureInfo]::InvariantCulture)
}

function Convert-H3ERInt {
    param([Parameter(Mandatory = $true)][string]$Value)

    return [int64]::Parse(
        $Value,
        [Globalization.CultureInfo]::InvariantCulture)
}

function Get-H3ERDeltaPct {
    param(
        [Parameter(Mandatory = $true)][double]$Value,
        [Parameter(Mandatory = $true)][double]$Reference
    )

    if ($Reference -eq 0.0) {
        throw "H3ER_DELTA_REFERENCE_ZERO"
    }

    return (($Value / $Reference) - 1.0) * 100.0
}

function Get-H3ERLongBuckets {
    param([Parameter(Mandatory = $true)][string]$Text)

    $pattern =
        '(?m)^LONG_BUCKET ' +
        'INDEX=(\d+) ' +
        'START_S=([0-9.]+) ' +
        'END_S=([0-9.]+) ' +
        'REQ_S=([0-9.]+) ' +
        'OK=(\d+) ' +
        'LAT_AVG_US=([0-9.]+) ' +
        'P95_US=([0-9.]+) ' +
        'P99_US=([0-9.]+) ' +
        'MAX_US=([0-9.]+)\r?$'

    return @(
        [regex]::Matches($Text, $pattern) |
            ForEach-Object {
                [PSCustomObject]@{
                    Index = (Convert-H3ERInt -Value $_.Groups[1].Value)
                    StartS = (Convert-H3ERDouble -Value $_.Groups[2].Value)
                    EndS = (Convert-H3ERDouble -Value $_.Groups[3].Value)
                    ReqS = (Convert-H3ERDouble -Value $_.Groups[4].Value)
                    Ok = (Convert-H3ERInt -Value $_.Groups[5].Value)
                    AvgUs = (Convert-H3ERDouble -Value $_.Groups[6].Value)
                    P95Us = (Convert-H3ERDouble -Value $_.Groups[7].Value)
                    P99Us = (Convert-H3ERDouble -Value $_.Groups[8].Value)
                    MaxUs = (Convert-H3ERDouble -Value $_.Groups[9].Value)
                }
            }
    )
}

function Get-H3ERModbusRtuBuildEvidence {
    param(
        [Parameter(Mandatory = $true)]
        [string]$NormalizedCompileText
    )

    $sourcePath =
        "/libraries/jwplc_modbusrtu/src/jwplc_modbusrtu.cpp"
    $sourceObject =
        "/libraries/jwplc_modbusrtu/jwplc_modbusrtu.cpp.o"
    $archiveDirectory =
        "/libraries/jwplc_modbusrtu/src/esp32"
    $canonicalText =
        [regex]::Replace($NormalizedCompileText, '/+', '/')

    [string[]]$linkLines = @(
        [regex]::Matches(
            $canonicalText,
            '(?m)^.*-wl,--start-group.*$') |
            ForEach-Object { $_.Value }
    )

    $linkText = $linkLines -join "`n"
    $sourceCompiled =
        $canonicalText.Contains($sourcePath) -and
        [regex]::IsMatch(
            $canonicalText,
            '(?m)^.*jwplc_modbusrtu\.cpp"?\s+-o\s+"?.*jwplc_modbusrtu\.cpp\.o"?.*$')
    $sourceLinked =
        $linkLines.Count -eq 1 -and
        $linkText.Contains($sourceObject)
    $archiveLinkedDirect =
        $linkText.Contains(
            "$archiveDirectory/libjwplc_modbusrtu.a")
    $archiveLinkedByFlag =
        $linkText.Contains($archiveDirectory) -and
        [regex]::IsMatch(
            $linkText,
            '(?m)(?:^|\s)"?-ljwplc_modbusrtu"?(?=\s|$)')
    $precompiledMarker =
        [regex]::IsMatch(
            $canonicalText,
            '(?m)^using precompiled library .*jwplc_modbusrtu.*$')

    return [PSCustomObject]@{
        SourceCompiled = $sourceCompiled
        SourceLinked = $sourceLinked
        ArchiveLinked =
            $archiveLinkedDirect -or $archiveLinkedByFlag
        PrecompiledMarker = $precompiledMarker
    }
}

Write-Host "=============================================================================="
Write-Host " A14 H3E-R - POST-P4.2 CURRENT PACKAGE FULL RUNTIME"
Write-Host " TCP 1000 req/s + RTU 50 Hz + DATALOG + FULL PERIPHERALS"
Write-Host "=============================================================================="

Assert-G2Branch

$repo = $script:G2RepoRoot
$head = Get-G2Head
$dirty = @(Get-G2TrackedDirtyPaths)
$staged = @(& git -C $repo diff --cached --name-only)

Write-Host "H3ER_HEAD=$head"
Write-Host "H3ER_PHASE=POST_P4_2"
Write-Host "H3ER_MASTER_PORT=$MasterPort"
Write-Host "H3ER_SLAVE_PORT=$SlavePort"
Write-Host ("H3ER_TCP_TARGET_REQ_S={0:F0}" -f $TcpRate)
Write-Host ("H3ER_DURATION_S={0:F0}" -f $DurationS)
Write-Host "H3ER_RTU_TARGET_HZ=50"
Write-Host "H3ER_RTU_PERIOD_US=20000"
Write-Host "H3ER_W5500_SPI_HZ=$(Get-G2SpiHz)"
Write-Host "H3ER_FIFO_REUSE_SOURCE=PACKAGE_DEFAULT"
Write-Host "H3ER_DLEN_REUSE_SOURCE=PACKAGE_DEFAULT"
Write-Host "H3ER_COPY_OUT_64_SOURCE=PACKAGE_DEFAULT"
Write-Host "H3ER_DIRECT_RX_SOURCE=PACKAGE_DEFAULT"
Write-Host "H3ER_RX_COMMIT=IMMEDIATE"
Write-Host "H3ER_RX_COMMIT_PATH=LEGACY"
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

if ($DurationS -ne 120.0) {
    throw "H3ER_DURATION_MUST_BE_120S"
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
$qualificationRunnerPath =
    Get-G2Path "tools/modbus-tcp-benchmark/pc/a14_perf_full_runtime_realistic_1000rps_qualification.py"
$frontierRunnerPath =
    Get-G2Path "tools/modbus-tcp-benchmark/pc/a14_perf_fc03_125_formal_frontier.py"

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
$modbusRtuSourcePath =
    Get-G2Path "JWPLC/2.1.0/libraries/JWPLC_ModbusRTU/src/JWPLC_ModbusRTU.cpp"
$modbusRtuArchivePath =
    Get-G2Path "JWPLC/2.1.0/libraries/JWPLC_ModbusRTU/src/esp32/libJWPLC_ModbusRTU.a"
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
$qualificationRunnerText =
    [IO.File]::ReadAllText($qualificationRunnerPath)
$frontierRunnerText =
    [IO.File]::ReadAllText($frontierRunnerPath)

if (-not $w5100Text.Contains("#define JWPLC_W5500_RX_FIFO_REUSE 1")) {
    throw "H3ER_FIFO_REUSE_DEFAULT_NOT_ON"
}

if (-not $w5100Text.Contains("#define JWPLC_W5500_RX_DIRECT_TRANSFER_BYTES 0")) {
    throw "H3ER_DIRECT_RX_DEFAULT_NOT_OFF"
}

if (-not $spiHeaderText.Contains("#define JWPLC_SPI_FIFO_REUSE_DLEN_CACHE 1")) {
    throw "H3ER_DLEN_REUSE_DEFAULT_NOT_ON"
}

if (-not $spiHeaderText.Contains("#define JWPLC_SPI_FIFO_REUSE_COPY_OUT_64 1")) {
    throw "H3ER_COPY_OUT_64_DEFAULT_NOT_ON"
}

Write-Host "H3ER_COPY_OUT_64_DEFAULT=1"

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
    "JWPLC_FRAM.read(",
    "JWPLCDataLog sdDataLog",
    "jwplcGetRTCState()",
    "jwplcGetIOState()",
    "JWPLCButtons::isReady()",
    "probeSpiMutex()"
)) {
    if (-not $masterText.Contains($token)) {
        throw "H3ER_FULL_RUNTIME_PERIPHERAL_SOURCE_MISSING=$token"
    }
}
Write-Host "H3ER_FRAM_WORKLOAD=EXERCISED"
Write-Host "H3ER_RTC_WORKLOAD=EXERCISED"
Write-Host "H3ER_TCA_IO_WORKLOAD=EXERCISED_VIA_GLOBAL_PERIPHERALS"
Write-Host "H3ER_BUTTON_WORKLOAD=EXERCISED"
Write-Host "H3ER_SPI_PROBE_WORKLOAD=EXERCISED"

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
if (-not $modbusTcpImplText.Contains("_client.read(") -or
    -not $ethernetClientText.Contains(
        "return Ethernet.socketRecv(_sockindex, buf, size);")) {
    throw "H3ER_RX_COMMIT_IMMEDIATE_LEGACY_PATH_CHANGED"
}
if ($modbusTcpImplText.Contains("beginWriteAsync(")) {
    throw "H3ER_MODBUS_TCP_ASYNC_TX_UNEXPECTED_FOR_POST_P4_2_GATE"
}
Write-Host "H3ER_TCP_ASYNC_CONNECT=PRESENT_BUT_NOT_EXERCISED_INBOUND_SERVER"
Write-Host "H3ER_TCP_ASYNC_WRITE=PRESENT_BUT_NOT_EXERCISED_NOT_INTEGRATED_IN_MODBUS_TCP"
Write-Host "H3ER_TCP_ASYNC_FLUSH=PRESENT_BUT_NOT_EXERCISED_BY_MODBUS_TCP"
Write-Host "H3ER_TCP_ASYNC_STOP=PRESENT_LEGACY_WRAPPER_USES_ASYNC_ENGINE"
Write-Host "H3ER_MODBUS_TCP_TX_PATH=LEGACY_BLOCKING_WRITE"
Write-Host "H3ER_RX_COMMIT=IMMEDIATE"
Write-Host "H3ER_MODBUS_TCP_ASYNC_INTEGRATION=DEFERRED_SEPARATE_VARIABLE_GATE"

if ($modbusRtuPropsText -match '(?m)^\s*precompiled\s*=') {
    throw "H3ER_MODBUS_RTU_SOURCE_FIRST_POLICY_CHANGED"
}

if (-not (Test-Path -LiteralPath $modbusRtuSourcePath)) {
    throw "H3ER_MODBUS_RTU_SOURCE_MISSING"
}

$modbusRtuArchivePresent =
    Test-Path -LiteralPath $modbusRtuArchivePath

Write-Host "H3ER_MODBUS_RTU_POLICY=SOURCE_FIRST"
Write-Host "H3ER_MODBUS_RTU_PRECOMPILED_REQUIRED=NO"
Write-Host "H3ER_MODBUS_RTU_ARCHIVE_PRESENT=$($modbusRtuArchivePresent.ToString().ToUpperInvariant())"
Write-Host "H3ER_MODBUS_RTU_ARCHIVE_PRESENCE_NOT_EQUAL_LINKAGE=PASS"
Write-Host "H3ER_MODBUS_RTU_POLICY_FIX=PASS"

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

if ($p5bText.Contains("JWPLC_SPI_FIFO_REUSE_COPY_OUT_64=")) {
    throw "H3ER_P5B_MUST_NOT_OVERRIDE_COPY_OUT_64_DEFAULT"
}

$formalLoopIndex =
    $frontierRunnerText.IndexOf("for i in range(target_requests):")
$formalWindowEndIndex =
    $frontierRunnerText.IndexOf("unexpected_resets += (", $formalLoopIndex)
$finalSnapshotIndex =
    $frontierRunnerText.IndexOf('ser.write(b"S\n")', $formalWindowEndIndex)

if ($formalLoopIndex -lt 0 -or
    $formalWindowEndIndex -le $formalLoopIndex -or
    $finalSnapshotIndex -le $formalWindowEndIndex) {
    throw "H3ER_SERIAL_WINDOW_LIFECYCLE_NOT_PROVEN"
}

$formalLoopText =
    $frontierRunnerText.Substring(
        $formalLoopIndex,
        $formalWindowEndIndex - $formalLoopIndex)

if ($formalLoopText.Contains("collect_snapshot(") -or
    $formalLoopText.Contains('ser.write(b"S')) {
    throw "H3ER_PERIODIC_SERIAL_SNAPSHOT_FORBIDDEN"
}

if (-not $qualificationRunnerText.Contains("frontier.print_result(") -or
    -not $frontierRunnerText.Contains('"LONG_BUCKET "') -or
    -not $frontierRunnerText.Contains("QUANTITY = 125")) {
    throw "H3ER_LONG_BUCKET_PRODUCER_CONTRACT_MISSING"
}

Write-Host "H3ER_TCP_FUNCTION=FC03"
Write-Host "H3ER_TCP_QUANTITY_REGISTERS=125"

Write-Host "H3ER_FIFO_REUSE_DEFAULT_CONTRACT=PASS"
Write-Host "H3ER_DLEN_REUSE_DEFAULT_CONTRACT=PASS"
Write-Host "H3ER_COPY_OUT_64_DEFAULT_CONTRACT=PASS"
Write-Host "H3ER_COPY_OUT_64_BUILD_OVERRIDE=NO"
Write-Host "H3ER_DATALOG_SOURCE_CONTRACT=PASS"
Write-Host "H3ER_DATALOG_AUTOSERVICE=CORE_SYSTEM_TASK"
Write-Host "H3ER_MANUAL_DATALOG_SERVICE=NO"
Write-Host "H3ER_SERIAL_POLICY=COMPACT_QUIET"
Write-Host "H3ER_PERIODIC_SERIAL_FORMAL_WINDOW=NO"
Write-Host "H3ER_FINAL_SNAPSHOT=POST_FORMAL_WINDOW"
Write-Host "H3ER_BUCKET_PRODUCER=LONG_BUCKET_60S"

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

$tcpCsv =
    Join-Path $tempRoot "tcp_result.csv"

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
    $tcpCsv,
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

[object[]]$tcpCsvRows = @(Import-Csv -LiteralPath $tcpCsv)

if ($tcpCsvRows.Count -ne 1) {
    throw "H3ER_TCP_CSV_ROW_COUNT_INVALID=$($tcpCsvRows.Count)"
}

$tcpRow = $tcpCsvRows[0]

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

$latencyAvgUs =
    Get-H3ERDouble -Text $qualificationText -Key "LATENCY_AVG_US"

$loopAvgUs =
    Get-H3ERDouble -Text $qualificationText -Key "LOOP_GAP_AVG_US"

$loopMaxUs =
    Get-H3ERDouble -Text $qualificationText -Key "LOOP_GAP_MAX_US"

$tcpTargetRequests =
    Convert-H3ERInt -Value $tcpRow.target_requests
$tcpRequestsSent =
    Convert-H3ERInt -Value $tcpRow.requests_sent
$tcpRequestsOk =
    Convert-H3ERInt -Value $tcpRow.requests_ok
$tcpTimeouts =
    Convert-H3ERInt -Value $tcpRow.timeouts
$tcpTransportErrors =
    Convert-H3ERInt -Value $tcpRow.transport_errors
$tcpProtocolErrors =
    Convert-H3ERInt -Value $tcpRow.protocol_errors
$tcpBusLockTimeouts =
    Convert-H3ERInt -Value $tcpRow.server_bus_lock_timeouts
$masterUnexpectedResets =
    Convert-H3ERInt -Value $tcpRow.unexpected_resets

[object[]]$longBuckets =
    @(Get-H3ERLongBuckets -Text $qualificationText)

if ($longBuckets.Count -ne 2) {
    Write-Host "TAIL_REGRESSION=INCONCLUSIVE"
    throw "H3ER_LONG_BUCKET_COUNT_INVALID=$($longBuckets.Count)"
}

$bucket0To60 = $longBuckets[0]
$bucket60To120 = $longBuckets[1]

if ($bucket0To60.Index -ne 1 -or
    $bucket0To60.StartS -ne 0.0 -or
    $bucket0To60.EndS -ne 60.0 -or
    $bucket60To120.Index -ne 2 -or
    $bucket60To120.StartS -ne 60.0 -or
    $bucket60To120.EndS -ne 120.0) {
    Write-Host "TAIL_REGRESSION=INCONCLUSIVE"
    throw "H3ER_LONG_BUCKET_BOUNDARIES_INVALID"
}

$rtuStarted =
    Get-H3ERInt -Text $masterTextResult -Key "RTU_REQUESTS_STARTED"

$rtuSuccess =
    Get-H3ERInt -Text $masterTextResult -Key "RTU_REQUESTS_SUCCESS"

$rtuSkipped =
    Get-H3ERInt -Text $masterTextResult -Key "RTU_PERIODS_SKIPPED"

$rtuFailed =
    Get-H3ERInt -Text $masterTextResult -Key "RTU_REQUESTS_FAILED"

$rtuCrcErrors =
    Get-H3ERInt -Text $masterTextResult -Key "RTU_CRC_ERRORS"

$rtuTimeouts =
    Get-H3ERInt -Text $masterTextResult -Key "RTU_MASTER_TIMEOUTS"

$rtuTimeoutMs =
    Get-H3ERInt -Text $masterTextResult -Key "RTU_TIMEOUT_MS"

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

$sdCommitCount =
    Get-H3ERInt -Text $masterTextResult -Key "SD_FLUSH_CYCLES"

$framReady =
    Get-H3ERValue -Text $masterTextResult -Key "FRAM_READY"
$framFails =
    Get-H3ERInt -Text $masterTextResult -Key "FRAM_FAILS"
$rtcPresent =
    Get-H3ERValue -Text $masterTextResult -Key "RTC_PRESENT"
$rtcUnavailable =
    Get-H3ERInt -Text $masterTextResult -Key "RTC_UNAVAILABLE"
$rtcStale =
    Get-H3ERInt -Text $masterTextResult -Key "RTC_STALE"
$ioInitialized =
    Get-H3ERValue -Text $masterTextResult -Key "IO_INITIALIZED"
$ioStale =
    Get-H3ERInt -Text $masterTextResult -Key "IO_STALE"
$buttonsReady =
    Get-H3ERValue -Text $masterTextResult -Key "BUTTONS_READY"
$buttonsNotReady =
    Get-H3ERInt -Text $masterTextResult -Key "BUTTON_NOT_READY"
$spiProbeFails =
    Get-H3ERInt -Text $masterTextResult -Key "SPI_PROBE_FAILS"

$masterTftPhysical =
    Get-H3ERValue -Text $p5bOutputText -Key "MASTER_TFT_PHYSICAL_PASS"
$slaveTftPhysical =
    Get-H3ERValue -Text $p5bOutputText -Key "SLAVE_TFT_PHYSICAL_PASS"

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

if ($tcpTargetRequests -ne 120000 -or
    $tcpRequestsSent -ne $tcpTargetRequests -or
    $tcpRequestsOk -ne $tcpTargetRequests) {
    throw (
        "H3ER_TCP_REQUEST_COUNTS_INVALID={0}/{1}/{2}" -f
        $tcpTargetRequests,
        $tcpRequestsSent,
        $tcpRequestsOk
    )
}

if ($tcpTimeouts -ne 0 -or
    $tcpTransportErrors -ne 0 -or
    $tcpProtocolErrors -ne 0 -or
    $tcpBusLockTimeouts -ne 0) {
    throw (
        "H3ER_TCP_ERRORS_PRESENT=TIMEOUTS:{0},TRANSPORT:{1},PROTOCOL:{2},BUS_LOCK:{3}" -f
        $tcpTimeouts,
        $tcpTransportErrors,
        $tcpProtocolErrors,
        $tcpBusLockTimeouts
    )
}

if ($masterUnexpectedResets -ne 0) {
    throw "H3ER_MASTER_UNEXPECTED_RESETS=$masterUnexpectedResets"
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

if ($rtuFailed -ne 0 -or
    $rtuCrcErrors -ne 0 -or
    $rtuTimeouts -ne 0) {
    throw (
        "H3ER_RTU_ERRORS_PRESENT=FAILED:{0},CRC:{1},TIMEOUTS:{2}" -f
        $rtuFailed,
        $rtuCrcErrors,
        $rtuTimeouts
    )
}

if ($rtuTimeoutMs -ne 25) {
    throw "H3ER_RTU_TIMEOUT_DIAGNOSTIC_CHANGED=$rtuTimeoutMs"
}

if ($sdActive -ne "YES") {
    throw "H3ER_DATALOG_NOT_ACTIVE"
}

if ($sdAccepted -le 0 -or
    $sdCommitted -le 0 -or
    $sdCommitCount -le 0) {
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

if ($framReady -ne "YES" -or $framFails -ne 0) {
    throw "H3ER_FRAM_INVALID=READY:$framReady,FAILS:$framFails"
}

if ($rtcPresent -ne "YES" -or
    $rtcUnavailable -ne 0 -or
    $rtcStale -ne 0) {
    throw (
        "H3ER_RTC_INVALID=PRESENT:{0},UNAVAILABLE:{1},STALE:{2}" -f
        $rtcPresent,
        $rtcUnavailable,
        $rtcStale
    )
}

if ($ioInitialized -ne "YES" -or $ioStale -ne 0) {
    throw "H3ER_IO_INVALID=INITIALIZED:$ioInitialized,STALE:$ioStale"
}

if ($buttonsReady -ne "YES" -or $buttonsNotReady -ne 0) {
    throw (
        "H3ER_BUTTONS_INVALID=READY:{0},NOT_READY:{1}" -f
        $buttonsReady,
        $buttonsNotReady
    )
}

if ($spiProbeFails -ne 0) {
    throw "H3ER_SPI_PROBE_FAILURES=$spiProbeFails"
}

if ($masterTftPhysical.ToUpperInvariant() -ne "TRUE" -or
    $slaveTftPhysical.ToUpperInvariant() -ne "TRUE") {
    throw (
        "H3ER_TFT_PHYSICAL_FAIL=MASTER:{0},SLAVE:{1}" -f
        $masterTftPhysical,
        $slaveTftPhysical
    )
}

# Baseline full-runtime comparable inmediatamente anterior, medido POST-P4.1.
# Conservamos los guards historicos del gate (+10 % P95, +15 % P99). MAX y
# loop max se reportan como diagnostico por su variabilidad y no vetan solos.
$postP41ReqS = 1000.0
$postP41P95Us = 1270.8
$postP41P99Us = 3666.0
$postP41MaxUs = 22676.2
$postP41LoopAvgUs = 641.0
$postP41LoopMaxUs = 9498.0
$postP41TotalMbps = 2.1680
$postP41UsefulMbps = 2.0000

$p95DeltaPct =
    Get-H3ERDeltaPct -Value $p95Us -Reference $postP41P95Us
$p99DeltaPct =
    Get-H3ERDeltaPct -Value $p99Us -Reference $postP41P99Us
$maxDeltaPct =
    Get-H3ERDeltaPct -Value $maxUs -Reference $postP41MaxUs
$loopMaxDeltaPct =
    Get-H3ERDeltaPct -Value $loopMaxUs -Reference $postP41LoopMaxUs
$reqSDeltaPct =
    Get-H3ERDeltaPct -Value $achievedReqS -Reference $postP41ReqS

$p95GuardUs = $postP41P95Us * 1.10
$p99GuardUs = $postP41P99Us * 1.15

$p95GuardPass = $p95Us -le $p95GuardUs
$p99GuardPass = $p99Us -le $p99GuardUs
$bucket0To60P95Pass = $bucket0To60.P95Us -le $p95GuardUs
$bucket0To60P99Pass = $bucket0To60.P99Us -le $p99GuardUs
$bucket60To120P95Pass = $bucket60To120.P95Us -le $p95GuardUs
$bucket60To120P99Pass = $bucket60To120.P99Us -le $p99GuardUs

$tailRegression = "NOT_PRESENT"
if (-not $p95GuardPass -or
    -not $p99GuardPass -or
    -not $bucket0To60P95Pass -or
    -not $bucket0To60P99Pass -or
    -not $bucket60To120P95Pass -or
    -not $bucket60To120P99Pass) {
    $tailRegression = "PRESENT"
}

Write-Host "H3ER_BASELINE=POST_P4_1_H3ER"
Write-Host ("H3ER_POST_P4_1_REQ_S={0:F2}" -f $postP41ReqS)
Write-Host ("H3ER_POST_P4_1_TOTAL_MBPS={0:F4}" -f $postP41TotalMbps)
Write-Host ("H3ER_POST_P4_1_USEFUL_MBPS={0:F4}" -f $postP41UsefulMbps)
Write-Host ("H3ER_POST_P4_1_P95_US={0:F1}" -f $postP41P95Us)
Write-Host ("H3ER_POST_P4_1_P99_US={0:F1}" -f $postP41P99Us)
Write-Host ("H3ER_POST_P4_1_MAX_US={0:F1}" -f $postP41MaxUs)
Write-Host ("H3ER_POST_P4_1_LOOP_AVG_US={0:F1}" -f $postP41LoopAvgUs)
Write-Host ("H3ER_POST_P4_1_LOOP_MAX_US={0:F1}" -f $postP41LoopMaxUs)
Write-Host ("H3ER_POST_P4_2_DELTA_P95_PCT={0:F3}" -f $p95DeltaPct)
Write-Host ("H3ER_POST_P4_2_DELTA_P99_PCT={0:F3}" -f $p99DeltaPct)
Write-Host ("H3ER_POST_P4_2_DELTA_MAX_PCT={0:F3}" -f $maxDeltaPct)
Write-Host ("H3ER_POST_P4_2_DELTA_LOOP_MAX_PCT={0:F3}" -f $loopMaxDeltaPct)
Write-Host ("H3ER_POST_P4_2_DELTA_REQ_S_PCT={0:F3}" -f $reqSDeltaPct)
Write-Host ("H3ER_P95_GUARD_US={0:F1}" -f $p95GuardUs)
Write-Host ("H3ER_P99_GUARD_US={0:F1}" -f $p99GuardUs)
Write-Host "H3ER_P95_GUARD_PASS=$($p95GuardPass.ToString().ToUpperInvariant())"
Write-Host "H3ER_P99_GUARD_PASS=$($p99GuardPass.ToString().ToUpperInvariant())"
Write-Host "H3ER_BUCKET_0_60_P95_GUARD_PASS=$($bucket0To60P95Pass.ToString().ToUpperInvariant())"
Write-Host "H3ER_BUCKET_0_60_P99_GUARD_PASS=$($bucket0To60P99Pass.ToString().ToUpperInvariant())"
Write-Host "H3ER_BUCKET_60_120_P95_GUARD_PASS=$($bucket60To120P95Pass.ToString().ToUpperInvariant())"
Write-Host "H3ER_BUCKET_60_120_P99_GUARD_PASS=$($bucket60To120P99Pass.ToString().ToUpperInvariant())"
Write-Host "TAIL_REGRESSION=$tailRegression"
Write-Host "H3ER_LOOP_MAX_POLICY=DIAGNOSTIC_ONLY"
Write-Host "H3ER_MAX_POLICY=DIAGNOSTIC_ONLY"

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

$combinedCompileText = $compileText + "`n" + $slaveCompileText

if ($combinedCompileText -match '(?i)-DJWPLC_SPI_FIFO_REUSE_COPY_OUT_64=') {
    throw "H3ER_COPY_OUT_64_WAS_OVERRIDDEN_AT_BUILD"
}

Write-Host "H3ER_COPY_OUT_64_BUILD_OVERRIDE=NO"

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

$masterModbusRtuEvidence =
    Get-H3ERModbusRtuBuildEvidence `
        -NormalizedCompileText $compileNormalized

$slaveModbusRtuEvidence =
    Get-H3ERModbusRtuBuildEvidence `
        -NormalizedCompileText $slaveCompileNormalized

foreach ($item in @(
    [PSCustomObject]@{ Label = "MASTER"; Evidence = $masterModbusRtuEvidence },
    [PSCustomObject]@{ Label = "SLAVE"; Evidence = $slaveModbusRtuEvidence }
)) {
    if (-not $item.Evidence.SourceCompiled) {
        throw "H3ER_$($item.Label)_MODBUS_RTU_SOURCE_NOT_COMPILED"
    }
    if (-not $item.Evidence.SourceLinked) {
        throw "H3ER_$($item.Label)_MODBUS_RTU_SOURCE_NOT_LINKED"
    }
    if ($item.Evidence.ArchiveLinked) {
        throw "H3ER_$($item.Label)_MODBUS_RTU_ARCHIVE_LINKED"
    }
    if ($item.Evidence.PrecompiledMarker) {
        throw "H3ER_$($item.Label)_MODBUS_RTU_PRECOMPILED_MARKER_PRESENT"
    }
}

Write-Host "H3ER_DISPLAY_SOURCE_LINKAGE=PASS"
Write-Host "H3ER_TFT_SOURCE_LINKAGE=PASS"
Write-Host "H3ER_SPI_SOURCE_LINKAGE=PASS"
Write-Host "H3ER_ETHERNET_SOURCE_LINKAGE=PASS"
Write-Host "H3ER_MODBUS_TCP_SOURCE_LINKAGE=PASS"
Write-Host "H3ER_DATALOG_SOURCE_LINKAGE=PASS"
Write-Host "H3ER_MASTER_MODBUS_RTU_SOURCE_COMPILED=YES"
Write-Host "H3ER_MASTER_MODBUS_RTU_SOURCE_LINKED=YES"
Write-Host "H3ER_MASTER_MODBUS_RTU_ARCHIVE_LINKED=NO"
Write-Host "H3ER_SLAVE_MODBUS_RTU_SOURCE_COMPILED=YES"
Write-Host "H3ER_SLAVE_MODBUS_RTU_SOURCE_LINKED=YES"
Write-Host "H3ER_SLAVE_MODBUS_RTU_ARCHIVE_LINKED=NO"
Write-Host "H3ER_MODBUS_RTU_SOURCE_FIRST_LINKAGE=PASS"

Write-Host ""
Write-Host "=============================================================================="
Write-Host " A14 H3E-R SUMMARY"
Write-Host "=============================================================================="
Write-Host ("H3ER_TCP_REQUESTED_REQ_S={0:F2}" -f $requestedReqS)
Write-Host ("H3ER_TCP_ACHIEVED_REQ_S={0:F2}" -f $achievedReqS)
Write-Host ("H3ER_TCP_ACHIEVED_PCT={0:F3}" -f $achievedPct)
Write-Host ("H3ER_TCP_TOTAL_MBPS={0:F4}" -f $totalMbps)
Write-Host ("H3ER_TCP_USEFUL_MBPS={0:F4}" -f $usefulMbps)
Write-Host ("H3ER_TCP_LATENCY_AVG_US={0:F1}" -f $latencyAvgUs)
Write-Host ("H3ER_TCP_P95_US={0:F1}" -f $p95Us)
Write-Host ("H3ER_TCP_P99_US={0:F1}" -f $p99Us)
Write-Host ("H3ER_TCP_MAX_US={0:F1}" -f $maxUs)
Write-Host "H3ER_TCP_TARGET_REQUESTS=$tcpTargetRequests"
Write-Host "H3ER_TCP_REQUESTS_SENT=$tcpRequestsSent"
Write-Host "H3ER_TCP_REQUESTS_OK=$tcpRequestsOk"
Write-Host "H3ER_TCP_TIMEOUTS=$tcpTimeouts"
Write-Host "H3ER_TCP_TRANSPORT_ERRORS=$tcpTransportErrors"
Write-Host "H3ER_TCP_PROTOCOL_ERRORS=$tcpProtocolErrors"
Write-Host "H3ER_TCP_BUS_LOCK_TIMEOUTS=$tcpBusLockTimeouts"
Write-Host ("H3ER_BUCKET_0_60_REQ_S={0:F2}" -f $bucket0To60.ReqS)
Write-Host "H3ER_BUCKET_0_60_OK=$($bucket0To60.Ok)"
Write-Host ("H3ER_BUCKET_0_60_AVG_US={0:F1}" -f $bucket0To60.AvgUs)
Write-Host ("H3ER_BUCKET_0_60_P95_US={0:F1}" -f $bucket0To60.P95Us)
Write-Host ("H3ER_BUCKET_0_60_P99_US={0:F1}" -f $bucket0To60.P99Us)
Write-Host ("H3ER_BUCKET_0_60_MAX_US={0:F1}" -f $bucket0To60.MaxUs)
Write-Host ("H3ER_BUCKET_60_120_REQ_S={0:F2}" -f $bucket60To120.ReqS)
Write-Host "H3ER_BUCKET_60_120_OK=$($bucket60To120.Ok)"
Write-Host ("H3ER_BUCKET_60_120_AVG_US={0:F1}" -f $bucket60To120.AvgUs)
Write-Host ("H3ER_BUCKET_60_120_P95_US={0:F1}" -f $bucket60To120.P95Us)
Write-Host ("H3ER_BUCKET_60_120_P99_US={0:F1}" -f $bucket60To120.P99Us)
Write-Host ("H3ER_BUCKET_60_120_MAX_US={0:F1}" -f $bucket60To120.MaxUs)
Write-Host ("H3ER_LOOP_AVG_US={0:F1}" -f $loopAvgUs)
Write-Host ("H3ER_LOOP_MAX_US={0:F1}" -f $loopMaxUs)
Write-Host ("H3ER_RTU_ACHIEVED_HZ={0:F3}" -f $rtuHz)
Write-Host "H3ER_RTU_REQUESTS_STARTED=$rtuStarted"
Write-Host "H3ER_RTU_REQUESTS_SUCCESS=$rtuSuccess"
Write-Host "H3ER_RTU_REQUESTS_FAILED=$rtuFailed"
Write-Host "H3ER_RTU_PERIODS_SKIPPED=$rtuSkipped"
Write-Host "H3ER_RTU_CRC_ERRORS=$rtuCrcErrors"
Write-Host "H3ER_RTU_TIMEOUTS=$rtuTimeouts"
Write-Host "H3ER_RTU_TIMEOUT_MS=$rtuTimeoutMs"
Write-Host "H3ER_DATALOG_ACTIVE=$sdActive"
Write-Host "H3ER_DATALOG_ACCEPTED_BYTES=$sdAccepted"
Write-Host "H3ER_DATALOG_COMMITTED_BYTES=$sdCommitted"
Write-Host "H3ER_DATALOG_PENDING_BYTES=$sdPending"
Write-Host "H3ER_DATALOG_COMMIT_COUNT=$sdCommitCount"
Write-Host "H3ER_DATALOG_FAILED_COMMITS=$sdFailed"
Write-Host "H3ER_DATALOG_MANUAL_SERVICE=NO"
Write-Host "H3ER_FRAM_READY=$framReady"
Write-Host "H3ER_FRAM_FAILS=$framFails"
Write-Host "H3ER_RTC_PRESENT=$rtcPresent"
Write-Host "H3ER_RTC_UNAVAILABLE=$rtcUnavailable"
Write-Host "H3ER_RTC_STALE=$rtcStale"
Write-Host "H3ER_IO_INITIALIZED=$ioInitialized"
Write-Host "H3ER_IO_STALE=$ioStale"
Write-Host "H3ER_BUTTONS_READY=$buttonsReady"
Write-Host "H3ER_BUTTON_NOT_READY=$buttonsNotReady"
Write-Host "H3ER_SPI_PROBE_FAILS=$spiProbeFails"
Write-Host "H3ER_PERIPHERAL_FAILURE_COUNT=$peripheralFailures"
Write-Host "H3ER_MASTER_UNEXPECTED_RESETS=$masterUnexpectedResets"
Write-Host "H3ER_SLAVE_RESET_DIRECT_COUNTER=NOT_EXPOSED"
Write-Host "H3ER_SLAVE_RESET_GUARD=RTU_CROSS_COUNT_AND_QUIESCED_FINAL_SNAPSHOT"
Write-Host "H3ER_MASTER_TFT_PHYSICAL=PASS"
Write-Host "H3ER_SLAVE_TFT_PHYSICAL=PASS"
Write-Host "H3ER_FIFO_REUSE_DEFAULT=PASS"
Write-Host "H3ER_DLEN_REUSE_DEFAULT=PASS"
Write-Host "H3ER_COPY_OUT_64_DEFAULT=1"
Write-Host "H3ER_COPY_OUT_64_SOURCE=PACKAGE_DEFAULT"
Write-Host "H3ER_COPY_OUT_64_BUILD_OVERRIDE=NO"
Write-Host "H3ER_DIRECT_RX_DEFAULT=OFF"
Write-Host "H3ER_RX_COMMIT=IMMEDIATE"
Write-Host "H3ER_SINGLE_STATUS_POLICY=PASS"
Write-Host "H3ER_DATALOG_POLICY=PASS"
Write-Host "H3ER_DISPLAY_DIRTY_POLICY=PASS"
Write-Host "H3ER_RTU_ASYNC_POLICY=PASS"
Write-Host "H3ER_MODBUS_RTU_POLICY=SOURCE_FIRST"
Write-Host "H3ER_MODBUS_RTU_PRECOMPILED_REQUIRED=NO"
Write-Host "H3ER_H3E5_SECONDARY_P95_US=1197.2"
Write-Host "H3ER_H3E5_SECONDARY_P99_US=3423.3"
Write-Host "H3ER_H3E5_SECONDARY_MAX_US=18823.7"
Write-Host "HARNESS_FAILURE=NO"
Write-Host "HARDWARE_FAILURE=NO_EVIDENCE"
Write-Host "H3ER_POST_P4_2_COMPOSITION_AUDIT=PASS"
Write-Host "H3ER_TEMP_ROOT=$tempRoot"

if ($tailRegression -eq "PRESENT") {
    Write-Host "H3ER_POST_P4_2_LATENCY_GUARD=FAIL"
    Write-Host "PRODUCT_FAILURE=REGRESSION_EVIDENCE"
    Write-Host "A14_H3E_R_POST_P4_2=FAIL"
    Write-Host "NEXT=RETURN_REGRESSION_FOR_REVIEW"
    throw "H3ER_POST_P4_2_TAIL_REGRESSION_PRESENT"
}

Write-Host "H3ER_POST_P4_2_LATENCY_GUARD=PASS"
Write-Host "PRODUCT_FAILURE=NO_EVIDENCE"
Write-Host "A14_H3E_R_POST_P4_2=PASS"
Write-Host "NEXT=RETURN_RESULT_FOR_REVIEW_BEFORE_P4_2_CLOSURE"
