param(
    [string]$ArduinoCli = "C:\Program Files\Arduino PLC IDE Tools\arduino-cli.exe",
    [string]$Fqbn = "jwplc_local:esp32:jwplcbasic",
    [string]$ResultRoot = ""
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$repo = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot "..\..\.."))
$expectedBranch = "v2.1.0-alpha.12/feature/modbus-tcp"

$libraryRel = "JWPLC/2.1.0/libraries/JWPLC_ModbusRTU"
$libraryRoot = Join-Path $repo $libraryRel
$headerPath = Join-Path $libraryRoot "src\JWPLC_ModbusRTU.h"
$cppPath = Join-Path $libraryRoot "src\JWPLC_ModbusRTU.cpp"
$archiveRel = "$libraryRel/src/esp32/libJWPLC_ModbusRTU.a"
$archivePath = Join-Path $repo $archiveRel
$repoLibraries = Join-Path $repo "JWPLC\2.1.0\libraries"

$slaveSketch = Join-Path $repo "JWPLC\2.1.0\libraries\JWPLC_ModbusRTU\examples\01.ModbusRTU_Slave_Holding"
$masterSketch = Join-Path $repo "JWPLC\2.1.0\libraries\JWPLC_ModbusRTU\examples\02.ModbusRTU_Master_Read"

function Get-Sha256Lower {
    param([string]$Path)
    return (Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash.ToLowerInvariant()
}

function Invoke-NativeCaptured {
    param(
        [string]$FilePath,
        [string[]]$Arguments
    )

    $previousPreference = $ErrorActionPreference
    try {
        $ErrorActionPreference = "Continue"
        [object[]]$output = @(& $FilePath @Arguments 2>&1)
        $exitCode = [int]$LASTEXITCODE
    }
    finally {
        $ErrorActionPreference = $previousPreference
    }

    return [pscustomobject]@{
        ExitCode = $exitCode
        Output = @($output | ForEach-Object { $_.ToString() })
    }
}

function Resolve-Archiver {
    param([string[]]$Lines)

    foreach ($line in $Lines) {
        if ($line -match '"(?<exe>[^"]*xtensa-esp32-elf-gcc-ar(?:\.exe)?)"') {
            $candidate = $Matches["exe"]
            foreach ($path in @($candidate, ($candidate + ".exe"))) {
                if (Test-Path -LiteralPath $path) {
                    return (Resolve-Path -LiteralPath $path).Path
                }
            }
        }
    }

    foreach ($line in $Lines) {
        if ($line -match '"(?<exe>[^"]*xtensa-esp32-elf-g\+\+(?:\.exe)?)"') {
            $toolDir = Split-Path -Parent $Matches["exe"]
            foreach ($name in @("xtensa-esp32-elf-gcc-ar.exe", "xtensa-esp32-elf-gcc-ar")) {
                $candidate = Join-Path $toolDir $name
                if (Test-Path -LiteralPath $candidate) {
                    return (Resolve-Path -LiteralPath $candidate).Path
                }
            }
        }
    }

    throw "A12_RTU_ARCHIVER_NOT_FOUND"
}

function Assert-LibrarySelected {
    param(
        [string[]]$Lines,
        [string]$ExpectedRoot,
        [string]$Label
    )

    $normalizedExpected = [IO.Path]::GetFullPath($ExpectedRoot).TrimEnd('\', '/')
    $selected = $false

    foreach ($line in $Lines) {
        if ($line -match '^Using library JWPLC_ModbusRTU at version .+ in folder: (.+)$') {
            $actual = [IO.Path]::GetFullPath($Matches[1].Trim()).TrimEnd('\', '/')
            if ($actual -ieq $normalizedExpected) {
                $selected = $true
            }
        }
    }

    Write-Host "$($Label)_MODBUS_RTU_SELECTED=$selected"

    if (-not $selected) {
        throw "A12_RTU_$($Label)_LIBRARY_NOT_SELECTED"
    }
}

function Assert-SourceBuild {
    param(
        [string[]]$Lines,
        [string]$BuildPath,
        [string]$ExpectedRoot,
        [string]$Label
    )

    Assert-LibrarySelected -Lines $Lines -ExpectedRoot $ExpectedRoot -Label $Label

    [int]$sourceObjects = @(
        Get-ChildItem -LiteralPath $BuildPath -Recurse -File -ErrorAction SilentlyContinue |
            Where-Object { $_.Name -eq "JWPLC_ModbusRTU.cpp.o" }
    ).Count

    $precompiled = @(
        $Lines | Where-Object {
            ($_ -match 'Using precompiled library|Usando libreria precompilada|Usando biblioteca precompilada') -and
            ($_ -match 'JWPLC_ModbusRTU')
        }
    ).Count -gt 0

    Write-Host "$($Label)_SOURCE_OBJECT_COUNT=$sourceObjects"
    Write-Host "$($Label)_PRECOMPILED_MARKER=$(if ($precompiled) { 'YES' } else { 'NO' })"

    if ($sourceObjects -ne 1 -or $precompiled) {
        throw "A12_RTU_$($Label)_SOURCE_POLICY_FAILED"
    }
}

function Assert-PrecompiledBuild {
    param(
        [string[]]$Lines,
        [string]$BuildPath,
        [string]$ExpectedRoot,
        [string]$Label
    )

    Assert-LibrarySelected -Lines $Lines -ExpectedRoot $ExpectedRoot -Label $Label

    [int]$sourceObjects = @(
        Get-ChildItem -LiteralPath $BuildPath -Recurse -File -ErrorAction SilentlyContinue |
            Where-Object { $_.Name -eq "JWPLC_ModbusRTU.cpp.o" }
    ).Count

    $precompiled = @(
        $Lines | Where-Object {
            ($_ -match 'Using precompiled library|Usando libreria precompilada|Usando biblioteca precompilada') -and
            ($_ -match 'JWPLC_ModbusRTU')
        }
    ).Count -gt 0

    Write-Host "$($Label)_SOURCE_OBJECT_COUNT=$sourceObjects"
    Write-Host "$($Label)_PRECOMPILED_MARKER=$(if ($precompiled) { 'YES' } else { 'NO' })"

    if ($sourceObjects -ne 0 -or -not $precompiled) {
        throw "A12_RTU_$($Label)_PRECOMPILED_POLICY_FAILED"
    }
}

$branch = (& git -C $repo branch --show-current).Trim()
$head = (& git -C $repo rev-parse HEAD).Trim()

if ($branch -ne $expectedBranch) {
    throw "A12_RTU_BRANCH_MISMATCH expected=$expectedBranch actual=$branch"
}

[string[]]$entryDirty = @(& git -C $repo diff --name-only)
[string[]]$entryStaged = @(& git -C $repo diff --cached --name-only)

if ($entryDirty.Count -ne 0) {
    $entryDirty | ForEach-Object { Write-Host "ENTRY_DIRTY=$_" }
    throw "A12_RTU_TRACKED_TREE_NOT_CLEAN"
}
if ($entryStaged.Count -ne 0) {
    throw "A12_RTU_INDEX_NOT_CLEAN"
}

& git -C $repo diff --check
if ($LASTEXITCODE -ne 0) {
    throw "A12_RTU_GIT_DIFF_CHECK_FAILED"
}

foreach ($required in @(
    $libraryRoot,
    $headerPath,
    $cppPath,
    $archivePath,
    $repoLibraries,
    $slaveSketch,
    $masterSketch
)) {
    if (-not (Test-Path -LiteralPath $required)) {
        throw "A12_RTU_REQUIRED_PATH_MISSING=$required"
    }
}

if (-not (Test-Path -LiteralPath $ArduinoCli)) {
    $candidates = @(
        (Join-Path $env:LOCALAPPDATA "Programs\Arduino IDE\resources\app\lib\backend\resources\arduino-cli.exe"),
        (Join-Path $env:LOCALAPPDATA "Programs\arduino-ide\resources\app\lib\backend\resources\arduino-cli.exe"),
        "C:\Program Files\Arduino IDE\resources\app\lib\backend\resources\arduino-cli.exe"
    )

    foreach ($candidate in $candidates) {
        if (Test-Path -LiteralPath $candidate) {
            $ArduinoCli = $candidate
            break
        }
    }
}

if (-not (Test-Path -LiteralPath $ArduinoCli)) {
    throw "A12_RTU_ARDUINO_CLI_NOT_FOUND"
}

$headerText = [IO.File]::ReadAllText($headerPath)
$cppText = [IO.File]::ReadAllText($cppPath)

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
        throw "A12_RTU_HEADER_CONTRACT_MISSING=$symbol"
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
        throw "A12_RTU_SOURCE_DEFINITION_MISSING=$symbol"
    }
}

Write-Host "A12_RTU_CURRENT_SOURCE_API_CONTRACT=PASS"

$sourceCommit = (& git -C $repo log -1 --format=%H -- "$libraryRel/src").Trim()
$sourceDate = (& git -C $repo log -1 --format=%cI -- "$libraryRel/src").Trim()
$archiveCommit = (& git -C $repo log -1 --format=%H -- $archiveRel).Trim()
$archiveDate = (& git -C $repo log -1 --format=%cI -- $archiveRel).Trim()

$oldSha = Get-Sha256Lower $archivePath
$oldBytes = (Get-Item -LiteralPath $archivePath).Length

$stamp = Get-Date -Format "yyyyMMdd_HHmmss"
if ([string]::IsNullOrWhiteSpace($ResultRoot)) {
    $ResultRoot = Join-Path $repo "tools\alpha12\results\modbus_rtu_precompiled_refresh_$stamp"
}
New-Item -ItemType Directory -Force -Path $ResultRoot | Out-Null

$runRoot = Join-Path $env:TEMP ("jwplc_a12_rtu_refresh_" + $stamp)
$sourceRoot = Join-Path $runRoot "source-libraries\JWPLC_ModbusRTU"
$sourceSrc = Join-Path $sourceRoot "src"
$sourceBuild = Join-Path $runRoot "source-slave-build"
$sourceLog = Join-Path $ResultRoot "source_slave.log"

$candidateRoot = Join-Path $runRoot "candidate-libraries\JWPLC_ModbusRTU"
$candidateSrc = Join-Path $candidateRoot "src"
$candidateArchiveDir = Join-Path $candidateSrc "esp32"
$candidateArchive = Join-Path $candidateArchiveDir "libJWPLC_ModbusRTU.a"
$candidateEvidenceArchive = Join-Path $ResultRoot "libJWPLC_ModbusRTU-candidate.a"

$candidateSlaveBuild = Join-Path $runRoot "candidate-slave-build"
$candidateSlaveLog = Join-Path $ResultRoot "candidate_slave.log"
$candidateMasterBuild = Join-Path $runRoot "candidate-master-build"
$candidateMasterLog = Join-Path $ResultRoot "candidate_master.log"
$extractDir = Join-Path $runRoot "archive-members"

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

$utf8NoBom = [Text.UTF8Encoding]::new($false)

$sourceProperties = @'
name=JWPLC_ModbusRTU
version=1.0.0-alpha12-source
author=JW Control
maintainer=JW Control <jw.control.peru@gmail.com>
sentence=Alpha12 source qualification for current Modbus RTU.
paragraph=Temporary source-only qualification library.
category=Communication
architectures=esp32
depends=JWPLC_RS485
'@

[IO.File]::WriteAllText(
    (Join-Path $sourceRoot "library.properties"),
    $sourceProperties,
    $utf8NoBom
)

$candidateProperties = @'
name=JWPLC_ModbusRTU
version=1.0.0-alpha12-candidate
author=JW Control
maintainer=JW Control <jw.control.peru@gmail.com>
sentence=Alpha12 precompiled candidate for current Modbus RTU.
paragraph=Temporary precompiled qualification library.
category=Communication
architectures=esp32
depends=JWPLC_RS485
precompiled=full
'@

[IO.File]::WriteAllText(
    (Join-Path $candidateRoot "library.properties"),
    $candidateProperties,
    $utf8NoBom
)

Copy-Item -LiteralPath $headerPath -Destination (Join-Path $candidateSrc "JWPLC_ModbusRTU.h") -Force

$backup = Join-Path $env:TEMP ("a12_rtu_archive_backup_" + $stamp + ".a")
Copy-Item -LiteralPath $archivePath -Destination $backup -Force

@(
    "DATE=$(Get-Date -Format o)"
    "BRANCH=$branch"
    "HEAD=$head"
    "SOURCE_LAST_COMMIT=$sourceCommit"
    "SOURCE_LAST_DATE=$sourceDate"
    "ARCHIVE_LAST_COMMIT=$archiveCommit"
    "ARCHIVE_LAST_DATE=$archiveDate"
    "OLD_ARCHIVE_BYTES=$oldBytes"
    "OLD_ARCHIVE_SHA256=$oldSha"
    "FQBN=$Fqbn"
    "ARDUINO_CLI=$ArduinoCli"
) | Set-Content -LiteralPath (Join-Path $ResultRoot "MANIFEST.txt") -Encoding UTF8

Write-Host "=============================================================================="
Write-Host " ALPHA12 - JWPLC_ModbusRTU PRECOMPILED REFRESH"
Write-Host "=============================================================================="
Write-Host "BRANCH=$branch"
Write-Host "HEAD=$head"
Write-Host "SOURCE_LAST_COMMIT=$sourceCommit"
Write-Host "SOURCE_LAST_DATE=$sourceDate"
Write-Host "ARCHIVE_LAST_COMMIT=$archiveCommit"
Write-Host "ARCHIVE_LAST_DATE=$archiveDate"
Write-Host "OLD_ARCHIVE_BYTES=$oldBytes"
Write-Host "OLD_ARCHIVE_SHA256=$oldSha"
Write-Host "RESULT_ROOT=$ResultRoot"

$success = $false

try {
    Write-Host ""
    Write-Host "=== SOURCE-ONLY SLAVE BUILD ==="

    $sourceRun = Invoke-NativeCaptured -FilePath $ArduinoCli -Arguments @(
        "compile",
        "--fqbn", $Fqbn,
        "-j", "0",
        "-v",
        "--clean",
        "--build-path", $sourceBuild,
        "--library", $sourceRoot,
        "--libraries", $repoLibraries,
        $slaveSketch
    )

    $sourceRun.Output | Set-Content -LiteralPath $sourceLog -Encoding UTF8
    Write-Host "SOURCE_COMPILE_EXIT=$($sourceRun.ExitCode)"

    if ($sourceRun.ExitCode -ne 0) {
        $sourceRun.Output | Select-Object -Last 220 | ForEach-Object { Write-Host $_ }
        throw "A12_RTU_SOURCE_COMPILE_FAILED"
    }

    Assert-SourceBuild -Lines $sourceRun.Output -BuildPath $sourceBuild -ExpectedRoot $sourceRoot -Label "SOURCE"

    $archiver = Resolve-Archiver -Lines $sourceRun.Output
    Write-Host "ARCHIVER=$archiver"

    $sourceObject = @(
        Get-ChildItem -LiteralPath $sourceBuild -Recurse -File -ErrorAction SilentlyContinue |
            Where-Object { $_.Name -eq "JWPLC_ModbusRTU.cpp.o" }
    )[0]

    Write-Host ""
    Write-Host "=== CREATE CANDIDATE ARCHIVE ==="

    $archiveRun = Invoke-NativeCaptured -FilePath $archiver -Arguments @(
        "crs",
        $candidateArchive,
        $sourceObject.FullName
    )

    if ($archiveRun.ExitCode -ne 0 -or -not (Test-Path -LiteralPath $candidateArchive)) {
        throw "A12_RTU_ARCHIVE_CREATE_FAILED"
    }

    $listRun = Invoke-NativeCaptured -FilePath $archiver -Arguments @("t", $candidateArchive)
    if ($listRun.ExitCode -ne 0) {
        throw "A12_RTU_ARCHIVE_LIST_FAILED"
    }

    [string[]]$members = @(
        $listRun.Output |
            ForEach-Object { $_.Trim() } |
            Where-Object { -not [string]::IsNullOrWhiteSpace($_) }
    )

    Write-Host "ARCHIVE_MEMBER_COUNT=$($members.Count)"
    $members | ForEach-Object { Write-Host "ARCHIVE_MEMBER=$_" }

    if ($members.Count -ne 1 -or $members[0] -ne "JWPLC_ModbusRTU.cpp.o") {
        throw "A12_RTU_ARCHIVE_MEMBER_SET_INVALID"
    }

    $oldLocation = Get-Location
    try {
        Set-Location $extractDir
        $extractRun = Invoke-NativeCaptured -FilePath $archiver -Arguments @("x", $candidateArchive)
    }
    finally {
        Set-Location $oldLocation
    }

    if ($extractRun.ExitCode -ne 0) {
        throw "A12_RTU_ARCHIVE_EXTRACT_FAILED"
    }

    $extractedObject = Join-Path $extractDir "JWPLC_ModbusRTU.cpp.o"
    if (-not (Test-Path -LiteralPath $extractedObject)) {
        throw "A12_RTU_EXTRACTED_OBJECT_MISSING"
    }

    $sourceObjectSha = Get-Sha256Lower $sourceObject.FullName
    $extractedObjectSha = Get-Sha256Lower $extractedObject

    Write-Host "SOURCE_OBJECT_SHA256=$sourceObjectSha"
    Write-Host "ARCHIVE_MEMBER_SHA256=$extractedObjectSha"

    if ($sourceObjectSha -ne $extractedObjectSha) {
        throw "A12_RTU_ARCHIVE_MEMBER_BYTE_PARITY_FAILED"
    }

    Write-Host "ARCHIVE_MEMBER_BYTE_PARITY=PASS"

    $candidateSha = Get-Sha256Lower $candidateArchive
    $candidateBytes = (Get-Item -LiteralPath $candidateArchive).Length
    $archiveChanged = $candidateSha -ne $oldSha

    Copy-Item -LiteralPath $candidateArchive -Destination $candidateEvidenceArchive -Force

    Write-Host "CANDIDATE_ARCHIVE_BYTES=$candidateBytes"
    Write-Host "CANDIDATE_ARCHIVE_SHA256=$candidateSha"
    Write-Host "ARCHIVE_CHANGED=$(if ($archiveChanged) { 'YES' } else { 'NO' })"

    Write-Host ""
    Write-Host "=== CANDIDATE PRECOMPILED SLAVE LINK ==="

    $candidateSlaveRun = Invoke-NativeCaptured -FilePath $ArduinoCli -Arguments @(
        "compile",
        "--fqbn", $Fqbn,
        "-j", "0",
        "-v",
        "--clean",
        "--build-path", $candidateSlaveBuild,
        "--library", $candidateRoot,
        "--libraries", $repoLibraries,
        $slaveSketch
    )

    $candidateSlaveRun.Output | Set-Content -LiteralPath $candidateSlaveLog -Encoding UTF8
    Write-Host "CANDIDATE_SLAVE_COMPILE_EXIT=$($candidateSlaveRun.ExitCode)"

    if ($candidateSlaveRun.ExitCode -ne 0) {
        $candidateSlaveRun.Output | Select-Object -Last 220 | ForEach-Object { Write-Host $_ }
        throw "A12_RTU_CANDIDATE_SLAVE_COMPILE_FAILED"
    }

    Assert-PrecompiledBuild -Lines $candidateSlaveRun.Output -BuildPath $candidateSlaveBuild -ExpectedRoot $candidateRoot -Label "CANDIDATE_SLAVE"

    Write-Host ""
    Write-Host "=== CANDIDATE PRECOMPILED MASTER LINK ==="

    $candidateMasterRun = Invoke-NativeCaptured -FilePath $ArduinoCli -Arguments @(
        "compile",
        "--fqbn", $Fqbn,
        "-j", "0",
        "-v",
        "--clean",
        "--build-path", $candidateMasterBuild,
        "--library", $candidateRoot,
        "--libraries", $repoLibraries,
        $masterSketch
    )

    $candidateMasterRun.Output | Set-Content -LiteralPath $candidateMasterLog -Encoding UTF8
    Write-Host "CANDIDATE_MASTER_COMPILE_EXIT=$($candidateMasterRun.ExitCode)"

    if ($candidateMasterRun.ExitCode -ne 0) {
        $candidateMasterRun.Output | Select-Object -Last 220 | ForEach-Object { Write-Host $_ }
        throw "A12_RTU_CANDIDATE_MASTER_COMPILE_FAILED"
    }

    Assert-PrecompiledBuild -Lines $candidateMasterRun.Output -BuildPath $candidateMasterBuild -ExpectedRoot $candidateRoot -Label "CANDIDATE_MASTER"

    Write-Host ""
    Write-Host "=== PLACE CANDIDATE IN OFFICIAL WORKTREE ==="

    Copy-Item -LiteralPath $candidateArchive -Destination $archivePath -Force

    $officialSha = Get-Sha256Lower $archivePath
    $officialBytes = (Get-Item -LiteralPath $archivePath).Length

    Write-Host "OFFICIAL_CANDIDATE_BYTES=$officialBytes"
    Write-Host "OFFICIAL_CANDIDATE_SHA256=$officialSha"

    if ($officialSha -ne $candidateSha -or $officialBytes -ne $candidateBytes) {
        throw "A12_RTU_OFFICIAL_CANDIDATE_IDENTITY_FAILED"
    }

    [string[]]$finalDirty = @(
        & git -C $repo diff --name-only |
            ForEach-Object { $_.Replace("\", "/") } |
            Sort-Object
    )
    [string[]]$finalStaged = @(& git -C $repo diff --cached --name-only)

    Write-Host "FINAL_TRACKED_DIRTY_COUNT=$($finalDirty.Count)"
    $finalDirty | ForEach-Object { Write-Host "FINAL_TRACKED_DIRTY=$_" }
    Write-Host "FINAL_STAGED_COUNT=$($finalStaged.Count)"

    $normalizedArchiveRel = $archiveRel.Replace("\", "/")

    if ($finalDirty.Count -ne 1 -or $finalDirty[0] -ne $normalizedArchiveRel) {
        throw "A12_RTU_FINAL_DIRTY_SCOPE_INVALID"
    }
    if ($finalStaged.Count -ne 0) {
        throw "A12_RTU_FINAL_INDEX_NOT_CLEAN"
    }

    @(
        "ALPHA12_MODBUS_RTU_PRECOMPILED_REFRESH=PASS"
        "BRANCH=$branch"
        "HEAD_SOURCE=$head"
        "SOURCE_LAST_COMMIT=$sourceCommit"
        "ARCHIVE_PREVIOUS_COMMIT=$archiveCommit"
        "OLD_ARCHIVE_BYTES=$oldBytes"
        "OLD_ARCHIVE_SHA256=$oldSha"
        "NEW_ARCHIVE_BYTES=$candidateBytes"
        "NEW_ARCHIVE_SHA256=$candidateSha"
        "ARCHIVE_CHANGED=$(if ($archiveChanged) { 'YES' } else { 'NO' })"
        "ARCHIVE_MEMBER_COUNT=1"
        "ARCHIVE_MEMBER=JWPLC_ModbusRTU.cpp.o"
        "ARCHIVE_MEMBER_BYTE_PARITY=PASS"
        "CANDIDATE_SLAVE_LINK=PASS"
        "CANDIDATE_MASTER_LINK=PASS"
        "FINAL_TRACKED_DIRTY=$normalizedArchiveRel"
        "PHYSICAL_UPLOAD_PERFORMED=NO"
        "RESULT_ROOT=$ResultRoot"
    ) | Set-Content -LiteralPath (Join-Path $ResultRoot "SUMMARY.txt") -Encoding UTF8

    $success = $true

    Write-Host ""
    Write-Host "=============================================================================="
    Write-Host "ALPHA12_MODBUS_RTU_PRECOMPILED_REFRESH=PASS"
    Write-Host "NEW_ARCHIVE_BYTES=$candidateBytes"
    Write-Host "NEW_ARCHIVE_SHA256=$candidateSha"
    Write-Host "ARCHIVE_MEMBER_BYTE_PARITY=PASS"
    Write-Host "CANDIDATE_SLAVE_LINK=PASS"
    Write-Host "CANDIDATE_MASTER_LINK=PASS"
    Write-Host "FINAL_TRACKED_DIRTY=$normalizedArchiveRel"
    Write-Host "PHYSICAL_UPLOAD_PERFORMED=NO"
    Write-Host "RESULT_ROOT=$ResultRoot"
}
finally {
    if (-not $success) {
        Write-Host "A12_RTU_ROLLBACK=START"

        if (Test-Path -LiteralPath $backup) {
            Copy-Item -LiteralPath $backup -Destination $archivePath -Force
        }

        $restoredSha = Get-Sha256Lower $archivePath
        Write-Host "A12_RTU_ROLLBACK_SHA256=$restoredSha"

        if ($restoredSha -ne $oldSha) {
            throw "A12_RTU_ROLLBACK_FAILED"
        }

        Write-Host "A12_RTU_ROLLBACK=PASS"
    }

    if (Test-Path -LiteralPath $backup) {
        Remove-Item -LiteralPath $backup -Force
    }
}
