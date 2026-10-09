param(
    [Parameter(Position = 0)][string]$SerialPort = '',
    [Parameter(Position = 1)][string]$CandidateRoot = '',
    [string]$ArduinoCli = 'C:\Program Files\Arduino PLC IDE Tools\arduino-cli.exe'
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$ExpectedBranch = 'v2.1.0-alpha.13/feature/cleanup-robustness'
$Fqbn = 'jwplc_local:esp32:jwplcbasic'
$ExpectedCandidateCppSha = '494440b00e74e74a7b23975420574e035ef2138fdf5a0cea2fed51c986c29d25'
$ExpectedCandidateSetupSha = '8fa079444ca130772d3a642eaf814035100bc098032b403c922c458d351ae5e1'
$ExpectedCandidateInitSha = '44873be82fe836084934a328df77f098e9ab88d212dd1d570da5e8aac74671bc'
$ExpectedBackendVersion = '2.5.43'
$ExpectedBackendCppSha = '01ed6edb0530d38b94ddeac079ba81633aa21d77d049b12da21a37f4bec69ee1'
$ExpectedBackendHeaderSha = 'b1b2789ace7ac8fd4a4c414054757d91e6e62649a23d74226ea28ceb8d6f4462'

$ScriptDir = $PSScriptRoot
$RepoRoot = [IO.Path]::GetFullPath((Join-Path $ScriptDir '..\..\..'))
$CommonPath = Join-Path $ScriptDir 'common.ps1'
$RepoLibraries = Join-Path $RepoRoot 'JWPLC\2.1.0\libraries'
$ProbeDir = Join-Path $RepoRoot 'tools\alpha13\firmware\a13_tft_pre1_startup_baseline_probe'
$ClientPath = Join-Path $ScriptDir 'a13_tft_pre1_startup_baseline_client.py'
$OfficialHeader = Join-Path $RepoRoot 'JWPLC\2.1.0\libraries\JWPLC_TFT\src\JWPLC_TFT.h'
$BackendProbeDir = Join-Path $RepoRoot 'tools\modbus-tcp-benchmark\firmware\a14_h3e1d1_tft_espi_compile_probe'

$RunId = Get-Date -Format 'yyyyMMdd_HHmmss'
$RunRoot = Join-Path $RepoRoot ("tools\alpha13\results\tft_pre5_{0}" -f $RunId)
$TempRoot = Join-Path $env:TEMP ("jwplc_a13_tft_pre5_{0}" -f $RunId)
$BuildRoot = Join-Path $TempRoot 'build'
$BackendProbeBuild = Join-Path $TempRoot 'backend-probe'
$CandidateLibraries = Join-Path $TempRoot 'libraries'
$CandidateJwplcRoot = Join-Path $CandidateLibraries 'JWPLC_TFT'
$CandidateJwplcSrc = Join-Path $CandidateJwplcRoot 'src'
$CandidateBackendRoot = Join-Path $CandidateLibraries 'TFT_eSPI'
$CompileLog = Join-Path $RunRoot 'compile.log'
$UploadLog = Join-Path $RunRoot 'upload.log'
$ClientLog = Join-Path $RunRoot 'client.log'
$SummaryLog = Join-Path $RunRoot 'SUMMARY.log'

New-Item -ItemType Directory -Force -Path $RunRoot | Out-Null
New-Item -ItemType Directory -Force -Path $BuildRoot | Out-Null
New-Item -ItemType Directory -Force -Path $BackendProbeBuild | Out-Null
New-Item -ItemType Directory -Force -Path $CandidateJwplcSrc | Out-Null
New-Item -ItemType Directory -Force -Path $CandidateBackendRoot | Out-Null

. $CommonPath

function Finish-TFTPre5 {
    param(
        [Parameter(Mandatory = $true)][ValidateSet('PASS','REVIEW','FAIL')][string]$Status,
        [Parameter(Mandatory = $true)][string]$Reason,
        [string]$ProductFailure = 'NO',
        [string]$HarnessFailure = 'NO',
        [string]$HardwareFailure = 'NO',
        [string]$EnvironmentFailure = 'NO',
        [int]$ExitCode = 0,
        [string[]]$Extra = @()
    )

    $lines = @(
        'GATE=A13-TFT-PRE5',
        "STATUS=$Status",
        "REASON=$Reason",
        "PRODUCT_FAILURE=$ProductFailure",
        "HARNESS_FAILURE=$HarnessFailure",
        "HARDWARE_FAILURE=$HardwareFailure",
        "ENVIRONMENT_FAILURE=$EnvironmentFailure",
        "RUN_ROOT=$RunRoot",
        "SUMMARY_LOG=$SummaryLog"
    ) + $Extra

    $lines | Set-Content -LiteralPath $SummaryLog -Encoding utf8
    $lines | ForEach-Object { Write-Host $_ }
    exit $ExitCode
}

function Read-SummaryValue {
    param([string]$Path,[string]$Key)
    foreach ($line in Get-Content -LiteralPath $Path) {
        if ($line.StartsWith($Key + '=', [StringComparison]::Ordinal)) {
            return $line.Substring($Key.Length + 1).Trim()
        }
    }
    return $null
}

function Resolve-LatestPre4Candidate {
    $resultRoot = Join-Path $RepoRoot 'tools\alpha13\results'
    if (-not (Test-Path -LiteralPath $resultRoot)) { return $null }

    $summaries = @(
        Get-ChildItem -LiteralPath $resultRoot -Directory -Filter 'tft_pre4_*' -ErrorAction SilentlyContinue |
        Sort-Object Name -Descending |
        ForEach-Object { Join-Path $_.FullName 'SUMMARY.log' } |
        Where-Object { Test-Path -LiteralPath $_ }
    )

    foreach ($summary in $summaries) {
        $status = Read-SummaryValue $summary 'STATUS'
        $reason = Read-SummaryValue $summary 'REASON'
        $root = Read-SummaryValue $summary 'CANDIDATE_ROOT'
        if ($status -eq 'PASS' -and $reason -eq 'TEMP_DEFERRED_DISPON_CANDIDATE_READY' -and
            -not [string]::IsNullOrWhiteSpace($root) -and (Test-Path -LiteralPath $root)) {
            return [pscustomobject]@{ Summary = $summary; Root = $root }
        }
    }
    return $null
}

function Find-LibrarySelection {
    param([string[]]$Lines,[string]$Name)
    $pattern = '^Using library ' + [regex]::Escape($Name) + ' at version .+ in folder: (?<folder>.+)$'
    $hits = @(
        foreach ($line in $Lines) {
            $m = [regex]::Match($line,$pattern)
            if ($m.Success) {
                [IO.Path]::GetFullPath($m.Groups['folder'].Value.Trim()).TrimEnd('\','/')
            }
        }
    )
    if ($hits.Count -ne 1) { return $null }
    return $hits[0]
}

function Find-TftEspiSelection {
    param([string[]]$Lines)
    $hits = @($Lines | Where-Object { $_ -match '^Using library TFT_eSPI at version .+ in folder: .+$' })
    if ($hits.Count -ne 1) { return $null }
    $m = [regex]::Match($hits[0],'^Using library TFT_eSPI at version (?<version>\\S+) in folder: (?<folder>.+)$')
    if (-not $m.Success) { return $null }
    return [pscustomobject]@{
        Version = $m.Groups['version'].Value.Trim()
        Root = [IO.Path]::GetFullPath($m.Groups['folder'].Value.Trim())
    }
}

function Count-Objects {
    param([string]$Root,[string]$Name)
    return @(
        Get-ChildItem -LiteralPath $Root -Recurse -File -ErrorAction SilentlyContinue |
        Where-Object { $_.Name -eq $Name }
    ).Count
}

Write-Host '============================================================'
Write-Host ' A13-TFT-PRE5 - SOURCE-FIRST PHYSICAL CANDIDATE'
Write-Host '============================================================'

Push-Location $RepoRoot
try {
    foreach ($required in @($CommonPath,$ProbeDir,$ClientPath,$OfficialHeader,$RepoLibraries,$BackendProbeDir)) {
        if (-not (Test-Path -LiteralPath $required)) {
            Finish-TFTPre5 -Status 'REVIEW' -Reason "REQUIRED_PATH_MISSING:$required" -HarnessFailure 'YES' -ExitCode 3
        }
    }

    $branch = (git branch --show-current).Trim()
    $head = (git rev-parse HEAD).Trim()
    if ($branch -ne $ExpectedBranch) {
        Finish-TFTPre5 -Status 'REVIEW' -Reason 'UNEXPECTED_BRANCH' -HarnessFailure 'YES' -ExitCode 4
    }

    $entryStatus = @(& git status --porcelain=v1 --untracked-files=normal)
    if ($entryStatus.Count -ne 0) {
        Finish-TFTPre5 -Status 'REVIEW' -Reason 'WORKTREE_NOT_CLEAN' -HarnessFailure 'YES' -ExitCode 5 -Extra @("STATUS=$($entryStatus -join ';')")
    }

    if ([string]::IsNullOrWhiteSpace($CandidateRoot)) {
        $resolved = Resolve-LatestPre4Candidate
        if ($null -eq $resolved) {
            Finish-TFTPre5 -Status 'REVIEW' -Reason 'PRE4_CANDIDATE_NOT_FOUND' -HarnessFailure 'YES' -ExitCode 6
        }
        $CandidateRoot = $resolved.Root
        $pre4Summary = $resolved.Summary
    }
    else {
        $CandidateRoot = [IO.Path]::GetFullPath($CandidateRoot)
        $pre4Summary = 'EXPLICIT_CANDIDATE_ROOT'
    }

    Write-Host "PRE4_CANDIDATE_ROOT=$CandidateRoot"
    Write-Host "PRE4_SUMMARY=$pre4Summary"

    $pre4Cpp = Join-Path $CandidateRoot 'JWPLC_TFT\src\JWPLC_TFT.cpp'
    $pre4Setup = Join-Path $CandidateRoot 'JWPLC_TFT\src\tft_setup.h'
    $pre4Init = Join-Path $CandidateRoot 'TFT_eSPI\ST7789_Init.h'

    foreach ($required in @($pre4Cpp,$pre4Setup,$pre4Init)) {
        if (-not (Test-Path -LiteralPath $required)) {
            Finish-TFTPre5 -Status 'REVIEW' -Reason "PRE4_CANDIDATE_FILE_MISSING:$required" -HarnessFailure 'YES' -ExitCode 7
        }
    }

    $pre4CppSha = Get-A13Sha256 -Path $pre4Cpp
    $pre4SetupSha = Get-A13Sha256 -Path $pre4Setup
    $pre4InitSha = Get-A13Sha256 -Path $pre4Init

    Write-Host "PRE4_CPP_SHA256=$pre4CppSha"
    Write-Host "PRE4_SETUP_SHA256=$pre4SetupSha"
    Write-Host "PRE4_INIT_SHA256=$pre4InitSha"

    if ($pre4CppSha -ne $ExpectedCandidateCppSha -or
        $pre4SetupSha -ne $ExpectedCandidateSetupSha -or
        $pre4InitSha -ne $ExpectedCandidateInitSha) {
        Finish-TFTPre5 -Status 'REVIEW' -Reason 'PRE4_CANDIDATE_IDENTITY_MISMATCH' -HarnessFailure 'YES' -ExitCode 8
    }

    if (-not (Test-Path -LiteralPath $ArduinoCli)) {
        $cli = Get-Command arduino-cli -ErrorAction SilentlyContinue
        if ($null -eq $cli) {
            Finish-TFTPre5 -Status 'REVIEW' -Reason 'ARDUINO_CLI_NOT_FOUND' -EnvironmentFailure 'YES' -ExitCode 9
        }
        $ArduinoCli = $cli.Source
    }

    $cliVersion = @(& $ArduinoCli version 2>&1 | ForEach-Object { $_.ToString() }) -join ' '
    Write-Host "ARDUINO_CLI=$cliVersion"
    if ($cliVersion -notmatch 'Version:\s*1\.0\.2') {
        Finish-TFTPre5 -Status 'REVIEW' -Reason 'ARDUINO_CLI_VERSION_MISMATCH' -EnvironmentFailure 'YES' -ExitCode 10
    }

    $portResolution = Resolve-A13SerialPort -RequestedPort $SerialPort
    Write-Host "PORTS_VISIBLE=$($portResolution.Ports -join ',')"
    Write-Host "PORT_RESOLUTION=$($portResolution.Reason)"
    if ($portResolution.Status -ne 'PASS') {
        Finish-TFTPre5 -Status 'REVIEW' -Reason $portResolution.Reason -EnvironmentFailure 'YES' -ExitCode 11
    }
    $resolvedPort = $portResolution.Port
    Write-Host "SERIAL_PORT=$resolvedPort"

    $pythonCommand = Get-Command python.exe -ErrorAction SilentlyContinue
    if ($null -eq $pythonCommand) { $pythonCommand = Get-Command python -ErrorAction SilentlyContinue }
    if ($null -eq $pythonCommand) {
        Finish-TFTPre5 -Status 'REVIEW' -Reason 'PYTHON_NOT_FOUND' -EnvironmentFailure 'YES' -ExitCode 12
    }
    $pythonExe = $pythonCommand.Source

    $pySerial = Invoke-A13NativeCaptured -FilePath $pythonExe -Arguments @('-c','import serial; print(serial.__version__)')
    if ($pySerial.ExitCode -ne 0) {
        Finish-TFTPre5 -Status 'REVIEW' -Reason 'PYSERIAL_NOT_AVAILABLE' -EnvironmentFailure 'YES' -ExitCode 13
    }

    $backendProbe = Invoke-A13NativeCaptured -FilePath $ArduinoCli -Arguments @(
        'compile','--fqbn',$Fqbn,'-j','0','-v','--clean','--build-path',$BackendProbeBuild,$BackendProbeDir
    )
    if ($backendProbe.ExitCode -ne 0) {
        Finish-TFTPre5 -Status 'REVIEW' -Reason 'BACKEND_RESOLUTION_PROBE_FAILED' -EnvironmentFailure 'YES' -ExitCode 14
    }

    $backendSelection = Find-TftEspiSelection -Lines $backendProbe.Output
    if ($null -eq $backendSelection) {
        Finish-TFTPre5 -Status 'REVIEW' -Reason 'BACKEND_SELECTION_AMBIGUOUS' -HarnessFailure 'YES' -ExitCode 15
    }
    if ($backendSelection.Version -ne $ExpectedBackendVersion) {
        Finish-TFTPre5 -Status 'REVIEW' -Reason 'BACKEND_VERSION_MISMATCH' -EnvironmentFailure 'YES' -ExitCode 16
    }

    $installedBackendRoot = $backendSelection.Root
    $installedBackendCpp = Join-Path $installedBackendRoot 'TFT_eSPI.cpp'
    $installedBackendHeader = Join-Path $installedBackendRoot 'TFT_eSPI.h'

    if ((Get-A13Sha256 -Path $installedBackendCpp) -ne $ExpectedBackendCppSha -or
        (Get-A13Sha256 -Path $installedBackendHeader) -ne $ExpectedBackendHeaderSha) {
        Finish-TFTPre5 -Status 'REVIEW' -Reason 'BACKEND_IDENTITY_MISMATCH' -EnvironmentFailure 'YES' -ExitCode 17
    }

    Get-ChildItem -LiteralPath $installedBackendRoot -Force | ForEach-Object {
        Copy-Item -LiteralPath $_.FullName -Destination $CandidateBackendRoot -Recurse -Force
    }

    Copy-Item -LiteralPath $pre4Init -Destination (Join-Path $CandidateBackendRoot 'TFT_Drivers\ST7789_Init.h') -Force
    Copy-Item -LiteralPath $pre4Setup -Destination (Join-Path $CandidateBackendRoot 'User_Setup.h') -Force

    $utf8 = New-Object Text.UTF8Encoding($false)
    $selectText = "#pragma once`r`n#ifndef USER_SETUP_LOADED`r`n#define USER_SETUP_LOADED`r`n#include `"User_Setup.h`"`r`n#endif`r`n"
    [IO.File]::WriteAllText((Join-Path $CandidateBackendRoot 'User_Setup_Select.h'),$selectText,$utf8)

    Copy-Item -LiteralPath $OfficialHeader -Destination (Join-Path $CandidateJwplcSrc 'JWPLC_TFT.h') -Force
    Copy-Item -LiteralPath $pre4Cpp -Destination (Join-Path $CandidateJwplcSrc 'JWPLC_TFT.cpp') -Force
    Copy-Item -LiteralPath $pre4Setup -Destination (Join-Path $CandidateJwplcSrc 'tft_setup.h') -Force

    $properties = "name=JWPLC_TFT`r`nversion=0.1.0-alpha13-pre5`r`nauthor=JW Control`r`nmaintainer=JW Control`r`nsentence=Alpha13 temporary source-first TFT startup candidate.`r`nparagraph=Temporary candidate only; not a distributable package artifact.`r`ncategory=Display`r`narchitectures=esp32`r`nincludes=JWPLC_TFT.h`r`ndepends=TFT_eSPI,SPI`r`n"
    [IO.File]::WriteAllText((Join-Path $CandidateJwplcRoot 'library.properties'),$properties,$utf8)

    $candidateBackendInit = Join-Path $CandidateBackendRoot 'TFT_Drivers\ST7789_Init.h'
    if ((Get-A13Sha256 -Path $candidateBackendInit) -ne $ExpectedCandidateInitSha) {
        Finish-TFTPre5 -Status 'REVIEW' -Reason 'TEMP_BACKEND_PATCH_IDENTITY_FAILED' -HarnessFailure 'YES' -ExitCode 18
    }

    $compile = Invoke-A13NativeCaptured -FilePath $ArduinoCli -Arguments @(
        'compile','--fqbn',$Fqbn,'-j','0','-v','--clean','--build-path',$BuildRoot,
        '--library',$CandidateJwplcRoot,
        '--library',$CandidateBackendRoot,
        '--libraries',$RepoLibraries,
        $ProbeDir
    )

    $compile.Output | Set-Content -LiteralPath $CompileLog -Encoding utf8
    Write-Host "COMPILE_EXIT=$($compile.ExitCode)"

    if ($compile.ExitCode -ne 0) {
        $compile.Output | Select-Object -Last 160 | ForEach-Object { Write-Host $_ }
        Finish-TFTPre5 -Status 'REVIEW' -Reason 'SOURCE_FIRST_CANDIDATE_COMPILE_FAILED' -HarnessFailure 'YES' -ExitCode 19 -Extra @("COMPILE_LOG=$CompileLog")
    }

    $jwplcSelected = Find-LibrarySelection -Lines $compile.Output -Name 'JWPLC_TFT'
    $backendSelected = Find-LibrarySelection -Lines $compile.Output -Name 'TFT_eSPI'
    $expectedJwplcRoot = [IO.Path]::GetFullPath($CandidateJwplcRoot).TrimEnd('\','/')
    $expectedBackendRoot = [IO.Path]::GetFullPath($CandidateBackendRoot).TrimEnd('\','/')
    $jwplcSelectedOk = $null -ne $jwplcSelected -and $jwplcSelected -ieq $expectedJwplcRoot
    $backendSelectedOk = $null -ne $backendSelected -and $backendSelected -ieq $expectedBackendRoot

    $jwplcObjectCount = Count-Objects -Root $BuildRoot -Name 'JWPLC_TFT.cpp.o'
    $backendObjectCount = Count-Objects -Root $BuildRoot -Name 'TFT_eSPI.cpp.o'

    $jwplcPrecompiled = @(
        $compile.Output | Where-Object {
            ($_ -match 'Using precompiled library|Usando libreria precompilada|Usando biblioteca precompilada') -and
            ($_ -match 'JWPLC_TFT')
        }
    ).Count -gt 0

    $displayPrecompiled = @(
        $compile.Output | Where-Object {
            ($_ -match 'Using precompiled library|Usando libreria precompilada|Usando biblioteca precompilada') -and
            ($_ -match 'JWPLC_Display')
        }
    ).Count -gt 0

    $compileText = $compile.Output -join [Environment]::NewLine
    $usesStubCore = $compileText -match "Using core 'jwcontrol_precompiled_stub'"
    $coreArchiveLinked = $compileText -match '[\\/]precompiled[\\/]core[\\/]JWPLCBASIC[\\/]core\.a'

    Write-Host "JWPLC_TFT_TEMP_SELECTED=$jwplcSelectedOk"
    Write-Host "TFT_ESPI_TEMP_SELECTED=$backendSelectedOk"
    Write-Host "JWPLC_TFT_SOURCE_OBJECT_COUNT=$jwplcObjectCount"
    Write-Host "TFT_ESPI_SOURCE_OBJECT_COUNT=$backendObjectCount"
    Write-Host "JWPLC_TFT_PRECOMPILED=$jwplcPrecompiled"
    Write-Host "JWPLC_DISPLAY_PRECOMPILED=$displayPrecompiled"
    Write-Host "USES_STUB_CORE=$usesStubCore"
    Write-Host "CORE_A_LINKED=$coreArchiveLinked"

    if (-not $jwplcSelectedOk -or -not $backendSelectedOk -or
        $jwplcObjectCount -ne 1 -or $backendObjectCount -ne 1 -or
        $jwplcPrecompiled -or -not $displayPrecompiled -or
        -not $usesStubCore -or -not $coreArchiveLinked) {
        Finish-TFTPre5 -Status 'REVIEW' -Reason 'SOURCE_FIRST_CANDIDATE_BUILD_CONTRACT_FAILED' -HarnessFailure 'YES' -ExitCode 20
    }

    $upload = Invoke-A13NativeCaptured -FilePath $ArduinoCli -Arguments @(
        'upload','--fqbn',$Fqbn,'--port',$resolvedPort,'--input-dir',$BuildRoot,$ProbeDir
    )
    $upload.Output | Set-Content -LiteralPath $UploadLog -Encoding utf8
    Write-Host "UPLOAD_EXIT=$($upload.ExitCode)"

    if ($upload.ExitCode -ne 0) {
        Finish-TFTPre5 -Status 'REVIEW' -Reason 'UPLOAD_FAILED' -HardwareFailure 'YES' -EnvironmentFailure 'YES' -ExitCode 21
    }

    Start-Sleep -Milliseconds 500

    $client = Invoke-A13NativeCaptured -FilePath $pythonExe -Arguments @(
        $ClientPath,'--serial',$resolvedPort,'--baud','115200','--timeout-s','10'
    )
    $client.Output | Set-Content -LiteralPath $ClientLog -Encoding utf8
    Write-Host "CLIENT_EXIT=$($client.ExitCode)"
    $client.Output | ForEach-Object { Write-Host $_ }

    if ($client.ExitCode -ne 0) {
        Finish-TFTPre5 -Status 'REVIEW' -Reason 'CLIENT_FAILED' -ProductFailure 'UNDETERMINED' -HardwareFailure 'YES' -ExitCode 22
    }

    $clientText = $client.Output -join [Environment]::NewLine
    $setupEntry = Get-A13LogInt -Text $clientText -Key 'A13_TFT_PRE1_CLIENT_SETUP_ENTRY_MS'
    $displayReady = Get-A13LogValue -Text $clientText -Key 'A13_TFT_PRE1_CLIENT_DISPLAY_READY'
    $ioReady = Get-A13LogValue -Text $clientText -Key 'A13_TFT_PRE1_CLIENT_IO_READY'
    $clientPass = Get-A13LogValue -Text $clientText -Key 'A13_TFT_PRE1_CLIENT_PASS'

    if ($null -eq $setupEntry -or $displayReady -ne 'YES' -or $ioReady -ne 'YES' -or $clientPass -ne 'YES') {
        Finish-TFTPre5 -Status 'REVIEW' -Reason 'CANDIDATE_RUNTIME_CONTRACT_FAILED' -ProductFailure 'YES' -ExitCode 23
    }

    $finalStatus = @(& git status --porcelain=v1 --untracked-files=normal)
    & git diff --check
    $diffCheck = ($LASTEXITCODE -eq 0)

    if ($finalStatus.Count -ne 0 -or -not $diffCheck) {
        Finish-TFTPre5 -Status 'REVIEW' -Reason 'FINAL_REPO_AUDIT_FAILED' -HarnessFailure 'YES' -ExitCode 24
    }

    Finish-TFTPre5 -Status 'PASS' -Reason 'SOURCE_FIRST_CANDIDATE_FLASHED' -ExitCode 0 -Extra @(
        "HEAD=$head",
        "SERIAL_PORT=$resolvedPort",
        "PRE4_CANDIDATE_ROOT=$CandidateRoot",
        "TEMP_JWPLC_TFT_ROOT=$CandidateJwplcRoot",
        "TEMP_TFT_ESPI_ROOT=$CandidateBackendRoot",
        "JWPLC_TFT_TEMP_SELECTED=$jwplcSelectedOk",
        "TFT_ESPI_TEMP_SELECTED=$backendSelectedOk",
        "JWPLC_TFT_SOURCE_OBJECT_COUNT=$jwplcObjectCount",
        "TFT_ESPI_SOURCE_OBJECT_COUNT=$backendObjectCount",
        "JWPLC_DISPLAY_PRECOMPILED=$displayPrecompiled",
        "USES_STUB_CORE=$usesStubCore",
        "CORE_A_LINKED=$coreArchiveLinked",
        "SETUP_ENTRY_MS=$setupEntry",
        "DISPLAY_READY=$displayReady",
        "IO_READY=$ioReady",
        'VISUAL_POWER_CYCLE_REQUIRED=YES',
        'EXPECTED_VISUAL=OFF_TO_BLACK_TO_IDLE',
        'PRODUCT_REPO_MUTATED=NO',
        'INSTALLED_TFT_ESPI_MUTATED=NO',
        'WORKTREE_FINAL=CLEAN',
        "DIFF_CHECK_FINAL=$diffCheck",
        "COMPILE_LOG=$CompileLog",
        "UPLOAD_LOG=$UploadLog",
        "CLIENT_LOG=$ClientLog"
    )
}
catch {
    Finish-TFTPre5 -Status 'REVIEW' -Reason 'UNEXPECTED_GATE_EXCEPTION' -HarnessFailure 'YES' -ProductFailure 'UNDETERMINED' -ExitCode 90 -Extra @("EXCEPTION=$($_.Exception.Message)")
}
finally {
    Pop-Location
}
