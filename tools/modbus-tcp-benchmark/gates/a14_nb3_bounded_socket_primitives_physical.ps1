param(
    [string]$SerialPort = "COM14",
    [string]$DutIp = "",
    [double]$DurationSeconds = 3.0
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"
. (Join-Path $PSScriptRoot "common.ps1")

function Invoke-NB3NativeToLog {
    param(
        [Parameter(Mandatory = $true)][string]$FilePath,
        [Parameter(Mandatory = $true)][string[]]$Arguments,
        [Parameter(Mandatory = $true)][string]$LogPath
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

function Get-NB3LogValue {
    param(
        [Parameter(Mandatory = $true)][string]$Text,
        [Parameter(Mandatory = $true)][string]$Key
    )

    $pattern = "(?m)^" + [regex]::Escape($Key) + "=(.*)\r?$"
    $matches = @([regex]::Matches($Text, $pattern))

    if ($matches.Count -ne 1) {
        throw "A14_NB3C_LOG_KEY_COUNT_$($Key)=$($matches.Count)"
    }

    return $matches[0].Groups[1].Value.Trim()
}

function Get-NB3LogInt64 {
    param(
        [Parameter(Mandatory = $true)][string]$Text,
        [Parameter(Mandatory = $true)][string]$Key
    )

    return [int64]::Parse(
        (Get-NB3LogValue -Text $Text -Key $Key),
        [System.Globalization.CultureInfo]::InvariantCulture
    )
}

function Assert-NB3ExactValue {
    param(
        [Parameter(Mandatory = $true)][string]$Text,
        [Parameter(Mandatory = $true)][string]$Key,
        [Parameter(Mandatory = $true)][string]$Expected
    )

    $actual = Get-NB3LogValue -Text $Text -Key ("G2_FINAL_{0}" -f $Key)
    Write-Host "EXACT_$($Key)=$actual"

    if ($actual -ne $Expected) {
        throw "A14_NB3C_EXACT_$($Key)_MISMATCH=$actual"
    }
}

function Assert-NB3ExactZero {
    param(
        [Parameter(Mandatory = $true)][string]$Text,
        [Parameter(Mandatory = $true)][string]$Key
    )

    $value = Get-NB3LogInt64 -Text $Text -Key ("G2_FINAL_{0}" -f $Key)
    Write-Host "EXACT_$($Key)=$value"

    if ($value -ne 0) {
        throw "A14_NB3C_EXACT_$($Key)_NONZERO=$value"
    }
}

function Get-NB3CurrentDutIp {
    param(
        [Parameter(Mandatory = $true)][string]$PortName,
        [int]$TimeoutSeconds = 15
    )

    $serial = New-Object System.IO.Ports.SerialPort
    $serial.PortName = $PortName
    $serial.BaudRate = 115200
    $serial.Parity = [System.IO.Ports.Parity]::None
    $serial.DataBits = 8
    $serial.StopBits = [System.IO.Ports.StopBits]::One
    $serial.Handshake = [System.IO.Ports.Handshake]::None
    $serial.DtrEnable = $false
    $serial.RtsEnable = $false
    $serial.ReadTimeout = 100
    $serial.WriteTimeout = 1000

    $deadline = [DateTime]::UtcNow.AddSeconds($TimeoutSeconds)
    $lastSnapshot = ""

    try {
        $serial.Open()
        Start-Sleep -Milliseconds 500

        while ([DateTime]::UtcNow -lt $deadline) {
            $serial.DiscardInBuffer()
            $serial.Write("S")

            $raw = ""
            $snapshotDeadline = [DateTime]::UtcNow.AddSeconds(2)

            while ([DateTime]::UtcNow -lt $snapshotDeadline) {
                Start-Sleep -Milliseconds 50
                $chunk = $serial.ReadExisting()

                if (-not [string]::IsNullOrEmpty($chunk)) {
                    $raw += $chunk

                    if ($raw.Contains("ETH14_RAW_SNAPSHOT=END")) {
                        break
                    }
                }
            }

            if (-not $raw.Contains("ETH14_RAW_SNAPSHOT=END")) {
                Start-Sleep -Milliseconds 250
                continue
            }

            $lastSnapshot = $raw

            $ready = [regex]::IsMatch(
                $raw,
                '(?m)^RAW_SERVER_READY=YES\r?    param()

    $expected = [ordered]@{
        "JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/Dns.cpp" = "3DE3CD1A14BD07C1FE3B0725941B2137A5109B43647AA3E10B3EF6A1E6EEC5F5"
        "JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/Dns.h" = "EF6C5392C0CBFB8545BFF8EB4D30E3FEDAC005725DF292418A5CF974BE6F6FD2"
        "JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/EthernetClient.cpp" = "93B71C92E298053425FF03D750632A30102BE743E1C8B1FDAB800214EABED4B2"
        "JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/JWPLC_W5x00_Ethernet.h" = "3E11094D68873D81F264F8AD97C1D55867F5399E28613061BC38484CD7DE614B"
        "JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/jwplc_ethernet_async_tx.cpp" = "CC8EEE779FC932C5A8FA0175ABBF0421AA8C95279B4260FADAC1276E7BD74E1C"
        "JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/jwplc_ethernet_async_tx.h" = "C1C5615DFF167F3572FBEA85CFFE1A7F5051732E2BD446387F1025A1D981FB47"
        "JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/socket.cpp" = "EF450C75880A2427494585E8EB1C1E3FD62EC7B9562E37A3AA75C48761886104"
        "JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/utility/w5100.cpp" = "9F94AAC1BB25966C18EDFD6A5C5D5908A9DBD4CF11B6E9BC099E10B28CEF272F"
        "JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/utility/w5100.h" = "9A833532C44E0CFCD66429A764E8BDBF62A8871838B0355565BC0A63043455FC"
        "tools/modbus-tcp-benchmark/firmware/eth14_raw_transport_server/eth14_raw_transport_server.ino" = "9A0C6CF27E44A61DC97686FB1A769EBBBB34D036CDF8D0CA0EEAB597E699AB6B"
    }

    foreach ($entry in $expected.GetEnumerator()) {
        $actual = Get-G2Sha256 $entry.Key
        Write-Host "CANDIDATE_SHA256=$($entry.Key)=$actual"

        if ($actual -ne $entry.Value) {
            throw "A14_NB3C_CANDIDATE_HASH_MISMATCH=$($entry.Key)"
        }
    }

    $runnerHash = Get-G2Sha256 $script:G2RawRunnerRelative
    Write-Host "RAW_RUNNER_SHA256=$runnerHash"

    if ($runnerHash -ne "FD25997CFE777B4FF50BBC10EFE555BD6B7A8A8DCE9D4ADD1E7D2DBA6D52470F") {
        throw "A14_NB3C_RAW_RUNNER_HASH_MISMATCH"
    }
}

Write-Host "============================================================"
Write-Host " A14 NB3-C - BOUNDED SOCKET PRIMITIVES PHYSICAL SMOKE"
Write-Host "============================================================"

Assert-G2Branch

if ($DurationSeconds -le 0) {
    throw "A14_NB3C_DURATION_MUST_BE_POSITIVE"
}

$expectedDirty = @(
    "JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/Dns.cpp",
    "JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/Dns.h",
    "JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/EthernetClient.cpp",
    "JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/JWPLC_W5x00_Ethernet.h",
    "JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/jwplc_ethernet_async_tx.cpp",
    "JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/jwplc_ethernet_async_tx.h",
    "JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/socket.cpp",
    "JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/utility/w5100.cpp",
    "JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/utility/w5100.h",
    "tools/modbus-tcp-benchmark/firmware/eth14_raw_transport_server/eth14_raw_transport_server.ino"
) | Sort-Object

$dirtyBefore = @(Get-G2TrackedDirtyPaths)

Write-Host "HEAD=$(Get-G2Head)"
Write-Host "EFFECTIVE_SPI_HZ=$(Get-G2SpiHz)"
Write-Host "SERIAL_PORT=$SerialPort"
Write-Host "DUT_IP_REQUESTED=$DutIp"
Write-Host "DUT_IP_SOURCE=SERIAL_AFTER_UPLOAD"
Write-Host "DURATION_S=$DurationSeconds"
Write-Host "PREFERRED_HOLD_MAX_US=5000"
Write-Host "HARD_HOLD_MAX_US=10000"
Write-Host "HARD_LOOP_GAP_MAX_US=15000"
Write-Host "TRACKED_DIRTY_COUNT=$($dirtyBefore.Count)"
$dirtyBefore | ForEach-Object { Write-Host "DIRTY=$_" }

if ((Get-G2SpiHz) -ne 26000000) {
    throw "A14_NB3C_SPI_FREQUENCY_MISMATCH"
}

if ($dirtyBefore.Count -ne $expectedDirty.Count) {
    throw "A14_NB3C_DIRTY_COUNT_INVALID=$($dirtyBefore.Count)"
}

for ($i = 0; $i -lt $expectedDirty.Count; ++$i) {
    if ($dirtyBefore[$i] -ne $expectedDirty[$i]) {
        throw "A14_NB3C_DIRTY_PATH_INVALID=$($dirtyBefore[$i])"
    }
}

$stagedBefore = @(& git -C $script:G2RepoRoot diff --cached --name-only)
if ($LASTEXITCODE -ne 0 -or $stagedBefore.Count -ne 0) {
    throw "A14_NB3C_INDEX_NOT_CLEAN"
}
Write-Host "STAGED_COUNT=0"

Assert-G2ProtectedArtifacts
Assert-NB3CandidateHashes

$arduinoCli = "C:\Program Files\Arduino PLC IDE Tools\arduino-cli.exe"
if (-not (Test-Path -LiteralPath $arduinoCli)) {
    throw "A14_NB3C_ARDUINO_CLI_NOT_FOUND=$arduinoCli"
}

$pythonCommand = Get-Command python.exe -ErrorAction SilentlyContinue
if ($null -eq $pythonCommand) {
    $pythonCommand = Get-Command python -ErrorAction Stop
}

$pythonExe = $pythonCommand.Source
$fqbn = "jwplc_local:esp32:jwplcbasic"
$firmwarePath = Get-G2Path $script:G2RawFirmwareRelative
$sketchDir = Split-Path -Parent $firmwarePath
$bridgePath = Join-Path $PSScriptRoot "g2_runner_snapshot_bridge.py"
$librariesRoot = Get-G2Path "JWPLC/2.1.0/libraries"
$expectedEthernetLibrary = Join-Path $librariesRoot "JWPLC_Ethernet"

if (-not (Test-Path -LiteralPath $bridgePath)) {
    throw "A14_NB3C_SNAPSHOT_BRIDGE_NOT_FOUND"
}

$timestamp = Get-Date -Format "yyyyMMdd_HHmmss"
$tempRoot = Join-Path $env:TEMP ("jwplc_a14_nb3c_physical_{0}" -f $timestamp)
$buildPath = Join-Path $tempRoot "build"
$compileLog = Join-Path $tempRoot "compile.log"
$uploadLog = Join-Path $tempRoot "upload.log"

New-Item -ItemType Directory -Force -Path $buildPath | Out-Null

Write-Host "ARDUINO_CLI=$arduinoCli"
Write-Host "PYTHON=$pythonExe"
Write-Host "FQBN=$fqbn"
Write-Host "BRIDGE=$bridgePath"
Write-Host "TEMP_ROOT=$tempRoot"

Write-Host ""
Write-Host "=== COMPILE NB3-B RAW CANDIDATE ==="

$compileArgs = @(
    "compile",
    "--fqbn", $fqbn,
    "--build-path", $buildPath,
    "--libraries", $librariesRoot,
    $sketchDir
)

$compileExit = Invoke-NB3NativeToLog -FilePath $arduinoCli -Arguments $compileArgs -LogPath $compileLog

Write-Host "COMPILE_EXIT=$compileExit"
Write-Host "COMPILE_LOG=$compileLog"

if ($compileExit -ne 0) {
    Invoke-G2CompileFinishedSound -Success $false
    Get-Content -LiteralPath $compileLog -Tail 120 | ForEach-Object { Write-Host $_ }
    throw "A14_NB3C_COMPILE_FAILED"
}

$compileText = [System.IO.File]::ReadAllText($compileLog)
$repoEthernetUsed = (
    $compileText.IndexOf(
        $expectedEthernetLibrary,
        [System.StringComparison]::OrdinalIgnoreCase
    ) -ge 0
)

Write-Host "REPO_ETHERNET_LIBRARY_USED=$repoEthernetUsed"
if (-not $repoEthernetUsed) {
    throw "A14_NB3C_WRONG_ETHERNET_LIBRARY"
}

$binCount = @(
    Get-ChildItem -LiteralPath $buildPath -Recurse -File -Filter "*.bin"
).Count

Write-Host "BIN_COUNT=$binCount"
if ($binCount -lt 1) {
    Invoke-G2CompileFinishedSound -Success $false
    throw "A14_NB3C_NO_BIN"
}

Invoke-G2CompileFinishedSound -Success $true
Write-Host "COMPILE_FINISHED_SOUND=PLAYED"

Write-Host ""
Write-Host "=== UPLOAD NB3-B RAW CANDIDATE ==="

$uploadArgs = @(
    "upload",
    "--fqbn", $fqbn,
    "--port", $SerialPort,
    "--input-dir", $buildPath,
    $sketchDir
)

$uploadExit = Invoke-NB3NativeToLog -FilePath $arduinoCli -Arguments $uploadArgs -LogPath $uploadLog

Write-Host "UPLOAD_EXIT=$uploadExit"
Write-Host "UPLOAD_LOG=$uploadLog"

if ($uploadExit -ne 0) {
    Get-Content -LiteralPath $uploadLog -Tail 120 | ForEach-Object { Write-Host $_ }
    throw "A14_NB3C_UPLOAD_FAILED"
}

Write-Host ""
Write-Host "=== RESOLVE EFFECTIVE DUT IP FROM SERIAL ==="

$effectiveDutIp = Get-NB3CurrentDutIp -PortName $SerialPort

Write-Host "DUT_IP_EFFECTIVE=$effectiveDutIp"

if (-not [string]::IsNullOrWhiteSpace($DutIp)) {
    Write-Host "DUT_IP_REQUESTED_MATCH=$($DutIp -eq $effectiveDutIp)"

    if ($DutIp -ne $effectiveDutIp) {
        throw "A14_NB3C_REQUESTED_DUT_IP_MISMATCH requested=$DutIp actual=$effectiveDutIp"
    }
}
else {
    Write-Host "DUT_IP_REQUESTED_MATCH=NOT_APPLICABLE_AUTO_DISCOVERY"
}

$modeMap = @(
    [pscustomobject]@{ Cli = "tcp-rx"; Key = "TCP_RX" },
    [pscustomobject]@{ Cli = "tcp-tx"; Key = "TCP_TX" },
    [pscustomobject]@{ Cli = "udp-rx"; Key = "UDP_RX" },
    [pscustomobject]@{ Cli = "udp-tx"; Key = "UDP_TX" }
)

$durationText = $DurationSeconds.ToString(
    [System.Globalization.CultureInfo]::InvariantCulture
)

$preferredHoldReview = $false

Write-Host ""
Write-Host "=== FOUR-MODE PHYSICAL SMOKE ==="

foreach ($mode in $modeMap) {
    $runLog = Join-Path $tempRoot ("{0}.log" -f $mode.Key.ToLowerInvariant())

    $runnerArgs = @(
        $bridgePath,
        "--host", $effectiveDutIp,
        "--serial", $SerialPort,
        "--duration", $durationText,
        "--mode", $mode.Cli,
        "--tcp-chunk", "4096",
        "--udp-payload", "1472"
    )

    $runExit = Invoke-NB3NativeToLog -FilePath $pythonExe -Arguments $runnerArgs -LogPath $runLog

    Write-Host "MODE=$($mode.Key) RUNNER_EXIT=$runExit LOG=$runLog"

    if ($runExit -ne 0) {
        Get-Content -LiteralPath $runLog -Tail 120 | ForEach-Object { Write-Host $_ }
        throw "A14_NB3C_RUNNER_FAILED_$($mode.Key)"
    }

    $runText = [System.IO.File]::ReadAllText($runLog)

    if ((Get-NB3LogValue -Text $runText -Key "RAW_BENCH_FUNCTIONAL_PASS") -ne "YES") {
        throw "A14_NB3C_FUNCTIONAL_FAIL_$($mode.Key)"
    }

    if ((Get-NB3LogValue -Text $runText -Key "DUT_READY") -ne "YES") {
        throw "A14_NB3C_DUT_NOT_READY_$($mode.Key)"
    }

    if ((Get-NB3LogValue -Text $runText -Key "G2_FINAL_SNAPSHOT_PRESENT") -ne "YES") {
        throw "A14_NB3C_FINAL_SNAPSHOT_MISSING_$($mode.Key)"
    }

    Assert-NB3ExactValue -Text $runText -Key "RAW_SERVER_READY" -Expected "YES"
    Assert-NB3ExactValue -Text $runText -Key "ETH_READY" -Expected "YES"
    Assert-NB3ExactValue -Text $runText -Key "ETH_LINK" -Expected "UP"
    Assert-NB3ExactValue -Text $runText -Key "IP" -Expected $effectiveDutIp

    @(
        "TRANSPORT_ERRORS",
        "UDP_BEGIN_PACKET_ERRORS",
        "UDP_WRITE_ERRORS",
        "UDP_END_PACKET_ERRORS",
        "UDP_SPI_LOCK_ERRORS",
        "TCP_SPI_LOCK_ERRORS",
        "UDP_LAST_SHORT_WRITE_BYTES"
    ) | ForEach-Object {
        Assert-NB3ExactZero -Text $runText -Key $_
    }

    $pcMbps = Get-NB3LogValue -Text $runText -Key ("SUMMARY_{0}_PC_MBPS" -f $mode.Key)
    $dutMbps = Get-NB3LogValue -Text $runText -Key ("SUMMARY_{0}_DUT_MBPS" -f $mode.Key)
    $holdMaxUs = Get-NB3LogInt64 -Text $runText -Key "G2_FINAL_TCP_SPI_HOLD_MAX_US"
    $loopGapMaxUs = Get-NB3LogInt64 -Text $runText -Key "G2_FINAL_LOOP_GAP_MAX_US"

    Write-Host "MODE=$($mode.Key) PC_MBPS=$pcMbps DUT_MBPS=$dutMbps HOLD_MAX_US=$holdMaxUs LOOP_GAP_MAX_US=$loopGapMaxUs"

    if ($holdMaxUs -gt 10000) {
        throw "A14_NB3C_HARD_HOLD_REGRESSION_$($mode.Key)=$holdMaxUs"
    }

    if ($holdMaxUs -gt 5000) {
        $preferredHoldReview = $true
        Write-Host "MODE=$($mode.Key) PREFERRED_HOLD_5MS=REVIEW"
    }
    else {
        Write-Host "MODE=$($mode.Key) PREFERRED_HOLD_5MS=PASS"
    }

    if ($loopGapMaxUs -gt 15000) {
        throw "A14_NB3C_LOOP_GAP_REGRESSION_$($mode.Key)=$loopGapMaxUs"
    }

    if ($mode.Key -eq "UDP_TX") {
        @(
            "UDP_TX_SEQUENCE_DECODE_ERRORS",
            "UDP_TX_SEQUENCE_TOTAL_DUPLICATES",
            "UDP_TX_SEQUENCE_REORDERS",
            "UDP_TX_SEQUENCE_TOTAL_RANGE_MISSING",
            "UDP_TX_UNEXPECTED_SOURCE_PACKETS",
            "UDP_TX_WRONG_SIZE_FROM_DUT"
        ) | ForEach-Object {
            $integrityValue = Get-NB3LogInt64 -Text $runText -Key $_
            Write-Host "$_=$integrityValue"

            if ($integrityValue -ne 0) {
                throw "A14_NB3C_UDP_TX_INTEGRITY_FAIL_$($_)=$integrityValue"
            }
        }

        $sequenceCount = Get-NB3LogInt64 -Text $runText -Key "UDP_TX_SEQUENCE_TOTAL_COUNT"
        Write-Host "UDP_TX_SEQUENCE_TOTAL_COUNT=$sequenceCount"

        if ($sequenceCount -le 0) {
            throw "A14_NB3C_UDP_TX_SEQUENCE_EMPTY"
        }
    }

    Write-Host "NB3_C_MODE_$($mode.Key)=PASS"
    Start-Sleep -Milliseconds 400
}

Write-Host ""
Write-Host "OBSERVACION FISICA REQUERIDA: mira la TFT durante NB3-C."
$visualAnswer = ""

while ($visualAnswer -notin @("S", "N")) {
    $visualAnswer = (Read-Host 'Aparecio el diagnostico visual "SPI" durante NB3-C? (S/N)').Trim().ToUpperInvariant()
}

$visualEvents = if ($visualAnswer -eq "S") { 1 } else { 0 }
Write-Host "VISUAL_SPI_EVENTS=$visualEvents"

if ($visualEvents -ne 0) {
    throw "A14_NB3C_VISUAL_SPI_FAIL"
}

Write-Host ""
Write-Host "=== FINAL INVARIANTS ==="

Assert-G2ProtectedArtifacts
Assert-NB3CandidateHashes

$dirtyFinal = @(Get-G2TrackedDirtyPaths)
$stagedFinal = @(& git -C $script:G2RepoRoot diff --cached --name-only)

Write-Host "TRACKED_DIRTY_COUNT_FINAL=$($dirtyFinal.Count)"
$dirtyFinal | ForEach-Object { Write-Host "DIRTY_FINAL=$_" }
Write-Host "STAGED_COUNT_FINAL=$($stagedFinal.Count)"

if ($dirtyFinal.Count -ne $expectedDirty.Count -or $stagedFinal.Count -ne 0) {
    throw "A14_NB3C_FINAL_WORKTREE_INVALID"
}

for ($i = 0; $i -lt $expectedDirty.Count; ++$i) {
    if ($dirtyFinal[$i] -ne $expectedDirty[$i]) {
        throw "A14_NB3C_FINAL_DIRTY_PATH_INVALID=$($dirtyFinal[$i])"
    }
}

$preferredState = if ($preferredHoldReview) { "REVIEW_KNOWN_8_CHUNK_RX" } else { "PASS" }

Write-Host "NB3_BOUNDED_PRIMITIVES_RAW_COMPILE=PASS"
Write-Host "NB3_BOUNDED_PRIMITIVES_PHYSICAL_FOUR_MODES=PASS"
Write-Host "NB3_TRANSPORT_ERRORS=ZERO"
Write-Host "NB3_UDP_TX_INTEGRITY=PASS"
Write-Host "NB3_VISUAL_SPI=PASS"
Write-Host "NB3_HARD_HOLD_10MS=PASS"
Write-Host "NB3_HARD_LOOP_GAP_15MS=PASS"
Write-Host "NB3_PREFERRED_HOLD_5MS=$preferredState"
Write-Host "NB3_DUT_IP_DISCOVERY=SERIAL_DYNAMIC"
Write-Host "NB3_UDP_SEND_SYNC_PATH=UNCHANGED_PENDING_NEXT_GATE"
Write-Host "A14_NB3_BOUNDED_SOCKET_PRIMITIVES_PHYSICAL=PASS"

            )
            $ethReady = [regex]::IsMatch(
                $raw,
                '(?m)^ETH_READY=YES\r?    param()

    $expected = [ordered]@{
        "JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/Dns.cpp" = "3DE3CD1A14BD07C1FE3B0725941B2137A5109B43647AA3E10B3EF6A1E6EEC5F5"
        "JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/Dns.h" = "EF6C5392C0CBFB8545BFF8EB4D30E3FEDAC005725DF292418A5CF974BE6F6FD2"
        "JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/EthernetClient.cpp" = "93B71C92E298053425FF03D750632A30102BE743E1C8B1FDAB800214EABED4B2"
        "JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/JWPLC_W5x00_Ethernet.h" = "3E11094D68873D81F264F8AD97C1D55867F5399E28613061BC38484CD7DE614B"
        "JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/jwplc_ethernet_async_tx.cpp" = "CC8EEE779FC932C5A8FA0175ABBF0421AA8C95279B4260FADAC1276E7BD74E1C"
        "JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/jwplc_ethernet_async_tx.h" = "C1C5615DFF167F3572FBEA85CFFE1A7F5051732E2BD446387F1025A1D981FB47"
        "JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/socket.cpp" = "EF450C75880A2427494585E8EB1C1E3FD62EC7B9562E37A3AA75C48761886104"
        "JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/utility/w5100.cpp" = "9F94AAC1BB25966C18EDFD6A5C5D5908A9DBD4CF11B6E9BC099E10B28CEF272F"
        "JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/utility/w5100.h" = "9A833532C44E0CFCD66429A764E8BDBF62A8871838B0355565BC0A63043455FC"
        "tools/modbus-tcp-benchmark/firmware/eth14_raw_transport_server/eth14_raw_transport_server.ino" = "9A0C6CF27E44A61DC97686FB1A769EBBBB34D036CDF8D0CA0EEAB597E699AB6B"
    }

    foreach ($entry in $expected.GetEnumerator()) {
        $actual = Get-G2Sha256 $entry.Key
        Write-Host "CANDIDATE_SHA256=$($entry.Key)=$actual"

        if ($actual -ne $entry.Value) {
            throw "A14_NB3C_CANDIDATE_HASH_MISMATCH=$($entry.Key)"
        }
    }

    $runnerHash = Get-G2Sha256 $script:G2RawRunnerRelative
    Write-Host "RAW_RUNNER_SHA256=$runnerHash"

    if ($runnerHash -ne "FD25997CFE777B4FF50BBC10EFE555BD6B7A8A8DCE9D4ADD1E7D2DBA6D52470F") {
        throw "A14_NB3C_RAW_RUNNER_HASH_MISMATCH"
    }
}

Write-Host "============================================================"
Write-Host " A14 NB3-C - BOUNDED SOCKET PRIMITIVES PHYSICAL SMOKE"
Write-Host "============================================================"

Assert-G2Branch

if ($DurationSeconds -le 0) {
    throw "A14_NB3C_DURATION_MUST_BE_POSITIVE"
}

$expectedDirty = @(
    "JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/Dns.cpp",
    "JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/Dns.h",
    "JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/EthernetClient.cpp",
    "JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/JWPLC_W5x00_Ethernet.h",
    "JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/jwplc_ethernet_async_tx.cpp",
    "JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/jwplc_ethernet_async_tx.h",
    "JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/socket.cpp",
    "JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/utility/w5100.cpp",
    "JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/utility/w5100.h",
    "tools/modbus-tcp-benchmark/firmware/eth14_raw_transport_server/eth14_raw_transport_server.ino"
) | Sort-Object

$dirtyBefore = @(Get-G2TrackedDirtyPaths)

Write-Host "HEAD=$(Get-G2Head)"
Write-Host "EFFECTIVE_SPI_HZ=$(Get-G2SpiHz)"
Write-Host "SERIAL_PORT=$SerialPort"
Write-Host "DUT_IP=$DutIp"
Write-Host "DURATION_S=$DurationSeconds"
Write-Host "PREFERRED_HOLD_MAX_US=5000"
Write-Host "HARD_HOLD_MAX_US=10000"
Write-Host "HARD_LOOP_GAP_MAX_US=15000"
Write-Host "TRACKED_DIRTY_COUNT=$($dirtyBefore.Count)"
$dirtyBefore | ForEach-Object { Write-Host "DIRTY=$_" }

if ((Get-G2SpiHz) -ne 26000000) {
    throw "A14_NB3C_SPI_FREQUENCY_MISMATCH"
}

if ($dirtyBefore.Count -ne $expectedDirty.Count) {
    throw "A14_NB3C_DIRTY_COUNT_INVALID=$($dirtyBefore.Count)"
}

for ($i = 0; $i -lt $expectedDirty.Count; ++$i) {
    if ($dirtyBefore[$i] -ne $expectedDirty[$i]) {
        throw "A14_NB3C_DIRTY_PATH_INVALID=$($dirtyBefore[$i])"
    }
}

$stagedBefore = @(& git -C $script:G2RepoRoot diff --cached --name-only)
if ($LASTEXITCODE -ne 0 -or $stagedBefore.Count -ne 0) {
    throw "A14_NB3C_INDEX_NOT_CLEAN"
}
Write-Host "STAGED_COUNT=0"

Assert-G2ProtectedArtifacts
Assert-NB3CandidateHashes

$arduinoCli = "C:\Program Files\Arduino PLC IDE Tools\arduino-cli.exe"
if (-not (Test-Path -LiteralPath $arduinoCli)) {
    throw "A14_NB3C_ARDUINO_CLI_NOT_FOUND=$arduinoCli"
}

$pythonCommand = Get-Command python.exe -ErrorAction SilentlyContinue
if ($null -eq $pythonCommand) {
    $pythonCommand = Get-Command python -ErrorAction Stop
}

$pythonExe = $pythonCommand.Source
$fqbn = "jwplc_local:esp32:jwplcbasic"
$firmwarePath = Get-G2Path $script:G2RawFirmwareRelative
$sketchDir = Split-Path -Parent $firmwarePath
$bridgePath = Join-Path $PSScriptRoot "g2_runner_snapshot_bridge.py"
$librariesRoot = Get-G2Path "JWPLC/2.1.0/libraries"
$expectedEthernetLibrary = Join-Path $librariesRoot "JWPLC_Ethernet"

if (-not (Test-Path -LiteralPath $bridgePath)) {
    throw "A14_NB3C_SNAPSHOT_BRIDGE_NOT_FOUND"
}

$timestamp = Get-Date -Format "yyyyMMdd_HHmmss"
$tempRoot = Join-Path $env:TEMP ("jwplc_a14_nb3c_physical_{0}" -f $timestamp)
$buildPath = Join-Path $tempRoot "build"
$compileLog = Join-Path $tempRoot "compile.log"
$uploadLog = Join-Path $tempRoot "upload.log"

New-Item -ItemType Directory -Force -Path $buildPath | Out-Null

Write-Host "ARDUINO_CLI=$arduinoCli"
Write-Host "PYTHON=$pythonExe"
Write-Host "FQBN=$fqbn"
Write-Host "BRIDGE=$bridgePath"
Write-Host "TEMP_ROOT=$tempRoot"

Write-Host ""
Write-Host "=== COMPILE NB3-B RAW CANDIDATE ==="

$compileArgs = @(
    "compile",
    "--fqbn", $fqbn,
    "--build-path", $buildPath,
    "--libraries", $librariesRoot,
    $sketchDir
)

$compileExit = Invoke-NB3NativeToLog -FilePath $arduinoCli -Arguments $compileArgs -LogPath $compileLog

Write-Host "COMPILE_EXIT=$compileExit"
Write-Host "COMPILE_LOG=$compileLog"

if ($compileExit -ne 0) {
    Invoke-G2CompileFinishedSound -Success $false
    Get-Content -LiteralPath $compileLog -Tail 120 | ForEach-Object { Write-Host $_ }
    throw "A14_NB3C_COMPILE_FAILED"
}

$compileText = [System.IO.File]::ReadAllText($compileLog)
$repoEthernetUsed = (
    $compileText.IndexOf(
        $expectedEthernetLibrary,
        [System.StringComparison]::OrdinalIgnoreCase
    ) -ge 0
)

Write-Host "REPO_ETHERNET_LIBRARY_USED=$repoEthernetUsed"
if (-not $repoEthernetUsed) {
    throw "A14_NB3C_WRONG_ETHERNET_LIBRARY"
}

$binCount = @(
    Get-ChildItem -LiteralPath $buildPath -Recurse -File -Filter "*.bin"
).Count

Write-Host "BIN_COUNT=$binCount"
if ($binCount -lt 1) {
    Invoke-G2CompileFinishedSound -Success $false
    throw "A14_NB3C_NO_BIN"
}

Invoke-G2CompileFinishedSound -Success $true
Write-Host "COMPILE_FINISHED_SOUND=PLAYED"

Write-Host ""
Write-Host "=== UPLOAD NB3-B RAW CANDIDATE ==="

$uploadArgs = @(
    "upload",
    "--fqbn", $fqbn,
    "--port", $SerialPort,
    "--input-dir", $buildPath,
    $sketchDir
)

$uploadExit = Invoke-NB3NativeToLog -FilePath $arduinoCli -Arguments $uploadArgs -LogPath $uploadLog

Write-Host "UPLOAD_EXIT=$uploadExit"
Write-Host "UPLOAD_LOG=$uploadLog"

if ($uploadExit -ne 0) {
    Get-Content -LiteralPath $uploadLog -Tail 120 | ForEach-Object { Write-Host $_ }
    throw "A14_NB3C_UPLOAD_FAILED"
}

$modeMap = @(
    [pscustomobject]@{ Cli = "tcp-rx"; Key = "TCP_RX" },
    [pscustomobject]@{ Cli = "tcp-tx"; Key = "TCP_TX" },
    [pscustomobject]@{ Cli = "udp-rx"; Key = "UDP_RX" },
    [pscustomobject]@{ Cli = "udp-tx"; Key = "UDP_TX" }
)

$durationText = $DurationSeconds.ToString(
    [System.Globalization.CultureInfo]::InvariantCulture
)

$preferredHoldReview = $false

Write-Host ""
Write-Host "=== FOUR-MODE PHYSICAL SMOKE ==="

foreach ($mode in $modeMap) {
    $runLog = Join-Path $tempRoot ("{0}.log" -f $mode.Key.ToLowerInvariant())

    $runnerArgs = @(
        $bridgePath,
        "--host", $DutIp,
        "--serial", $SerialPort,
        "--duration", $durationText,
        "--mode", $mode.Cli,
        "--tcp-chunk", "4096",
        "--udp-payload", "1472"
    )

    $runExit = Invoke-NB3NativeToLog -FilePath $pythonExe -Arguments $runnerArgs -LogPath $runLog

    Write-Host "MODE=$($mode.Key) RUNNER_EXIT=$runExit LOG=$runLog"

    if ($runExit -ne 0) {
        Get-Content -LiteralPath $runLog -Tail 120 | ForEach-Object { Write-Host $_ }
        throw "A14_NB3C_RUNNER_FAILED_$($mode.Key)"
    }

    $runText = [System.IO.File]::ReadAllText($runLog)

    if ((Get-NB3LogValue -Text $runText -Key "RAW_BENCH_FUNCTIONAL_PASS") -ne "YES") {
        throw "A14_NB3C_FUNCTIONAL_FAIL_$($mode.Key)"
    }

    if ((Get-NB3LogValue -Text $runText -Key "DUT_READY") -ne "YES") {
        throw "A14_NB3C_DUT_NOT_READY_$($mode.Key)"
    }

    if ((Get-NB3LogValue -Text $runText -Key "G2_FINAL_SNAPSHOT_PRESENT") -ne "YES") {
        throw "A14_NB3C_FINAL_SNAPSHOT_MISSING_$($mode.Key)"
    }

    Assert-NB3ExactValue -Text $runText -Key "RAW_SERVER_READY" -Expected "YES"
    Assert-NB3ExactValue -Text $runText -Key "ETH_READY" -Expected "YES"
    Assert-NB3ExactValue -Text $runText -Key "ETH_LINK" -Expected "UP"
    Assert-NB3ExactValue -Text $runText -Key "IP" -Expected $DutIp

    @(
        "TRANSPORT_ERRORS",
        "UDP_BEGIN_PACKET_ERRORS",
        "UDP_WRITE_ERRORS",
        "UDP_END_PACKET_ERRORS",
        "UDP_SPI_LOCK_ERRORS",
        "TCP_SPI_LOCK_ERRORS",
        "UDP_LAST_SHORT_WRITE_BYTES"
    ) | ForEach-Object {
        Assert-NB3ExactZero -Text $runText -Key $_
    }

    $pcMbps = Get-NB3LogValue -Text $runText -Key ("SUMMARY_{0}_PC_MBPS" -f $mode.Key)
    $dutMbps = Get-NB3LogValue -Text $runText -Key ("SUMMARY_{0}_DUT_MBPS" -f $mode.Key)
    $holdMaxUs = Get-NB3LogInt64 -Text $runText -Key "G2_FINAL_TCP_SPI_HOLD_MAX_US"
    $loopGapMaxUs = Get-NB3LogInt64 -Text $runText -Key "G2_FINAL_LOOP_GAP_MAX_US"

    Write-Host "MODE=$($mode.Key) PC_MBPS=$pcMbps DUT_MBPS=$dutMbps HOLD_MAX_US=$holdMaxUs LOOP_GAP_MAX_US=$loopGapMaxUs"

    if ($holdMaxUs -gt 10000) {
        throw "A14_NB3C_HARD_HOLD_REGRESSION_$($mode.Key)=$holdMaxUs"
    }

    if ($holdMaxUs -gt 5000) {
        $preferredHoldReview = $true
        Write-Host "MODE=$($mode.Key) PREFERRED_HOLD_5MS=REVIEW"
    }
    else {
        Write-Host "MODE=$($mode.Key) PREFERRED_HOLD_5MS=PASS"
    }

    if ($loopGapMaxUs -gt 15000) {
        throw "A14_NB3C_LOOP_GAP_REGRESSION_$($mode.Key)=$loopGapMaxUs"
    }

    if ($mode.Key -eq "UDP_TX") {
        @(
            "UDP_TX_SEQUENCE_DECODE_ERRORS",
            "UDP_TX_SEQUENCE_TOTAL_DUPLICATES",
            "UDP_TX_SEQUENCE_REORDERS",
            "UDP_TX_SEQUENCE_TOTAL_RANGE_MISSING",
            "UDP_TX_UNEXPECTED_SOURCE_PACKETS",
            "UDP_TX_WRONG_SIZE_FROM_DUT"
        ) | ForEach-Object {
            $integrityValue = Get-NB3LogInt64 -Text $runText -Key $_
            Write-Host "$_=$integrityValue"

            if ($integrityValue -ne 0) {
                throw "A14_NB3C_UDP_TX_INTEGRITY_FAIL_$($_)=$integrityValue"
            }
        }

        $sequenceCount = Get-NB3LogInt64 -Text $runText -Key "UDP_TX_SEQUENCE_TOTAL_COUNT"
        Write-Host "UDP_TX_SEQUENCE_TOTAL_COUNT=$sequenceCount"

        if ($sequenceCount -le 0) {
            throw "A14_NB3C_UDP_TX_SEQUENCE_EMPTY"
        }
    }

    Write-Host "NB3_C_MODE_$($mode.Key)=PASS"
    Start-Sleep -Milliseconds 400
}

Write-Host ""
Write-Host "OBSERVACION FISICA REQUERIDA: mira la TFT durante NB3-C."
$visualAnswer = ""

while ($visualAnswer -notin @("S", "N")) {
    $visualAnswer = (Read-Host 'Aparecio el diagnostico visual "SPI" durante NB3-C? (S/N)').Trim().ToUpperInvariant()
}

$visualEvents = if ($visualAnswer -eq "S") { 1 } else { 0 }
Write-Host "VISUAL_SPI_EVENTS=$visualEvents"

if ($visualEvents -ne 0) {
    throw "A14_NB3C_VISUAL_SPI_FAIL"
}

Write-Host ""
Write-Host "=== FINAL INVARIANTS ==="

Assert-G2ProtectedArtifacts
Assert-NB3CandidateHashes

$dirtyFinal = @(Get-G2TrackedDirtyPaths)
$stagedFinal = @(& git -C $script:G2RepoRoot diff --cached --name-only)

Write-Host "TRACKED_DIRTY_COUNT_FINAL=$($dirtyFinal.Count)"
$dirtyFinal | ForEach-Object { Write-Host "DIRTY_FINAL=$_" }
Write-Host "STAGED_COUNT_FINAL=$($stagedFinal.Count)"

if ($dirtyFinal.Count -ne $expectedDirty.Count -or $stagedFinal.Count -ne 0) {
    throw "A14_NB3C_FINAL_WORKTREE_INVALID"
}

for ($i = 0; $i -lt $expectedDirty.Count; ++$i) {
    if ($dirtyFinal[$i] -ne $expectedDirty[$i]) {
        throw "A14_NB3C_FINAL_DIRTY_PATH_INVALID=$($dirtyFinal[$i])"
    }
}

$preferredState = if ($preferredHoldReview) { "REVIEW_KNOWN_8_CHUNK_RX" } else { "PASS" }

Write-Host "NB3_BOUNDED_PRIMITIVES_RAW_COMPILE=PASS"
Write-Host "NB3_BOUNDED_PRIMITIVES_PHYSICAL_FOUR_MODES=PASS"
Write-Host "NB3_TRANSPORT_ERRORS=ZERO"
Write-Host "NB3_UDP_TX_INTEGRITY=PASS"
Write-Host "NB3_VISUAL_SPI=PASS"
Write-Host "NB3_HARD_HOLD_10MS=PASS"
Write-Host "NB3_HARD_LOOP_GAP_15MS=PASS"
Write-Host "NB3_PREFERRED_HOLD_5MS=$preferredState"
Write-Host "NB3_UDP_SEND_SYNC_PATH=UNCHANGED_PENDING_NEXT_GATE"
Write-Host "A14_NB3_BOUNDED_SOCKET_PRIMITIVES_PHYSICAL=PASS"

            )
            $linkUp = [regex]::IsMatch(
                $raw,
                '(?m)^ETH_LINK=UP\r?    param()

    $expected = [ordered]@{
        "JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/Dns.cpp" = "3DE3CD1A14BD07C1FE3B0725941B2137A5109B43647AA3E10B3EF6A1E6EEC5F5"
        "JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/Dns.h" = "EF6C5392C0CBFB8545BFF8EB4D30E3FEDAC005725DF292418A5CF974BE6F6FD2"
        "JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/EthernetClient.cpp" = "93B71C92E298053425FF03D750632A30102BE743E1C8B1FDAB800214EABED4B2"
        "JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/JWPLC_W5x00_Ethernet.h" = "3E11094D68873D81F264F8AD97C1D55867F5399E28613061BC38484CD7DE614B"
        "JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/jwplc_ethernet_async_tx.cpp" = "CC8EEE779FC932C5A8FA0175ABBF0421AA8C95279B4260FADAC1276E7BD74E1C"
        "JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/jwplc_ethernet_async_tx.h" = "C1C5615DFF167F3572FBEA85CFFE1A7F5051732E2BD446387F1025A1D981FB47"
        "JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/socket.cpp" = "EF450C75880A2427494585E8EB1C1E3FD62EC7B9562E37A3AA75C48761886104"
        "JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/utility/w5100.cpp" = "9F94AAC1BB25966C18EDFD6A5C5D5908A9DBD4CF11B6E9BC099E10B28CEF272F"
        "JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/utility/w5100.h" = "9A833532C44E0CFCD66429A764E8BDBF62A8871838B0355565BC0A63043455FC"
        "tools/modbus-tcp-benchmark/firmware/eth14_raw_transport_server/eth14_raw_transport_server.ino" = "9A0C6CF27E44A61DC97686FB1A769EBBBB34D036CDF8D0CA0EEAB597E699AB6B"
    }

    foreach ($entry in $expected.GetEnumerator()) {
        $actual = Get-G2Sha256 $entry.Key
        Write-Host "CANDIDATE_SHA256=$($entry.Key)=$actual"

        if ($actual -ne $entry.Value) {
            throw "A14_NB3C_CANDIDATE_HASH_MISMATCH=$($entry.Key)"
        }
    }

    $runnerHash = Get-G2Sha256 $script:G2RawRunnerRelative
    Write-Host "RAW_RUNNER_SHA256=$runnerHash"

    if ($runnerHash -ne "FD25997CFE777B4FF50BBC10EFE555BD6B7A8A8DCE9D4ADD1E7D2DBA6D52470F") {
        throw "A14_NB3C_RAW_RUNNER_HASH_MISMATCH"
    }
}

Write-Host "============================================================"
Write-Host " A14 NB3-C - BOUNDED SOCKET PRIMITIVES PHYSICAL SMOKE"
Write-Host "============================================================"

Assert-G2Branch

if ($DurationSeconds -le 0) {
    throw "A14_NB3C_DURATION_MUST_BE_POSITIVE"
}

$expectedDirty = @(
    "JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/Dns.cpp",
    "JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/Dns.h",
    "JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/EthernetClient.cpp",
    "JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/JWPLC_W5x00_Ethernet.h",
    "JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/jwplc_ethernet_async_tx.cpp",
    "JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/jwplc_ethernet_async_tx.h",
    "JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/socket.cpp",
    "JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/utility/w5100.cpp",
    "JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/utility/w5100.h",
    "tools/modbus-tcp-benchmark/firmware/eth14_raw_transport_server/eth14_raw_transport_server.ino"
) | Sort-Object

$dirtyBefore = @(Get-G2TrackedDirtyPaths)

Write-Host "HEAD=$(Get-G2Head)"
Write-Host "EFFECTIVE_SPI_HZ=$(Get-G2SpiHz)"
Write-Host "SERIAL_PORT=$SerialPort"
Write-Host "DUT_IP=$DutIp"
Write-Host "DURATION_S=$DurationSeconds"
Write-Host "PREFERRED_HOLD_MAX_US=5000"
Write-Host "HARD_HOLD_MAX_US=10000"
Write-Host "HARD_LOOP_GAP_MAX_US=15000"
Write-Host "TRACKED_DIRTY_COUNT=$($dirtyBefore.Count)"
$dirtyBefore | ForEach-Object { Write-Host "DIRTY=$_" }

if ((Get-G2SpiHz) -ne 26000000) {
    throw "A14_NB3C_SPI_FREQUENCY_MISMATCH"
}

if ($dirtyBefore.Count -ne $expectedDirty.Count) {
    throw "A14_NB3C_DIRTY_COUNT_INVALID=$($dirtyBefore.Count)"
}

for ($i = 0; $i -lt $expectedDirty.Count; ++$i) {
    if ($dirtyBefore[$i] -ne $expectedDirty[$i]) {
        throw "A14_NB3C_DIRTY_PATH_INVALID=$($dirtyBefore[$i])"
    }
}

$stagedBefore = @(& git -C $script:G2RepoRoot diff --cached --name-only)
if ($LASTEXITCODE -ne 0 -or $stagedBefore.Count -ne 0) {
    throw "A14_NB3C_INDEX_NOT_CLEAN"
}
Write-Host "STAGED_COUNT=0"

Assert-G2ProtectedArtifacts
Assert-NB3CandidateHashes

$arduinoCli = "C:\Program Files\Arduino PLC IDE Tools\arduino-cli.exe"
if (-not (Test-Path -LiteralPath $arduinoCli)) {
    throw "A14_NB3C_ARDUINO_CLI_NOT_FOUND=$arduinoCli"
}

$pythonCommand = Get-Command python.exe -ErrorAction SilentlyContinue
if ($null -eq $pythonCommand) {
    $pythonCommand = Get-Command python -ErrorAction Stop
}

$pythonExe = $pythonCommand.Source
$fqbn = "jwplc_local:esp32:jwplcbasic"
$firmwarePath = Get-G2Path $script:G2RawFirmwareRelative
$sketchDir = Split-Path -Parent $firmwarePath
$bridgePath = Join-Path $PSScriptRoot "g2_runner_snapshot_bridge.py"
$librariesRoot = Get-G2Path "JWPLC/2.1.0/libraries"
$expectedEthernetLibrary = Join-Path $librariesRoot "JWPLC_Ethernet"

if (-not (Test-Path -LiteralPath $bridgePath)) {
    throw "A14_NB3C_SNAPSHOT_BRIDGE_NOT_FOUND"
}

$timestamp = Get-Date -Format "yyyyMMdd_HHmmss"
$tempRoot = Join-Path $env:TEMP ("jwplc_a14_nb3c_physical_{0}" -f $timestamp)
$buildPath = Join-Path $tempRoot "build"
$compileLog = Join-Path $tempRoot "compile.log"
$uploadLog = Join-Path $tempRoot "upload.log"

New-Item -ItemType Directory -Force -Path $buildPath | Out-Null

Write-Host "ARDUINO_CLI=$arduinoCli"
Write-Host "PYTHON=$pythonExe"
Write-Host "FQBN=$fqbn"
Write-Host "BRIDGE=$bridgePath"
Write-Host "TEMP_ROOT=$tempRoot"

Write-Host ""
Write-Host "=== COMPILE NB3-B RAW CANDIDATE ==="

$compileArgs = @(
    "compile",
    "--fqbn", $fqbn,
    "--build-path", $buildPath,
    "--libraries", $librariesRoot,
    $sketchDir
)

$compileExit = Invoke-NB3NativeToLog -FilePath $arduinoCli -Arguments $compileArgs -LogPath $compileLog

Write-Host "COMPILE_EXIT=$compileExit"
Write-Host "COMPILE_LOG=$compileLog"

if ($compileExit -ne 0) {
    Invoke-G2CompileFinishedSound -Success $false
    Get-Content -LiteralPath $compileLog -Tail 120 | ForEach-Object { Write-Host $_ }
    throw "A14_NB3C_COMPILE_FAILED"
}

$compileText = [System.IO.File]::ReadAllText($compileLog)
$repoEthernetUsed = (
    $compileText.IndexOf(
        $expectedEthernetLibrary,
        [System.StringComparison]::OrdinalIgnoreCase
    ) -ge 0
)

Write-Host "REPO_ETHERNET_LIBRARY_USED=$repoEthernetUsed"
if (-not $repoEthernetUsed) {
    throw "A14_NB3C_WRONG_ETHERNET_LIBRARY"
}

$binCount = @(
    Get-ChildItem -LiteralPath $buildPath -Recurse -File -Filter "*.bin"
).Count

Write-Host "BIN_COUNT=$binCount"
if ($binCount -lt 1) {
    Invoke-G2CompileFinishedSound -Success $false
    throw "A14_NB3C_NO_BIN"
}

Invoke-G2CompileFinishedSound -Success $true
Write-Host "COMPILE_FINISHED_SOUND=PLAYED"

Write-Host ""
Write-Host "=== UPLOAD NB3-B RAW CANDIDATE ==="

$uploadArgs = @(
    "upload",
    "--fqbn", $fqbn,
    "--port", $SerialPort,
    "--input-dir", $buildPath,
    $sketchDir
)

$uploadExit = Invoke-NB3NativeToLog -FilePath $arduinoCli -Arguments $uploadArgs -LogPath $uploadLog

Write-Host "UPLOAD_EXIT=$uploadExit"
Write-Host "UPLOAD_LOG=$uploadLog"

if ($uploadExit -ne 0) {
    Get-Content -LiteralPath $uploadLog -Tail 120 | ForEach-Object { Write-Host $_ }
    throw "A14_NB3C_UPLOAD_FAILED"
}

$modeMap = @(
    [pscustomobject]@{ Cli = "tcp-rx"; Key = "TCP_RX" },
    [pscustomobject]@{ Cli = "tcp-tx"; Key = "TCP_TX" },
    [pscustomobject]@{ Cli = "udp-rx"; Key = "UDP_RX" },
    [pscustomobject]@{ Cli = "udp-tx"; Key = "UDP_TX" }
)

$durationText = $DurationSeconds.ToString(
    [System.Globalization.CultureInfo]::InvariantCulture
)

$preferredHoldReview = $false

Write-Host ""
Write-Host "=== FOUR-MODE PHYSICAL SMOKE ==="

foreach ($mode in $modeMap) {
    $runLog = Join-Path $tempRoot ("{0}.log" -f $mode.Key.ToLowerInvariant())

    $runnerArgs = @(
        $bridgePath,
        "--host", $DutIp,
        "--serial", $SerialPort,
        "--duration", $durationText,
        "--mode", $mode.Cli,
        "--tcp-chunk", "4096",
        "--udp-payload", "1472"
    )

    $runExit = Invoke-NB3NativeToLog -FilePath $pythonExe -Arguments $runnerArgs -LogPath $runLog

    Write-Host "MODE=$($mode.Key) RUNNER_EXIT=$runExit LOG=$runLog"

    if ($runExit -ne 0) {
        Get-Content -LiteralPath $runLog -Tail 120 | ForEach-Object { Write-Host $_ }
        throw "A14_NB3C_RUNNER_FAILED_$($mode.Key)"
    }

    $runText = [System.IO.File]::ReadAllText($runLog)

    if ((Get-NB3LogValue -Text $runText -Key "RAW_BENCH_FUNCTIONAL_PASS") -ne "YES") {
        throw "A14_NB3C_FUNCTIONAL_FAIL_$($mode.Key)"
    }

    if ((Get-NB3LogValue -Text $runText -Key "DUT_READY") -ne "YES") {
        throw "A14_NB3C_DUT_NOT_READY_$($mode.Key)"
    }

    if ((Get-NB3LogValue -Text $runText -Key "G2_FINAL_SNAPSHOT_PRESENT") -ne "YES") {
        throw "A14_NB3C_FINAL_SNAPSHOT_MISSING_$($mode.Key)"
    }

    Assert-NB3ExactValue -Text $runText -Key "RAW_SERVER_READY" -Expected "YES"
    Assert-NB3ExactValue -Text $runText -Key "ETH_READY" -Expected "YES"
    Assert-NB3ExactValue -Text $runText -Key "ETH_LINK" -Expected "UP"
    Assert-NB3ExactValue -Text $runText -Key "IP" -Expected $DutIp

    @(
        "TRANSPORT_ERRORS",
        "UDP_BEGIN_PACKET_ERRORS",
        "UDP_WRITE_ERRORS",
        "UDP_END_PACKET_ERRORS",
        "UDP_SPI_LOCK_ERRORS",
        "TCP_SPI_LOCK_ERRORS",
        "UDP_LAST_SHORT_WRITE_BYTES"
    ) | ForEach-Object {
        Assert-NB3ExactZero -Text $runText -Key $_
    }

    $pcMbps = Get-NB3LogValue -Text $runText -Key ("SUMMARY_{0}_PC_MBPS" -f $mode.Key)
    $dutMbps = Get-NB3LogValue -Text $runText -Key ("SUMMARY_{0}_DUT_MBPS" -f $mode.Key)
    $holdMaxUs = Get-NB3LogInt64 -Text $runText -Key "G2_FINAL_TCP_SPI_HOLD_MAX_US"
    $loopGapMaxUs = Get-NB3LogInt64 -Text $runText -Key "G2_FINAL_LOOP_GAP_MAX_US"

    Write-Host "MODE=$($mode.Key) PC_MBPS=$pcMbps DUT_MBPS=$dutMbps HOLD_MAX_US=$holdMaxUs LOOP_GAP_MAX_US=$loopGapMaxUs"

    if ($holdMaxUs -gt 10000) {
        throw "A14_NB3C_HARD_HOLD_REGRESSION_$($mode.Key)=$holdMaxUs"
    }

    if ($holdMaxUs -gt 5000) {
        $preferredHoldReview = $true
        Write-Host "MODE=$($mode.Key) PREFERRED_HOLD_5MS=REVIEW"
    }
    else {
        Write-Host "MODE=$($mode.Key) PREFERRED_HOLD_5MS=PASS"
    }

    if ($loopGapMaxUs -gt 15000) {
        throw "A14_NB3C_LOOP_GAP_REGRESSION_$($mode.Key)=$loopGapMaxUs"
    }

    if ($mode.Key -eq "UDP_TX") {
        @(
            "UDP_TX_SEQUENCE_DECODE_ERRORS",
            "UDP_TX_SEQUENCE_TOTAL_DUPLICATES",
            "UDP_TX_SEQUENCE_REORDERS",
            "UDP_TX_SEQUENCE_TOTAL_RANGE_MISSING",
            "UDP_TX_UNEXPECTED_SOURCE_PACKETS",
            "UDP_TX_WRONG_SIZE_FROM_DUT"
        ) | ForEach-Object {
            $integrityValue = Get-NB3LogInt64 -Text $runText -Key $_
            Write-Host "$_=$integrityValue"

            if ($integrityValue -ne 0) {
                throw "A14_NB3C_UDP_TX_INTEGRITY_FAIL_$($_)=$integrityValue"
            }
        }

        $sequenceCount = Get-NB3LogInt64 -Text $runText -Key "UDP_TX_SEQUENCE_TOTAL_COUNT"
        Write-Host "UDP_TX_SEQUENCE_TOTAL_COUNT=$sequenceCount"

        if ($sequenceCount -le 0) {
            throw "A14_NB3C_UDP_TX_SEQUENCE_EMPTY"
        }
    }

    Write-Host "NB3_C_MODE_$($mode.Key)=PASS"
    Start-Sleep -Milliseconds 400
}

Write-Host ""
Write-Host "OBSERVACION FISICA REQUERIDA: mira la TFT durante NB3-C."
$visualAnswer = ""

while ($visualAnswer -notin @("S", "N")) {
    $visualAnswer = (Read-Host 'Aparecio el diagnostico visual "SPI" durante NB3-C? (S/N)').Trim().ToUpperInvariant()
}

$visualEvents = if ($visualAnswer -eq "S") { 1 } else { 0 }
Write-Host "VISUAL_SPI_EVENTS=$visualEvents"

if ($visualEvents -ne 0) {
    throw "A14_NB3C_VISUAL_SPI_FAIL"
}

Write-Host ""
Write-Host "=== FINAL INVARIANTS ==="

Assert-G2ProtectedArtifacts
Assert-NB3CandidateHashes

$dirtyFinal = @(Get-G2TrackedDirtyPaths)
$stagedFinal = @(& git -C $script:G2RepoRoot diff --cached --name-only)

Write-Host "TRACKED_DIRTY_COUNT_FINAL=$($dirtyFinal.Count)"
$dirtyFinal | ForEach-Object { Write-Host "DIRTY_FINAL=$_" }
Write-Host "STAGED_COUNT_FINAL=$($stagedFinal.Count)"

if ($dirtyFinal.Count -ne $expectedDirty.Count -or $stagedFinal.Count -ne 0) {
    throw "A14_NB3C_FINAL_WORKTREE_INVALID"
}

for ($i = 0; $i -lt $expectedDirty.Count; ++$i) {
    if ($dirtyFinal[$i] -ne $expectedDirty[$i]) {
        throw "A14_NB3C_FINAL_DIRTY_PATH_INVALID=$($dirtyFinal[$i])"
    }
}

$preferredState = if ($preferredHoldReview) { "REVIEW_KNOWN_8_CHUNK_RX" } else { "PASS" }

Write-Host "NB3_BOUNDED_PRIMITIVES_RAW_COMPILE=PASS"
Write-Host "NB3_BOUNDED_PRIMITIVES_PHYSICAL_FOUR_MODES=PASS"
Write-Host "NB3_TRANSPORT_ERRORS=ZERO"
Write-Host "NB3_UDP_TX_INTEGRITY=PASS"
Write-Host "NB3_VISUAL_SPI=PASS"
Write-Host "NB3_HARD_HOLD_10MS=PASS"
Write-Host "NB3_HARD_LOOP_GAP_15MS=PASS"
Write-Host "NB3_PREFERRED_HOLD_5MS=$preferredState"
Write-Host "NB3_UDP_SEND_SYNC_PATH=UNCHANGED_PENDING_NEXT_GATE"
Write-Host "A14_NB3_BOUNDED_SOCKET_PRIMITIVES_PHYSICAL=PASS"

            )

            $ipMatches = @([regex]::Matches(
                $raw,
                '(?m)^IP=([0-9]{1,3}(?:\.[0-9]{1,3}){3})\r?    param()

    $expected = [ordered]@{
        "JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/Dns.cpp" = "3DE3CD1A14BD07C1FE3B0725941B2137A5109B43647AA3E10B3EF6A1E6EEC5F5"
        "JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/Dns.h" = "EF6C5392C0CBFB8545BFF8EB4D30E3FEDAC005725DF292418A5CF974BE6F6FD2"
        "JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/EthernetClient.cpp" = "93B71C92E298053425FF03D750632A30102BE743E1C8B1FDAB800214EABED4B2"
        "JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/JWPLC_W5x00_Ethernet.h" = "3E11094D68873D81F264F8AD97C1D55867F5399E28613061BC38484CD7DE614B"
        "JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/jwplc_ethernet_async_tx.cpp" = "CC8EEE779FC932C5A8FA0175ABBF0421AA8C95279B4260FADAC1276E7BD74E1C"
        "JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/jwplc_ethernet_async_tx.h" = "C1C5615DFF167F3572FBEA85CFFE1A7F5051732E2BD446387F1025A1D981FB47"
        "JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/socket.cpp" = "EF450C75880A2427494585E8EB1C1E3FD62EC7B9562E37A3AA75C48761886104"
        "JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/utility/w5100.cpp" = "9F94AAC1BB25966C18EDFD6A5C5D5908A9DBD4CF11B6E9BC099E10B28CEF272F"
        "JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/utility/w5100.h" = "9A833532C44E0CFCD66429A764E8BDBF62A8871838B0355565BC0A63043455FC"
        "tools/modbus-tcp-benchmark/firmware/eth14_raw_transport_server/eth14_raw_transport_server.ino" = "9A0C6CF27E44A61DC97686FB1A769EBBBB34D036CDF8D0CA0EEAB597E699AB6B"
    }

    foreach ($entry in $expected.GetEnumerator()) {
        $actual = Get-G2Sha256 $entry.Key
        Write-Host "CANDIDATE_SHA256=$($entry.Key)=$actual"

        if ($actual -ne $entry.Value) {
            throw "A14_NB3C_CANDIDATE_HASH_MISMATCH=$($entry.Key)"
        }
    }

    $runnerHash = Get-G2Sha256 $script:G2RawRunnerRelative
    Write-Host "RAW_RUNNER_SHA256=$runnerHash"

    if ($runnerHash -ne "FD25997CFE777B4FF50BBC10EFE555BD6B7A8A8DCE9D4ADD1E7D2DBA6D52470F") {
        throw "A14_NB3C_RAW_RUNNER_HASH_MISMATCH"
    }
}

Write-Host "============================================================"
Write-Host " A14 NB3-C - BOUNDED SOCKET PRIMITIVES PHYSICAL SMOKE"
Write-Host "============================================================"

Assert-G2Branch

if ($DurationSeconds -le 0) {
    throw "A14_NB3C_DURATION_MUST_BE_POSITIVE"
}

$expectedDirty = @(
    "JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/Dns.cpp",
    "JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/Dns.h",
    "JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/EthernetClient.cpp",
    "JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/JWPLC_W5x00_Ethernet.h",
    "JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/jwplc_ethernet_async_tx.cpp",
    "JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/jwplc_ethernet_async_tx.h",
    "JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/socket.cpp",
    "JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/utility/w5100.cpp",
    "JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/utility/w5100.h",
    "tools/modbus-tcp-benchmark/firmware/eth14_raw_transport_server/eth14_raw_transport_server.ino"
) | Sort-Object

$dirtyBefore = @(Get-G2TrackedDirtyPaths)

Write-Host "HEAD=$(Get-G2Head)"
Write-Host "EFFECTIVE_SPI_HZ=$(Get-G2SpiHz)"
Write-Host "SERIAL_PORT=$SerialPort"
Write-Host "DUT_IP=$DutIp"
Write-Host "DURATION_S=$DurationSeconds"
Write-Host "PREFERRED_HOLD_MAX_US=5000"
Write-Host "HARD_HOLD_MAX_US=10000"
Write-Host "HARD_LOOP_GAP_MAX_US=15000"
Write-Host "TRACKED_DIRTY_COUNT=$($dirtyBefore.Count)"
$dirtyBefore | ForEach-Object { Write-Host "DIRTY=$_" }

if ((Get-G2SpiHz) -ne 26000000) {
    throw "A14_NB3C_SPI_FREQUENCY_MISMATCH"
}

if ($dirtyBefore.Count -ne $expectedDirty.Count) {
    throw "A14_NB3C_DIRTY_COUNT_INVALID=$($dirtyBefore.Count)"
}

for ($i = 0; $i -lt $expectedDirty.Count; ++$i) {
    if ($dirtyBefore[$i] -ne $expectedDirty[$i]) {
        throw "A14_NB3C_DIRTY_PATH_INVALID=$($dirtyBefore[$i])"
    }
}

$stagedBefore = @(& git -C $script:G2RepoRoot diff --cached --name-only)
if ($LASTEXITCODE -ne 0 -or $stagedBefore.Count -ne 0) {
    throw "A14_NB3C_INDEX_NOT_CLEAN"
}
Write-Host "STAGED_COUNT=0"

Assert-G2ProtectedArtifacts
Assert-NB3CandidateHashes

$arduinoCli = "C:\Program Files\Arduino PLC IDE Tools\arduino-cli.exe"
if (-not (Test-Path -LiteralPath $arduinoCli)) {
    throw "A14_NB3C_ARDUINO_CLI_NOT_FOUND=$arduinoCli"
}

$pythonCommand = Get-Command python.exe -ErrorAction SilentlyContinue
if ($null -eq $pythonCommand) {
    $pythonCommand = Get-Command python -ErrorAction Stop
}

$pythonExe = $pythonCommand.Source
$fqbn = "jwplc_local:esp32:jwplcbasic"
$firmwarePath = Get-G2Path $script:G2RawFirmwareRelative
$sketchDir = Split-Path -Parent $firmwarePath
$bridgePath = Join-Path $PSScriptRoot "g2_runner_snapshot_bridge.py"
$librariesRoot = Get-G2Path "JWPLC/2.1.0/libraries"
$expectedEthernetLibrary = Join-Path $librariesRoot "JWPLC_Ethernet"

if (-not (Test-Path -LiteralPath $bridgePath)) {
    throw "A14_NB3C_SNAPSHOT_BRIDGE_NOT_FOUND"
}

$timestamp = Get-Date -Format "yyyyMMdd_HHmmss"
$tempRoot = Join-Path $env:TEMP ("jwplc_a14_nb3c_physical_{0}" -f $timestamp)
$buildPath = Join-Path $tempRoot "build"
$compileLog = Join-Path $tempRoot "compile.log"
$uploadLog = Join-Path $tempRoot "upload.log"

New-Item -ItemType Directory -Force -Path $buildPath | Out-Null

Write-Host "ARDUINO_CLI=$arduinoCli"
Write-Host "PYTHON=$pythonExe"
Write-Host "FQBN=$fqbn"
Write-Host "BRIDGE=$bridgePath"
Write-Host "TEMP_ROOT=$tempRoot"

Write-Host ""
Write-Host "=== COMPILE NB3-B RAW CANDIDATE ==="

$compileArgs = @(
    "compile",
    "--fqbn", $fqbn,
    "--build-path", $buildPath,
    "--libraries", $librariesRoot,
    $sketchDir
)

$compileExit = Invoke-NB3NativeToLog -FilePath $arduinoCli -Arguments $compileArgs -LogPath $compileLog

Write-Host "COMPILE_EXIT=$compileExit"
Write-Host "COMPILE_LOG=$compileLog"

if ($compileExit -ne 0) {
    Invoke-G2CompileFinishedSound -Success $false
    Get-Content -LiteralPath $compileLog -Tail 120 | ForEach-Object { Write-Host $_ }
    throw "A14_NB3C_COMPILE_FAILED"
}

$compileText = [System.IO.File]::ReadAllText($compileLog)
$repoEthernetUsed = (
    $compileText.IndexOf(
        $expectedEthernetLibrary,
        [System.StringComparison]::OrdinalIgnoreCase
    ) -ge 0
)

Write-Host "REPO_ETHERNET_LIBRARY_USED=$repoEthernetUsed"
if (-not $repoEthernetUsed) {
    throw "A14_NB3C_WRONG_ETHERNET_LIBRARY"
}

$binCount = @(
    Get-ChildItem -LiteralPath $buildPath -Recurse -File -Filter "*.bin"
).Count

Write-Host "BIN_COUNT=$binCount"
if ($binCount -lt 1) {
    Invoke-G2CompileFinishedSound -Success $false
    throw "A14_NB3C_NO_BIN"
}

Invoke-G2CompileFinishedSound -Success $true
Write-Host "COMPILE_FINISHED_SOUND=PLAYED"

Write-Host ""
Write-Host "=== UPLOAD NB3-B RAW CANDIDATE ==="

$uploadArgs = @(
    "upload",
    "--fqbn", $fqbn,
    "--port", $SerialPort,
    "--input-dir", $buildPath,
    $sketchDir
)

$uploadExit = Invoke-NB3NativeToLog -FilePath $arduinoCli -Arguments $uploadArgs -LogPath $uploadLog

Write-Host "UPLOAD_EXIT=$uploadExit"
Write-Host "UPLOAD_LOG=$uploadLog"

if ($uploadExit -ne 0) {
    Get-Content -LiteralPath $uploadLog -Tail 120 | ForEach-Object { Write-Host $_ }
    throw "A14_NB3C_UPLOAD_FAILED"
}

$modeMap = @(
    [pscustomobject]@{ Cli = "tcp-rx"; Key = "TCP_RX" },
    [pscustomobject]@{ Cli = "tcp-tx"; Key = "TCP_TX" },
    [pscustomobject]@{ Cli = "udp-rx"; Key = "UDP_RX" },
    [pscustomobject]@{ Cli = "udp-tx"; Key = "UDP_TX" }
)

$durationText = $DurationSeconds.ToString(
    [System.Globalization.CultureInfo]::InvariantCulture
)

$preferredHoldReview = $false

Write-Host ""
Write-Host "=== FOUR-MODE PHYSICAL SMOKE ==="

foreach ($mode in $modeMap) {
    $runLog = Join-Path $tempRoot ("{0}.log" -f $mode.Key.ToLowerInvariant())

    $runnerArgs = @(
        $bridgePath,
        "--host", $DutIp,
        "--serial", $SerialPort,
        "--duration", $durationText,
        "--mode", $mode.Cli,
        "--tcp-chunk", "4096",
        "--udp-payload", "1472"
    )

    $runExit = Invoke-NB3NativeToLog -FilePath $pythonExe -Arguments $runnerArgs -LogPath $runLog

    Write-Host "MODE=$($mode.Key) RUNNER_EXIT=$runExit LOG=$runLog"

    if ($runExit -ne 0) {
        Get-Content -LiteralPath $runLog -Tail 120 | ForEach-Object { Write-Host $_ }
        throw "A14_NB3C_RUNNER_FAILED_$($mode.Key)"
    }

    $runText = [System.IO.File]::ReadAllText($runLog)

    if ((Get-NB3LogValue -Text $runText -Key "RAW_BENCH_FUNCTIONAL_PASS") -ne "YES") {
        throw "A14_NB3C_FUNCTIONAL_FAIL_$($mode.Key)"
    }

    if ((Get-NB3LogValue -Text $runText -Key "DUT_READY") -ne "YES") {
        throw "A14_NB3C_DUT_NOT_READY_$($mode.Key)"
    }

    if ((Get-NB3LogValue -Text $runText -Key "G2_FINAL_SNAPSHOT_PRESENT") -ne "YES") {
        throw "A14_NB3C_FINAL_SNAPSHOT_MISSING_$($mode.Key)"
    }

    Assert-NB3ExactValue -Text $runText -Key "RAW_SERVER_READY" -Expected "YES"
    Assert-NB3ExactValue -Text $runText -Key "ETH_READY" -Expected "YES"
    Assert-NB3ExactValue -Text $runText -Key "ETH_LINK" -Expected "UP"
    Assert-NB3ExactValue -Text $runText -Key "IP" -Expected $DutIp

    @(
        "TRANSPORT_ERRORS",
        "UDP_BEGIN_PACKET_ERRORS",
        "UDP_WRITE_ERRORS",
        "UDP_END_PACKET_ERRORS",
        "UDP_SPI_LOCK_ERRORS",
        "TCP_SPI_LOCK_ERRORS",
        "UDP_LAST_SHORT_WRITE_BYTES"
    ) | ForEach-Object {
        Assert-NB3ExactZero -Text $runText -Key $_
    }

    $pcMbps = Get-NB3LogValue -Text $runText -Key ("SUMMARY_{0}_PC_MBPS" -f $mode.Key)
    $dutMbps = Get-NB3LogValue -Text $runText -Key ("SUMMARY_{0}_DUT_MBPS" -f $mode.Key)
    $holdMaxUs = Get-NB3LogInt64 -Text $runText -Key "G2_FINAL_TCP_SPI_HOLD_MAX_US"
    $loopGapMaxUs = Get-NB3LogInt64 -Text $runText -Key "G2_FINAL_LOOP_GAP_MAX_US"

    Write-Host "MODE=$($mode.Key) PC_MBPS=$pcMbps DUT_MBPS=$dutMbps HOLD_MAX_US=$holdMaxUs LOOP_GAP_MAX_US=$loopGapMaxUs"

    if ($holdMaxUs -gt 10000) {
        throw "A14_NB3C_HARD_HOLD_REGRESSION_$($mode.Key)=$holdMaxUs"
    }

    if ($holdMaxUs -gt 5000) {
        $preferredHoldReview = $true
        Write-Host "MODE=$($mode.Key) PREFERRED_HOLD_5MS=REVIEW"
    }
    else {
        Write-Host "MODE=$($mode.Key) PREFERRED_HOLD_5MS=PASS"
    }

    if ($loopGapMaxUs -gt 15000) {
        throw "A14_NB3C_LOOP_GAP_REGRESSION_$($mode.Key)=$loopGapMaxUs"
    }

    if ($mode.Key -eq "UDP_TX") {
        @(
            "UDP_TX_SEQUENCE_DECODE_ERRORS",
            "UDP_TX_SEQUENCE_TOTAL_DUPLICATES",
            "UDP_TX_SEQUENCE_REORDERS",
            "UDP_TX_SEQUENCE_TOTAL_RANGE_MISSING",
            "UDP_TX_UNEXPECTED_SOURCE_PACKETS",
            "UDP_TX_WRONG_SIZE_FROM_DUT"
        ) | ForEach-Object {
            $integrityValue = Get-NB3LogInt64 -Text $runText -Key $_
            Write-Host "$_=$integrityValue"

            if ($integrityValue -ne 0) {
                throw "A14_NB3C_UDP_TX_INTEGRITY_FAIL_$($_)=$integrityValue"
            }
        }

        $sequenceCount = Get-NB3LogInt64 -Text $runText -Key "UDP_TX_SEQUENCE_TOTAL_COUNT"
        Write-Host "UDP_TX_SEQUENCE_TOTAL_COUNT=$sequenceCount"

        if ($sequenceCount -le 0) {
            throw "A14_NB3C_UDP_TX_SEQUENCE_EMPTY"
        }
    }

    Write-Host "NB3_C_MODE_$($mode.Key)=PASS"
    Start-Sleep -Milliseconds 400
}

Write-Host ""
Write-Host "OBSERVACION FISICA REQUERIDA: mira la TFT durante NB3-C."
$visualAnswer = ""

while ($visualAnswer -notin @("S", "N")) {
    $visualAnswer = (Read-Host 'Aparecio el diagnostico visual "SPI" durante NB3-C? (S/N)').Trim().ToUpperInvariant()
}

$visualEvents = if ($visualAnswer -eq "S") { 1 } else { 0 }
Write-Host "VISUAL_SPI_EVENTS=$visualEvents"

if ($visualEvents -ne 0) {
    throw "A14_NB3C_VISUAL_SPI_FAIL"
}

Write-Host ""
Write-Host "=== FINAL INVARIANTS ==="

Assert-G2ProtectedArtifacts
Assert-NB3CandidateHashes

$dirtyFinal = @(Get-G2TrackedDirtyPaths)
$stagedFinal = @(& git -C $script:G2RepoRoot diff --cached --name-only)

Write-Host "TRACKED_DIRTY_COUNT_FINAL=$($dirtyFinal.Count)"
$dirtyFinal | ForEach-Object { Write-Host "DIRTY_FINAL=$_" }
Write-Host "STAGED_COUNT_FINAL=$($stagedFinal.Count)"

if ($dirtyFinal.Count -ne $expectedDirty.Count -or $stagedFinal.Count -ne 0) {
    throw "A14_NB3C_FINAL_WORKTREE_INVALID"
}

for ($i = 0; $i -lt $expectedDirty.Count; ++$i) {
    if ($dirtyFinal[$i] -ne $expectedDirty[$i]) {
        throw "A14_NB3C_FINAL_DIRTY_PATH_INVALID=$($dirtyFinal[$i])"
    }
}

$preferredState = if ($preferredHoldReview) { "REVIEW_KNOWN_8_CHUNK_RX" } else { "PASS" }

Write-Host "NB3_BOUNDED_PRIMITIVES_RAW_COMPILE=PASS"
Write-Host "NB3_BOUNDED_PRIMITIVES_PHYSICAL_FOUR_MODES=PASS"
Write-Host "NB3_TRANSPORT_ERRORS=ZERO"
Write-Host "NB3_UDP_TX_INTEGRITY=PASS"
Write-Host "NB3_VISUAL_SPI=PASS"
Write-Host "NB3_HARD_HOLD_10MS=PASS"
Write-Host "NB3_HARD_LOOP_GAP_15MS=PASS"
Write-Host "NB3_PREFERRED_HOLD_5MS=$preferredState"
Write-Host "NB3_UDP_SEND_SYNC_PATH=UNCHANGED_PENDING_NEXT_GATE"
Write-Host "A14_NB3_BOUNDED_SOCKET_PRIMITIVES_PHYSICAL=PASS"

            ))

            if (
                $ready -and
                $ethReady -and
                $linkUp -and
                $ipMatches.Count -eq 1
            ) {
                $ip = $ipMatches[0].Groups[1].Value

                if ($ip -ne "0.0.0.0") {
                    return $ip
                }
            }

            Start-Sleep -Milliseconds 250
        }
    }
    finally {
        if ($serial.IsOpen) {
            $serial.Close()
        }

        $serial.Dispose()
    }

    Write-Host "NB3_C_LAST_SERIAL_SNAPSHOT_BEGIN"
    Write-Host $lastSnapshot
    Write-Host "NB3_C_LAST_SERIAL_SNAPSHOT_END"

    throw "A14_NB3C_EFFECTIVE_DUT_IP_NOT_RESOLVED"
}

function Assert-NB3CandidateHashes {
    param()

    $expected = [ordered]@{
        "JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/Dns.cpp" = "3DE3CD1A14BD07C1FE3B0725941B2137A5109B43647AA3E10B3EF6A1E6EEC5F5"
        "JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/Dns.h" = "EF6C5392C0CBFB8545BFF8EB4D30E3FEDAC005725DF292418A5CF974BE6F6FD2"
        "JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/EthernetClient.cpp" = "93B71C92E298053425FF03D750632A30102BE743E1C8B1FDAB800214EABED4B2"
        "JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/JWPLC_W5x00_Ethernet.h" = "3E11094D68873D81F264F8AD97C1D55867F5399E28613061BC38484CD7DE614B"
        "JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/jwplc_ethernet_async_tx.cpp" = "CC8EEE779FC932C5A8FA0175ABBF0421AA8C95279B4260FADAC1276E7BD74E1C"
        "JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/jwplc_ethernet_async_tx.h" = "C1C5615DFF167F3572FBEA85CFFE1A7F5051732E2BD446387F1025A1D981FB47"
        "JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/socket.cpp" = "EF450C75880A2427494585E8EB1C1E3FD62EC7B9562E37A3AA75C48761886104"
        "JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/utility/w5100.cpp" = "9F94AAC1BB25966C18EDFD6A5C5D5908A9DBD4CF11B6E9BC099E10B28CEF272F"
        "JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/utility/w5100.h" = "9A833532C44E0CFCD66429A764E8BDBF62A8871838B0355565BC0A63043455FC"
        "tools/modbus-tcp-benchmark/firmware/eth14_raw_transport_server/eth14_raw_transport_server.ino" = "9A0C6CF27E44A61DC97686FB1A769EBBBB34D036CDF8D0CA0EEAB597E699AB6B"
    }

    foreach ($entry in $expected.GetEnumerator()) {
        $actual = Get-G2Sha256 $entry.Key
        Write-Host "CANDIDATE_SHA256=$($entry.Key)=$actual"

        if ($actual -ne $entry.Value) {
            throw "A14_NB3C_CANDIDATE_HASH_MISMATCH=$($entry.Key)"
        }
    }

    $runnerHash = Get-G2Sha256 $script:G2RawRunnerRelative
    Write-Host "RAW_RUNNER_SHA256=$runnerHash"

    if ($runnerHash -ne "FD25997CFE777B4FF50BBC10EFE555BD6B7A8A8DCE9D4ADD1E7D2DBA6D52470F") {
        throw "A14_NB3C_RAW_RUNNER_HASH_MISMATCH"
    }
}

Write-Host "============================================================"
Write-Host " A14 NB3-C - BOUNDED SOCKET PRIMITIVES PHYSICAL SMOKE"
Write-Host "============================================================"

Assert-G2Branch

if ($DurationSeconds -le 0) {
    throw "A14_NB3C_DURATION_MUST_BE_POSITIVE"
}

$expectedDirty = @(
    "JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/Dns.cpp",
    "JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/Dns.h",
    "JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/EthernetClient.cpp",
    "JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/JWPLC_W5x00_Ethernet.h",
    "JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/jwplc_ethernet_async_tx.cpp",
    "JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/jwplc_ethernet_async_tx.h",
    "JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/socket.cpp",
    "JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/utility/w5100.cpp",
    "JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/utility/w5100.h",
    "tools/modbus-tcp-benchmark/firmware/eth14_raw_transport_server/eth14_raw_transport_server.ino"
) | Sort-Object

$dirtyBefore = @(Get-G2TrackedDirtyPaths)

Write-Host "HEAD=$(Get-G2Head)"
Write-Host "EFFECTIVE_SPI_HZ=$(Get-G2SpiHz)"
Write-Host "SERIAL_PORT=$SerialPort"
Write-Host "DUT_IP=$DutIp"
Write-Host "DURATION_S=$DurationSeconds"
Write-Host "PREFERRED_HOLD_MAX_US=5000"
Write-Host "HARD_HOLD_MAX_US=10000"
Write-Host "HARD_LOOP_GAP_MAX_US=15000"
Write-Host "TRACKED_DIRTY_COUNT=$($dirtyBefore.Count)"
$dirtyBefore | ForEach-Object { Write-Host "DIRTY=$_" }

if ((Get-G2SpiHz) -ne 26000000) {
    throw "A14_NB3C_SPI_FREQUENCY_MISMATCH"
}

if ($dirtyBefore.Count -ne $expectedDirty.Count) {
    throw "A14_NB3C_DIRTY_COUNT_INVALID=$($dirtyBefore.Count)"
}

for ($i = 0; $i -lt $expectedDirty.Count; ++$i) {
    if ($dirtyBefore[$i] -ne $expectedDirty[$i]) {
        throw "A14_NB3C_DIRTY_PATH_INVALID=$($dirtyBefore[$i])"
    }
}

$stagedBefore = @(& git -C $script:G2RepoRoot diff --cached --name-only)
if ($LASTEXITCODE -ne 0 -or $stagedBefore.Count -ne 0) {
    throw "A14_NB3C_INDEX_NOT_CLEAN"
}
Write-Host "STAGED_COUNT=0"

Assert-G2ProtectedArtifacts
Assert-NB3CandidateHashes

$arduinoCli = "C:\Program Files\Arduino PLC IDE Tools\arduino-cli.exe"
if (-not (Test-Path -LiteralPath $arduinoCli)) {
    throw "A14_NB3C_ARDUINO_CLI_NOT_FOUND=$arduinoCli"
}

$pythonCommand = Get-Command python.exe -ErrorAction SilentlyContinue
if ($null -eq $pythonCommand) {
    $pythonCommand = Get-Command python -ErrorAction Stop
}

$pythonExe = $pythonCommand.Source
$fqbn = "jwplc_local:esp32:jwplcbasic"
$firmwarePath = Get-G2Path $script:G2RawFirmwareRelative
$sketchDir = Split-Path -Parent $firmwarePath
$bridgePath = Join-Path $PSScriptRoot "g2_runner_snapshot_bridge.py"
$librariesRoot = Get-G2Path "JWPLC/2.1.0/libraries"
$expectedEthernetLibrary = Join-Path $librariesRoot "JWPLC_Ethernet"

if (-not (Test-Path -LiteralPath $bridgePath)) {
    throw "A14_NB3C_SNAPSHOT_BRIDGE_NOT_FOUND"
}

$timestamp = Get-Date -Format "yyyyMMdd_HHmmss"
$tempRoot = Join-Path $env:TEMP ("jwplc_a14_nb3c_physical_{0}" -f $timestamp)
$buildPath = Join-Path $tempRoot "build"
$compileLog = Join-Path $tempRoot "compile.log"
$uploadLog = Join-Path $tempRoot "upload.log"

New-Item -ItemType Directory -Force -Path $buildPath | Out-Null

Write-Host "ARDUINO_CLI=$arduinoCli"
Write-Host "PYTHON=$pythonExe"
Write-Host "FQBN=$fqbn"
Write-Host "BRIDGE=$bridgePath"
Write-Host "TEMP_ROOT=$tempRoot"

Write-Host ""
Write-Host "=== COMPILE NB3-B RAW CANDIDATE ==="

$compileArgs = @(
    "compile",
    "--fqbn", $fqbn,
    "--build-path", $buildPath,
    "--libraries", $librariesRoot,
    $sketchDir
)

$compileExit = Invoke-NB3NativeToLog -FilePath $arduinoCli -Arguments $compileArgs -LogPath $compileLog

Write-Host "COMPILE_EXIT=$compileExit"
Write-Host "COMPILE_LOG=$compileLog"

if ($compileExit -ne 0) {
    Invoke-G2CompileFinishedSound -Success $false
    Get-Content -LiteralPath $compileLog -Tail 120 | ForEach-Object { Write-Host $_ }
    throw "A14_NB3C_COMPILE_FAILED"
}

$compileText = [System.IO.File]::ReadAllText($compileLog)
$repoEthernetUsed = (
    $compileText.IndexOf(
        $expectedEthernetLibrary,
        [System.StringComparison]::OrdinalIgnoreCase
    ) -ge 0
)

Write-Host "REPO_ETHERNET_LIBRARY_USED=$repoEthernetUsed"
if (-not $repoEthernetUsed) {
    throw "A14_NB3C_WRONG_ETHERNET_LIBRARY"
}

$binCount = @(
    Get-ChildItem -LiteralPath $buildPath -Recurse -File -Filter "*.bin"
).Count

Write-Host "BIN_COUNT=$binCount"
if ($binCount -lt 1) {
    Invoke-G2CompileFinishedSound -Success $false
    throw "A14_NB3C_NO_BIN"
}

Invoke-G2CompileFinishedSound -Success $true
Write-Host "COMPILE_FINISHED_SOUND=PLAYED"

Write-Host ""
Write-Host "=== UPLOAD NB3-B RAW CANDIDATE ==="

$uploadArgs = @(
    "upload",
    "--fqbn", $fqbn,
    "--port", $SerialPort,
    "--input-dir", $buildPath,
    $sketchDir
)

$uploadExit = Invoke-NB3NativeToLog -FilePath $arduinoCli -Arguments $uploadArgs -LogPath $uploadLog

Write-Host "UPLOAD_EXIT=$uploadExit"
Write-Host "UPLOAD_LOG=$uploadLog"

if ($uploadExit -ne 0) {
    Get-Content -LiteralPath $uploadLog -Tail 120 | ForEach-Object { Write-Host $_ }
    throw "A14_NB3C_UPLOAD_FAILED"
}

$modeMap = @(
    [pscustomobject]@{ Cli = "tcp-rx"; Key = "TCP_RX" },
    [pscustomobject]@{ Cli = "tcp-tx"; Key = "TCP_TX" },
    [pscustomobject]@{ Cli = "udp-rx"; Key = "UDP_RX" },
    [pscustomobject]@{ Cli = "udp-tx"; Key = "UDP_TX" }
)

$durationText = $DurationSeconds.ToString(
    [System.Globalization.CultureInfo]::InvariantCulture
)

$preferredHoldReview = $false

Write-Host ""
Write-Host "=== FOUR-MODE PHYSICAL SMOKE ==="

foreach ($mode in $modeMap) {
    $runLog = Join-Path $tempRoot ("{0}.log" -f $mode.Key.ToLowerInvariant())

    $runnerArgs = @(
        $bridgePath,
        "--host", $DutIp,
        "--serial", $SerialPort,
        "--duration", $durationText,
        "--mode", $mode.Cli,
        "--tcp-chunk", "4096",
        "--udp-payload", "1472"
    )

    $runExit = Invoke-NB3NativeToLog -FilePath $pythonExe -Arguments $runnerArgs -LogPath $runLog

    Write-Host "MODE=$($mode.Key) RUNNER_EXIT=$runExit LOG=$runLog"

    if ($runExit -ne 0) {
        Get-Content -LiteralPath $runLog -Tail 120 | ForEach-Object { Write-Host $_ }
        throw "A14_NB3C_RUNNER_FAILED_$($mode.Key)"
    }

    $runText = [System.IO.File]::ReadAllText($runLog)

    if ((Get-NB3LogValue -Text $runText -Key "RAW_BENCH_FUNCTIONAL_PASS") -ne "YES") {
        throw "A14_NB3C_FUNCTIONAL_FAIL_$($mode.Key)"
    }

    if ((Get-NB3LogValue -Text $runText -Key "DUT_READY") -ne "YES") {
        throw "A14_NB3C_DUT_NOT_READY_$($mode.Key)"
    }

    if ((Get-NB3LogValue -Text $runText -Key "G2_FINAL_SNAPSHOT_PRESENT") -ne "YES") {
        throw "A14_NB3C_FINAL_SNAPSHOT_MISSING_$($mode.Key)"
    }

    Assert-NB3ExactValue -Text $runText -Key "RAW_SERVER_READY" -Expected "YES"
    Assert-NB3ExactValue -Text $runText -Key "ETH_READY" -Expected "YES"
    Assert-NB3ExactValue -Text $runText -Key "ETH_LINK" -Expected "UP"
    Assert-NB3ExactValue -Text $runText -Key "IP" -Expected $DutIp

    @(
        "TRANSPORT_ERRORS",
        "UDP_BEGIN_PACKET_ERRORS",
        "UDP_WRITE_ERRORS",
        "UDP_END_PACKET_ERRORS",
        "UDP_SPI_LOCK_ERRORS",
        "TCP_SPI_LOCK_ERRORS",
        "UDP_LAST_SHORT_WRITE_BYTES"
    ) | ForEach-Object {
        Assert-NB3ExactZero -Text $runText -Key $_
    }

    $pcMbps = Get-NB3LogValue -Text $runText -Key ("SUMMARY_{0}_PC_MBPS" -f $mode.Key)
    $dutMbps = Get-NB3LogValue -Text $runText -Key ("SUMMARY_{0}_DUT_MBPS" -f $mode.Key)
    $holdMaxUs = Get-NB3LogInt64 -Text $runText -Key "G2_FINAL_TCP_SPI_HOLD_MAX_US"
    $loopGapMaxUs = Get-NB3LogInt64 -Text $runText -Key "G2_FINAL_LOOP_GAP_MAX_US"

    Write-Host "MODE=$($mode.Key) PC_MBPS=$pcMbps DUT_MBPS=$dutMbps HOLD_MAX_US=$holdMaxUs LOOP_GAP_MAX_US=$loopGapMaxUs"

    if ($holdMaxUs -gt 10000) {
        throw "A14_NB3C_HARD_HOLD_REGRESSION_$($mode.Key)=$holdMaxUs"
    }

    if ($holdMaxUs -gt 5000) {
        $preferredHoldReview = $true
        Write-Host "MODE=$($mode.Key) PREFERRED_HOLD_5MS=REVIEW"
    }
    else {
        Write-Host "MODE=$($mode.Key) PREFERRED_HOLD_5MS=PASS"
    }

    if ($loopGapMaxUs -gt 15000) {
        throw "A14_NB3C_LOOP_GAP_REGRESSION_$($mode.Key)=$loopGapMaxUs"
    }

    if ($mode.Key -eq "UDP_TX") {
        @(
            "UDP_TX_SEQUENCE_DECODE_ERRORS",
            "UDP_TX_SEQUENCE_TOTAL_DUPLICATES",
            "UDP_TX_SEQUENCE_REORDERS",
            "UDP_TX_SEQUENCE_TOTAL_RANGE_MISSING",
            "UDP_TX_UNEXPECTED_SOURCE_PACKETS",
            "UDP_TX_WRONG_SIZE_FROM_DUT"
        ) | ForEach-Object {
            $integrityValue = Get-NB3LogInt64 -Text $runText -Key $_
            Write-Host "$_=$integrityValue"

            if ($integrityValue -ne 0) {
                throw "A14_NB3C_UDP_TX_INTEGRITY_FAIL_$($_)=$integrityValue"
            }
        }

        $sequenceCount = Get-NB3LogInt64 -Text $runText -Key "UDP_TX_SEQUENCE_TOTAL_COUNT"
        Write-Host "UDP_TX_SEQUENCE_TOTAL_COUNT=$sequenceCount"

        if ($sequenceCount -le 0) {
            throw "A14_NB3C_UDP_TX_SEQUENCE_EMPTY"
        }
    }

    Write-Host "NB3_C_MODE_$($mode.Key)=PASS"
    Start-Sleep -Milliseconds 400
}

Write-Host ""
Write-Host "OBSERVACION FISICA REQUERIDA: mira la TFT durante NB3-C."
$visualAnswer = ""

while ($visualAnswer -notin @("S", "N")) {
    $visualAnswer = (Read-Host 'Aparecio el diagnostico visual "SPI" durante NB3-C? (S/N)').Trim().ToUpperInvariant()
}

$visualEvents = if ($visualAnswer -eq "S") { 1 } else { 0 }
Write-Host "VISUAL_SPI_EVENTS=$visualEvents"

if ($visualEvents -ne 0) {
    throw "A14_NB3C_VISUAL_SPI_FAIL"
}

Write-Host ""
Write-Host "=== FINAL INVARIANTS ==="

Assert-G2ProtectedArtifacts
Assert-NB3CandidateHashes

$dirtyFinal = @(Get-G2TrackedDirtyPaths)
$stagedFinal = @(& git -C $script:G2RepoRoot diff --cached --name-only)

Write-Host "TRACKED_DIRTY_COUNT_FINAL=$($dirtyFinal.Count)"
$dirtyFinal | ForEach-Object { Write-Host "DIRTY_FINAL=$_" }
Write-Host "STAGED_COUNT_FINAL=$($stagedFinal.Count)"

if ($dirtyFinal.Count -ne $expectedDirty.Count -or $stagedFinal.Count -ne 0) {
    throw "A14_NB3C_FINAL_WORKTREE_INVALID"
}

for ($i = 0; $i -lt $expectedDirty.Count; ++$i) {
    if ($dirtyFinal[$i] -ne $expectedDirty[$i]) {
        throw "A14_NB3C_FINAL_DIRTY_PATH_INVALID=$($dirtyFinal[$i])"
    }
}

$preferredState = if ($preferredHoldReview) { "REVIEW_KNOWN_8_CHUNK_RX" } else { "PASS" }

Write-Host "NB3_BOUNDED_PRIMITIVES_RAW_COMPILE=PASS"
Write-Host "NB3_BOUNDED_PRIMITIVES_PHYSICAL_FOUR_MODES=PASS"
Write-Host "NB3_TRANSPORT_ERRORS=ZERO"
Write-Host "NB3_UDP_TX_INTEGRITY=PASS"
Write-Host "NB3_VISUAL_SPI=PASS"
Write-Host "NB3_HARD_HOLD_10MS=PASS"
Write-Host "NB3_HARD_LOOP_GAP_15MS=PASS"
Write-Host "NB3_PREFERRED_HOLD_5MS=$preferredState"
Write-Host "NB3_UDP_SEND_SYNC_PATH=UNCHANGED_PENDING_NEXT_GATE"
Write-Host "A14_NB3_BOUNDED_SOCKET_PRIMITIVES_PHYSICAL=PASS"
