param(
    [string]$MasterPort = "COM14",
    [string]$SlavePort = "COM4",
    [double]$TcpRate = 1000.0,
    [double]$DurationS = 600.0,
    [string]$PythonExe = ""
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"
. (Join-Path $PSScriptRoot "common.ps1")

function Get-LR600Value {
    param(
        [Parameter(Mandatory = $true)][string]$Text,
        [Parameter(Mandatory = $true)][string]$Key
    )

    $pattern =
        "(?m)^" + [regex]::Escape($Key) + "=(.*)\r?$"
    [object[]]$matches = @([regex]::Matches($Text, $pattern))

    if ($matches.Count -lt 1) {
        throw "H3ER_LR600_KEY_MISSING=$Key"
    }

    [string[]]$values = @(
        $matches |
            ForEach-Object { $_.Groups[1].Value.Trim() } |
            Select-Object -Unique
    )

    if ($values.Count -ne 1) {
        throw "H3ER_LR600_KEY_VALUES_CONFLICT=${Key}:$($values -join ',')"
    }

    return $values[0]
}

function Convert-LR600Double {
    param([Parameter(Mandatory = $true)][string]$Value)

    return [double]::Parse(
        $Value,
        [Globalization.CultureInfo]::InvariantCulture)
}

function Convert-LR600Int {
    param([Parameter(Mandatory = $true)][string]$Value)

    return [int64]::Parse(
        $Value,
        [Globalization.CultureInfo]::InvariantCulture)
}

function Get-LR600Double {
    param([string]$Text, [string]$Key)

    return Convert-LR600Double -Value (
        Get-LR600Value -Text $Text -Key $Key)
}

function Get-LR600Int {
    param([string]$Text, [string]$Key)

    return Convert-LR600Int -Value (
        Get-LR600Value -Text $Text -Key $Key)
}

function Format-LR600Double {
    param(
        [Parameter(Mandatory = $true)][double]$Value,
        [string]$Format = "F3"
    )

    return $Value.ToString(
        $Format,
        [Globalization.CultureInfo]::InvariantCulture)
}

function Get-LR600DeltaPct {
    param(
        [Parameter(Mandatory = $true)][double]$Value,
        [Parameter(Mandatory = $true)][double]$Reference
    )

    if ($Reference -eq 0.0) {
        throw "H3ER_LR600_DELTA_REFERENCE_ZERO"
    }

    return (($Value / $Reference) - 1.0) * 100.0
}

function Get-LR600Median {
    param([Parameter(Mandatory = $true)][double[]]$Values)

    if ($Values.Count -lt 1) {
        throw "H3ER_LR600_MEDIAN_EMPTY"
    }

    [double[]]$ordered = @($Values | Sort-Object)
    $middle = [int][Math]::Floor($ordered.Count / 2.0)

    if (($ordered.Count % 2) -eq 1) {
        return $ordered[$middle]
    }

    return ($ordered[$middle - 1] + $ordered[$middle]) / 2.0
}

function Get-LR600SpreadPct {
    param([Parameter(Mandatory = $true)][double[]]$Values)

    $median = Get-LR600Median -Values $Values

    if ($median -eq 0.0) {
        throw "H3ER_LR600_SPREAD_MEDIAN_ZERO"
    }

    $minimum = ($Values | Measure-Object -Minimum).Minimum
    $maximum = ($Values | Measure-Object -Maximum).Maximum
    return (($maximum - $minimum) / $median) * 100.0
}

function Get-LR600LongBuckets {
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
                    Index = Convert-LR600Int $_.Groups[1].Value
                    StartS = Convert-LR600Double $_.Groups[2].Value
                    EndS = Convert-LR600Double $_.Groups[3].Value
                    ReqS = Convert-LR600Double $_.Groups[4].Value
                    Ok = Convert-LR600Int $_.Groups[5].Value
                    AvgUs = Convert-LR600Double $_.Groups[6].Value
                    P95Us = Convert-LR600Double $_.Groups[7].Value
                    P99Us = Convert-LR600Double $_.Groups[8].Value
                    MaxUs = Convert-LR600Double $_.Groups[9].Value
                }
            }
    )
}

function Get-LR600ModbusRtuBuildEvidence {
    param(
        [Parameter(Mandatory = $true)]
        [string]$NormalizedCompileText
    )

    $canonicalText =
        [regex]::Replace($NormalizedCompileText, '/+', '/')
    $sourcePath =
        "/libraries/jwplc_modbusrtu/src/jwplc_modbusrtu.cpp"
    $sourceObject =
        "/libraries/jwplc_modbusrtu/jwplc_modbusrtu.cpp.o"
    $archiveDirectory =
        "/libraries/jwplc_modbusrtu/src/esp32"

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
    $archiveLinked =
        $linkText.Contains(
            "$archiveDirectory/libjwplc_modbusrtu.a") -or
        ($linkText.Contains($archiveDirectory) -and
            [regex]::IsMatch(
                $linkText,
                '(?m)(?:^|\s)"?-ljwplc_modbusrtu"?(?=\s|$)'))
    $precompiledMarker =
        [regex]::IsMatch(
            $canonicalText,
            '(?m)^using precompiled library .*jwplc_modbusrtu.*$')

    return [PSCustomObject]@{
        SourceCompiled = $sourceCompiled
        SourceLinked = $sourceLinked
        ArchiveLinked = $archiveLinked
        PrecompiledMarker = $precompiledMarker
    }
}

function Copy-LR600Artifacts {
    param(
        [Parameter(Mandatory = $true)][string]$SourceRoot,
        [Parameter(Mandatory = $true)][string]$DestinationRoot
    )

    foreach ($name in @(
        "qualification.log",
        "master_final.txt",
        "slave_final.txt",
        "tcp_result.csv",
        "compile_master.log",
        "compile_slave.log",
        "upload_master.log",
        "upload_slave.log",
        "resolver.log",
        "py_compile.log"
    )) {
        $source = Join-Path $SourceRoot $name

        if (Test-Path -LiteralPath $source) {
            Copy-Item `
                -LiteralPath $source `
                -Destination (Join-Path $DestinationRoot $name) `
                -Force
        }
    }
}

function Write-LR600SummaryLine {
    param([Parameter(Mandatory = $true)][string]$Line)

    Write-Host $Line
    [void]$script:LR600SummaryLines.Add($Line)
}

Write-Host "=============================================================================="
Write-Host " A14 H3E-R POST-P4.2 LONG RUN 600S - FULL RUNTIME"
Write-Host " TCP 1000 req/s + RTU 50 Hz + DATALOG + FULL PERIPHERALS"
Write-Host "=============================================================================="

Assert-G2Branch

$repo = $script:G2RepoRoot
$head = Get-G2Head
$dirty = @(Get-G2TrackedDirtyPaths)
$staged = @(& git -C $repo diff --cached --name-only)

if ($dirty.Count -ne 0 -or $staged.Count -ne 0) {
    throw "H3ER_LR600_TREE_MUST_BE_CLEAN"
}

if ($MasterPort -ne "COM14" -or $SlavePort -ne "COM4") {
    throw "H3ER_LR600_PORTS_MUST_BE_COM14_COM4"
}

if ($TcpRate -ne 1000.0) {
    throw "H3ER_LR600_TCP_RATE_MUST_BE_1000"
}

if ($DurationS -ne 600.0) {
    throw "H3ER_LR600_DURATION_MUST_BE_600S"
}

if ((Get-G2SpiHz) -ne 26000000) {
    throw "H3ER_LR600_W5500_SPI_NOT_26MHZ"
}

if (-not [string]::IsNullOrWhiteSpace($PythonExe)) {
    if (-not (Test-Path -LiteralPath $PythonExe)) {
        throw "H3ER_LR600_PYTHON_EXPLICIT_NOT_FOUND=$PythonExe"
    }

    $pythonExeResolved = (Resolve-Path -LiteralPath $PythonExe).Path
}
else {
    $pythonCommand = Get-Command python.exe -ErrorAction SilentlyContinue

    if ($null -eq $pythonCommand) {
        $pythonCommand = Get-Command python -ErrorAction SilentlyContinue
    }

    if ($null -eq $pythonCommand) {
        throw "H3ER_LR600_PYTHON_NOT_FOUND"
    }

    $pythonExeResolved = $pythonCommand.Source
}

$p5bGate =
    Join-Path $PSScriptRoot "a14_p5b_physical_master_slave_combined.ps1"
$shortGate =
    Join-Path $PSScriptRoot "a14_h3er_current_package_full_runtime.ps1"
$packageContract =
    Join-Path $PSScriptRoot "a14_package_promotion_contract.py"
$qualificationRunner =
    Get-G2Path "tools/modbus-tcp-benchmark/pc/a14_perf_full_runtime_realistic_1000rps_qualification.py"
$frontierRunner =
    Get-G2Path "tools/modbus-tcp-benchmark/pc/a14_perf_fc03_125_formal_frontier.py"
$masterSketch =
    Get-G2Path "tools/modbus-tcp-benchmark/firmware/a14_p5_full_runtime_master/a14_p5_full_runtime_master.ino"
$slaveSketch =
    Get-G2Path "tools/modbus-tcp-benchmark/firmware/a14_p5_rtu_slave/a14_p5_rtu_slave.ino"
$w5100Path =
    Get-G2Path "JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/utility/w5100.h"
$spiHeaderPath =
    Get-G2Path "JWPLC/2.1.0/libraries/SPI/src/SPI.h"
$ethernetClientPath =
    Get-G2Path "JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/EthernetClient.cpp"
$modbusTcpPath =
    Get-G2Path "JWPLC/2.1.0/libraries/JWPLC_ModbusTCP/src/JWPLC_ModbusTCP.cpp"
$modbusRtuPropsPath =
    Get-G2Path "JWPLC/2.1.0/libraries/JWPLC_ModbusRTU/library.properties"
$modbusRtuSourcePath =
    Get-G2Path "JWPLC/2.1.0/libraries/JWPLC_ModbusRTU/src/JWPLC_ModbusRTU.cpp"
$coreMainPath =
    Get-G2Path "JWPLC/2.1.0/cores/jwcontrol/main.cpp"
$displayPropsPath =
    Get-G2Path "JWPLC/2.1.0/libraries/JWPLC_Display/library.properties"
$tftPropsPath =
    Get-G2Path "JWPLC/2.1.0/libraries/JWPLC_TFT/library.properties"
$displayImplPath =
    Get-G2Path "JWPLC/2.1.0/libraries/JWPLC_Display/src/JWPLC_Display.cpp"
$uiImplPath =
    Get-G2Path "JWPLC/2.1.0/libraries/JWPLC_Display/src/JWPLC_UI.cpp"
$tftImplPath =
    Get-G2Path "JWPLC/2.1.0/libraries/JWPLC_TFT/src/JWPLC_TFT.cpp"

foreach ($required in @(
    $p5bGate,
    $shortGate,
    $packageContract,
    $qualificationRunner,
    $frontierRunner,
    $masterSketch,
    $slaveSketch,
    $w5100Path,
    $spiHeaderPath,
    $ethernetClientPath,
    $modbusTcpPath,
    $modbusRtuPropsPath,
    $modbusRtuSourcePath,
    $coreMainPath,
    $displayPropsPath,
    $tftPropsPath,
    $displayImplPath,
    $uiImplPath,
    $tftImplPath
)) {
    if (-not (Test-Path -LiteralPath $required)) {
        throw "H3ER_LR600_REQUIRED_FILE_MISSING=$required"
    }
}

$p5bText = [IO.File]::ReadAllText($p5bGate)
$qualificationTextSource = [IO.File]::ReadAllText($qualificationRunner)
$frontierText = [IO.File]::ReadAllText($frontierRunner)
$masterText = [IO.File]::ReadAllText($masterSketch)
$slaveText = [IO.File]::ReadAllText($slaveSketch)
$w5100Text = [IO.File]::ReadAllText($w5100Path)
$spiHeaderText = [IO.File]::ReadAllText($spiHeaderPath)
$ethernetClientText = [IO.File]::ReadAllText($ethernetClientPath)
$modbusTcpText = [IO.File]::ReadAllText($modbusTcpPath)
$modbusRtuPropsText = [IO.File]::ReadAllText($modbusRtuPropsPath)
$coreMainText = [IO.File]::ReadAllText($coreMainPath)
$displayPropsText = [IO.File]::ReadAllText($displayPropsPath)
$tftPropsText = [IO.File]::ReadAllText($tftPropsPath)
$displayImplText = [IO.File]::ReadAllText($displayImplPath)
$uiImplText = [IO.File]::ReadAllText($uiImplPath)
$tftImplText = [IO.File]::ReadAllText($tftImplPath)

if (-not $p5bText.Contains('"--duration", $durationText') -or
    -not $qualificationTextSource.Contains("frontier.run_case(") -or
    -not $frontierText.Contains("math.ceil(duration / long_bucket_seconds)")) {
    throw "H3ER_LR600_DURATION_AGNOSTIC_CHAIN_NOT_PROVEN"
}

$formalLoopIndex =
    $frontierText.IndexOf("for i in range(target_requests):")
$formalWindowEndIndex =
    $frontierText.IndexOf("unexpected_resets += (", $formalLoopIndex)
$finalSnapshotIndex =
    $frontierText.IndexOf('ser.write(b"S\n")', $formalWindowEndIndex)

if ($formalLoopIndex -lt 0 -or
    $formalWindowEndIndex -le $formalLoopIndex -or
    $finalSnapshotIndex -le $formalWindowEndIndex) {
    throw "H3ER_LR600_SERIAL_LIFECYCLE_NOT_PROVEN"
}

$formalLoopText =
    $frontierText.Substring(
        $formalLoopIndex,
        $formalWindowEndIndex - $formalLoopIndex)

if ($formalLoopText.Contains("collect_snapshot(") -or
    $formalLoopText.Contains('ser.write(b"S')) {
    throw "H3ER_LR600_PERIODIC_SERIAL_FORBIDDEN"
}

if (-not $w5100Text.Contains(
        "#define JWPLC_W5500_RX_FIFO_REUSE 1") -or
    -not $w5100Text.Contains(
        "#define JWPLC_W5500_RX_DIRECT_TRANSFER_BYTES 0") -or
    -not $spiHeaderText.Contains(
        "#define JWPLC_SPI_FIFO_REUSE_DLEN_CACHE 1") -or
    -not $spiHeaderText.Contains(
        "#define JWPLC_SPI_FIFO_REUSE_COPY_OUT_64 1")) {
    throw "H3ER_LR600_PACKAGE_DEFAULT_CONTRACT_FAILED"
}

foreach ($macro in @(
    "JWPLC_W5500_RX_FIFO_REUSE=",
    "JWPLC_W5500_RX_DIRECT_TRANSFER_BYTES=",
    "JWPLC_SPI_FIFO_REUSE_DLEN_CACHE=",
    "JWPLC_SPI_FIFO_REUSE_COPY_OUT_64="
)) {
    if ($p5bText.Contains($macro)) {
        throw "H3ER_LR600_P5B_PRODUCT_OVERRIDE=$macro"
    }
}

foreach ($token in @(
    "RTU_TARGET_SLAVE_ID = 2",
    "RTU_BAUD = 115200UL",
    "RTU_CONFIG = SERIAL_8N1",
    "RTU_PERIOD_DEFAULT_US = 20000UL",
    "RTU_TIMEOUT_MS = 25UL",
    "JWPLC_ModbusRTU.motor(ASYNC)",
    "JWPLCDataLog sdDataLog",
    "SD_WORKLOAD_MODE=BUFFERED_DATALOG",
    "USER_REFRESH_ON_DEMAND"
)) {
    if (-not $masterText.Contains($token)) {
        throw "H3ER_LR600_MASTER_CONTRACT_MISSING=$token"
    }
}

foreach ($token in @(
    "SLAVE_ID = 2",
    "RTU_BAUD = 115200UL",
    "RTU_CONFIG = SERIAL_8N1",
    "JWPLC_ModbusRTU.motor(ASYNC)",
    "USER_REFRESH_ON_DEMAND"
)) {
    if (-not $slaveText.Contains($token)) {
        throw "H3ER_LR600_SLAVE_CONTRACT_MISSING=$token"
    }
}

if ($masterText.Contains("sdDataLog.service(") -or
    $masterText.Contains("JWPLC_SD.serviceDataLogs(") -or
    -not $coreMainText.Contains("jwplcDataLogTickCallback();")) {
    throw "H3ER_LR600_DATALOG_AUTOSERVICE_CONTRACT_FAILED"
}

if ($displayPropsText -match '(?m)^precompiled\s*=' -or
    $tftPropsText -match '(?m)^precompiled\s*=') {
    throw "H3ER_LR600_DISPLAY_TFT_MUST_BUILD_FROM_SOURCE"
}

foreach ($token in @(
    "jwplcUIRuntimeDrawDirty",
    "jwplcUserDisplayRefreshNeededCallback"
)) {
    if (-not $displayImplText.Contains($token)) {
        throw "H3ER_LR600_DISPLAY_DIRTY_CONTRACT_MISSING=$token"
    }
}

foreach ($token in @(
    "USER_REFRESH_ON_DEMAND",
    "field.dirty",
    "drawDirty",
    "refreshNeeded"
)) {
    if (-not $uiImplText.Contains($token)) {
        throw "H3ER_LR600_UI_DIRTY_CONTRACT_MISSING=$token"
    }
}

foreach ($token in @(
    "#include <TFT_eSPI.h>",
    "jwplcSPI_acquire",
    "jwplcSPI_prepareForTFT"
)) {
    if (-not $tftImplText.Contains($token)) {
        throw "H3ER_LR600_TFT_BACKEND_CONTRACT_MISSING=$token"
    }
}

foreach ($token in @(
    "JWPLC_FRAM.read(",
    "jwplcGetRTCState()",
    "jwplcGetIOState()",
    "JWPLCButtons::isReady()",
    "probeSpiMutex()"
)) {
    if (-not $masterText.Contains($token)) {
        throw "H3ER_LR600_PERIPHERAL_SOURCE_MISSING=$token"
    }
}

if (-not $modbusTcpText.Contains("_client.read(") -or
    -not $ethernetClientText.Contains(
        "return Ethernet.socketRecv(_sockindex, buf, size);")) {
    throw "H3ER_LR600_RX_COMMIT_NOT_IMMEDIATE"
}

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
        throw "H3ER_LR600_TCP_ASYNC_API_MISSING=$token"
    }
}

if ($modbusTcpText.Contains("beginWriteAsync(") -or
    -not $modbusTcpText.Contains(
        "_client.write(_txBuffer, responseLength)")) {
    throw "H3ER_LR600_MODBUS_TCP_TX_PATH_CHANGED"
}

if ($modbusRtuPropsText -match '(?m)^\s*precompiled\s*=' -or
    -not (Test-Path -LiteralPath $modbusRtuSourcePath)) {
    throw "H3ER_LR600_MODBUS_RTU_NOT_SOURCE_FIRST"
}

$contractOutput =
    @(& $pythonExeResolved -B $packageContract 2>&1)
$contractText =
    ($contractOutput | ForEach-Object { $_.ToString() }) -join "`n"

if ($LASTEXITCODE -ne 0 -or
    -not $contractText.Contains("A14_PACKAGE_PROMOTION_CONTRACT=PASS")) {
    throw "H3ER_LR600_PACKAGE_PROMOTION_CONTRACT_FAILED"
}

Write-Host "H3ER_LR600_HEAD=$head"
Write-Host "H3ER_LR600_DURATION_S=600"
Write-Host "H3ER_LR600_BUCKET_POLICY=10X60S"
Write-Host "H3ER_LR600_SERIAL_POLICY=COMPACT_QUIET"
Write-Host "H3ER_LR600_FINAL_SNAPSHOT=POST_FORMAL_WINDOW"
Write-Host "H3ER_LR600_TCP_FUNCTION=FC03"
Write-Host "H3ER_LR600_TCP_QUANTITY_REGISTERS=125"
Write-Host "H3ER_LR600_FIFO_REUSE_SOURCE=PACKAGE_DEFAULT"
Write-Host "H3ER_LR600_DLEN_REUSE_SOURCE=PACKAGE_DEFAULT"
Write-Host "H3ER_LR600_COPY_OUT_64_SOURCE=PACKAGE_DEFAULT"
Write-Host "H3ER_LR600_COPY_OUT_64_DEFAULT=1"
Write-Host "H3ER_LR600_COPY_OUT_64_BUILD_OVERRIDE=NO"
Write-Host "H3ER_LR600_DIRECT_RX_SOURCE=PACKAGE_DEFAULT"
Write-Host "H3ER_LR600_RX_COMMIT=IMMEDIATE"
Write-Host "H3ER_LR600_DATALOG_POLICY=MANDATORY_PRODUCT_PATH"
Write-Host "H3ER_LR600_DATALOG_MANUAL_SERVICE=NO"
Write-Host "H3ER_LR600_DISPLAY_POLICY=JWPLC_DISPLAY_HMI_ON_DEMAND_DIRTY"
Write-Host "H3ER_LR600_TFT_BACKEND=TFT_ESPI_PRIVATE"
Write-Host "H3ER_LR600_MODBUS_RTU_POLICY=SOURCE_FIRST"
Write-Host "H3ER_LR600_TCP_ASYNC_CONNECT=PRESENT_BUT_NOT_EXERCISED_INBOUND_SERVER"
Write-Host "H3ER_LR600_TCP_ASYNC_WRITE=PRESENT_BUT_NOT_EXERCISED_NOT_INTEGRATED_IN_MODBUS_TCP"
Write-Host "H3ER_LR600_TCP_ASYNC_FLUSH=PRESENT_BUT_NOT_EXERCISED_BY_MODBUS_TCP"
Write-Host "H3ER_LR600_TCP_ASYNC_STOP=PRESENT_LEGACY_WRAPPER_USES_ASYNC_ENGINE"
Write-Host "H3ER_LR600_MODBUS_TCP_TX=LEGACY_BLOCKING_WRITE"
Write-Host "H3ER_LR600_BASELINE=H3ER_POST_P4_2_120S"
Write-Host "H3ER_LR600_PRODUCT_MUTATION=NO"
Write-Host "H3ER_LR600_PREFLIGHT=PASS"

$timestamp = Get-Date -Format "yyyyMMdd_HHmmss"
$resultRoot =
    Join-Path $env:TEMP ("jwplc_a14_h3er_lr600_{0}" -f $timestamp)
New-Item -ItemType Directory -Force -Path $resultRoot | Out-Null
Write-Host "H3ER_LR600_RESULT_ROOT=$resultRoot"

$p5bArgs = @{
    MasterPort = $MasterPort
    SlavePort = $SlavePort
    TcpRate = $TcpRate
    DurationS = $DurationS
    PythonExe = $pythonExeResolved
}

$p5bLines = New-Object System.Collections.Generic.List[string]
$p5bFailure = $null

try {
    & $p5bGate @p5bArgs *>&1 |
        ForEach-Object {
            $line = $_.ToString()
            [void]$p5bLines.Add($line)

            if ($line -match '^(SLAVE_COMPILE_EXIT|MASTER_COMPILE_EXIT|SLAVE_UPLOAD_EXIT|MASTER_UPLOAD_EXIT|P5B_RESOLVER_EXIT|P5B_QUALIFICATION_EXIT|MASTER_TFT_PHYSICAL_PASS|SLAVE_TFT_PHYSICAL_PASS|TFT_PHYSICAL_PASS)=') {
                Write-Host $line
            }
        }
}
catch {
    $p5bFailure = $_
}

$p5bTextResult = $p5bLines -join "`n"
$sourceTempRoot = ""

foreach ($key in @("P5B_TEMP_ROOT", "TEMP_ROOT")) {
    [object[]]$rootMatches = @(
        [regex]::Matches(
            $p5bTextResult,
            "(?m)^" + [regex]::Escape($key) + "=(.*)\r?$")
    )

    if ($rootMatches.Count -gt 0) {
        $sourceTempRoot =
            $rootMatches[$rootMatches.Count - 1].Groups[1].Value.Trim()
        break
    }
}

if (-not [string]::IsNullOrWhiteSpace($sourceTempRoot) -and
    (Test-Path -LiteralPath $sourceTempRoot)) {
    Copy-LR600Artifacts `
        -SourceRoot $sourceTempRoot `
        -DestinationRoot $resultRoot
}

if ($null -ne $p5bFailure) {
    $failureSummary = @(
        "H3ER_LR600_HARNESS_FAILURE=YES",
        "H3ER_LR600_FORMAL_RESULT=INCOMPLETE",
        "H3ER_LR600_ERROR=$($p5bFailure.Exception.Message)",
        "H3ER_LR600_RESULT_ROOT=$resultRoot"
    )
    Set-Content `
        -LiteralPath (Join-Path $resultRoot "SUMMARY.log") `
        -Value $failureSummary `
        -Encoding UTF8
    $failureSummary | ForEach-Object { Write-Host $_ }
    throw $p5bFailure
}

if (-not $p5bTextResult.Contains(
        "A14_P5B_PHYSICAL_MASTER_SLAVE_COMBINED=PASS")) {
    throw "H3ER_LR600_P5B_NOT_PASS"
}

if ([string]::IsNullOrWhiteSpace($sourceTempRoot)) {
    throw "H3ER_LR600_P5B_TEMP_ROOT_MISSING"
}

$qualificationLog = Join-Path $sourceTempRoot "qualification.log"
$masterSnapshot = Join-Path $sourceTempRoot "master_final.txt"
$slaveSnapshot = Join-Path $sourceTempRoot "slave_final.txt"
$tcpCsv = Join-Path $sourceTempRoot "tcp_result.csv"
$masterCompileLog = Join-Path $sourceTempRoot "compile_master.log"
$slaveCompileLog = Join-Path $sourceTempRoot "compile_slave.log"

foreach ($required in @(
    $qualificationLog,
    $masterSnapshot,
    $slaveSnapshot,
    $tcpCsv,
    $masterCompileLog,
    $slaveCompileLog
)) {
    if (-not (Test-Path -LiteralPath $required)) {
        throw "H3ER_LR600_RESULT_MISSING=$required"
    }
}

$qualificationText = [IO.File]::ReadAllText($qualificationLog)
$masterSnapshotText = [IO.File]::ReadAllText($masterSnapshot)
$slaveSnapshotText = [IO.File]::ReadAllText($slaveSnapshot)
$masterCompileText = [IO.File]::ReadAllText($masterCompileLog)
$slaveCompileText = [IO.File]::ReadAllText($slaveCompileLog)

[object[]]$tcpRows = @(Import-Csv -LiteralPath $tcpCsv)

if ($tcpRows.Count -ne 1) {
    throw "H3ER_LR600_TCP_CSV_ROW_COUNT=$($tcpRows.Count)"
}

$tcpRow = $tcpRows[0]
[object[]]$buckets = @(Get-LR600LongBuckets -Text $qualificationText)

if ($buckets.Count -ne 10) {
    throw "H3ER_LR600_BUCKET_COUNT=$($buckets.Count)"
}

for ($i = 0; $i -lt 10; ++$i) {
    $expectedIndex = $i + 1
    $expectedStart = [double]($i * 60)
    $expectedEnd = [double](($i + 1) * 60)
    $bucket = $buckets[$i]

    if ($bucket.Index -ne $expectedIndex -or
        $bucket.StartS -ne $expectedStart -or
        $bucket.EndS -ne $expectedEnd) {
        throw "H3ER_LR600_BUCKET_BOUNDARY_INVALID=$expectedIndex"
    }
}

$requestedReqS = Get-LR600Double $qualificationText "REQUESTED_REQ_S"
$achievedReqS = Get-LR600Double $qualificationText "ACHIEVED_REQ_S"
$achievedPct = Get-LR600Double $qualificationText "ACHIEVED_PCT"
$totalMbps = Get-LR600Double $qualificationText "TOTAL_TCP_PAYLOAD_MBPS"
$usefulMbps = Get-LR600Double $qualificationText "USEFUL_DATA_MBPS"
$latencyAvgUs = Get-LR600Double $qualificationText "LATENCY_AVG_US"
$p95Us = Get-LR600Double $qualificationText "LATENCY_P95_US"
$p99Us = Get-LR600Double $qualificationText "LATENCY_P99_US"
$maxUs = Get-LR600Double $qualificationText "LATENCY_MAX_US"
$loopAvgUs = Get-LR600Double $qualificationText "LOOP_GAP_AVG_US"
$loopMaxUs = Get-LR600Double $qualificationText "LOOP_GAP_MAX_US"

$tcpTargetRequests = Convert-LR600Int $tcpRow.target_requests
$tcpRequestsSent = Convert-LR600Int $tcpRow.requests_sent
$tcpRequestsOk = Convert-LR600Int $tcpRow.requests_ok
$tcpTimeouts = Convert-LR600Int $tcpRow.timeouts
$tcpTransportErrors = Convert-LR600Int $tcpRow.transport_errors
$tcpProtocolErrors = Convert-LR600Int $tcpRow.protocol_errors
$tcpBusLockTimeouts =
    Convert-LR600Int $tcpRow.server_bus_lock_timeouts
$masterUnexpectedResets =
    Convert-LR600Int $tcpRow.unexpected_resets

$rtuStarted = Get-LR600Int $masterSnapshotText "RTU_REQUESTS_STARTED"
$rtuRejected = Get-LR600Int $masterSnapshotText "RTU_REQUESTS_REJECTED"
$rtuCompleted = Get-LR600Int $masterSnapshotText "RTU_REQUESTS_COMPLETED"
$rtuSuccess = Get-LR600Int $masterSnapshotText "RTU_REQUESTS_SUCCESS"
$rtuFailed = Get-LR600Int $masterSnapshotText "RTU_REQUESTS_FAILED"
$rtuVerifyFails = Get-LR600Int $masterSnapshotText "RTU_VERIFY_FAILS"
$rtuSkipped = Get-LR600Int $masterSnapshotText "RTU_PERIODS_SKIPPED"
$rtuCrcErrors = Get-LR600Int $masterSnapshotText "RTU_CRC_ERRORS"
$rtuTimeouts = Get-LR600Int $masterSnapshotText "RTU_MASTER_TIMEOUTS"
$rtuServiceGapMaxUs =
    Get-LR600Int $masterSnapshotText "RTU_SERVICE_GAP_MAX_US"
$rtuDurationMs =
    Get-LR600Int $masterSnapshotText "RTU_TRAFFIC_DURATION_MS"
$rtuBaudEffective =
    Get-LR600Int $masterSnapshotText "RTU_BAUD_EFFECTIVE"
$rtuPeriodUs = Get-LR600Int $masterSnapshotText "RTU_PERIOD_US"
$rtuTimeoutMs = Get-LR600Int $masterSnapshotText "RTU_TIMEOUT_MS"
$masterRtuMotor = Get-LR600Value $masterSnapshotText "RTU_MOTOR"
$masterRtuTxMode = Get-LR600Value $masterSnapshotText "RTU_TX_MODE"
$masterRtuQueued =
    Get-LR600Value $masterSnapshotText "RTU_TX_QUEUED_ACTIVE"

if ($rtuDurationMs -le 0) {
    throw "H3ER_LR600_RTU_DURATION_INVALID"
}

$rtuHz =
    [double]$rtuSuccess / ([double]$rtuDurationMs / 1000.0)
$slaveRtuRx = Get-LR600Int $slaveSnapshotText "RTU_RX_FRAMES"
$slaveRtuTx = Get-LR600Int $slaveSnapshotText "RTU_TX_FRAMES"
$slaveRtuOk = Get-LR600Int $slaveSnapshotText "RTU_REQUESTS_OK"
$slaveRtuBaudEffective =
    Get-LR600Int $slaveSnapshotText "RTU_BAUD_EFFECTIVE"
$slaveRtuMotor = Get-LR600Value $slaveSnapshotText "RTU_MOTOR"
$slaveRtuTxMode = Get-LR600Value $slaveSnapshotText "RTU_TX_MODE"
$slaveRtuQueued =
    Get-LR600Value $slaveSnapshotText "RTU_TX_QUEUED_ACTIVE"

$sdActive = Get-LR600Value $masterSnapshotText "SD_DATALOG_ACTIVE"
$sdBufferBytes =
    Get-LR600Int $masterSnapshotText "SD_DATALOG_BUFFER_BYTES"
$sdPendingBytes =
    Get-LR600Int $masterSnapshotText "SD_DATALOG_PENDING_BYTES"
$sdCommitThreshold =
    Get-LR600Int $masterSnapshotText "SD_DATALOG_COMMIT_THRESHOLD_BYTES"
$sdCommitTimeoutMs =
    Get-LR600Int $masterSnapshotText "SD_DATALOG_COMMIT_TIMEOUT_MS"
$sdAcceptedBytes =
    Get-LR600Int $masterSnapshotText "SD_DATALOG_ACCEPTED_BYTES"
$sdCommittedBytes =
    Get-LR600Int $masterSnapshotText "SD_DATALOG_COMMITTED_BYTES"
$sdCommitCount = Get-LR600Int $masterSnapshotText "SD_FLUSH_CYCLES"
$sdFailedCommits =
    Get-LR600Int $masterSnapshotText "SD_DATALOG_FAILED_COMMITS"
$sdAppendCycles = Get-LR600Int $masterSnapshotText "SD_APPEND_CYCLES"
$sdAppendFails = Get-LR600Int $masterSnapshotText "SD_APPEND_FAILS"
$sdAppendMaxUs = Get-LR600Int $masterSnapshotText "SD_APPEND_MAX_US"
$sdVerifyCycles = Get-LR600Int $masterSnapshotText "SD_VERIFY_CYCLES"
$sdVerifyFails = Get-LR600Int $masterSnapshotText "SD_VERIFY_FAILS"
$sdVerifyMaxUs = Get-LR600Int $masterSnapshotText "SD_VERIFY_MAX_US"
$sdWorkloadMode =
    Get-LR600Value $masterSnapshotText "SD_WORKLOAD_MODE"

$displayFrames = Get-LR600Int $masterSnapshotText "DISPLAY_FRAMES"
$displayGapMaxMs = Get-LR600Int $masterSnapshotText "DISPLAY_GAP_MAX_MS"
$displayRenderMode =
    Get-LR600Value $masterSnapshotText "DISPLAY_RENDER_MODE"
$displayRefreshMode =
    Get-LR600Value $masterSnapshotText "DISPLAY_REFRESH_MODE"
$slaveDisplayRenderMode =
    Get-LR600Value $slaveSnapshotText "DISPLAY_RENDER_MODE"
$slaveDisplayRefreshMode =
    Get-LR600Value $slaveSnapshotText "DISPLAY_REFRESH_MODE"
$framReady = Get-LR600Value $masterSnapshotText "FRAM_READY"
$framCycles = Get-LR600Int $masterSnapshotText "FRAM_CYCLES"
$framFails = Get-LR600Int $masterSnapshotText "FRAM_FAILS"
$framMaxUs = Get-LR600Int $masterSnapshotText "FRAM_MAX_US"
$rtcSamples = Get-LR600Int $masterSnapshotText "RTC_SAMPLES"
$rtcPresent = Get-LR600Value $masterSnapshotText "RTC_PRESENT"
$rtcUnavailable = Get-LR600Int $masterSnapshotText "RTC_UNAVAILABLE"
$rtcStale = Get-LR600Int $masterSnapshotText "RTC_STALE"
$rtcMaxAgeMs = Get-LR600Int $masterSnapshotText "RTC_MAX_AGE_MS"
$ioSamples = Get-LR600Int $masterSnapshotText "IO_SAMPLES"
$ioInitialized = Get-LR600Value $masterSnapshotText "IO_INITIALIZED"
$ioStale = Get-LR600Int $masterSnapshotText "IO_STALE"
$ioMaxAgeMs = Get-LR600Int $masterSnapshotText "IO_MAX_AGE_MS"
$buttonSamples = Get-LR600Int $masterSnapshotText "BUTTON_SAMPLES"
$buttonsReady = Get-LR600Value $masterSnapshotText "BUTTONS_READY"
$buttonNotReady = Get-LR600Int $masterSnapshotText "BUTTON_NOT_READY"
$buttonGapMaxMs =
    Get-LR600Int $masterSnapshotText "BUTTON_SAMPLE_GAP_MAX_MS"
$spiProbeSamples = Get-LR600Int $masterSnapshotText "SPI_PROBE_SAMPLES"
$spiProbeFails = Get-LR600Int $masterSnapshotText "SPI_PROBE_FAILS"
$spiProbeMaxWaitUs =
    Get-LR600Int $masterSnapshotText "SPI_PROBE_MAX_WAIT_US"
$spiProbeOver1Ms =
    Get-LR600Int $masterSnapshotText "SPI_PROBE_OVER_1MS"
$spiProbeOver10Ms =
    Get-LR600Int $masterSnapshotText "SPI_PROBE_OVER_10MS"
$peripheralFailures =
    Get-LR600Int $masterSnapshotText "PERIPHERAL_FAILURE_COUNT"

$masterTftRaw =
    Get-LR600Value $p5bTextResult "MASTER_TFT_PHYSICAL_PASS"
$slaveTftRaw =
    Get-LR600Value $p5bTextResult "SLAVE_TFT_PHYSICAL_PASS"
$masterTftPass = $masterTftRaw.ToUpperInvariant() -eq "TRUE"
$slaveTftPass = $slaveTftRaw.ToUpperInvariant() -eq "TRUE"

$combinedCompileText = $masterCompileText + "`n" + $slaveCompileText

foreach ($macro in @(
    "JWPLC_W5500_RX_FIFO_REUSE",
    "JWPLC_W5500_RX_DIRECT_TRANSFER_BYTES",
    "JWPLC_SPI_FIFO_REUSE_DLEN_CACHE",
    "JWPLC_SPI_FIFO_REUSE_COPY_OUT_64"
)) {
    if ($combinedCompileText -match (
        '(?i)-D' + [regex]::Escape($macro) + '=')) {
        throw "H3ER_LR600_BUILD_OVERRIDE=$macro"
    }
}

$masterCompileNormalized =
    $masterCompileText.Replace("\", "/").ToLowerInvariant()
$slaveCompileNormalized =
    $slaveCompileText.Replace("\", "/").ToLowerInvariant()

foreach ($token in @(
    "jwplc_display.cpp",
    "jwplc_ui.cpp",
    "jwplc_tft.cpp",
    "spi.cpp",
    "jw_sd.cpp",
    "jwplc_modbustcp.cpp",
    "ethernetclient.cpp",
    "socket.cpp",
    "w5100.cpp"
)) {
    if (-not $masterCompileNormalized.Contains($token)) {
        throw "H3ER_LR600_MASTER_SOURCE_LINKAGE_MISSING=$token"
    }
}

foreach ($token in @(
    "jwplc_display.cpp",
    "jwplc_ui.cpp",
    "jwplc_tft.cpp",
    "spi.cpp"
)) {
    if (-not $slaveCompileNormalized.Contains($token)) {
        throw "H3ER_LR600_SLAVE_SOURCE_LINKAGE_MISSING=$token"
    }
}

foreach ($archive in @(
    "libjwplc_display.a",
    "libjwplc_tft.a",
    "libspi.a"
)) {
    if ($masterCompileNormalized.Contains($archive) -or
        $slaveCompileNormalized.Contains($archive)) {
        throw "H3ER_LR600_STALE_ARCHIVE_LINKED=$archive"
    }
}

$masterRtuBuild =
    Get-LR600ModbusRtuBuildEvidence $masterCompileNormalized
$slaveRtuBuild =
    Get-LR600ModbusRtuBuildEvidence $slaveCompileNormalized

foreach ($item in @(
    [PSCustomObject]@{ Label = "MASTER"; Evidence = $masterRtuBuild },
    [PSCustomObject]@{ Label = "SLAVE"; Evidence = $slaveRtuBuild }
)) {
    if (-not $item.Evidence.SourceCompiled -or
        -not $item.Evidence.SourceLinked -or
        $item.Evidence.ArchiveLinked -or
        $item.Evidence.PrecompiledMarker) {
        throw "H3ER_LR600_$($item.Label)_MODBUS_RTU_SOURCE_FIRST_FAILED"
    }
}

$p95GuardUs = 1397.9
$p99GuardUs = 4215.9
$aggregateP95GuardPass = $p95Us -le $p95GuardUs
$aggregateP99GuardPass = $p99Us -le $p99GuardUs
$bucketGuardCrossed = $false

foreach ($bucket in $buckets) {
    $bucket | Add-Member `
        -NotePropertyName P95GuardPass `
        -NotePropertyValue ($bucket.P95Us -le $p95GuardUs)
    $bucket | Add-Member `
        -NotePropertyName P99GuardPass `
        -NotePropertyValue ($bucket.P99Us -le $p99GuardUs)

    if (-not $bucket.P95GuardPass -or -not $bucket.P99GuardPass) {
        $bucketGuardCrossed = $true
    }
}

[double[]]$reqValues = @($buckets | ForEach-Object { $_.ReqS })
[double[]]$avgValues = @($buckets | ForEach-Object { $_.AvgUs })
[double[]]$p95Values = @($buckets | ForEach-Object { $_.P95Us })
[double[]]$p99Values = @($buckets | ForEach-Object { $_.P99Us })
[double[]]$maxValues = @($buckets | ForEach-Object { $_.MaxUs })

$reqMin = ($reqValues | Measure-Object -Minimum).Minimum
$reqMedian = Get-LR600Median $reqValues
$reqMax = ($reqValues | Measure-Object -Maximum).Maximum
$reqSpreadPct = Get-LR600SpreadPct $reqValues
$avgMin = ($avgValues | Measure-Object -Minimum).Minimum
$avgMedian = Get-LR600Median $avgValues
$avgMax = ($avgValues | Measure-Object -Maximum).Maximum
$p95Min = ($p95Values | Measure-Object -Minimum).Minimum
$p95Median = Get-LR600Median $p95Values
$p95Max = ($p95Values | Measure-Object -Maximum).Maximum
$p99Min = ($p99Values | Measure-Object -Minimum).Minimum
$p99Median = Get-LR600Median $p99Values
$p99Max = ($p99Values | Measure-Object -Maximum).Maximum
$maxMin = ($maxValues | Measure-Object -Minimum).Minimum
$maxMedian = Get-LR600Median $maxValues
$maxMax = ($maxValues | Measure-Object -Maximum).Maximum

$firstReqS = $buckets[0].ReqS
$lastReqS = $buckets[9].ReqS
$reqDriftPct = Get-LR600DeltaPct $lastReqS $firstReqS
$firstP95 = $buckets[0].P95Us
$lastP95 = $buckets[9].P95Us
$p95DriftPct = Get-LR600DeltaPct $lastP95 $firstP95
$firstP99 = $buckets[0].P99Us
$lastP99 = $buckets[9].P99Us
$p99DriftPct = Get-LR600DeltaPct $lastP99 $firstP99

[double[]]$first3P95 = @($buckets[0..2] | ForEach-Object { $_.P95Us })
[double[]]$last3P95 = @($buckets[7..9] | ForEach-Object { $_.P95Us })
[double[]]$first3P99 = @($buckets[0..2] | ForEach-Object { $_.P99Us })
[double[]]$last3P99 = @($buckets[7..9] | ForEach-Object { $_.P99Us })
$first3P95Median = Get-LR600Median $first3P95
$last3P95Median = Get-LR600Median $last3P95
$p95First3Last3Delta =
    Get-LR600DeltaPct $last3P95Median $first3P95Median
$first3P99Median = Get-LR600Median $first3P99
$last3P99Median = Get-LR600Median $last3P99
$p99First3Last3Delta =
    Get-LR600DeltaPct $last3P99Median $first3P99Median

$clearSustainedTailWorsening =
    (($last3P95 | Measure-Object -Minimum).Minimum -gt
        ($first3P95 | Measure-Object -Maximum).Maximum) -and
    (($last3P99 | Measure-Object -Minimum).Minimum -gt
        ($first3P99 | Measure-Object -Maximum).Maximum)

$shortAvgUs = 880.3
$shortP95Us = 1253.2
$shortP99Us = 2273.1
$shortMaxUs = 8831.5
$shortLoopMaxUs = 12012.0
$shortReqS = 1000.0

$functionalFailures = New-Object System.Collections.Generic.List[string]
$rtuCrossCountPass =
    $slaveRtuRx -eq $rtuSuccess -and
    $slaveRtuTx -eq $rtuSuccess -and
    $slaveRtuOk -eq $rtuSuccess -and
    $qualificationText.Contains("RTU_CROSS_COUNT_PASS=YES")

if ($requestedReqS -ne 1000.0 -or $achievedPct -lt 99.9) {
    [void]$functionalFailures.Add("TCP_RATE")
}
if ($tcpRequestsSent -ne $tcpRequestsOk) {
    [void]$functionalFailures.Add("TCP_REQUEST_COUNT")
}
if ($tcpTimeouts -ne 0 -or
    $tcpTransportErrors -ne 0 -or
    $tcpProtocolErrors -ne 0 -or
    $tcpBusLockTimeouts -ne 0) {
    [void]$functionalFailures.Add("TCP_ERRORS")
}
if ($rtuHz -lt 49.5 -or $rtuHz -gt 50.5 -or
    $rtuRejected -ne 0 -or $rtuFailed -ne 0 -or
    $rtuVerifyFails -ne 0 -or $rtuSkipped -ne 0 -or
    $rtuCrcErrors -ne 0 -or $rtuTimeouts -ne 0 -or
    $rtuStarted -ne $rtuCompleted -or
    $rtuCompleted -ne $rtuSuccess -or
    $rtuBaudEffective -ne 115200 -or
    $slaveRtuBaudEffective -ne 115200 -or
    $rtuPeriodUs -ne 20000 -or $rtuTimeoutMs -ne 25 -or
    $masterRtuMotor -ne "ASYNC" -or
    $slaveRtuMotor -ne "ASYNC" -or
    $masterRtuTxMode -ne "QUEUED" -or
    $slaveRtuTxMode -ne "QUEUED" -or
    $masterRtuQueued -ne "YES" -or
    $slaveRtuQueued -ne "YES") {
    [void]$functionalFailures.Add("RTU")
}
if (-not $rtuCrossCountPass) {
    [void]$functionalFailures.Add("RTU_CROSS_COUNT")
}
if ($sdActive -ne "YES" -or $sdCommittedBytes -le 0 -or
    $sdFailedCommits -ne 0 -or $sdAppendFails -ne 0 -or
    $sdVerifyFails -ne 0 -or
    $sdWorkloadMode -ne "BUFFERED_DATALOG") {
    [void]$functionalFailures.Add("DATALOG")
}
if ($peripheralFailures -ne 0 -or
    $framReady -ne "YES" -or $framFails -ne 0 -or
    $rtcPresent -ne "YES" -or
    $rtcUnavailable -ne 0 -or $rtcStale -ne 0 -or
    $ioInitialized -ne "YES" -or $ioStale -ne 0 -or
    $buttonsReady -ne "YES" -or $buttonNotReady -ne 0 -or
    $spiProbeFails -ne 0 -or
    $displayRenderMode -ne "HMI_ON_DEMAND_DIRTY" -or
    $displayRefreshMode -ne "USER_REFRESH_ON_DEMAND" -or
    $slaveDisplayRenderMode -ne "HMI_ON_DEMAND_DIRTY" -or
    $slaveDisplayRefreshMode -ne "USER_REFRESH_ON_DEMAND") {
    [void]$functionalFailures.Add("PERIPHERALS")
}
if ($masterUnexpectedResets -ne 0) {
    [void]$functionalFailures.Add("MASTER_RESET")
}
if (-not $masterTftPass -or -not $slaveTftPass) {
    [void]$functionalFailures.Add("TFT_PHYSICAL")
}

$tailGuardCrossed =
    -not $aggregateP95GuardPass -or
    -not $aggregateP99GuardPass -or
    $bucketGuardCrossed
$temporalDegradation = "NOT_PRESENT"

if ($tailGuardCrossed -or $clearSustainedTailWorsening) {
    $temporalDegradation = "REVIEW"
}

$script:LR600SummaryLines =
    New-Object System.Collections.Generic.List[string]

Write-Host ""
Write-Host "=== H3E-R LR600 BUCKETS ==="

foreach ($bucket in $buckets) {
    $line =
        "H3ER_LR600_BUCKET_$($bucket.Index)_REQ_S=$(Format-LR600Double $bucket.ReqS 'F2') " +
        "H3ER_LR600_BUCKET_$($bucket.Index)_OK=$($bucket.Ok) " +
        "H3ER_LR600_BUCKET_$($bucket.Index)_AVG_US=$(Format-LR600Double $bucket.AvgUs 'F1') " +
        "H3ER_LR600_BUCKET_$($bucket.Index)_P95_US=$(Format-LR600Double $bucket.P95Us 'F1') " +
        "H3ER_LR600_BUCKET_$($bucket.Index)_P99_US=$(Format-LR600Double $bucket.P99Us 'F1') " +
        "H3ER_LR600_BUCKET_$($bucket.Index)_MAX_US=$(Format-LR600Double $bucket.MaxUs 'F1') " +
        "H3ER_LR600_BUCKET_$($bucket.Index)_P95_GUARD_PASS=$($bucket.P95GuardPass.ToString().ToUpperInvariant()) " +
        "H3ER_LR600_BUCKET_$($bucket.Index)_P99_GUARD_PASS=$($bucket.P99GuardPass.ToString().ToUpperInvariant())"
    Write-LR600SummaryLine $line
}

Write-Host ""
Write-Host "=== H3E-R LR600 SUMMARY ==="

foreach ($line in @(
    "H3ER_LR600_TCP_REQUESTED_REQ_S=$(Format-LR600Double $requestedReqS 'F2')",
    "H3ER_LR600_TCP_ACHIEVED_REQ_S=$(Format-LR600Double $achievedReqS 'F2')",
    "H3ER_LR600_TCP_ACHIEVED_PCT=$(Format-LR600Double $achievedPct 'F3')",
    "H3ER_LR600_TCP_TARGET_REQUESTS=$tcpTargetRequests",
    "H3ER_LR600_TCP_REQUESTS_SENT=$tcpRequestsSent",
    "H3ER_LR600_TCP_REQUESTS_OK=$tcpRequestsOk",
    "H3ER_LR600_TCP_TOTAL_MBPS=$(Format-LR600Double $totalMbps 'F4')",
    "H3ER_LR600_TCP_USEFUL_MBPS=$(Format-LR600Double $usefulMbps 'F4')",
    "H3ER_LR600_TCP_LATENCY_AVG_US=$(Format-LR600Double $latencyAvgUs 'F1')",
    "H3ER_LR600_TCP_P95_US=$(Format-LR600Double $p95Us 'F1')",
    "H3ER_LR600_TCP_P99_US=$(Format-LR600Double $p99Us 'F1')",
    "H3ER_LR600_TCP_MAX_US=$(Format-LR600Double $maxUs 'F1')",
    "H3ER_LR600_TCP_TIMEOUTS=$tcpTimeouts",
    "H3ER_LR600_TCP_TRANSPORT_ERRORS=$tcpTransportErrors",
    "H3ER_LR600_TCP_PROTOCOL_ERRORS=$tcpProtocolErrors",
    "H3ER_LR600_TCP_BUS_LOCK_TIMEOUTS=$tcpBusLockTimeouts",
    "H3ER_LR600_REQ_S_MIN=$(Format-LR600Double $reqMin 'F2')",
    "H3ER_LR600_REQ_S_MEDIAN=$(Format-LR600Double $reqMedian 'F2')",
    "H3ER_LR600_REQ_S_MAX=$(Format-LR600Double $reqMax 'F2')",
    "H3ER_LR600_REQ_S_SPREAD_PCT=$(Format-LR600Double $reqSpreadPct 'F3')",
    "H3ER_LR600_AVG_US_MIN=$(Format-LR600Double $avgMin 'F1')",
    "H3ER_LR600_AVG_US_MEDIAN=$(Format-LR600Double $avgMedian 'F1')",
    "H3ER_LR600_AVG_US_MAX=$(Format-LR600Double $avgMax 'F1')",
    "H3ER_LR600_P95_US_MIN=$(Format-LR600Double $p95Min 'F1')",
    "H3ER_LR600_P95_US_MEDIAN=$(Format-LR600Double $p95Median 'F1')",
    "H3ER_LR600_P95_US_MAX=$(Format-LR600Double $p95Max 'F1')",
    "H3ER_LR600_P99_US_MIN=$(Format-LR600Double $p99Min 'F1')",
    "H3ER_LR600_P99_US_MEDIAN=$(Format-LR600Double $p99Median 'F1')",
    "H3ER_LR600_P99_US_MAX=$(Format-LR600Double $p99Max 'F1')",
    "H3ER_LR600_MAX_US_MIN=$(Format-LR600Double $maxMin 'F1')",
    "H3ER_LR600_MAX_US_MEDIAN=$(Format-LR600Double $maxMedian 'F1')",
    "H3ER_LR600_MAX_US_MAX=$(Format-LR600Double $maxMax 'F1')",
    "H3ER_LR600_FIRST_MINUTE_REQ_S=$(Format-LR600Double $firstReqS 'F2')",
    "H3ER_LR600_LAST_MINUTE_REQ_S=$(Format-LR600Double $lastReqS 'F2')",
    "H3ER_LR600_REQ_S_DRIFT_PCT=$(Format-LR600Double $reqDriftPct 'F3')",
    "H3ER_LR600_FIRST_MINUTE_P95=$(Format-LR600Double $firstP95 'F1')",
    "H3ER_LR600_LAST_MINUTE_P95=$(Format-LR600Double $lastP95 'F1')",
    "H3ER_LR600_P95_DRIFT_PCT=$(Format-LR600Double $p95DriftPct 'F3')",
    "H3ER_LR600_FIRST_MINUTE_P99=$(Format-LR600Double $firstP99 'F1')",
    "H3ER_LR600_LAST_MINUTE_P99=$(Format-LR600Double $lastP99 'F1')",
    "H3ER_LR600_P99_DRIFT_PCT=$(Format-LR600Double $p99DriftPct 'F3')",
    "H3ER_LR600_FIRST_3_BUCKETS_P95_MEDIAN=$(Format-LR600Double $first3P95Median 'F1')",
    "H3ER_LR600_LAST_3_BUCKETS_P95_MEDIAN=$(Format-LR600Double $last3P95Median 'F1')",
    "H3ER_LR600_P95_FIRST3_VS_LAST3_DELTA_PCT=$(Format-LR600Double $p95First3Last3Delta 'F3')",
    "H3ER_LR600_FIRST_3_BUCKETS_P99_MEDIAN=$(Format-LR600Double $first3P99Median 'F1')",
    "H3ER_LR600_LAST_3_BUCKETS_P99_MEDIAN=$(Format-LR600Double $last3P99Median 'F1')",
    "H3ER_LR600_P99_FIRST3_VS_LAST3_DELTA_PCT=$(Format-LR600Double $p99First3Last3Delta 'F3')",
    "H3ER_LR600_P95_GUARD_US=1397.9",
    "H3ER_LR600_P99_GUARD_US=4215.9",
    "H3ER_LR600_AGGREGATE_P95_GUARD_PASS=$($aggregateP95GuardPass.ToString().ToUpperInvariant())",
    "H3ER_LR600_AGGREGATE_P99_GUARD_PASS=$($aggregateP99GuardPass.ToString().ToUpperInvariant())",
    "H3ER_LR600_MAX_POLICY=DIAGNOSTIC_ONLY",
    "H3ER_LR600_LOOP_AVG_US=$(Format-LR600Double $loopAvgUs 'F1')",
    "H3ER_LR600_LOOP_MAX_US=$(Format-LR600Double $loopMaxUs 'F1')",
    "H3ER_LR600_LOOP_MAX_POLICY=DIAGNOSTIC_ONLY",
    "H3ER_LR600_DELTA_VS_120S_AVG_PCT=$(Format-LR600Double (Get-LR600DeltaPct $latencyAvgUs $shortAvgUs) 'F3')",
    "H3ER_LR600_DELTA_VS_120S_P95_PCT=$(Format-LR600Double (Get-LR600DeltaPct $p95Us $shortP95Us) 'F3')",
    "H3ER_LR600_DELTA_VS_120S_P99_PCT=$(Format-LR600Double (Get-LR600DeltaPct $p99Us $shortP99Us) 'F3')",
    "H3ER_LR600_DELTA_VS_120S_MAX_PCT=$(Format-LR600Double (Get-LR600DeltaPct $maxUs $shortMaxUs) 'F3')",
    "H3ER_LR600_DELTA_VS_120S_LOOP_MAX_PCT=$(Format-LR600Double (Get-LR600DeltaPct $loopMaxUs $shortLoopMaxUs) 'F3')",
    "H3ER_LR600_DELTA_VS_120S_REQ_S_PCT=$(Format-LR600Double (Get-LR600DeltaPct $achievedReqS $shortReqS) 'F3')",
    "H3ER_LR600_RTU_ACHIEVED_HZ=$(Format-LR600Double $rtuHz 'F3')",
    "H3ER_LR600_RTU_REQUESTS_STARTED=$rtuStarted",
    "H3ER_LR600_RTU_REQUESTS_REJECTED=$rtuRejected",
    "H3ER_LR600_RTU_REQUESTS_COMPLETED=$rtuCompleted",
    "H3ER_LR600_RTU_REQUESTS_SUCCESS=$rtuSuccess",
    "H3ER_LR600_RTU_REQUESTS_FAILED=$rtuFailed",
    "H3ER_LR600_RTU_VERIFY_FAILS=$rtuVerifyFails",
    "H3ER_LR600_RTU_PERIODS_SKIPPED=$rtuSkipped",
    "H3ER_LR600_RTU_CRC_ERRORS=$rtuCrcErrors",
    "H3ER_LR600_RTU_TIMEOUTS=$rtuTimeouts",
    "H3ER_LR600_RTU_BAUD_EFFECTIVE=$rtuBaudEffective",
    "H3ER_LR600_RTU_CONFIG=8N1",
    "H3ER_LR600_RTU_PERIOD_US=$rtuPeriodUs",
    "H3ER_LR600_RTU_TIMEOUT_MS=$rtuTimeoutMs",
    "H3ER_LR600_RTU_MASTER_MOTOR=$masterRtuMotor",
    "H3ER_LR600_RTU_MASTER_TX_MODE=$masterRtuTxMode",
    "H3ER_LR600_RTU_MASTER_TX_QUEUED_ACTIVE=$masterRtuQueued",
    "H3ER_LR600_RTU_SLAVE_MOTOR=$slaveRtuMotor",
    "H3ER_LR600_RTU_SLAVE_TX_MODE=$slaveRtuTxMode",
    "H3ER_LR600_RTU_SLAVE_TX_QUEUED_ACTIVE=$slaveRtuQueued",
    "H3ER_LR600_RTU_SERVICE_GAP_MAX_US=$rtuServiceGapMaxUs",
    "H3ER_LR600_SLAVE_RTU_RX=$slaveRtuRx",
    "H3ER_LR600_SLAVE_RTU_TX=$slaveRtuTx",
    "H3ER_LR600_SLAVE_RTU_OK=$slaveRtuOk",
    "H3ER_LR600_RTU_CROSS_COUNT=$(if($rtuCrossCountPass){'PASS'}else{'FAIL'})",
    "H3ER_LR600_DATALOG_ACTIVE=$sdActive",
    "H3ER_LR600_DATALOG_BUFFER_BYTES=$sdBufferBytes",
    "H3ER_LR600_DATALOG_PENDING_BYTES=$sdPendingBytes",
    "H3ER_LR600_DATALOG_COMMIT_THRESHOLD_BYTES=$sdCommitThreshold",
    "H3ER_LR600_DATALOG_COMMIT_TIMEOUT_MS=$sdCommitTimeoutMs",
    "H3ER_LR600_DATALOG_ACCEPTED_BYTES=$sdAcceptedBytes",
    "H3ER_LR600_DATALOG_COMMITTED_BYTES=$sdCommittedBytes",
    "H3ER_LR600_DATALOG_COMMIT_COUNT=$sdCommitCount",
    "H3ER_LR600_DATALOG_FAILED_COMMITS=$sdFailedCommits",
    "H3ER_LR600_DATALOG_APPEND_CYCLES=$sdAppendCycles",
    "H3ER_LR600_DATALOG_APPEND_FAILS=$sdAppendFails",
    "H3ER_LR600_DATALOG_APPEND_MAX_US=$sdAppendMaxUs",
    "H3ER_LR600_DATALOG_VERIFY_CYCLES=$sdVerifyCycles",
    "H3ER_LR600_DATALOG_VERIFY_FAILS=$sdVerifyFails",
    "H3ER_LR600_DATALOG_VERIFY_MAX_US=$sdVerifyMaxUs",
    "H3ER_LR600_DATALOG_WORKLOAD_MODE=$sdWorkloadMode",
    "H3ER_LR600_DATALOG_MANUAL_SERVICE=NO",
    "H3ER_LR600_DISPLAY_FRAMES=$displayFrames",
    "H3ER_LR600_DISPLAY_GAP_MAX_MS=$displayGapMaxMs",
    "H3ER_LR600_DISPLAY_RENDER_MODE=$displayRenderMode",
    "H3ER_LR600_DISPLAY_REFRESH_MODE=$displayRefreshMode",
    "H3ER_LR600_SLAVE_DISPLAY_RENDER_MODE=$slaveDisplayRenderMode",
    "H3ER_LR600_SLAVE_DISPLAY_REFRESH_MODE=$slaveDisplayRefreshMode",
    "H3ER_LR600_FRAM_READY=$framReady",
    "H3ER_LR600_FRAM_CYCLES=$framCycles",
    "H3ER_LR600_FRAM_FAILS=$framFails",
    "H3ER_LR600_FRAM_MAX_US=$framMaxUs",
    "H3ER_LR600_RTC_PRESENT=$rtcPresent",
    "H3ER_LR600_RTC_SAMPLES=$rtcSamples",
    "H3ER_LR600_RTC_UNAVAILABLE=$rtcUnavailable",
    "H3ER_LR600_RTC_STALE=$rtcStale",
    "H3ER_LR600_RTC_MAX_AGE_MS=$rtcMaxAgeMs",
    "H3ER_LR600_IO_INITIALIZED=$ioInitialized",
    "H3ER_LR600_IO_SAMPLES=$ioSamples",
    "H3ER_LR600_IO_STALE=$ioStale",
    "H3ER_LR600_IO_MAX_AGE_MS=$ioMaxAgeMs",
    "H3ER_LR600_BUTTONS_READY=$buttonsReady",
    "H3ER_LR600_BUTTON_SAMPLES=$buttonSamples",
    "H3ER_LR600_BUTTON_NOT_READY=$buttonNotReady",
    "H3ER_LR600_BUTTON_SAMPLE_GAP_MAX_MS=$buttonGapMaxMs",
    "H3ER_LR600_SPI_PROBE_SAMPLES=$spiProbeSamples",
    "H3ER_LR600_SPI_PROBE_FAILS=$spiProbeFails",
    "H3ER_LR600_SPI_PROBE_MAX_WAIT_US=$spiProbeMaxWaitUs",
    "H3ER_LR600_SPI_PROBE_OVER_1MS=$spiProbeOver1Ms",
    "H3ER_LR600_SPI_PROBE_OVER_10MS=$spiProbeOver10Ms",
    "H3ER_LR600_PERIPHERAL_FAILURE_COUNT=$peripheralFailures",
    "H3ER_LR600_MASTER_UNEXPECTED_RESETS=$masterUnexpectedResets",
    "H3ER_LR600_SLAVE_RESET_DIRECT_COUNTER=NOT_EXPOSED",
    "H3ER_LR600_SLAVE_RESET_GUARD=RTU_CROSS_COUNT_AND_QUIESCED_FINAL_SNAPSHOT",
    "H3ER_LR600_MASTER_TFT_PHYSICAL=$(if($masterTftPass){'PASS'}else{'FAIL'})",
    "H3ER_LR600_SLAVE_TFT_PHYSICAL=$(if($slaveTftPass){'PASS'}else{'FAIL'})",
    "H3ER_LR600_TFT_PHYSICAL_PASS=$(if($masterTftPass -and $slaveTftPass){'PASS'}else{'FAIL'})",
    "H3ER_LR600_MODBUS_RTU_POLICY=SOURCE_FIRST",
    "H3ER_LR600_MASTER_MODBUS_RTU_SOURCE_COMPILED=YES",
    "H3ER_LR600_MASTER_MODBUS_RTU_SOURCE_LINKED=YES",
    "H3ER_LR600_MASTER_MODBUS_RTU_ARCHIVE_LINKED=NO",
    "H3ER_LR600_SLAVE_MODBUS_RTU_SOURCE_COMPILED=YES",
    "H3ER_LR600_SLAVE_MODBUS_RTU_SOURCE_LINKED=YES",
    "H3ER_LR600_SLAVE_MODBUS_RTU_ARCHIVE_LINKED=NO",
    "H3ER_LR600_TAIL_GUARD=$(if($tailGuardCrossed){'REVIEW'}else{'PASS'})",
    "H3ER_LR600_SUSTAINED_TAIL_WORSENING=$(if($clearSustainedTailWorsening){'YES'}else{'NO'})",
    "H3ER_LR600_TEMPORAL_DEGRADATION=$temporalDegradation",
    "H3ER_LR600_FUNCTIONAL_FAILURE_COUNT=$($functionalFailures.Count)",
    "H3ER_LR600_RESULT_ROOT=$resultRoot"
)) {
    Write-LR600SummaryLine $line
}

if ($functionalFailures.Count -eq 0) {
    Write-LR600SummaryLine "H3ER_LR600_FUNCTIONAL=PASS"

    if ($temporalDegradation -eq "REVIEW") {
        Write-LR600SummaryLine "A14_H3ER_LR600=REVIEW"
    }
    else {
        Write-LR600SummaryLine "A14_H3ER_LR600=PASS"
    }
}
else {
    Write-LR600SummaryLine (
        "H3ER_LR600_FUNCTIONAL_FAILURES=" +
        ($functionalFailures -join ","))
    Write-LR600SummaryLine "H3ER_LR600_FUNCTIONAL=FAIL"
    Write-LR600SummaryLine "A14_H3ER_LR600=FAIL"
}

$summaryPath = Join-Path $resultRoot "SUMMARY.log"
Set-Content `
    -LiteralPath $summaryPath `
    -Value $script:LR600SummaryLines `
    -Encoding UTF8

Write-Host "H3ER_LR600_SUMMARY_LOG=$summaryPath"

if ($functionalFailures.Count -ne 0) {
    throw "H3ER_LR600_FUNCTIONAL_FAIL"
}

Write-Host "NEXT=RETURN_LR600_RESULT_FOR_REVIEW"
