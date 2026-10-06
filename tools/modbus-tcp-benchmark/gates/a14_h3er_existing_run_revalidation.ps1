param(
    [string]$RunDir = (
        Join-Path $PSScriptRoot `
            "..\results\20260930_181806_h3er_post_p4_1")
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

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
        throw "H3ER_OFFLINE_KEY_MISSING=$Key"
    }

    [string[]]$values = @(
        $matches |
            ForEach-Object { $_.Groups[1].Value.Trim() } |
            Select-Object -Unique
    )

    if ($values.Count -ne 1) {
        throw (
            "H3ER_OFFLINE_KEY_VALUES_CONFLICT={0}:{1}" -f
            $Key,
            ($values -join ","))
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

function Assert-H3EREqual {
    param(
        [string]$Text,
        [string]$Key,
        [string]$Expected
    )

    $actual = Get-H3ERValue -Text $Text -Key $Key
    if ($actual -ne $Expected) {
        throw "H3ER_OFFLINE_${Key}_EXPECTED_${Expected}_GOT_$actual"
    }
}

$resolvedRunDir =
    (Resolve-Path -LiteralPath $RunDir).Path

$paths = @{
    Qualification = Join-Path $resolvedRunDir "qualification.log"
    Master = Join-Path $resolvedRunDir "master_final.txt"
    Slave = Join-Path $resolvedRunDir "slave_final.txt"
    MasterCompile = Join-Path $resolvedRunDir "compile_master.log"
    SlaveCompile = Join-Path $resolvedRunDir "compile_slave.log"
    Wrapper = Join-Path $resolvedRunDir "H3ER_WRAPPER_RESULT.txt"
}

foreach ($entry in $paths.GetEnumerator()) {
    if (-not (Test-Path -LiteralPath $entry.Value -PathType Leaf)) {
        throw "H3ER_OFFLINE_ARTIFACT_MISSING=$($entry.Key):$($entry.Value)"
    }
}

$qualificationText =
    [IO.File]::ReadAllText($paths.Qualification)
$masterText =
    [IO.File]::ReadAllText($paths.Master)
$slaveText =
    [IO.File]::ReadAllText($paths.Slave)
$masterCompileText =
    [IO.File]::ReadAllText($paths.MasterCompile)
$slaveCompileText =
    [IO.File]::ReadAllText($paths.SlaveCompile)
$wrapperText =
    [IO.File]::ReadAllText($paths.Wrapper)

foreach ($marker in @(
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
    if (-not $qualificationText.Contains($marker)) {
        throw "H3ER_OFFLINE_REQUIRED_MARKER_MISSING=$marker"
    }
}

$requestedReqS =
    Get-H3ERDouble -Text $qualificationText -Key "REQUESTED_REQ_S"
$achievedReqS =
    Get-H3ERDouble -Text $qualificationText -Key "ACHIEVED_REQ_S"
$achievedPct =
    Get-H3ERDouble -Text $qualificationText -Key "ACHIEVED_PCT"
$p95Us =
    Get-H3ERDouble -Text $qualificationText -Key "LATENCY_P95_US"
$p99Us =
    Get-H3ERDouble -Text $qualificationText -Key "LATENCY_P99_US"
$maxUs =
    Get-H3ERDouble -Text $qualificationText -Key "LATENCY_MAX_US"

if ($requestedReqS -ne 1000.0 -or $achievedPct -lt 99.9) {
    throw "H3ER_OFFLINE_TCP_TARGET_FAIL=$achievedReqS/$requestedReqS"
}

if ($p95Us -gt (1275.5 * 1.10)) {
    throw "H3ER_OFFLINE_P95_GUARD_FAIL=$p95Us"
}

if ($p99Us -gt (3781.0 * 1.15)) {
    throw "H3ER_OFFLINE_P99_GUARD_FAIL=$p99Us"
}

foreach ($expectation in @(
    @($masterText, "RTU_MOTOR", "ASYNC"),
    @($masterText, "RTU_TX_MODE", "QUEUED"),
    @($masterText, "RTU_TX_QUEUED_ACTIVE", "YES"),
    @($masterText, "RTU_RX_MODE", "BYTE"),
    @($masterText, "RTU_CRC_MODE", "BITWISE"),
    @($masterText, "RTU_SERVER_FRAMING", "GAP"),
    @($masterText, "DISPLAY_RENDER_MODE", "HMI_ON_DEMAND_DIRTY"),
    @($masterText, "DISPLAY_REFRESH_MODE", "USER_REFRESH_ON_DEMAND"),
    @($slaveText, "RTU_MOTOR", "ASYNC"),
    @($slaveText, "RTU_TX_MODE", "QUEUED"),
    @($slaveText, "RTU_TX_QUEUED_ACTIVE", "YES"),
    @($slaveText, "RTU_RX_MODE", "BYTE"),
    @($slaveText, "RTU_CRC_MODE", "BITWISE"),
    @($slaveText, "RTU_SERVER_FRAMING", "GAP"),
    @($slaveText, "DISPLAY_RENDER_MODE", "HMI_ON_DEMAND_DIRTY"),
    @($slaveText, "DISPLAY_REFRESH_MODE", "USER_REFRESH_ON_DEMAND")
)) {
    Assert-H3EREqual `
        -Text $expectation[0] `
        -Key $expectation[1] `
        -Expected $expectation[2]
}

$rtuStarted =
    Get-H3ERInt -Text $masterText -Key "RTU_REQUESTS_STARTED"
$rtuSuccess =
    Get-H3ERInt -Text $masterText -Key "RTU_REQUESTS_SUCCESS"
$rtuDurationMs =
    Get-H3ERInt -Text $masterText -Key "RTU_TRAFFIC_DURATION_MS"
$rtuHz =
    [double]$rtuSuccess / ([double]$rtuDurationMs / 1000.0)

if ($rtuStarted -ne $rtuSuccess -or
    $rtuHz -lt 49.5 -or $rtuHz -gt 50.5) {
    throw "H3ER_OFFLINE_RTU_RATE_OR_SUCCESS_FAIL=$rtuSuccess/$rtuStarted@$rtuHz"
}

foreach ($zeroKey in @(
    "RTU_PERIODS_SKIPPED",
    "RTU_REQUESTS_FAILED",
    "RTU_VERIFY_FAILS",
    "RTU_CRC_ERRORS",
    "RTU_MASTER_TIMEOUTS",
    "SD_DATALOG_FAILED_COMMITS",
    "PERIPHERAL_FAILURE_COUNT"
)) {
    if ((Get-H3ERInt -Text $masterText -Key $zeroKey) -ne 0) {
        throw "H3ER_OFFLINE_NONZERO_MASTER_COUNTER=$zeroKey"
    }
}

foreach ($zeroKey in @(
    "RTU_CRC_ERRORS",
    "RTU_EXCEPTIONS_SENT",
    "RTU_MASTER_TIMEOUTS"
)) {
    if ((Get-H3ERInt -Text $slaveText -Key $zeroKey) -ne 0) {
        throw "H3ER_OFFLINE_NONZERO_SLAVE_COUNTER=$zeroKey"
    }
}

Assert-H3EREqual -Text $masterText -Key "SD_DATALOG_ACTIVE" -Expected "YES"
Assert-H3EREqual -Text $masterText -Key "RTU_LAST_ERROR" -Expected "OK"
Assert-H3EREqual -Text $slaveText -Key "RTU_LAST_ERROR" -Expected "OK"

$sdAccepted =
    Get-H3ERInt -Text $masterText -Key "SD_DATALOG_ACCEPTED_BYTES"
$sdCommitted =
    Get-H3ERInt -Text $masterText -Key "SD_DATALOG_COMMITTED_BYTES"
$sdPending =
    Get-H3ERInt -Text $masterText -Key "SD_DATALOG_PENDING_BYTES"
$sdThreshold =
    Get-H3ERInt -Text $masterText -Key "SD_DATALOG_COMMIT_THRESHOLD_BYTES"

if ($sdAccepted -le 0 -or $sdCommitted -le 0 -or
    $sdPending -ge $sdThreshold) {
    throw "H3ER_OFFLINE_DATALOG_ACTIVITY_FAIL"
}

$masterCompileNormalized =
    $masterCompileText.Replace("\", "/").ToLowerInvariant()
$slaveCompileNormalized =
    $slaveCompileText.Replace("\", "/").ToLowerInvariant()

foreach ($compile in @($masterCompileText, $slaveCompileText)) {
    if ($compile -match '(?i)-DJWPLC_W5500_RX_FIFO_REUSE=' -or
        $compile -match '(?i)-DJWPLC_SPI_FIFO_REUSE_DLEN_CACHE=') {
        throw "H3ER_OFFLINE_PACKAGE_DEFAULT_WAS_OVERRIDDEN"
    }
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
    if (-not $masterCompileNormalized.Contains($token)) {
        throw "H3ER_OFFLINE_MASTER_SOURCE_LINKAGE_MISSING=$token"
    }
}

foreach ($archive in @(
    "libjwplc_display.a",
    "libjwplc_tft.a",
    "libspi.a"
)) {
    if ($masterCompileNormalized.Contains($archive)) {
        throw "H3ER_OFFLINE_UNEXPECTED_STALE_ARCHIVE_LINKAGE=$archive"
    }
}

$masterArchiveLinkForm =
    Get-H3ERModbusRtuArchiveLinkForm `
        -NormalizedCompileText $masterCompileNormalized
$slaveArchiveLinkForm =
    Get-H3ERModbusRtuArchiveLinkForm `
        -NormalizedCompileText $slaveCompileNormalized

if ($masterArchiveLinkForm -eq "NONE") {
    throw "H3ER_OFFLINE_MASTER_MODBUS_RTU_ARCHIVE_NOT_LINKED"
}

if ($slaveArchiveLinkForm -eq "NONE") {
    throw "H3ER_OFFLINE_SLAVE_MODBUS_RTU_ARCHIVE_NOT_LINKED"
}

foreach ($marker in @(
    "UNDERLYING_PHYSICAL_RUN=PASS",
    "TFT_PHYSICAL=PASS",
    "PHYSICAL_RERUN_REQUIRED=NO"
)) {
    if (-not $wrapperText.Contains($marker)) {
        throw "H3ER_OFFLINE_WRAPPER_MARKER_MISSING=$marker"
    }
}

Write-Host "H3ER_OFFLINE_RUN_DIR=$resolvedRunDir"
Write-Host ("H3ER_TCP_ACHIEVED_REQ_S={0:F2}" -f $achievedReqS)
Write-Host ("H3ER_TCP_ACHIEVED_PCT={0:F3}" -f $achievedPct)
Write-Host ("H3ER_TCP_P95_US={0:F1}" -f $p95Us)
Write-Host ("H3ER_TCP_P99_US={0:F1}" -f $p99Us)
Write-Host ("H3ER_TCP_MAX_US={0:F1}" -f $maxUs)
Write-Host ("H3ER_RTU_ACHIEVED_HZ={0:F3}" -f $rtuHz)
Write-Host "H3ER_RTU_SUCCESS=$rtuSuccess/$rtuStarted"
Write-Host "H3ER_DATALOG_FAILED_COMMITS=0"
Write-Host "H3ER_PERIPHERAL_FAILURE_COUNT=0"
Write-Host "H3ER_TFT_PHYSICAL=PASS"
Write-Host "H3ER_MASTER_MODBUS_RTU_ARCHIVE_LINK_FORM=$masterArchiveLinkForm"
Write-Host "H3ER_SLAVE_MODBUS_RTU_ARCHIVE_LINK_FORM=$slaveArchiveLinkForm"
Write-Host "H3ER_HARNESS_FAILURE=FIXED"
Write-Host "H3ER_PRODUCT_FAILURE=NO"
Write-Host "H3ER_HARDWARE_FAILURE=NO"
Write-Host "H3ER_ENVIRONMENT_FAILURE=NO"
Write-Host "H3E_R_POST_P4_1=PASS"
Write-Host "H3ER_PHYSICAL_RERUN_REQUIRED=NO"
