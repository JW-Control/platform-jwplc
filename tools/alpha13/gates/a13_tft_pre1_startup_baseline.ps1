param(
    [string]$SerialPort = '',
    [string]$ArduinoCli = 'C:\Program Files\Arduino PLC IDE Tools\arduino-cli.exe'
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$ExpectedBranch = 'v2.1.0-alpha.13/feature/cleanup-robustness'
$ProductHead = '6a585693c20c4f44bd789e00a7ae65b9828bcdec'
$ExpectedCoreSha256 = '6f328eeb796091070c8d852a2c0e90f71047d8d2b5786fe3cd5079b2fb6ff983'

$ScriptDir = $PSScriptRoot
$RepoRoot = [IO.Path]::GetFullPath((Join-Path $ScriptDir '..\..\..'))
$CommonPath = Join-Path $ScriptDir 'common.ps1'
$ProbeDir = Join-Path $RepoRoot 'tools\alpha13\firmware\a13_tft_pre1_startup_baseline_probe'
$ClientPath = Join-Path $ScriptDir 'a13_tft_pre1_startup_baseline_client.py'
$CoreRelative = 'JWPLC/2.1.0/precompiled/core/JWPLCBASIC/core.a'
$CorePath = Join-Path $RepoRoot $CoreRelative
$Fqbn = 'jwplc_local:esp32:jwplcbasic'

$RunId = Get-Date -Format 'yyyyMMdd_HHmmss'
$RunRoot = Join-Path $RepoRoot ("tools\alpha13\results\tft_pre1_{0}" -f $RunId)
$BuildRoot = Join-Path $env:TEMP ("jwplc_a13_tft_pre1_{0}" -f $RunId)
$CompileLog = Join-Path $RunRoot 'compile.log'
$UploadLog = Join-Path $RunRoot 'upload.log'
$ClientLog = Join-Path $RunRoot 'client.log'
$SummaryLog = Join-Path $RunRoot 'SUMMARY.log'

New-Item -ItemType Directory -Path $RunRoot -Force | Out-Null
New-Item -ItemType Directory -Path $BuildRoot -Force | Out-Null

. $CommonPath

function Finish-TFTPre1
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

    $out = @(
        'GATE=A13-TFT-PRE1',
        "STATUS=$Status",
        "REASON=$Reason",
        "PRODUCT_FAILURE=$ProductFailure",
        "HARNESS_FAILURE=$HarnessFailure",
        "HARDWARE_FAILURE=$HardwareFailure",
        "ENVIRONMENT_FAILURE=$EnvironmentFailure",
        "RUN_ROOT=$RunRoot",
        "SUMMARY_LOG=$SummaryLog"
    ) + $Extra

    $out | Set-Content -LiteralPath $SummaryLog -Encoding utf8
    $out | ForEach-Object { Write-Host $_ }
    exit $ExitCode
}

function Get-CompileProof
{
    param([Parameter(Mandatory = $true)][string]$BuildPath)

    $dbPath = Join-Path $BuildPath 'compile_commands.json'
    if (-not (Test-Path -LiteralPath $dbPath))
    {
        throw "compile_commands.json missing: $dbPath"
    }

    $parsed = (Get-Content -LiteralPath $dbPath -Raw) | ConvertFrom-Json
    $entries = New-Object System.Collections.Generic.List[object]

    foreach ($entry in @($parsed))
    {
        if ($entry -is [System.Array])
        {
            foreach ($nested in $entry)
            {
                if ($null -ne $nested) { [void]$entries.Add($nested) }
            }
        }
        elseif ($null -ne $entry)
        {
            [void]$entries.Add($entry)
        }
    }

    $sourceCount = 0
    $stubCount = 0
    $stubNamedCount = 0

    foreach ($entry in $entries)
    {
        $file = ([string]$entry.file).Replace('\','/')
        $directory = ([string]$entry.directory).Replace('\','/')
        if ($file -notmatch '^(?:[A-Za-z]:/|/)' -and $directory.Length -gt 0)
        {
            $candidate = $directory.TrimEnd('/') + '/' + $file.TrimStart('/')
        }
        else
        {
            $candidate = $file
        }

        if ($candidate -match '/cores/jwcontrol_precompiled_stub/')
        {
            ++$stubCount
            if ($candidate.EndsWith('/precompiled_core_stub.c', [StringComparison]::OrdinalIgnoreCase))
            {
                ++$stubNamedCount
            }
        }
        elseif ($candidate -match '/cores/jwcontrol/')
        {
            ++$sourceCount
        }
    }

    return [PSCustomObject]@{
        Entries = $entries.Count
        SourceCount = $sourceCount
        StubCount = $stubCount
        StubNamedCount = $stubNamedCount
    }
}

Write-Host '============================================================'
Write-Host ' A13-TFT-PRE1 - STARTUP BASELINE / NORMAL PACKAGE'
Write-Host '============================================================'

Push-Location $RepoRoot
try
{
    foreach ($required in @($CommonPath, $ProbeDir, $ClientPath, $CorePath))
    {
        if (-not (Test-Path -LiteralPath $required))
        {
            Finish-TFTPre1 -Status 'REVIEW' -Reason "REQUIRED_PATH_MISSING:$required" -HarnessFailure 'YES' -ExitCode 3
        }
    }

    $branch = (git branch --show-current).Trim()
    $head = (git rev-parse HEAD).Trim()

    if ($branch -ne $ExpectedBranch)
    {
        Finish-TFTPre1 -Status 'REVIEW' -Reason 'UNEXPECTED_BRANCH' -HarnessFailure 'YES' -ExitCode 4
    }

    & git merge-base --is-ancestor $ProductHead $head
    if ($LASTEXITCODE -ne 0)
    {
        Finish-TFTPre1 -Status 'REVIEW' -Reason 'G2_PRODUCT_HEAD_NOT_ANCESTOR' -HarnessFailure 'YES' -ExitCode 5
    }

    $committedProduct = @(
        & git diff --name-only "$ProductHead..$head" -- 'JWPLC/2.1.0/cores/jwcontrol' $CoreRelative |
            Where-Object { $_ -and $_.Trim().Length -gt 0 }
    )
    if ($committedProduct.Count -ne 0)
    {
        Finish-TFTPre1 -Status 'REVIEW' -Reason 'UNEXPECTED_PRODUCT_CHANGE_AFTER_G2' -HarnessFailure 'YES' -ExitCode 6 -Extra @(
            "COMMITTED_PRODUCT=$($committedProduct -join ';')"
        )
    }

    $dirty = @(Get-A13TrackedDirty)
    $staged = @(Get-A13Staged)
    if ($dirty.Count -ne 0 -or $staged.Count -ne 0)
    {
        Finish-TFTPre1 -Status 'REVIEW' -Reason 'WORKTREE_NOT_CLEAN' -HarnessFailure 'YES' -ExitCode 7 -Extra @(
            "DIRTY=$($dirty -join ';')",
            "STAGED=$($staged -join ';')"
        )
    }

    $coreSha = Get-A13Sha256 -Path $CorePath
    if ($coreSha -ne $ExpectedCoreSha256)
    {
        Finish-TFTPre1 -Status 'REVIEW' -Reason 'CORE_SHA_MISMATCH' -HarnessFailure 'YES' -ExitCode 8 -Extra @(
            "CORE_SHA256=$coreSha"
        )
    }

    $portResolution = Resolve-A13SerialPort -RequestedPort $SerialPort
    Write-Host "PORTS_VISIBLE=$($portResolution.Ports -join ',')"
    Write-Host "PORT_RESOLUTION=$($portResolution.Reason)"
    if ($portResolution.Status -ne 'PASS')
    {
        Finish-TFTPre1 -Status 'REVIEW' -Reason $portResolution.Reason -EnvironmentFailure 'YES' -ExitCode 9
    }
    $resolvedPort = $portResolution.Port
    Write-Host "SERIAL_PORT=$resolvedPort"

    if (-not (Test-Path -LiteralPath $ArduinoCli))
    {
        $cli = Get-Command arduino-cli -ErrorAction SilentlyContinue
        if ($null -eq $cli)
        {
            Finish-TFTPre1 -Status 'REVIEW' -Reason 'ARDUINO_CLI_NOT_FOUND' -EnvironmentFailure 'YES' -ExitCode 10
        }
        $ArduinoCli = $cli.Source
    }

    $cliVersion = @(& $ArduinoCli version 2>&1 | ForEach-Object { $_.ToString() }) -join ' '
    Write-Host "ARDUINO_CLI=$cliVersion"
    if ($cliVersion -notmatch 'Version:\s*1\.0\.2')
    {
        Finish-TFTPre1 -Status 'REVIEW' -Reason 'ARDUINO_CLI_VERSION_MISMATCH' -EnvironmentFailure 'YES' -ExitCode 11
    }

    $pythonCommand = Get-Command python.exe -ErrorAction SilentlyContinue
    if ($null -eq $pythonCommand) { $pythonCommand = Get-Command python -ErrorAction SilentlyContinue }
    if ($null -eq $pythonCommand)
    {
        Finish-TFTPre1 -Status 'REVIEW' -Reason 'PYTHON_NOT_FOUND' -EnvironmentFailure 'YES' -ExitCode 12
    }
    $pythonExe = $pythonCommand.Source

    $pySerial = Invoke-A13NativeCaptured -FilePath $pythonExe -Arguments @('-c','import serial; print(serial.__version__)')
    if ($pySerial.ExitCode -ne 0)
    {
        Finish-TFTPre1 -Status 'REVIEW' -Reason 'PYSERIAL_NOT_AVAILABLE' -EnvironmentFailure 'YES' -ExitCode 13
    }

    $clientSyntax = Invoke-A13NativeCaptured -FilePath $pythonExe -Arguments @('-c','import ast,pathlib,sys; ast.parse(pathlib.Path(sys.argv[1]).read_text(encoding="utf-8")); print("PY_SYNTAX=PASS")',$ClientPath)
    if ($clientSyntax.ExitCode -ne 0)
    {
        Finish-TFTPre1 -Status 'REVIEW' -Reason 'CLIENT_SYNTAX_ERROR' -HarnessFailure 'YES' -ExitCode 14
    }

    $compile = Invoke-A13NativeCaptured -FilePath $ArduinoCli -Arguments @(
        'compile','--fqbn',$Fqbn,'-j','0','-v','--clean','--build-path',$BuildRoot,$ProbeDir
    )
    $compile.Output | Set-Content -LiteralPath $CompileLog -Encoding utf8
    Write-Host "COMPILE_EXIT=$($compile.ExitCode)"
    if ($compile.ExitCode -ne 0)
    {
        Finish-TFTPre1 -Status 'REVIEW' -Reason 'COMPILE_FAILED' -HarnessFailure 'YES' -ExitCode 20 -Extra @("COMPILE_LOG=$CompileLog")
    }

    $compileText = $compile.Output -join [Environment]::NewLine
    $proof = Get-CompileProof -BuildPath $BuildRoot
    $usesStub = $compileText -match "Using core 'jwcontrol_precompiled_stub'"
    $usesSource = $compileText -match "Using core 'jwcontrol'"
    $archiveLinked = $compileText -match '[\\/]precompiled[\\/]core[\\/]JWPLCBASIC[\\/]core\.a'

    Write-Host "USES_STUB_CORE=$usesStub"
    Write-Host "USES_SOURCE_CORE=$usesSource"
    Write-Host "CORE_A_LINKED=$archiveLinked"
    Write-Host "SOURCE_TU_COUNT=$($proof.SourceCount)"
    Write-Host "STUB_TU_COUNT=$($proof.StubCount)"

    if (-not $usesStub -or $usesSource -or -not $archiveLinked -or $proof.SourceCount -ne 0 -or $proof.StubCount -ne 1 -or $proof.StubNamedCount -ne 1)
    {
        Finish-TFTPre1 -Status 'REVIEW' -Reason 'NORMAL_PACKAGE_BUILD_CONTRACT_FAILED' -HarnessFailure 'YES' -ExitCode 21
    }

    $upload = Invoke-A13NativeCaptured -FilePath $ArduinoCli -Arguments @(
        'upload','--fqbn',$Fqbn,'--port',$resolvedPort,'--input-dir',$BuildRoot,$ProbeDir
    )
    $upload.Output | Set-Content -LiteralPath $UploadLog -Encoding utf8
    Write-Host "UPLOAD_EXIT=$($upload.ExitCode)"
    if ($upload.ExitCode -ne 0)
    {
        Finish-TFTPre1 -Status 'REVIEW' -Reason 'UPLOAD_FAILED' -HardwareFailure 'YES' -EnvironmentFailure 'YES' -ExitCode 30 -Extra @("UPLOAD_LOG=$UploadLog")
    }

    Start-Sleep -Milliseconds 500
    $client = Invoke-A13NativeCaptured -FilePath $pythonExe -Arguments @(
        $ClientPath,'--serial',$resolvedPort,'--baud','115200','--timeout-s','10'
    )
    $client.Output | Set-Content -LiteralPath $ClientLog -Encoding utf8
    Write-Host "CLIENT_EXIT=$($client.ExitCode)"
    $client.Output | ForEach-Object { Write-Host $_ }
    if ($client.ExitCode -ne 0)
    {
        Finish-TFTPre1 -Status 'REVIEW' -Reason 'CLIENT_FAILED' -HarnessFailure 'YES' -EnvironmentFailure 'YES' -ExitCode 31 -Extra @("CLIENT_LOG=$ClientLog")
    }

    $text = $client.Output -join [Environment]::NewLine
    $setupMs = Get-A13LogInt -Text $text -Key 'A13_TFT_PRE1_CLIENT_SETUP_ENTRY_MS'
    $uptimeMs = Get-A13LogInt -Text $text -Key 'A13_TFT_PRE1_CLIENT_UPTIME_MS'
    $displayReady = Get-A13LogValue -Text $text -Key 'A13_TFT_PRE1_CLIENT_DISPLAY_READY'
    $ioReady = Get-A13LogValue -Text $text -Key 'A13_TFT_PRE1_CLIENT_IO_READY'
    $rstEnable = Get-A13LogValue -Text $text -Key 'A13_TFT_PRE1_CLIENT_TFT_RST_OUTPUT_ENABLE'
    $rstLatch = Get-A13LogValue -Text $text -Key 'A13_TFT_PRE1_CLIENT_TFT_RST_OUTPUT_LATCH'
    $csEnable = Get-A13LogValue -Text $text -Key 'A13_TFT_PRE1_CLIENT_TFT_CS_OUTPUT_ENABLE'
    $csLatch = Get-A13LogValue -Text $text -Key 'A13_TFT_PRE1_CLIENT_TFT_CS_OUTPUT_LATCH'
    $clientPass = Get-A13LogValue -Text $text -Key 'A13_TFT_PRE1_CLIENT_PASS'

    if ($null -eq $setupMs -or $null -eq $uptimeMs -or $clientPass -ne 'YES')
    {
        Finish-TFTPre1 -Status 'REVIEW' -Reason 'CANONICAL_CLIENT_CONTRACT_FAILED' -HarnessFailure 'YES' -ExitCode 32
    }

    if ($displayReady -ne 'YES')
    {
        Finish-TFTPre1 -Status 'REVIEW' -Reason 'DISPLAY_NOT_READY_AT_SETUP' -ProductFailure 'UNDETERMINED' -ExitCode 33 -Extra @(
            "SETUP_ENTRY_MS=$setupMs",
            "IO_READY=$ioReady"
        )
    }

    $dirtyFinal = @(Get-A13TrackedDirty)
    $stagedFinal = @(Get-A13Staged)
    & git diff --check
    $diffCheck = ($LASTEXITCODE -eq 0)

    if ($dirtyFinal.Count -ne 0 -or $stagedFinal.Count -ne 0 -or -not $diffCheck)
    {
        Finish-TFTPre1 -Status 'REVIEW' -Reason 'FINAL_REPO_AUDIT_FAILED' -HarnessFailure 'YES' -ExitCode 40
    }

    Finish-TFTPre1 -Status 'PASS' -Reason 'BASELINE_TIMING_CAPTURED' -ExitCode 0 -Extra @(
        "HEAD=$head",
        "SERIAL_PORT=$resolvedPort",
        "CORE_SHA256=$coreSha",
        "USES_STUB_CORE=$usesStub",
        "USES_SOURCE_CORE=$usesSource",
        "CORE_A_LINKED=$archiveLinked",
        "SETUP_ENTRY_MS=$setupMs",
        "UPTIME_MS=$uptimeMs",
        "DISPLAY_READY=$displayReady",
        "IO_READY=$ioReady",
        "TFT_RST_OUTPUT_ENABLE=$rstEnable",
        "TFT_RST_OUTPUT_LATCH=$rstLatch",
        "TFT_CS_OUTPUT_ENABLE=$csEnable",
        "TFT_CS_OUTPUT_LATCH=$csLatch",
        "TRACKED_DIRTY_FINAL=$($dirtyFinal.Count)",
        "STAGED_FINAL=$($stagedFinal.Count)",
        "DIFF_CHECK_FINAL=$diffCheck",
        "COMPILE_LOG=$CompileLog",
        "UPLOAD_LOG=$UploadLog",
        "CLIENT_LOG=$ClientLog"
    )
}
catch
{
    Finish-TFTPre1 -Status 'REVIEW' -Reason 'UNEXPECTED_GATE_EXCEPTION' -HarnessFailure 'YES' -ProductFailure 'UNDETERMINED' -ExitCode 90 -Extra @(
        "EXCEPTION=$($_.Exception.Message)"
    )
}
finally
{
    Pop-Location
}
