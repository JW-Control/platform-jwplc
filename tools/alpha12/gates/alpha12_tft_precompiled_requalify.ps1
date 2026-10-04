param(
    [string]$ArduinoCli = "C:\Program Files\Arduino PLC IDE Tools\arduino-cli.exe",
    [string]$Fqbn = "jwplc_local:esp32:jwplcbasic",
    [string]$ResultRoot = ""
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$repo = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot "..\..\.."))
$expectedBranch = "v2.1.0-alpha.12/feature/modbus-tcp"
$diagDefine = "-DJWPLC_TFT_ESPI_DIAGNOSTIC_NO_DISPLAY_AUTOLOAD=1"

$libraryRel = "JWPLC/2.1.0/libraries/JWPLC_TFT"
$libraryRoot = Join-Path $repo $libraryRel
$srcRoot = Join-Path $libraryRoot "src"
$headerRel = "$libraryRel/src/JWPLC_TFT.h"
$cppRel = "$libraryRel/src/JWPLC_TFT.cpp"
$setupRel = "$libraryRel/src/tft_setup.h"
$archiveRel = "$libraryRel/src/esp32/libJWPLC_TFT.a"
$headerPath = Join-Path $repo $headerRel
$cppPath = Join-Path $repo $cppRel
$setupPath = Join-Path $repo $setupRel
$archivePath = Join-Path $repo $archiveRel
$repoLibraries = Join-Path $repo "JWPLC\2.1.0\libraries"
$shapeSketch = Join-Path $repo "tools\modbus-tcp-benchmark\firmware\a14_h3e4a1_jwplc_tft_shapes_probe"
$displaySketch = Join-Path $repo "JWPLC\2.1.0\libraries\JWPLC_Display\examples\04.Display_TFT_Direct"
$emptySketch = Join-Path $repo "tools\build-speed-benchmark\sketches\01_empty"

$qualifiedArchiveSha = "5d860a131811dd9a7eb6fa55f5674b1d78b0de7dfaf8748ce18a60ceed2d3738"
$qualifiedArchiveBytes = [int64]1091098
$expectedHeaderBlob = "c24373eddaff5f148092c49fe997fb59f94bdbdc"
$expectedCppBlob = "2bdb55d504cfd2a1481ff561d33535d1740bb472"
$expectedSetupBlob = "773f8123844f783bfc67c3123150c27f29f45d34"

[string[]]$expectedMembers = @(
    "JWPLC_TFT.cpp.o",
    "TFT_eSPI.cpp.o"
) | Sort-Object

function Get-Sha256Lower {
    param([string]$Path)

    $stream = [System.IO.File]::OpenRead($Path)
    $sha256 = [System.Security.Cryptography.SHA256]::Create()

    try {
        return (
            [System.BitConverter]::ToString(
                $sha256.ComputeHash($stream)
            ).Replace("-", "").ToLowerInvariant()
        )
    }
    finally {
        $stream.Dispose()
        $sha256.Dispose()
    }
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

function Resolve-ToolSibling {
    param(
        [string[]]$Lines,
        [string]$Leaf
    )

    foreach ($line in $Lines) {
        if ($line -match '"(?<exe>[^"]*xtensa-esp32-elf-g\+\+(?:\.exe)?)"') {
            $toolDir = Split-Path -Parent $Matches["exe"]
            foreach ($name in @($Leaf + ".exe", $Leaf)) {
                $candidate = Join-Path $toolDir $name
                if (Test-Path -LiteralPath $candidate) {
                    return (Resolve-Path -LiteralPath $candidate).Path
                }
            }
        }
    }

    throw ("A12_TFT_REQUAL_TOOL_NOT_FOUND=" + $Leaf)
}

function Resolve-ToolBesidePath {
    param(
        [string]$KnownToolPath,
        [string]$Leaf
    )

    if ([string]::IsNullOrWhiteSpace($KnownToolPath)) {
        throw ("A12_TFT_REQUAL_KNOWN_TOOL_PATH_EMPTY=" + $Leaf)
    }

    $toolDir = Split-Path -Parent $KnownToolPath

    foreach ($name in @($Leaf + ".exe", $Leaf)) {
        $candidate = Join-Path $toolDir $name
        if (Test-Path -LiteralPath $candidate) {
            return (Resolve-Path -LiteralPath $candidate).Path
        }
    }

    throw ("A12_TFT_REQUAL_SIBLING_TOOL_NOT_FOUND=" + $Leaf + " base=" + $toolDir)
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

    return Resolve-ToolSibling -Lines $Lines -Leaf "xtensa-esp32-elf-gcc-ar"
}

function Assert-LibrarySelected {
    param(
        [string[]]$Lines,
        [string]$LibraryName,
        [string]$ExpectedRoot,
        [string]$Label
    )

    $normalizedExpected = [IO.Path]::GetFullPath($ExpectedRoot).TrimEnd('\', '/')
    $selected = $false

    foreach ($line in $Lines) {
        if ($line -match ("^Using library " + [regex]::Escape($LibraryName) + " at version .+ in folder: (.+)$")) {
            $actual = [IO.Path]::GetFullPath($Matches[1].Trim()).TrimEnd('\', '/')
            if ($actual -ieq $normalizedExpected) {
                $selected = $true
            }
        }
    }

    Write-Host ($Label + "_" + $LibraryName + "_SELECTED=" + [string]$selected)

    if (-not $selected) {
        throw ("A12_TFT_REQUAL_" + $Label + "_" + $LibraryName + "_NOT_SELECTED")
    }
}

function Get-NamedObjects {
    param(
        [string]$BuildPath,
        [string]$ObjectName
    )

    return @(
        Get-ChildItem -LiteralPath $BuildPath -Recurse -File -ErrorAction SilentlyContinue |
            Where-Object { $_.Name -eq $ObjectName }
    )
}

function Test-PrecompiledMarker {
    param(
        [string[]]$Lines,
        [string]$LibraryName
    )

    return @(
        $Lines | Where-Object {
            ($_ -match 'Using precompiled library|Usando libreria precompilada|Usando biblioteca precompilada') -and
            ($_ -match [regex]::Escape($LibraryName))
        }
    ).Count -gt 0
}

function Get-TftEspiSelection {
    param([string[]]$Lines)

    $selectionLines = @(
        $Lines | Where-Object {
            $_ -match '^Using library TFT_eSPI at version .+ in folder: .+$'
        }
    )

    if ($selectionLines.Count -eq 0) {
        return $null
    }

    if ($selectionLines.Count -ne 1) {
        throw "A12_TFT_REQUAL_TFT_ESPI_SELECTION_AMBIGUOUS"
    }

    $selectionLine = $selectionLines[0]
    if ($selectionLine -notmatch '^Using library TFT_eSPI at version (?<version>\S+) in folder: (?<folder>.+)$') {
        throw "A12_TFT_REQUAL_TFT_ESPI_SELECTION_PARSE_FAILED"
    }

    return [pscustomobject]@{
        Version = $Matches["version"].Trim()
        Folder = $Matches["folder"].Trim()
        Line = $selectionLine
    }
}

function Get-Usage {
    param(
        [string[]]$Lines,
        [string]$Kind
    )

    foreach ($line in $Lines) {
        if ($Kind -eq "FLASH" -and $line -match '(?:Sketch uses|El Sketch usa)\s+(?<n>\d+)\s+bytes') {
            return [int64]$Matches["n"]
        }

        if ($Kind -eq "RAM" -and $line -match '(?:Global variables use|Las variables Globales usan)\s+(?<n>\d+)\s+bytes') {
            return [int64]$Matches["n"]
        }
    }

    return [int64]-1
}

function Get-SingleElf {
    param(
        [string]$BuildPath,
        [string]$Label
    )

    [object[]]$files = @(
        Get-ChildItem -LiteralPath $BuildPath -File -Filter "*.elf" -ErrorAction SilentlyContinue
    )

    if ($files.Count -ne 1) {
        throw ("A12_TFT_REQUAL_" + $Label + "_ELF_COUNT_INVALID=" + [string]$files.Count)
    }

    return $files[0].FullName
}

function Get-DefinedSymbols {
    param(
        [string]$NmPath,
        [string]$ElfPath
    )

    $run = Invoke-NativeCaptured -FilePath $NmPath -Arguments @(
        "-S",
        "--defined-only",
        $ElfPath
    )

    if ($run.ExitCode -ne 0) {
        throw "A12_TFT_REQUAL_NM_FAILED"
    }

    [string[]]$symbols = @(
        foreach ($line in $run.Output) {
            $trimmed = $line.Trim()
            if ($trimmed -match '^[0-9A-Fa-f]+\s+(?<size>[0-9A-Fa-f]+)\s+(?<type>\S)\s+(?<name>.+)$') {
                ($Matches["size"].ToUpperInvariant() + "|" + $Matches["type"] + "|" + $Matches["name"])
            }
        }
    )

    return @($symbols | Sort-Object)
}

function Invoke-CandidateCase {
    param(
        [string]$Label,
        [string]$SketchPath,
        [string]$CandidateRoot,
        [string]$BuildPath,
        [string]$LogPath,
        [bool]$DiagnosticDisplayBypass
    )

    Write-Host ""
    Write-Host ("=== CANDIDATE CASE " + $Label + " ===")

    [string[]]$compileArgs = @(
        "compile",
        "--fqbn", $Fqbn,
        "-j", "0",
        "-v",
        "--clean",
        "--build-path", $BuildPath,
        "--library", $CandidateRoot,
        "--libraries", $repoLibraries
    )

    if ($DiagnosticDisplayBypass) {
        $compileArgs += @(
            "--build-property",
            ("compiler.cpp.extra_flags=" + $diagDefine)
        )
    }

    $compileArgs += $SketchPath

    $run = Invoke-NativeCaptured -FilePath $ArduinoCli -Arguments $compileArgs

    $run.Output | Set-Content -LiteralPath $LogPath -Encoding UTF8
    Write-Host ($Label + "_COMPILE_EXIT=" + [string]$run.ExitCode)

    if ($run.ExitCode -ne 0) {
        $run.Output | Select-Object -Last 180 | ForEach-Object { Write-Host $_ }
        throw ("A12_TFT_REQUAL_" + $Label + "_COMPILE_FAILED")
    }

    Assert-LibrarySelected -Lines $run.Output -LibraryName "JWPLC_TFT" -ExpectedRoot $CandidateRoot -Label $Label

    $precompiled = Test-PrecompiledMarker -Lines $run.Output -LibraryName "JWPLC_TFT"
    $tftObjects = @(Get-NamedObjects -BuildPath $BuildPath -ObjectName "JWPLC_TFT.cpp.o")
    $backendObjects = @(Get-NamedObjects -BuildPath $BuildPath -ObjectName "TFT_eSPI.cpp.o")
    $tftEspiSelection = Get-TftEspiSelection -Lines $run.Output
    $warningCount = @($run.Output | Where-Object { $_ -match '(?i)\bwarning:' }).Count
    $errorCount = @($run.Output | Where-Object { $_ -match '(?i)\berror:' }).Count

    Write-Host ($Label + "_TFT_SOURCE_OBJECT_COUNT=" + [string]$tftObjects.Count)
    Write-Host ($Label + "_TFT_ESPI_SOURCE_OBJECT_COUNT=" + [string]$backendObjects.Count)
    Write-Host ($Label + "_TFT_PRECOMPILED_MARKER=" + $(if ($precompiled) { "YES" } else { "NO" }))
    Write-Host ($Label + "_GLOBAL_TFT_ESPI_SELECTED=" + $(if ($null -eq $tftEspiSelection) { "NO" } else { "YES" }))
    Write-Host ($Label + "_WARNING_LINES=" + [string]$warningCount)
    Write-Host ($Label + "_ERROR_LINES=" + [string]$errorCount)

    if (-not $precompiled) {
        throw ("A12_TFT_REQUAL_" + $Label + "_PRECOMPILED_MARKER_MISSING")
    }
    if ($tftObjects.Count -ne 0 -or $backendObjects.Count -ne 0) {
        throw ("A12_TFT_REQUAL_" + $Label + "_SOURCE_RECOMPILED")
    }
    if ($null -ne $tftEspiSelection) {
        Write-Host ($Label + "_TFT_ESPI_SELECTION_LINE=" + $tftEspiSelection.Line)
        throw ("A12_TFT_REQUAL_" + $Label + "_GLOBAL_TFT_ESPI_DEPENDENCY")
    }
    if ($warningCount -ne 0 -or $errorCount -ne 0) {
        throw ("A12_TFT_REQUAL_" + $Label + "_COMPILER_DIAGNOSTICS")
    }

    return [pscustomobject]@{
        Label = $Label
        Lines = $run.Output
        BuildPath = $BuildPath
        WarningCount = $warningCount
        ErrorCount = $errorCount
    }
}

$branch = (& git -C $repo branch --show-current).Trim()
$head = (& git -C $repo rev-parse HEAD).Trim()

if ($branch -ne $expectedBranch) {
    throw "A12_TFT_REQUAL_BRANCH_MISMATCH"
}

[string[]]$entryDirty = @(& git -C $repo diff --name-only)
[string[]]$entryStaged = @(& git -C $repo diff --cached --name-only)

if ($entryDirty.Count -ne 0) {
    $entryDirty | ForEach-Object { Write-Host ("ENTRY_DIRTY=" + $_) }
    throw "A12_TFT_REQUAL_TRACKED_TREE_NOT_CLEAN"
}
if ($entryStaged.Count -ne 0) {
    throw "A12_TFT_REQUAL_INDEX_NOT_CLEAN"
}

& git -C $repo diff --check
if ($LASTEXITCODE -ne 0) {
    throw "A12_TFT_REQUAL_GIT_DIFF_CHECK_FAILED"
}

foreach ($required in @(
    $libraryRoot,
    $headerPath,
    $cppPath,
    $setupPath,
    $archivePath,
    $repoLibraries,
    $shapeSketch,
    $displaySketch,
    $emptySketch
)) {
    if (-not (Test-Path -LiteralPath $required)) {
        throw ("A12_TFT_REQUAL_REQUIRED_PATH_MISSING=" + $required)
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
    $cliCommand = Get-Command "arduino-cli" -ErrorAction SilentlyContinue
    if ($null -ne $cliCommand) {
        $ArduinoCli = $cliCommand.Source
    }
}

if (-not (Test-Path -LiteralPath $ArduinoCli)) {
    throw "A12_TFT_REQUAL_ARDUINO_CLI_NOT_FOUND"
}

$headerBlob = (& git -C $repo hash-object -- $headerRel).Trim()
$cppBlob = (& git -C $repo hash-object -- $cppRel).Trim()
$setupBlob = (& git -C $repo hash-object -- $setupRel).Trim()

Write-Host ("CURRENT_HEADER_BLOB=" + $headerBlob)
Write-Host ("CURRENT_CPP_BLOB=" + $cppBlob)
Write-Host ("CURRENT_SETUP_BLOB=" + $setupBlob)

if ($headerBlob -ne $expectedHeaderBlob -or $cppBlob -ne $expectedCppBlob -or $setupBlob -ne $expectedSetupBlob) {
    throw "A12_TFT_REQUAL_SOURCE_BLOB_IDENTITY_CHANGED"
}

Write-Host "CURRENT_SOURCE_BLOB_IDENTITY=PASS"

$archiveSha = Get-Sha256Lower $archivePath
$archiveBytes = (Get-Item -LiteralPath $archivePath).Length

Write-Host ("ARCHIVE_BYTES=" + [string]$archiveBytes)
Write-Host ("ARCHIVE_SHA256=" + $archiveSha)

if ($archiveSha -ne $qualifiedArchiveSha -or $archiveBytes -ne $qualifiedArchiveBytes) {
    throw "A12_TFT_REQUAL_PHYSICAL_ARCHIVE_IDENTITY_CHANGED"
}

Write-Host "ARCHIVE_IDENTITY_PHYSICAL_QUALIFIED=PASS"

$stamp = Get-Date -Format "yyyyMMdd_HHmmss"
if ([string]::IsNullOrWhiteSpace($ResultRoot)) {
    $ResultRoot = Join-Path $repo "tools\alpha12\results\tft_precompiled_requalify_$stamp"
}
New-Item -ItemType Directory -Force -Path $ResultRoot | Out-Null

$runRoot = Join-Path $env:TEMP ("jwplc_a12_tft_requal_" + $stamp)
$sourceRoot = Join-Path $runRoot "source-libraries\JWPLC_TFT"
$sourceSrc = Join-Path $sourceRoot "src"
$sourceBuild = Join-Path $runRoot "source-build"
$sourceLog = Join-Path $ResultRoot "source.log"
$extractDir = Join-Path $runRoot "archive-members"
$candidateRoot = Join-Path $runRoot "candidate-libraries\JWPLC_TFT"
$candidateSrc = Join-Path $candidateRoot "src"
$candidateArchiveDir = Join-Path $candidateSrc "esp32"
$directBuild = Join-Path $runRoot "candidate-direct-build"
$displayBuild = Join-Path $runRoot "candidate-display-build"
$autoloadBuild = Join-Path $runRoot "candidate-autoload-build"

foreach ($dir in @(
    $sourceSrc,
    $sourceBuild,
    $extractDir,
    $candidateArchiveDir,
    $directBuild,
    $displayBuild,
    $autoloadBuild
)) {
    New-Item -ItemType Directory -Force -Path $dir | Out-Null
}

$utf8NoBom = [Text.UTF8Encoding]::new($false)

Copy-Item -LiteralPath $headerPath -Destination (Join-Path $sourceSrc "JWPLC_TFT.h") -Force
Copy-Item -LiteralPath $cppPath -Destination (Join-Path $sourceSrc "JWPLC_TFT.cpp") -Force
Copy-Item -LiteralPath $setupPath -Destination (Join-Path $sourceSrc "tft_setup.h") -Force

$sourceProperties = @'
name=JWPLC_TFT
version=0.1.0-alpha12-source
author=JW Control
maintainer=JW Control
sentence=Alpha12 source qualification for JWPLC_TFT.
paragraph=Temporary source-only qualification library matching historical H3E4 recipe.
category=Display
architectures=esp32
includes=JWPLC_TFT.h
depends=TFT_eSPI,SPI
'@
[IO.File]::WriteAllText((Join-Path $sourceRoot "library.properties"), $sourceProperties, $utf8NoBom)

Copy-Item -LiteralPath $headerPath -Destination (Join-Path $candidateSrc "JWPLC_TFT.h") -Force
Copy-Item -LiteralPath $archivePath -Destination (Join-Path $candidateArchiveDir "libJWPLC_TFT.a") -Force

$candidateProperties = @'
name=JWPLC_TFT
version=0.1.0-alpha12-requal
author=JW Control
maintainer=JW Control
sentence=Alpha12 self-contained precompiled TFT requalification.
paragraph=Temporary release-like qualification library with no backend source files.
category=Display
architectures=esp32
includes=JWPLC_TFT.h
dot_a_linkage=true
precompiled=full
depends=SPI
'@
[IO.File]::WriteAllText((Join-Path $candidateRoot "library.properties"), $candidateProperties, $utf8NoBom)

@(
    "DATE=$(Get-Date -Format o)"
    "BRANCH=$branch"
    "HEAD=$head"
    "ARCHIVE_BYTES=$archiveBytes"
    "ARCHIVE_SHA256=$archiveSha"
    "QUALIFIED_ARCHIVE_BYTES=$qualifiedArchiveBytes"
    "QUALIFIED_ARCHIVE_SHA256=$qualifiedArchiveSha"
    "HEADER_BLOB=$headerBlob"
    "CPP_BLOB=$cppBlob"
    "SETUP_BLOB=$setupBlob"
    "FQBN=$Fqbn"
    "ARDUINO_CLI=$ArduinoCli"
    "HISTORICAL_DIAG_DEFINE=$diagDefine"
) | Set-Content -LiteralPath (Join-Path $ResultRoot "MANIFEST.txt") -Encoding UTF8

Write-Host "=============================================================================="
Write-Host " ALPHA12 - JWPLC_TFT PRECOMPILED REQUALIFICATION R3"
Write-Host "=============================================================================="
Write-Host ("BRANCH=" + $branch)
Write-Host ("HEAD=" + $head)
Write-Host ("RESULT_ROOT=" + $ResultRoot)

Write-Host ""
Write-Host "=== SOURCE-FIRST HISTORICAL-RECIPE BUILD ==="

$sourceRun = Invoke-NativeCaptured -FilePath $ArduinoCli -Arguments @(
    "compile",
    "--fqbn", $Fqbn,
    "-j", "0",
    "-v",
    "--clean",
    "--build-path", $sourceBuild,
    "--library", $sourceRoot,
    "--libraries", $repoLibraries,
    "--build-property", ("compiler.cpp.extra_flags=" + $diagDefine),
    $shapeSketch
)

$sourceRun.Output | Set-Content -LiteralPath $sourceLog -Encoding UTF8
Write-Host ("SOURCE_COMPILE_EXIT=" + [string]$sourceRun.ExitCode)

if ($sourceRun.ExitCode -ne 0) {
    $sourceRun.Output | Select-Object -Last 220 | ForEach-Object { Write-Host $_ }
    throw "A12_TFT_REQUAL_SOURCE_COMPILE_FAILED"
}

Assert-LibrarySelected -Lines $sourceRun.Output -LibraryName "JWPLC_TFT" -ExpectedRoot $sourceRoot -Label "SOURCE"

$sourcePrecompiled = Test-PrecompiledMarker -Lines $sourceRun.Output -LibraryName "JWPLC_TFT"
$sourceTftObjects = @(Get-NamedObjects -BuildPath $sourceBuild -ObjectName "JWPLC_TFT.cpp.o")
$sourceBackendObjects = @(Get-NamedObjects -BuildPath $sourceBuild -ObjectName "TFT_eSPI.cpp.o")
$tftEspiSelection = Get-TftEspiSelection -Lines $sourceRun.Output
$sourceWarnings = @($sourceRun.Output | Where-Object { $_ -match '(?i)\bwarning:' }).Count
$sourceErrors = @($sourceRun.Output | Where-Object { $_ -match '(?i)\berror:' }).Count

Write-Host ("SOURCE_TFT_OBJECT_COUNT=" + [string]$sourceTftObjects.Count)
Write-Host ("SOURCE_TFT_ESPI_OBJECT_COUNT=" + [string]$sourceBackendObjects.Count)
Write-Host ("SOURCE_TFT_PRECOMPILED_MARKER=" + $(if ($sourcePrecompiled) { "YES" } else { "NO" }))
Write-Host ("SOURCE_WARNING_LINES=" + [string]$sourceWarnings)
Write-Host ("SOURCE_ERROR_LINES=" + [string]$sourceErrors)

if ($sourcePrecompiled) {
    throw "A12_TFT_REQUAL_SOURCE_UNEXPECTED_PRECOMPILED"
}
if ($sourceTftObjects.Count -ne 1 -or $sourceBackendObjects.Count -ne 1) {
    throw "A12_TFT_REQUAL_SOURCE_OBJECT_SET_INVALID"
}
if ($null -eq $tftEspiSelection) {
    throw "A12_TFT_REQUAL_SOURCE_TFT_ESPI_NOT_SELECTED"
}
if ($tftEspiSelection.Version -ne "2.5.43") {
    Write-Host ("SOURCE_TFT_ESPI_VERSION=" + $tftEspiSelection.Version)
    Write-Host ("SOURCE_TFT_ESPI_FOLDER=" + $tftEspiSelection.Folder)
    throw "A12_TFT_REQUAL_MAINTAINER_TFT_ESPI_VERSION_MISMATCH"
}
if ($sourceWarnings -ne 0 -or $sourceErrors -ne 0) {
    throw "A12_TFT_REQUAL_SOURCE_COMPILER_DIAGNOSTICS"
}

Write-Host ("SOURCE_TFT_ESPI_VERSION=" + $tftEspiSelection.Version)
Write-Host ("SOURCE_TFT_ESPI_FOLDER=" + $tftEspiSelection.Folder)
Write-Host "SOURCE_FIRST_CURRENT_SOURCE=PASS"

$archiver = Resolve-Archiver -Lines $sourceRun.Output
$nm = Resolve-ToolBesidePath -KnownToolPath $archiver -Leaf "xtensa-esp32-elf-nm"
Write-Host ("ARCHIVER=" + $archiver)
Write-Host ("NM=" + $nm)

Write-Host ""
Write-Host "=== OFFICIAL ARCHIVE STRUCTURE ==="

$listRun = Invoke-NativeCaptured -FilePath $archiver -Arguments @("t", $archivePath)
if ($listRun.ExitCode -ne 0) {
    throw "A12_TFT_REQUAL_ARCHIVE_LIST_FAILED"
}

[string[]]$members = @(
    $listRun.Output |
        ForEach-Object { $_.Trim() } |
        Where-Object { -not [string]::IsNullOrWhiteSpace($_) } |
        Sort-Object
)

Write-Host ("ARCHIVE_MEMBER_COUNT=" + [string]$members.Count)
$members | ForEach-Object { Write-Host ("ARCHIVE_MEMBER=" + $_) }

if ($members.Count -ne $expectedMembers.Count) {
    throw "A12_TFT_REQUAL_ARCHIVE_MEMBER_COUNT_INVALID"
}
if (Compare-Object -ReferenceObject $expectedMembers -DifferenceObject $members) {
    throw "A12_TFT_REQUAL_ARCHIVE_MEMBER_SET_INVALID"
}

$oldLocation = Get-Location
try {
    Set-Location $extractDir
    $extractRun = Invoke-NativeCaptured -FilePath $archiver -Arguments @("x", $archivePath)
}
finally {
    Set-Location $oldLocation
}

if ($extractRun.ExitCode -ne 0) {
    throw "A12_TFT_REQUAL_ARCHIVE_EXTRACT_FAILED"
}

$sourceTftSha = Get-Sha256Lower $sourceTftObjects[0].FullName
$sourceBackendSha = Get-Sha256Lower $sourceBackendObjects[0].FullName
$archiveTftSha = Get-Sha256Lower (Join-Path $extractDir "JWPLC_TFT.cpp.o")
$archiveBackendSha = Get-Sha256Lower (Join-Path $extractDir "TFT_eSPI.cpp.o")

Write-Host ("REBUILT_JWPLC_TFT_OBJECT_SHA256=" + $sourceTftSha)
Write-Host ("ARCHIVE_JWPLC_TFT_OBJECT_SHA256=" + $archiveTftSha)
Write-Host ("REBUILT_TFT_ESPI_OBJECT_SHA256=" + $sourceBackendSha)
Write-Host ("ARCHIVE_TFT_ESPI_OBJECT_SHA256=" + $archiveBackendSha)
Write-Host ("JWPLC_TFT_OBJECT_BIT_FOR_BIT_REBUILD=" + $(if ($sourceTftSha -eq $archiveTftSha) { "YES" } else { "NO" }))
Write-Host ("TFT_ESPI_OBJECT_BIT_FOR_BIT_REBUILD=" + $(if ($sourceBackendSha -eq $archiveBackendSha) { "YES" } else { "NO" }))
Write-Host "OBJECT_BIT_FOR_BIT_REBUILD_REQUIRED=NO"

Write-Host ""
Write-Host "=== SELF-CONTAINED PRECOMPILED CASES ==="

$directResult = Invoke-CandidateCase -Label "DIRECT_TFT" -SketchPath $shapeSketch -CandidateRoot $candidateRoot -BuildPath $directBuild -LogPath (Join-Path $ResultRoot "candidate_direct_tft.log") -DiagnosticDisplayBypass $true
$displayResult = Invoke-CandidateCase -Label "DISPLAY_INTEGRATION" -SketchPath $displaySketch -CandidateRoot $candidateRoot -BuildPath $displayBuild -LogPath (Join-Path $ResultRoot "candidate_display_integration.log") -DiagnosticDisplayBypass $false
$autoloadResult = Invoke-CandidateCase -Label "NORMAL_AUTOLOAD" -SketchPath $emptySketch -CandidateRoot $candidateRoot -BuildPath $autoloadBuild -LogPath (Join-Path $ResultRoot "candidate_normal_autoload.log") -DiagnosticDisplayBypass $false

Write-Host ""
Write-Host "=== SOURCE / ARCHIVE STRUCTURAL EQUIVALENCE ==="

$sourceFlash = Get-Usage -Lines $sourceRun.Output -Kind "FLASH"
$sourceRam = Get-Usage -Lines $sourceRun.Output -Kind "RAM"
$candidateFlash = Get-Usage -Lines $directResult.Lines -Kind "FLASH"
$candidateRam = Get-Usage -Lines $directResult.Lines -Kind "RAM"

if ($sourceFlash -lt 0 -or $sourceRam -lt 0 -or $candidateFlash -lt 0 -or $candidateRam -lt 0) {
    throw "A12_TFT_REQUAL_USAGE_METRICS_MISSING"
}

$flashDelta = $candidateFlash - $sourceFlash
$ramDelta = $candidateRam - $sourceRam

Write-Host ("SOURCE_FLASH_BYTES=" + [string]$sourceFlash)
Write-Host ("CANDIDATE_FLASH_BYTES=" + [string]$candidateFlash)
Write-Host ("FLASH_DELTA_BYTES=" + [string]$flashDelta)
Write-Host ("SOURCE_RAM_BYTES=" + [string]$sourceRam)
Write-Host ("CANDIDATE_RAM_BYTES=" + [string]$candidateRam)
Write-Host ("RAM_DELTA_BYTES=" + [string]$ramDelta)

if ($ramDelta -ne 0) {
    throw "A12_TFT_REQUAL_RAM_PARITY_FAILED"
}
if ([Math]::Abs($flashDelta) -gt 64) {
    throw "A12_TFT_REQUAL_FLASH_PARITY_REVIEW_REQUIRED"
}

$sourceElf = Get-SingleElf -BuildPath $sourceBuild -Label "SOURCE"
$candidateElf = Get-SingleElf -BuildPath $directBuild -Label "CANDIDATE"

[string[]]$sourceSymbols = @(Get-DefinedSymbols -NmPath $nm -ElfPath $sourceElf)
[string[]]$candidateSymbols = @(Get-DefinedSymbols -NmPath $nm -ElfPath $candidateElf)
[object[]]$symbolDiff = @(Compare-Object -ReferenceObject $sourceSymbols -DifferenceObject $candidateSymbols)

Write-Host ("SOURCE_DEFINED_SYMBOL_COUNT=" + [string]$sourceSymbols.Count)
Write-Host ("CANDIDATE_DEFINED_SYMBOL_COUNT=" + [string]$candidateSymbols.Count)
Write-Host ("DEFINED_SYMBOL_NAME_TYPE_SIZE_PARITY=" + $(if ($symbolDiff.Count -eq 0) { "PASS" } else { "FAIL" }))

if ($symbolDiff.Count -ne 0) {
    $symbolDiff | Select-Object -First 40 | ForEach-Object {
        Write-Host ("SYMBOL_DIFF=" + $_.SideIndicator + ":" + $_.InputObject)
    }
    throw "A12_TFT_REQUAL_SYMBOL_PARITY_FAILED"
}

Write-Host "STRUCTURAL_EQUIVALENCE=PASS"

[string[]]$finalDirty = @(& git -C $repo diff --name-only)
[string[]]$finalStaged = @(& git -C $repo diff --cached --name-only)

Write-Host ("FINAL_TRACKED_DIRTY_COUNT=" + [string]$finalDirty.Count)
Write-Host ("FINAL_STAGED_COUNT=" + [string]$finalStaged.Count)

if ($finalDirty.Count -ne 0 -or $finalStaged.Count -ne 0) {
    $finalDirty | ForEach-Object { Write-Host ("FINAL_TRACKED_DIRTY=" + $_) }
    $finalStaged | ForEach-Object { Write-Host ("FINAL_STAGED=" + $_) }
    throw "A12_TFT_REQUAL_WORKTREE_MUTATED"
}

@(
    "ALPHA12_TFT_PRECOMPILED_REQUALIFICATION=PASS"
    "BRANCH=$branch"
    "HEAD=$head"
    "CURRENT_SOURCE_BLOB_IDENTITY=PASS"
    "ARCHIVE_IDENTITY_PHYSICAL_QUALIFIED=PASS"
    "ARCHIVE_BYTES=$archiveBytes"
    "ARCHIVE_SHA256=$archiveSha"
    "ARCHIVE_MEMBER_COUNT=2"
    "ARCHIVE_MEMBERS=$($expectedMembers -join ',')"
    "MAINTAINER_TFT_ESPI_VERSION=$($tftEspiSelection.Version)"
    "SOURCE_FIRST_CURRENT_SOURCE=PASS"
    "OBJECT_BIT_FOR_BIT_REBUILD_REQUIRED=NO"
    "JWPLC_TFT_OBJECT_BIT_FOR_BIT_REBUILD=$(if ($sourceTftSha -eq $archiveTftSha) { 'YES' } else { 'NO' })"
    "TFT_ESPI_OBJECT_BIT_FOR_BIT_REBUILD=$(if ($sourceBackendSha -eq $archiveBackendSha) { 'YES' } else { 'NO' })"
    "STRUCTURAL_EQUIVALENCE=PASS"
    "FLASH_DELTA_BYTES=$flashDelta"
    "RAM_DELTA_BYTES=$ramDelta"
    "DEFINED_SYMBOL_NAME_TYPE_SIZE_PARITY=PASS"
    "DIRECT_TFT_PRECOMPILED_LINK=PASS"
    "DISPLAY_INTEGRATION_PRECOMPILED_LINK=PASS"
    "NORMAL_AUTOLOAD_PRECOMPILED_LINK=PASS"
    "GLOBAL_TFT_ESPI_REQUIRED_AT_USER_BUILD=NO"
    "OFFICIAL_ARCHIVE_CHANGED=NO"
    "PHYSICAL_UPLOAD_PERFORMED=NO"
    "FINAL_TRACKED_DIRTY_COUNT=0"
    "RESULT_ROOT=$ResultRoot"
) | Set-Content -LiteralPath (Join-Path $ResultRoot "SUMMARY.txt") -Encoding UTF8

Write-Host ""
Write-Host "=============================================================================="
Write-Host "ALPHA12_TFT_PRECOMPILED_REQUALIFICATION=PASS"
Write-Host "CURRENT_SOURCE_BLOB_IDENTITY=PASS"
Write-Host "ARCHIVE_IDENTITY_PHYSICAL_QUALIFIED=PASS"
Write-Host ("ARCHIVE_BYTES=" + [string]$archiveBytes)
Write-Host ("ARCHIVE_SHA256=" + $archiveSha)
Write-Host "ARCHIVE_MEMBER_COUNT=2"
Write-Host ("MAINTAINER_TFT_ESPI_VERSION=" + $tftEspiSelection.Version)
Write-Host "SOURCE_FIRST_CURRENT_SOURCE=PASS"
Write-Host "OBJECT_BIT_FOR_BIT_REBUILD_REQUIRED=NO"
Write-Host "STRUCTURAL_EQUIVALENCE=PASS"
Write-Host ("FLASH_DELTA_BYTES=" + [string]$flashDelta)
Write-Host ("RAM_DELTA_BYTES=" + [string]$ramDelta)
Write-Host "DEFINED_SYMBOL_NAME_TYPE_SIZE_PARITY=PASS"
Write-Host "DIRECT_TFT_PRECOMPILED_LINK=PASS"
Write-Host "DISPLAY_INTEGRATION_PRECOMPILED_LINK=PASS"
Write-Host "NORMAL_AUTOLOAD_PRECOMPILED_LINK=PASS"
Write-Host "GLOBAL_TFT_ESPI_REQUIRED_AT_USER_BUILD=NO"
Write-Host "OFFICIAL_ARCHIVE_CHANGED=NO"
Write-Host "PHYSICAL_UPLOAD_PERFORMED=NO"
Write-Host "FINAL_TRACKED_DIRTY_COUNT=0"
Write-Host ("RESULT_ROOT=" + $ResultRoot)
