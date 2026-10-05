param(
    [string]$ArduinoCli = "C:\Program Files\Arduino PLC IDE Tools\arduino-cli.exe"
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$repo = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot "..\..\.."))
$expectedBranch = "v2.1.0-alpha.12/feature/modbus-tcp"
$benchmark = Join-Path $repo "tools\build-speed-benchmark\Run-JWPLCBuildBenchmark.ps1"
$outputRoot = Join-Path $repo "tools\alpha12\results\final_build_speed_benchmark"

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

function Resolve-ArduinoCli {
    param([string]$Preferred)

    [string[]]$candidates = @(
        $Preferred,
        (Join-Path $env:LOCALAPPDATA "Programs\Arduino IDE\resources\app\lib\backend\resources\arduino-cli.exe")
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

    throw "A12_BUILD_SPEED_ARDUINO_CLI_NOT_FOUND"
}

function Read-PropertyFlag {
    param(
        [string]$LibraryName,
        [string]$Key,
        [string]$ExpectedValue
    )

    $propertiesPath = Join-Path $repo ("JWPLC\2.1.0\libraries\" + $LibraryName + "\library.properties")

    if (-not (Test-Path -LiteralPath $propertiesPath)) {
        throw ("A12_BUILD_SPEED_PROPERTIES_MISSING:" + $LibraryName)
    }

    [string[]]$lines = @(Get-Content -LiteralPath $propertiesPath)

    foreach ($line in $lines) {
        if ($line.Trim() -ieq ($Key + "=" + $ExpectedValue)) {
            return $true
        }
    }

    return $false
}

$branch = (& git -C $repo branch --show-current).Trim()
$head = (& git -C $repo rev-parse HEAD).Trim()

if ($branch -ne $expectedBranch) {
    throw ("A12_BUILD_SPEED_BRANCH_MISMATCH expected=" + $expectedBranch + " actual=" + $branch)
}

[string[]]$entryDirty = @(& git -C $repo diff --name-only)
[string[]]$entryStaged = @(& git -C $repo diff --cached --name-only)

if ($entryDirty.Count -ne 0) {
    $entryDirty | ForEach-Object { Write-Host ("ENTRY_DIRTY=" + $_) }
    throw "A12_BUILD_SPEED_TRACKED_TREE_NOT_CLEAN"
}

if ($entryStaged.Count -ne 0) {
    throw "A12_BUILD_SPEED_INDEX_NOT_CLEAN"
}

if (-not (Test-Path -LiteralPath $benchmark)) {
    throw "A12_BUILD_SPEED_BENCHMARK_SCRIPT_MISSING"
}

$tokens = $null
$parseErrors = $null

[System.Management.Automation.Language.Parser]::ParseFile(
    $benchmark,
    [ref]$tokens,
    [ref]$parseErrors
) | Out-Null

Write-Host ("BENCHMARK_SCRIPT_SYNTAX_ERROR_COUNT=" + [string]$parseErrors.Count)

if ($parseErrors.Count -ne 0) {
    foreach ($parseError in $parseErrors) {
        Write-Host (
            "BENCHMARK_SCRIPT_SYNTAX_ERROR=" +
            $parseError.Message +
            " @ " +
            [string]$parseError.Extent.StartLineNumber +
            ":" +
            [string]$parseError.Extent.StartColumnNumber
        )
    }

    throw "A12_BUILD_SPEED_BENCHMARK_SCRIPT_SYNTAX_INVALID"
}

Write-Host "BENCHMARK_SCRIPT_SYNTAX=PASS"

[object[]]$frozenArchives = @(
    [pscustomobject]@{
        Name = "JWPLC_Display"
        Path = "JWPLC\2.1.0\libraries\JWPLC_Display\src\esp32\libJWPLC_Display.a"
        Sha = "c960d718433e29a40e3cc55bc745c9a2e121ee1ee598ec72c04872327592dc02"
        DotA = $true
    },
    [pscustomobject]@{
        Name = "JWPLC_ModbusRTU"
        Path = "JWPLC\2.1.0\libraries\JWPLC_ModbusRTU\src\esp32\libJWPLC_ModbusRTU.a"
        Sha = "424ed3f462bb57cce0019d690486e612d5f243c40ae62d44d3ad973b0a521085"
        DotA = $false
    },
    [pscustomobject]@{
        Name = "JWPLC_TFT"
        Path = "JWPLC\2.1.0\libraries\JWPLC_TFT\src\esp32\libJWPLC_TFT.a"
        Sha = "5d860a131811dd9a7eb6fa55f5674b1d78b0de7dfaf8748ce18a60ceed2d3738"
        DotA = $true
    },
    [pscustomobject]@{
        Name = "JW_SD"
        Path = "JWPLC\2.1.0\libraries\JW_SD\src\esp32\libJW_SD.a"
        Sha = "1b9619ba37295782ade1edc6f06439a38e7e534fc08f5b05757c9af7a645acc0"
        DotA = $false
    },
    [pscustomobject]@{
        Name = "SPI"
        Path = "JWPLC\2.1.0\libraries\SPI\src\esp32\libSPI.a"
        Sha = "b433758b746380bf8d1ea102aca1d1637b5a8cab50616b56024f4b022a916445"
        DotA = $false
    },
    [pscustomobject]@{
        Name = "JW_FRAM"
        Path = "JWPLC\2.1.0\libraries\JW_FRAM\src\esp32\libJW_FRAM.a"
        Sha = "b8734763bfa1287167feda72a5c9b30df97340d41ca0c3fd000321e218632ceb"
        DotA = $false
    }
)

Write-Host "=============================================================================="
Write-Host " ALPHA12 - FINAL BUILD SPEED BENCHMARK RUNNER"
Write-Host "=============================================================================="
Write-Host ("BRANCH=" + $branch)
Write-Host ("HEAD=" + $head)

foreach ($entry in $frozenArchives) {
    $archivePath = Join-Path $repo $entry.Path

    if (-not (Test-Path -LiteralPath $archivePath)) {
        throw ("A12_BUILD_SPEED_FROZEN_ARCHIVE_MISSING:" + $entry.Name)
    }

    $actualSha = Get-Sha256Lower -Path $archivePath
    $precompiledFull = Read-PropertyFlag -LibraryName $entry.Name -Key "precompiled" -ExpectedValue "full"
    $dotALinkage = Read-PropertyFlag -LibraryName $entry.Name -Key "dot_a_linkage" -ExpectedValue "true"

    Write-Host ("FREEZE_" + $entry.Name + "_SHA_PASS=" + [string]($actualSha -eq $entry.Sha))
    Write-Host ("FREEZE_" + $entry.Name + "_PRECOMPILED_FULL=" + [string]$precompiledFull)

    if ($actualSha -ne $entry.Sha) {
        throw ("A12_BUILD_SPEED_FREEZE_SHA_MISMATCH:" + $entry.Name)
    }

    if (-not $precompiledFull) {
        throw ("A12_BUILD_SPEED_FREEZE_POLICY_MISMATCH:" + $entry.Name)
    }

    if ($entry.DotA -and -not $dotALinkage) {
        throw ("A12_BUILD_SPEED_FREEZE_DOT_A_MISMATCH:" + $entry.Name)
    }
}

[string[]]$sourceOnly = @(
    "JW_RTC",
    "JWPLC_GlobalPeripherals",
    "JWPLC_Ethernet",
    "JWPLC_RS485"
)

foreach ($libraryName in $sourceOnly) {
    $precompiledFull = Read-PropertyFlag -LibraryName $libraryName -Key "precompiled" -ExpectedValue "full"
    Write-Host ("FREEZE_SOURCE_ONLY_" + $libraryName + "=" + $(if ($precompiledFull) { "FAIL" } else { "PASS" }))

    if ($precompiledFull) {
        throw ("A12_BUILD_SPEED_SOURCE_ONLY_POLICY_CHANGED:" + $libraryName)
    }
}

$ArduinoCli = Resolve-ArduinoCli -Preferred $ArduinoCli
Write-Host ("ARDUINO_CLI=" + $ArduinoCli)
Write-Host "PRECOMPILED_FREEZE_PREFLIGHT=PASS"

New-Item -ItemType Directory -Force -Path $outputRoot | Out-Null

[string[]]$beforeRuns = @(
    Get-ChildItem -LiteralPath $outputRoot -Directory -ErrorAction SilentlyContinue |
        ForEach-Object { $_.Name }
)

Write-Host ""
Write-Host "=== RUN HISTORICAL 6-PHASE MATRIX ==="
Write-Host "TARGETS=Basic,Core"
Write-Host "SKETCH=01_empty"
Write-Host "JOBS=0"
Write-Host "UPLOADS=SKIPPED"

$benchmarkParams = @{
    ArduinoCli = $ArduinoCli
    PackageNamespace = "jwplc_local"
    Targets = @("Basic", "Core")
    Sketches = @("01_empty")
    SkipUploads = $true
    Jobs = 0
    RunLabel = "alpha12-final-precompiled-freeze"
    OutputRoot = $outputRoot
}

& $benchmark @benchmarkParams

[object[]]$newRuns = @(
    Get-ChildItem -LiteralPath $outputRoot -Directory |
        Where-Object { $beforeRuns -notcontains $_.Name } |
        Sort-Object LastWriteTime
)

if ($newRuns.Count -ne 1) {
    Write-Host ("NEW_RUN_COUNT=" + [string]$newRuns.Count)
    $newRuns | ForEach-Object { Write-Host ("NEW_RUN=" + $_.FullName) }
    throw "A12_BUILD_SPEED_RESULT_DIRECTORY_AMBIGUOUS"
}

$runRoot = $newRuns[0].FullName
$csvPath = Join-Path $runRoot "results.csv"
$environmentPath = Join-Path $runRoot "environment.json"
$summaryPath = Join-Path $runRoot "SUMMARY.md"

foreach ($requiredPath in @($csvPath, $environmentPath, $summaryPath)) {
    if (-not (Test-Path -LiteralPath $requiredPath)) {
        throw ("A12_BUILD_SPEED_RESULT_FILE_MISSING:" + $requiredPath)
    }
}

[object[]]$rows = @(Import-Csv -LiteralPath $csvPath)
$environment = Get-Content -LiteralPath $environmentPath -Raw | ConvertFrom-Json

[string[]]$expectedPhases = @(
    "managed_cold",
    "managed_warm_nochange",
    "managed_warm_touch",
    "explicit_cold",
    "explicit_warm_nochange",
    "explicit_warm_touch"
)

[string[]]$expectedTargets = @("Basic", "Core")

Write-Host ""
Write-Host "=== VALIDATE BENCHMARK MATRIX ==="
Write-Host ("RESULT_ROOT=" + $runRoot)
Write-Host ("RESULT_ROW_COUNT=" + [string]$rows.Count)

if ($rows.Count -ne 12) {
    throw "A12_BUILD_SPEED_RESULT_ROW_COUNT_INVALID"
}

foreach ($target in $expectedTargets) {
    foreach ($phase in $expectedPhases) {
        [object[]]$match = @(
            $rows |
                Where-Object {
                    $_.Target -eq $target -and
                    $_.Sketch -eq "01_empty" -and
                    $_.Phase -eq $phase
                }
        )

        if ($match.Count -ne 1) {
            throw ("A12_BUILD_SPEED_MATRIX_CELL_INVALID:" + $target + ":" + $phase)
        }

        if ($match[0].Success -ne "True") {
            throw ("A12_BUILD_SPEED_PHASE_FAILED:" + $target + ":" + $phase)
        }

        Write-Host (
            "BENCHMARK_ROW=" +
            $target + "|" +
            $phase + "|" +
            $match[0].DurationMs + "|" +
            $match[0].CompilerInvocations + "|" +
            $match[0].LinkInvocations + "|" +
            $match[0].BinaryBytes
        )
    }
}

[object[]]$unexpectedRows = @(
    $rows |
        Where-Object {
            $_.Target -notin $expectedTargets -or
            $_.Sketch -ne "01_empty" -or
            $_.Phase -notin $expectedPhases
        }
)

if ($unexpectedRows.Count -ne 0) {
    throw "A12_BUILD_SPEED_UNEXPECTED_RESULT_ROWS"
}

if ($environment.gitBranch -ne $expectedBranch) {
    throw "A12_BUILD_SPEED_ENVIRONMENT_BRANCH_MISMATCH"
}

if ($environment.gitCommit -ne $head) {
    throw "A12_BUILD_SPEED_ENVIRONMENT_COMMIT_MISMATCH"
}

if ($environment.packageNamespace -ne "jwplc_local") {
    throw "A12_BUILD_SPEED_ENVIRONMENT_NAMESPACE_MISMATCH"
}

if ([int]$environment.jobs -ne 0) {
    throw "A12_BUILD_SPEED_ENVIRONMENT_JOBS_MISMATCH"
}

[string[]]$finalDirty = @(& git -C $repo diff --name-only)
[string[]]$finalStaged = @(& git -C $repo diff --cached --name-only)

Write-Host ("FINAL_TRACKED_DIRTY_COUNT=" + [string]$finalDirty.Count)
Write-Host ("FINAL_STAGED_COUNT=" + [string]$finalStaged.Count)

if ($finalDirty.Count -ne 0 -or $finalStaged.Count -ne 0) {
    throw "A12_BUILD_SPEED_REPOSITORY_MUTATED"
}

@(
    "ALPHA12_FINAL_BUILD_SPEED_BENCHMARK=PASS"
    "MATRIX=Basic,Core x 01_empty x 6 phases"
    "RESULT_ROW_COUNT=12"
    "JOBS=0"
    "UPLOADS=SKIPPED"
    "PRECOMPILED_FREEZE=PASS"
    "FINAL_TRACKED_DIRTY_COUNT=0"
    "RESULT_ROOT=$runRoot"
) | Set-Content -LiteralPath (Join-Path $runRoot "ALPHA12_VALIDATION.txt") -Encoding UTF8

Write-Host ""
Write-Host "=============================================================================="
Write-Host "ALPHA12_FINAL_BUILD_SPEED_BENCHMARK=PASS"
Write-Host "ALPHA12_BUILD_SPEED_MATRIX_COMPLETE=YES"
Write-Host "RESULT_ROW_COUNT=12"
Write-Host "JOBS=0"
Write-Host "UPLOADS=SKIPPED"
Write-Host "PRECOMPILED_FREEZE=PASS"
Write-Host "FINAL_TRACKED_DIRTY_COUNT=0"
Write-Host ("RESULT_ROOT=" + $runRoot)
