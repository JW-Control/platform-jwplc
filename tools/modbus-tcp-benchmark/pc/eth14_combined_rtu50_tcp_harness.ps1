param(
    [string]$DutPort = "COM14",
    [string]$MasterPort = "COM4",

    [string]$DutIp = "",

    [int]$TcpPort = 502,

    [double]$TcpRate = 500.0,

    [double]$DurationS = 60.0,

    [string]$TargetFile = (
        Join-Path $env:TEMP `
            "jwplc_alpha14_g4b_p2c_target.txt"
    ),

    [switch]$RequireCleanTracked,

    [switch]$SkipPhysicalPrompt
)

$ErrorActionPreference = "Stop"
$env:PYTHONDONTWRITEBYTECODE = "1"

# ============================================================
# CONSTANTS / PATHS
# ============================================================

$ExpectedBranch =
    "v2.1.0-alpha.14/feature/modbus-tcp"

$RepoRoot = (
    git rev-parse --show-toplevel
).Trim()

if (-not $RepoRoot) {
    throw "GIT_ROOT_NOT_FOUND"
}

$Runner = Join-Path `
    $RepoRoot `
    "tools\modbus-tcp-benchmark\pc\a14_perf_full_runtime_realistic_1000rps_qualification.py"

if (-not (Test-Path -LiteralPath $Runner)) {
    throw "TCP_QUALIFICATION_RUNNER_MISSING"
}

$RunId = Get-Date -Format "yyyyMMdd_HHmmss"

$Work = Join-Path `
    $env:TEMP `
    "jwplc_eth14_g0_combined_$RunId"

New-Item `
    -ItemType Directory `
    -Path $Work `
    -Force |
    Out-Null

$TcpLog = Join-Path `
    $Work `
    "tcp_runner.log"

$TcpErr = Join-Path `
    $Work `
    "tcp_runner.err"

$Csv = Join-Path `
    $Work `
    "tcp_result.csv"

$MasterSnapshotFile = Join-Path `
    $Work `
    "master_final.txt"

# IMPORTANTE:
# Este archivo lo genera Python ANTES de cerrar COM14.
# PowerShell nunca reabre COM14 para obtenerlo.
$DutSnapshotFile = Join-Path `
    $Work `
    "dut_final_live.txt"

$SummaryFile = Join-Path `
    $Work `
    "combined_summary.txt"

# ============================================================
# HELPERS
# ============================================================

function Normalize-Text {
    param(
        [AllowNull()]
        [string]$Text
    )

    if ($null -eq $Text) {
        return ""
    }

    return (
        $Text -replace "`r", ""
    )
}

function Send-MasterCommand {
    param(
        [Parameter(Mandatory=$true)]
        [string]$PortName,

        [Parameter(Mandatory=$true)]
        [string]$Command,

        [int]$DrainMs = 400
    )

    $Serial =
        New-Object `
            System.IO.Ports.SerialPort `
            $PortName,
            115200,
            ([System.IO.Ports.Parity]::None),
            8,
            ([System.IO.Ports.StopBits]::One)

    $Serial.ReadTimeout  = 100
    $Serial.WriteTimeout = 1000
    $Serial.NewLine      = "`n"
    $Serial.DtrEnable    = $false
    $Serial.RtsEnable    = $false

    try {
        $Serial.Open()

        Start-Sleep -Milliseconds 200

        $Serial.DiscardInBuffer()

        $Serial.WriteLine(
            $Command
        )

        Start-Sleep `
            -Milliseconds $DrainMs

        $Reply =
            New-Object `
                System.Collections.Generic.List[string]

        while ($true) {
            try {
                $Line =
                    $Serial.ReadLine().TrimEnd("`r")

                if ($Line.Length -gt 0) {
                    $Reply.Add($Line)
                }
            }
            catch [System.TimeoutException] {
                break
            }
        }

        return @($Reply)
    }
    finally {
        if ($Serial.IsOpen) {
            $Serial.Close()
        }

        $Serial.Dispose()
    }
}

function Get-MasterSnapshot {
    param(
        [Parameter(Mandatory=$true)]
        [string]$PortName,

        [int]$Seconds = 5
    )

    $Serial =
        New-Object `
            System.IO.Ports.SerialPort `
            $PortName,
            115200,
            ([System.IO.Ports.Parity]::None),
            8,
            ([System.IO.Ports.StopBits]::One)

    $Serial.ReadTimeout  = 250
    $Serial.WriteTimeout = 1000
    $Serial.NewLine      = "`n"
    $Serial.DtrEnable    = $false
    $Serial.RtsEnable    = $false

    $Lines =
        New-Object `
            System.Collections.Generic.List[string]

    try {
        $Serial.Open()

        Start-Sleep -Milliseconds 300

        $Serial.DiscardInBuffer()
        $Serial.WriteLine("S")

        $Deadline =
            (Get-Date).AddSeconds(
                $Seconds
            )

        while ((Get-Date) -lt $Deadline) {
            try {
                $Line =
                    $Serial.ReadLine().TrimEnd("`r")

                if ($Line.Length -gt 0) {
                    $Lines.Add($Line)
                }
            }
            catch [System.TimeoutException] {
            }
        }

        return @($Lines)
    }
    finally {
        if ($Serial.IsOpen) {
            $Serial.Close()
        }

        $Serial.Dispose()
    }
}

function Get-IntValue {
    param(
        [Parameter(Mandatory=$true)]
        [string]$Text,

        [Parameter(Mandatory=$true)]
        [string]$Key,

        [int64]$Default = -1
    )

    $Text = Normalize-Text $Text

    $Match =
        [regex]::Match(
            $Text,
            "(?m)^$([regex]::Escape($Key))=(\d+)$"
        )

    if (-not $Match.Success) {
        return $Default
    }

    return [int64]$Match.Groups[1].Value
}

function Get-DoubleValue {
    param(
        [Parameter(Mandatory=$true)]
        [string]$Text,

        [Parameter(Mandatory=$true)]
        [string]$Key,

        [double]$Default = 0.0
    )

    $Text = Normalize-Text $Text

    $Match =
        [regex]::Match(
            $Text,
            "(?m)^$([regex]::Escape($Key))=([0-9.]+)$"
        )

    if (-not $Match.Success) {
        return $Default
    }

    return [double]::Parse(
        $Match.Groups[1].Value,
        [System.Globalization.CultureInfo]::InvariantCulture
    )
}

function Has-Marker {
    param(
        [Parameter(Mandatory=$true)]
        [string]$Text,

        [Parameter(Mandatory=$true)]
        [string]$Marker
    )

    $Text = Normalize-Text $Text

    return (
        $Text -match
        "(?m)^$([regex]::Escape($Marker))$"
    )
}

# ============================================================
# HEADER
# ============================================================

Write-Host "============================================================"
Write-Host " ETH14 COMBINED HARNESS"
Write-Host " RTU50 + TCP + DATALOG + FULL RUNTIME"
Write-Host "============================================================"

Write-Host "RUN_ID=$RunId"
Write-Host "WORK=$Work"
Write-Host "NO_BUILD=YES"
Write-Host "NO_UPLOAD=YES"

# ============================================================
# PREFLIGHT
# ============================================================

Write-Host ""
Write-Host "=== PREFLIGHT ==="

$Branch = (
    git branch --show-current
).Trim()

$Head = (
    git rev-parse HEAD
).Trim()

$TrackedDirty = @(
    git status `
        --porcelain `
        --untracked-files=no
)

Write-Host "BRANCH=$Branch"
Write-Host "HEAD=$Head"
Write-Host "TRACKED_DIRTY_COUNT=$($TrackedDirty.Count)"

if ($Branch -ne $ExpectedBranch) {
    throw "BRANCH_MISMATCH"
}

if (
    $RequireCleanTracked -and
    $TrackedDirty.Count -ne 0
) {
    $TrackedDirty
    throw "TRACKED_WORKTREE_NOT_CLEAN"
}

# ============================================================
# DUT IP
# ============================================================

if ([string]::IsNullOrWhiteSpace($DutIp)) {
    if (-not (Test-Path -LiteralPath $TargetFile)) {
        throw "DUT_IP_NOT_PROVIDED_AND_TARGET_FILE_MISSING"
    }

    $DutIp = (
        Get-Content `
            -LiteralPath $TargetFile `
            -Raw
    ).Trim()
}

if (
    $DutIp -notmatch
    '^\d{1,3}(?:\.\d{1,3}){3}$'
) {
    throw "INVALID_DUT_IP"
}

Write-Host "DUT_IP=$DutIp"
Write-Host "DUT_TCP_PORT=$TcpPort"
Write-Host "DUT_SERIAL=$DutPort"
Write-Host "MASTER_SERIAL=$MasterPort"

# ============================================================
# COM PRESENCE
# ============================================================

$Ports = @(
    [System.IO.Ports.SerialPort]::GetPortNames() |
    Sort-Object
)

Write-Host "COM_PORTS=$($Ports -join ',')"

if ($DutPort -notin $Ports) {
    throw "DUT_COM_NOT_PRESENT"
}

if ($MasterPort -notin $Ports) {
    throw "MASTER_COM_NOT_PRESENT"
}

# ============================================================
# TCP PRESENCE
# ============================================================

$TcpReachable =
    Test-NetConnection `
        -ComputerName $DutIp `
        -Port $TcpPort `
        -InformationLevel Quiet

Write-Host "TCP_REACHABLE=$TcpReachable"

if (-not $TcpReachable) {
    throw "TCP_NOT_REACHABLE"
}

# ============================================================
# PYTHON
# ============================================================

$PythonCmd =
    Get-Command `
        py `
        -ErrorAction SilentlyContinue

if (-not $PythonCmd) {
    throw "PY_LAUNCHER_NOT_FOUND"
}

$Python = $PythonCmd.Source

& $Python `
    -3 `
    -B `
    -c "import serial; print('PYSERIAL=PASS')"

if ($LASTEXITCODE -ne 0) {
    throw "PYSERIAL_NOT_AVAILABLE"
}

# ============================================================
# MASTER BASELINE
# ============================================================

Write-Host ""
Write-Host "=== MASTER BASELINE ==="

# Garantizar RTU OFF.
[void](
    Send-MasterCommand `
        -PortName $MasterPort `
        -Command "X"
)

Start-Sleep -Milliseconds 200

# Reset estadístico.
[void](
    Send-MasterCommand `
        -PortName $MasterPort `
        -Command "R"
)

$MasterInitial =
    @(
        Get-MasterSnapshot `
            -PortName $MasterPort `
            -Seconds 4
    )

$MasterInitialText =
    Normalize-Text (
        $MasterInitial -join "`n"
    )

foreach ($Line in $MasterInitial) {
    Write-Host $Line
}

$MasterReady =
    Has-Marker `
        $MasterInitialText `
        "MASTER_READY=YES"

$MasterOff =
    Has-Marker `
        $MasterInitialText `
        "TRAFFIC_ENABLED=NO"

$MasterSlave2 =
    Has-Marker `
        $MasterInitialText `
        "RTU_TARGET_SLAVE_ID=2"

$MasterZero = (
    (Get-IntValue `
        $MasterInitialText `
        "RTU_REQUESTS_STARTED") -eq 0
) -and (
    (Get-IntValue `
        $MasterInitialText `
        "RTU_REQUESTS_SUCCESS") -eq 0
)

Write-Host "MASTER_READY=$MasterReady"
Write-Host "MASTER_TRAFFIC_OFF=$MasterOff"
Write-Host "MASTER_TARGET_SLAVE_2=$MasterSlave2"
Write-Host "MASTER_COUNTERS_RESET=$MasterZero"

if (
    -not $MasterReady -or
    -not $MasterOff -or
    -not $MasterSlave2 -or
    -not $MasterZero
) {
    throw "MASTER_BASELINE_INVALID"
}

# ============================================================
# START TCP RUNNER
# ============================================================

Write-Host ""
Write-Host "=== START TCP RUNNER ==="

$Arguments = @(
    "-3",
    "-B",
    "-u",
    $Runner,
    "--serial", $DutPort,
    "--baud", "115200",
    "--host", $DutIp,
    "--port", "$TcpPort",
    "--rate", "$TcpRate",
    "--duration", "$DurationS",
    "--csv", $Csv,
    "--snapshot-out", $DutSnapshotFile
)

$Process =
    Start-Process `
        -FilePath $Python `
        -ArgumentList $Arguments `
        -WorkingDirectory $RepoRoot `
        -RedirectStandardOutput $TcpLog `
        -RedirectStandardError $TcpErr `
        -PassThru

Write-Host "TCP_RUNNER_PID=$($Process.Id)"
Write-Host "TCP_LOG=$TcpLog"
Write-Host "TCP_ERR=$TcpErr"
Write-Host "TCP_CSV=$Csv"
Write-Host "DUT_LIVE_SNAPSHOT=$DutSnapshotFile"

$MasterStarted = $false
$MasterStopped = $false

$RunMarkerSeen = $false
$ResultMarkerSeen = $false

try {
    # ========================================================
    # WAIT TCP WINDOW START
    # ========================================================

    Write-Host ""
    Write-Host "=== WAIT TCP RUN MARKER ==="

    $StartDeadline =
        (Get-Date).AddSeconds(45)

    while ((Get-Date) -lt $StartDeadline) {
        if (Test-Path -LiteralPath $TcpLog) {
            $CurrentLog =
                Normalize-Text (
                    Get-Content `
                        -LiteralPath $TcpLog `
                        -Raw `
                        -ErrorAction SilentlyContinue
                )

            # El runner conserva todavía texto histórico
            # "1000 req/s"; usamos el comienzo del marcador.
            if (
                $CurrentLog -match
                '=== RUN [0-9.]+S FC03/125 @'
            ) {
                $RunMarkerSeen = $true
                break
            }
        }

        if ($Process.HasExited) {
            break
        }

        Start-Sleep -Milliseconds 20
    }

    Write-Host "TCP_RUN_MARKER_SEEN=$RunMarkerSeen"

    if (-not $RunMarkerSeen) {
        throw "TCP_RUN_MARKER_NOT_SEEN"
    }

    # ========================================================
    # START RTU MASTER
    # ========================================================

    Write-Host ""
    Write-Host "=== START RTU MASTER ==="

    $GoReply =
        @(
            Send-MasterCommand `
                -PortName $MasterPort `
                -Command "G" `
                -DrainMs 150
        )

    foreach ($Line in $GoReply) {
        Write-Host $Line
    }

    $MasterStarted = $true

    Write-Host "RTU_MASTER_GO_SENT=YES"

    # ========================================================
    # WAIT TCP CASE END
    # ========================================================

    Write-Host ""
    Write-Host "=== SIMULTANEOUS WINDOW ==="
    Write-Host "RTU_TARGET_HZ=50"
    Write-Host "TCP_TARGET_REQ_S=$TcpRate"

    $FinishDeadline =
        (Get-Date).AddSeconds(
            [Math]::Max(
                90,
                [int][Math]::Ceiling(
                    $DurationS + 30
                )
            )
        )

    while ((Get-Date) -lt $FinishDeadline) {
        if (Test-Path -LiteralPath $TcpLog) {
            $CurrentLog =
                Normalize-Text (
                    Get-Content `
                        -LiteralPath $TcpLog `
                        -Raw `
                        -ErrorAction SilentlyContinue
                )

            if (
                $CurrentLog -match
                '(?m)^FORMAL_RESULT '
            ) {
                $ResultMarkerSeen = $true
                break
            }
        }

        if ($Process.HasExited) {
            break
        }

        Start-Sleep -Milliseconds 20
    }

    Write-Host "TCP_RESULT_MARKER_SEEN=$ResultMarkerSeen"

    # ========================================================
    # STOP RTU AT TCP WINDOW END
    # ========================================================

    if ($MasterStarted) {
        Write-Host ""
        Write-Host "=== STOP RTU MASTER ==="

        $StopReply =
            @(
                Send-MasterCommand `
                    -PortName $MasterPort `
                    -Command "X" `
                    -DrainMs 200
            )

        foreach ($Line in $StopReply) {
            Write-Host $Line
        }

        $MasterStopped = $true

        Write-Host "RTU_MASTER_STOP_SENT=YES"
    }

    if (-not $ResultMarkerSeen) {
        throw "TCP_RESULT_MARKER_NOT_SEEN"
    }

    # Python ahora captura/persiste el snapshot final de COM14
    # manteniendo todavía abierta su propia sesión Serial.
    $Process.WaitForExit()

    Write-Host "TCP_RUNNER_EXIT=$($Process.ExitCode)"
}
finally {
    # Seguridad: jamás dejar COM4 transmitiendo.
    if (
        $MasterStarted -and
        -not $MasterStopped
    ) {
        try {
            [void](
                Send-MasterCommand `
                    -PortName $MasterPort `
                    -Command "X" `
                    -DrainMs 200
            )

            $MasterStopped = $true

            Write-Host "RTU_MASTER_EMERGENCY_STOP=PASS"
        }
        catch {
            Write-Host (
                "WARNING_MASTER_EMERGENCY_STOP_FAILED=" +
                $_.Exception.Message
            )
        }
    }

    if (-not $Process.HasExited) {
        try {
            $Process.Kill()
            $Process.WaitForExit()
        }
        catch {
        }
    }
}

# ============================================================
# TCP OUTPUT
# ============================================================

$TcpText =
    if (Test-Path -LiteralPath $TcpLog) {
        Normalize-Text (
            Get-Content `
                -LiteralPath $TcpLog `
                -Raw
        )
    }
    else {
        ""
    }

$TcpErrText =
    if (Test-Path -LiteralPath $TcpErr) {
        Normalize-Text (
            Get-Content `
                -LiteralPath $TcpErr `
                -Raw
        )
    }
    else {
        ""
    }

Write-Host ""
Write-Host "============================================================"
Write-Host " TCP RUNNER OUTPUT"
Write-Host "============================================================"
Write-Host $TcpText

if (-not [string]::IsNullOrWhiteSpace($TcpErrText)) {
    Write-Host ""
    Write-Host "=== TCP STDERR ==="
    Write-Host $TcpErrText
}

# ============================================================
# MASTER FINAL SNAPSHOT
# ============================================================

Write-Host ""
Write-Host "============================================================"
Write-Host " COM4 FINAL SNAPSHOT"
Write-Host "============================================================"

$MasterFinal =
    @(
        Get-MasterSnapshot `
            -PortName $MasterPort `
            -Seconds 5
    )

$MasterText =
    Normalize-Text (
        $MasterFinal -join "`n"
    )

foreach ($Line in $MasterFinal) {
    Write-Host $Line
}

[System.IO.File]::WriteAllText(
    $MasterSnapshotFile,
    $MasterText + "`n",
    [System.Text.UTF8Encoding]::new($false)
)

# ============================================================
# DUT FINAL SNAPSHOT
# ============================================================
#
# CRITICAL:
# NO abrir COM14 aquí.
# El snapshot fue generado por Python antes de ser.close().
# ============================================================

Write-Host ""
Write-Host "============================================================"
Write-Host " COM14 FINAL LIVE SNAPSHOT"
Write-Host "============================================================"

if (-not (Test-Path -LiteralPath $DutSnapshotFile)) {
    throw "DUT_LIVE_SNAPSHOT_MISSING"
}

$DutText =
    Normalize-Text (
        Get-Content `
            -LiteralPath $DutSnapshotFile `
            -Raw
    )

Write-Host $DutText

# ============================================================
# TCP ASSERTIONS
# ============================================================

$TcpRunnerPass =
    $Process.ExitCode -eq 0

$TcpRequested =
    Get-DoubleValue `
        $TcpText `
        "REQUESTED_REQ_S" `
        -1

$TcpAchieved =
    Get-DoubleValue `
        $TcpText `
        "ACHIEVED_REQ_S" `
        0

$TcpClean =
    Has-Marker `
        $TcpText `
        "TCP_CLEAN=YES"

$TcpRatePass =
    Has-Marker `
        $TcpText `
        "TCP_RATE_PASS=YES"

$ReadinessPass =
    Has-Marker `
        $TcpText `
        "READINESS_PASS=YES"

$PeripheralPass =
    Has-Marker `
        $TcpText `
        "PERIPHERAL_PASS=YES"

$SnapshotMarker =
    $TcpText -match
    '(?m)^FINAL_SNAPSHOT_FILE=.+$'

$RequestedMatches =
    [Math]::Abs(
        $TcpRequested - $TcpRate
    ) -lt 0.01

$TcpPass = (
    $TcpRunnerPass -and
    $RequestedMatches -and
    $TcpClean -and
    $TcpRatePass -and
    $ReadinessPass -and
    $PeripheralPass -and
    $SnapshotMarker -and
    $TcpAchieved -ge ($TcpRate * 0.95)
)

Write-Host ""
Write-Host "=== TCP ASSERTIONS ==="
Write-Host "TCP_RUNNER_PASS=$TcpRunnerPass"
Write-Host "TCP_REQUESTED_REQ_S=$TcpRequested"
Write-Host "TCP_REQUEST_MATCH=$RequestedMatches"
Write-Host "TCP_CLEAN=$TcpClean"
Write-Host "TCP_RATE_PASS=$TcpRatePass"
Write-Host "TCP_READINESS_PASS=$ReadinessPass"
Write-Host "TCP_PERIPHERAL_PASS=$PeripheralPass"
Write-Host "TCP_LIVE_SNAPSHOT_MARKER=$SnapshotMarker"
Write-Host "TCP_ACHIEVED_REQ_S=$TcpAchieved"
Write-Host "TCP_PASS=$TcpPass"

# ============================================================
# MASTER RTU ASSERTIONS
# ============================================================

$TrafficOff =
    Has-Marker `
        $MasterText `
        "TRAFFIC_ENABLED=NO"

$DurationMs =
    Get-IntValue `
        $MasterText `
        "TRAFFIC_DURATION_MS"

$Started =
    Get-IntValue `
        $MasterText `
        "RTU_REQUESTS_STARTED"

$Rejected =
    Get-IntValue `
        $MasterText `
        "RTU_REQUESTS_REJECTED"

$Completed =
    Get-IntValue `
        $MasterText `
        "RTU_REQUESTS_COMPLETED"

$Success =
    Get-IntValue `
        $MasterText `
        "RTU_REQUESTS_SUCCESS"

$Failed =
    Get-IntValue `
        $MasterText `
        "RTU_REQUESTS_FAILED"

$VerifyFails =
    Get-IntValue `
        $MasterText `
        "RTU_VERIFY_FAILS"

# Métrica observacional.
# NO forma parte del criterio PASS.
$Skipped =
    Get-IntValue `
        $MasterText `
        "RTU_PERIODS_SKIPPED"

$MasterCrc =
    Get-IntValue `
        $MasterText `
        "RTU_CRC_ERRORS"

$MasterTimeouts =
    Get-IntValue `
        $MasterText `
        "RTU_MASTER_TIMEOUTS"

$MasterLastOk =
    Has-Marker `
        $MasterText `
        "RTU_LAST_ERROR=OK"

$RtuHz =
    if ($DurationMs -gt 0) {
        $Success /
        ($DurationMs / 1000.0)
    }
    else {
        0.0
    }

# Tolerancia temporal proporcional al duration solicitado.
$MinDurationMs =
    [int64](
        [Math]::Max(
            1000,
            ($DurationS * 1000.0) - 2000.0
        )
    )

$MaxDurationMs =
    [int64](
        ($DurationS * 1000.0) + 5000.0
    )

$MinStarted =
    [int64](
        [Math]::Floor(
            $DurationS * 45.0
        )
    )

$MasterRtuPass = (
    $TrafficOff -and
    $DurationMs -ge $MinDurationMs -and
    $DurationMs -le $MaxDurationMs -and
    $Started -ge $MinStarted -and
    $Rejected -eq 0 -and
    $Completed -eq $Started -and
    $Success -eq $Completed -and
    $Failed -eq 0 -and
    $VerifyFails -eq 0 -and
    $MasterCrc -eq 0 -and
    $MasterTimeouts -eq 0 -and
    $MasterLastOk -and
    $RtuHz -ge 45.0 -and
    $RtuHz -le 52.0
)

Write-Host ""
Write-Host "=== COM4 RTU ASSERTIONS ==="
Write-Host "MASTER_TRAFFIC_OFF_FINAL=$TrafficOff"
Write-Host "RTU_DURATION_MS=$DurationMs"
Write-Host "RTU_REQUESTS_STARTED=$Started"
Write-Host "RTU_REQUESTS_REJECTED=$Rejected"
Write-Host "RTU_REQUESTS_COMPLETED=$Completed"
Write-Host "RTU_REQUESTS_SUCCESS=$Success"
Write-Host "RTU_REQUESTS_FAILED=$Failed"
Write-Host "RTU_VERIFY_FAILS=$VerifyFails"
Write-Host "RTU_PERIODS_SKIPPED=$Skipped"
Write-Host "RTU_PERIODS_SKIPPED_GATE=NO"
Write-Host "RTU_CRC_ERRORS=$MasterCrc"
Write-Host "RTU_MASTER_TIMEOUTS=$MasterTimeouts"
Write-Host "RTU_LAST_ERROR_OK=$MasterLastOk"
Write-Host ("RTU_ACHIEVED_HZ={0:F3}" -f $RtuHz)
Write-Host "RTU_MASTER_PASS=$MasterRtuPass"

# ============================================================
# DUT ASSERTIONS
# ============================================================

$CombinedReady =
    Has-Marker `
        $DutText `
        "G4B_COMBINED_READY=YES"

$FullRuntime =
    Has-Marker `
        $DutText `
        "FULL_RUNTIME_READY=YES"

$ServerReady =
    Has-Marker `
        $DutText `
        "SERVER_READY=YES"

$SdReady =
    Has-Marker `
        $DutText `
        "SD_READY=YES"

$DataLogActive =
    Has-Marker `
        $DutText `
        "DATALOG_ACTIVE=YES"

$DataLogFailZero =
    Has-Marker `
        $DutText `
        "DATALOG_FAILED_COMMITS_DELTA=0"

$DataLogAuto =
    Has-Marker `
        $DutText `
        "DATALOG_AUTO_COMMIT_OBSERVED=YES"

$PeripheralZero =
    Has-Marker `
        $DutText `
        "PERIPHERAL_FAILURE_COUNT=0"

$RtuReady =
    Has-Marker `
        $DutText `
        "RTU_READY=YES"

$DutRx =
    Get-IntValue `
        $DutText `
        "RTU_RX_FRAMES"

$DutTx =
    Get-IntValue `
        $DutText `
        "RTU_TX_FRAMES"

$DutOk =
    Get-IntValue `
        $DutText `
        "RTU_REQUESTS_OK"

$DutCrc =
    Get-IntValue `
        $DutText `
        "RTU_CRC_ERRORS"

$DutEx =
    Get-IntValue `
        $DutText `
        "RTU_EXCEPTIONS_SENT"

$DutLastOk =
    Has-Marker `
        $DutText `
        "RTU_LAST_ERROR=OK"

$ServiceGap =
    Get-IntValue `
        $DutText `
        "RTU_SERVICE_GAP_MAX_US"

$MinSlaveFrames =
    [Math]::Max(
        $MinStarted,
        $Success - 5
    )

$DutRtuPass = (
    $RtuReady -and
    $DutRx -ge $MinSlaveFrames -and
    $DutTx -ge $MinSlaveFrames -and
    $DutOk -ge $MinSlaveFrames -and
    $DutCrc -eq 0 -and
    $DutEx -eq 0 -and
    $DutLastOk
)

$DutRuntimePass = (
    $CombinedReady -and
    $FullRuntime -and
    $ServerReady -and
    $SdReady -and
    $DataLogActive -and
    $DataLogFailZero -and
    $DataLogAuto -and
    $PeripheralZero
)

Write-Host ""
Write-Host "=== COM14 COMBINED ASSERTIONS ==="
Write-Host "FULL_RUNTIME_READY=$FullRuntime"
Write-Host "SERVER_READY=$ServerReady"
Write-Host "SD_READY=$SdReady"
Write-Host "DATALOG_ACTIVE=$DataLogActive"
Write-Host "DATALOG_FAILED_COMMITS_ZERO=$DataLogFailZero"
Write-Host "DATALOG_AUTO_COMMIT=$DataLogAuto"
Write-Host "PERIPHERAL_FAILURE_COUNT_ZERO=$PeripheralZero"
Write-Host "RTU_READY=$RtuReady"
Write-Host "RTU_RX_FRAMES=$DutRx"
Write-Host "RTU_TX_FRAMES=$DutTx"
Write-Host "RTU_REQUESTS_OK=$DutOk"
Write-Host "RTU_CRC_ERRORS=$DutCrc"
Write-Host "RTU_EXCEPTIONS_SENT=$DutEx"
Write-Host "RTU_LAST_ERROR_OK=$DutLastOk"
Write-Host "RTU_SERVICE_GAP_MAX_US=$ServiceGap"
Write-Host "G4B_COMBINED_READY=$CombinedReady"
Write-Host "RTU_SLAVE_PASS=$DutRtuPass"
Write-Host "FULL_RUNTIME_COMBINED_PASS=$DutRuntimePass"

# ============================================================
# PHYSICAL OBSERVATION
# ============================================================

if ($SkipPhysicalPrompt) {
    $MasterPhysical = $true
    $DutPhysical = $true

    Write-Host ""
    Write-Host "PHYSICAL_PROMPT=SKIPPED"
}
else {
    Write-Host ""
    Write-Host "============================================================"
    Write-Host " PHYSICAL OBSERVATION"
    Write-Host "============================================================"

    Write-Host "COM4 esperado:"
    Write-Host " - RUN durante la ventana"
    Write-Host " - RTU ~50 Hz"
    Write-Host " - FAIL=0"
    Write-Host " - TFT estable"

    Write-Host ""
    Write-Host "COM14 esperado:"
    Write-Host " - HMI actualizando"
    Write-Host " - sin flicker/cortes"

    $A =
        Read-Host `
            "¿COM4 estable, sin flicker y sin FAIL visibles? (S/N)"

    $B =
        Read-Host `
            "¿COM14 estable, sin flicker ni cortes? (S/N)"

    $MasterPhysical =
        $A.Trim().ToUpper() -eq "S"

    $DutPhysical =
        $B.Trim().ToUpper() -eq "S"
}

$PhysicalPass = (
    $MasterPhysical -and
    $DutPhysical
)

# ============================================================
# FINAL CLASSIFICATION
# ============================================================

$OverallPass = (
    $TcpPass -and
    $MasterRtuPass -and
    $DutRtuPass -and
    $DutRuntimePass -and
    $PhysicalPass
)

$Summary = @(
    "RUN_ID=$RunId",
    "HEAD=$Head",
    "DUT_IP=$DutIp",
    "TCP_TARGET_REQ_S=$TcpRate",
    "TCP_ACHIEVED_REQ_S=$TcpAchieved",
    "TCP_PASS=$TcpPass",
    "RTU_TARGET_HZ=50",
    ("RTU_ACHIEVED_HZ={0:F3}" -f $RtuHz),
    "RTU_PERIODS_SKIPPED=$Skipped",
    "RTU_PERIODS_SKIPPED_GATE=NO",
    "RTU_MASTER_PASS=$MasterRtuPass",
    "RTU_SLAVE_PASS=$DutRtuPass",
    "FULL_RUNTIME_COMBINED_PASS=$DutRuntimePass",
    "COM4_PHYSICAL=$MasterPhysical",
    "COM14_PHYSICAL=$DutPhysical",
    "OVERALL_PASS=$OverallPass",
    "TCP_LOG=$TcpLog",
    "TCP_ERR=$TcpErr",
    "TCP_CSV=$Csv",
    "MASTER_SNAPSHOT=$MasterSnapshotFile",
    "DUT_LIVE_SNAPSHOT=$DutSnapshotFile"
)

[System.IO.File]::WriteAllLines(
    $SummaryFile,
    $Summary,
    [System.Text.UTF8Encoding]::new($false)
)

Write-Host ""
Write-Host "============================================================"
Write-Host " ETH14 COMBINED SUMMARY"
Write-Host "============================================================"

foreach ($Line in $Summary) {
    Write-Host $Line
}

Write-Host "SUMMARY_FILE=$SummaryFile"

if ($OverallPass) {
    Write-Host ""
    Write-Host "ETH14_COMBINED_HARNESS=PASS_PHYSICAL"
}
else {
    Write-Host ""
    Write-Host "ETH14_COMBINED_HARNESS=REVIEW"
    throw "ETH14_COMBINED_HARNESS_NOT_CLOSED"
}