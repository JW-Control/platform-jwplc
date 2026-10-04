param(
    [switch]$PreflightOnly
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"
. (Join-Path $PSScriptRoot "common.ps1")

Write-Host "============================================================"
Write-Host " A14 H3E.5A - REFRESH PRECOMPILED JWPLC_ModbusRTU"
Write-Host " SOURCE CURRENT -> CANDIDATE .A -> REAL MASTER/SLAVE LINK"
Write-Host "============================================================"

Assert-G2Branch

$arduinoCli =
    "C:\Program Files\Arduino PLC IDE Tools\arduino-cli.exe"

$fqbn =
    "jwplc_local:esp32:jwplcbasic"

$repoLibraries =
    Get-G2Path "JWPLC/2.1.0/libraries"

$officialRoot =
    Get-G2Path "JWPLC/2.1.0/libraries/JWPLC_ModbusRTU"

$headerPath =
    Join-Path $officialRoot "src\JWPLC_ModbusRTU.h"

$cppPath =
    Join-Path $officialRoot "src\JWPLC_ModbusRTU.cpp"

$officialArchive =
    Join-Path $officialRoot "src\esp32\libJWPLC_ModbusRTU.a"

$slaveSketch =
    Get-G2Path "tools/modbus-tcp-benchmark/firmware/a14_p5_rtu_slave"

$masterSketch =
    Get-G2Path "tools/modbus-tcp-benchmark/firmware/a14_p5_full_runtime_master"

$oldArchiveSha =
    "444BE3A04079A579252B2737FE6070E00ADCA949FD176880588FE69561B2A79F"

$oldArchiveBytes =
    [int64]231062

foreach ($required in @(
    $arduinoCli,
    $repoLibraries,
    $officialRoot,
    $headerPath,
    $cppPath,
    $officialArchive,
    $slaveSketch,
    $masterSketch
)) {
    if (-not (Test-Path -LiteralPath $required)) {
        throw "H3E5A_REQUIRED_PATH_MISSING=$required"
    }
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

if ($normalizedEntryDirty.Count -ne 1 -or
    $normalizedEntryDirty[0] -ne $expectedCoreDirty) {
    $normalizedEntryDirty |
        ForEach-Object {
            Write-Host "ENTRY_DIRTY=$_"
        }

    throw "H3E5A_ENTRY_DIRTY_SCOPE_INVALID"
}

if ($entryStaged.Count -ne 0) {
    throw "H3E5A_ENTRY_INDEX_NOT_CLEAN"
}

$currentArchiveSha =
    (Get-G2Sha256Path $officialArchive).ToUpperInvariant()

$currentArchiveBytes =
    (Get-Item -LiteralPath $officialArchive).Length

Write-Host "HEAD=$(Get-G2Head)"
Write-Host "H3E5A_OFFICIAL_ARCHIVE_BYTES=$currentArchiveBytes"
Write-Host "H3E5A_OFFICIAL_ARCHIVE_SHA256=$currentArchiveSha"
Write-Host "H3E5A_EXPECTED_ALPHA7_BYTES=$oldArchiveBytes"
Write-Host "H3E5A_EXPECTED_ALPHA7_SHA256=$oldArchiveSha"

if ($currentArchiveBytes -ne $oldArchiveBytes -or
    $currentArchiveSha -ne $oldArchiveSha) {
    throw "H3E5A_OFFICIAL_ARCHIVE_NOT_ALPHA7_BASELINE"
}

Write-Host "H3E5A_STALE_ARCHIVE_BASELINE=CONFIRMED_ALPHA7"

$headerText =
    [IO.File]::ReadAllText($headerPath)

$cppText =
    [IO.File]::ReadAllText($cppPath)

foreach ($symbol in @(
    "effectiveBaudRate",
    "motor(",
    "setFrameGapUs",
    "setQueuedTxEnabled",
    "setBulkRxEnabled",
    "setCrcLookupEnabled",
    "setEarlyServerDispatchEnabled"
)) {
    if (-not $headerText.Contains($symbol)) {
        throw "H3E5A_HEADER_CONTRACT_MISSING=$symbol"
    }
}

foreach ($symbol in @(
    "JWPLC_ModbusRTUClass::effectiveBaudRate",
    "JWPLC_ModbusRTUClass::motor(",
    "JWPLC_ModbusRTUClass::setFrameGapUs",
    "JWPLC_ModbusRTUClass::setQueuedTxEnabled",
    "JWPLC_ModbusRTUClass::setBulkRxEnabled",
    "JWPLC_ModbusRTUClass::setCrcLookupEnabled",
    "JWPLC_ModbusRTUClass::setEarlyServerDispatchEnabled"
)) {
    if (-not $cppText.Contains($symbol)) {
        throw "H3E5A_SOURCE_DEFINITION_MISSING=$symbol"
    }
}

Write-Host "H3E5A_CURRENT_SOURCE_API_CONTRACT=PASS"

if ($PreflightOnly) {
    Write-Host "H3E5A_PREFLIGHT_COMPILES=NO"
    Write-Host "H3E5A_PREFLIGHT_ARCHIVE_MUTATION=NO"
    Write-Host "A14_H3E5A_MODBUS_RTU_PRECOMPILED_REFRESH_PREFLIGHT=PASS"
    return
}

$runRoot =
    Join-Path $env:TEMP (
        "jwplc_a14_h3e5a_{0}" -f
        (Get-Date -Format "yyyyMMdd_HHmmss")
    )

$sourceRoot =
    Join-Path $runRoot "source-libraries\JWPLC_ModbusRTU"

$sourceSrc =
    Join-Path $sourceRoot "src"

$sourceBuild =
    Join-Path $runRoot "source-slave-build"

$sourceLog =
    Join-Path $runRoot "source-slave.log"

$candidateRoot =
    Join-Path $runRoot "candidate-libraries\JWPLC_ModbusRTU"

$candidateSrc =
    Join-Path $candidateRoot "src"

$candidateArchiveDir =
    Join-Path $candidateSrc "esp32"

$candidateArchive =
    Join-Path $candidateArchiveDir "libJWPLC_ModbusRTU.a"

$candidateSlaveBuild =
    Join-Path $runRoot "candidate-slave-build"

$candidateSlaveLog =
    Join-Path $runRoot "candidate-slave.log"

$candidateMasterBuild =
    Join-Path $runRoot "candidate-master-build"

$candidateMasterLog =
    Join-Path $runRoot "candidate-master.log"

$extractDir =
    Join-Path $runRoot "archive-members"

foreach ($dir in @(
    $sourceSrc,
    $sourceBuild,
    $candidateArchiveDir,
    $candidateSlaveBuild,
    $candidateMasterBuild,
    $extractDir
)) {
    New-Item -ItemType Directory -Force -Path $dir | Out-Null
}

Copy-Item -LiteralPath $headerPath -Destination (Join-Path $sourceSrc "JWPLC_ModbusRTU.h") -Force
Copy-Item -LiteralPath $cppPath -Destination (Join-Path $sourceSrc "JWPLC_ModbusRTU.cpp") -Force

$utf8NoBom =
    [Text.UTF8Encoding]::new($false)

$sourceProperties = @'
name=JWPLC_ModbusRTU
version=1.0.0-h3e5a
author=JW Control
maintainer=JW Control <jw.control.peru@gmail.com>
sentence=H3E5A source qualification for current Modbus RTU.
paragraph=Temporary source-only qualification library.
category=Communication
architectures=esp32
depends=JWPLC_RS485
'@

[IO.File]::WriteAllText(
    (Join-Path $sourceRoot "library.properties"),
    $sourceProperties,
    $utf8NoBom)

function Invoke-H3E5ANative {
    param(
        [string]$FilePath,
        [string[]]$Arguments
    )

    $previousPreference =
        $ErrorActionPreference

    try {
        $ErrorActionPreference =
            "Continue"

        [object[]]$output =
            @(& $FilePath @Arguments 2>&1)

        $exitCode =
            [int]$LASTEXITCODE
    }
    finally {
        $ErrorActionPreference =
            $previousPreference
    }

    return [PSCustomObject]@{
        ExitCode = $exitCode
        Output = @(
            $output |
                ForEach-Object {
                    $_.ToString()
                }
        )
    }
}

function Resolve-H3E5AArchiver {
    param([string[]]$Lines)

    foreach ($line in $Lines) {
        if ($line -match '"(?<exe>[^"]*xtensa-esp32-elf-gcc-ar(?:\.exe)?)"') {
            $candidate =
                $Matches["exe"]

            foreach ($path in @(
                $candidate,
                ($candidate + ".exe")
            )) {
                if (Test-Path -LiteralPath $path) {
                    return (Resolve-Path -LiteralPath $path).Path
                }
            }
        }
    }

    foreach ($line in $Lines) {
        if ($line -match '"(?<exe>[^"]*xtensa-esp32-elf-g\+\+(?:\.exe)?)"') {
            $toolDir =
                Split-Path -Parent $Matches["exe"]

            foreach ($name in @(
                "xtensa-esp32-elf-gcc-ar.exe",
                "xtensa-esp32-elf-gcc-ar"
            )) {
                $candidate =
                    Join-Path $toolDir $name

                if (Test-Path -LiteralPath $candidate) {
                    return (Resolve-Path -LiteralPath $candidate).Path
                }
            }
        }
    }

    throw "H3E5A_ARCHIVER_NOT_FOUND"
}

function Assert-H3E5ALibrarySelected {
    param(
        [string[]]$Lines,
        [string]$ExpectedRoot,
        [string]$Label
    )

    $normalizedExpected =
        [IO.Path]::GetFullPath(
            $ExpectedRoot
        ).TrimEnd('\', '/')

    $selected =
        $false

    foreach ($line in $Lines) {
        if ($line -match '^Using library JWPLC_ModbusRTU at version .+ in folder: (.+)$') {
            $actual =
                [IO.Path]::GetFullPath(
                    $Matches[1].Trim()
                ).TrimEnd('\', '/')

            if ($actual -ieq $normalizedExpected) {
                $selected =
                    $true
            }
        }
    }

    Write-Host ("{0}_MODBUS_RTU_SELECTED={1}" -f $Label, $selected)

    if (-not $selected) {
        throw ("H3E5A_{0}_LIBRARY_NOT_SELECTED" -f $Label)
    }
}

function Assert-H3E5APrecompiledBuild {
    param(
        [string[]]$Lines,
        [string]$BuildPath,
        [string]$ExpectedRoot,
        [string]$Label
    )

    $selectedArgs = @{
        Lines = $Lines
        ExpectedRoot = $ExpectedRoot
        Label = $Label
    }

    Assert-H3E5ALibrarySelected @selectedArgs

    $precompiled =
        @(
            $Lines |
                Where-Object {
                    ($_ -match 'Using precompiled library|Usando libreria precompilada|Usando biblioteca precompilada') -and
                    ($_ -match 'JWPLC_ModbusRTU')
                }
        ).Count -gt 0

    [int]$sourceObjects = @(
        Get-ChildItem -LiteralPath $BuildPath -Recurse -File -ErrorAction SilentlyContinue |
            Where-Object {
                $_.Name -eq "JWPLC_ModbusRTU.cpp.o"
            }
    ).Count

    Write-Host ("{0}_MODBUS_RTU_PRECOMPILED={1}" -f $Label, $precompiled)
    Write-Host ("{0}_MODBUS_RTU_SOURCE_OBJECT_COUNT={1}" -f $Label, $sourceObjects)

    if (-not $precompiled -or
        $sourceObjects -ne 0) {
        throw ("H3E5A_{0}_PRECOMPILED_POLICY_FAILED" -f $Label)
    }
}

$sourceArgs = @(
    "compile",
    "--fqbn", $fqbn,
    "-j", "0",
    "-v",
    "--clean",
    "--build-path", $sourceBuild,
    "--library", $sourceRoot,
    "--libraries", $repoLibraries,
    $slaveSketch
)

Write-Host ""
Write-Host "=== H3E5A SOURCE-ONLY SLAVE BUILD ==="

$sourceRun =
    Invoke-H3E5ANative -FilePath $arduinoCli -Arguments $sourceArgs

$sourceRun.Output |
    Set-Content -LiteralPath $sourceLog -Encoding UTF8

Write-Host "H3E5A_SOURCE_COMPILE_EXIT=$($sourceRun.ExitCode)"
Write-Host "H3E5A_SOURCE_COMPILE_LOG=$sourceLog"

if ($sourceRun.ExitCode -ne 0) {
    $sourceRun.Output |
        Select-Object -Last 180 |
        ForEach-Object {
            Write-Host $_
        }

    throw "H3E5A_SOURCE_COMPILE_FAILED"
}

$sourceSelectedArgs = @{
    Lines = $sourceRun.Output
    ExpectedRoot = $sourceRoot
    Label = "H3E5A_SOURCE"
}

Assert-H3E5ALibrarySelected @sourceSelectedArgs

[object[]]$sourceObjects = @(
    Get-ChildItem -LiteralPath $sourceBuild -Recurse -File -ErrorAction SilentlyContinue |
        Where-Object {
            $_.Name -eq "JWPLC_ModbusRTU.cpp.o"
        }
)

Write-Host "H3E5A_SOURCE_OBJECT_COUNT=$($sourceObjects.Count)"

if ($sourceObjects.Count -ne 1) {
    throw "H3E5A_SOURCE_OBJECT_COUNT_INVALID"
}

$archiver =
    Resolve-H3E5AArchiver -Lines $sourceRun.Output

Write-Host "H3E5A_ARCHIVER=$archiver"

$archiveRun =
    Invoke-H3E5ANative -FilePath $archiver -Arguments @(
        "crs",
        $candidateArchive,
        $sourceObjects[0].FullName
    )

if ($archiveRun.ExitCode -ne 0 -or
    -not (Test-Path -LiteralPath $candidateArchive)) {
    throw "H3E5A_ARCHIVE_CREATE_FAILED"
}

$listRun =
    Invoke-H3E5ANative -FilePath $archiver -Arguments @(
        "t",
        $candidateArchive
    )

if ($listRun.ExitCode -ne 0) {
    throw "H3E5A_ARCHIVE_LIST_FAILED"
}

[string[]]$members = @(
    $listRun.Output |
        ForEach-Object {
            $_.Trim()
        } |
        Where-Object {
            -not [string]::IsNullOrWhiteSpace($_)
        }
)

Write-Host "H3E5A_ARCHIVE_MEMBER_COUNT=$($members.Count)"

$members |
    ForEach-Object {
        Write-Host "H3E5A_ARCHIVE_MEMBER=$_"
    }

if ($members.Count -ne 1 -or
    $members[0] -ne "JWPLC_ModbusRTU.cpp.o") {
    throw "H3E5A_ARCHIVE_MEMBER_SET_INVALID"
}

$oldLocation =
    Get-Location

try {
    Set-Location $extractDir

    $extractRun =
        Invoke-H3E5ANative -FilePath $archiver -Arguments @(
            "x",
            $candidateArchive
        )
}
finally {
    Set-Location $oldLocation
}

if ($extractRun.ExitCode -ne 0) {
    throw "H3E5A_ARCHIVE_EXTRACT_FAILED"
}

$extractedObject =
    Join-Path $extractDir "JWPLC_ModbusRTU.cpp.o"

if (-not (Test-Path -LiteralPath $extractedObject)) {
    throw "H3E5A_EXTRACTED_OBJECT_MISSING"
}

if ((Get-G2Sha256Path $sourceObjects[0].FullName) -ne
    (Get-G2Sha256Path $extractedObject)) {
    throw "H3E5A_ARCHIVE_MEMBER_BYTE_PARITY_FAILED"
}

Write-Host "H3E5A_ARCHIVE_MEMBER_BYTE_PARITY=PASS"

Copy-Item -LiteralPath $headerPath -Destination (Join-Path $candidateSrc "JWPLC_ModbusRTU.h") -Force

$candidateProperties = @'
name=JWPLC_ModbusRTU
version=1.0.0
author=JW Control
maintainer=JW Control <jw.control.peru@gmail.com>
sentence=Modbus RTU cooperativo para JWPLC Basic sobre RS-485.
paragraph=Current Alpha14 Modbus RTU precompiled candidate.
category=Communication
architectures=esp32
depends=JWPLC_RS485
precompiled=full
'@

[IO.File]::WriteAllText(
    (Join-Path $candidateRoot "library.properties"),
    $candidateProperties,
    $utf8NoBom)

$candidateSha =
    (Get-G2Sha256Path $candidateArchive).ToUpperInvariant()

$candidateBytes =
    (Get-Item -LiteralPath $candidateArchive).Length

Write-Host "H3E5A_CANDIDATE_ROOT=$candidateRoot"
Write-Host "H3E5A_CANDIDATE_ARCHIVE=$candidateArchive"
Write-Host "H3E5A_CANDIDATE_ARCHIVE_BYTES=$candidateBytes"
Write-Host "H3E5A_CANDIDATE_ARCHIVE_SHA256=$candidateSha"

if ($candidateSha -eq $oldArchiveSha) {
    throw "H3E5A_CANDIDATE_IDENTICAL_TO_STALE_ALPHA7"
}

$candidateSlaveArgs = @(
    "compile",
    "--fqbn", $fqbn,
    "-j", "0",
    "-v",
    "--clean",
    "--build-path", $candidateSlaveBuild,
    "--library", $candidateRoot,
    "--libraries", $repoLibraries,
    $slaveSketch
)

Write-Host ""
Write-Host "=== H3E5A CANDIDATE SLAVE BUILD ==="

$candidateSlaveRun =
    Invoke-H3E5ANative -FilePath $arduinoCli -Arguments $candidateSlaveArgs

$candidateSlaveRun.Output |
    Set-Content -LiteralPath $candidateSlaveLog -Encoding UTF8

Write-Host "H3E5A_CANDIDATE_SLAVE_COMPILE_EXIT=$($candidateSlaveRun.ExitCode)"
Write-Host "H3E5A_CANDIDATE_SLAVE_COMPILE_LOG=$candidateSlaveLog"

if ($candidateSlaveRun.ExitCode -ne 0) {
    $candidateSlaveRun.Output |
        Select-Object -Last 180 |
        ForEach-Object {
            Write-Host $_
        }

    throw "H3E5A_CANDIDATE_SLAVE_COMPILE_FAILED"
}

$candidateSlaveAssertArgs = @{
    Lines = $candidateSlaveRun.Output
    BuildPath = $candidateSlaveBuild
    ExpectedRoot = $candidateRoot
    Label = "H3E5A_CANDIDATE_SLAVE"
}

Assert-H3E5APrecompiledBuild @candidateSlaveAssertArgs

$candidateMasterArgs = @(
    "compile",
    "--fqbn", $fqbn,
    "-j", "0",
    "-v",
    "--clean",
    "--build-path", $candidateMasterBuild,
    "--library", $candidateRoot,
    "--libraries", $repoLibraries,
    $masterSketch
)

Write-Host ""
Write-Host "=== H3E5A CANDIDATE MASTER BUILD ==="

$candidateMasterRun =
    Invoke-H3E5ANative -FilePath $arduinoCli -Arguments $candidateMasterArgs

$candidateMasterRun.Output |
    Set-Content -LiteralPath $candidateMasterLog -Encoding UTF8

Write-Host "H3E5A_CANDIDATE_MASTER_COMPILE_EXIT=$($candidateMasterRun.ExitCode)"
Write-Host "H3E5A_CANDIDATE_MASTER_COMPILE_LOG=$candidateMasterLog"

if ($candidateMasterRun.ExitCode -ne 0) {
    $candidateMasterRun.Output |
        Select-Object -Last 180 |
        ForEach-Object {
            Write-Host $_
        }

    throw "H3E5A_CANDIDATE_MASTER_COMPILE_FAILED"
}

$candidateMasterAssertArgs = @{
    Lines = $candidateMasterRun.Output
    BuildPath = $candidateMasterBuild
    ExpectedRoot = $candidateRoot
    Label = "H3E5A_CANDIDATE_MASTER"
}

Assert-H3E5APrecompiledBuild @candidateMasterAssertArgs

$originalArchive =
    [IO.File]::ReadAllBytes(
        $officialArchive
    )

$placementSucceeded =
    $false

try {
    Write-Host ""
    Write-Host "=== H3E5A PLACE CANDIDATE IN OFFICIAL WORKTREE ==="

    Copy-Item -LiteralPath $candidateArchive -Destination $officialArchive -Force

    $officialCandidateSha =
        (Get-G2Sha256Path $officialArchive).ToUpperInvariant()

    $officialCandidateBytes =
        (Get-Item -LiteralPath $officialArchive).Length

    Write-Host "H3E5A_OFFICIAL_CANDIDATE_BYTES=$officialCandidateBytes"
    Write-Host "H3E5A_OFFICIAL_CANDIDATE_SHA256=$officialCandidateSha"

    if ($officialCandidateSha -ne $candidateSha -or
        $officialCandidateBytes -ne $candidateBytes) {
        throw "H3E5A_OFFICIAL_CANDIDATE_IDENTITY_FAILED"
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

    [string[]]$expectedDirty = @(
        $expectedCoreDirty,
        "JWPLC/2.1.0/libraries/JWPLC_ModbusRTU/src/esp32/libJWPLC_ModbusRTU.a"
    ) |
        Sort-Object

    [object[]]$dirtyDiff = @(
        Compare-Object -ReferenceObject $expectedDirty -DifferenceObject $normalizedFinalDirty
    )

    Write-Host "H3E5A_FINAL_DIRTY_COUNT=$($normalizedFinalDirty.Count)"

    $normalizedFinalDirty |
        ForEach-Object {
            Write-Host "H3E5A_FINAL_DIRTY=$_"
        }

    if ($dirtyDiff.Count -ne 0) {
        throw "H3E5A_FINAL_DIRTY_SCOPE_INVALID"
    }

    if ($finalStaged.Count -ne 0) {
        throw "H3E5A_FINAL_INDEX_NOT_CLEAN"
    }

    $placementSucceeded =
        $true

    Write-Host "H3E5A_SOURCE_ONLY_BUILD=PASS"
    Write-Host "H3E5A_ARCHIVE_SELF_CONTAINED=PASS"
    Write-Host "H3E5A_CANDIDATE_SLAVE_LINK=PASS"
    Write-Host "H3E5A_CANDIDATE_MASTER_LINK=PASS"
    Write-Host "H3E5A_OFFICIAL_WORKTREE=CANDIDATE_DIRTY"
    Write-Host "H3E5A_PRODUCT_SOURCE_MUTATION=NO"
    Write-Host "H3E5A_PRODUCT_ARCHIVE_MUTATION=CANDIDATE_ONLY"
    Write-Host "A14_H3E5A_MODBUS_RTU_PRECOMPILED_REFRESH=PASS"
    Write-Host "NEXT=RUN_H3E5_FULL_RUNTIME_WITH_THIS_EXACT_ARCHIVE"
}
finally {
    if (-not $placementSucceeded) {
        Write-Host "H3E5A_ROLLBACK=START"

        [IO.File]::WriteAllBytes(
            $officialArchive,
            $originalArchive)

        Write-Host "H3E5A_ROLLBACK=PASS"
    }
}
