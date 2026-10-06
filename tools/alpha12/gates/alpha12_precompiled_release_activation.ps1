param(
    [string]$ArduinoCli = "C:\Program Files\Arduino PLC IDE Tools\arduino-cli.exe",
    [string]$Fqbn = "jwplc_local:esp32:jwplcbasic",
    [string]$ResultRoot = ""
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$repo = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot "..\..\.."))
$expectedBranch = "v2.1.0-alpha.12/feature/modbus-tcp"
$repoLibraries = Join-Path $repo "JWPLC\2.1.0\libraries"
$sketch = Join-Path $repo "tools\build-speed-benchmark\sketches\01_empty"

function Get-Sha256Lower {
    param([string]$Path)

    $stream = [IO.File]::OpenRead($Path)
    $sha256 = [Security.Cryptography.SHA256]::Create()

    try {
        return (
            [BitConverter]::ToString(
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

function Resolve-ArduinoCli {
    param([string]$Preferred)

    [string[]]$candidates = @(
        $Preferred,
        (Join-Path $env:LOCALAPPDATA "Programs\Arduino IDE\resources\app\lib\backend\resources\arduino-cli.exe"),
        (Join-Path $env:LOCALAPPDATA "Arduino15\packages\jwplc_local\tools\arduino-cli\arduino-cli.exe")
    )

    foreach ($candidate in $candidates) {
        if (-not [string]::IsNullOrWhiteSpace($candidate)) {
            if (Test-Path -LiteralPath $candidate) {
                return (Resolve-Path -LiteralPath $candidate).Path
            }
        }
    }

    $command = Get-Command "arduino-cli.exe" -ErrorAction SilentlyContinue

    if ($null -ne $command) {
        return $command.Source
    }

    throw "A12_P7B_ARDUINO_CLI_NOT_FOUND"
}

function Read-PropertyFlag {
    param(
        [string]$LibraryName,
        [string]$Key,
        [string]$ExpectedValue
    )

    $propertiesPath = Join-Path $repoLibraries ($LibraryName + "\library.properties")

    if (-not (Test-Path -LiteralPath $propertiesPath)) {
        throw ("A12_P7B_PROPERTIES_MISSING:" + $LibraryName)
    }

    [string[]]$lines = @(Get-Content -LiteralPath $propertiesPath)

    foreach ($line in $lines) {
        if ($line.Trim() -ieq ($Key + "=" + $ExpectedValue)) {
            return $true
        }
    }

    return $false
}

function Test-LibrarySelected {
    param(
        [string[]]$Lines,
        [string]$LibraryName
    )

    foreach ($line in $Lines) {
        if ($line -match ("^Using library " + [regex]::Escape($LibraryName) + " at version .+ in folder: (.+)$")) {
            return $true
        }
    }

    return $false
}

function Test-PrecompiledMarker {
    param(
        [string[]]$Lines,
        [string]$LibraryName
    )

    foreach ($line in $Lines) {
        if (
            $line -match 'Using precompiled library|Usando libreria precompilada|Usando biblioteca precompilada'
        ) {
            if ($line -match [regex]::Escape($LibraryName)) {
                return $true
            }
        }
    }

    return $false
}

function Count-NamedObjects {
    param(
        [string]$BuildPath,
        [string[]]$ObjectNames
    )

    return @(
        Get-ChildItem -LiteralPath $BuildPath -Recurse -File -ErrorAction SilentlyContinue |
            Where-Object { $ObjectNames -contains $_.Name }
    ).Count
}

$branch = (& git -C $repo branch --show-current).Trim()
$head = (& git -C $repo rev-parse HEAD).Trim()

if ($branch -ne $expectedBranch) {
    throw ("A12_P7B_BRANCH_MISMATCH expected=" + $expectedBranch + " actual=" + $branch)
}

[string[]]$entryDirty = @(& git -C $repo diff --name-only)
[string[]]$entryStaged = @(& git -C $repo diff --cached --name-only)

if ($entryDirty.Count -ne 0) {
    $entryDirty | ForEach-Object { Write-Host ("ENTRY_DIRTY=" + $_) }
    throw "A12_P7B_TRACKED_TREE_NOT_CLEAN"
}

if ($entryStaged.Count -ne 0) {
    throw "A12_P7B_INDEX_NOT_CLEAN"
}

& git -C $repo diff --check

if ($LASTEXITCODE -ne 0) {
    throw "A12_P7B_GIT_DIFF_CHECK_FAILED"
}

$ArduinoCli = Resolve-ArduinoCli -Preferred $ArduinoCli

if (-not (Test-Path -LiteralPath $sketch)) {
    throw "A12_P7B_01_EMPTY_SKETCH_MISSING"
}

[object[]]$archives = @(
    [pscustomobject]@{
        Name = "JWPLC_Display"
        RelativePath = "JWPLC\2.1.0\libraries\JWPLC_Display\src\esp32\libJWPLC_Display.a"
        ExpectedSha = "c960d718433e29a40e3cc55bc745c9a2e121ee1ee598ec72c04872327592dc02"
        RequireDotA = $true
        ObjectNames = @(
            "JWPLC_Display.cpp.o",
            "JWPLC_Display_H3E1_Profile.cpp.o",
            "JWPLC_IdleScreen.cpp.o",
            "JWPLC_UI.cpp.o",
            "JWPLC_UI_API.cpp.o",
            "JWPLC_UI_Pages.cpp.o",
            "JWPLC_UI_PixelMap.cpp.o"
        )
    },
    [pscustomobject]@{
        Name = "JWPLC_ModbusRTU"
        RelativePath = "JWPLC\2.1.0\libraries\JWPLC_ModbusRTU\src\esp32\libJWPLC_ModbusRTU.a"
        ExpectedSha = "424ed3f462bb57cce0019d690486e612d5f243c40ae62d44d3ad973b0a521085"
        RequireDotA = $false
        ObjectNames = @("JWPLC_ModbusRTU.cpp.o")
    },
    [pscustomobject]@{
        Name = "JWPLC_TFT"
        RelativePath = "JWPLC\2.1.0\libraries\JWPLC_TFT\src\esp32\libJWPLC_TFT.a"
        ExpectedSha = "5d860a131811dd9a7eb6fa55f5674b1d78b0de7dfaf8748ce18a60ceed2d3738"
        RequireDotA = $true
        ObjectNames = @("JWPLC_TFT.cpp.o", "TFT_eSPI.cpp.o")
    },
    [pscustomobject]@{
        Name = "JW_SD"
        RelativePath = "JWPLC\2.1.0\libraries\JW_SD\src\esp32\libJW_SD.a"
        ExpectedSha = "1b9619ba37295782ade1edc6f06439a38e7e534fc08f5b05757c9af7a645acc0"
        RequireDotA = $false
        ObjectNames = @("JW_SD.cpp.o")
    },
    [pscustomobject]@{
        Name = "SPI"
        RelativePath = "JWPLC\2.1.0\libraries\SPI\src\esp32\libSPI.a"
        ExpectedSha = "b433758b746380bf8d1ea102aca1d1637b5a8cab50616b56024f4b022a916445"
        RequireDotA = $false
        ObjectNames = @("SPI.cpp.o")
    }
)

Write-Host "=============================================================================="
Write-Host " ALPHA12 - P7B RELEASE-LIKE PRECOMPILED ACTIVATION"
Write-Host "=============================================================================="
Write-Host ("BRANCH=" + $branch)
Write-Host ("HEAD=" + $head)
Write-Host ("ARDUINO_CLI=" + $ArduinoCli)
Write-Host ("FQBN=" + $Fqbn)

foreach ($entry in $archives) {
    $archivePath = Join-Path $repo $entry.RelativePath

    if (-not (Test-Path -LiteralPath $archivePath)) {
        throw ("A12_P7B_ARCHIVE_MISSING:" + $entry.Name)
    }

    $actualSha = Get-Sha256Lower -Path $archivePath

    if ($entry.ExpectedSha.Length -ne 64 -or $entry.ExpectedSha -notmatch '^[0-9a-fA-F]+$') {
        throw ("A12_P7B_EXPECTED_SHA_LITERAL_INVALID:" + $entry.Name)
    }

    $precompiledFull = Read-PropertyFlag -LibraryName $entry.Name -Key "precompiled" -ExpectedValue "full"
    $dotALinkage = Read-PropertyFlag -LibraryName $entry.Name -Key "dot_a_linkage" -ExpectedValue "true"

    Write-Host ""
    Write-Host ("POLICY_LIBRARY=" + $entry.Name)
    Write-Host ("POLICY_ARCHIVE_SHA256=" + $actualSha)
    Write-Host ("POLICY_PRECOMPILED_FULL=" + [string]$precompiledFull)
    Write-Host ("POLICY_DOT_A_LINKAGE=" + [string]$dotALinkage)

    if ($actualSha -ne $entry.ExpectedSha) {
        throw ("A12_P7B_ARCHIVE_SHA_MISMATCH:" + $entry.Name)
    }

    if (-not $precompiledFull) {
        throw ("A12_P7B_PRECOMPILED_FULL_MISSING:" + $entry.Name)
    }

    if ($entry.RequireDotA -and -not $dotALinkage) {
        throw ("A12_P7B_DOT_A_LINKAGE_MISSING:" + $entry.Name)
    }
}

[object[]]$sourceOnly = @(
    [pscustomobject]@{ Name = "JW_RTC"; ObjectNames = @("JW_RTC.cpp.o") },
    [pscustomobject]@{ Name = "JWPLC_GlobalPeripherals"; ObjectNames = @("JWPLC_GlobalPeripherals.cpp.o") },
    [pscustomobject]@{ Name = "JWPLC_Ethernet"; ObjectNames = @("JWPLC_Ethernet.cpp.o") },
    [pscustomobject]@{ Name = "JWPLC_RS485"; ObjectNames = @("JWPLC_RS485.cpp.o") }
)

foreach ($entry in $sourceOnly) {
    $precompiledFull = Read-PropertyFlag -LibraryName $entry.Name -Key "precompiled" -ExpectedValue "full"
    Write-Host ("SOURCE_ONLY_POLICY_" + $entry.Name + "=" + $(if ($precompiledFull) { "FAIL" } else { "PASS" }))

    if ($precompiledFull) {
        throw ("A12_P7B_SOURCE_ONLY_POLICY_CHANGED:" + $entry.Name)
    }
}

$stamp = Get-Date -Format "yyyyMMdd_HHmmss"

if ([string]::IsNullOrWhiteSpace($ResultRoot)) {
    $ResultRoot = Join-Path $repo ("tools\alpha12\results\precompiled_release_activation_" + $stamp)
}

$buildPath = Join-Path $ResultRoot "build"
$logPath = Join-Path $ResultRoot "compile.log"

New-Item -ItemType Directory -Force -Path $ResultRoot | Out-Null

Write-Host ""
Write-Host "=== NORMAL AUTOLOAD RELEASE-LIKE BUILD ==="

$compileArgs = @(
    "compile",
    "--fqbn", $Fqbn,
    "-j", "0",
    "-v",
    "--clean",
    "--build-path", $buildPath,
    "--libraries", $repoLibraries,
    $sketch
)

$run = Invoke-NativeCaptured -FilePath $ArduinoCli -Arguments $compileArgs
$run.Output | Set-Content -LiteralPath $logPath -Encoding UTF8

Write-Host ("COMPILE_EXIT=" + [string]$run.ExitCode)

if ($run.ExitCode -ne 0) {
    $run.Output | Select-Object -Last 220 | ForEach-Object { Write-Host $_ }
    throw "A12_P7B_NORMAL_AUTOLOAD_COMPILE_FAILED"
}

$warningCount = @($run.Output | Where-Object { $_ -match '(?i)\bwarning:' }).Count
$errorCount = @($run.Output | Where-Object { $_ -match '(?i)\berror:' }).Count

Write-Host ("WARNING_LINES=" + [string]$warningCount)
Write-Host ("ERROR_LINES=" + [string]$errorCount)

if ($warningCount -ne 0 -or $errorCount -ne 0) {
    throw "A12_P7B_COMPILER_DIAGNOSTICS"
}

foreach ($entry in $archives) {
    $selected = Test-LibrarySelected -Lines $run.Output -LibraryName $entry.Name
    $precompiled = Test-PrecompiledMarker -Lines $run.Output -LibraryName $entry.Name
    $sourceObjectCount = Count-NamedObjects -BuildPath $buildPath -ObjectNames $entry.ObjectNames

    Write-Host ("LINK_" + $entry.Name + "_SELECTED=" + [string]$selected)
    Write-Host ("LINK_" + $entry.Name + "_PRECOMPILED_MARKER=" + $(if ($precompiled) { "YES" } else { "NO" }))
    Write-Host ("LINK_" + $entry.Name + "_SOURCE_OBJECT_COUNT=" + [string]$sourceObjectCount)

    if (-not $selected) {
        throw ("A12_P7B_LIBRARY_NOT_SELECTED:" + $entry.Name)
    }

    if (-not $precompiled) {
        throw ("A12_P7B_PRECOMPILED_MARKER_MISSING:" + $entry.Name)
    }

    if ($sourceObjectCount -ne 0) {
        throw ("A12_P7B_PRECOMPILED_SOURCE_RECOMPILED:" + $entry.Name)
    }
}

foreach ($entry in $sourceOnly) {
    $selected = Test-LibrarySelected -Lines $run.Output -LibraryName $entry.Name
    $precompiled = Test-PrecompiledMarker -Lines $run.Output -LibraryName $entry.Name
    $sourceObjectCount = Count-NamedObjects -BuildPath $buildPath -ObjectNames $entry.ObjectNames

    Write-Host ("SOURCE_ONLY_" + $entry.Name + "_SELECTED=" + [string]$selected)
    Write-Host ("SOURCE_ONLY_" + $entry.Name + "_PRECOMPILED_MARKER=" + $(if ($precompiled) { "YES" } else { "NO" }))
    Write-Host ("SOURCE_ONLY_" + $entry.Name + "_SOURCE_OBJECT_COUNT=" + [string]$sourceObjectCount)

    if (-not $selected) {
        throw ("A12_P7B_SOURCE_ONLY_LIBRARY_NOT_SELECTED:" + $entry.Name)
    }

    if ($precompiled) {
        throw ("A12_P7B_SOURCE_ONLY_PRECOMPILED_UNEXPECTED:" + $entry.Name)
    }

    if ($sourceObjectCount -lt 1) {
        throw ("A12_P7B_SOURCE_ONLY_OBJECT_MISSING:" + $entry.Name)
    }
}

$globalTftEspiSelected = Test-LibrarySelected -Lines $run.Output -LibraryName "TFT_eSPI"

Write-Host ("GLOBAL_TFT_ESPI_SELECTED=" + $(if ($globalTftEspiSelected) { "YES" } else { "NO" }))

if ($globalTftEspiSelected) {
    throw "A12_P7B_GLOBAL_TFT_ESPI_UNEXPECTED"
}

[string[]]$requiredAutoload = @(
    "JWPLC_Display",
    "JWPLC_TFT",
    "JW_MatrixButtons",
    "JWPLC_GlobalPeripherals",
    "JW_RTC",
    "JW_FRAM",
    "JW_SD",
    "JWPLC_Ethernet",
    "JWPLC_RS485",
    "JWPLC_ModbusRTU",
    "SPI",
    "SD"
)

foreach ($libraryName in $requiredAutoload) {
    $selected = Test-LibrarySelected -Lines $run.Output -LibraryName $libraryName
    Write-Host ("AUTOLOAD_" + $libraryName + "_SELECTED=" + [string]$selected)

    if (-not $selected) {
        throw ("A12_P7B_AUTOLOAD_LIBRARY_MISSING:" + $libraryName)
    }
}

[string[]]$finalDirty = @(& git -C $repo diff --name-only)
[string[]]$finalStaged = @(& git -C $repo diff --cached --name-only)

Write-Host ("FINAL_TRACKED_DIRTY_COUNT=" + [string]$finalDirty.Count)
Write-Host ("FINAL_STAGED_COUNT=" + [string]$finalStaged.Count)

if ($finalDirty.Count -ne 0 -or $finalStaged.Count -ne 0) {
    throw "A12_P7B_REPOSITORY_MUTATED"
}

@(
    "ALPHA12_P7B_RELEASE_LIKE_PRECOMPILED_ACTIVATION=PASS"
    "BRANCH=$branch"
    "HEAD=$head"
    "COMPILE_EXIT=0"
    "WARNING_LINES=0"
    "ERROR_LINES=0"
    "PRECOMPILED_ACTIVE=JWPLC_Display,JWPLC_ModbusRTU,JWPLC_TFT,JW_SD,SPI"
    "SOURCE_ONLY=JW_RTC,JWPLC_GlobalPeripherals,JWPLC_Ethernet,JWPLC_RS485"
    "GLOBAL_TFT_ESPI_REQUIRED_AT_USER_BUILD=NO"
    "NORMAL_AUTOLOAD_COMPLETE=PASS"
    "PRECOMPILED_FREEZE=READY"
    "FINAL_TRACKED_DIRTY_COUNT=0"
    "RESULT_ROOT=$ResultRoot"
) | Set-Content -LiteralPath (Join-Path $ResultRoot "SUMMARY.txt") -Encoding UTF8

Write-Host ""
Write-Host "=============================================================================="
Write-Host "ALPHA12_P7B_RELEASE_LIKE_PRECOMPILED_ACTIVATION=PASS"
Write-Host "PRECOMPILED_ACTIVE=JWPLC_Display,JWPLC_ModbusRTU,JWPLC_TFT,JW_SD,SPI"
Write-Host "SOURCE_ONLY=JW_RTC,JWPLC_GlobalPeripherals,JWPLC_Ethernet,JWPLC_RS485"
Write-Host "GLOBAL_TFT_ESPI_REQUIRED_AT_USER_BUILD=NO"
Write-Host "NORMAL_AUTOLOAD_COMPLETE=PASS"
Write-Host "PRECOMPILED_FREEZE=READY"
Write-Host "FINAL_TRACKED_DIRTY_COUNT=0"
Write-Host ("RESULT_ROOT=" + $ResultRoot)
