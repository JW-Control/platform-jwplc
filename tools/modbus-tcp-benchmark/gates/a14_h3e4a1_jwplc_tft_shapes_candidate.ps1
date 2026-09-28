param()

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"
. (Join-Path $PSScriptRoot "common.ps1")

Write-Host "============================================================"
Write-Host " A14 H3E.4A1 - JWPLC_TFT SHAPES SOURCE / PRECOMPILED"
Write-Host "============================================================"

Assert-G2Branch

$arduinoCli = "C:\Program Files\Arduino PLC IDE Tools\arduino-cli.exe"
$fqbn = "jwplc_local:esp32:jwplcbasic"
$diagDefine = "-DJWPLC_TFT_ESPI_DIAGNOSTIC_NO_DISPLAY_AUTOLOAD=1"

$repoLibraries =
    Get-G2Path "JWPLC/2.1.0/libraries"

$officialRoot =
    Get-G2Path "JWPLC/2.1.0/libraries/JWPLC_TFT"

$headerPath =
    Join-Path $officialRoot "src\JWPLC_TFT.h"

$cppPath =
    Join-Path $officialRoot "src\JWPLC_TFT.cpp"

$setupPath =
    Join-Path $officialRoot "src\tft_setup.h"

$probePath =
    Get-G2Path "tools/modbus-tcp-benchmark/firmware/a14_h3e4a1_jwplc_tft_shapes_probe"

foreach ($required in @(
    $arduinoCli,
    $officialRoot,
    $headerPath,
    $cppPath,
    $setupPath,
    $probePath
)) {
    if (-not (Test-Path -LiteralPath $required)) {
        throw "H3E4A1_REQUIRED_PATH_MISSING=$required"
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

    throw "H3E4A1_ENTRY_DIRTY_SCOPE_INVALID"
}

if ($entryStaged.Count -ne 0) {
    throw "H3E4A1_ENTRY_INDEX_NOT_CLEAN"
}

$headerText =
    [IO.File]::ReadAllText($headerPath)

$cppText =
    [IO.File]::ReadAllText($cppPath)

foreach ($forbidden in @(
    "TFT_eSPI",
    "Adafruit_"
)) {
    if ($headerText.Contains($forbidden)) {
        throw "H3E4A1_PUBLIC_BACKEND_LEAK=$forbidden"
    }
}

foreach ($contract in @(
    "bool fillRoundRect(",
    "bool drawRoundRect(",
    "bool fillCircle(",
    "bool drawCircle("
)) {
    $count =
        ([regex]::Matches(
            $headerText,
            [regex]::Escape($contract)
        )).Count

    Write-Host "H3E4A1_PUBLIC_CONTRACT=$contract COUNT=$count"

    if ($count -ne 1) {
        throw "H3E4A1_PUBLIC_CONTRACT_INVALID=$contract"
    }
}

foreach ($contract in @(
    "g_backend.fillRoundRect(",
    "g_backend.drawRoundRect(",
    "g_backend.fillCircle(",
    "g_backend.drawCircle("
)) {
    $count =
        ([regex]::Matches(
            $cppText,
            [regex]::Escape($contract)
        )).Count

    Write-Host "H3E4A1_BACKEND_CONTRACT=$contract COUNT=$count"

    if ($count -ne 1) {
        throw "H3E4A1_BACKEND_CONTRACT_INVALID=$contract"
    }
}

Write-Host "HEAD=$(Get-G2Head)"
Write-Host "H3E4A1_PUBLIC_BACKEND_LEAK=NO"
Write-Host "H3E4A1_API_CONTRACT=PASS"
Write-Host "H3E4A1_REPOSITORY_MUTATION=NO"
Write-Host "H3E4A1_UPLOADS=NO"

$runRoot =
    Join-Path $env:TEMP (
        "jwplc_a14_h3e4a1_{0}" -f
        (Get-Date -Format "yyyyMMdd_HHmmss")
    )

$sourceRoot =
    Join-Path $runRoot "source-libraries\JWPLC_TFT"

$sourceSrc =
    Join-Path $sourceRoot "src"

$sourceBuild =
    Join-Path $runRoot "source-build"

$sourceLog =
    Join-Path $runRoot "source.log"

$candidateRoot =
    Join-Path $runRoot "candidate-libraries\JWPLC_TFT"

$candidateSrc =
    Join-Path $candidateRoot "src"

$candidateArchiveDir =
    Join-Path $candidateSrc "esp32"

$candidateArchive =
    Join-Path $candidateArchiveDir "libJWPLC_TFT.a"

$candidateBuild =
    Join-Path $runRoot "candidate-build"

$candidateLog =
    Join-Path $runRoot "candidate.log"

$extractDir =
    Join-Path $runRoot "archive-members"

foreach ($dir in @(
    $sourceSrc,
    $sourceBuild,
    $candidateArchiveDir,
    $candidateBuild,
    $extractDir
)) {
    New-Item -ItemType Directory -Force -Path $dir | Out-Null
}

Copy-Item -LiteralPath $headerPath -Destination (Join-Path $sourceSrc "JWPLC_TFT.h") -Force
Copy-Item -LiteralPath $cppPath -Destination (Join-Path $sourceSrc "JWPLC_TFT.cpp") -Force
Copy-Item -LiteralPath $setupPath -Destination (Join-Path $sourceSrc "tft_setup.h") -Force

$utf8NoBom =
    [Text.UTF8Encoding]::new($false)

$sourceProperties = @'
name=JWPLC_TFT
version=0.1.0
author=JW Control
maintainer=JW Control <jw.control.peru@gmail.com>
sentence=JWPLC_TFT H3E4A1 source qualification.
paragraph=Temporary source-only qualification library.
category=Display
architectures=esp32
includes=JWPLC_TFT.h
depends=TFT_eSPI,SPI
'@

[IO.File]::WriteAllText(
    (Join-Path $sourceRoot "library.properties"),
    $sourceProperties,
    $utf8NoBom)

function Invoke-H3E4A1Native {
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

function Resolve-H3E4A1Archiver {
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

    throw "H3E4A1_ARCHIVER_NOT_FOUND"
}

function Get-H3E4A1Usage {
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

function Get-H3E4A1SingleFile {
    param(
        [string]$Root,
        [string]$Filter,
        [string]$Kind
    )

    [object[]]$files = @(
        Get-ChildItem -LiteralPath $Root -File -Filter $Filter -ErrorAction SilentlyContinue
    )

    if ($files.Count -ne 1) {
        throw ("H3E4A1_{0}_COUNT_INVALID={1}" -f $Kind, $files.Count)
    }

    return $files[0].FullName
}

function Get-H3E4A1Symbols {
    param(
        [string]$NmPath,
        [string]$ElfPath
    )

    $run =
        Invoke-H3E4A1Native -FilePath $NmPath -Arguments @(
            "-S",
            "--defined-only",
            $ElfPath
        )

    if ($run.ExitCode -ne 0) {
        throw "H3E4A1_NM_FAILED"
    }

    [string[]]$symbols = @(
        foreach ($line in $run.Output) {
            $trimmed =
                $line.Trim()

            if ($trimmed -match '^[0-9A-Fa-f]+\s+(?<size>[0-9A-Fa-f]+)\s+(?<type>\S)\s+(?<name>.+)
        }
    )

    return @(
        $symbols |
            Sort-Object
    )
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
    "--build-property", "compiler.cpp.extra_flags=$diagDefine",
    $probePath
)

Write-Host ""
Write-Host "=== H3E4A1 SOURCE BUILD ==="
Write-Host "H3E4A1_SOURCE_COMPILE_START=YES"

$sourceRun =
    Invoke-H3E4A1Native -FilePath $arduinoCli -Arguments $sourceArgs

$sourceRun.Output |
    Set-Content -LiteralPath $sourceLog -Encoding UTF8

Write-Host "H3E4A1_SOURCE_COMPILE_EXIT=$($sourceRun.ExitCode)"
Write-Host "H3E4A1_SOURCE_COMPILE_LOG=$sourceLog"

if ($sourceRun.ExitCode -ne 0) {
    $sourceRun.Output |
        Select-Object -Last 160 |
        ForEach-Object {
            Write-Host $_
        }

    throw "H3E4A1_SOURCE_COMPILE_FAILED"
}

$sourceText =
    $sourceRun.Output -join [Environment]::NewLine

if (-not $sourceText.Contains("Using library TFT_eSPI at version 2.5.43")) {
    throw "H3E4A1_TFT_ESPI_2_5_43_NOT_SELECTED"
}

$normalizedSourceRoot =
    [IO.Path]::GetFullPath($sourceRoot).TrimEnd('\', '/')

$sourceSelected =
    $false

foreach ($line in $sourceRun.Output) {
    if ($line -match '^Using library JWPLC_TFT at version .+ in folder: (.+)$') {
        $selected =
            [IO.Path]::GetFullPath(
                $Matches[1].Trim()
            ).TrimEnd('\', '/')

        if ($selected -ieq $normalizedSourceRoot) {
            $sourceSelected =
                $true
        }
    }
}

Write-Host "H3E4A1_SOURCE_LIBRARY_SELECTED=$(if ($sourceSelected) { 'PASS' } else { 'FAIL' })"

if (-not $sourceSelected) {
    throw "H3E4A1_SOURCE_LIBRARY_NOT_SELECTED"
}

[object[]]$jwplcObjects = @(
    Get-ChildItem -LiteralPath $sourceBuild -Recurse -File -ErrorAction SilentlyContinue |
        Where-Object {
            $_.Name -eq "JWPLC_TFT.cpp.o"
        }
)

[object[]]$backendObjects = @(
    Get-ChildItem -LiteralPath $sourceBuild -Recurse -File -ErrorAction SilentlyContinue |
        Where-Object {
            $_.Name -eq "TFT_eSPI.cpp.o"
        }
)

Write-Host "H3E4A1_SOURCE_JWPLC_TFT_OBJECT_COUNT=$($jwplcObjects.Count)"
Write-Host "H3E4A1_SOURCE_TFT_ESPI_OBJECT_COUNT=$($backendObjects.Count)"

if ($jwplcObjects.Count -ne 1 -or
    $backendObjects.Count -ne 1) {
    throw "H3E4A1_SOURCE_OBJECT_COUNT_INVALID"
}

$archiver =
    Resolve-H3E4A1Archiver -Lines $sourceRun.Output

Write-Host "H3E4A1_ARCHIVER=$archiver"

$archiveRun =
    Invoke-H3E4A1Native -FilePath $archiver -Arguments @(
        "crs",
        $candidateArchive,
        $jwplcObjects[0].FullName,
        $backendObjects[0].FullName
    )

if ($archiveRun.ExitCode -ne 0 -or
    -not (Test-Path -LiteralPath $candidateArchive)) {
    throw "H3E4A1_ARCHIVE_CREATE_FAILED"
}

$listRun =
    Invoke-H3E4A1Native -FilePath $archiver -Arguments @(
        "t",
        $candidateArchive
    )

if ($listRun.ExitCode -ne 0) {
    throw "H3E4A1_ARCHIVE_LIST_FAILED"
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

[string[]]$expectedMembers = @(
    "JWPLC_TFT.cpp.o",
    "TFT_eSPI.cpp.o"
)

[object[]]$memberDiff = @(
    Compare-Object -ReferenceObject $expectedMembers -DifferenceObject $members
)

Write-Host "H3E4A1_ARCHIVE_MEMBER_COUNT=$($members.Count)"

$members |
    ForEach-Object {
        Write-Host "H3E4A1_ARCHIVE_MEMBER=$_"
    }

if ($memberDiff.Count -ne 0) {
    throw "H3E4A1_ARCHIVE_MEMBER_SET_FAILED"
}

$oldLocation =
    Get-Location

try {
    Set-Location $extractDir

    $extractRun =
        Invoke-H3E4A1Native -FilePath $archiver -Arguments @(
            "x",
            $candidateArchive
        )
}
finally {
    Set-Location $oldLocation
}

if ($extractRun.ExitCode -ne 0) {
    throw "H3E4A1_ARCHIVE_EXTRACT_FAILED"
}

$sourceObjectMap = @{
    "JWPLC_TFT.cpp.o" = $jwplcObjects[0].FullName
    "TFT_eSPI.cpp.o" = $backendObjects[0].FullName
}

foreach ($member in $expectedMembers) {
    $memberPath =
        Join-Path $extractDir $member

    if (-not (Test-Path -LiteralPath $memberPath)) {
        throw "H3E4A1_MEMBER_MISSING=$member"
    }

    if ((Get-G2Sha256Path $sourceObjectMap[$member]) -ne
        (Get-G2Sha256Path $memberPath)) {
        throw "H3E4A1_MEMBER_BYTE_PARITY_FAILED=$member"
    }
}

Write-Host "H3E4A1_ARCHIVE_MEMBER_BYTE_PARITY=PASS"

Copy-Item -LiteralPath $headerPath -Destination (Join-Path $candidateSrc "JWPLC_TFT.h") -Force

$candidateProperties = @'
name=JWPLC_TFT
version=0.1.0
author=JW Control
maintainer=JW Control <jw.control.peru@gmail.com>
sentence=Backend grafico ST7789 encapsulado para el ecosistema JWPLC.
paragraph=JWPLC_TFT precompilado con backend privado calificado por JW Control.
category=Display
architectures=esp32
includes=JWPLC_TFT.h
dot_a_linkage=true
precompiled=full
depends=SPI
'@

[IO.File]::WriteAllText(
    (Join-Path $candidateRoot "library.properties"),
    $candidateProperties,
    $utf8NoBom)

$archiveSha =
    (Get-G2Sha256Path $candidateArchive).ToUpperInvariant()

$archiveBytes =
    (Get-Item -LiteralPath $candidateArchive).Length

Write-Host "H3E4A1_CANDIDATE_LIBRARY_ROOT=$candidateRoot"
Write-Host "H3E4A1_CANDIDATE_ARCHIVE=$candidateArchive"
Write-Host "H3E4A1_CANDIDATE_ARCHIVE_BYTES=$archiveBytes"
Write-Host "H3E4A1_CANDIDATE_ARCHIVE_SHA256=$archiveSha"

$candidateArgs = @(
    "compile",
    "--fqbn", $fqbn,
    "-j", "0",
    "-v",
    "--clean",
    "--build-path", $candidateBuild,
    "--library", $candidateRoot,
    "--libraries", $repoLibraries,
    "--build-property", "compiler.cpp.extra_flags=$diagDefine",
    $probePath
)

Write-Host ""
Write-Host "=== H3E4A1 PRECOMPILED CANDIDATE BUILD ==="
Write-Host "H3E4A1_CANDIDATE_COMPILE_START=YES"

$candidateRun =
    Invoke-H3E4A1Native -FilePath $arduinoCli -Arguments $candidateArgs

$candidateRun.Output |
    Set-Content -LiteralPath $candidateLog -Encoding UTF8

Write-Host "H3E4A1_CANDIDATE_COMPILE_EXIT=$($candidateRun.ExitCode)"
Write-Host "H3E4A1_CANDIDATE_COMPILE_LOG=$candidateLog"

if ($candidateRun.ExitCode -ne 0) {
    $candidateRun.Output |
        Select-Object -Last 160 |
        ForEach-Object {
            Write-Host $_
        }

    throw "H3E4A1_CANDIDATE_COMPILE_FAILED"
}

$normalizedCandidateRoot =
    [IO.Path]::GetFullPath($candidateRoot).TrimEnd('\', '/')

$candidateSelected =
    $false

foreach ($line in $candidateRun.Output) {
    if ($line -match '^Using library JWPLC_TFT at version .+ in folder: (.+)$') {
        $selected =
            [IO.Path]::GetFullPath(
                $Matches[1].Trim()
            ).TrimEnd('\', '/')

        if ($selected -ieq $normalizedCandidateRoot) {
            $candidateSelected =
                $true
        }
    }
}

Write-Host "H3E4A1_CANDIDATE_LIBRARY_SELECTED=$(if ($candidateSelected) { 'PASS' } else { 'FAIL' })"

if (-not $candidateSelected) {
    throw "H3E4A1_CANDIDATE_LIBRARY_NOT_SELECTED"
}

[string[]]$externalBackend = @(
    $candidateRun.Output |
        Where-Object {
            $_ -match '^Using library TFT_eSPI at version '
        }
)

Write-Host "H3E4A1_EXTERNAL_TFT_ESPI_SELECTION_COUNT=$($externalBackend.Count)"

if ($externalBackend.Count -ne 0) {
    throw "H3E4A1_EXTERNAL_TFT_ESPI_SELECTED"
}

$compileDbPath =
    Join-Path $candidateBuild "compile_commands.json"

if (-not (Test-Path -LiteralPath $compileDbPath)) {
    throw "H3E4A1_CANDIDATE_COMPILE_DB_MISSING"
}

[object[]]$compileDb =
    @(Get-Content -LiteralPath $compileDbPath -Raw | ConvertFrom-Json)

[string[]]$tuFiles = @(
    foreach ($entry in $compileDb) {
        $file =
            [string]$entry.file

        if (-not [string]::IsNullOrWhiteSpace($file)) {
            $file.Trim().Trim('"').Replace([char]92, [char]47)
        }
    }
)

[int]$jwplcSourceCompiles = @(
    $tuFiles |
        Where-Object {
            $_.EndsWith(
                "/JWPLC_TFT.cpp",
                [StringComparison]::OrdinalIgnoreCase)
        }
).Count

[int]$backendSourceCompiles = @(
    $tuFiles |
        Where-Object {
            $_.EndsWith(
                "/TFT_eSPI.cpp",
                [StringComparison]::OrdinalIgnoreCase)
        }
).Count

Write-Host "H3E4A1_CANDIDATE_JWPLC_TFT_SOURCE_COMPILES=$jwplcSourceCompiles"
Write-Host "H3E4A1_CANDIDATE_TFT_ESPI_SOURCE_COMPILES=$backendSourceCompiles"

if ($jwplcSourceCompiles -ne 0 -or
    $backendSourceCompiles -ne 0) {
    throw "H3E4A1_PRECOMPILED_SOURCE_POLICY_FAILED"
}

$precompiledObserved = @(
    $candidateRun.Output |
        Where-Object {
            ($_ -match 'Using precompiled library|Usando libreria precompilada|Usando biblioteca precompilada') -and
            ($_ -match 'JWPLC_TFT')
        }
).Count -gt 0

Write-Host "H3E4A1_PRECOMPILED_SELECTION_PROOF=$(if ($precompiledObserved) { 'PASS' } else { 'FAIL' })"

if (-not $precompiledObserved) {
    throw "H3E4A1_PRECOMPILED_SELECTION_NOT_PROVEN"
}

$sourceFlash =
    Get-H3E4A1Usage -Lines $sourceRun.Output -Kind "FLASH"

$candidateFlash =
    Get-H3E4A1Usage -Lines $candidateRun.Output -Kind "FLASH"

$sourceRam =
    Get-H3E4A1Usage -Lines $sourceRun.Output -Kind "RAM"

$candidateRam =
    Get-H3E4A1Usage -Lines $candidateRun.Output -Kind "RAM"

$flashDelta =
    $candidateFlash - $sourceFlash

$ramDelta =
    $candidateRam - $sourceRam

Write-Host "H3E4A1_SOURCE_FLASH_BYTES=$sourceFlash"
Write-Host "H3E4A1_CANDIDATE_FLASH_BYTES=$candidateFlash"
Write-Host "H3E4A1_FLASH_DELTA_BYTES=$flashDelta"
Write-Host "H3E4A1_SOURCE_RAM_BYTES=$sourceRam"
Write-Host "H3E4A1_CANDIDATE_RAM_BYTES=$candidateRam"
Write-Host "H3E4A1_RAM_DELTA_BYTES=$ramDelta"

if ($sourceFlash -lt 0 -or
    $candidateFlash -lt 0 -or
    $sourceRam -lt 0 -or
    $candidateRam -lt 0) {
    throw "H3E4A1_USAGE_METRICS_MISSING"
}

if ($ramDelta -ne 0) {
    throw "H3E4A1_RAM_PARITY_FAILED"
}

$toolDir =
    Split-Path -Parent $archiver

$nm =
    $null

foreach ($name in @(
    "xtensa-esp32-elf-nm.exe",
    "xtensa-esp32-elf-nm"
)) {
    $candidate =
        Join-Path $toolDir $name

    if (Test-Path -LiteralPath $candidate) {
        $nm =
            (Resolve-Path -LiteralPath $candidate).Path
        break
    }
}

if ([string]::IsNullOrWhiteSpace($nm)) {
    throw "H3E4A1_NM_NOT_FOUND"
}

$sourceElf =
    Get-H3E4A1SingleFile -Root $sourceBuild -Filter "*.elf" -Kind "SOURCE_ELF"

$candidateElf =
    Get-H3E4A1SingleFile -Root $candidateBuild -Filter "*.elf" -Kind "CANDIDATE_ELF"

[string[]]$sourceSymbols =
    @(Get-H3E4A1Symbols -NmPath $nm -ElfPath $sourceElf)

[string[]]$candidateSymbols =
    @(Get-H3E4A1Symbols -NmPath $nm -ElfPath $candidateElf)

[object[]]$symbolDiff = @(
    Compare-Object -ReferenceObject $sourceSymbols -DifferenceObject $candidateSymbols
)

Write-Host "H3E4A1_SOURCE_DEFINED_SYMBOL_COUNT=$($sourceSymbols.Count)"
Write-Host "H3E4A1_CANDIDATE_DEFINED_SYMBOL_COUNT=$($candidateSymbols.Count)"
Write-Host "H3E4A1_DEFINED_SYMBOL_NAME_TYPE_SIZE_PARITY=$(if ($symbolDiff.Count -eq 0) { 'PASS' } else { 'FAIL' })"

if ($symbolDiff.Count -ne 0) {
    $symbolDiff |
        Select-Object -First 40 |
        ForEach-Object {
            Write-Host "H3E4A1_SYMBOL_DIFF=$($_.SideIndicator):$($_.InputObject)"
        }

    throw "H3E4A1_SYMBOL_PARITY_FAILED"
}

$flashClass =
    if ($flashDelta -eq 0) {
        "EXACT"
    }
    else {
        "LINK_LAYOUT_ONLY"
    }

Write-Host "H3E4A1_FLASH_DELTA_CLASSIFICATION=$flashClass"
Write-Host "H3E4A1_STRUCTURAL_EQUIVALENCE=PASS"

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

if ($normalizedFinalDirty.Count -ne 1 -or
    $normalizedFinalDirty[0] -ne $expectedCoreDirty) {
    throw "H3E4A1_FINAL_DIRTY_SCOPE_INVALID"
}

if ($finalStaged.Count -ne 0) {
    throw "H3E4A1_FINAL_INDEX_NOT_CLEAN"
}

Write-Host "H3E4A1_NEW_PRIMITIVE_COUNT=4"
Write-Host "H3E4A1_SOURCE_ONLY_BUILD=PASS"
Write-Host "H3E4A1_ARCHIVE_SELF_CONTAINED=PASS"
Write-Host "H3E4A1_EXTERNAL_TFT_ESPI_REQUIRED_AT_USER_BUILD=NO"
Write-Host "H3E4A1_REPOSITORY_MUTATION=NO"
Write-Host "H3E4A1_UPLOADS=NO"
Write-Host "A14_H3E4A1_JWPLC_TFT_SHAPES_CANDIDATE_GATE=PASS"
Write-Host "NEXT=RETURN_OUTPUT_TO_CHAT_FOR_H3E4A2_PHYSICAL"
) {
                $size =
                    $Matches["size"].ToUpperInvariant()

                $type =
                    $Matches["type"]

                $name =
                    $Matches["name"]

                "{0}|{1}|{2}" -f $size, $type, $name
            }
        }
    )

    return @(
        $symbols |
            Sort-Object
    )
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
    "--build-property", "compiler.cpp.extra_flags=$diagDefine",
    $probePath
)

Write-Host ""
Write-Host "=== H3E4A1 SOURCE BUILD ==="
Write-Host "H3E4A1_SOURCE_COMPILE_START=YES"

$sourceRun =
    Invoke-H3E4A1Native -FilePath $arduinoCli -Arguments $sourceArgs

$sourceRun.Output |
    Set-Content -LiteralPath $sourceLog -Encoding UTF8

Write-Host "H3E4A1_SOURCE_COMPILE_EXIT=$($sourceRun.ExitCode)"
Write-Host "H3E4A1_SOURCE_COMPILE_LOG=$sourceLog"

if ($sourceRun.ExitCode -ne 0) {
    $sourceRun.Output |
        Select-Object -Last 160 |
        ForEach-Object {
            Write-Host $_
        }

    throw "H3E4A1_SOURCE_COMPILE_FAILED"
}

$sourceText =
    $sourceRun.Output -join [Environment]::NewLine

if (-not $sourceText.Contains("Using library TFT_eSPI at version 2.5.43")) {
    throw "H3E4A1_TFT_ESPI_2_5_43_NOT_SELECTED"
}

$normalizedSourceRoot =
    [IO.Path]::GetFullPath($sourceRoot).TrimEnd('\', '/')

$sourceSelected =
    $false

foreach ($line in $sourceRun.Output) {
    if ($line -match '^Using library JWPLC_TFT at version .+ in folder: (.+)$') {
        $selected =
            [IO.Path]::GetFullPath(
                $Matches[1].Trim()
            ).TrimEnd('\', '/')

        if ($selected -ieq $normalizedSourceRoot) {
            $sourceSelected =
                $true
        }
    }
}

Write-Host "H3E4A1_SOURCE_LIBRARY_SELECTED=$(if ($sourceSelected) { 'PASS' } else { 'FAIL' })"

if (-not $sourceSelected) {
    throw "H3E4A1_SOURCE_LIBRARY_NOT_SELECTED"
}

[object[]]$jwplcObjects = @(
    Get-ChildItem -LiteralPath $sourceBuild -Recurse -File -ErrorAction SilentlyContinue |
        Where-Object {
            $_.Name -eq "JWPLC_TFT.cpp.o"
        }
)

[object[]]$backendObjects = @(
    Get-ChildItem -LiteralPath $sourceBuild -Recurse -File -ErrorAction SilentlyContinue |
        Where-Object {
            $_.Name -eq "TFT_eSPI.cpp.o"
        }
)

Write-Host "H3E4A1_SOURCE_JWPLC_TFT_OBJECT_COUNT=$($jwplcObjects.Count)"
Write-Host "H3E4A1_SOURCE_TFT_ESPI_OBJECT_COUNT=$($backendObjects.Count)"

if ($jwplcObjects.Count -ne 1 -or
    $backendObjects.Count -ne 1) {
    throw "H3E4A1_SOURCE_OBJECT_COUNT_INVALID"
}

$archiver =
    Resolve-H3E4A1Archiver -Lines $sourceRun.Output

Write-Host "H3E4A1_ARCHIVER=$archiver"

$archiveRun =
    Invoke-H3E4A1Native -FilePath $archiver -Arguments @(
        "crs",
        $candidateArchive,
        $jwplcObjects[0].FullName,
        $backendObjects[0].FullName
    )

if ($archiveRun.ExitCode -ne 0 -or
    -not (Test-Path -LiteralPath $candidateArchive)) {
    throw "H3E4A1_ARCHIVE_CREATE_FAILED"
}

$listRun =
    Invoke-H3E4A1Native -FilePath $archiver -Arguments @(
        "t",
        $candidateArchive
    )

if ($listRun.ExitCode -ne 0) {
    throw "H3E4A1_ARCHIVE_LIST_FAILED"
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

[string[]]$expectedMembers = @(
    "JWPLC_TFT.cpp.o",
    "TFT_eSPI.cpp.o"
)

[object[]]$memberDiff = @(
    Compare-Object -ReferenceObject $expectedMembers -DifferenceObject $members
)

Write-Host "H3E4A1_ARCHIVE_MEMBER_COUNT=$($members.Count)"

$members |
    ForEach-Object {
        Write-Host "H3E4A1_ARCHIVE_MEMBER=$_"
    }

if ($memberDiff.Count -ne 0) {
    throw "H3E4A1_ARCHIVE_MEMBER_SET_FAILED"
}

$oldLocation =
    Get-Location

try {
    Set-Location $extractDir

    $extractRun =
        Invoke-H3E4A1Native -FilePath $archiver -Arguments @(
            "x",
            $candidateArchive
        )
}
finally {
    Set-Location $oldLocation
}

if ($extractRun.ExitCode -ne 0) {
    throw "H3E4A1_ARCHIVE_EXTRACT_FAILED"
}

$sourceObjectMap = @{
    "JWPLC_TFT.cpp.o" = $jwplcObjects[0].FullName
    "TFT_eSPI.cpp.o" = $backendObjects[0].FullName
}

foreach ($member in $expectedMembers) {
    $memberPath =
        Join-Path $extractDir $member

    if (-not (Test-Path -LiteralPath $memberPath)) {
        throw "H3E4A1_MEMBER_MISSING=$member"
    }

    if ((Get-G2Sha256Path $sourceObjectMap[$member]) -ne
        (Get-G2Sha256Path $memberPath)) {
        throw "H3E4A1_MEMBER_BYTE_PARITY_FAILED=$member"
    }
}

Write-Host "H3E4A1_ARCHIVE_MEMBER_BYTE_PARITY=PASS"

Copy-Item -LiteralPath $headerPath -Destination (Join-Path $candidateSrc "JWPLC_TFT.h") -Force

$candidateProperties = @'
name=JWPLC_TFT
version=0.1.0
author=JW Control
maintainer=JW Control <jw.control.peru@gmail.com>
sentence=Backend grafico ST7789 encapsulado para el ecosistema JWPLC.
paragraph=JWPLC_TFT precompilado con backend privado calificado por JW Control.
category=Display
architectures=esp32
includes=JWPLC_TFT.h
dot_a_linkage=true
precompiled=full
depends=SPI
'@

[IO.File]::WriteAllText(
    (Join-Path $candidateRoot "library.properties"),
    $candidateProperties,
    $utf8NoBom)

$archiveSha =
    (Get-G2Sha256Path $candidateArchive).ToUpperInvariant()

$archiveBytes =
    (Get-Item -LiteralPath $candidateArchive).Length

Write-Host "H3E4A1_CANDIDATE_LIBRARY_ROOT=$candidateRoot"
Write-Host "H3E4A1_CANDIDATE_ARCHIVE=$candidateArchive"
Write-Host "H3E4A1_CANDIDATE_ARCHIVE_BYTES=$archiveBytes"
Write-Host "H3E4A1_CANDIDATE_ARCHIVE_SHA256=$archiveSha"

$candidateArgs = @(
    "compile",
    "--fqbn", $fqbn,
    "-j", "0",
    "-v",
    "--clean",
    "--build-path", $candidateBuild,
    "--library", $candidateRoot,
    "--libraries", $repoLibraries,
    "--build-property", "compiler.cpp.extra_flags=$diagDefine",
    $probePath
)

Write-Host ""
Write-Host "=== H3E4A1 PRECOMPILED CANDIDATE BUILD ==="
Write-Host "H3E4A1_CANDIDATE_COMPILE_START=YES"

$candidateRun =
    Invoke-H3E4A1Native -FilePath $arduinoCli -Arguments $candidateArgs

$candidateRun.Output |
    Set-Content -LiteralPath $candidateLog -Encoding UTF8

Write-Host "H3E4A1_CANDIDATE_COMPILE_EXIT=$($candidateRun.ExitCode)"
Write-Host "H3E4A1_CANDIDATE_COMPILE_LOG=$candidateLog"

if ($candidateRun.ExitCode -ne 0) {
    $candidateRun.Output |
        Select-Object -Last 160 |
        ForEach-Object {
            Write-Host $_
        }

    throw "H3E4A1_CANDIDATE_COMPILE_FAILED"
}

$normalizedCandidateRoot =
    [IO.Path]::GetFullPath($candidateRoot).TrimEnd('\', '/')

$candidateSelected =
    $false

foreach ($line in $candidateRun.Output) {
    if ($line -match '^Using library JWPLC_TFT at version .+ in folder: (.+)$') {
        $selected =
            [IO.Path]::GetFullPath(
                $Matches[1].Trim()
            ).TrimEnd('\', '/')

        if ($selected -ieq $normalizedCandidateRoot) {
            $candidateSelected =
                $true
        }
    }
}

Write-Host "H3E4A1_CANDIDATE_LIBRARY_SELECTED=$(if ($candidateSelected) { 'PASS' } else { 'FAIL' })"

if (-not $candidateSelected) {
    throw "H3E4A1_CANDIDATE_LIBRARY_NOT_SELECTED"
}

[string[]]$externalBackend = @(
    $candidateRun.Output |
        Where-Object {
            $_ -match '^Using library TFT_eSPI at version '
        }
)

Write-Host "H3E4A1_EXTERNAL_TFT_ESPI_SELECTION_COUNT=$($externalBackend.Count)"

if ($externalBackend.Count -ne 0) {
    throw "H3E4A1_EXTERNAL_TFT_ESPI_SELECTED"
}

$compileDbPath =
    Join-Path $candidateBuild "compile_commands.json"

if (-not (Test-Path -LiteralPath $compileDbPath)) {
    throw "H3E4A1_CANDIDATE_COMPILE_DB_MISSING"
}

[object[]]$compileDb =
    @(Get-Content -LiteralPath $compileDbPath -Raw | ConvertFrom-Json)

[string[]]$tuFiles = @(
    foreach ($entry in $compileDb) {
        $file =
            [string]$entry.file

        if (-not [string]::IsNullOrWhiteSpace($file)) {
            $file.Trim().Trim('"').Replace([char]92, [char]47)
        }
    }
)

[int]$jwplcSourceCompiles = @(
    $tuFiles |
        Where-Object {
            $_.EndsWith(
                "/JWPLC_TFT.cpp",
                [StringComparison]::OrdinalIgnoreCase)
        }
).Count

[int]$backendSourceCompiles = @(
    $tuFiles |
        Where-Object {
            $_.EndsWith(
                "/TFT_eSPI.cpp",
                [StringComparison]::OrdinalIgnoreCase)
        }
).Count

Write-Host "H3E4A1_CANDIDATE_JWPLC_TFT_SOURCE_COMPILES=$jwplcSourceCompiles"
Write-Host "H3E4A1_CANDIDATE_TFT_ESPI_SOURCE_COMPILES=$backendSourceCompiles"

if ($jwplcSourceCompiles -ne 0 -or
    $backendSourceCompiles -ne 0) {
    throw "H3E4A1_PRECOMPILED_SOURCE_POLICY_FAILED"
}

$precompiledObserved = @(
    $candidateRun.Output |
        Where-Object {
            ($_ -match 'Using precompiled library|Usando libreria precompilada|Usando biblioteca precompilada') -and
            ($_ -match 'JWPLC_TFT')
        }
).Count -gt 0

Write-Host "H3E4A1_PRECOMPILED_SELECTION_PROOF=$(if ($precompiledObserved) { 'PASS' } else { 'FAIL' })"

if (-not $precompiledObserved) {
    throw "H3E4A1_PRECOMPILED_SELECTION_NOT_PROVEN"
}

$sourceFlash =
    Get-H3E4A1Usage -Lines $sourceRun.Output -Kind "FLASH"

$candidateFlash =
    Get-H3E4A1Usage -Lines $candidateRun.Output -Kind "FLASH"

$sourceRam =
    Get-H3E4A1Usage -Lines $sourceRun.Output -Kind "RAM"

$candidateRam =
    Get-H3E4A1Usage -Lines $candidateRun.Output -Kind "RAM"

$flashDelta =
    $candidateFlash - $sourceFlash

$ramDelta =
    $candidateRam - $sourceRam

Write-Host "H3E4A1_SOURCE_FLASH_BYTES=$sourceFlash"
Write-Host "H3E4A1_CANDIDATE_FLASH_BYTES=$candidateFlash"
Write-Host "H3E4A1_FLASH_DELTA_BYTES=$flashDelta"
Write-Host "H3E4A1_SOURCE_RAM_BYTES=$sourceRam"
Write-Host "H3E4A1_CANDIDATE_RAM_BYTES=$candidateRam"
Write-Host "H3E4A1_RAM_DELTA_BYTES=$ramDelta"

if ($sourceFlash -lt 0 -or
    $candidateFlash -lt 0 -or
    $sourceRam -lt 0 -or
    $candidateRam -lt 0) {
    throw "H3E4A1_USAGE_METRICS_MISSING"
}

if ($ramDelta -ne 0) {
    throw "H3E4A1_RAM_PARITY_FAILED"
}

$toolDir =
    Split-Path -Parent $archiver

$nm =
    $null

foreach ($name in @(
    "xtensa-esp32-elf-nm.exe",
    "xtensa-esp32-elf-nm"
)) {
    $candidate =
        Join-Path $toolDir $name

    if (Test-Path -LiteralPath $candidate) {
        $nm =
            (Resolve-Path -LiteralPath $candidate).Path
        break
    }
}

if ([string]::IsNullOrWhiteSpace($nm)) {
    throw "H3E4A1_NM_NOT_FOUND"
}

$sourceElf =
    Get-H3E4A1SingleFile -Root $sourceBuild -Filter "*.elf" -Kind "SOURCE_ELF"

$candidateElf =
    Get-H3E4A1SingleFile -Root $candidateBuild -Filter "*.elf" -Kind "CANDIDATE_ELF"

[string[]]$sourceSymbols =
    @(Get-H3E4A1Symbols -NmPath $nm -ElfPath $sourceElf)

[string[]]$candidateSymbols =
    @(Get-H3E4A1Symbols -NmPath $nm -ElfPath $candidateElf)

[object[]]$symbolDiff = @(
    Compare-Object -ReferenceObject $sourceSymbols -DifferenceObject $candidateSymbols
)

Write-Host "H3E4A1_SOURCE_DEFINED_SYMBOL_COUNT=$($sourceSymbols.Count)"
Write-Host "H3E4A1_CANDIDATE_DEFINED_SYMBOL_COUNT=$($candidateSymbols.Count)"
Write-Host "H3E4A1_DEFINED_SYMBOL_NAME_TYPE_SIZE_PARITY=$(if ($symbolDiff.Count -eq 0) { 'PASS' } else { 'FAIL' })"

if ($symbolDiff.Count -ne 0) {
    $symbolDiff |
        Select-Object -First 40 |
        ForEach-Object {
            Write-Host "H3E4A1_SYMBOL_DIFF=$($_.SideIndicator):$($_.InputObject)"
        }

    throw "H3E4A1_SYMBOL_PARITY_FAILED"
}

$flashClass =
    if ($flashDelta -eq 0) {
        "EXACT"
    }
    else {
        "LINK_LAYOUT_ONLY"
    }

Write-Host "H3E4A1_FLASH_DELTA_CLASSIFICATION=$flashClass"
Write-Host "H3E4A1_STRUCTURAL_EQUIVALENCE=PASS"

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

if ($normalizedFinalDirty.Count -ne 1 -or
    $normalizedFinalDirty[0] -ne $expectedCoreDirty) {
    throw "H3E4A1_FINAL_DIRTY_SCOPE_INVALID"
}

if ($finalStaged.Count -ne 0) {
    throw "H3E4A1_FINAL_INDEX_NOT_CLEAN"
}

Write-Host "H3E4A1_NEW_PRIMITIVE_COUNT=4"
Write-Host "H3E4A1_SOURCE_ONLY_BUILD=PASS"
Write-Host "H3E4A1_ARCHIVE_SELF_CONTAINED=PASS"
Write-Host "H3E4A1_EXTERNAL_TFT_ESPI_REQUIRED_AT_USER_BUILD=NO"
Write-Host "H3E4A1_REPOSITORY_MUTATION=NO"
Write-Host "H3E4A1_UPLOADS=NO"
Write-Host "A14_H3E4A1_JWPLC_TFT_SHAPES_CANDIDATE_GATE=PASS"
Write-Host "NEXT=RETURN_OUTPUT_TO_CHAT_FOR_H3E4A2_PHYSICAL"
