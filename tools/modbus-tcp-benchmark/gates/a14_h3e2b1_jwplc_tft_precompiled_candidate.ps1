param()

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"
. (Join-Path $PSScriptRoot "common.ps1")

Write-Host "============================================================"
Write-Host " A14 H3E.2B1 - JWPLC_TFT PRECOMPILED CANDIDATE"
Write-Host "============================================================"

Assert-G2Branch

$arduinoCli = "C:\Program Files\Arduino PLC IDE Tools\arduino-cli.exe"
$fqbn = "jwplc_local:esp32:jwplcbasic"
$diagDefine = "-DJWPLC_TFT_ESPI_DIAGNOSTIC_NO_DISPLAY_AUTOLOAD=1"

$repoLibrariesRoot = Get-G2Path "JWPLC/2.1.0/libraries"
$sourceLibraryRoot = Get-G2Path "JWPLC/2.1.0/libraries/JWPLC_TFT"
$sourceHeader = Join-Path $sourceLibraryRoot "src\JWPLC_TFT.h"
$sourceProperties = Join-Path $sourceLibraryRoot "library.properties"
$sourceCpp = Join-Path $sourceLibraryRoot "src\JWPLC_TFT.cpp"

$sketchRelative = "tools/modbus-tcp-benchmark/firmware/a14_h3e2a_jwplc_tft_compile_probe"
$sketchPath = Get-G2Path $sketchRelative
$inoPath = Join-Path $sketchPath "a14_h3e2a_jwplc_tft_compile_probe.ino"

foreach ($required in @(
    $arduinoCli,
    $sourceLibraryRoot,
    $sourceHeader,
    $sourceProperties,
    $sourceCpp,
    $sketchPath,
    $inoPath
)) {
    if (-not (Test-Path -LiteralPath $required)) {
        throw "H3E2B1_REQUIRED_PATH_MISSING=$required"
    }
}

[string[]]$dirty = @(Get-G2TrackedDirtyPaths)
[string[]]$staged = @(& git -C $script:G2RepoRoot diff --cached --name-only)
[string[]]$normalizedDirty = @(
    $dirty |
        ForEach-Object { $_.Replace("\", "/") } |
        Sort-Object
)
$expectedCoreDirty = $script:G2CoreRelative.Replace("\", "/")

if ($normalizedDirty.Count -ne 1 -or $normalizedDirty[0] -ne $expectedCoreDirty) {
    $normalizedDirty | ForEach-Object { Write-Host "ENTRY_DIRTY=$_" }
    throw "H3E2B1_ENTRY_DIRTY_SCOPE_INVALID"
}
if ($staged.Count -ne 0) {
    throw "H3E2B1_ENTRY_INDEX_NOT_CLEAN"
}

$runRoot = Join-Path $env:TEMP ("jwplc_a14_h3e2b1_{0}" -f (Get-Date -Format "yyyyMMdd_HHmmss"))
$sourceBuild = Join-Path $runRoot "source-build"
$sourceLog = Join-Path $runRoot "source.log"
$candidateLibraries = Join-Path $runRoot "candidate-libraries"
$candidateLibraryRoot = Join-Path $candidateLibraries "JWPLC_TFT"
$candidateSrc = Join-Path $candidateLibraryRoot "src"
$candidateArchiveDir = Join-Path $candidateSrc "esp32"
$candidateArchive = Join-Path $candidateArchiveDir "libJWPLC_TFT.a"
$candidateProperties = Join-Path $candidateLibraryRoot "library.properties"
$candidateBuild = Join-Path $runRoot "candidate-build"
$candidateLog = Join-Path $runRoot "candidate.log"
$extractDir = Join-Path $runRoot "archive-members"

New-Item -ItemType Directory -Force -Path $sourceBuild | Out-Null
New-Item -ItemType Directory -Force -Path $candidateArchiveDir | Out-Null
New-Item -ItemType Directory -Force -Path $candidateBuild | Out-Null
New-Item -ItemType Directory -Force -Path $extractDir | Out-Null

function Invoke-H3E2B1Native {
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

    return [PSCustomObject]@{
        ExitCode = $exitCode
        Output = @($output | ForEach-Object { $_.ToString() })
    }
}

function Resolve-H3E2B1NativeTool {
    param([string]$Candidate)

    if ([string]::IsNullOrWhiteSpace($Candidate)) {
        return $null
    }

    $normalized = $Candidate.Trim().Trim('"')

    foreach ($path in @(
        $normalized,
        ($normalized + ".exe"),
        ($normalized + ".cmd"),
        ($normalized + ".bat")
    )) {
        if (Test-Path -LiteralPath $path) {
            return (Resolve-Path -LiteralPath $path).Path
        }
    }

    return $null
}

function Resolve-H3E2B1Archiver {
    param([string[]]$Lines)

    foreach ($line in $Lines) {
        $candidate = $null

        if ($line -match '"(?<exe>[^"]*xtensa-esp32-elf-gcc-ar(?:\.exe)?)"') {
            $candidate = $Matches["exe"]
        }
        elseif ($line -match '(?<exe>\S*xtensa-esp32-elf-gcc-ar(?:\.exe)?)\s+(?:cr|crs)') {
            $candidate = $Matches["exe"]
        }

        if (-not [string]::IsNullOrWhiteSpace($candidate)) {
            $resolved = Resolve-H3E2B1NativeTool -Candidate $candidate

            if (-not [string]::IsNullOrWhiteSpace($resolved)) {
                return $resolved
            }
        }
    }

    foreach ($line in $Lines) {
        if ($line -match '"(?<exe>[^"]*xtensa-esp32-elf-g\+\+(?:\.exe)?)"') {
            $toolDir = Split-Path -Parent $Matches["exe"]

            foreach ($name in @(
                "xtensa-esp32-elf-gcc-ar.exe",
                "xtensa-esp32-elf-gcc-ar"
            )) {
                $candidate = Join-Path $toolDir $name

                if (Test-Path -LiteralPath $candidate) {
                    return (Resolve-Path -LiteralPath $candidate).Path
                }
            }
        }
    }

    throw "H3E2B1_ARCHIVER_NOT_FOUND"
}

function Get-H3E2B1Usage {
    param(
        [string[]]$Lines,
        [string]$Kind
    )

    foreach ($line in $Lines) {
        if ($Kind -eq "FLASH" -and
            $line -match '(?:Sketch uses|El Sketch usa)\s+(?<n>\d+)\s+bytes') {
            return [int64]$Matches["n"]
        }

        if ($Kind -eq "RAM" -and
            $line -match '(?:Global variables use|Las variables Globales usan)\s+(?<n>\d+)\s+bytes') {
            return [int64]$Matches["n"]
        }
    }

    return [int64]-1
}

Write-Host "H3E2B1_REPOSITORY_MUTATION=NO"
Write-Host "H3E2B1_UPLOADS=NO"
Write-Host "H3E2B1_RUN_ROOT=$runRoot"

# -------------------------------------------------------------------------
# 1) Build fuente validado: produce los objetos exactos.
# -------------------------------------------------------------------------
$sourceArgs = @(
    "compile",
    "--fqbn", $fqbn,
    "-j", "0",
    "-v",
    "--clean",
    "--build-path", $sourceBuild,
    "--libraries", $repoLibrariesRoot,
    "--build-property", "compiler.cpp.extra_flags=$diagDefine",
    $sketchPath
)

Write-Host "H3E2B1_SOURCE_COMPILE_START=YES"
$sourceRun = Invoke-H3E2B1Native -FilePath $arduinoCli -Arguments $sourceArgs
$sourceRun.Output | Set-Content -LiteralPath $sourceLog -Encoding UTF8

Write-Host "H3E2B1_SOURCE_COMPILE_EXIT=$($sourceRun.ExitCode)"
Write-Host "H3E2B1_SOURCE_COMPILE_LOG=$sourceLog"

if ($sourceRun.ExitCode -ne 0) {
    $sourceRun.Output |
        Select-Object -Last 120 |
        ForEach-Object { Write-Host $_ }

    throw "H3E2B1_SOURCE_COMPILE_FAILED"
}

[object[]]$jwplcObjects = @(
    Get-ChildItem -LiteralPath $sourceBuild -Recurse -File -ErrorAction SilentlyContinue |
        Where-Object { $_.Name -eq "JWPLC_TFT.cpp.o" }
)

[object[]]$backendObjects = @(
    Get-ChildItem -LiteralPath $sourceBuild -Recurse -File -ErrorAction SilentlyContinue |
        Where-Object { $_.Name -eq "TFT_eSPI.cpp.o" }
)

Write-Host "H3E2B1_SOURCE_JWPLC_TFT_OBJECT_COUNT=$($jwplcObjects.Count)"
Write-Host "H3E2B1_SOURCE_TFT_ESPI_OBJECT_COUNT=$($backendObjects.Count)"

if ($jwplcObjects.Count -ne 1) {
    throw "H3E2B1_JWPLC_TFT_OBJECT_COUNT_INVALID"
}
if ($backendObjects.Count -ne 1) {
    throw "H3E2B1_TFT_ESPI_OBJECT_COUNT_INVALID"
}

$archiver = Resolve-H3E2B1Archiver -Lines $sourceRun.Output
Write-Host "H3E2B1_ARCHIVER=$archiver"

# -------------------------------------------------------------------------
# 2) Un unico archive autocontenido.
# -------------------------------------------------------------------------
$archiveArgs = @(
    "crs",
    $candidateArchive,
    $jwplcObjects[0].FullName,
    $backendObjects[0].FullName
)

$arRun = Invoke-H3E2B1Native -FilePath $archiver -Arguments $archiveArgs

if ($arRun.ExitCode -ne 0 -or
    -not (Test-Path -LiteralPath $candidateArchive)) {
    throw "H3E2B1_ARCHIVE_CREATE_FAILED"
}

$listRun = Invoke-H3E2B1Native -FilePath $archiver -Arguments @(
    "t",
    $candidateArchive
)

if ($listRun.ExitCode -ne 0) {
    throw "H3E2B1_ARCHIVE_LIST_FAILED"
}

[string[]]$members = @(
    $listRun.Output |
        ForEach-Object { $_.Trim() } |
        Where-Object { -not [string]::IsNullOrWhiteSpace($_) }
)

[string[]]$expectedMembers = @(
    "JWPLC_TFT.cpp.o",
    "TFT_eSPI.cpp.o"
)

[object[]]$memberDiff = @(
    Compare-Object -ReferenceObject $expectedMembers -DifferenceObject $members
)

Write-Host "H3E2B1_ARCHIVE_MEMBER_COUNT=$($members.Count)"
$members | ForEach-Object { Write-Host "H3E2B1_ARCHIVE_MEMBER=$_" }
Write-Host "H3E2B1_ARCHIVE_MEMBERS_EXACT=$(
    if ($memberDiff.Count -eq 0) { 'PASS' } else { 'FAIL' }
)"

if ($memberDiff.Count -ne 0) {
    throw "H3E2B1_ARCHIVE_MEMBERS_INVALID"
}

$oldLocation = Get-Location
try {
    Set-Location $extractDir

    $extractRun = Invoke-H3E2B1Native -FilePath $archiver -Arguments @(
        "x",
        $candidateArchive
    )
}
finally {
    Set-Location $oldLocation
}

if ($extractRun.ExitCode -ne 0) {
    throw "H3E2B1_ARCHIVE_EXTRACT_FAILED"
}

$sourceObjectByName = @{
    "JWPLC_TFT.cpp.o" = $jwplcObjects[0].FullName
    "TFT_eSPI.cpp.o" = $backendObjects[0].FullName
}

foreach ($member in $expectedMembers) {
    $extracted = Join-Path $extractDir $member

    if (-not (Test-Path -LiteralPath $extracted)) {
        throw "H3E2B1_EXTRACTED_MEMBER_MISSING=$member"
    }

    $sourceHash = Get-G2Sha256Path $sourceObjectByName[$member]
    $memberHash = Get-G2Sha256Path $extracted

    if ($sourceHash -ne $memberHash) {
        throw "H3E2B1_MEMBER_BYTE_PARITY_FAILED=$member"
    }
}

Write-Host "H3E2B1_ARCHIVE_MEMBER_BYTE_PARITY=PASS"

$archiveSha = Get-G2Sha256Path $candidateArchive
$archiveBytes = (Get-Item -LiteralPath $candidateArchive).Length

Write-Host "H3E2B1_CANDIDATE_LIBRARY_ROOT=$candidateLibraryRoot"
Write-Host "H3E2B1_CANDIDATE_ARCHIVE=$candidateArchive"
Write-Host "H3E2B1_CANDIDATE_ARCHIVE_BYTES=$archiveBytes"
Write-Host "H3E2B1_CANDIDATE_ARCHIVE_SHA256=$archiveSha"

# -------------------------------------------------------------------------
# 3) Construir libreria temporal release-like:
#    header publico + archive; cero source backend y cero depends TFT_eSPI.
# -------------------------------------------------------------------------
Copy-Item -LiteralPath $sourceHeader -Destination (Join-Path $candidateSrc "JWPLC_TFT.h") -Force

$candidatePropertiesText = @"
name=JWPLC_TFT
version=0.1.0
author=JW Control
maintainer=JW Control <jw.control.peru@gmail.com>
sentence=Backend grafico ST7789 encapsulado para el ecosistema JWPLC.
paragraph=JWPLC_TFT precompilado con backend privado calificado por JW Control.
category=Display
url=https://github.com/JW-Control/platform-jwplc
architectures=esp32
includes=JWPLC_TFT.h
dot_a_linkage=true
precompiled=full
depends=SPI
"@

[IO.File]::WriteAllText(
    $candidateProperties,
    $candidatePropertiesText,
    (New-Object Text.UTF8Encoding($false)))

[object[]]$candidateCppFiles = @(
    Get-ChildItem -LiteralPath $candidateSrc -Recurse -File -Filter "*.cpp" -ErrorAction SilentlyContinue
)

Write-Host "H3E2B1_CANDIDATE_CPP_COUNT=$($candidateCppFiles.Count)"

if ($candidateCppFiles.Count -ne 0) {
    throw "H3E2B1_CANDIDATE_SOURCE_CPP_PRESENT"
}

if ((Get-Content -LiteralPath $candidateProperties -Raw).Contains("TFT_eSPI")) {
    throw "H3E2B1_CANDIDATE_PROPERTIES_BACKEND_LEAK"
}

Write-Host "H3E2B1_CANDIDATE_DEPENDS_TFT_ESPI=NO"
Write-Host "H3E2B1_CANDIDATE_PRECOMPILED_FULL=YES"

# -------------------------------------------------------------------------
# 4) Compile con la libreria temporal. Debe ganar sobre la copia source
#    instalada y no debe seleccionar TFT_eSPI.
# -------------------------------------------------------------------------
$candidateArgs = @(
    "compile",
    "--fqbn", $fqbn,
    "-j", "0",
    "-v",
    "--clean",
    "--build-path", $candidateBuild,
    "--library", $candidateLibraryRoot,
    "--build-property", "compiler.cpp.extra_flags=$diagDefine",
    $sketchPath
)

Write-Host "H3E2B1_CANDIDATE_COMPILE_START=YES"
$candidateRun = Invoke-H3E2B1Native -FilePath $arduinoCli -Arguments $candidateArgs
$candidateRun.Output | Set-Content -LiteralPath $candidateLog -Encoding UTF8

Write-Host "H3E2B1_CANDIDATE_COMPILE_EXIT=$($candidateRun.ExitCode)"
Write-Host "H3E2B1_CANDIDATE_COMPILE_LOG=$candidateLog"

if ($candidateRun.ExitCode -ne 0) {
    $candidateRun.Output |
        Select-Object -Last 160 |
        ForEach-Object { Write-Host $_ }

    throw "H3E2B1_CANDIDATE_COMPILE_FAILED"
}

$normalizedCandidateRoot =
    [IO.Path]::GetFullPath($candidateLibraryRoot).TrimEnd('\', '/')

[string[]]$jwplcSelection = @(
    $candidateRun.Output |
        Where-Object {
            $_ -match '^Using library JWPLC_TFT at version .+ in folder: (.+)$'
        }
)

if ($jwplcSelection.Count -lt 1) {
    throw "H3E2B1_CANDIDATE_SELECTION_LINE_MISSING"
}

$selectedCandidate = $false

foreach ($line in $jwplcSelection) {
    if ($line -match '^Using library JWPLC_TFT at version .+ in folder: (.+)$') {
        $selectedPath = [IO.Path]::GetFullPath($Matches[1].Trim())

        if ($selectedPath.TrimEnd('\', '/') -ieq $normalizedCandidateRoot) {
            $selectedCandidate = $true
        }
    }
}

Write-Host "H3E2B1_CANDIDATE_LIBRARY_SELECTED=$(
    if ($selectedCandidate) { 'PASS' } else { 'FAIL' }
)"

if (-not $selectedCandidate) {
    $jwplcSelection | ForEach-Object { Write-Host "H3E2B1_SELECTION_LINE=$_" }
    throw "H3E2B1_TEMP_LIBRARY_NOT_SELECTED"
}

[string[]]$backendSelection = @(
    $candidateRun.Output |
        Where-Object {
            $_ -match '^Using library TFT_eSPI at version '
        }
)

Write-Host "H3E2B1_EXTERNAL_TFT_ESPI_SELECTION_COUNT=$($backendSelection.Count)"

if ($backendSelection.Count -ne 0) {
    $backendSelection | ForEach-Object { Write-Host "H3E2B1_TFT_ESPI_SELECTION=$_" }
    throw "H3E2B1_EXTERNAL_TFT_ESPI_SELECTED"
}

$candidateCompileDbPath = Join-Path $candidateBuild "compile_commands.json"

if (-not (Test-Path -LiteralPath $candidateCompileDbPath)) {
    throw "H3E2B1_CANDIDATE_COMPILE_DB_MISSING"
}

[object[]]$candidateCompileDbEntries = @(
    Get-Content -LiteralPath $candidateCompileDbPath -Raw |
        ConvertFrom-Json
)

[string[]]$candidateTuFiles = @(
    foreach ($entry in $candidateCompileDbEntries) {
        $fileText = [string]$entry.file

        if ([string]::IsNullOrWhiteSpace($fileText)) {
            continue
        }

        $fileText.
            Trim().
            Trim('"').
            Replace([char]92, [char]47)
    }
)

[int]$jwplcSourceCompiles = @(
    $candidateTuFiles |
        Where-Object {
            $_.EndsWith(
                "/JWPLC_TFT.cpp",
                [StringComparison]::OrdinalIgnoreCase)
        }
).Count

[int]$backendSourceCompiles = @(
    $candidateTuFiles |
        Where-Object {
            $_.EndsWith(
                "/TFT_eSPI.cpp",
                [StringComparison]::OrdinalIgnoreCase)
        }
).Count

Write-Host "H3E2B1_CANDIDATE_COMPILE_DB_ENTRY_COUNT=$($candidateCompileDbEntries.Count)"
Write-Host "H3E2B1_CANDIDATE_JWPLC_TFT_SOURCE_COMPILES=$jwplcSourceCompiles"
Write-Host "H3E2B1_CANDIDATE_TFT_ESPI_SOURCE_COMPILES=$backendSourceCompiles"
Write-Host "H3E2B1_SOURCE_COMPILE_CLASSIFICATION=COMPILE_DB_ENTRY_FILE"

if ($jwplcSourceCompiles -ne 0) {
    $candidateTuFiles |
        Where-Object {
            $_.EndsWith(
                "/JWPLC_TFT.cpp",
                [StringComparison]::OrdinalIgnoreCase)
        } |
        ForEach-Object {
            Write-Host "H3E2B1_UNEXPECTED_JWPLC_TFT_TU=$_"
        }

    throw "H3E2B1_JWPLC_TFT_SOURCE_COMPILED"
}

if ($backendSourceCompiles -ne 0) {
    $candidateTuFiles |
        Where-Object {
            $_.EndsWith(
                "/TFT_eSPI.cpp",
                [StringComparison]::OrdinalIgnoreCase)
        } |
        ForEach-Object {
            Write-Host "H3E2B1_UNEXPECTED_TFT_ESPI_TU=$_"
        }

    throw "H3E2B1_TFT_ESPI_SOURCE_COMPILED"
}

$precompiledObserved = @(
    $candidateRun.Output |
        Where-Object {
            ($_ -match 'Using precompiled library|Usando libreria precompilada|Usando biblioteca precompilada') -and
            ($_ -match 'JWPLC_TFT')
        }
).Count -gt 0

Write-Host "H3E2B1_PRECOMPILED_SELECTION_PROOF=$(
    if ($precompiledObserved) { 'PASS' } else { 'FAIL' }
)"

if (-not $precompiledObserved) {
    throw "H3E2B1_PRECOMPILED_SELECTION_NOT_PROVEN"
}

$sourceFlash = Get-H3E2B1Usage -Lines $sourceRun.Output -Kind "FLASH"
$sourceRam = Get-H3E2B1Usage -Lines $sourceRun.Output -Kind "RAM"
$candidateFlash = Get-H3E2B1Usage -Lines $candidateRun.Output -Kind "FLASH"
$candidateRam = Get-H3E2B1Usage -Lines $candidateRun.Output -Kind "RAM"

Write-Host "H3E2B1_SOURCE_FLASH_BYTES=$sourceFlash"
Write-Host "H3E2B1_CANDIDATE_FLASH_BYTES=$candidateFlash"
Write-Host "H3E2B1_SOURCE_RAM_BYTES=$sourceRam"
Write-Host "H3E2B1_CANDIDATE_RAM_BYTES=$candidateRam"

if ($sourceFlash -lt 0 -or
    $candidateFlash -lt 0 -or
    $sourceRam -lt 0 -or
    $candidateRam -lt 0) {
    throw "H3E2B1_USAGE_METRICS_MISSING"
}

$flashDelta = $candidateFlash - $sourceFlash
$ramDelta = $candidateRam - $sourceRam

Write-Host "H3E2B1_FLASH_DELTA_BYTES=$flashDelta"
Write-Host "H3E2B1_RAM_DELTA_BYTES=$ramDelta"

if ($ramDelta -ne 0) {
    throw "H3E2B1_RAM_PARITY_FAILED"
}

if ([Math]::Abs($flashDelta) -gt 64) {
    throw "H3E2B1_FLASH_PARITY_REVIEW_REQUIRED"
}

[string[]]$finalDirty = @(Get-G2TrackedDirtyPaths)
[string[]]$finalStaged = @(& git -C $script:G2RepoRoot diff --cached --name-only)
[string[]]$normalizedFinalDirty = @(
    $finalDirty |
        ForEach-Object { $_.Replace("\", "/") } |
        Sort-Object
)

if ($normalizedFinalDirty.Count -ne 1 -or $normalizedFinalDirty[0] -ne $expectedCoreDirty) {
    throw "H3E2B1_FINAL_DIRTY_SCOPE_INVALID"
}
if ($finalStaged.Count -ne 0) {
    throw "H3E2B1_FINAL_INDEX_NOT_CLEAN"
}

Write-Host "H3E2B1_EXTERNAL_TFT_ESPI_REQUIRED_AT_USER_BUILD=NO"
Write-Host "H3E2B1_PUBLIC_HEADER_BACKEND_LEAK=NO"
Write-Host "H3E2B1_ARCHIVE_SELF_CONTAINED=PASS"
Write-Host "H3E2B1_REPOSITORY_MUTATION=NO"
Write-Host "H3E2B1_UPLOADS=NO"
Write-Host "A14_H3E2B1_JWPLC_TFT_PRECOMPILED_CANDIDATE_GATE=PASS"
Write-Host "NEXT=RETURN_OUTPUT_TO_CHAT_FOR_H3E2B2_PRECOMPILED_PHYSICAL_GATE"
