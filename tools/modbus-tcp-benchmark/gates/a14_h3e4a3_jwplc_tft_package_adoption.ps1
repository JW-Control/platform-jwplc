param(
    [switch]$PreflightOnly
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"
. (Join-Path $PSScriptRoot "common.ps1")

Write-Host "============================================================"
Write-Host " A14 H3E.4A3 - ADOPCION JWPLC_TFT SHAPES"
Write-Host "============================================================"

Assert-G2Branch

$cli = "C:\Program Files\Arduino PLC IDE Tools\arduino-cli.exe"
$fqbn = "jwplc_local:esp32:jwplcbasic"
$diagDefine = "-DJWPLC_TFT_ESPI_DIAGNOSTIC_NO_DISPLAY_AUTOLOAD=1"

$physicalArchiveSha =
    "5D860A131811DD9A7EB6FA55F5674B1D78B0DE7DFAF8748CE18A60CEED2D3738"

$physicalAppSha =
    "0B9D8C5A217636187F2937651AFEE6EC3A898E1EE62461772564A9919316129A"

$physicalArchiveBytes = [int64]1091098
$physicalAppBytes = [int64]412800

$tftRoot =
    Get-G2Path "JWPLC/2.1.0/libraries/JWPLC_TFT"

$officialArchive =
    Join-Path $tftRoot "src\esp32\libJWPLC_TFT.a"

$repoLibraries =
    Get-G2Path "JWPLC/2.1.0/libraries"

$shapeSketch =
    Get-G2Path "tools/modbus-tcp-benchmark/firmware/a14_h3e4a1_jwplc_tft_shapes_probe"

$emptySketch =
    Get-G2Path "tools/build-speed-benchmark/sketches/01_empty"

foreach ($required in @(
    $cli,
    $tftRoot,
    $officialArchive,
    $repoLibraries,
    $shapeSketch,
    $emptySketch
)) {
    if (-not (Test-Path -LiteralPath $required)) {
        throw "H3E4A3_REQUIRED_PATH_MISSING=$required"
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

    throw "H3E4A3_ENTRY_DIRTY_SCOPE_INVALID"
}

if ($entryStaged.Count -ne 0) {
    throw "H3E4A3_ENTRY_INDEX_NOT_CLEAN"
}

$mode =
    if ($PreflightOnly) {
        "PREFLIGHT"
    }
    else {
        "ADOPT"
    }

Write-Host "HEAD=$(Get-G2Head)"
Write-Host "H3E4A3_MODE=$mode"

Write-Host ""
Write-Host "=== H3E4A3 RESOLVE H3E4A2 PHYSICAL ARTIFACT ==="

[object[]]$runs = @(
    Get-ChildItem -LiteralPath $env:TEMP -Directory -Filter "jwplc_a14_h3e4a1_*" -ErrorAction SilentlyContinue |
        Sort-Object LastWriteTime -Descending
)

$physicalRunRoot = $null
$candidateRoot = $null
$candidateArchive = $null
$physicalAppBin = $null

foreach ($run in $runs) {
    $root =
        Join-Path $run.FullName "candidate-libraries\JWPLC_TFT"

    $archive =
        Join-Path $root "src\esp32\libJWPLC_TFT.a"

    $app =
        Join-Path $run.FullName "candidate-build\a14_h3e4a1_jwplc_tft_shapes_probe.ino.bin"

    if (-not (Test-Path -LiteralPath $root) -or
        -not (Test-Path -LiteralPath $archive) -or
        -not (Test-Path -LiteralPath $app)) {
        continue
    }

    $archiveBytes =
        (Get-Item -LiteralPath $archive).Length

    $appBytes =
        (Get-Item -LiteralPath $app).Length

    if ($archiveBytes -ne $physicalArchiveBytes -or
        $appBytes -ne $physicalAppBytes) {
        continue
    }

    $archiveSha =
        (Get-G2Sha256Path $archive).ToUpperInvariant()

    if ($archiveSha -ne $physicalArchiveSha) {
        continue
    }

    $appSha =
        (Get-G2Sha256Path $app).ToUpperInvariant()

    if ($appSha -ne $physicalAppSha) {
        continue
    }

    $physicalRunRoot = $run.FullName
    $candidateRoot = $root
    $candidateArchive = $archive
    $physicalAppBin = $app
    break
}

if ([string]::IsNullOrWhiteSpace($physicalRunRoot)) {
    throw "H3E4A3_H3E4A2_PHYSICAL_ARTIFACT_NOT_FOUND"
}

$resolvedArchiveSha =
    (Get-G2Sha256Path $candidateArchive).ToUpperInvariant()

$resolvedAppSha =
    (Get-G2Sha256Path $physicalAppBin).ToUpperInvariant()

Write-Host "H3E4A3_H3E4A2_PHYSICAL_ARTIFACT=PASS"
Write-Host "H3E4A3_PHYSICAL_RUN_ROOT=$physicalRunRoot"
Write-Host "H3E4A3_PHYSICAL_TFT_ROOT=$candidateRoot"
Write-Host "H3E4A3_PHYSICAL_ARCHIVE=$candidateArchive"
Write-Host "H3E4A3_PHYSICAL_ARCHIVE_BYTES=$physicalArchiveBytes"
Write-Host "H3E4A3_PHYSICAL_ARCHIVE_SHA256=$resolvedArchiveSha"
Write-Host "H3E4A3_PHYSICAL_APP_BIN=$physicalAppBin"
Write-Host "H3E4A3_PHYSICAL_APP_BIN_BYTES=$physicalAppBytes"
Write-Host "H3E4A3_PHYSICAL_APP_BIN_SHA256=$resolvedAppSha"

function Invoke-H3E4A3Compile {
    param(
        [string]$LibraryRoot,
        [string]$SketchPath,
        [string]$BuildPath,
        [string]$LogPath,
        [string]$Label,
        [bool]$DiagnosticDisplayBypass
    )

    New-Item -ItemType Directory -Force -Path $BuildPath | Out-Null

    [string[]]$args = @(
        "compile",
        "--fqbn", $fqbn,
        "-j", "0",
        "-v",
        "--clean",
        "--build-path", $BuildPath,
        "--library", $LibraryRoot,
        "--libraries", $repoLibraries
    )

    if ($DiagnosticDisplayBypass) {
        $args += @(
            "--build-property",
            "compiler.cpp.extra_flags=$diagDefine"
        )
    }

    $args += $SketchPath

    Write-Host ""
    Write-Host ("=== {0} ===" -f $Label)

    $previousPreference =
        $ErrorActionPreference

    try {
        $ErrorActionPreference = "Continue"

        [object[]]$output =
            @(& $cli @args 2>&1)

        $exitCode =
            [int]$LASTEXITCODE
    }
    finally {
        $ErrorActionPreference =
            $previousPreference
    }

    $output |
        ForEach-Object {
            $_.ToString()
        } |
        Set-Content -LiteralPath $LogPath -Encoding UTF8

    Write-Host ("{0}_COMPILE_EXIT={1}" -f $Label, $exitCode)
    Write-Host ("{0}_COMPILE_LOG={1}" -f $Label, $LogPath)

    if ($exitCode -ne 0) {
        $output |
            Select-Object -Last 180 |
            ForEach-Object {
                Write-Host $_
            }

        throw ("{0}_COMPILE_FAILED" -f $Label)
    }

    return @(
        $output |
            ForEach-Object {
                $_.ToString()
            }
    )
}

function Assert-H3E4A3Build {
    param(
        [string[]]$Lines,
        [string]$BuildPath,
        [string]$ExpectedTftRoot,
        [string]$Label,
        [bool]$ExpectDisplay
    )

    $text =
        $Lines -join [Environment]::NewLine

    $normalizedExpected =
        [IO.Path]::GetFullPath(
            $ExpectedTftRoot
        ).TrimEnd('\', '/')

    $tftSelected = $false

    foreach ($line in $Lines) {
        if ($line -match '^Using library JWPLC_TFT at version .+ in folder: (.+)$') {
            $selected =
                [IO.Path]::GetFullPath(
                    $Matches[1].Trim()
                ).TrimEnd('\', '/')

            if ($selected -ieq $normalizedExpected) {
                $tftSelected = $true
            }
        }
    }

    $tftPrecompiled =
        @(
            $Lines |
                Where-Object {
                    ($_ -match 'Using precompiled library|Usando libreria precompilada|Usando biblioteca precompilada') -and
                    ($_ -match 'JWPLC_TFT')
                }
        ).Count -gt 0

    [string[]]$externalBackend = @(
        $Lines |
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

    [int]$backendSourceObjects = @(
        Get-ChildItem -LiteralPath $BuildPath -Recurse -File -ErrorAction SilentlyContinue |
            Where-Object {
                $_.Name -eq "TFT_eSPI.cpp.o"
            }
    ).Count

    $displaySelected =
        $text.Contains("Using library JWPLC_Display")

    $tftSelectedText =
        if ($tftSelected) {
            "PASS"
        }
        else {
            "FAIL"
        }

    $tftPrecompiledText =
        if ($tftPrecompiled) {
            "PASS"
        }
        else {
            "FAIL"
        }

    $displayText =
        if ($displaySelected) {
            "YES"
        }
        else {
            "NO"
        }

    Write-Host ("{0}_JWPLC_TFT_SELECTED={1}" -f $Label, $tftSelectedText)
    Write-Host ("{0}_JWPLC_TFT_PRECOMPILED={1}" -f $Label, $tftPrecompiledText)
    Write-Host ("{0}_EXTERNAL_TFT_ESPI_SELECTION_COUNT={1}" -f $Label, $externalBackend.Count)
    Write-Host ("{0}_JWPLC_TFT_SOURCE_OBJECT_COUNT={1}" -f $Label, $tftSourceObjects)
    Write-Host ("{0}_TFT_ESPI_SOURCE_OBJECT_COUNT={1}" -f $Label, $backendSourceObjects)
    Write-Host ("{0}_DISPLAY_SELECTED={1}" -f $Label, $displayText)

    if (-not $tftSelected -or
        -not $tftPrecompiled -or
        $externalBackend.Count -ne 0 -or
        $tftSourceObjects -ne 0 -or
        $backendSourceObjects -ne 0) {
        throw ("{0}_TFT_BUILD_POLICY_FAILED" -f $Label)
    }

    if ($ExpectDisplay -and
        -not $displaySelected) {
        throw ("{0}_DISPLAY_AUTOLOAD_MISSING" -f $Label)
    }

    if (-not $ExpectDisplay -and
        $displaySelected) {
        throw ("{0}_DISPLAY_DIAGNOSTIC_BYPASS_FAILED" -f $Label)
    }
}

$tempRoot =
    Join-Path $env:TEMP (
        "jwplc_a14_h3e4a3_{0}" -f
        (Get-Date -Format "yyyyMMdd_HHmmss")
    )

$tempShapesBuild =
    Join-Path $tempRoot "temp-shapes"

$tempShapesLog =
    Join-Path $tempRoot "temp-shapes.log"

$tempAutoloadBuild =
    Join-Path $tempRoot "temp-autoload"

$tempAutoloadLog =
    Join-Path $tempRoot "temp-autoload.log"

$tempShapesCompileArgs = @{
    LibraryRoot = $candidateRoot
    SketchPath = $shapeSketch
    BuildPath = $tempShapesBuild
    LogPath = $tempShapesLog
    Label = "H3E4A3_TEMP_SHAPES"
    DiagnosticDisplayBypass = $true
}

[string[]]$tempShapesOutput =
    @(Invoke-H3E4A3Compile @tempShapesCompileArgs)

$tempShapesAssertArgs = @{
    Lines = $tempShapesOutput
    BuildPath = $tempShapesBuild
    ExpectedTftRoot = $candidateRoot
    Label = "H3E4A3_TEMP_SHAPES"
    ExpectDisplay = $false
}

Assert-H3E4A3Build @tempShapesAssertArgs

$tempAutoloadCompileArgs = @{
    LibraryRoot = $candidateRoot
    SketchPath = $emptySketch
    BuildPath = $tempAutoloadBuild
    LogPath = $tempAutoloadLog
    Label = "H3E4A3_TEMP_AUTOLOAD"
    DiagnosticDisplayBypass = $false
}

[string[]]$tempAutoloadOutput =
    @(Invoke-H3E4A3Compile @tempAutoloadCompileArgs)

$tempAutoloadAssertArgs = @{
    Lines = $tempAutoloadOutput
    BuildPath = $tempAutoloadBuild
    ExpectedTftRoot = $candidateRoot
    Label = "H3E4A3_TEMP_AUTOLOAD"
    ExpectDisplay = $true
}

Assert-H3E4A3Build @tempAutoloadAssertArgs

Write-Host "H3E4A3_TEMP_RELEASE_LIKE=PASS"
Write-Host "H3E4A3_TEMP_SHAPES_LINK=PASS"
Write-Host "H3E4A3_TEMP_NORMAL_AUTOLOAD=PASS"

if ($PreflightOnly) {
    Write-Host "H3E4A3_PREFLIGHT_REPOSITORY_MUTATION=NO"
    Write-Host "A14_H3E4A3_JWPLC_TFT_PACKAGE_ADOPTION_PREFLIGHT=PASS"
    return
}

$originalArchive =
    [IO.File]::ReadAllBytes(
        $officialArchive
    )

$adoptionSucceeded = $false

try {
    Write-Host ""
    Write-Host "=== H3E4A3 ADOPT PHYSICAL ARCHIVE ==="

    Copy-Item -LiteralPath $candidateArchive -Destination $officialArchive -Force

    $officialSha =
        (Get-G2Sha256Path $officialArchive).ToUpperInvariant()

    $officialBytes =
        (Get-Item -LiteralPath $officialArchive).Length

    Write-Host "H3E4A3_OFFICIAL_ARCHIVE_BYTES=$officialBytes"
    Write-Host "H3E4A3_OFFICIAL_ARCHIVE_SHA256=$officialSha"

    if ($officialSha -ne $physicalArchiveSha -or
        $officialBytes -ne $physicalArchiveBytes) {
        throw "H3E4A3_OFFICIAL_ARCHIVE_IDENTITY_FAILED"
    }

    Write-Host "H3E4A3_ARCHIVE_IDENTITY_WITH_H3E4A2_PHYSICAL=PASS"

    $officialShapesBuild =
        Join-Path $tempRoot "official-shapes"

    $officialShapesLog =
        Join-Path $tempRoot "official-shapes.log"

    $officialAutoloadBuild =
        Join-Path $tempRoot "official-autoload"

    $officialAutoloadLog =
        Join-Path $tempRoot "official-autoload.log"

    $officialShapesCompileArgs = @{
        LibraryRoot = $tftRoot
        SketchPath = $shapeSketch
        BuildPath = $officialShapesBuild
        LogPath = $officialShapesLog
        Label = "H3E4A3_OFFICIAL_SHAPES"
        DiagnosticDisplayBypass = $true
    }

    [string[]]$officialShapesOutput =
        @(Invoke-H3E4A3Compile @officialShapesCompileArgs)

    $officialShapesAssertArgs = @{
        Lines = $officialShapesOutput
        BuildPath = $officialShapesBuild
        ExpectedTftRoot = $tftRoot
        Label = "H3E4A3_OFFICIAL_SHAPES"
        ExpectDisplay = $false
    }

    Assert-H3E4A3Build @officialShapesAssertArgs

    $officialAutoloadCompileArgs = @{
        LibraryRoot = $tftRoot
        SketchPath = $emptySketch
        BuildPath = $officialAutoloadBuild
        LogPath = $officialAutoloadLog
        Label = "H3E4A3_OFFICIAL_AUTOLOAD"
        DiagnosticDisplayBypass = $false
    }

    [string[]]$officialAutoloadOutput =
        @(Invoke-H3E4A3Compile @officialAutoloadCompileArgs)

    $officialAutoloadAssertArgs = @{
        Lines = $officialAutoloadOutput
        BuildPath = $officialAutoloadBuild
        ExpectedTftRoot = $tftRoot
        Label = "H3E4A3_OFFICIAL_AUTOLOAD"
        ExpectDisplay = $true
    }

    Assert-H3E4A3Build @officialAutoloadAssertArgs

    $officialShaAfter =
        (Get-G2Sha256Path $officialArchive).ToUpperInvariant()

    if ($officialShaAfter -ne $physicalArchiveSha) {
        throw "H3E4A3_OFFICIAL_ARCHIVE_MUTATED_DURING_BUILD"
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
        "JWPLC/2.1.0/libraries/JWPLC_TFT/src/esp32/libJWPLC_TFT.a"
    ) |
        Sort-Object

    [object[]]$dirtyDiff = @(
        Compare-Object -ReferenceObject $expectedDirty -DifferenceObject $normalizedFinalDirty
    )

    Write-Host "H3E4A3_FINAL_DIRTY_COUNT=$($normalizedFinalDirty.Count)"

    $normalizedFinalDirty |
        ForEach-Object {
            Write-Host "H3E4A3_FINAL_DIRTY=$_"
        }

    if ($dirtyDiff.Count -ne 0) {
        throw "H3E4A3_FINAL_DIRTY_SCOPE_INVALID"
    }

    if ($finalStaged.Count -ne 0) {
        throw "H3E4A3_FINAL_INDEX_NOT_CLEAN"
    }

    $adoptionSucceeded = $true

    Write-Host "H3E4A3_TFT_ARCHIVE=OFFICIAL_ADOPTED"
    Write-Host "H3E4A3_TFT_ARCHIVE_SOURCE=H3E4A2_PHYSICAL_QUALIFIED"
    Write-Host "H3E4A3_OFFICIAL_SHAPES_LINK=PASS"
    Write-Host "H3E4A3_OFFICIAL_NORMAL_AUTOLOAD=PASS"
    Write-Host "H3E4A3_EXTERNAL_TFT_ESPI_REQUIRED_AT_USER_BUILD=NO"
    Write-Host "H3E4A3_REPOSITORY_MUTATION=PRODUCT_ARCHIVE_ONLY"
    Write-Host "A14_H3E4A3_JWPLC_TFT_PACKAGE_ADOPTION_GATE=PASS"
    Write-Host "NEXT=REVIEW_DIFF_AND_COMMIT_H3E4A3_ARCHIVE"
}
finally {
    if (-not $adoptionSucceeded) {
        Write-Host "H3E4A3_ROLLBACK=START"

        [IO.File]::WriteAllBytes(
            $officialArchive,
            $originalArchive)

        Write-Host "H3E4A3_ROLLBACK=PASS"
    }
}
