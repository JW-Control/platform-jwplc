param(
    [Parameter(Position = 0)][string]$SerialPort = '',
    [string]$ArduinoCli = 'C:\Program Files\Arduino PLC IDE Tools\arduino-cli.exe'
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$ExpectedBranch = 'v2.1.0-alpha.13/feature/cleanup-robustness'
$ProductHead = '6a585693c20c4f44bd789e00a7ae65b9828bcdec'
$ExpectedSourceBlob = '23efb3935a34e6b5649875b57c804e60538827cd'
$ExpectedCoreSha256 = '6f328eeb796091070c8d852a2c0e90f71047d8d2b5786fe3cd5079b2fb6ff983'
$Fqbn = 'jwplc_local:esp32:jwplcbasic'

$ScriptDir = $PSScriptRoot
$RepoRoot = [IO.Path]::GetFullPath((Join-Path $ScriptDir '..\..\..'))
$CommonPath = Join-Path $ScriptDir 'common.ps1'
$SourceRelative = 'JWPLC/2.1.0/cores/jwcontrol/peripherals_init.cpp'
$CoreRelative = 'JWPLC/2.1.0/precompiled/core/JWPLCBASIC/core.a'
$BoardsLocalRelative = 'JWPLC/2.1.0/boards.local.txt'
$ProbeRelative = 'tools/alpha13/firmware/a13_tft_pre2_internal_timeline_probe/a13_tft_pre2_internal_timeline_probe.ino'
$ClientRelative = 'tools/alpha13/gates/a13_tft_pre2_internal_timeline_client.py'

$SourcePath = Join-Path $RepoRoot $SourceRelative
$CorePath = Join-Path $RepoRoot $CoreRelative
$BoardsLocalPath = Join-Path $RepoRoot $BoardsLocalRelative
$ProbePath = Join-Path $RepoRoot $ProbeRelative
$ProbeDir = Split-Path -Parent $ProbePath
$ClientPath = Join-Path $RepoRoot $ClientRelative

$RunId = Get-Date -Format 'yyyyMMdd_HHmmss'
$RunRoot = Join-Path $RepoRoot ("tools\alpha13\results\tft_pre2_{0}" -f $RunId)
$BuildRoot = Join-Path $env:TEMP ("jwplc_a13_tft_pre2_{0}" -f $RunId)
$CompileLog = Join-Path $RunRoot 'compile.log'
$UploadLog = Join-Path $RunRoot 'upload.log'
$ClientLog = Join-Path $RunRoot 'client.log'
$SummaryLog = Join-Path $RunRoot 'SUMMARY.log'

New-Item -ItemType Directory -Path $RunRoot -Force | Out-Null
New-Item -ItemType Directory -Path $BuildRoot -Force | Out-Null

. $CommonPath

function Finish-TFTPre2
{
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
        'GATE=A13-TFT-PRE2',
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

function Get-GitBlobSha
{
    param([Parameter(Mandatory = $true)][string]$Path)
    $value = @(& git -C $RepoRoot hash-object -- $Path 2>&1)
    if ($LASTEXITCODE -ne 0 -or $value.Count -ne 1)
    {
        throw "git hash-object failed for $Path"
    }
    return ([string]$value[0]).Trim()
}

function Restore-BoardsLocal
{
    param([bool]$Existed, [byte[]]$Bytes)
    if ($Existed)
    {
        [IO.File]::WriteAllBytes($BoardsLocalPath, $Bytes)
    }
    elseif (Test-Path -LiteralPath $BoardsLocalPath)
    {
        Remove-Item -LiteralPath $BoardsLocalPath -Force
    }
}

function Enable-SourceCoreOverride
{
    param([bool]$Existed, [byte[]]$Bytes)
    $baseText = ''
    if ($Existed)
    {
        $baseText = [Text.Encoding]::UTF8.GetString($Bytes)
    }
    $baseText = [regex]::Replace(
        $baseText,
        '(?ms)^# BEGIN JWPLC_SOURCE_CORE_BUILD\r?\n.*?^# END JWPLC_SOURCE_CORE_BUILD\r?\n?',
        ''
    ).TrimEnd()

    $parts = New-Object System.Collections.Generic.List[string]
    if (-not [string]::IsNullOrWhiteSpace($baseText))
    {
        $parts.Add($baseText)
        $parts.Add('')
    }
    $parts.Add('# BEGIN JWPLC_SOURCE_CORE_BUILD')
    $parts.Add('# Temporary A13 TFT-PRE2 source-core timing instrumentation.')
    $parts.Add('jwplcbasic.build.core=jwcontrol')
    $parts.Add('jwplcbasic.build.extra_libs=')
    $parts.Add('# END JWPLC_SOURCE_CORE_BUILD')

    $utf8 = New-Object Text.UTF8Encoding($false)
    [IO.File]::WriteAllText(
        $BoardsLocalPath,
        (($parts -join [Environment]::NewLine) + [Environment]::NewLine),
        $utf8
    )
}

function Replace-ExactlyOnce
{
    param(
        [string]$Text,
        [string]$Old,
        [string]$New,
        [string]$Name
    )
    $count = @([regex]::Matches($Text, [regex]::Escape($Old))).Count
    if ($count -ne 1)
    {
        throw "A13_TFT_PRE2_ANCHOR_${Name}_COUNT=$count"
    }
    return $Text.Replace($Old, $New)
}

function Apply-TimingInstrumentation
{
    param([byte[]]$OriginalBytes)

    $text = [Text.Encoding]::UTF8.GetString($OriginalBytes)
    $crlf = ([string][char]13) + ([string][char]10)
    $nl = if ($text.Contains($crlf)) { $crlf } else { [string][char]10 }

    $text = Replace-ExactlyOnce -Text $text `
        -Old '#include "driver/gpio.h"' `
        -New ('#include "driver/gpio.h"' + $nl + '#include "esp_timer.h"') `
        -Name 'INCLUDE'

    $anchor = 'static bool g_jwplc_peripherals_initialized = false;'
    $globals = @(
        $anchor,
        '',
        'extern "C"',
        '{',
        'volatile uint64_t a13_tft_pre2_init_entry_us = 0;',
        'volatile uint64_t a13_tft_pre2_rst_low_us = 0;',
        'volatile uint64_t a13_tft_pre2_state_end_us = 0;',
        'volatile uint64_t a13_tft_pre2_i2c_end_us = 0;',
        'volatile uint64_t a13_tft_pre2_rtc_end_us = 0;',
        'volatile uint64_t a13_tft_pre2_fram_end_us = 0;',
        'volatile uint64_t a13_tft_pre2_sd_end_us = 0;',
        'volatile uint64_t a13_tft_pre2_buttons_end_us = 0;',
        'volatile uint64_t a13_tft_pre2_display_begin_start_us = 0;',
        'volatile uint64_t a13_tft_pre2_display_begin_end_us = 0;',
        'volatile uint64_t a13_tft_pre2_display_refresh_end_us = 0;',
        'volatile uint64_t a13_tft_pre2_tca_init_end_us = 0;',
        'volatile uint64_t a13_tft_pre2_tca_config_end_us = 0;',
        'volatile uint64_t a13_tft_pre2_en_io_high_us = 0;',
        'volatile uint64_t a13_tft_pre2_init_end_us = 0;',
        'volatile bool a13_tft_pre2_display_begin_ok = false;',
        '}'
    ) -join $nl
    $text = Replace-ExactlyOnce -Text $text -Old $anchor -New $globals -Name 'GLOBALS'

    $entryOld = '#if defined(JWPLC_BASIC)' + $nl + '    gpio_set_direction((gpio_num_t)EN_IO, GPIO_MODE_OUTPUT);'
    $entryNew = '#if defined(JWPLC_BASIC)' + $nl + '    a13_tft_pre2_init_entry_us = (uint64_t)esp_timer_get_time();' + $nl + '    gpio_set_direction((gpio_num_t)EN_IO, GPIO_MODE_OUTPUT);'
    $text = Replace-ExactlyOnce -Text $text -Old $entryOld -New $entryNew -Name 'INIT_ENTRY'

    $text = Replace-ExactlyOnce -Text $text `
        -Old '    gpio_set_level((gpio_num_t)JWPLC_TFT_RST, 0);' `
        -New ('    gpio_set_level((gpio_num_t)JWPLC_TFT_RST, 0);' + $nl + '    a13_tft_pre2_rst_low_us = (uint64_t)esp_timer_get_time();') `
        -Name 'RST_LOW'

    $stateOld = '    vTaskDelay(pdMS_TO_TICKS(2));' + $nl + $nl + '    jwplcSystemInitState();'
    $stateNew = '    vTaskDelay(pdMS_TO_TICKS(2));' + $nl + $nl + '    jwplcSystemInitState();' + $nl + '    a13_tft_pre2_state_end_us = (uint64_t)esp_timer_get_time();'
    $text = Replace-ExactlyOnce -Text $text -Old $stateOld -New $stateNew -Name 'STATE_END'

    $i2cOld = @(
        '    if (jwplcI2C_begin() != 0)',
        '    {',
        '        return;',
        '    }'
    ) -join $nl
    $text = Replace-ExactlyOnce -Text $text -Old $i2cOld -New ($i2cOld + $nl + '    a13_tft_pre2_i2c_end_us = (uint64_t)esp_timer_get_time();') -Name 'I2C_END'

    $rtcBoundary = '#endif' + $nl + $nl + '#if JWPLC_HAS_FRAM'
    $text = Replace-ExactlyOnce -Text $text -Old $rtcBoundary -New ('#endif' + $nl + '    a13_tft_pre2_rtc_end_us = (uint64_t)esp_timer_get_time();' + $nl + $nl + '#if JWPLC_HAS_FRAM') -Name 'RTC_END'

    $framBoundary = '#endif' + $nl + $nl + '    // microSD no es crítica para permitir que el resto del sistema arranque.'
    $text = Replace-ExactlyOnce -Text $text -Old $framBoundary -New ('#endif' + $nl + '    a13_tft_pre2_fram_end_us = (uint64_t)esp_timer_get_time();' + $nl + $nl + '    // microSD no es crítica para permitir que el resto del sistema arranque.') -Name 'FRAM_END'

    $text = Replace-ExactlyOnce -Text $text `
        -Old '    (void)jwplcSDBeginCallback();' `
        -New ('    (void)jwplcSDBeginCallback();' + $nl + '    a13_tft_pre2_sd_end_us = (uint64_t)esp_timer_get_time();') `
        -Name 'SD_END'

    $text = Replace-ExactlyOnce -Text $text `
        -Old '    (void)jwplcButtonsBeginCallback();' `
        -New ('    (void)jwplcButtonsBeginCallback();' + $nl + '    a13_tft_pre2_buttons_end_us = (uint64_t)esp_timer_get_time();') `
        -Name 'BUTTONS_END'

    $displayOld = @(
        '    if (jwplcDisplayBeginCallback())',
        '    {',
        '        jwplcDisplayRefreshCallback(jwplcGetIOState(), jwplcGetRTCState());',
        '    }'
    ) -join $nl
    $displayNew = @(
        '    a13_tft_pre2_display_begin_start_us = (uint64_t)esp_timer_get_time();',
        '    a13_tft_pre2_display_begin_ok = jwplcDisplayBeginCallback();',
        '    a13_tft_pre2_display_begin_end_us = (uint64_t)esp_timer_get_time();',
        '    if (a13_tft_pre2_display_begin_ok)',
        '    {',
        '        jwplcDisplayRefreshCallback(jwplcGetIOState(), jwplcGetRTCState());',
        '    }',
        '    a13_tft_pre2_display_refresh_end_us = (uint64_t)esp_timer_get_time();'
    ) -join $nl
    $text = Replace-ExactlyOnce -Text $text -Old $displayOld -New $displayNew -Name 'DISPLAY'

    $tcaOld = @(
        '    if (!TCA6424A_init(TCA6424A_DEFAULT_ADDRESS))',
        '    {',
        '        return;',
        '    }'
    ) -join $nl
    $text = Replace-ExactlyOnce -Text $text -Old $tcaOld -New ($tcaOld + $nl + '    a13_tft_pre2_tca_init_end_us = (uint64_t)esp_timer_get_time();') -Name 'TCA_INIT'

    $highOld = '    gpio_set_level((gpio_num_t)EN_IO, 1);'
    $highNew = @(
        '    a13_tft_pre2_tca_config_end_us = (uint64_t)esp_timer_get_time();',
        '    gpio_set_level((gpio_num_t)EN_IO, 1);',
        '    a13_tft_pre2_en_io_high_us = (uint64_t)esp_timer_get_time();'
    ) -join $nl
    $text = Replace-ExactlyOnce -Text $text -Old $highOld -New $highNew -Name 'EN_IO_HIGH'

    $endOld = @(
        '    g_jwplc_peripherals_initialized = true;',
        '    jwplcSystemSetIOReady(true);',
        '#else'
    ) -join $nl
    $endNew = @(
        '    g_jwplc_peripherals_initialized = true;',
        '    jwplcSystemSetIOReady(true);',
        '    a13_tft_pre2_init_end_us = (uint64_t)esp_timer_get_time();',
        '#else'
    ) -join $nl
    $text = Replace-ExactlyOnce -Text $text -Old $endOld -New $endNew -Name 'INIT_END'

    $utf8 = New-Object Text.UTF8Encoding($false)
    [IO.File]::WriteAllText($SourcePath, $text, $utf8)
}

function Get-CompileInfo
{
    param([string]$BuildPath)
    $dbPath = Join-Path $BuildPath 'compile_commands.json'
    if (-not (Test-Path -LiteralPath $dbPath)) { throw 'compile_commands.json missing' }
    $raw = [IO.File]::ReadAllText($dbPath)
    $parsed = $raw | ConvertFrom-Json
    $entries = @($parsed)
    if ($entries.Count -eq 1 -and $entries[0] -is [System.Array]) { $entries = @($entries[0]) }
    $sourceCount = 0
    $stubCount = 0
    $peripheralsCount = 0
    foreach ($entry in $entries)
    {
        $file = ([string]$entry.file).Replace('\','/')
        $dir = ([string]$entry.directory).Replace('\','/')
        $candidate = if (-not [IO.Path]::IsPathRooted($file) -and $dir.Length -gt 0) { $dir.TrimEnd('/') + '/' + $file.TrimStart('/') } else { $file }
        if ($candidate -match '/cores/jwcontrol_precompiled_stub/') { ++$stubCount }
        elseif ($candidate -match '/cores/jwcontrol/')
        {
            ++$sourceCount
            if ($candidate.EndsWith('/peripherals_init.cpp',[StringComparison]::OrdinalIgnoreCase)) { ++$peripheralsCount }
        }
    }
    return [PSCustomObject]@{ Raw=$raw; Entries=$entries.Count; SourceCount=$sourceCount; StubCount=$stubCount; PeripheralsCount=$peripheralsCount }
}

Write-Host '============================================================'
Write-Host ' A13-TFT-PRE2 - INTERNAL STARTUP TIMELINE'
Write-Host '============================================================'

Push-Location $RepoRoot
try
{
    foreach ($required in @($CommonPath,$SourcePath,$CorePath,$ProbePath,$ClientPath))
    {
        if (-not (Test-Path -LiteralPath $required))
        {
            Finish-TFTPre2 -Status 'REVIEW' -Reason "REQUIRED_PATH_MISSING:$required" -HarnessFailure 'YES' -ExitCode 3
        }
    }

    $branch = (git branch --show-current).Trim()
    $head = (git rev-parse HEAD).Trim()
    if ($branch -ne $ExpectedBranch)
    {
        Finish-TFTPre2 -Status 'REVIEW' -Reason 'UNEXPECTED_BRANCH' -HarnessFailure 'YES' -ExitCode 4
    }
    & git merge-base --is-ancestor $ProductHead $head
    if ($LASTEXITCODE -ne 0)
    {
        Finish-TFTPre2 -Status 'REVIEW' -Reason 'G2_PRODUCT_HEAD_NOT_ANCESTOR' -HarnessFailure 'YES' -ExitCode 5
    }

    $committedProduct = @(& git diff --name-only "$ProductHead..$head" -- 'JWPLC/2.1.0/cores/jwcontrol' $CoreRelative | Where-Object { $_ })
    if ($committedProduct.Count -ne 0)
    {
        Finish-TFTPre2 -Status 'REVIEW' -Reason 'UNEXPECTED_PRODUCT_CHANGE_AFTER_G2' -HarnessFailure 'YES' -ExitCode 6 -Extra @("COMMITTED_PRODUCT=$($committedProduct -join ';')")
    }

    $porcelain = @(& git status --porcelain=v1 --untracked-files=normal)
    if ($porcelain.Count -ne 0)
    {
        Finish-TFTPre2 -Status 'REVIEW' -Reason 'WORKTREE_NOT_CLEAN' -HarnessFailure 'YES' -ExitCode 7 -Extra @("STATUS=$($porcelain -join ';')")
    }

    if ((Get-GitBlobSha -Path $SourceRelative) -ne $ExpectedSourceBlob)
    {
        Finish-TFTPre2 -Status 'REVIEW' -Reason 'SOURCE_BLOB_MISMATCH' -HarnessFailure 'YES' -ExitCode 8
    }
    if ((Get-A13Sha256 -Path $CorePath) -ne $ExpectedCoreSha256)
    {
        Finish-TFTPre2 -Status 'REVIEW' -Reason 'CORE_SHA_MISMATCH' -HarnessFailure 'YES' -ExitCode 9
    }

    $portResolution = Resolve-A13SerialPort -RequestedPort $SerialPort
    Write-Host "PORTS_VISIBLE=$($portResolution.Ports -join ',')"
    Write-Host "PORT_RESOLUTION=$($portResolution.Reason)"
    if ($portResolution.Status -ne 'PASS')
    {
        Finish-TFTPre2 -Status 'REVIEW' -Reason $portResolution.Reason -EnvironmentFailure 'YES' -ExitCode 10
    }
    $resolvedPort = $portResolution.Port
    Write-Host "SERIAL_PORT=$resolvedPort"

    if (-not (Test-Path -LiteralPath $ArduinoCli))
    {
        $cli = Get-Command arduino-cli -ErrorAction SilentlyContinue
        if ($null -eq $cli)
        {
            Finish-TFTPre2 -Status 'REVIEW' -Reason 'ARDUINO_CLI_NOT_FOUND' -EnvironmentFailure 'YES' -ExitCode 11
        }
        $ArduinoCli = $cli.Source
    }
    $cliVersion = @(& $ArduinoCli version 2>&1 | ForEach-Object { $_.ToString() }) -join ' '
    Write-Host "ARDUINO_CLI=$cliVersion"
    if ($cliVersion -notmatch 'Version:\s*1\.0\.2')
    {
        Finish-TFTPre2 -Status 'REVIEW' -Reason 'ARDUINO_CLI_VERSION_MISMATCH' -EnvironmentFailure 'YES' -ExitCode 12
    }

    $pythonCommand = Get-Command python.exe -ErrorAction SilentlyContinue
    if ($null -eq $pythonCommand) { $pythonCommand = Get-Command python -ErrorAction SilentlyContinue }
    if ($null -eq $pythonCommand)
    {
        Finish-TFTPre2 -Status 'REVIEW' -Reason 'PYTHON_NOT_FOUND' -EnvironmentFailure 'YES' -ExitCode 13
    }
    $pythonExe = $pythonCommand.Source
    $pySerial = Invoke-A13NativeCaptured -FilePath $pythonExe -Arguments @('-c','import serial; print(serial.__version__)')
    if ($pySerial.ExitCode -ne 0)
    {
        Finish-TFTPre2 -Status 'REVIEW' -Reason 'PYSERIAL_NOT_AVAILABLE' -EnvironmentFailure 'YES' -ExitCode 14
    }
    $clientSyntax = Invoke-A13NativeCaptured -FilePath $pythonExe -Arguments @('-c','import ast,pathlib,sys; ast.parse(pathlib.Path(sys.argv[1]).read_text(encoding="utf-8")); print("PY_SYNTAX=PASS")',$ClientPath)
    if ($clientSyntax.ExitCode -ne 0)
    {
        Finish-TFTPre2 -Status 'REVIEW' -Reason 'CLIENT_SYNTAX_ERROR' -HarnessFailure 'YES' -ExitCode 15
    }

    $sourceOriginal = [IO.File]::ReadAllBytes($SourcePath)
    $boardsExisted = Test-Path -LiteralPath $BoardsLocalPath
    $boardsOriginal = if ($boardsExisted) { [IO.File]::ReadAllBytes($BoardsLocalPath) } else { [byte[]]@() }
    $compileError = $null
    try
    {
        Apply-TimingInstrumentation -OriginalBytes $sourceOriginal
        Enable-SourceCoreOverride -Existed $boardsExisted -Bytes $boardsOriginal
        $compile = Invoke-A13NativeCaptured -FilePath $ArduinoCli -Arguments @(
            'compile','--fqbn',$Fqbn,'-j','0','-v','--clean','--build-path',$BuildRoot,$ProbeDir
        )
        $compile.Output | Set-Content -LiteralPath $CompileLog -Encoding utf8
        Write-Host "COMPILE_EXIT=$($compile.ExitCode)"
        if ($compile.ExitCode -ne 0)
        {
            $compileError = 'SOURCE_FIRST_COMPILE_FAILED'
        }
        else
        {
            $compileText = $compile.Output -join [Environment]::NewLine
            $proof = Get-CompileInfo -BuildPath $BuildRoot
            $usesSource = $compileText -match "Using core 'jwcontrol'"
            $usesStub = $compileText -match "Using core 'jwcontrol_precompiled_stub'"
            $archiveLinked = $compileText -match '[\\/]precompiled[\\/]core[\\/]JWPLCBASIC[\\/]core\.a'
            $fullProfile = $proof.Raw.Contains('-DJWPLC_HAS_RTC=1') -and $proof.Raw.Contains('-DJWPLC_HAS_FRAM=1') -and $proof.Raw.Contains('-DJWPLC_HAS_SD=1') -and $proof.Raw.Contains('-DJWPLC_HAS_ETHERNET=1')
            Write-Host "USES_SOURCE_CORE=$usesSource"
            Write-Host "USES_STUB_CORE=$usesStub"
            Write-Host "CORE_A_LINKED=$archiveLinked"
            Write-Host "SOURCE_TU_COUNT=$($proof.SourceCount)"
            Write-Host "STUB_TU_COUNT=$($proof.StubCount)"
            Write-Host "PERIPHERALS_INIT_COUNT=$($proof.PeripheralsCount)"
            Write-Host "FULL_PROFILE=$fullProfile"
            if (-not $usesSource -or $usesStub -or $archiveLinked -or $proof.PeripheralsCount -ne 1 -or -not $fullProfile)
            {
                $compileError = 'SOURCE_FIRST_BUILD_CONTRACT_FAILED'
            }
        }
    }
    catch
    {
        $compileError = 'TEMPORARY_INSTRUMENTATION_EXCEPTION:' + $_.Exception.Message
    }
    finally
    {
        [IO.File]::WriteAllBytes($SourcePath, $sourceOriginal)
        Restore-BoardsLocal -Existed $boardsExisted -Bytes $boardsOriginal
    }

    $sourceRestored = (Get-GitBlobSha -Path $SourceRelative) -eq $ExpectedSourceBlob
    $corePreserved = (Get-A13Sha256 -Path $CorePath) -eq $ExpectedCoreSha256
    $porcelainAfterBuild = @(& git status --porcelain=v1 --untracked-files=normal)
    Write-Host "SOURCE_RESTORED=$sourceRestored"
    Write-Host "CORE_SHA_PRESERVED=$corePreserved"
    Write-Host "STATUS_AFTER_BUILD_COUNT=$($porcelainAfterBuild.Count)"
    if (-not $sourceRestored -or -not $corePreserved -or $porcelainAfterBuild.Count -ne 0)
    {
        Finish-TFTPre2 -Status 'REVIEW' -Reason 'TEMPORARY_INSTRUMENTATION_RESTORE_FAILED' -HarnessFailure 'YES' -ExitCode 20
    }
    if ($null -ne $compileError)
    {
        Finish-TFTPre2 -Status 'REVIEW' -Reason $compileError -HarnessFailure 'YES' -ExitCode 21 -Extra @("COMPILE_LOG=$CompileLog")
    }

    $upload = Invoke-A13NativeCaptured -FilePath $ArduinoCli -Arguments @(
        'upload','--fqbn',$Fqbn,'--port',$resolvedPort,'--input-dir',$BuildRoot,$ProbeDir
    )
    $upload.Output | Set-Content -LiteralPath $UploadLog -Encoding utf8
    Write-Host "UPLOAD_EXIT=$($upload.ExitCode)"
    if ($upload.ExitCode -ne 0)
    {
        Finish-TFTPre2 -Status 'REVIEW' -Reason 'UPLOAD_FAILED' -HardwareFailure 'YES' -EnvironmentFailure 'YES' -ExitCode 30
    }

    Start-Sleep -Milliseconds 500
    $client = Invoke-A13NativeCaptured -FilePath $pythonExe -Arguments @($ClientPath,'--serial',$resolvedPort,'--baud','115200','--timeout-s','10')
    $client.Output | Set-Content -LiteralPath $ClientLog -Encoding utf8
    Write-Host "CLIENT_EXIT=$($client.ExitCode)"
    $client.Output | ForEach-Object { Write-Host $_ }
    if ($client.ExitCode -ne 0)
    {
        Finish-TFTPre2 -Status 'REVIEW' -Reason 'CLIENT_FAILED' -ProductFailure 'UNDETERMINED' -HarnessFailure $(if ($client.ExitCode -eq 4) { 'YES' } else { 'NO' }) -ExitCode 31 -Extra @("CLIENT_LOG=$ClientLog")
    }

    $text = $client.Output -join [Environment]::NewLine
    $keys = @('INIT_ENTRY_US','RST_LOW_US','STATE_END_US','I2C_END_US','RTC_END_US','FRAM_END_US','SD_END_US','BUTTONS_END_US','DISPLAY_BEGIN_START_US','DISPLAY_BEGIN_END_US','DISPLAY_REFRESH_END_US','TCA_INIT_END_US','TCA_CONFIG_END_US','EN_IO_HIGH_US','INIT_END_US','SETUP_ENTRY_US','PROBE_PRINT_US')
    $values = @{}
    foreach ($key in $keys)
    {
        $value = Get-A13LogInt -Text $text -Key ('A13_TFT_PRE2_CLIENT_' + $key)
        if ($null -eq $value)
        {
            Finish-TFTPre2 -Status 'REVIEW' -Reason ("CANONICAL_VALUE_MISSING:" + $key) -HarnessFailure 'YES' -ExitCode 32
        }
        $values[$key] = [int64]$value
    }

    $displayBeginOk = Get-A13LogValue -Text $text -Key 'A13_TFT_PRE2_CLIENT_DISPLAY_BEGIN_OK'
    $displayReady = Get-A13LogValue -Text $text -Key 'A13_TFT_PRE2_CLIENT_DISPLAY_READY'
    $ioReady = Get-A13LogValue -Text $text -Key 'A13_TFT_PRE2_CLIENT_IO_READY'
    $clientPass = Get-A13LogValue -Text $text -Key 'A13_TFT_PRE2_CLIENT_PASS'
    if ($displayBeginOk -ne 'YES' -or $displayReady -ne 'YES' -or $ioReady -ne 'YES' -or $clientPass -ne 'YES')
    {
        Finish-TFTPre2 -Status 'REVIEW' -Reason 'CANONICAL_RUNTIME_CONTRACT_FAILED' -HarnessFailure 'YES' -ExitCode 33
    }

    $bootToInit = $values['INIT_ENTRY_US']
    $initToRst = $values['RST_LOW_US'] - $values['INIT_ENTRY_US']
    $rstToDisplayStart = $values['DISPLAY_BEGIN_START_US'] - $values['RST_LOW_US']
    $displayBeginDuration = $values['DISPLAY_BEGIN_END_US'] - $values['DISPLAY_BEGIN_START_US']
    $firstRefreshDuration = $values['DISPLAY_REFRESH_END_US'] - $values['DISPLAY_BEGIN_END_US']
    $bootToFirstRefresh = $values['DISPLAY_REFRESH_END_US']
    $firstRefreshToSetup = $values['SETUP_ENTRY_US'] - $values['DISPLAY_REFRESH_END_US']

    $porcelainFinal = @(& git status --porcelain=v1 --untracked-files=normal)
    & git diff --check
    $diffCheck = ($LASTEXITCODE -eq 0)
    if ($porcelainFinal.Count -ne 0 -or -not $diffCheck)
    {
        Finish-TFTPre2 -Status 'REVIEW' -Reason 'FINAL_REPO_AUDIT_FAILED' -HarnessFailure 'YES' -ExitCode 40 -Extra @("STATUS=$($porcelainFinal -join ';')","DIFF_CHECK=$diffCheck")
    }

    Finish-TFTPre2 -Status 'PASS' -Reason 'INTERNAL_TIMELINE_CAPTURED' -ExitCode 0 -Extra @(
        "HEAD=$head",
        "SERIAL_PORT=$resolvedPort",
        "CORE_SHA256=$ExpectedCoreSha256",
        "USES_SOURCE_CORE=$usesSource",
        "USES_STUB_CORE=$usesStub",
        "CORE_A_LINKED=$archiveLinked",
        "SOURCE_TU_COUNT=$($proof.SourceCount)",
        "PERIPHERALS_INIT_COUNT=$($proof.PeripheralsCount)",
        "FULL_PROFILE=$fullProfile",
        "INIT_ENTRY_US=$($values['INIT_ENTRY_US'])",
        "RST_LOW_US=$($values['RST_LOW_US'])",
        "I2C_END_US=$($values['I2C_END_US'])",
        "RTC_END_US=$($values['RTC_END_US'])",
        "FRAM_END_US=$($values['FRAM_END_US'])",
        "SD_END_US=$($values['SD_END_US'])",
        "BUTTONS_END_US=$($values['BUTTONS_END_US'])",
        "DISPLAY_BEGIN_START_US=$($values['DISPLAY_BEGIN_START_US'])",
        "DISPLAY_BEGIN_END_US=$($values['DISPLAY_BEGIN_END_US'])",
        "DISPLAY_REFRESH_END_US=$($values['DISPLAY_REFRESH_END_US'])",
        "TCA_INIT_END_US=$($values['TCA_INIT_END_US'])",
        "INIT_END_US=$($values['INIT_END_US'])",
        "SETUP_ENTRY_US=$($values['SETUP_ENTRY_US'])",
        "BOOT_TO_INIT_ENTRY_US=$bootToInit",
        "INIT_TO_RST_US=$initToRst",
        "RST_TO_DISPLAY_START_US=$rstToDisplayStart",
        "DISPLAY_BEGIN_DURATION_US=$displayBeginDuration",
        "FIRST_REFRESH_DURATION_US=$firstRefreshDuration",
        "BOOT_TO_FIRST_REFRESH_US=$bootToFirstRefresh",
        "FIRST_REFRESH_TO_SETUP_US=$firstRefreshToSetup",
        "DISPLAY_READY=$displayReady",
        "IO_READY=$ioReady",
        "SOURCE_RESTORED=$sourceRestored",
        "CORE_SHA_PRESERVED=$corePreserved",
        "WORKTREE_FINAL=CLEAN",
        "DIFF_CHECK_FINAL=$diffCheck",
        "COMPILE_LOG=$CompileLog",
        "UPLOAD_LOG=$UploadLog",
        "CLIENT_LOG=$ClientLog"
    )
}
catch
{
    Finish-TFTPre2 -Status 'REVIEW' -Reason 'UNEXPECTED_GATE_EXCEPTION' -HarnessFailure 'YES' -ProductFailure 'UNDETERMINED' -ExitCode 90 -Extra @("EXCEPTION=$($_.Exception.Message)")
}
finally
{
    Pop-Location
}
