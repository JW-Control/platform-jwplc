param(
    [string]$MasterPort = "COM14",
    [string]$SlavePort = "COM4",
    [double]$TcpRate = 1000.0,
    [double]$DurationS = 120.0,
    [switch]$PreflightOnly
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"
. (Join-Path $PSScriptRoot "common.ps1")

Write-Host "============================================================"
Write-Host " A14 H3E.5 - FINAL FULL RUNTIME REGRESSION"
Write-Host " RTU + TCP + DISPLAY + SD + FRAM + RTC + TCA/I-O + BUTTONS"
Write-Host "============================================================"

Assert-G2Branch

$expectedTftSha =
    "5D860A131811DD9A7EB6FA55F5674B1D78B0DE7DFAF8748CE18A60CEED2D3738"

$expectedDisplaySha =
    "52B9BC617FACB77705161B4F07E6D45571043E4473934EFE19A1F5444BB5D986"

$tftArchive =
    Get-G2Path "JWPLC/2.1.0/libraries/JWPLC_TFT/src/esp32/libJWPLC_TFT.a"

$displayArchive =
    Get-G2Path "JWPLC/2.1.0/libraries/JWPLC_Display/src/esp32/libJWPLC_Display.a"

$modbusRtuArchive =
    Get-G2Path "JWPLC/2.1.0/libraries/JWPLC_ModbusRTU/src/esp32/libJWPLC_ModbusRTU.a"

$staleModbusRtuSha =
    "444BE3A04079A579252B2737FE6070E00ADCA949FD176880588FE69561B2A79F"

$p5bGate =
    Join-Path $PSScriptRoot "a14_p5b_physical_master_slave_combined.ps1"

foreach ($required in @(
    $tftArchive,
    $displayArchive,
    $modbusRtuArchive,
    $p5bGate
)) {
    if (-not (Test-Path -LiteralPath $required)) {
        throw "H3E5_REQUIRED_PATH_MISSING=$required"
    }
}

if ($TcpRate -ne 1000.0) {
    throw "H3E5_TCP_RATE_MUST_BE_1000"
}

if ($DurationS -lt 120.0) {
    throw "H3E5_DURATION_MUST_BE_AT_LEAST_120S"
}

[string[]]$entryDirty =
    @(Get-G2TrackedDirtyPaths)

[string[]]$entryStaged =
    @(& git -C $script:G2RepoRoot diff --cached --name-only)

[string[]]$normalizedEntryDirty = @(
    $entryDirty |
        ForEach-Object {
            $_.Replace("\", "/")
        } |
        Sort-Object
)

$expectedCoreDirty =
    $script:G2CoreRelative.Replace("\", "/")

$expectedModbusDirty =
    "JWPLC/2.1.0/libraries/JWPLC_ModbusRTU/src/esp32/libJWPLC_ModbusRTU.a"

[string[]]$expectedEntryDirty = @(
    $expectedCoreDirty,
    $expectedModbusDirty
) |
    Sort-Object

[object[]]$entryDirtyDiff = @(
    Compare-Object -ReferenceObject $expectedEntryDirty -DifferenceObject $normalizedEntryDirty
)

if ($entryDirtyDiff.Count -ne 0) {
    $normalizedEntryDirty |
        ForEach-Object {
            Write-Host "ENTRY_DIRTY=$_"
        }

    throw "H3E5_ENTRY_DIRTY_SCOPE_INVALID"
}

if ($entryStaged.Count -ne 0) {
    throw "H3E5_ENTRY_INDEX_NOT_CLEAN"
}

$tftSha =
    (Get-G2Sha256Path $tftArchive).ToUpperInvariant()

$displaySha =
    (Get-G2Sha256Path $displayArchive).ToUpperInvariant()

$modbusRtuSha =
    (Get-G2Sha256Path $modbusRtuArchive).ToUpperInvariant()

if ($modbusRtuSha -eq $staleModbusRtuSha) {
    throw "H3E5_MODBUS_RTU_ARCHIVE_STILL_STALE_ALPHA7"
}

$tcpRateText =
    $TcpRate.ToString(
        "F0",
        [Globalization.CultureInfo]::InvariantCulture)

$durationText =
    $DurationS.ToString(
        "F0",
        [Globalization.CultureInfo]::InvariantCulture)

Write-Host "HEAD=$(Get-G2Head)"
Write-Host "H3E5_MASTER_PORT=$MasterPort"
Write-Host "H3E5_SLAVE_PORT=$SlavePort"
Write-Host "H3E5_TCP_TARGET_REQ_S=$tcpRateText"
Write-Host "H3E5_DURATION_S=$durationText"
Write-Host "H3E5_RTU_TARGET_HZ=50"
Write-Host "H3E5_RTU_BAUD=115200"
Write-Host "H3E5_RTU_CONFIG=8N1"
Write-Host "H3E5_W5500_SPI_HZ=$(Get-G2SpiHz)"
Write-Host "H3E5_TFT_ARCHIVE_SHA256=$tftSha"
Write-Host "H3E5_DISPLAY_ARCHIVE_SHA256=$displaySha"
Write-Host "H3E5_MODBUS_RTU_ARCHIVE_SHA256=$modbusRtuSha"

if ($tftSha -ne $expectedTftSha) {
    throw "H3E5_TFT_ARCHIVE_NOT_H3E4A2_PHYSICAL"
}

if ($displaySha -ne $expectedDisplaySha) {
    throw "H3E5_DISPLAY_ARCHIVE_NOT_H3E3D_PHYSICAL"
}

Write-Host "H3E5_TFT_ARCHIVE_PROVENANCE=H3E4A2_PHYSICAL"
Write-Host "H3E5_DISPLAY_ARCHIVE_PROVENANCE=H3E3D_PHYSICAL"

function Get-H3E5Marker {
    param(
        [string[]]$Lines,
        [string]$Prefix
    )

    [string[]]$matches = @(
        $Lines |
            Where-Object {
                $_.StartsWith(
                    $Prefix,
                    [StringComparison]::Ordinal)
            }
    )

    if ($matches.Count -ne 1) {
        throw ("H3E5_MARKER_COUNT_INVALID={0}:{1}" -f $Prefix, $matches.Count)
    }

    return $matches[0].Substring($Prefix.Length).Trim()
}

function Get-H3E5SnapshotValue {
    param(
        [string]$Text,
        [string]$Key
    )

    $pattern =
        "(?m)^" +
        [regex]::Escape($Key) +
        "=(.*)\r?$"

    [object[]]$matches =
        @([regex]::Matches($Text, $pattern))

    if ($matches.Count -ne 1) {
        throw ("H3E5_SNAPSHOT_KEY_COUNT_INVALID={0}:{1}" -f $Key, $matches.Count)
    }

    return $matches[0].Groups[1].Value.Trim()
}

function Assert-H3E5SnapshotValue {
    param(
        [string]$Text,
        [string]$Key,
        [string]$Expected,
        [string]$Label
    )

    $value =
        Get-H3E5SnapshotValue -Text $Text -Key $Key

    Write-Host ("{0}_{1}={2}" -f $Label, $Key, $value)

    if ($value -ne $Expected) {
        throw ("H3E5_{0}_{1}_EXPECTED_{2}_GOT_{3}" -f $Label, $Key, $Expected, $value)
    }
}

function Assert-H3E5Build {
    param(
        [string]$CompileLog,
        [string]$BuildPath,
        [string]$Label
    )

    if (-not (Test-Path -LiteralPath $CompileLog)) {
        throw "H3E5_COMPILE_LOG_MISSING=$CompileLog"
    }

    if (-not (Test-Path -LiteralPath $BuildPath)) {
        throw "H3E5_BUILD_PATH_MISSING=$BuildPath"
    }

    [string[]]$lines =
        @(Get-Content -LiteralPath $CompileLog)

    $text =
        $lines -join [Environment]::NewLine

    $displaySelected =
        @(
            $lines |
                Where-Object {
                    ($_ -match '^Using library JWPLC_Display at version ') -or
                    ($_ -match '^\s*JWPLC_Display\s+\S+\s+.+[\\/]JWPLC_Display\s*

    $displayPrecompiled =
        @(
            $lines |
                Where-Object {
                    ($_ -match 'Using precompiled library|Usando libreria precompilada|Usando biblioteca precompilada') -and
                    ($_ -match 'JWPLC_Display')
                }
        ).Count -gt 0

    $tftPrecompiled =
        @(
            $lines |
                Where-Object {
                    ($_ -match 'Using precompiled library|Usando libreria precompilada|Usando biblioteca precompilada') -and
                    ($_ -match 'JWPLC_TFT')
                }
        ).Count -gt 0

    $modbusRtuPrecompiled =
        @(
            $lines |
                Where-Object {
                    ($_ -match 'Using precompiled library|Usando libreria precompilada|Usando biblioteca precompilada') -and
                    ($_ -match 'JWPLC_ModbusRTU')
                }
        ).Count -gt 0

    [string[]]$externalTftEspi = @(
        $lines |
            Where-Object {
                ($_ -match '^Using library TFT_eSPI at version ') -or
                ($_ -match '^\s*TFT_eSPI\s+\S+\s+.+[\\/]TFT_eSPI\s*

    [int]$tftSourceObjects = @(
        Get-ChildItem -LiteralPath $BuildPath -Recurse -File -ErrorAction SilentlyContinue |
            Where-Object {
                $_.Name -eq "JWPLC_TFT.cpp.o"
            }
    ).Count

    [int]$tftEspiSourceObjects = @(
        Get-ChildItem -LiteralPath $BuildPath -Recurse -File -ErrorAction SilentlyContinue |
            Where-Object {
                $_.Name -eq "TFT_eSPI.cpp.o"
            }
    ).Count

    [int]$displaySourceObjects = @(
        Get-ChildItem -LiteralPath $BuildPath -Recurse -File -ErrorAction SilentlyContinue |
            Where-Object {
                $_.FullName -match '[\\/]libraries[\\/]JWPLC_Display[\\/].+\.cpp\.o$'
            }
    ).Count

    [int]$modbusRtuSourceObjects = @(
        Get-ChildItem -LiteralPath $BuildPath -Recurse -File -ErrorAction SilentlyContinue |
            Where-Object {
                $_.Name -eq "JWPLC_ModbusRTU.cpp.o"
            }
    ).Count

    Write-Host ("{0}_DISPLAY_SELECTED={1}" -f $Label, $displaySelected)
    Write-Host ("{0}_DISPLAY_PRECOMPILED={1}" -f $Label, $displayPrecompiled)
    Write-Host ("{0}_JWPLC_TFT_SELECTED={1}" -f $Label, $tftSelected)
    Write-Host ("{0}_JWPLC_TFT_PRECOMPILED={1}" -f $Label, $tftPrecompiled)
    Write-Host ("{0}_MODBUS_RTU_SELECTED={1}" -f $Label, $modbusRtuSelected)
    Write-Host ("{0}_MODBUS_RTU_PRECOMPILED={1}" -f $Label, $modbusRtuPrecompiled)
    Write-Host ("{0}_EXTERNAL_TFT_ESPI_SELECTION_COUNT={1}" -f $Label, $externalTftEspi.Count)
    Write-Host ("{0}_DISPLAY_SOURCE_OBJECT_COUNT={1}" -f $Label, $displaySourceObjects)
    Write-Host ("{0}_JWPLC_TFT_SOURCE_OBJECT_COUNT={1}" -f $Label, $tftSourceObjects)
    Write-Host ("{0}_TFT_ESPI_SOURCE_OBJECT_COUNT={1}" -f $Label, $tftEspiSourceObjects)
    Write-Host ("{0}_MODBUS_RTU_SOURCE_OBJECT_COUNT={1}" -f $Label, $modbusRtuSourceObjects)

    if (-not $displaySelected -or
        -not $displayPrecompiled -or
        -not $tftSelected -or
        -not $tftPrecompiled -or
        -not $modbusRtuSelected -or
        -not $modbusRtuPrecompiled -or
        $externalTftEspi.Count -ne 0 -or
        $displaySourceObjects -ne 0 -or
        $tftSourceObjects -ne 0 -or
        $tftEspiSourceObjects -ne 0 -or
        $modbusRtuSourceObjects -ne 0) {
        throw ("H3E5_{0}_BUILD_POLICY_FAILED" -f $Label)
    }
}

Write-Host ""
Write-Host "=== H3E5 STATIC PREFLIGHT ==="
Write-Host "H3E5_PROFILE=FULL_RUNTIME_MASTER_SLAVE"
Write-Host "H3E5_MASTER_PERIPHERALS=DISPLAY,ETHERNET,SD,FRAM,RTC,TCA_IO,BUTTONS,RS485,MODBUS_RTU,MODBUS_TCP"
Write-Host "H3E5_SLAVE_PERIPHERALS=DISPLAY,RS485,MODBUS_RTU"
Write-Host "H3E5_PRODUCT_SOURCE_MUTATION=NO"

if ($PreflightOnly) {
    Write-Host "H3E5_PREFLIGHT_COMPILES=NO"
    Write-Host "H3E5_PREFLIGHT_UPLOADS=NO"
    Write-Host "H3E5_PREFLIGHT_REPOSITORY_MUTATION=NO"
    Write-Host "A14_H3E5_FINAL_FULL_RUNTIME_PREFLIGHT=PASS"
    return
}

Write-Host ""
Write-Host "=== H3E5 RUN P5B FULL RUNTIME ENGINE ==="

$p5bArgs = @{
    MasterPort = $MasterPort
    SlavePort = $SlavePort
    TcpRate = $TcpRate
    DurationS = $DurationS
    AllowDirtyCoreCandidate = $true
    AllowMissingModbusRtuArchiveCandidate = $true
}

[object[]]$p5bOutput =
    @(& $p5bGate @p5bArgs *>&1)

[string[]]$p5bLines = @(
    $p5bOutput |
        ForEach-Object {
            $_.ToString()
        }
)

$p5bLines |
    ForEach-Object {
        Write-Host $_
    }

$p5bText =
    $p5bLines -join [Environment]::NewLine

if (-not $p5bText.Contains(
    "A14_P5B_PHYSICAL_MASTER_SLAVE_COMBINED=PASS")) {
    throw "H3E5_P5B_NOT_PASS"
}

Write-Host "H3E5_P5B_FULL_RUNTIME=PASS"

$tempRoot =
    Get-H3E5Marker -Lines $p5bLines -Prefix "P5B_TEMP_ROOT="

$qualificationLog =
    Join-Path $tempRoot "qualification.log"

$masterSnapshot =
    Join-Path $tempRoot "master_final.txt"

$slaveSnapshot =
    Join-Path $tempRoot "slave_final.txt"

$masterBuild =
    Join-Path $tempRoot "build_master"

$slaveBuild =
    Join-Path $tempRoot "build_slave"

$masterCompileLog =
    Join-Path $tempRoot "compile_master.log"

$slaveCompileLog =
    Join-Path $tempRoot "compile_slave.log"

foreach ($required in @(
    $qualificationLog,
    $masterSnapshot,
    $slaveSnapshot,
    $masterBuild,
    $slaveBuild,
    $masterCompileLog,
    $slaveCompileLog
)) {
    if (-not (Test-Path -LiteralPath $required)) {
        throw "H3E5_P5B_ARTIFACT_MISSING=$required"
    }
}

$qualificationText =
    [IO.File]::ReadAllText($qualificationLog)

foreach ($requiredLine in @(
    "TCP_FULL_RUNTIME_PASS=YES",
    "MASTER_RUNTIME_PASS=YES",
    "RTU_MASTER_PASS=YES",
    "RTU_SLAVE_PASS=YES",
    "RTU_CROSS_COUNT_PASS=YES",
    "RTU_SNAPSHOT_TAIL_TOLERANCE=0",
    "RTU_FINAL_SNAPSHOT_MODE=QUIESCED",
    "A14_P5B_AUTOMATED=PASS"
)) {
    if (-not $qualificationText.Contains($requiredLine)) {
        throw "H3E5_QUALIFICATION_MARKER_MISSING=$requiredLine"
    }

    Write-Host "H3E5_QUALIFICATION=$requiredLine"
}

$masterBuildArgs = @{
    CompileLog = $masterCompileLog
    BuildPath = $masterBuild
    Label = "H3E5_MASTER_BUILD"
}

Assert-H3E5Build @masterBuildArgs

$slaveBuildArgs = @{
    CompileLog = $slaveCompileLog
    BuildPath = $slaveBuild
    Label = "H3E5_SLAVE_BUILD"
}

Assert-H3E5Build @slaveBuildArgs

$masterText =
    [IO.File]::ReadAllText($masterSnapshot)

$slaveText =
    [IO.File]::ReadAllText($slaveSnapshot)

$masterExpectations = @(
    @("FULL_RUNTIME_READY", "YES"),
    @("ETH_READY", "YES"),
    @("ETH_LINK", "UP"),
    @("DISPLAY_READY", "YES"),
    @("FRAM_READY", "YES"),
    @("FRAM_FAILS", "0"),
    @("SD_READY", "YES"),
    @("SD_DATALOG_ACTIVE", "YES"),
    @("SD_DATALOG_FAILED_COMMITS", "0"),
    @("RTC_PRESENT", "YES"),
    @("RTC_UNAVAILABLE", "0"),
    @("RTC_STALE", "0"),
    @("IO_INITIALIZED", "YES"),
    @("IO_STALE", "0"),
    @("BUTTONS_READY", "YES"),
    @("BUTTON_NOT_READY", "0"),
    @("SPI_PROBE_FAILS", "0"),
    @("PERIPHERAL_FAILURE_COUNT", "0"),
    @("RTU_READY", "YES"),
    @("RTU_REQUESTS_FAILED", "0"),
    @("RTU_VERIFY_FAILS", "0"),
    @("RTU_CRC_ERRORS", "0"),
    @("RTU_MASTER_TIMEOUTS", "0"),
    @("RTU_LAST_ERROR", "OK")
)

foreach ($pair in $masterExpectations) {
    $assertArgs = @{
        Text = $masterText
        Key = $pair[0]
        Expected = $pair[1]
        Label = "MASTER"
    }

    Assert-H3E5SnapshotValue @assertArgs
}

$slaveExpectations = @(
    @("SLAVE_READY", "YES"),
    @("RTU_READY", "YES"),
    @("RTU_ROLE", "SLAVE"),
    @("RTU_SLAVE_ID", "2"),
    @("RTU_BAUD", "115200"),
    @("DISPLAY_READY", "YES"),
    @("RTU_CRC_ERRORS", "0"),
    @("RTU_EXCEPTIONS_SENT", "0"),
    @("RTU_LAST_ERROR", "OK"),
    @("SLAVE_HR1", "21930")
)

foreach ($pair in $slaveExpectations) {
    $assertArgs = @{
        Text = $slaveText
        Key = $pair[0]
        Expected = $pair[1]
        Label = "SLAVE"
    }

    Assert-H3E5SnapshotValue @assertArgs
}

$tftShaFinal =
    (Get-G2Sha256Path $tftArchive).ToUpperInvariant()

$displayShaFinal =
    (Get-G2Sha256Path $displayArchive).ToUpperInvariant()

$modbusRtuShaFinal =
    (Get-G2Sha256Path $modbusRtuArchive).ToUpperInvariant()

if ($tftShaFinal -ne $expectedTftSha) {
    throw "H3E5_TFT_ARCHIVE_CHANGED"
}

if ($displayShaFinal -ne $expectedDisplaySha) {
    throw "H3E5_DISPLAY_ARCHIVE_CHANGED"
}

if ($modbusRtuShaFinal -ne $modbusRtuSha) {
    throw "H3E5_MODBUS_RTU_ARCHIVE_CHANGED"
}

[string[]]$finalDirty =
    @(Get-G2TrackedDirtyPaths)

[string[]]$finalStaged =
    @(& git -C $script:G2RepoRoot diff --cached --name-only)

[string[]]$normalizedFinalDirty = @(
    $finalDirty |
        ForEach-Object {
            $_.Replace("\", "/")
        } |
        Sort-Object
)

[string[]]$expectedFinalDirty = @(
    $expectedCoreDirty,
    $expectedModbusDirty
) |
    Sort-Object

[object[]]$finalDirtyDiff = @(
    Compare-Object -ReferenceObject $expectedFinalDirty -DifferenceObject $normalizedFinalDirty
)

if ($finalDirtyDiff.Count -ne 0) {
    throw "H3E5_FINAL_DIRTY_SCOPE_INVALID"
}

if ($finalStaged.Count -ne 0) {
    throw "H3E5_FINAL_INDEX_NOT_CLEAN"
}

Write-Host ""
Write-Host "============================================================"
Write-Host " A14 H3E.5 FINAL SUMMARY"
Write-Host "============================================================"
Write-Host "H3E5_TCP_FULL_RUNTIME=PASS"
Write-Host "H3E5_RTU_MASTER=PASS"
Write-Host "H3E5_RTU_SLAVE=PASS"
Write-Host "H3E5_RTU_CROSS_COUNT=PASS"
Write-Host "H3E5_DISPLAY_MASTER=PASS"
Write-Host "H3E5_DISPLAY_SLAVE=PASS"
Write-Host "H3E5_ETHERNET=PASS"
Write-Host "H3E5_MICROSD_BUFFERED_DATALOG=PASS"
Write-Host "H3E5_FRAM=PASS"
Write-Host "H3E5_RTC=PASS"
Write-Host "H3E5_TCA_IO=PASS"
Write-Host "H3E5_BUTTONS=PASS"
Write-Host "H3E5_SPI_OWNERSHIP=PASS"
Write-Host "H3E5_MODBUS_RTU_PRECOMPILED=PASS"
Write-Host "H3E5_MODBUS_RTU_ARCHIVE_SHA256=$modbusRtuShaFinal"
Write-Host "H3E5_EXTERNAL_TFT_ESPI_AT_USER_BUILD=NO"
Write-Host "H3E5_PRODUCT_SOURCE_MUTATION=NO"
Write-Host "H3E5_REPOSITORY_MUTATION=NO"
Write-Host "A14_H3E5_FINAL_FULL_RUNTIME_REGRESSION=PASS"
Write-Host "NEXT=RETURN_OUTPUT_TO_CHAT_FOR_H3E_CLOSURE"
)
                }
        ).Count -gt 0

    $tftSelected =
        @(
            $lines |
                Where-Object {
                    ($_ -match '^Using library JWPLC_TFT at version ') -or
                    ($_ -match '^\s*JWPLC_TFT\s+\S+\s+.+[\\/]JWPLC_TFT\s*

    $displayPrecompiled =
        @(
            $lines |
                Where-Object {
                    ($_ -match 'Using precompiled library|Usando libreria precompilada|Usando biblioteca precompilada') -and
                    ($_ -match 'JWPLC_Display')
                }
        ).Count -gt 0

    $tftPrecompiled =
        @(
            $lines |
                Where-Object {
                    ($_ -match 'Using precompiled library|Usando libreria precompilada|Usando biblioteca precompilada') -and
                    ($_ -match 'JWPLC_TFT')
                }
        ).Count -gt 0

    $modbusRtuPrecompiled =
        @(
            $lines |
                Where-Object {
                    ($_ -match 'Using precompiled library|Usando libreria precompilada|Usando biblioteca precompilada') -and
                    ($_ -match 'JWPLC_ModbusRTU')
                }
        ).Count -gt 0

    [string[]]$externalTftEspi = @(
        $lines |
            Where-Object {
                $_ -match '^Using library TFT_eSPI at version '
            }
    )

    [int]$tftSourceObjects = @(
        Get-ChildItem -LiteralPath $BuildPath -Recurse -File -ErrorAction SilentlyContinue |
            Where-Object {
                $_.Name -eq "JWPLC_TFT.cpp.o"
            }
    ).Count

    [int]$tftEspiSourceObjects = @(
        Get-ChildItem -LiteralPath $BuildPath -Recurse -File -ErrorAction SilentlyContinue |
            Where-Object {
                $_.Name -eq "TFT_eSPI.cpp.o"
            }
    ).Count

    [int]$displaySourceObjects = @(
        Get-ChildItem -LiteralPath $BuildPath -Recurse -File -ErrorAction SilentlyContinue |
            Where-Object {
                $_.FullName -match '[\\/]libraries[\\/]JWPLC_Display[\\/].+\.cpp\.o$'
            }
    ).Count

    [int]$modbusRtuSourceObjects = @(
        Get-ChildItem -LiteralPath $BuildPath -Recurse -File -ErrorAction SilentlyContinue |
            Where-Object {
                $_.Name -eq "JWPLC_ModbusRTU.cpp.o"
            }
    ).Count

    Write-Host ("{0}_DISPLAY_SELECTED={1}" -f $Label, $displaySelected)
    Write-Host ("{0}_DISPLAY_PRECOMPILED={1}" -f $Label, $displayPrecompiled)
    Write-Host ("{0}_JWPLC_TFT_SELECTED={1}" -f $Label, $tftSelected)
    Write-Host ("{0}_JWPLC_TFT_PRECOMPILED={1}" -f $Label, $tftPrecompiled)
    Write-Host ("{0}_MODBUS_RTU_SELECTED={1}" -f $Label, $modbusRtuSelected)
    Write-Host ("{0}_MODBUS_RTU_PRECOMPILED={1}" -f $Label, $modbusRtuPrecompiled)
    Write-Host ("{0}_EXTERNAL_TFT_ESPI_SELECTION_COUNT={1}" -f $Label, $externalTftEspi.Count)
    Write-Host ("{0}_DISPLAY_SOURCE_OBJECT_COUNT={1}" -f $Label, $displaySourceObjects)
    Write-Host ("{0}_JWPLC_TFT_SOURCE_OBJECT_COUNT={1}" -f $Label, $tftSourceObjects)
    Write-Host ("{0}_TFT_ESPI_SOURCE_OBJECT_COUNT={1}" -f $Label, $tftEspiSourceObjects)
    Write-Host ("{0}_MODBUS_RTU_SOURCE_OBJECT_COUNT={1}" -f $Label, $modbusRtuSourceObjects)

    if (-not $displaySelected -or
        -not $displayPrecompiled -or
        -not $tftSelected -or
        -not $tftPrecompiled -or
        -not $modbusRtuSelected -or
        -not $modbusRtuPrecompiled -or
        $externalTftEspi.Count -ne 0 -or
        $displaySourceObjects -ne 0 -or
        $tftSourceObjects -ne 0 -or
        $tftEspiSourceObjects -ne 0 -or
        $modbusRtuSourceObjects -ne 0) {
        throw ("H3E5_{0}_BUILD_POLICY_FAILED" -f $Label)
    }
}

Write-Host ""
Write-Host "=== H3E5 STATIC PREFLIGHT ==="
Write-Host "H3E5_PROFILE=FULL_RUNTIME_MASTER_SLAVE"
Write-Host "H3E5_MASTER_PERIPHERALS=DISPLAY,ETHERNET,SD,FRAM,RTC,TCA_IO,BUTTONS,RS485,MODBUS_RTU,MODBUS_TCP"
Write-Host "H3E5_SLAVE_PERIPHERALS=DISPLAY,RS485,MODBUS_RTU"
Write-Host "H3E5_PRODUCT_SOURCE_MUTATION=NO"

if ($PreflightOnly) {
    Write-Host "H3E5_PREFLIGHT_COMPILES=NO"
    Write-Host "H3E5_PREFLIGHT_UPLOADS=NO"
    Write-Host "H3E5_PREFLIGHT_REPOSITORY_MUTATION=NO"
    Write-Host "A14_H3E5_FINAL_FULL_RUNTIME_PREFLIGHT=PASS"
    return
}

Write-Host ""
Write-Host "=== H3E5 RUN P5B FULL RUNTIME ENGINE ==="

$p5bArgs = @{
    MasterPort = $MasterPort
    SlavePort = $SlavePort
    TcpRate = $TcpRate
    DurationS = $DurationS
    AllowDirtyCoreCandidate = $true
    AllowMissingModbusRtuArchiveCandidate = $true
}

[object[]]$p5bOutput =
    @(& $p5bGate @p5bArgs *>&1)

[string[]]$p5bLines = @(
    $p5bOutput |
        ForEach-Object {
            $_.ToString()
        }
)

$p5bLines |
    ForEach-Object {
        Write-Host $_
    }

$p5bText =
    $p5bLines -join [Environment]::NewLine

if (-not $p5bText.Contains(
    "A14_P5B_PHYSICAL_MASTER_SLAVE_COMBINED=PASS")) {
    throw "H3E5_P5B_NOT_PASS"
}

Write-Host "H3E5_P5B_FULL_RUNTIME=PASS"

$tempRoot =
    Get-H3E5Marker -Lines $p5bLines -Prefix "P5B_TEMP_ROOT="

$qualificationLog =
    Join-Path $tempRoot "qualification.log"

$masterSnapshot =
    Join-Path $tempRoot "master_final.txt"

$slaveSnapshot =
    Join-Path $tempRoot "slave_final.txt"

$masterBuild =
    Join-Path $tempRoot "build_master"

$slaveBuild =
    Join-Path $tempRoot "build_slave"

$masterCompileLog =
    Join-Path $tempRoot "compile_master.log"

$slaveCompileLog =
    Join-Path $tempRoot "compile_slave.log"

foreach ($required in @(
    $qualificationLog,
    $masterSnapshot,
    $slaveSnapshot,
    $masterBuild,
    $slaveBuild,
    $masterCompileLog,
    $slaveCompileLog
)) {
    if (-not (Test-Path -LiteralPath $required)) {
        throw "H3E5_P5B_ARTIFACT_MISSING=$required"
    }
}

$qualificationText =
    [IO.File]::ReadAllText($qualificationLog)

foreach ($requiredLine in @(
    "TCP_FULL_RUNTIME_PASS=YES",
    "MASTER_RUNTIME_PASS=YES",
    "RTU_MASTER_PASS=YES",
    "RTU_SLAVE_PASS=YES",
    "RTU_CROSS_COUNT_PASS=YES",
    "RTU_SNAPSHOT_TAIL_TOLERANCE=0",
    "RTU_FINAL_SNAPSHOT_MODE=QUIESCED",
    "A14_P5B_AUTOMATED=PASS"
)) {
    if (-not $qualificationText.Contains($requiredLine)) {
        throw "H3E5_QUALIFICATION_MARKER_MISSING=$requiredLine"
    }

    Write-Host "H3E5_QUALIFICATION=$requiredLine"
}

$masterBuildArgs = @{
    CompileLog = $masterCompileLog
    BuildPath = $masterBuild
    Label = "H3E5_MASTER_BUILD"
}

Assert-H3E5Build @masterBuildArgs

$slaveBuildArgs = @{
    CompileLog = $slaveCompileLog
    BuildPath = $slaveBuild
    Label = "H3E5_SLAVE_BUILD"
}

Assert-H3E5Build @slaveBuildArgs

$masterText =
    [IO.File]::ReadAllText($masterSnapshot)

$slaveText =
    [IO.File]::ReadAllText($slaveSnapshot)

$masterExpectations = @(
    @("FULL_RUNTIME_READY", "YES"),
    @("ETH_READY", "YES"),
    @("ETH_LINK", "UP"),
    @("DISPLAY_READY", "YES"),
    @("FRAM_READY", "YES"),
    @("FRAM_FAILS", "0"),
    @("SD_READY", "YES"),
    @("SD_DATALOG_ACTIVE", "YES"),
    @("SD_DATALOG_FAILED_COMMITS", "0"),
    @("RTC_PRESENT", "YES"),
    @("RTC_UNAVAILABLE", "0"),
    @("RTC_STALE", "0"),
    @("IO_INITIALIZED", "YES"),
    @("IO_STALE", "0"),
    @("BUTTONS_READY", "YES"),
    @("BUTTON_NOT_READY", "0"),
    @("SPI_PROBE_FAILS", "0"),
    @("PERIPHERAL_FAILURE_COUNT", "0"),
    @("RTU_READY", "YES"),
    @("RTU_REQUESTS_FAILED", "0"),
    @("RTU_VERIFY_FAILS", "0"),
    @("RTU_CRC_ERRORS", "0"),
    @("RTU_MASTER_TIMEOUTS", "0"),
    @("RTU_LAST_ERROR", "OK")
)

foreach ($pair in $masterExpectations) {
    $assertArgs = @{
        Text = $masterText
        Key = $pair[0]
        Expected = $pair[1]
        Label = "MASTER"
    }

    Assert-H3E5SnapshotValue @assertArgs
}

$slaveExpectations = @(
    @("SLAVE_READY", "YES"),
    @("RTU_READY", "YES"),
    @("RTU_ROLE", "SLAVE"),
    @("RTU_SLAVE_ID", "2"),
    @("RTU_BAUD", "115200"),
    @("DISPLAY_READY", "YES"),
    @("RTU_CRC_ERRORS", "0"),
    @("RTU_EXCEPTIONS_SENT", "0"),
    @("RTU_LAST_ERROR", "OK"),
    @("SLAVE_HR1", "21930")
)

foreach ($pair in $slaveExpectations) {
    $assertArgs = @{
        Text = $slaveText
        Key = $pair[0]
        Expected = $pair[1]
        Label = "SLAVE"
    }

    Assert-H3E5SnapshotValue @assertArgs
}

$tftShaFinal =
    (Get-G2Sha256Path $tftArchive).ToUpperInvariant()

$displayShaFinal =
    (Get-G2Sha256Path $displayArchive).ToUpperInvariant()

$modbusRtuShaFinal =
    (Get-G2Sha256Path $modbusRtuArchive).ToUpperInvariant()

if ($tftShaFinal -ne $expectedTftSha) {
    throw "H3E5_TFT_ARCHIVE_CHANGED"
}

if ($displayShaFinal -ne $expectedDisplaySha) {
    throw "H3E5_DISPLAY_ARCHIVE_CHANGED"
}

if ($modbusRtuShaFinal -ne $modbusRtuSha) {
    throw "H3E5_MODBUS_RTU_ARCHIVE_CHANGED"
}

[string[]]$finalDirty =
    @(Get-G2TrackedDirtyPaths)

[string[]]$finalStaged =
    @(& git -C $script:G2RepoRoot diff --cached --name-only)

[string[]]$normalizedFinalDirty = @(
    $finalDirty |
        ForEach-Object {
            $_.Replace("\", "/")
        } |
        Sort-Object
)

[string[]]$expectedFinalDirty = @(
    $expectedCoreDirty,
    $expectedModbusDirty
) |
    Sort-Object

[object[]]$finalDirtyDiff = @(
    Compare-Object -ReferenceObject $expectedFinalDirty -DifferenceObject $normalizedFinalDirty
)

if ($finalDirtyDiff.Count -ne 0) {
    throw "H3E5_FINAL_DIRTY_SCOPE_INVALID"
}

if ($finalStaged.Count -ne 0) {
    throw "H3E5_FINAL_INDEX_NOT_CLEAN"
}

Write-Host ""
Write-Host "============================================================"
Write-Host " A14 H3E.5 FINAL SUMMARY"
Write-Host "============================================================"
Write-Host "H3E5_TCP_FULL_RUNTIME=PASS"
Write-Host "H3E5_RTU_MASTER=PASS"
Write-Host "H3E5_RTU_SLAVE=PASS"
Write-Host "H3E5_RTU_CROSS_COUNT=PASS"
Write-Host "H3E5_DISPLAY_MASTER=PASS"
Write-Host "H3E5_DISPLAY_SLAVE=PASS"
Write-Host "H3E5_ETHERNET=PASS"
Write-Host "H3E5_MICROSD_BUFFERED_DATALOG=PASS"
Write-Host "H3E5_FRAM=PASS"
Write-Host "H3E5_RTC=PASS"
Write-Host "H3E5_TCA_IO=PASS"
Write-Host "H3E5_BUTTONS=PASS"
Write-Host "H3E5_SPI_OWNERSHIP=PASS"
Write-Host "H3E5_MODBUS_RTU_PRECOMPILED=PASS"
Write-Host "H3E5_MODBUS_RTU_ARCHIVE_SHA256=$modbusRtuShaFinal"
Write-Host "H3E5_EXTERNAL_TFT_ESPI_AT_USER_BUILD=NO"
Write-Host "H3E5_PRODUCT_SOURCE_MUTATION=NO"
Write-Host "H3E5_REPOSITORY_MUTATION=NO"
Write-Host "A14_H3E5_FINAL_FULL_RUNTIME_REGRESSION=PASS"
Write-Host "NEXT=RETURN_OUTPUT_TO_CHAT_FOR_H3E_CLOSURE"
)
                }
        ).Count -gt 0

    $modbusRtuSelected =
        @(
            $lines |
                Where-Object {
                    ($_ -match '^Using library JWPLC_ModbusRTU at version ') -or
                    ($_ -match '^\s*JWPLC_ModbusRTU\s+\S+\s+.+[\\/]JWPLC_ModbusRTU\s*

    $displayPrecompiled =
        @(
            $lines |
                Where-Object {
                    ($_ -match 'Using precompiled library|Usando libreria precompilada|Usando biblioteca precompilada') -and
                    ($_ -match 'JWPLC_Display')
                }
        ).Count -gt 0

    $tftPrecompiled =
        @(
            $lines |
                Where-Object {
                    ($_ -match 'Using precompiled library|Usando libreria precompilada|Usando biblioteca precompilada') -and
                    ($_ -match 'JWPLC_TFT')
                }
        ).Count -gt 0

    $modbusRtuPrecompiled =
        @(
            $lines |
                Where-Object {
                    ($_ -match 'Using precompiled library|Usando libreria precompilada|Usando biblioteca precompilada') -and
                    ($_ -match 'JWPLC_ModbusRTU')
                }
        ).Count -gt 0

    [string[]]$externalTftEspi = @(
        $lines |
            Where-Object {
                $_ -match '^Using library TFT_eSPI at version '
            }
    )

    [int]$tftSourceObjects = @(
        Get-ChildItem -LiteralPath $BuildPath -Recurse -File -ErrorAction SilentlyContinue |
            Where-Object {
                $_.Name -eq "JWPLC_TFT.cpp.o"
            }
    ).Count

    [int]$tftEspiSourceObjects = @(
        Get-ChildItem -LiteralPath $BuildPath -Recurse -File -ErrorAction SilentlyContinue |
            Where-Object {
                $_.Name -eq "TFT_eSPI.cpp.o"
            }
    ).Count

    [int]$displaySourceObjects = @(
        Get-ChildItem -LiteralPath $BuildPath -Recurse -File -ErrorAction SilentlyContinue |
            Where-Object {
                $_.FullName -match '[\\/]libraries[\\/]JWPLC_Display[\\/].+\.cpp\.o$'
            }
    ).Count

    [int]$modbusRtuSourceObjects = @(
        Get-ChildItem -LiteralPath $BuildPath -Recurse -File -ErrorAction SilentlyContinue |
            Where-Object {
                $_.Name -eq "JWPLC_ModbusRTU.cpp.o"
            }
    ).Count

    Write-Host ("{0}_DISPLAY_SELECTED={1}" -f $Label, $displaySelected)
    Write-Host ("{0}_DISPLAY_PRECOMPILED={1}" -f $Label, $displayPrecompiled)
    Write-Host ("{0}_JWPLC_TFT_SELECTED={1}" -f $Label, $tftSelected)
    Write-Host ("{0}_JWPLC_TFT_PRECOMPILED={1}" -f $Label, $tftPrecompiled)
    Write-Host ("{0}_MODBUS_RTU_SELECTED={1}" -f $Label, $modbusRtuSelected)
    Write-Host ("{0}_MODBUS_RTU_PRECOMPILED={1}" -f $Label, $modbusRtuPrecompiled)
    Write-Host ("{0}_EXTERNAL_TFT_ESPI_SELECTION_COUNT={1}" -f $Label, $externalTftEspi.Count)
    Write-Host ("{0}_DISPLAY_SOURCE_OBJECT_COUNT={1}" -f $Label, $displaySourceObjects)
    Write-Host ("{0}_JWPLC_TFT_SOURCE_OBJECT_COUNT={1}" -f $Label, $tftSourceObjects)
    Write-Host ("{0}_TFT_ESPI_SOURCE_OBJECT_COUNT={1}" -f $Label, $tftEspiSourceObjects)
    Write-Host ("{0}_MODBUS_RTU_SOURCE_OBJECT_COUNT={1}" -f $Label, $modbusRtuSourceObjects)

    if (-not $displaySelected -or
        -not $displayPrecompiled -or
        -not $tftSelected -or
        -not $tftPrecompiled -or
        -not $modbusRtuSelected -or
        -not $modbusRtuPrecompiled -or
        $externalTftEspi.Count -ne 0 -or
        $displaySourceObjects -ne 0 -or
        $tftSourceObjects -ne 0 -or
        $tftEspiSourceObjects -ne 0 -or
        $modbusRtuSourceObjects -ne 0) {
        throw ("H3E5_{0}_BUILD_POLICY_FAILED" -f $Label)
    }
}

Write-Host ""
Write-Host "=== H3E5 STATIC PREFLIGHT ==="
Write-Host "H3E5_PROFILE=FULL_RUNTIME_MASTER_SLAVE"
Write-Host "H3E5_MASTER_PERIPHERALS=DISPLAY,ETHERNET,SD,FRAM,RTC,TCA_IO,BUTTONS,RS485,MODBUS_RTU,MODBUS_TCP"
Write-Host "H3E5_SLAVE_PERIPHERALS=DISPLAY,RS485,MODBUS_RTU"
Write-Host "H3E5_PRODUCT_SOURCE_MUTATION=NO"

if ($PreflightOnly) {
    Write-Host "H3E5_PREFLIGHT_COMPILES=NO"
    Write-Host "H3E5_PREFLIGHT_UPLOADS=NO"
    Write-Host "H3E5_PREFLIGHT_REPOSITORY_MUTATION=NO"
    Write-Host "A14_H3E5_FINAL_FULL_RUNTIME_PREFLIGHT=PASS"
    return
}

Write-Host ""
Write-Host "=== H3E5 RUN P5B FULL RUNTIME ENGINE ==="

$p5bArgs = @{
    MasterPort = $MasterPort
    SlavePort = $SlavePort
    TcpRate = $TcpRate
    DurationS = $DurationS
    AllowDirtyCoreCandidate = $true
    AllowMissingModbusRtuArchiveCandidate = $true
}

[object[]]$p5bOutput =
    @(& $p5bGate @p5bArgs *>&1)

[string[]]$p5bLines = @(
    $p5bOutput |
        ForEach-Object {
            $_.ToString()
        }
)

$p5bLines |
    ForEach-Object {
        Write-Host $_
    }

$p5bText =
    $p5bLines -join [Environment]::NewLine

if (-not $p5bText.Contains(
    "A14_P5B_PHYSICAL_MASTER_SLAVE_COMBINED=PASS")) {
    throw "H3E5_P5B_NOT_PASS"
}

Write-Host "H3E5_P5B_FULL_RUNTIME=PASS"

$tempRoot =
    Get-H3E5Marker -Lines $p5bLines -Prefix "P5B_TEMP_ROOT="

$qualificationLog =
    Join-Path $tempRoot "qualification.log"

$masterSnapshot =
    Join-Path $tempRoot "master_final.txt"

$slaveSnapshot =
    Join-Path $tempRoot "slave_final.txt"

$masterBuild =
    Join-Path $tempRoot "build_master"

$slaveBuild =
    Join-Path $tempRoot "build_slave"

$masterCompileLog =
    Join-Path $tempRoot "compile_master.log"

$slaveCompileLog =
    Join-Path $tempRoot "compile_slave.log"

foreach ($required in @(
    $qualificationLog,
    $masterSnapshot,
    $slaveSnapshot,
    $masterBuild,
    $slaveBuild,
    $masterCompileLog,
    $slaveCompileLog
)) {
    if (-not (Test-Path -LiteralPath $required)) {
        throw "H3E5_P5B_ARTIFACT_MISSING=$required"
    }
}

$qualificationText =
    [IO.File]::ReadAllText($qualificationLog)

foreach ($requiredLine in @(
    "TCP_FULL_RUNTIME_PASS=YES",
    "MASTER_RUNTIME_PASS=YES",
    "RTU_MASTER_PASS=YES",
    "RTU_SLAVE_PASS=YES",
    "RTU_CROSS_COUNT_PASS=YES",
    "RTU_SNAPSHOT_TAIL_TOLERANCE=0",
    "RTU_FINAL_SNAPSHOT_MODE=QUIESCED",
    "A14_P5B_AUTOMATED=PASS"
)) {
    if (-not $qualificationText.Contains($requiredLine)) {
        throw "H3E5_QUALIFICATION_MARKER_MISSING=$requiredLine"
    }

    Write-Host "H3E5_QUALIFICATION=$requiredLine"
}

$masterBuildArgs = @{
    CompileLog = $masterCompileLog
    BuildPath = $masterBuild
    Label = "H3E5_MASTER_BUILD"
}

Assert-H3E5Build @masterBuildArgs

$slaveBuildArgs = @{
    CompileLog = $slaveCompileLog
    BuildPath = $slaveBuild
    Label = "H3E5_SLAVE_BUILD"
}

Assert-H3E5Build @slaveBuildArgs

$masterText =
    [IO.File]::ReadAllText($masterSnapshot)

$slaveText =
    [IO.File]::ReadAllText($slaveSnapshot)

$masterExpectations = @(
    @("FULL_RUNTIME_READY", "YES"),
    @("ETH_READY", "YES"),
    @("ETH_LINK", "UP"),
    @("DISPLAY_READY", "YES"),
    @("FRAM_READY", "YES"),
    @("FRAM_FAILS", "0"),
    @("SD_READY", "YES"),
    @("SD_DATALOG_ACTIVE", "YES"),
    @("SD_DATALOG_FAILED_COMMITS", "0"),
    @("RTC_PRESENT", "YES"),
    @("RTC_UNAVAILABLE", "0"),
    @("RTC_STALE", "0"),
    @("IO_INITIALIZED", "YES"),
    @("IO_STALE", "0"),
    @("BUTTONS_READY", "YES"),
    @("BUTTON_NOT_READY", "0"),
    @("SPI_PROBE_FAILS", "0"),
    @("PERIPHERAL_FAILURE_COUNT", "0"),
    @("RTU_READY", "YES"),
    @("RTU_REQUESTS_FAILED", "0"),
    @("RTU_VERIFY_FAILS", "0"),
    @("RTU_CRC_ERRORS", "0"),
    @("RTU_MASTER_TIMEOUTS", "0"),
    @("RTU_LAST_ERROR", "OK")
)

foreach ($pair in $masterExpectations) {
    $assertArgs = @{
        Text = $masterText
        Key = $pair[0]
        Expected = $pair[1]
        Label = "MASTER"
    }

    Assert-H3E5SnapshotValue @assertArgs
}

$slaveExpectations = @(
    @("SLAVE_READY", "YES"),
    @("RTU_READY", "YES"),
    @("RTU_ROLE", "SLAVE"),
    @("RTU_SLAVE_ID", "2"),
    @("RTU_BAUD", "115200"),
    @("DISPLAY_READY", "YES"),
    @("RTU_CRC_ERRORS", "0"),
    @("RTU_EXCEPTIONS_SENT", "0"),
    @("RTU_LAST_ERROR", "OK"),
    @("SLAVE_HR1", "21930")
)

foreach ($pair in $slaveExpectations) {
    $assertArgs = @{
        Text = $slaveText
        Key = $pair[0]
        Expected = $pair[1]
        Label = "SLAVE"
    }

    Assert-H3E5SnapshotValue @assertArgs
}

$tftShaFinal =
    (Get-G2Sha256Path $tftArchive).ToUpperInvariant()

$displayShaFinal =
    (Get-G2Sha256Path $displayArchive).ToUpperInvariant()

$modbusRtuShaFinal =
    (Get-G2Sha256Path $modbusRtuArchive).ToUpperInvariant()

if ($tftShaFinal -ne $expectedTftSha) {
    throw "H3E5_TFT_ARCHIVE_CHANGED"
}

if ($displayShaFinal -ne $expectedDisplaySha) {
    throw "H3E5_DISPLAY_ARCHIVE_CHANGED"
}

if ($modbusRtuShaFinal -ne $modbusRtuSha) {
    throw "H3E5_MODBUS_RTU_ARCHIVE_CHANGED"
}

[string[]]$finalDirty =
    @(Get-G2TrackedDirtyPaths)

[string[]]$finalStaged =
    @(& git -C $script:G2RepoRoot diff --cached --name-only)

[string[]]$normalizedFinalDirty = @(
    $finalDirty |
        ForEach-Object {
            $_.Replace("\", "/")
        } |
        Sort-Object
)

[string[]]$expectedFinalDirty = @(
    $expectedCoreDirty,
    $expectedModbusDirty
) |
    Sort-Object

[object[]]$finalDirtyDiff = @(
    Compare-Object -ReferenceObject $expectedFinalDirty -DifferenceObject $normalizedFinalDirty
)

if ($finalDirtyDiff.Count -ne 0) {
    throw "H3E5_FINAL_DIRTY_SCOPE_INVALID"
}

if ($finalStaged.Count -ne 0) {
    throw "H3E5_FINAL_INDEX_NOT_CLEAN"
}

Write-Host ""
Write-Host "============================================================"
Write-Host " A14 H3E.5 FINAL SUMMARY"
Write-Host "============================================================"
Write-Host "H3E5_TCP_FULL_RUNTIME=PASS"
Write-Host "H3E5_RTU_MASTER=PASS"
Write-Host "H3E5_RTU_SLAVE=PASS"
Write-Host "H3E5_RTU_CROSS_COUNT=PASS"
Write-Host "H3E5_DISPLAY_MASTER=PASS"
Write-Host "H3E5_DISPLAY_SLAVE=PASS"
Write-Host "H3E5_ETHERNET=PASS"
Write-Host "H3E5_MICROSD_BUFFERED_DATALOG=PASS"
Write-Host "H3E5_FRAM=PASS"
Write-Host "H3E5_RTC=PASS"
Write-Host "H3E5_TCA_IO=PASS"
Write-Host "H3E5_BUTTONS=PASS"
Write-Host "H3E5_SPI_OWNERSHIP=PASS"
Write-Host "H3E5_MODBUS_RTU_PRECOMPILED=PASS"
Write-Host "H3E5_MODBUS_RTU_ARCHIVE_SHA256=$modbusRtuShaFinal"
Write-Host "H3E5_EXTERNAL_TFT_ESPI_AT_USER_BUILD=NO"
Write-Host "H3E5_PRODUCT_SOURCE_MUTATION=NO"
Write-Host "H3E5_REPOSITORY_MUTATION=NO"
Write-Host "A14_H3E5_FINAL_FULL_RUNTIME_REGRESSION=PASS"
Write-Host "NEXT=RETURN_OUTPUT_TO_CHAT_FOR_H3E_CLOSURE"
)
                }
        ).Count -gt 0

    $displayPrecompiled =
        @(
            $lines |
                Where-Object {
                    ($_ -match 'Using precompiled library|Usando libreria precompilada|Usando biblioteca precompilada') -and
                    ($_ -match 'JWPLC_Display')
                }
        ).Count -gt 0

    $tftPrecompiled =
        @(
            $lines |
                Where-Object {
                    ($_ -match 'Using precompiled library|Usando libreria precompilada|Usando biblioteca precompilada') -and
                    ($_ -match 'JWPLC_TFT')
                }
        ).Count -gt 0

    $modbusRtuPrecompiled =
        @(
            $lines |
                Where-Object {
                    ($_ -match 'Using precompiled library|Usando libreria precompilada|Usando biblioteca precompilada') -and
                    ($_ -match 'JWPLC_ModbusRTU')
                }
        ).Count -gt 0

    [string[]]$externalTftEspi = @(
        $lines |
            Where-Object {
                $_ -match '^Using library TFT_eSPI at version '
            }
    )

    [int]$tftSourceObjects = @(
        Get-ChildItem -LiteralPath $BuildPath -Recurse -File -ErrorAction SilentlyContinue |
            Where-Object {
                $_.Name -eq "JWPLC_TFT.cpp.o"
            }
    ).Count

    [int]$tftEspiSourceObjects = @(
        Get-ChildItem -LiteralPath $BuildPath -Recurse -File -ErrorAction SilentlyContinue |
            Where-Object {
                $_.Name -eq "TFT_eSPI.cpp.o"
            }
    ).Count

    [int]$displaySourceObjects = @(
        Get-ChildItem -LiteralPath $BuildPath -Recurse -File -ErrorAction SilentlyContinue |
            Where-Object {
                $_.FullName -match '[\\/]libraries[\\/]JWPLC_Display[\\/].+\.cpp\.o$'
            }
    ).Count

    [int]$modbusRtuSourceObjects = @(
        Get-ChildItem -LiteralPath $BuildPath -Recurse -File -ErrorAction SilentlyContinue |
            Where-Object {
                $_.Name -eq "JWPLC_ModbusRTU.cpp.o"
            }
    ).Count

    Write-Host ("{0}_DISPLAY_SELECTED={1}" -f $Label, $displaySelected)
    Write-Host ("{0}_DISPLAY_PRECOMPILED={1}" -f $Label, $displayPrecompiled)
    Write-Host ("{0}_JWPLC_TFT_SELECTED={1}" -f $Label, $tftSelected)
    Write-Host ("{0}_JWPLC_TFT_PRECOMPILED={1}" -f $Label, $tftPrecompiled)
    Write-Host ("{0}_MODBUS_RTU_SELECTED={1}" -f $Label, $modbusRtuSelected)
    Write-Host ("{0}_MODBUS_RTU_PRECOMPILED={1}" -f $Label, $modbusRtuPrecompiled)
    Write-Host ("{0}_EXTERNAL_TFT_ESPI_SELECTION_COUNT={1}" -f $Label, $externalTftEspi.Count)
    Write-Host ("{0}_DISPLAY_SOURCE_OBJECT_COUNT={1}" -f $Label, $displaySourceObjects)
    Write-Host ("{0}_JWPLC_TFT_SOURCE_OBJECT_COUNT={1}" -f $Label, $tftSourceObjects)
    Write-Host ("{0}_TFT_ESPI_SOURCE_OBJECT_COUNT={1}" -f $Label, $tftEspiSourceObjects)
    Write-Host ("{0}_MODBUS_RTU_SOURCE_OBJECT_COUNT={1}" -f $Label, $modbusRtuSourceObjects)

    if (-not $displaySelected -or
        -not $displayPrecompiled -or
        -not $tftSelected -or
        -not $tftPrecompiled -or
        -not $modbusRtuSelected -or
        -not $modbusRtuPrecompiled -or
        $externalTftEspi.Count -ne 0 -or
        $displaySourceObjects -ne 0 -or
        $tftSourceObjects -ne 0 -or
        $tftEspiSourceObjects -ne 0 -or
        $modbusRtuSourceObjects -ne 0) {
        throw ("H3E5_{0}_BUILD_POLICY_FAILED" -f $Label)
    }
}

Write-Host ""
Write-Host "=== H3E5 STATIC PREFLIGHT ==="
Write-Host "H3E5_PROFILE=FULL_RUNTIME_MASTER_SLAVE"
Write-Host "H3E5_MASTER_PERIPHERALS=DISPLAY,ETHERNET,SD,FRAM,RTC,TCA_IO,BUTTONS,RS485,MODBUS_RTU,MODBUS_TCP"
Write-Host "H3E5_SLAVE_PERIPHERALS=DISPLAY,RS485,MODBUS_RTU"
Write-Host "H3E5_PRODUCT_SOURCE_MUTATION=NO"

if ($PreflightOnly) {
    Write-Host "H3E5_PREFLIGHT_COMPILES=NO"
    Write-Host "H3E5_PREFLIGHT_UPLOADS=NO"
    Write-Host "H3E5_PREFLIGHT_REPOSITORY_MUTATION=NO"
    Write-Host "A14_H3E5_FINAL_FULL_RUNTIME_PREFLIGHT=PASS"
    return
}

Write-Host ""
Write-Host "=== H3E5 RUN P5B FULL RUNTIME ENGINE ==="

$p5bArgs = @{
    MasterPort = $MasterPort
    SlavePort = $SlavePort
    TcpRate = $TcpRate
    DurationS = $DurationS
    AllowDirtyCoreCandidate = $true
    AllowMissingModbusRtuArchiveCandidate = $true
}

[object[]]$p5bOutput =
    @(& $p5bGate @p5bArgs *>&1)

[string[]]$p5bLines = @(
    $p5bOutput |
        ForEach-Object {
            $_.ToString()
        }
)

$p5bLines |
    ForEach-Object {
        Write-Host $_
    }

$p5bText =
    $p5bLines -join [Environment]::NewLine

if (-not $p5bText.Contains(
    "A14_P5B_PHYSICAL_MASTER_SLAVE_COMBINED=PASS")) {
    throw "H3E5_P5B_NOT_PASS"
}

Write-Host "H3E5_P5B_FULL_RUNTIME=PASS"

$tempRoot =
    Get-H3E5Marker -Lines $p5bLines -Prefix "P5B_TEMP_ROOT="

$qualificationLog =
    Join-Path $tempRoot "qualification.log"

$masterSnapshot =
    Join-Path $tempRoot "master_final.txt"

$slaveSnapshot =
    Join-Path $tempRoot "slave_final.txt"

$masterBuild =
    Join-Path $tempRoot "build_master"

$slaveBuild =
    Join-Path $tempRoot "build_slave"

$masterCompileLog =
    Join-Path $tempRoot "compile_master.log"

$slaveCompileLog =
    Join-Path $tempRoot "compile_slave.log"

foreach ($required in @(
    $qualificationLog,
    $masterSnapshot,
    $slaveSnapshot,
    $masterBuild,
    $slaveBuild,
    $masterCompileLog,
    $slaveCompileLog
)) {
    if (-not (Test-Path -LiteralPath $required)) {
        throw "H3E5_P5B_ARTIFACT_MISSING=$required"
    }
}

$qualificationText =
    [IO.File]::ReadAllText($qualificationLog)

foreach ($requiredLine in @(
    "TCP_FULL_RUNTIME_PASS=YES",
    "MASTER_RUNTIME_PASS=YES",
    "RTU_MASTER_PASS=YES",
    "RTU_SLAVE_PASS=YES",
    "RTU_CROSS_COUNT_PASS=YES",
    "RTU_SNAPSHOT_TAIL_TOLERANCE=0",
    "RTU_FINAL_SNAPSHOT_MODE=QUIESCED",
    "A14_P5B_AUTOMATED=PASS"
)) {
    if (-not $qualificationText.Contains($requiredLine)) {
        throw "H3E5_QUALIFICATION_MARKER_MISSING=$requiredLine"
    }

    Write-Host "H3E5_QUALIFICATION=$requiredLine"
}

$masterBuildArgs = @{
    CompileLog = $masterCompileLog
    BuildPath = $masterBuild
    Label = "H3E5_MASTER_BUILD"
}

Assert-H3E5Build @masterBuildArgs

$slaveBuildArgs = @{
    CompileLog = $slaveCompileLog
    BuildPath = $slaveBuild
    Label = "H3E5_SLAVE_BUILD"
}

Assert-H3E5Build @slaveBuildArgs

$masterText =
    [IO.File]::ReadAllText($masterSnapshot)

$slaveText =
    [IO.File]::ReadAllText($slaveSnapshot)

$masterExpectations = @(
    @("FULL_RUNTIME_READY", "YES"),
    @("ETH_READY", "YES"),
    @("ETH_LINK", "UP"),
    @("DISPLAY_READY", "YES"),
    @("FRAM_READY", "YES"),
    @("FRAM_FAILS", "0"),
    @("SD_READY", "YES"),
    @("SD_DATALOG_ACTIVE", "YES"),
    @("SD_DATALOG_FAILED_COMMITS", "0"),
    @("RTC_PRESENT", "YES"),
    @("RTC_UNAVAILABLE", "0"),
    @("RTC_STALE", "0"),
    @("IO_INITIALIZED", "YES"),
    @("IO_STALE", "0"),
    @("BUTTONS_READY", "YES"),
    @("BUTTON_NOT_READY", "0"),
    @("SPI_PROBE_FAILS", "0"),
    @("PERIPHERAL_FAILURE_COUNT", "0"),
    @("RTU_READY", "YES"),
    @("RTU_REQUESTS_FAILED", "0"),
    @("RTU_VERIFY_FAILS", "0"),
    @("RTU_CRC_ERRORS", "0"),
    @("RTU_MASTER_TIMEOUTS", "0"),
    @("RTU_LAST_ERROR", "OK")
)

foreach ($pair in $masterExpectations) {
    $assertArgs = @{
        Text = $masterText
        Key = $pair[0]
        Expected = $pair[1]
        Label = "MASTER"
    }

    Assert-H3E5SnapshotValue @assertArgs
}

$slaveExpectations = @(
    @("SLAVE_READY", "YES"),
    @("RTU_READY", "YES"),
    @("RTU_ROLE", "SLAVE"),
    @("RTU_SLAVE_ID", "2"),
    @("RTU_BAUD", "115200"),
    @("DISPLAY_READY", "YES"),
    @("RTU_CRC_ERRORS", "0"),
    @("RTU_EXCEPTIONS_SENT", "0"),
    @("RTU_LAST_ERROR", "OK"),
    @("SLAVE_HR1", "21930")
)

foreach ($pair in $slaveExpectations) {
    $assertArgs = @{
        Text = $slaveText
        Key = $pair[0]
        Expected = $pair[1]
        Label = "SLAVE"
    }

    Assert-H3E5SnapshotValue @assertArgs
}

$tftShaFinal =
    (Get-G2Sha256Path $tftArchive).ToUpperInvariant()

$displayShaFinal =
    (Get-G2Sha256Path $displayArchive).ToUpperInvariant()

$modbusRtuShaFinal =
    (Get-G2Sha256Path $modbusRtuArchive).ToUpperInvariant()

if ($tftShaFinal -ne $expectedTftSha) {
    throw "H3E5_TFT_ARCHIVE_CHANGED"
}

if ($displayShaFinal -ne $expectedDisplaySha) {
    throw "H3E5_DISPLAY_ARCHIVE_CHANGED"
}

if ($modbusRtuShaFinal -ne $modbusRtuSha) {
    throw "H3E5_MODBUS_RTU_ARCHIVE_CHANGED"
}

[string[]]$finalDirty =
    @(Get-G2TrackedDirtyPaths)

[string[]]$finalStaged =
    @(& git -C $script:G2RepoRoot diff --cached --name-only)

[string[]]$normalizedFinalDirty = @(
    $finalDirty |
        ForEach-Object {
            $_.Replace("\", "/")
        } |
        Sort-Object
)

[string[]]$expectedFinalDirty = @(
    $expectedCoreDirty,
    $expectedModbusDirty
) |
    Sort-Object

[object[]]$finalDirtyDiff = @(
    Compare-Object -ReferenceObject $expectedFinalDirty -DifferenceObject $normalizedFinalDirty
)

if ($finalDirtyDiff.Count -ne 0) {
    throw "H3E5_FINAL_DIRTY_SCOPE_INVALID"
}

if ($finalStaged.Count -ne 0) {
    throw "H3E5_FINAL_INDEX_NOT_CLEAN"
}

Write-Host ""
Write-Host "============================================================"
Write-Host " A14 H3E.5 FINAL SUMMARY"
Write-Host "============================================================"
Write-Host "H3E5_TCP_FULL_RUNTIME=PASS"
Write-Host "H3E5_RTU_MASTER=PASS"
Write-Host "H3E5_RTU_SLAVE=PASS"
Write-Host "H3E5_RTU_CROSS_COUNT=PASS"
Write-Host "H3E5_DISPLAY_MASTER=PASS"
Write-Host "H3E5_DISPLAY_SLAVE=PASS"
Write-Host "H3E5_ETHERNET=PASS"
Write-Host "H3E5_MICROSD_BUFFERED_DATALOG=PASS"
Write-Host "H3E5_FRAM=PASS"
Write-Host "H3E5_RTC=PASS"
Write-Host "H3E5_TCA_IO=PASS"
Write-Host "H3E5_BUTTONS=PASS"
Write-Host "H3E5_SPI_OWNERSHIP=PASS"
Write-Host "H3E5_MODBUS_RTU_PRECOMPILED=PASS"
Write-Host "H3E5_MODBUS_RTU_ARCHIVE_SHA256=$modbusRtuShaFinal"
Write-Host "H3E5_EXTERNAL_TFT_ESPI_AT_USER_BUILD=NO"
Write-Host "H3E5_PRODUCT_SOURCE_MUTATION=NO"
Write-Host "H3E5_REPOSITORY_MUTATION=NO"
Write-Host "A14_H3E5_FINAL_FULL_RUNTIME_REGRESSION=PASS"
Write-Host "NEXT=RETURN_OUTPUT_TO_CHAT_FOR_H3E_CLOSURE"
)
            }
    )

    [int]$tftSourceObjects = @(
        Get-ChildItem -LiteralPath $BuildPath -Recurse -File -ErrorAction SilentlyContinue |
            Where-Object {
                $_.Name -eq "JWPLC_TFT.cpp.o"
            }
    ).Count

    [int]$tftEspiSourceObjects = @(
        Get-ChildItem -LiteralPath $BuildPath -Recurse -File -ErrorAction SilentlyContinue |
            Where-Object {
                $_.Name -eq "TFT_eSPI.cpp.o"
            }
    ).Count

    [int]$displaySourceObjects = @(
        Get-ChildItem -LiteralPath $BuildPath -Recurse -File -ErrorAction SilentlyContinue |
            Where-Object {
                $_.FullName -match '[\\/]libraries[\\/]JWPLC_Display[\\/].+\.cpp\.o$'
            }
    ).Count

    [int]$modbusRtuSourceObjects = @(
        Get-ChildItem -LiteralPath $BuildPath -Recurse -File -ErrorAction SilentlyContinue |
            Where-Object {
                $_.Name -eq "JWPLC_ModbusRTU.cpp.o"
            }
    ).Count

    Write-Host ("{0}_DISPLAY_SELECTED={1}" -f $Label, $displaySelected)
    Write-Host ("{0}_DISPLAY_PRECOMPILED={1}" -f $Label, $displayPrecompiled)
    Write-Host ("{0}_JWPLC_TFT_SELECTED={1}" -f $Label, $tftSelected)
    Write-Host ("{0}_JWPLC_TFT_PRECOMPILED={1}" -f $Label, $tftPrecompiled)
    Write-Host ("{0}_MODBUS_RTU_SELECTED={1}" -f $Label, $modbusRtuSelected)
    Write-Host ("{0}_MODBUS_RTU_PRECOMPILED={1}" -f $Label, $modbusRtuPrecompiled)
    Write-Host ("{0}_EXTERNAL_TFT_ESPI_SELECTION_COUNT={1}" -f $Label, $externalTftEspi.Count)
    Write-Host ("{0}_DISPLAY_SOURCE_OBJECT_COUNT={1}" -f $Label, $displaySourceObjects)
    Write-Host ("{0}_JWPLC_TFT_SOURCE_OBJECT_COUNT={1}" -f $Label, $tftSourceObjects)
    Write-Host ("{0}_TFT_ESPI_SOURCE_OBJECT_COUNT={1}" -f $Label, $tftEspiSourceObjects)
    Write-Host ("{0}_MODBUS_RTU_SOURCE_OBJECT_COUNT={1}" -f $Label, $modbusRtuSourceObjects)

    if (-not $displaySelected -or
        -not $displayPrecompiled -or
        -not $tftSelected -or
        -not $tftPrecompiled -or
        -not $modbusRtuSelected -or
        -not $modbusRtuPrecompiled -or
        $externalTftEspi.Count -ne 0 -or
        $displaySourceObjects -ne 0 -or
        $tftSourceObjects -ne 0 -or
        $tftEspiSourceObjects -ne 0 -or
        $modbusRtuSourceObjects -ne 0) {
        throw ("H3E5_{0}_BUILD_POLICY_FAILED" -f $Label)
    }
}

Write-Host ""
Write-Host "=== H3E5 STATIC PREFLIGHT ==="
Write-Host "H3E5_PROFILE=FULL_RUNTIME_MASTER_SLAVE"
Write-Host "H3E5_MASTER_PERIPHERALS=DISPLAY,ETHERNET,SD,FRAM,RTC,TCA_IO,BUTTONS,RS485,MODBUS_RTU,MODBUS_TCP"
Write-Host "H3E5_SLAVE_PERIPHERALS=DISPLAY,RS485,MODBUS_RTU"
Write-Host "H3E5_PRODUCT_SOURCE_MUTATION=NO"

if ($PreflightOnly) {
    Write-Host "H3E5_PREFLIGHT_COMPILES=NO"
    Write-Host "H3E5_PREFLIGHT_UPLOADS=NO"
    Write-Host "H3E5_PREFLIGHT_REPOSITORY_MUTATION=NO"
    Write-Host "A14_H3E5_FINAL_FULL_RUNTIME_PREFLIGHT=PASS"
    return
}

Write-Host ""
Write-Host "=== H3E5 RUN P5B FULL RUNTIME ENGINE ==="

$p5bArgs = @{
    MasterPort = $MasterPort
    SlavePort = $SlavePort
    TcpRate = $TcpRate
    DurationS = $DurationS
    AllowDirtyCoreCandidate = $true
    AllowMissingModbusRtuArchiveCandidate = $true
}

[object[]]$p5bOutput =
    @(& $p5bGate @p5bArgs *>&1)

[string[]]$p5bLines = @(
    $p5bOutput |
        ForEach-Object {
            $_.ToString()
        }
)

$p5bLines |
    ForEach-Object {
        Write-Host $_
    }

$p5bText =
    $p5bLines -join [Environment]::NewLine

if (-not $p5bText.Contains(
    "A14_P5B_PHYSICAL_MASTER_SLAVE_COMBINED=PASS")) {
    throw "H3E5_P5B_NOT_PASS"
}

Write-Host "H3E5_P5B_FULL_RUNTIME=PASS"

$tempRoot =
    Get-H3E5Marker -Lines $p5bLines -Prefix "P5B_TEMP_ROOT="

$qualificationLog =
    Join-Path $tempRoot "qualification.log"

$masterSnapshot =
    Join-Path $tempRoot "master_final.txt"

$slaveSnapshot =
    Join-Path $tempRoot "slave_final.txt"

$masterBuild =
    Join-Path $tempRoot "build_master"

$slaveBuild =
    Join-Path $tempRoot "build_slave"

$masterCompileLog =
    Join-Path $tempRoot "compile_master.log"

$slaveCompileLog =
    Join-Path $tempRoot "compile_slave.log"

foreach ($required in @(
    $qualificationLog,
    $masterSnapshot,
    $slaveSnapshot,
    $masterBuild,
    $slaveBuild,
    $masterCompileLog,
    $slaveCompileLog
)) {
    if (-not (Test-Path -LiteralPath $required)) {
        throw "H3E5_P5B_ARTIFACT_MISSING=$required"
    }
}

$qualificationText =
    [IO.File]::ReadAllText($qualificationLog)

foreach ($requiredLine in @(
    "TCP_FULL_RUNTIME_PASS=YES",
    "MASTER_RUNTIME_PASS=YES",
    "RTU_MASTER_PASS=YES",
    "RTU_SLAVE_PASS=YES",
    "RTU_CROSS_COUNT_PASS=YES",
    "RTU_SNAPSHOT_TAIL_TOLERANCE=0",
    "RTU_FINAL_SNAPSHOT_MODE=QUIESCED",
    "A14_P5B_AUTOMATED=PASS"
)) {
    if (-not $qualificationText.Contains($requiredLine)) {
        throw "H3E5_QUALIFICATION_MARKER_MISSING=$requiredLine"
    }

    Write-Host "H3E5_QUALIFICATION=$requiredLine"
}

$masterBuildArgs = @{
    CompileLog = $masterCompileLog
    BuildPath = $masterBuild
    Label = "H3E5_MASTER_BUILD"
}

Assert-H3E5Build @masterBuildArgs

$slaveBuildArgs = @{
    CompileLog = $slaveCompileLog
    BuildPath = $slaveBuild
    Label = "H3E5_SLAVE_BUILD"
}

Assert-H3E5Build @slaveBuildArgs

$masterText =
    [IO.File]::ReadAllText($masterSnapshot)

$slaveText =
    [IO.File]::ReadAllText($slaveSnapshot)

$masterExpectations = @(
    @("FULL_RUNTIME_READY", "YES"),
    @("ETH_READY", "YES"),
    @("ETH_LINK", "UP"),
    @("DISPLAY_READY", "YES"),
    @("FRAM_READY", "YES"),
    @("FRAM_FAILS", "0"),
    @("SD_READY", "YES"),
    @("SD_DATALOG_ACTIVE", "YES"),
    @("SD_DATALOG_FAILED_COMMITS", "0"),
    @("RTC_PRESENT", "YES"),
    @("RTC_UNAVAILABLE", "0"),
    @("RTC_STALE", "0"),
    @("IO_INITIALIZED", "YES"),
    @("IO_STALE", "0"),
    @("BUTTONS_READY", "YES"),
    @("BUTTON_NOT_READY", "0"),
    @("SPI_PROBE_FAILS", "0"),
    @("PERIPHERAL_FAILURE_COUNT", "0"),
    @("RTU_READY", "YES"),
    @("RTU_REQUESTS_FAILED", "0"),
    @("RTU_VERIFY_FAILS", "0"),
    @("RTU_CRC_ERRORS", "0"),
    @("RTU_MASTER_TIMEOUTS", "0"),
    @("RTU_LAST_ERROR", "OK")
)

foreach ($pair in $masterExpectations) {
    $assertArgs = @{
        Text = $masterText
        Key = $pair[0]
        Expected = $pair[1]
        Label = "MASTER"
    }

    Assert-H3E5SnapshotValue @assertArgs
}

$slaveExpectations = @(
    @("SLAVE_READY", "YES"),
    @("RTU_READY", "YES"),
    @("RTU_ROLE", "SLAVE"),
    @("RTU_SLAVE_ID", "2"),
    @("RTU_BAUD", "115200"),
    @("DISPLAY_READY", "YES"),
    @("RTU_CRC_ERRORS", "0"),
    @("RTU_EXCEPTIONS_SENT", "0"),
    @("RTU_LAST_ERROR", "OK"),
    @("SLAVE_HR1", "21930")
)

foreach ($pair in $slaveExpectations) {
    $assertArgs = @{
        Text = $slaveText
        Key = $pair[0]
        Expected = $pair[1]
        Label = "SLAVE"
    }

    Assert-H3E5SnapshotValue @assertArgs
}

$tftShaFinal =
    (Get-G2Sha256Path $tftArchive).ToUpperInvariant()

$displayShaFinal =
    (Get-G2Sha256Path $displayArchive).ToUpperInvariant()

$modbusRtuShaFinal =
    (Get-G2Sha256Path $modbusRtuArchive).ToUpperInvariant()

if ($tftShaFinal -ne $expectedTftSha) {
    throw "H3E5_TFT_ARCHIVE_CHANGED"
}

if ($displayShaFinal -ne $expectedDisplaySha) {
    throw "H3E5_DISPLAY_ARCHIVE_CHANGED"
}

if ($modbusRtuShaFinal -ne $modbusRtuSha) {
    throw "H3E5_MODBUS_RTU_ARCHIVE_CHANGED"
}

[string[]]$finalDirty =
    @(Get-G2TrackedDirtyPaths)

[string[]]$finalStaged =
    @(& git -C $script:G2RepoRoot diff --cached --name-only)

[string[]]$normalizedFinalDirty = @(
    $finalDirty |
        ForEach-Object {
            $_.Replace("\", "/")
        } |
        Sort-Object
)

[string[]]$expectedFinalDirty = @(
    $expectedCoreDirty,
    $expectedModbusDirty
) |
    Sort-Object

[object[]]$finalDirtyDiff = @(
    Compare-Object -ReferenceObject $expectedFinalDirty -DifferenceObject $normalizedFinalDirty
)

if ($finalDirtyDiff.Count -ne 0) {
    throw "H3E5_FINAL_DIRTY_SCOPE_INVALID"
}

if ($finalStaged.Count -ne 0) {
    throw "H3E5_FINAL_INDEX_NOT_CLEAN"
}

Write-Host ""
Write-Host "============================================================"
Write-Host " A14 H3E.5 FINAL SUMMARY"
Write-Host "============================================================"
Write-Host "H3E5_TCP_FULL_RUNTIME=PASS"
Write-Host "H3E5_RTU_MASTER=PASS"
Write-Host "H3E5_RTU_SLAVE=PASS"
Write-Host "H3E5_RTU_CROSS_COUNT=PASS"
Write-Host "H3E5_DISPLAY_MASTER=PASS"
Write-Host "H3E5_DISPLAY_SLAVE=PASS"
Write-Host "H3E5_ETHERNET=PASS"
Write-Host "H3E5_MICROSD_BUFFERED_DATALOG=PASS"
Write-Host "H3E5_FRAM=PASS"
Write-Host "H3E5_RTC=PASS"
Write-Host "H3E5_TCA_IO=PASS"
Write-Host "H3E5_BUTTONS=PASS"
Write-Host "H3E5_SPI_OWNERSHIP=PASS"
Write-Host "H3E5_MODBUS_RTU_PRECOMPILED=PASS"
Write-Host "H3E5_MODBUS_RTU_ARCHIVE_SHA256=$modbusRtuShaFinal"
Write-Host "H3E5_EXTERNAL_TFT_ESPI_AT_USER_BUILD=NO"
Write-Host "H3E5_PRODUCT_SOURCE_MUTATION=NO"
Write-Host "H3E5_REPOSITORY_MUTATION=NO"
Write-Host "A14_H3E5_FINAL_FULL_RUNTIME_REGRESSION=PASS"
Write-Host "NEXT=RETURN_OUTPUT_TO_CHAT_FOR_H3E_CLOSURE"
)
                }
        ).Count -gt 0

    $tftSelected =
        @(
            $lines |
                Where-Object {
                    ($_ -match '^Using library JWPLC_TFT at version ') -or
                    ($_ -match '^\s*JWPLC_TFT\s+\S+\s+.+[\\/]JWPLC_TFT\s*

    $displayPrecompiled =
        @(
            $lines |
                Where-Object {
                    ($_ -match 'Using precompiled library|Usando libreria precompilada|Usando biblioteca precompilada') -and
                    ($_ -match 'JWPLC_Display')
                }
        ).Count -gt 0

    $tftPrecompiled =
        @(
            $lines |
                Where-Object {
                    ($_ -match 'Using precompiled library|Usando libreria precompilada|Usando biblioteca precompilada') -and
                    ($_ -match 'JWPLC_TFT')
                }
        ).Count -gt 0

    $modbusRtuPrecompiled =
        @(
            $lines |
                Where-Object {
                    ($_ -match 'Using precompiled library|Usando libreria precompilada|Usando biblioteca precompilada') -and
                    ($_ -match 'JWPLC_ModbusRTU')
                }
        ).Count -gt 0

    [string[]]$externalTftEspi = @(
        $lines |
            Where-Object {
                $_ -match '^Using library TFT_eSPI at version '
            }
    )

    [int]$tftSourceObjects = @(
        Get-ChildItem -LiteralPath $BuildPath -Recurse -File -ErrorAction SilentlyContinue |
            Where-Object {
                $_.Name -eq "JWPLC_TFT.cpp.o"
            }
    ).Count

    [int]$tftEspiSourceObjects = @(
        Get-ChildItem -LiteralPath $BuildPath -Recurse -File -ErrorAction SilentlyContinue |
            Where-Object {
                $_.Name -eq "TFT_eSPI.cpp.o"
            }
    ).Count

    [int]$displaySourceObjects = @(
        Get-ChildItem -LiteralPath $BuildPath -Recurse -File -ErrorAction SilentlyContinue |
            Where-Object {
                $_.FullName -match '[\\/]libraries[\\/]JWPLC_Display[\\/].+\.cpp\.o$'
            }
    ).Count

    [int]$modbusRtuSourceObjects = @(
        Get-ChildItem -LiteralPath $BuildPath -Recurse -File -ErrorAction SilentlyContinue |
            Where-Object {
                $_.Name -eq "JWPLC_ModbusRTU.cpp.o"
            }
    ).Count

    Write-Host ("{0}_DISPLAY_SELECTED={1}" -f $Label, $displaySelected)
    Write-Host ("{0}_DISPLAY_PRECOMPILED={1}" -f $Label, $displayPrecompiled)
    Write-Host ("{0}_JWPLC_TFT_SELECTED={1}" -f $Label, $tftSelected)
    Write-Host ("{0}_JWPLC_TFT_PRECOMPILED={1}" -f $Label, $tftPrecompiled)
    Write-Host ("{0}_MODBUS_RTU_SELECTED={1}" -f $Label, $modbusRtuSelected)
    Write-Host ("{0}_MODBUS_RTU_PRECOMPILED={1}" -f $Label, $modbusRtuPrecompiled)
    Write-Host ("{0}_EXTERNAL_TFT_ESPI_SELECTION_COUNT={1}" -f $Label, $externalTftEspi.Count)
    Write-Host ("{0}_DISPLAY_SOURCE_OBJECT_COUNT={1}" -f $Label, $displaySourceObjects)
    Write-Host ("{0}_JWPLC_TFT_SOURCE_OBJECT_COUNT={1}" -f $Label, $tftSourceObjects)
    Write-Host ("{0}_TFT_ESPI_SOURCE_OBJECT_COUNT={1}" -f $Label, $tftEspiSourceObjects)
    Write-Host ("{0}_MODBUS_RTU_SOURCE_OBJECT_COUNT={1}" -f $Label, $modbusRtuSourceObjects)

    if (-not $displaySelected -or
        -not $displayPrecompiled -or
        -not $tftSelected -or
        -not $tftPrecompiled -or
        -not $modbusRtuSelected -or
        -not $modbusRtuPrecompiled -or
        $externalTftEspi.Count -ne 0 -or
        $displaySourceObjects -ne 0 -or
        $tftSourceObjects -ne 0 -or
        $tftEspiSourceObjects -ne 0 -or
        $modbusRtuSourceObjects -ne 0) {
        throw ("H3E5_{0}_BUILD_POLICY_FAILED" -f $Label)
    }
}

Write-Host ""
Write-Host "=== H3E5 STATIC PREFLIGHT ==="
Write-Host "H3E5_PROFILE=FULL_RUNTIME_MASTER_SLAVE"
Write-Host "H3E5_MASTER_PERIPHERALS=DISPLAY,ETHERNET,SD,FRAM,RTC,TCA_IO,BUTTONS,RS485,MODBUS_RTU,MODBUS_TCP"
Write-Host "H3E5_SLAVE_PERIPHERALS=DISPLAY,RS485,MODBUS_RTU"
Write-Host "H3E5_PRODUCT_SOURCE_MUTATION=NO"

if ($PreflightOnly) {
    Write-Host "H3E5_PREFLIGHT_COMPILES=NO"
    Write-Host "H3E5_PREFLIGHT_UPLOADS=NO"
    Write-Host "H3E5_PREFLIGHT_REPOSITORY_MUTATION=NO"
    Write-Host "A14_H3E5_FINAL_FULL_RUNTIME_PREFLIGHT=PASS"
    return
}

Write-Host ""
Write-Host "=== H3E5 RUN P5B FULL RUNTIME ENGINE ==="

$p5bArgs = @{
    MasterPort = $MasterPort
    SlavePort = $SlavePort
    TcpRate = $TcpRate
    DurationS = $DurationS
    AllowDirtyCoreCandidate = $true
    AllowMissingModbusRtuArchiveCandidate = $true
}

[object[]]$p5bOutput =
    @(& $p5bGate @p5bArgs *>&1)

[string[]]$p5bLines = @(
    $p5bOutput |
        ForEach-Object {
            $_.ToString()
        }
)

$p5bLines |
    ForEach-Object {
        Write-Host $_
    }

$p5bText =
    $p5bLines -join [Environment]::NewLine

if (-not $p5bText.Contains(
    "A14_P5B_PHYSICAL_MASTER_SLAVE_COMBINED=PASS")) {
    throw "H3E5_P5B_NOT_PASS"
}

Write-Host "H3E5_P5B_FULL_RUNTIME=PASS"

$tempRoot =
    Get-H3E5Marker -Lines $p5bLines -Prefix "P5B_TEMP_ROOT="

$qualificationLog =
    Join-Path $tempRoot "qualification.log"

$masterSnapshot =
    Join-Path $tempRoot "master_final.txt"

$slaveSnapshot =
    Join-Path $tempRoot "slave_final.txt"

$masterBuild =
    Join-Path $tempRoot "build_master"

$slaveBuild =
    Join-Path $tempRoot "build_slave"

$masterCompileLog =
    Join-Path $tempRoot "compile_master.log"

$slaveCompileLog =
    Join-Path $tempRoot "compile_slave.log"

foreach ($required in @(
    $qualificationLog,
    $masterSnapshot,
    $slaveSnapshot,
    $masterBuild,
    $slaveBuild,
    $masterCompileLog,
    $slaveCompileLog
)) {
    if (-not (Test-Path -LiteralPath $required)) {
        throw "H3E5_P5B_ARTIFACT_MISSING=$required"
    }
}

$qualificationText =
    [IO.File]::ReadAllText($qualificationLog)

foreach ($requiredLine in @(
    "TCP_FULL_RUNTIME_PASS=YES",
    "MASTER_RUNTIME_PASS=YES",
    "RTU_MASTER_PASS=YES",
    "RTU_SLAVE_PASS=YES",
    "RTU_CROSS_COUNT_PASS=YES",
    "RTU_SNAPSHOT_TAIL_TOLERANCE=0",
    "RTU_FINAL_SNAPSHOT_MODE=QUIESCED",
    "A14_P5B_AUTOMATED=PASS"
)) {
    if (-not $qualificationText.Contains($requiredLine)) {
        throw "H3E5_QUALIFICATION_MARKER_MISSING=$requiredLine"
    }

    Write-Host "H3E5_QUALIFICATION=$requiredLine"
}

$masterBuildArgs = @{
    CompileLog = $masterCompileLog
    BuildPath = $masterBuild
    Label = "H3E5_MASTER_BUILD"
}

Assert-H3E5Build @masterBuildArgs

$slaveBuildArgs = @{
    CompileLog = $slaveCompileLog
    BuildPath = $slaveBuild
    Label = "H3E5_SLAVE_BUILD"
}

Assert-H3E5Build @slaveBuildArgs

$masterText =
    [IO.File]::ReadAllText($masterSnapshot)

$slaveText =
    [IO.File]::ReadAllText($slaveSnapshot)

$masterExpectations = @(
    @("FULL_RUNTIME_READY", "YES"),
    @("ETH_READY", "YES"),
    @("ETH_LINK", "UP"),
    @("DISPLAY_READY", "YES"),
    @("FRAM_READY", "YES"),
    @("FRAM_FAILS", "0"),
    @("SD_READY", "YES"),
    @("SD_DATALOG_ACTIVE", "YES"),
    @("SD_DATALOG_FAILED_COMMITS", "0"),
    @("RTC_PRESENT", "YES"),
    @("RTC_UNAVAILABLE", "0"),
    @("RTC_STALE", "0"),
    @("IO_INITIALIZED", "YES"),
    @("IO_STALE", "0"),
    @("BUTTONS_READY", "YES"),
    @("BUTTON_NOT_READY", "0"),
    @("SPI_PROBE_FAILS", "0"),
    @("PERIPHERAL_FAILURE_COUNT", "0"),
    @("RTU_READY", "YES"),
    @("RTU_REQUESTS_FAILED", "0"),
    @("RTU_VERIFY_FAILS", "0"),
    @("RTU_CRC_ERRORS", "0"),
    @("RTU_MASTER_TIMEOUTS", "0"),
    @("RTU_LAST_ERROR", "OK")
)

foreach ($pair in $masterExpectations) {
    $assertArgs = @{
        Text = $masterText
        Key = $pair[0]
        Expected = $pair[1]
        Label = "MASTER"
    }

    Assert-H3E5SnapshotValue @assertArgs
}

$slaveExpectations = @(
    @("SLAVE_READY", "YES"),
    @("RTU_READY", "YES"),
    @("RTU_ROLE", "SLAVE"),
    @("RTU_SLAVE_ID", "2"),
    @("RTU_BAUD", "115200"),
    @("DISPLAY_READY", "YES"),
    @("RTU_CRC_ERRORS", "0"),
    @("RTU_EXCEPTIONS_SENT", "0"),
    @("RTU_LAST_ERROR", "OK"),
    @("SLAVE_HR1", "21930")
)

foreach ($pair in $slaveExpectations) {
    $assertArgs = @{
        Text = $slaveText
        Key = $pair[0]
        Expected = $pair[1]
        Label = "SLAVE"
    }

    Assert-H3E5SnapshotValue @assertArgs
}

$tftShaFinal =
    (Get-G2Sha256Path $tftArchive).ToUpperInvariant()

$displayShaFinal =
    (Get-G2Sha256Path $displayArchive).ToUpperInvariant()

$modbusRtuShaFinal =
    (Get-G2Sha256Path $modbusRtuArchive).ToUpperInvariant()

if ($tftShaFinal -ne $expectedTftSha) {
    throw "H3E5_TFT_ARCHIVE_CHANGED"
}

if ($displayShaFinal -ne $expectedDisplaySha) {
    throw "H3E5_DISPLAY_ARCHIVE_CHANGED"
}

if ($modbusRtuShaFinal -ne $modbusRtuSha) {
    throw "H3E5_MODBUS_RTU_ARCHIVE_CHANGED"
}

[string[]]$finalDirty =
    @(Get-G2TrackedDirtyPaths)

[string[]]$finalStaged =
    @(& git -C $script:G2RepoRoot diff --cached --name-only)

[string[]]$normalizedFinalDirty = @(
    $finalDirty |
        ForEach-Object {
            $_.Replace("\", "/")
        } |
        Sort-Object
)

[string[]]$expectedFinalDirty = @(
    $expectedCoreDirty,
    $expectedModbusDirty
) |
    Sort-Object

[object[]]$finalDirtyDiff = @(
    Compare-Object -ReferenceObject $expectedFinalDirty -DifferenceObject $normalizedFinalDirty
)

if ($finalDirtyDiff.Count -ne 0) {
    throw "H3E5_FINAL_DIRTY_SCOPE_INVALID"
}

if ($finalStaged.Count -ne 0) {
    throw "H3E5_FINAL_INDEX_NOT_CLEAN"
}

Write-Host ""
Write-Host "============================================================"
Write-Host " A14 H3E.5 FINAL SUMMARY"
Write-Host "============================================================"
Write-Host "H3E5_TCP_FULL_RUNTIME=PASS"
Write-Host "H3E5_RTU_MASTER=PASS"
Write-Host "H3E5_RTU_SLAVE=PASS"
Write-Host "H3E5_RTU_CROSS_COUNT=PASS"
Write-Host "H3E5_DISPLAY_MASTER=PASS"
Write-Host "H3E5_DISPLAY_SLAVE=PASS"
Write-Host "H3E5_ETHERNET=PASS"
Write-Host "H3E5_MICROSD_BUFFERED_DATALOG=PASS"
Write-Host "H3E5_FRAM=PASS"
Write-Host "H3E5_RTC=PASS"
Write-Host "H3E5_TCA_IO=PASS"
Write-Host "H3E5_BUTTONS=PASS"
Write-Host "H3E5_SPI_OWNERSHIP=PASS"
Write-Host "H3E5_MODBUS_RTU_PRECOMPILED=PASS"
Write-Host "H3E5_MODBUS_RTU_ARCHIVE_SHA256=$modbusRtuShaFinal"
Write-Host "H3E5_EXTERNAL_TFT_ESPI_AT_USER_BUILD=NO"
Write-Host "H3E5_PRODUCT_SOURCE_MUTATION=NO"
Write-Host "H3E5_REPOSITORY_MUTATION=NO"
Write-Host "A14_H3E5_FINAL_FULL_RUNTIME_REGRESSION=PASS"
Write-Host "NEXT=RETURN_OUTPUT_TO_CHAT_FOR_H3E_CLOSURE"
)
                }
        ).Count -gt 0

    $modbusRtuSelected =
        @(
            $lines |
                Where-Object {
                    ($_ -match '^Using library JWPLC_ModbusRTU at version ') -or
                    ($_ -match '^\s*JWPLC_ModbusRTU\s+\S+\s+.+[\\/]JWPLC_ModbusRTU\s*

    $displayPrecompiled =
        @(
            $lines |
                Where-Object {
                    ($_ -match 'Using precompiled library|Usando libreria precompilada|Usando biblioteca precompilada') -and
                    ($_ -match 'JWPLC_Display')
                }
        ).Count -gt 0

    $tftPrecompiled =
        @(
            $lines |
                Where-Object {
                    ($_ -match 'Using precompiled library|Usando libreria precompilada|Usando biblioteca precompilada') -and
                    ($_ -match 'JWPLC_TFT')
                }
        ).Count -gt 0

    $modbusRtuPrecompiled =
        @(
            $lines |
                Where-Object {
                    ($_ -match 'Using precompiled library|Usando libreria precompilada|Usando biblioteca precompilada') -and
                    ($_ -match 'JWPLC_ModbusRTU')
                }
        ).Count -gt 0

    [string[]]$externalTftEspi = @(
        $lines |
            Where-Object {
                $_ -match '^Using library TFT_eSPI at version '
            }
    )

    [int]$tftSourceObjects = @(
        Get-ChildItem -LiteralPath $BuildPath -Recurse -File -ErrorAction SilentlyContinue |
            Where-Object {
                $_.Name -eq "JWPLC_TFT.cpp.o"
            }
    ).Count

    [int]$tftEspiSourceObjects = @(
        Get-ChildItem -LiteralPath $BuildPath -Recurse -File -ErrorAction SilentlyContinue |
            Where-Object {
                $_.Name -eq "TFT_eSPI.cpp.o"
            }
    ).Count

    [int]$displaySourceObjects = @(
        Get-ChildItem -LiteralPath $BuildPath -Recurse -File -ErrorAction SilentlyContinue |
            Where-Object {
                $_.FullName -match '[\\/]libraries[\\/]JWPLC_Display[\\/].+\.cpp\.o$'
            }
    ).Count

    [int]$modbusRtuSourceObjects = @(
        Get-ChildItem -LiteralPath $BuildPath -Recurse -File -ErrorAction SilentlyContinue |
            Where-Object {
                $_.Name -eq "JWPLC_ModbusRTU.cpp.o"
            }
    ).Count

    Write-Host ("{0}_DISPLAY_SELECTED={1}" -f $Label, $displaySelected)
    Write-Host ("{0}_DISPLAY_PRECOMPILED={1}" -f $Label, $displayPrecompiled)
    Write-Host ("{0}_JWPLC_TFT_SELECTED={1}" -f $Label, $tftSelected)
    Write-Host ("{0}_JWPLC_TFT_PRECOMPILED={1}" -f $Label, $tftPrecompiled)
    Write-Host ("{0}_MODBUS_RTU_SELECTED={1}" -f $Label, $modbusRtuSelected)
    Write-Host ("{0}_MODBUS_RTU_PRECOMPILED={1}" -f $Label, $modbusRtuPrecompiled)
    Write-Host ("{0}_EXTERNAL_TFT_ESPI_SELECTION_COUNT={1}" -f $Label, $externalTftEspi.Count)
    Write-Host ("{0}_DISPLAY_SOURCE_OBJECT_COUNT={1}" -f $Label, $displaySourceObjects)
    Write-Host ("{0}_JWPLC_TFT_SOURCE_OBJECT_COUNT={1}" -f $Label, $tftSourceObjects)
    Write-Host ("{0}_TFT_ESPI_SOURCE_OBJECT_COUNT={1}" -f $Label, $tftEspiSourceObjects)
    Write-Host ("{0}_MODBUS_RTU_SOURCE_OBJECT_COUNT={1}" -f $Label, $modbusRtuSourceObjects)

    if (-not $displaySelected -or
        -not $displayPrecompiled -or
        -not $tftSelected -or
        -not $tftPrecompiled -or
        -not $modbusRtuSelected -or
        -not $modbusRtuPrecompiled -or
        $externalTftEspi.Count -ne 0 -or
        $displaySourceObjects -ne 0 -or
        $tftSourceObjects -ne 0 -or
        $tftEspiSourceObjects -ne 0 -or
        $modbusRtuSourceObjects -ne 0) {
        throw ("H3E5_{0}_BUILD_POLICY_FAILED" -f $Label)
    }
}

Write-Host ""
Write-Host "=== H3E5 STATIC PREFLIGHT ==="
Write-Host "H3E5_PROFILE=FULL_RUNTIME_MASTER_SLAVE"
Write-Host "H3E5_MASTER_PERIPHERALS=DISPLAY,ETHERNET,SD,FRAM,RTC,TCA_IO,BUTTONS,RS485,MODBUS_RTU,MODBUS_TCP"
Write-Host "H3E5_SLAVE_PERIPHERALS=DISPLAY,RS485,MODBUS_RTU"
Write-Host "H3E5_PRODUCT_SOURCE_MUTATION=NO"

if ($PreflightOnly) {
    Write-Host "H3E5_PREFLIGHT_COMPILES=NO"
    Write-Host "H3E5_PREFLIGHT_UPLOADS=NO"
    Write-Host "H3E5_PREFLIGHT_REPOSITORY_MUTATION=NO"
    Write-Host "A14_H3E5_FINAL_FULL_RUNTIME_PREFLIGHT=PASS"
    return
}

Write-Host ""
Write-Host "=== H3E5 RUN P5B FULL RUNTIME ENGINE ==="

$p5bArgs = @{
    MasterPort = $MasterPort
    SlavePort = $SlavePort
    TcpRate = $TcpRate
    DurationS = $DurationS
    AllowDirtyCoreCandidate = $true
    AllowMissingModbusRtuArchiveCandidate = $true
}

[object[]]$p5bOutput =
    @(& $p5bGate @p5bArgs *>&1)

[string[]]$p5bLines = @(
    $p5bOutput |
        ForEach-Object {
            $_.ToString()
        }
)

$p5bLines |
    ForEach-Object {
        Write-Host $_
    }

$p5bText =
    $p5bLines -join [Environment]::NewLine

if (-not $p5bText.Contains(
    "A14_P5B_PHYSICAL_MASTER_SLAVE_COMBINED=PASS")) {
    throw "H3E5_P5B_NOT_PASS"
}

Write-Host "H3E5_P5B_FULL_RUNTIME=PASS"

$tempRoot =
    Get-H3E5Marker -Lines $p5bLines -Prefix "P5B_TEMP_ROOT="

$qualificationLog =
    Join-Path $tempRoot "qualification.log"

$masterSnapshot =
    Join-Path $tempRoot "master_final.txt"

$slaveSnapshot =
    Join-Path $tempRoot "slave_final.txt"

$masterBuild =
    Join-Path $tempRoot "build_master"

$slaveBuild =
    Join-Path $tempRoot "build_slave"

$masterCompileLog =
    Join-Path $tempRoot "compile_master.log"

$slaveCompileLog =
    Join-Path $tempRoot "compile_slave.log"

foreach ($required in @(
    $qualificationLog,
    $masterSnapshot,
    $slaveSnapshot,
    $masterBuild,
    $slaveBuild,
    $masterCompileLog,
    $slaveCompileLog
)) {
    if (-not (Test-Path -LiteralPath $required)) {
        throw "H3E5_P5B_ARTIFACT_MISSING=$required"
    }
}

$qualificationText =
    [IO.File]::ReadAllText($qualificationLog)

foreach ($requiredLine in @(
    "TCP_FULL_RUNTIME_PASS=YES",
    "MASTER_RUNTIME_PASS=YES",
    "RTU_MASTER_PASS=YES",
    "RTU_SLAVE_PASS=YES",
    "RTU_CROSS_COUNT_PASS=YES",
    "RTU_SNAPSHOT_TAIL_TOLERANCE=0",
    "RTU_FINAL_SNAPSHOT_MODE=QUIESCED",
    "A14_P5B_AUTOMATED=PASS"
)) {
    if (-not $qualificationText.Contains($requiredLine)) {
        throw "H3E5_QUALIFICATION_MARKER_MISSING=$requiredLine"
    }

    Write-Host "H3E5_QUALIFICATION=$requiredLine"
}

$masterBuildArgs = @{
    CompileLog = $masterCompileLog
    BuildPath = $masterBuild
    Label = "H3E5_MASTER_BUILD"
}

Assert-H3E5Build @masterBuildArgs

$slaveBuildArgs = @{
    CompileLog = $slaveCompileLog
    BuildPath = $slaveBuild
    Label = "H3E5_SLAVE_BUILD"
}

Assert-H3E5Build @slaveBuildArgs

$masterText =
    [IO.File]::ReadAllText($masterSnapshot)

$slaveText =
    [IO.File]::ReadAllText($slaveSnapshot)

$masterExpectations = @(
    @("FULL_RUNTIME_READY", "YES"),
    @("ETH_READY", "YES"),
    @("ETH_LINK", "UP"),
    @("DISPLAY_READY", "YES"),
    @("FRAM_READY", "YES"),
    @("FRAM_FAILS", "0"),
    @("SD_READY", "YES"),
    @("SD_DATALOG_ACTIVE", "YES"),
    @("SD_DATALOG_FAILED_COMMITS", "0"),
    @("RTC_PRESENT", "YES"),
    @("RTC_UNAVAILABLE", "0"),
    @("RTC_STALE", "0"),
    @("IO_INITIALIZED", "YES"),
    @("IO_STALE", "0"),
    @("BUTTONS_READY", "YES"),
    @("BUTTON_NOT_READY", "0"),
    @("SPI_PROBE_FAILS", "0"),
    @("PERIPHERAL_FAILURE_COUNT", "0"),
    @("RTU_READY", "YES"),
    @("RTU_REQUESTS_FAILED", "0"),
    @("RTU_VERIFY_FAILS", "0"),
    @("RTU_CRC_ERRORS", "0"),
    @("RTU_MASTER_TIMEOUTS", "0"),
    @("RTU_LAST_ERROR", "OK")
)

foreach ($pair in $masterExpectations) {
    $assertArgs = @{
        Text = $masterText
        Key = $pair[0]
        Expected = $pair[1]
        Label = "MASTER"
    }

    Assert-H3E5SnapshotValue @assertArgs
}

$slaveExpectations = @(
    @("SLAVE_READY", "YES"),
    @("RTU_READY", "YES"),
    @("RTU_ROLE", "SLAVE"),
    @("RTU_SLAVE_ID", "2"),
    @("RTU_BAUD", "115200"),
    @("DISPLAY_READY", "YES"),
    @("RTU_CRC_ERRORS", "0"),
    @("RTU_EXCEPTIONS_SENT", "0"),
    @("RTU_LAST_ERROR", "OK"),
    @("SLAVE_HR1", "21930")
)

foreach ($pair in $slaveExpectations) {
    $assertArgs = @{
        Text = $slaveText
        Key = $pair[0]
        Expected = $pair[1]
        Label = "SLAVE"
    }

    Assert-H3E5SnapshotValue @assertArgs
}

$tftShaFinal =
    (Get-G2Sha256Path $tftArchive).ToUpperInvariant()

$displayShaFinal =
    (Get-G2Sha256Path $displayArchive).ToUpperInvariant()

$modbusRtuShaFinal =
    (Get-G2Sha256Path $modbusRtuArchive).ToUpperInvariant()

if ($tftShaFinal -ne $expectedTftSha) {
    throw "H3E5_TFT_ARCHIVE_CHANGED"
}

if ($displayShaFinal -ne $expectedDisplaySha) {
    throw "H3E5_DISPLAY_ARCHIVE_CHANGED"
}

if ($modbusRtuShaFinal -ne $modbusRtuSha) {
    throw "H3E5_MODBUS_RTU_ARCHIVE_CHANGED"
}

[string[]]$finalDirty =
    @(Get-G2TrackedDirtyPaths)

[string[]]$finalStaged =
    @(& git -C $script:G2RepoRoot diff --cached --name-only)

[string[]]$normalizedFinalDirty = @(
    $finalDirty |
        ForEach-Object {
            $_.Replace("\", "/")
        } |
        Sort-Object
)

[string[]]$expectedFinalDirty = @(
    $expectedCoreDirty,
    $expectedModbusDirty
) |
    Sort-Object

[object[]]$finalDirtyDiff = @(
    Compare-Object -ReferenceObject $expectedFinalDirty -DifferenceObject $normalizedFinalDirty
)

if ($finalDirtyDiff.Count -ne 0) {
    throw "H3E5_FINAL_DIRTY_SCOPE_INVALID"
}

if ($finalStaged.Count -ne 0) {
    throw "H3E5_FINAL_INDEX_NOT_CLEAN"
}

Write-Host ""
Write-Host "============================================================"
Write-Host " A14 H3E.5 FINAL SUMMARY"
Write-Host "============================================================"
Write-Host "H3E5_TCP_FULL_RUNTIME=PASS"
Write-Host "H3E5_RTU_MASTER=PASS"
Write-Host "H3E5_RTU_SLAVE=PASS"
Write-Host "H3E5_RTU_CROSS_COUNT=PASS"
Write-Host "H3E5_DISPLAY_MASTER=PASS"
Write-Host "H3E5_DISPLAY_SLAVE=PASS"
Write-Host "H3E5_ETHERNET=PASS"
Write-Host "H3E5_MICROSD_BUFFERED_DATALOG=PASS"
Write-Host "H3E5_FRAM=PASS"
Write-Host "H3E5_RTC=PASS"
Write-Host "H3E5_TCA_IO=PASS"
Write-Host "H3E5_BUTTONS=PASS"
Write-Host "H3E5_SPI_OWNERSHIP=PASS"
Write-Host "H3E5_MODBUS_RTU_PRECOMPILED=PASS"
Write-Host "H3E5_MODBUS_RTU_ARCHIVE_SHA256=$modbusRtuShaFinal"
Write-Host "H3E5_EXTERNAL_TFT_ESPI_AT_USER_BUILD=NO"
Write-Host "H3E5_PRODUCT_SOURCE_MUTATION=NO"
Write-Host "H3E5_REPOSITORY_MUTATION=NO"
Write-Host "A14_H3E5_FINAL_FULL_RUNTIME_REGRESSION=PASS"
Write-Host "NEXT=RETURN_OUTPUT_TO_CHAT_FOR_H3E_CLOSURE"
)
                }
        ).Count -gt 0

    $displayPrecompiled =
        @(
            $lines |
                Where-Object {
                    ($_ -match 'Using precompiled library|Usando libreria precompilada|Usando biblioteca precompilada') -and
                    ($_ -match 'JWPLC_Display')
                }
        ).Count -gt 0

    $tftPrecompiled =
        @(
            $lines |
                Where-Object {
                    ($_ -match 'Using precompiled library|Usando libreria precompilada|Usando biblioteca precompilada') -and
                    ($_ -match 'JWPLC_TFT')
                }
        ).Count -gt 0

    $modbusRtuPrecompiled =
        @(
            $lines |
                Where-Object {
                    ($_ -match 'Using precompiled library|Usando libreria precompilada|Usando biblioteca precompilada') -and
                    ($_ -match 'JWPLC_ModbusRTU')
                }
        ).Count -gt 0

    [string[]]$externalTftEspi = @(
        $lines |
            Where-Object {
                $_ -match '^Using library TFT_eSPI at version '
            }
    )

    [int]$tftSourceObjects = @(
        Get-ChildItem -LiteralPath $BuildPath -Recurse -File -ErrorAction SilentlyContinue |
            Where-Object {
                $_.Name -eq "JWPLC_TFT.cpp.o"
            }
    ).Count

    [int]$tftEspiSourceObjects = @(
        Get-ChildItem -LiteralPath $BuildPath -Recurse -File -ErrorAction SilentlyContinue |
            Where-Object {
                $_.Name -eq "TFT_eSPI.cpp.o"
            }
    ).Count

    [int]$displaySourceObjects = @(
        Get-ChildItem -LiteralPath $BuildPath -Recurse -File -ErrorAction SilentlyContinue |
            Where-Object {
                $_.FullName -match '[\\/]libraries[\\/]JWPLC_Display[\\/].+\.cpp\.o$'
            }
    ).Count

    [int]$modbusRtuSourceObjects = @(
        Get-ChildItem -LiteralPath $BuildPath -Recurse -File -ErrorAction SilentlyContinue |
            Where-Object {
                $_.Name -eq "JWPLC_ModbusRTU.cpp.o"
            }
    ).Count

    Write-Host ("{0}_DISPLAY_SELECTED={1}" -f $Label, $displaySelected)
    Write-Host ("{0}_DISPLAY_PRECOMPILED={1}" -f $Label, $displayPrecompiled)
    Write-Host ("{0}_JWPLC_TFT_SELECTED={1}" -f $Label, $tftSelected)
    Write-Host ("{0}_JWPLC_TFT_PRECOMPILED={1}" -f $Label, $tftPrecompiled)
    Write-Host ("{0}_MODBUS_RTU_SELECTED={1}" -f $Label, $modbusRtuSelected)
    Write-Host ("{0}_MODBUS_RTU_PRECOMPILED={1}" -f $Label, $modbusRtuPrecompiled)
    Write-Host ("{0}_EXTERNAL_TFT_ESPI_SELECTION_COUNT={1}" -f $Label, $externalTftEspi.Count)
    Write-Host ("{0}_DISPLAY_SOURCE_OBJECT_COUNT={1}" -f $Label, $displaySourceObjects)
    Write-Host ("{0}_JWPLC_TFT_SOURCE_OBJECT_COUNT={1}" -f $Label, $tftSourceObjects)
    Write-Host ("{0}_TFT_ESPI_SOURCE_OBJECT_COUNT={1}" -f $Label, $tftEspiSourceObjects)
    Write-Host ("{0}_MODBUS_RTU_SOURCE_OBJECT_COUNT={1}" -f $Label, $modbusRtuSourceObjects)

    if (-not $displaySelected -or
        -not $displayPrecompiled -or
        -not $tftSelected -or
        -not $tftPrecompiled -or
        -not $modbusRtuSelected -or
        -not $modbusRtuPrecompiled -or
        $externalTftEspi.Count -ne 0 -or
        $displaySourceObjects -ne 0 -or
        $tftSourceObjects -ne 0 -or
        $tftEspiSourceObjects -ne 0 -or
        $modbusRtuSourceObjects -ne 0) {
        throw ("H3E5_{0}_BUILD_POLICY_FAILED" -f $Label)
    }
}

Write-Host ""
Write-Host "=== H3E5 STATIC PREFLIGHT ==="
Write-Host "H3E5_PROFILE=FULL_RUNTIME_MASTER_SLAVE"
Write-Host "H3E5_MASTER_PERIPHERALS=DISPLAY,ETHERNET,SD,FRAM,RTC,TCA_IO,BUTTONS,RS485,MODBUS_RTU,MODBUS_TCP"
Write-Host "H3E5_SLAVE_PERIPHERALS=DISPLAY,RS485,MODBUS_RTU"
Write-Host "H3E5_PRODUCT_SOURCE_MUTATION=NO"

if ($PreflightOnly) {
    Write-Host "H3E5_PREFLIGHT_COMPILES=NO"
    Write-Host "H3E5_PREFLIGHT_UPLOADS=NO"
    Write-Host "H3E5_PREFLIGHT_REPOSITORY_MUTATION=NO"
    Write-Host "A14_H3E5_FINAL_FULL_RUNTIME_PREFLIGHT=PASS"
    return
}

Write-Host ""
Write-Host "=== H3E5 RUN P5B FULL RUNTIME ENGINE ==="

$p5bArgs = @{
    MasterPort = $MasterPort
    SlavePort = $SlavePort
    TcpRate = $TcpRate
    DurationS = $DurationS
    AllowDirtyCoreCandidate = $true
    AllowMissingModbusRtuArchiveCandidate = $true
}

[object[]]$p5bOutput =
    @(& $p5bGate @p5bArgs *>&1)

[string[]]$p5bLines = @(
    $p5bOutput |
        ForEach-Object {
            $_.ToString()
        }
)

$p5bLines |
    ForEach-Object {
        Write-Host $_
    }

$p5bText =
    $p5bLines -join [Environment]::NewLine

if (-not $p5bText.Contains(
    "A14_P5B_PHYSICAL_MASTER_SLAVE_COMBINED=PASS")) {
    throw "H3E5_P5B_NOT_PASS"
}

Write-Host "H3E5_P5B_FULL_RUNTIME=PASS"

$tempRoot =
    Get-H3E5Marker -Lines $p5bLines -Prefix "P5B_TEMP_ROOT="

$qualificationLog =
    Join-Path $tempRoot "qualification.log"

$masterSnapshot =
    Join-Path $tempRoot "master_final.txt"

$slaveSnapshot =
    Join-Path $tempRoot "slave_final.txt"

$masterBuild =
    Join-Path $tempRoot "build_master"

$slaveBuild =
    Join-Path $tempRoot "build_slave"

$masterCompileLog =
    Join-Path $tempRoot "compile_master.log"

$slaveCompileLog =
    Join-Path $tempRoot "compile_slave.log"

foreach ($required in @(
    $qualificationLog,
    $masterSnapshot,
    $slaveSnapshot,
    $masterBuild,
    $slaveBuild,
    $masterCompileLog,
    $slaveCompileLog
)) {
    if (-not (Test-Path -LiteralPath $required)) {
        throw "H3E5_P5B_ARTIFACT_MISSING=$required"
    }
}

$qualificationText =
    [IO.File]::ReadAllText($qualificationLog)

foreach ($requiredLine in @(
    "TCP_FULL_RUNTIME_PASS=YES",
    "MASTER_RUNTIME_PASS=YES",
    "RTU_MASTER_PASS=YES",
    "RTU_SLAVE_PASS=YES",
    "RTU_CROSS_COUNT_PASS=YES",
    "RTU_SNAPSHOT_TAIL_TOLERANCE=0",
    "RTU_FINAL_SNAPSHOT_MODE=QUIESCED",
    "A14_P5B_AUTOMATED=PASS"
)) {
    if (-not $qualificationText.Contains($requiredLine)) {
        throw "H3E5_QUALIFICATION_MARKER_MISSING=$requiredLine"
    }

    Write-Host "H3E5_QUALIFICATION=$requiredLine"
}

$masterBuildArgs = @{
    CompileLog = $masterCompileLog
    BuildPath = $masterBuild
    Label = "H3E5_MASTER_BUILD"
}

Assert-H3E5Build @masterBuildArgs

$slaveBuildArgs = @{
    CompileLog = $slaveCompileLog
    BuildPath = $slaveBuild
    Label = "H3E5_SLAVE_BUILD"
}

Assert-H3E5Build @slaveBuildArgs

$masterText =
    [IO.File]::ReadAllText($masterSnapshot)

$slaveText =
    [IO.File]::ReadAllText($slaveSnapshot)

$masterExpectations = @(
    @("FULL_RUNTIME_READY", "YES"),
    @("ETH_READY", "YES"),
    @("ETH_LINK", "UP"),
    @("DISPLAY_READY", "YES"),
    @("FRAM_READY", "YES"),
    @("FRAM_FAILS", "0"),
    @("SD_READY", "YES"),
    @("SD_DATALOG_ACTIVE", "YES"),
    @("SD_DATALOG_FAILED_COMMITS", "0"),
    @("RTC_PRESENT", "YES"),
    @("RTC_UNAVAILABLE", "0"),
    @("RTC_STALE", "0"),
    @("IO_INITIALIZED", "YES"),
    @("IO_STALE", "0"),
    @("BUTTONS_READY", "YES"),
    @("BUTTON_NOT_READY", "0"),
    @("SPI_PROBE_FAILS", "0"),
    @("PERIPHERAL_FAILURE_COUNT", "0"),
    @("RTU_READY", "YES"),
    @("RTU_REQUESTS_FAILED", "0"),
    @("RTU_VERIFY_FAILS", "0"),
    @("RTU_CRC_ERRORS", "0"),
    @("RTU_MASTER_TIMEOUTS", "0"),
    @("RTU_LAST_ERROR", "OK")
)

foreach ($pair in $masterExpectations) {
    $assertArgs = @{
        Text = $masterText
        Key = $pair[0]
        Expected = $pair[1]
        Label = "MASTER"
    }

    Assert-H3E5SnapshotValue @assertArgs
}

$slaveExpectations = @(
    @("SLAVE_READY", "YES"),
    @("RTU_READY", "YES"),
    @("RTU_ROLE", "SLAVE"),
    @("RTU_SLAVE_ID", "2"),
    @("RTU_BAUD", "115200"),
    @("DISPLAY_READY", "YES"),
    @("RTU_CRC_ERRORS", "0"),
    @("RTU_EXCEPTIONS_SENT", "0"),
    @("RTU_LAST_ERROR", "OK"),
    @("SLAVE_HR1", "21930")
)

foreach ($pair in $slaveExpectations) {
    $assertArgs = @{
        Text = $slaveText
        Key = $pair[0]
        Expected = $pair[1]
        Label = "SLAVE"
    }

    Assert-H3E5SnapshotValue @assertArgs
}

$tftShaFinal =
    (Get-G2Sha256Path $tftArchive).ToUpperInvariant()

$displayShaFinal =
    (Get-G2Sha256Path $displayArchive).ToUpperInvariant()

$modbusRtuShaFinal =
    (Get-G2Sha256Path $modbusRtuArchive).ToUpperInvariant()

if ($tftShaFinal -ne $expectedTftSha) {
    throw "H3E5_TFT_ARCHIVE_CHANGED"
}

if ($displayShaFinal -ne $expectedDisplaySha) {
    throw "H3E5_DISPLAY_ARCHIVE_CHANGED"
}

if ($modbusRtuShaFinal -ne $modbusRtuSha) {
    throw "H3E5_MODBUS_RTU_ARCHIVE_CHANGED"
}

[string[]]$finalDirty =
    @(Get-G2TrackedDirtyPaths)

[string[]]$finalStaged =
    @(& git -C $script:G2RepoRoot diff --cached --name-only)

[string[]]$normalizedFinalDirty = @(
    $finalDirty |
        ForEach-Object {
            $_.Replace("\", "/")
        } |
        Sort-Object
)

[string[]]$expectedFinalDirty = @(
    $expectedCoreDirty,
    $expectedModbusDirty
) |
    Sort-Object

[object[]]$finalDirtyDiff = @(
    Compare-Object -ReferenceObject $expectedFinalDirty -DifferenceObject $normalizedFinalDirty
)

if ($finalDirtyDiff.Count -ne 0) {
    throw "H3E5_FINAL_DIRTY_SCOPE_INVALID"
}

if ($finalStaged.Count -ne 0) {
    throw "H3E5_FINAL_INDEX_NOT_CLEAN"
}

Write-Host ""
Write-Host "============================================================"
Write-Host " A14 H3E.5 FINAL SUMMARY"
Write-Host "============================================================"
Write-Host "H3E5_TCP_FULL_RUNTIME=PASS"
Write-Host "H3E5_RTU_MASTER=PASS"
Write-Host "H3E5_RTU_SLAVE=PASS"
Write-Host "H3E5_RTU_CROSS_COUNT=PASS"
Write-Host "H3E5_DISPLAY_MASTER=PASS"
Write-Host "H3E5_DISPLAY_SLAVE=PASS"
Write-Host "H3E5_ETHERNET=PASS"
Write-Host "H3E5_MICROSD_BUFFERED_DATALOG=PASS"
Write-Host "H3E5_FRAM=PASS"
Write-Host "H3E5_RTC=PASS"
Write-Host "H3E5_TCA_IO=PASS"
Write-Host "H3E5_BUTTONS=PASS"
Write-Host "H3E5_SPI_OWNERSHIP=PASS"
Write-Host "H3E5_MODBUS_RTU_PRECOMPILED=PASS"
Write-Host "H3E5_MODBUS_RTU_ARCHIVE_SHA256=$modbusRtuShaFinal"
Write-Host "H3E5_EXTERNAL_TFT_ESPI_AT_USER_BUILD=NO"
Write-Host "H3E5_PRODUCT_SOURCE_MUTATION=NO"
Write-Host "H3E5_REPOSITORY_MUTATION=NO"
Write-Host "A14_H3E5_FINAL_FULL_RUNTIME_REGRESSION=PASS"
Write-Host "NEXT=RETURN_OUTPUT_TO_CHAT_FOR_H3E_CLOSURE"
