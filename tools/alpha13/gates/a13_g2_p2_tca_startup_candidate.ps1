param(
    [Parameter(Position = 0)][string]$SerialPort = '',
    [string]$ArduinoCli = 'C:\Program Files\Arduino PLC IDE Tools\arduino-cli.exe'
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$ExpectedBranch = 'v2.1.0-alpha.13/feature/cleanup-robustness'
$G2BaseHead = '154a8399b58fd3ef50301061e5819d04d3237e20'
$ExpectedSourceBlob = '23efb3935a34e6b5649875b57c804e60538827cd'
$ExpectedPeripheralsBlob = '875a50fd64552e8c4a4300e07b9494d2c32605d7'
$ExpectedHeaderBlob = '288667f1caa08142e2a155b8c85f24b2aa5beb44'
$ExpectedCoreSha256 = '78d0c0ab14f156b96116529e88872340d51877af40d24ba3559f6081e0bf34fb'
$Fqbn = 'jwplc_local:esp32:jwplcbasic'

$ScriptDir = $PSScriptRoot
$RepoRoot = [IO.Path]::GetFullPath((Join-Path $ScriptDir '..\..\..'))
$CommonPath = Join-Path $ScriptDir 'common.ps1'
$SourceRelative = 'JWPLC/2.1.0/cores/jwcontrol/peripherals_init.cpp'
$PeripheralsRelative = 'JWPLC/2.1.0/cores/jwcontrol/jwplc_peripherals.cpp'
$HeaderRelative = 'JWPLC/2.1.0/cores/jwcontrol/jwplc_peripherals.h'
$CoreRelative = 'JWPLC/2.1.0/precompiled/core/JWPLCBASIC/core.a'
$BoardsLocalRelative = 'JWPLC/2.1.0/boards.local.txt'
$ProbeRelative = 'tools/alpha13/firmware/a13_g2_tca_startup_candidate_probe/a13_g2_tca_startup_candidate_probe.ino'
$ClientRelative = 'tools/alpha13/gates/a13_g2_tca_startup_candidate_client.py'

$SourcePath = Join-Path $RepoRoot $SourceRelative
$PeripheralsPath = Join-Path $RepoRoot $PeripheralsRelative
$HeaderPath = Join-Path $RepoRoot $HeaderRelative
$CorePath = Join-Path $RepoRoot $CoreRelative
$BoardsLocalPath = Join-Path $RepoRoot $BoardsLocalRelative
$ProbePath = Join-Path $RepoRoot $ProbeRelative
$ProbeDir = Split-Path -Parent $ProbePath
$ClientPath = Join-Path $RepoRoot $ClientRelative

$ResultsRoot = Join-Path $RepoRoot 'tools\alpha13\results'
$RunId = Get-Date -Format 'yyyyMMdd_HHmmss'
$RunRoot = Join-Path $ResultsRoot ("g2_p2_{0}" -f $RunId)
$BuildRoot = Join-Path $env:TEMP ("jwplc_a13_g2_p2_{0}" -f $RunId)
$SummaryLog = Join-Path $RunRoot 'SUMMARY.log'

New-Item -ItemType Directory -Path $RunRoot -Force | Out-Null
New-Item -ItemType Directory -Path $BuildRoot -Force | Out-Null

. $CommonPath

function Finish-G2 {
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
        'GATE=A13-G2-P2',
        "STATUS=$Status",
        "REASON=$Reason",
        "PRODUCT_FAILURE=$ProductFailure",
        "HARNESS_FAILURE=$HarnessFailure",
        "HARDWARE_FAILURE=$HardwareFailure",
        "ENVIRONMENT_FAILURE=$EnvironmentFailure",
        "RUN_ROOT=$RunRoot",
        "SUMMARY_LOG=$SummaryLog"
    ) + $Extra

    $lines | Set-Content -LiteralPath $SummaryLog -Encoding UTF8
    $lines | ForEach-Object { Write-Host $_ }
    exit $ExitCode
}

function Get-GitBlobSha {
    param([Parameter(Mandatory = $true)][string]$Path)

    $value = @(& git -C $RepoRoot hash-object -- $Path 2>&1)
    if ($LASTEXITCODE -ne 0 -or $value.Count -ne 1)
    {
        throw "git hash-object fallo para $Path"
    }
    return ([string]$value[0]).Trim()
}

function Get-CompileDbInfo {
    param([Parameter(Mandatory = $true)][string]$BuildPath)

    $compileDbPath = Join-Path $BuildPath 'compile_commands.json'
    if (-not (Test-Path -LiteralPath $compileDbPath))
    {
        throw "compile_commands.json ausente: $compileDbPath"
    }

    $raw = [IO.File]::ReadAllText($compileDbPath)
    $parsed = $raw | ConvertFrom-Json
    $entries = @($parsed)

    if ($entries.Count -eq 1 -and $entries[0] -is [System.Array])
    {
        $entries = @($entries[0])
    }

    $sourceCount = 0
    $stubCount = 0
    $peripheralsInitCount = 0

    foreach ($entry in $entries)
    {
        $file = ([string]$entry.file).Replace('\','/')
        $dir = ([string]$entry.directory).Replace('\','/')

        if (-not [IO.Path]::IsPathRooted($file) -and $dir.Length -gt 0)
        {
            $candidate = $dir.TrimEnd('/') + '/' + $file.TrimStart('/')
        }
        else
        {
            $candidate = $file
        }

        if ($candidate -match '/cores/jwcontrol_precompiled_stub/')
        {
            ++$stubCount
        }
        elseif ($candidate -match '/cores/jwcontrol/')
        {
            ++$sourceCount
            if ($candidate.EndsWith(
                    '/peripherals_init.cpp',
                    [StringComparison]::OrdinalIgnoreCase))
            {
                ++$peripheralsInitCount
            }
        }
    }

    return [PSCustomObject]@{
        Raw = $raw
        EntryCount = $entries.Count
        SourceCount = $sourceCount
        StubCount = $stubCount
        PeripheralsInitCount = $peripheralsInitCount
    }
}

function Restore-BoardsLocal {
    param(
        [bool]$Existed,
        [byte[]]$Bytes
    )

    if ($Existed)
    {
        [IO.File]::WriteAllBytes($BoardsLocalPath, $Bytes)
    }
    elseif (Test-Path -LiteralPath $BoardsLocalPath)
    {
        Remove-Item -LiteralPath $BoardsLocalPath -Force
    }
}

function Enable-SourceCoreOverride {
    param(
        [bool]$Existed,
        [byte[]]$Bytes
    )

    $baseText = ''
    if ($Existed)
    {
        $baseText = [System.Text.Encoding]::UTF8.GetString($Bytes)
    }

    $baseText = [regex]::Replace(
        $baseText,
        '(?ms)^# BEGIN JWPLC_SOURCE_CORE_BUILD\r?\n.*?^# END JWPLC_SOURCE_CORE_BUILD\r?\n?',
        ''
    ).TrimEnd()

    $baseText = [regex]::Replace(
        $baseText,
        '(?ms)^# BEGIN JWPLC_P2_PRECOMPILED_CORE\r?\n.*?^# END JWPLC_P2_PRECOMPILED_CORE\r?\n?',
        ''
    ).TrimEnd()

    $lines = New-Object System.Collections.Generic.List[string]
    if (-not [string]::IsNullOrWhiteSpace($baseText))
    {
        $lines.Add($baseText)
        $lines.Add('')
    }

    $lines.Add('# BEGIN JWPLC_SOURCE_CORE_BUILD')
    $lines.Add('# Temporal A13-G2-P2: source core con perfil completo jwplcbasic.')
    $lines.Add('jwplcbasic.build.core=jwcontrol')
    $lines.Add('jwplcbasic.build.extra_libs=')
    $lines.Add('# END JWPLC_SOURCE_CORE_BUILD')

    $utf8NoBom = New-Object System.Text.UTF8Encoding($false)
    [IO.File]::WriteAllText(
        $BoardsLocalPath,
        (($lines -join [Environment]::NewLine) + [Environment]::NewLine),
        $utf8NoBom
    )
}

function Apply-G2Instrumentation {
    param([byte[]]$OriginalBytes)

    $text = [System.Text.Encoding]::UTF8.GetString($OriginalBytes)
    $crlf = ([string][char]13) + ([string][char]10)
    $nl = if ($text.Contains($crlf)) { $crlf } else { [string][char]10 }

    $anchor = 'static bool g_jwplc_peripherals_initialized = false;'
    $matches = @([regex]::Matches($text, [regex]::Escape($anchor)))
    if ($matches.Count -ne 1)
    {
        throw "A13_G2_SOURCE_ANCHOR_STATE_COUNT=$($matches.Count)"
    }

    $instrument = @(
        $anchor,
        '',
        '#ifndef A13_G2_FAULT_STEP',
        '#define A13_G2_FAULT_STEP 0',
        '#endif',
        '',
        '#if (A13_G2_FAULT_STEP < 0) || (A13_G2_FAULT_STEP > 5)',
        '#error "A13_G2_FAULT_STEP must be 0..5"',
        '#endif',
        '',
        'static volatile uint8_t g_a13_g2_attempt_mask = 0;',
        'static volatile uint8_t g_a13_g2_op_ok_mask = 0;',
        'static volatile bool g_a13_g2_en_io_high_requested = false;',
        '',
        'extern "C" uint8_t a13G2FaultStep(void)',
        '{',
        '    return (uint8_t)A13_G2_FAULT_STEP;',
        '}',
        '',
        'extern "C" uint8_t a13G2AttemptMask(void)',
        '{',
        '    return g_a13_g2_attempt_mask;',
        '}',
        '',
        'extern "C" uint8_t a13G2OpOkMask(void)',
        '{',
        '    return g_a13_g2_op_ok_mask;',
        '}',
        '',
        'extern "C" bool a13G2EnIoHighRequested(void)',
        '{',
        '    return g_a13_g2_en_io_high_requested;',
        '}',
        '',
        'extern "C" bool a13G2PeripheralsInitialized(void)',
        '{',
        '    return g_jwplc_peripherals_initialized;',
        '}'
    ) -join $nl

    $text = $text.Replace($anchor, $instrument)

    $oldConfig = @(
        '    jwplcSystemClearOutputShadow();',
        '',
        '    if (!TCA6424A_writeBank(TCA6424A_DEFAULT_ADDRESS, 1, 0x00) ||',
        '        !TCA6424A_writeBank(TCA6424A_DEFAULT_ADDRESS, 2, 0x00) ||',
        '        !TCA6424A_setBankDirection(TCA6424A_DEFAULT_ADDRESS, 0, 0xFF) ||',
        '        !TCA6424A_setBankDirection(TCA6424A_DEFAULT_ADDRESS, 1, 0x00) ||',
        '        !TCA6424A_setBankDirection(TCA6424A_DEFAULT_ADDRESS, 2, 0xFF))',
        '    {',
        '        return;',
        '    }'
    ) -join $nl

    $configMatches = @([regex]::Matches($text, [regex]::Escape($oldConfig)))
    if ($configMatches.Count -ne 1)
    {
        throw "A13_G2_SOURCE_ANCHOR_CONFIG_COUNT=$($configMatches.Count)"
    }

    $newConfig = @(
        '    jwplcSystemClearOutputShadow();',
        '    g_a13_g2_attempt_mask = 0;',
        '    g_a13_g2_op_ok_mask = 0;',
        '',
        '    g_a13_g2_attempt_mask |= (uint8_t)(1u << 0);',
        '#if A13_G2_FAULT_STEP == 1',
        '    return;',
        '#else',
        '    if (!TCA6424A_writeBank(TCA6424A_DEFAULT_ADDRESS, 1, 0x00)) return;',
        '    g_a13_g2_op_ok_mask |= (uint8_t)(1u << 0);',
        '#endif',
        '',
        '    g_a13_g2_attempt_mask |= (uint8_t)(1u << 1);',
        '#if A13_G2_FAULT_STEP == 2',
        '    return;',
        '#else',
        '    if (!TCA6424A_writeBank(TCA6424A_DEFAULT_ADDRESS, 2, 0x00)) return;',
        '    g_a13_g2_op_ok_mask |= (uint8_t)(1u << 1);',
        '#endif',
        '',
        '    g_a13_g2_attempt_mask |= (uint8_t)(1u << 2);',
        '#if A13_G2_FAULT_STEP == 3',
        '    return;',
        '#else',
        '    if (!TCA6424A_setBankDirection(TCA6424A_DEFAULT_ADDRESS, 0, 0xFF)) return;',
        '    g_a13_g2_op_ok_mask |= (uint8_t)(1u << 2);',
        '#endif',
        '',
        '    g_a13_g2_attempt_mask |= (uint8_t)(1u << 3);',
        '#if A13_G2_FAULT_STEP == 4',
        '    return;',
        '#else',
        '    if (!TCA6424A_setBankDirection(TCA6424A_DEFAULT_ADDRESS, 1, 0x00)) return;',
        '    g_a13_g2_op_ok_mask |= (uint8_t)(1u << 3);',
        '#endif',
        '',
        '    g_a13_g2_attempt_mask |= (uint8_t)(1u << 4);',
        '#if A13_G2_FAULT_STEP == 5',
        '    return;',
        '#else',
        '    if (!TCA6424A_setBankDirection(TCA6424A_DEFAULT_ADDRESS, 2, 0xFF)) return;',
        '    g_a13_g2_op_ok_mask |= (uint8_t)(1u << 4);',
        '#endif'
    ) -join $nl

    $text = $text.Replace($oldConfig, $newConfig)

    $highAnchor = '    gpio_set_level((gpio_num_t)EN_IO, 1);'
    $highMatches = @([regex]::Matches($text, [regex]::Escape($highAnchor)))
    if ($highMatches.Count -ne 1)
    {
        throw "A13_G2_SOURCE_ANCHOR_EN_IO_HIGH_COUNT=$($highMatches.Count)"
    }

    $highReplacement = @(
        '    g_a13_g2_en_io_high_requested = true;',
        '#if A13_G2_FAULT_STEP == 0',
        '    gpio_set_level((gpio_num_t)EN_IO, 1);',
        '#else',
        '    gpio_set_level((gpio_num_t)EN_IO, 0);',
        '#endif'
    ) -join $nl

    $text = $text.Replace($highAnchor, $highReplacement)

    $utf8NoBom = New-Object System.Text.UTF8Encoding($false)
    [IO.File]::WriteAllText($SourcePath, $text, $utf8NoBom)
}

Write-Host '============================================================'
Write-Host ' A13-G2-P2 - TCA STARTUP CANDIDATE SOURCE-FIRST'
Write-Host '============================================================'

Push-Location $RepoRoot
try
{
    foreach ($required in @(
        $CommonPath,
        $SourcePath,
        $CorePath,
        $ProbePath,
        $ClientPath
    ))
    {
        if (-not (Test-Path -LiteralPath $required))
        {
            Finish-G2 -Status 'REVIEW' -Reason "REQUIRED_PATH_MISSING:$required" -HarnessFailure 'YES' -ExitCode 3
        }
    }

    $Branch = (git branch --show-current).Trim()
    $Head = (git rev-parse HEAD).Trim()

    if ($Branch -ne $ExpectedBranch)
    {
        Finish-G2 -Status 'REVIEW' -Reason 'UNEXPECTED_BRANCH' -HarnessFailure 'YES' -ExitCode 4 -Extra @(
            "BRANCH=$Branch",
            "HEAD=$Head"
        )
    }

    & git merge-base --is-ancestor $G2BaseHead $Head
    $baseAncestor = ($LASTEXITCODE -eq 0)

    $committed = @(
        git diff --name-only "$G2BaseHead..$Head" |
            Where-Object { $_ -and $_.Trim().Length -gt 0 }
    )

    $allowed = @(
        'docs/v2.1.0-alpha.13/ALPHA13_STATUS.md',
        'docs/v2.1.0-alpha.13/A13_G2_P2_CANDIDATE_DESIGN_20261007.md',
        'tools/alpha13/candidates/a13_g2_p2_tca_startup.patch',
        'tools/alpha13/gates/apply_a13_g2_p2_candidate.ps1',
        'tools/alpha13/gates/a13_g2_p2_tca_startup_candidate.ps1',
        'tools/alpha13/gates/a13_g2_tca_startup_candidate_client.py',
        'tools/alpha13/gates/run_a13_g2_p2_tca_startup_candidate.bat',
        'tools/alpha13/firmware/a13_g2_tca_startup_candidate_probe/a13_g2_tca_startup_candidate_probe.ino'
    )

    $unexpected = @(
        $committed |
            Where-Object { $_.Replace('\','/') -notin $allowed }
    )

    if (-not $baseAncestor -or $unexpected.Count -ne 0)
    {
        Finish-G2 -Status 'REVIEW' -Reason 'UNEXPECTED_HEAD_TOPOLOGY' -HarnessFailure 'YES' -ExitCode 5 -Extra @(
            "HEAD=$Head",
            "BASELINE_IS_ANCESTOR=$baseAncestor",
            "UNEXPECTED_COMMITTED=$($unexpected -join ';')"
        )
    }

    $dirty = @(Get-A13TrackedDirty | Sort-Object)
    $staged = @(Get-A13Staged)
    $expectedDirty = @(
        $HeaderRelative,
        $PeripheralsRelative,
        $SourceRelative
    ) | Sort-Object
    $dirtyDiff = @(Compare-Object -ReferenceObject $expectedDirty -DifferenceObject $dirty)

    if ($dirtyDiff.Count -ne 0 -or $staged.Count -ne 0)
    {
        Finish-G2 -Status 'REVIEW' -Reason 'CANDIDATE_SCOPE_INVALID' -HarnessFailure 'YES' -ExitCode 6 -Extra @(
            "TRACKED_DIRTY_COUNT=$($dirty.Count)",
            "STAGED_COUNT=$($staged.Count)",
            "DIRTY=$($dirty -join ';')",
            "STAGED=$($staged -join ';')"
        )
    }

    & git diff --check
    if ($LASTEXITCODE -ne 0)
    {
        Finish-G2 -Status 'REVIEW' -Reason 'GIT_DIFF_CHECK_FAILED' -HarnessFailure 'YES' -ExitCode 7
    }

    $sourceBlob = Get-GitBlobSha -Path $SourceRelative
    $peripheralsBlob = Get-GitBlobSha -Path $PeripheralsRelative
    $headerBlob = Get-GitBlobSha -Path $HeaderRelative
    $coreSha = Get-A13Sha256 -Path $CorePath

    Write-Host "BRANCH=$Branch"
    Write-Host "HEAD=$Head"
    Write-Host "SOURCE_BLOB=$sourceBlob"
    Write-Host "PERIPHERALS_BLOB=$peripheralsBlob"
    Write-Host "HEADER_BLOB=$headerBlob"
    Write-Host "CORE_SHA256=$coreSha"

    if (
        $sourceBlob -ne $ExpectedSourceBlob -or
        $peripheralsBlob -ne $ExpectedPeripheralsBlob -or
        $headerBlob -ne $ExpectedHeaderBlob
    )
    {
        Finish-G2 -Status 'REVIEW' -Reason 'CANDIDATE_BLOB_MISMATCH' -HarnessFailure 'YES' -ExitCode 8 -Extra @(
            "SOURCE_BLOB=$sourceBlob",
            "PERIPHERALS_BLOB=$peripheralsBlob",
            "HEADER_BLOB=$headerBlob"
        )
    }

    if ($coreSha -ne $ExpectedCoreSha256)
    {
        Finish-G2 -Status 'REVIEW' -Reason 'CORE_ARCHIVE_BASELINE_MISMATCH' -HarnessFailure 'YES' -ExitCode 9 -Extra @(
            "CORE_SHA256=$coreSha",
            "EXPECTED_CORE_SHA256=$ExpectedCoreSha256"
        )
    }

    $portResolution = Resolve-A13SerialPort -RequestedPort $SerialPort
    Write-Host "PORTS_VISIBLE=$($portResolution.Ports -join ',')"
    Write-Host "PORT_CANDIDATES=$($portResolution.Candidates -join ',')"
    Write-Host "PORT_RESOLUTION=$($portResolution.Reason)"

    if ($portResolution.Status -ne 'PASS')
    {
        Finish-G2 -Status 'REVIEW' -Reason $portResolution.Reason -EnvironmentFailure 'YES' -ExitCode 10
    }

    $ResolvedPort = $portResolution.Port
    Write-Host "SERIAL_PORT=$ResolvedPort"

    if (-not (Test-Path -LiteralPath $ArduinoCli))
    {
        $cliCommand = Get-Command arduino-cli -ErrorAction SilentlyContinue
        if ($null -eq $cliCommand)
        {
            Finish-G2 -Status 'REVIEW' -Reason 'ARDUINO_CLI_NOT_FOUND' -EnvironmentFailure 'YES' -ExitCode 11
        }
        $ArduinoCli = $cliCommand.Source
    }

    $cliVersion = @( & $ArduinoCli version 2>&1 | ForEach-Object { $_.ToString() } ) -join ' '
    Write-Host "ARDUINO_CLI=$cliVersion"
    if ($cliVersion -notmatch 'Version:\s*1\.0\.2')
    {
        Finish-G2 -Status 'REVIEW' -Reason 'ARDUINO_CLI_VERSION_MISMATCH' -EnvironmentFailure 'YES' -ExitCode 12
    }

    $pythonCommand = Get-Command python.exe -ErrorAction SilentlyContinue
    if ($null -eq $pythonCommand)
    {
        $pythonCommand = Get-Command python -ErrorAction SilentlyContinue
    }
    if ($null -eq $pythonCommand)
    {
        Finish-G2 -Status 'REVIEW' -Reason 'PYTHON_NOT_FOUND' -EnvironmentFailure 'YES' -ExitCode 13
    }
    $PythonExe = $pythonCommand.Source

    $pySerial = Invoke-A13NativeCaptured -FilePath $PythonExe -Arguments @(
        '-c',
        'import serial; print(serial.__version__)'
    )
    if ($pySerial.ExitCode -ne 0)
    {
        Finish-G2 -Status 'REVIEW' -Reason 'PYSERIAL_NOT_AVAILABLE' -EnvironmentFailure 'YES' -ExitCode 14
    }

    $clientSyntax = Invoke-A13NativeCaptured -FilePath $PythonExe -Arguments @(
        '-m',
        'py_compile',
        $ClientPath
    )
    if ($clientSyntax.ExitCode -ne 0)
    {
        Finish-G2 -Status 'REVIEW' -Reason 'CLIENT_SYNTAX_ERROR' -HarnessFailure 'YES' -ExitCode 15
    }

    $sourceOriginalBytes = [IO.File]::ReadAllBytes($SourcePath)
    $boardsHadOriginal = Test-Path -LiteralPath $BoardsLocalPath
    $boardsOriginalBytes = if ($boardsHadOriginal)
    {
        [IO.File]::ReadAllBytes($BoardsLocalPath)
    }
    else
    {
        [byte[]]@()
    }

    $builds = @{}
    $compileError = $null

    try
    {
        Apply-G2Instrumentation -OriginalBytes $sourceOriginalBytes
        Enable-SourceCoreOverride -Existed $boardsHadOriginal -Bytes $boardsOriginalBytes

        for ($step = 0; $step -le 5; ++$step)
        {
            $buildPath = Join-Path $BuildRoot ("step{0}" -f $step)
            $compileLog = Join-Path $RunRoot ("compile_step{0}.log" -f $step)
            New-Item -ItemType Directory -Path $buildPath -Force | Out-Null

            $compile = Invoke-A13NativeCaptured -FilePath $ArduinoCli -Arguments @(
                'compile',
                '--fqbn', $Fqbn,
                '-j', '0',
                '-v',
                '--clean',
                '--build-path', $buildPath,
                '--build-property', ("compiler.cpp.extra_flags=-DA13_G2_FAULT_STEP={0}" -f $step),
                $ProbeDir
            )

            $compile.Output | Set-Content -LiteralPath $compileLog -Encoding UTF8
            Write-Host "STEP$($step)_COMPILE_EXIT=$($compile.ExitCode)"
            Write-Host "STEP$($step)_COMPILE_LOG=$compileLog"

            if ($compile.ExitCode -ne 0)
            {
                throw "COMPILE_FAILED_STEP_$step"
            }

            $compileText = $compile.Output -join [Environment]::NewLine
            $db = Get-CompileDbInfo -BuildPath $buildPath

            $usesSource = $compileText -match "Using core 'jwcontrol'"
            $usesStub = $compileText -match "Using core 'jwcontrol_precompiled_stub'"
            $archiveLinked =
                $compileText -match '[\\/]precompiled[\\/]core[\\/]JWPLCBASIC[\\/]core\.a'

            $fullProfile = (
                $db.Raw.Contains('-DJWPLC_HAS_RTC=1') -and
                $db.Raw.Contains('-DJWPLC_HAS_FRAM=1') -and
                $db.Raw.Contains('-DJWPLC_HAS_SD=1') -and
                $db.Raw.Contains('-DJWPLC_HAS_ETHERNET=1')
            )

            Write-Host "STEP$($step)_USES_SOURCE_CORE=$usesSource"
            Write-Host "STEP$($step)_USES_STUB_CORE=$usesStub"
            Write-Host "STEP$($step)_ARCHIVE_LINKED=$archiveLinked"
            Write-Host "STEP$($step)_PERIPHERALS_INIT_COUNT=$($db.PeripheralsInitCount)"
            Write-Host "STEP$($step)_FULL_PROFILE=$fullProfile"

            if (
                -not $usesSource -or
                $usesStub -or
                $archiveLinked -or
                $db.SourceCount -lt 1 -or
                $db.StubCount -ne 0 -or
                $db.PeripheralsInitCount -ne 1 -or
                -not $fullProfile
            )
            {
                throw "SOURCE_CORE_PROOF_FAILED_STEP_$step"
            }

            $builds[$step] = $buildPath
        }
    }
    catch
    {
        $compileError = $_.Exception.Message
    }
    finally
    {
        [IO.File]::WriteAllBytes($SourcePath, $sourceOriginalBytes)
        Restore-BoardsLocal -Existed $boardsHadOriginal -Bytes $boardsOriginalBytes
    }

    $sourceRestored = (Get-GitBlobSha -Path $SourceRelative) -eq $ExpectedSourceBlob
    $peripheralsRestored = (Get-GitBlobSha -Path $PeripheralsRelative) -eq $ExpectedPeripheralsBlob
    $headerRestored = (Get-GitBlobSha -Path $HeaderRelative) -eq $ExpectedHeaderBlob
    $coreRestoredSha = Get-A13Sha256 -Path $CorePath
    $dirtyAfterBuild = @(Get-A13TrackedDirty | Sort-Object)
    $stagedAfterBuild = @(Get-A13Staged)
    $dirtyAfterBuildDiff = @(Compare-Object -ReferenceObject $expectedDirty -DifferenceObject $dirtyAfterBuild)

    $boardsRestored = if ($boardsHadOriginal)
    {
        if (-not (Test-Path -LiteralPath $BoardsLocalPath))
        {
            $false
        }
        else
        {
            $currentBoardsBytes = [IO.File]::ReadAllBytes($BoardsLocalPath)
            [Convert]::ToBase64String($currentBoardsBytes) -eq
                [Convert]::ToBase64String($boardsOriginalBytes)
        }
    }
    else
    {
        -not (Test-Path -LiteralPath $BoardsLocalPath)
    }

    Write-Host "SOURCE_RESTORED=$sourceRestored"
    Write-Host "PERIPHERALS_RESTORED=$peripheralsRestored"
    Write-Host "HEADER_RESTORED=$headerRestored"
    Write-Host "BOARDS_LOCAL_RESTORED=$boardsRestored"
    Write-Host "CORE_SHA256_AFTER_BUILD=$coreRestoredSha"
    Write-Host "DIRTY_AFTER_BUILD=$($dirtyAfterBuild.Count)"
    Write-Host "STAGED_AFTER_BUILD=$($stagedAfterBuild.Count)"

    if (
        -not $sourceRestored -or
        -not $peripheralsRestored -or
        -not $headerRestored -or
        -not $boardsRestored -or
        $coreRestoredSha -ne $ExpectedCoreSha256 -or
        $dirtyAfterBuildDiff.Count -ne 0 -or
        $stagedAfterBuild.Count -ne 0
    )
    {
        Finish-G2 -Status 'REVIEW' -Reason 'TEMPORARY_INSTRUMENTATION_RESTORE_FAILED' -HarnessFailure 'YES' -ExitCode 20
    }

    if ($null -ne $compileError)
    {
        Finish-G2 -Status 'REVIEW' -Reason $compileError -HarnessFailure 'YES' -ExitCode 21
    }

    $allLegsPass = $true
    $extra = New-Object System.Collections.Generic.List[string]

    for ($step = 0; $step -le 5; ++$step)
    {
        $buildPath = [string]$builds[$step]
        $uploadLog = Join-Path $RunRoot ("upload_step{0}.log" -f $step)
        $clientLog = Join-Path $RunRoot ("client_step{0}.log" -f $step)

        $upload = Invoke-A13NativeCaptured -FilePath $ArduinoCli -Arguments @(
            'upload',
            '--fqbn', $Fqbn,
            '--port', $ResolvedPort,
            '--input-dir', $buildPath,
            $ProbeDir
        )

        $upload.Output | Set-Content -LiteralPath $uploadLog -Encoding UTF8
        Write-Host "STEP$($step)_UPLOAD_EXIT=$($upload.ExitCode)"

        if ($upload.ExitCode -ne 0)
        {
            Finish-G2 -Status 'REVIEW' -Reason "UPLOAD_FAILED_STEP_$step" -HardwareFailure 'YES' -EnvironmentFailure 'YES' -ExitCode 30 -Extra @(
                "STEP=$step",
                "UPLOAD_LOG=$uploadLog"
            )
        }

        Start-Sleep -Milliseconds 500

        $clientRun = Invoke-A13NativeCaptured -FilePath $PythonExe -Arguments @(
            $ClientPath,
            '--serial', $ResolvedPort,
            '--baud', '115200',
            '--timeout-s', '8',
            '--expected-step', $step.ToString()
        )

        $clientRun.Output | Set-Content -LiteralPath $clientLog -Encoding UTF8
        Write-Host "STEP$($step)_CLIENT_EXIT=$($clientRun.ExitCode)"
        $clientRun.Output | ForEach-Object { Write-Host $_ }

        if ($clientRun.ExitCode -ne 0)
        {
            Finish-G2 -Status 'REVIEW' -Reason "CLIENT_FAILED_STEP_$step" -HarnessFailure 'YES' -EnvironmentFailure 'YES' -ExitCode 31 -Extra @(
                "STEP=$step",
                "CLIENT_LOG=$clientLog"
            )
        }

        $text = $clientRun.Output -join [Environment]::NewLine
        $faultStep = Get-A13LogInt -Text $text -Key 'FAULT_STEP'
        $attemptMask = Get-A13LogInt -Text $text -Key 'OP_ATTEMPT_MASK'
        $mask = Get-A13LogInt -Text $text -Key 'OP_OK_MASK'
        $highRequested = Get-A13LogValue -Text $text -Key 'EN_IO_HIGH_REQUESTED'
        $outputEnable = Get-A13LogValue -Text $text -Key 'EN_IO_OUTPUT_ENABLE'
        $outputLatch = Get-A13LogValue -Text $text -Key 'EN_IO_OUTPUT_LATCH'
        $padReadback = Get-A13LogValue -Text $text -Key 'EN_IO_PAD_READBACK'
        $peripheralsInitialized = Get-A13LogValue -Text $text -Key 'PERIPHERALS_INITIALIZED'
        $ioStateInitialized = Get-A13LogValue -Text $text -Key 'IO_STATE_INITIALIZED'
        $ioViewReady = Get-A13LogValue -Text $text -Key 'IO_VIEW_READY'

        if ($step -eq 0)
        {
            $legPass = (
                $faultStep -eq 0 -and
                $attemptMask -eq 31 -and
                $mask -eq 31 -and
                $highRequested -eq 'YES' -and
                $outputEnable -eq 'YES' -and
                $outputLatch -eq 'HIGH' -and
                $peripheralsInitialized -eq 'YES' -and
                $ioStateInitialized -eq 'YES' -and
                $ioViewReady -eq 'YES'
            )
        }
        else
        {
            $expectedAttemptMask = (1 -shl $step) - 1
            $expectedOkMask = (1 -shl ($step - 1)) - 1

            $legPass = (
                $faultStep -eq $step -and
                $attemptMask -eq $expectedAttemptMask -and
                $mask -eq $expectedOkMask -and
                $highRequested -eq 'NO' -and
                $outputEnable -eq 'YES' -and
                $outputLatch -eq 'LOW' -and
                $peripheralsInitialized -eq 'NO' -and
                $ioStateInitialized -eq 'NO' -and
                $ioViewReady -eq 'NO'
            )
        }

        Write-Host "STEP$($step)_CONTRACT_PASS=$legPass"

        [void]$extra.Add("STEP$($step)_OP_ATTEMPT_MASK=$attemptMask")
        [void]$extra.Add("STEP$($step)_OP_OK_MASK=$mask")
        [void]$extra.Add("STEP$($step)_EN_IO_HIGH_REQUESTED=$highRequested")
        [void]$extra.Add("STEP$($step)_EN_IO_OUTPUT_ENABLE=$outputEnable")
        [void]$extra.Add("STEP$($step)_EN_IO_OUTPUT_LATCH=$outputLatch")
        [void]$extra.Add("STEP$($step)_EN_IO_PAD_READBACK=$padReadback")
        [void]$extra.Add("STEP$($step)_IO_READY=$ioViewReady")
        [void]$extra.Add("STEP$($step)_CONTRACT_PASS=$legPass")
        [void]$extra.Add("STEP$($step)_CLIENT_LOG=$clientLog")

        if (-not $legPass)
        {
            $allLegsPass = $false
        }
    }

    $sourceFinal = (Get-GitBlobSha -Path $SourceRelative) -eq $ExpectedSourceBlob
    $peripheralsFinal = (Get-GitBlobSha -Path $PeripheralsRelative) -eq $ExpectedPeripheralsBlob
    $headerFinal = (Get-GitBlobSha -Path $HeaderRelative) -eq $ExpectedHeaderBlob
    $coreFinal = Get-A13Sha256 -Path $CorePath
    $dirtyFinal = @(Get-A13TrackedDirty | Sort-Object)
    $stagedFinal = @(Get-A13Staged)
    $dirtyFinalDiff = @(Compare-Object -ReferenceObject $expectedDirty -DifferenceObject $dirtyFinal)

    & git diff --check
    $diffCheckFinal = ($LASTEXITCODE -eq 0)

    [void]$extra.Add("SOURCE_RESTORED_FINAL=$sourceFinal")
    [void]$extra.Add("PERIPHERALS_RESTORED_FINAL=$peripheralsFinal")
    [void]$extra.Add("HEADER_RESTORED_FINAL=$headerFinal")
    [void]$extra.Add("CORE_SHA256_FINAL=$coreFinal")
    [void]$extra.Add("TRACKED_DIRTY_FINAL=$($dirtyFinal.Count)")
    [void]$extra.Add("STAGED_FINAL=$($stagedFinal.Count)")
    [void]$extra.Add("DIFF_CHECK_FINAL=$diffCheckFinal")

    if (
        -not $sourceFinal -or
        -not $peripheralsFinal -or
        -not $headerFinal -or
        $coreFinal -ne $ExpectedCoreSha256 -or
        $dirtyFinalDiff.Count -ne 0 -or
        $stagedFinal.Count -ne 0 -or
        -not $diffCheckFinal
    )
    {
        Finish-G2 -Status 'REVIEW' -Reason 'FINAL_REPO_AUDIT_FAILED' -HarnessFailure 'YES' -ExitCode 40 -Extra $extra.ToArray()
    }

    if (-not $allLegsPass)
    {
        Finish-G2 -Status 'FAIL' -Reason 'CANDIDATE_SOURCE_FIRST_CONTRACT_FAILED' -HarnessFailure 'NO' -ProductFailure 'YES' -ExitCode 41 -Extra $extra.ToArray()
    }

    Finish-G2 -Status 'PASS' -Reason 'CANDIDATE_SOURCE_FIRST_PASS' -ProductFailure 'NO' -ExitCode 0 -Extra $extra.ToArray()
}
catch
{
    Finish-G2 -Status 'REVIEW' -Reason 'UNEXPECTED_GATE_EXCEPTION' -HarnessFailure 'YES' -ProductFailure 'UNDETERMINED' -ExitCode 90 -Extra @(
        "EXCEPTION=$($_.Exception.Message)"
    )
}
finally
{
    Pop-Location
}
