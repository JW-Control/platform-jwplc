param(
    [string]$SerialPort = '',
    [string]$ArduinoCli = 'C:\Program Files\Arduino PLC IDE Tools\arduino-cli.exe'
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$ExpectedBranch = 'v2.1.0-alpha.13/feature/cleanup-robustness'
$QualifiedToolingHead = '0cfec7d38719bea643baab97a5c4d7dac9833c48'
$ExpectedCoreSha256 = '6f328eeb796091070c8d852a2c0e90f71047d8d2b5786fe3cd5079b2fb6ff983'

$ExpectedSourceBlobs = @{
    'JWPLC/2.1.0/cores/jwcontrol/peripherals_init.cpp' = '23efb3935a34e6b5649875b57c804e60538827cd'
    'JWPLC/2.1.0/cores/jwcontrol/jwplc_peripherals.cpp' = 'c3d53566d274e95b7dda111327140b5393f7db35'
    'JWPLC/2.1.0/cores/jwcontrol/jwplc_peripherals.h' = '288667f1caa08142e2a155b8c85f24b2aa5beb44'
}

$ScriptDir = $PSScriptRoot
$RepoRoot = [IO.Path]::GetFullPath((Join-Path $ScriptDir '..\..\..'))
$CommonPath = Join-Path $ScriptDir 'common.ps1'
$ProbeDir = Join-Path $RepoRoot 'tools\alpha13\firmware\a13_g2_p4_archive_physical_probe'
$ClientPath = Join-Path $ScriptDir 'a13_g2_p4_archive_physical_client.py'
$CoreRelative = 'JWPLC/2.1.0/precompiled/core/JWPLCBASIC/core.a'
$CorePath = Join-Path $RepoRoot $CoreRelative
$Fqbn = 'jwplc_local:esp32:jwplcbasic'

$RunId = Get-Date -Format 'yyyyMMdd_HHmmss'
$RunRoot = Join-Path $RepoRoot ("tools\alpha13\results\g2_p4_{0}" -f $RunId)
$BuildRoot = Join-Path $env:TEMP ("jwplc_a13_g2_p4_{0}" -f $RunId)
$CompileLog = Join-Path $RunRoot 'compile.log'
$UploadLog = Join-Path $RunRoot 'upload.log'
$ClientLog = Join-Path $RunRoot 'client.log'
$SummaryLog = Join-Path $RunRoot 'SUMMARY.log'

New-Item -ItemType Directory -Path $RunRoot -Force | Out-Null
New-Item -ItemType Directory -Path $BuildRoot -Force | Out-Null

. $CommonPath

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

function Get-CompileProof
{
    param([Parameter(Mandatory = $true)][string]$BuildPath)

    $dbPath = Join-Path $BuildPath 'compile_commands.json'
    if (-not (Test-Path -LiteralPath $dbPath))
    {
        throw "compile_commands.json missing: $dbPath"
    }

    $raw = Get-Content -LiteralPath $dbPath -Raw
    $parsed = $raw | ConvertFrom-Json
    $entries = New-Object System.Collections.Generic.List[object]

    foreach ($entry in @($parsed))
    {
        if (
            $entry -is [System.Array] -or
            (
                $entry -is [System.Collections.IEnumerable] -and
                -not ($entry -is [string]) -and
                -not (@($entry.PSObject.Properties.Name) -contains 'file')
            )
        )
        {
            foreach ($nested in $entry)
            {
                if ($null -ne $nested)
                {
                    [void]$entries.Add($nested)
                }
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

    $fullProfile = (
        $raw.Contains('-DJWPLC_HAS_RTC=1') -and
        $raw.Contains('-DJWPLC_HAS_FRAM=1') -and
        $raw.Contains('-DJWPLC_HAS_SD=1') -and
        $raw.Contains('-DJWPLC_HAS_ETHERNET=1')
    )

    return [PSCustomObject]@{
        Entries = $entries.Count
        SourceCount = $sourceCount
        StubCount = $stubCount
        StubNamedCount = $stubNamedCount
        FullProfile = $fullProfile
    }
}

function Finish-G2P4
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
        'GATE=A13-G2-P4',
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

Write-Host '============================================================'
Write-Host ' A13-G2-P4 - NORMAL PRECOMPILED PHYSICAL GATE'
Write-Host '============================================================'

Push-Location $RepoRoot
try
{
    foreach ($required in @($CommonPath, $ProbeDir, $ClientPath, $CorePath))
    {
        if (-not (Test-Path -LiteralPath $required))
        {
            Finish-G2P4 -Status 'REVIEW' -Reason "REQUIRED_PATH_MISSING:$required" -HarnessFailure 'YES' -ExitCode 3
        }
    }

    $branch = (git branch --show-current).Trim()
    $head = (git rev-parse HEAD).Trim()

    if ($branch -ne $ExpectedBranch)
    {
        Finish-G2P4 -Status 'REVIEW' -Reason 'UNEXPECTED_BRANCH' -HarnessFailure 'YES' -ExitCode 4 -Extra @(
            "BRANCH=$branch",
            "HEAD=$head"
        )
    }

    & git merge-base --is-ancestor $QualifiedToolingHead $head
    if ($LASTEXITCODE -ne 0)
    {
        Finish-G2P4 -Status 'REVIEW' -Reason 'QUALIFIED_HEAD_NOT_ANCESTOR' -HarnessFailure 'YES' -ExitCode 5
    }

    $committedProduct = @(
        & git diff --name-only "$QualifiedToolingHead..$head" -- 'JWPLC/2.1.0/cores/jwcontrol' $CoreRelative |
            Where-Object { $_ -and $_.Trim().Length -gt 0 }
    )

    if ($committedProduct.Count -ne 0)
    {
        Finish-G2P4 -Status 'REVIEW' -Reason 'UNEXPECTED_COMMITTED_PRODUCT_CHANGE' -HarnessFailure 'YES' -ExitCode 6 -Extra @(
            "COMMITTED_PRODUCT=$($committedProduct -join ';')"
        )
    }

    $dirty = @(Get-A13TrackedDirty | Sort-Object)
    $staged = @(Get-A13Staged)
    $expectedDirty = @($ExpectedSourceBlobs.Keys)
    $expectedDirty += $CoreRelative
    $expectedDirty = @($expectedDirty | Sort-Object)
    $dirtyDiff = @(Compare-Object -ReferenceObject $expectedDirty -DifferenceObject $dirty)

    if ($dirtyDiff.Count -ne 0 -or $staged.Count -ne 0)
    {
        Finish-G2P4 -Status 'REVIEW' -Reason 'CANDIDATE_SCOPE_INVALID' -HarnessFailure 'YES' -ExitCode 7 -Extra @(
            "DIRTY=$($dirty -join ';')",
            "STAGED=$($staged -join ';')"
        )
    }

    & git diff --check
    if ($LASTEXITCODE -ne 0)
    {
        Finish-G2P4 -Status 'REVIEW' -Reason 'GIT_DIFF_CHECK_FAILED' -HarnessFailure 'YES' -ExitCode 8
    }

    foreach ($relative in $ExpectedSourceBlobs.Keys)
    {
        $actual = Get-GitBlobSha -Path $relative
        if ($actual -ne $ExpectedSourceBlobs[$relative])
        {
            Finish-G2P4 -Status 'REVIEW' -Reason 'SOURCE_BLOB_MISMATCH' -HarnessFailure 'YES' -ExitCode 9 -Extra @(
                "FILE=$relative",
                "ACTUAL=$actual",
                "EXPECTED=$($ExpectedSourceBlobs[$relative])"
            )
        }
    }

    $coreSha = Get-A13Sha256 -Path $CorePath
    if ($coreSha -ne $ExpectedCoreSha256)
    {
        Finish-G2P4 -Status 'REVIEW' -Reason 'CORE_SHA_MISMATCH' -HarnessFailure 'YES' -ExitCode 10 -Extra @(
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
        Finish-G2P4 -Status 'REVIEW' -Reason $portResolution.Reason -EnvironmentFailure 'YES' -ExitCode 11
    }

    $resolvedPort = $portResolution.Port
    Write-Host "SERIAL_PORT=$resolvedPort"

    if (-not (Test-Path -LiteralPath $ArduinoCli))
    {
        $cliCommand = Get-Command arduino-cli -ErrorAction SilentlyContinue
        if ($null -eq $cliCommand)
        {
            Finish-G2P4 -Status 'REVIEW' -Reason 'ARDUINO_CLI_NOT_FOUND' -EnvironmentFailure 'YES' -ExitCode 12
        }
        $ArduinoCli = $cliCommand.Source
    }

    $cliVersion = @(& $ArduinoCli version 2>&1 | ForEach-Object { $_.ToString() }) -join ' '
    Write-Host "ARDUINO_CLI=$cliVersion"

    if ($cliVersion -notmatch 'Version:\s*1\.0\.2')
    {
        Finish-G2P4 -Status 'REVIEW' -Reason 'ARDUINO_CLI_VERSION_MISMATCH' -EnvironmentFailure 'YES' -ExitCode 13
    }

    $pythonCommand = Get-Command python.exe -ErrorAction SilentlyContinue
    if ($null -eq $pythonCommand)
    {
        $pythonCommand = Get-Command python -ErrorAction SilentlyContinue
    }
    if ($null -eq $pythonCommand)
    {
        Finish-G2P4 -Status 'REVIEW' -Reason 'PYTHON_NOT_FOUND' -EnvironmentFailure 'YES' -ExitCode 14
    }
    $pythonExe = $pythonCommand.Source

    $pySerial = Invoke-A13NativeCaptured -FilePath $pythonExe -Arguments @('-c', 'import serial; print(serial.__version__)')
    if ($pySerial.ExitCode -ne 0)
    {
        Finish-G2P4 -Status 'REVIEW' -Reason 'PYSERIAL_NOT_AVAILABLE' -EnvironmentFailure 'YES' -ExitCode 15
    }

    $clientSyntax = Invoke-A13NativeCaptured -FilePath $pythonExe -Arguments @('-m', 'py_compile', $ClientPath)
    if ($clientSyntax.ExitCode -ne 0)
    {
        Finish-G2P4 -Status 'REVIEW' -Reason 'CLIENT_SYNTAX_ERROR' -HarnessFailure 'YES' -ExitCode 16
    }

    $compile = Invoke-A13NativeCaptured -FilePath $ArduinoCli -Arguments @(
        'compile',
        '--fqbn', $Fqbn,
        '-j', '0',
        '-v',
        '--clean',
        '--build-path', $BuildRoot,
        $ProbeDir
    )

    $compile.Output | Set-Content -LiteralPath $CompileLog -Encoding utf8
    Write-Host "COMPILE_EXIT=$($compile.ExitCode)"

    if ($compile.ExitCode -ne 0)
    {
        Finish-G2P4 -Status 'REVIEW' -Reason 'NORMAL_PRECOMPILED_COMPILE_FAILED' -HarnessFailure 'YES' -ProductFailure 'UNDETERMINED' -ExitCode 20 -Extra @(
            "COMPILE_LOG=$CompileLog"
        )
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
    Write-Host "PRECOMPILED_STUB_COUNT=$($proof.StubNamedCount)"
    Write-Host "FULL_PROFILE=$($proof.FullProfile)"

    if (
        -not $usesStub -or
        $usesSource -or
        -not $archiveLinked -or
        $proof.SourceCount -ne 0 -or
        $proof.StubCount -ne 1 -or
        $proof.StubNamedCount -ne 1 -or
        -not $proof.FullProfile
    )
    {
        Finish-G2P4 -Status 'REVIEW' -Reason 'NORMAL_PRECOMPILED_BUILD_CONTRACT_FAILED' -HarnessFailure 'YES' -ExitCode 21
    }

    $upload = Invoke-A13NativeCaptured -FilePath $ArduinoCli -Arguments @(
        'upload',
        '--fqbn', $Fqbn,
        '--port', $resolvedPort,
        '--input-dir', $BuildRoot,
        $ProbeDir
    )

    $upload.Output | Set-Content -LiteralPath $UploadLog -Encoding utf8
    Write-Host "UPLOAD_EXIT=$($upload.ExitCode)"

    if ($upload.ExitCode -ne 0)
    {
        Finish-G2P4 -Status 'REVIEW' -Reason 'UPLOAD_FAILED' -HardwareFailure 'YES' -EnvironmentFailure 'YES' -ExitCode 30 -Extra @(
            "UPLOAD_LOG=$UploadLog"
        )
    }

    Start-Sleep -Milliseconds 500

    $clientRun = Invoke-A13NativeCaptured -FilePath $pythonExe -Arguments @(
        $ClientPath,
        '--serial', $resolvedPort,
        '--baud', '115200',
        '--timeout-s', '10'
    )

    $clientRun.Output | Set-Content -LiteralPath $ClientLog -Encoding utf8
    Write-Host "CLIENT_EXIT=$($clientRun.ExitCode)"
    $clientRun.Output | ForEach-Object { Write-Host $_ }

    if ($clientRun.ExitCode -eq 8)
    {
        Finish-G2P4 -Status 'REVIEW' -Reason 'NORMAL_RUNTIME_NOT_READY' -ProductFailure 'UNDETERMINED' -HardwareFailure 'UNDETERMINED' -ExitCode 31 -Extra @(
            "CLIENT_LOG=$ClientLog"
        )
    }

    if ($clientRun.ExitCode -ne 0)
    {
        Finish-G2P4 -Status 'REVIEW' -Reason 'CLIENT_FAILED' -HarnessFailure 'YES' -EnvironmentFailure 'YES' -ExitCode 32 -Extra @(
            "CLIENT_LOG=$ClientLog"
        )
    }

    $clientText = $clientRun.Output -join [Environment]::NewLine
    $ioReady = Get-A13LogValue -Text $clientText -Key 'A13_G2_P4_CLIENT_IO_READY'
    $enIoEnable = Get-A13LogValue -Text $clientText -Key 'A13_G2_P4_CLIENT_EN_IO_OUTPUT_ENABLE'
    $enIoLatch = Get-A13LogValue -Text $clientText -Key 'A13_G2_P4_CLIENT_EN_IO_OUTPUT_LATCH'
    $lastScanMs = Get-A13LogInt -Text $clientText -Key 'A13_G2_P4_CLIENT_LAST_SCAN_MS'
    $clientPass = Get-A13LogValue -Text $clientText -Key 'A13_G2_P4_CLIENT_PASS'

    $runtimeContract = (
        $ioReady -eq 'YES' -and
        $enIoEnable -eq 'YES' -and
        $enIoLatch -eq 'HIGH' -and
        $null -ne $lastScanMs -and
        $lastScanMs -gt 0 -and
        $clientPass -eq 'YES'
    )

    if (-not $runtimeContract)
    {
        Finish-G2P4 -Status 'REVIEW' -Reason 'CANONICAL_CLIENT_CONTRACT_FAILED' -HarnessFailure 'YES' -ExitCode 33
    }

    $dirtyFinal = @(Get-A13TrackedDirty | Sort-Object)
    $stagedFinal = @(Get-A13Staged)
    $dirtyFinalDiff = @(Compare-Object -ReferenceObject $expectedDirty -DifferenceObject $dirtyFinal)

    $sourceBlobsFinal = $true
    foreach ($relative in $ExpectedSourceBlobs.Keys)
    {
        if ((Get-GitBlobSha -Path $relative) -ne $ExpectedSourceBlobs[$relative])
        {
            $sourceBlobsFinal = $false
        }
    }

    $coreFinal = Get-A13Sha256 -Path $CorePath
    & git diff --check
    $diffCheckFinal = ($LASTEXITCODE -eq 0)

    if (
        -not $sourceBlobsFinal -or
        $coreFinal -ne $ExpectedCoreSha256 -or
        $dirtyFinalDiff.Count -ne 0 -or
        $stagedFinal.Count -ne 0 -or
        -not $diffCheckFinal
    )
    {
        Finish-G2P4 -Status 'REVIEW' -Reason 'FINAL_REPO_AUDIT_FAILED' -HarnessFailure 'YES' -ExitCode 40 -Extra @(
            "SOURCE_BLOBS_FINAL=$sourceBlobsFinal",
            "CORE_FINAL_SHA256=$coreFinal",
            "DIRTY=$($dirtyFinal -join ';')",
            "STAGED=$($stagedFinal -join ';')",
            "DIFF_CHECK=$diffCheckFinal"
        )
    }

    Finish-G2P4 -Status 'PASS' -Reason 'NORMAL_PRECOMPILED_PHYSICAL_PASS' -ExitCode 0 -Extra @(
        "HEAD=$head",
        "SERIAL_PORT=$resolvedPort",
        "ARDUINO_CLI=$cliVersion",
        "CORE_SHA256=$coreFinal",
        "COMPILE_EXIT=$($compile.ExitCode)",
        "USES_STUB_CORE=$usesStub",
        "USES_SOURCE_CORE=$usesSource",
        "CORE_A_LINKED=$archiveLinked",
        "SOURCE_TU_COUNT=$($proof.SourceCount)",
        "STUB_TU_COUNT=$($proof.StubCount)",
        "FULL_PROFILE=$($proof.FullProfile)",
        "UPLOAD_EXIT=$($upload.ExitCode)",
        "CLIENT_EXIT=$($clientRun.ExitCode)",
        "IO_READY=$ioReady",
        "EN_IO_OUTPUT_ENABLE=$enIoEnable",
        "EN_IO_OUTPUT_LATCH=$enIoLatch",
        "LAST_SCAN_MS=$lastScanMs",
        "SOURCE_BLOBS_FINAL=$sourceBlobsFinal",
        "TRACKED_DIRTY_FINAL=$($dirtyFinal.Count)",
        "STAGED_FINAL=$($stagedFinal.Count)",
        "DIFF_CHECK_FINAL=$diffCheckFinal",
        "COMPILE_LOG=$CompileLog",
        "UPLOAD_LOG=$UploadLog",
        "CLIENT_LOG=$ClientLog"
    )
}
catch
{
    Finish-G2P4 -Status 'REVIEW' -Reason 'UNEXPECTED_GATE_EXCEPTION' -HarnessFailure 'YES' -ProductFailure 'UNDETERMINED' -ExitCode 90 -Extra @(
        "EXCEPTION=$($_.Exception.Message)"
    )
}
finally
{
    Pop-Location
}
