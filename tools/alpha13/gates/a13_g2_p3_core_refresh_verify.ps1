param(
    [string]$ArduinoCli = 'C:\Program Files\Arduino PLC IDE Tools\arduino-cli.exe'
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$ExpectedBranch = 'v2.1.0-alpha.13/feature/cleanup-robustness'
$SourceFirstHead = 'd41bd884e7b65b7561b78447a3ac0320558ebd30'
$ExpectedCoreBefore = '78d0c0ab14f156b96116529e88872340d51877af40d24ba3559f6081e0bf34fb'

$ExpectedCandidate = @{
    'JWPLC/2.1.0/cores/jwcontrol/peripherals_init.cpp' = '23efb3935a34e6b5649875b57c804e60538827cd'
    'JWPLC/2.1.0/cores/jwcontrol/jwplc_peripherals.cpp' = 'c3d53566d274e95b7dda111327140b5393f7db35'
    'JWPLC/2.1.0/cores/jwcontrol/jwplc_peripherals.h' = '288667f1caa08142e2a155b8c85f24b2aa5beb44'
}

$ScriptDir = $PSScriptRoot
$RepoRoot = [IO.Path]::GetFullPath((Join-Path $ScriptDir '..\..\..'))
$CommonPath = Join-Path $ScriptDir 'common.ps1'
$BuildScript = Join-Path $RepoRoot 'tools\build-speed-benchmark\Build-JWPLCPrecompiledCore.ps1'
$VerifyScript = Join-Path $RepoRoot 'tools\build-speed-benchmark\Verify-JWPLCPrecompiledCore.ps1'
$CoreRelative = 'JWPLC/2.1.0/precompiled/core/JWPLCBASIC/core.a'
$CorePath = Join-Path $RepoRoot $CoreRelative
$BoardsLocalPath = Join-Path $RepoRoot 'JWPLC\2.1.0\boards.local.txt'

$RunId = Get-Date -Format 'yyyyMMdd_HHmmss'
$RunRoot = Join-Path $RepoRoot ("tools\alpha13\results\g2_p3_{0}" -f $RunId)
$TempRoot = Join-Path $env:TEMP ("jwplc_a13_g2_p3_{0}" -f $RunId)
$BuildOutputRoot = Join-Path $TempRoot 'build'
$VerifyOutputRoot = Join-Path $TempRoot 'verify'
$CoreBackupPath = Join-Path $TempRoot 'core_before.a'
$SummaryLog = Join-Path $RunRoot 'SUMMARY.log'
$BuildWrapperLog = Join-Path $RunRoot 'build_core_wrapper.log'
$VerifyWrapperLog = Join-Path $RunRoot 'verify_core_wrapper.log'

New-Item -ItemType Directory -Path $RunRoot -Force | Out-Null
New-Item -ItemType Directory -Path $TempRoot -Force | Out-Null

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

function Get-OptionalFileState
{
    param([Parameter(Mandatory = $true)][string]$Path)

    if (-not (Test-Path -LiteralPath $Path))
    {
        return [PSCustomObject]@{ Exists = $false; Length = [int64]0; SHA256 = '' }
    }

    $item = Get-Item -LiteralPath $Path
    return [PSCustomObject]@{
        Exists = $true
        Length = [int64]$item.Length
        SHA256 = Get-A13Sha256 -Path $Path
    }
}

function Test-OptionalFileStateEqual
{
    param(
        [Parameter(Mandatory = $true)][object]$A,
        [Parameter(Mandatory = $true)][object]$B
    )

    if ($A.Exists -ne $B.Exists) { return $false }
    if (-not $A.Exists) { return $true }
    return ($A.Length -eq $B.Length -and $A.SHA256 -eq $B.SHA256)
}

function Copy-ChildEvidence
{
    param(
        [Parameter(Mandatory = $true)][string]$Root,
        [Parameter(Mandatory = $true)][string]$Prefix
    )

    if (-not (Test-Path -LiteralPath $Root)) { return }

    $files = @(
        Get-ChildItem -LiteralPath $Root -Recurse -File |
            Where-Object { $_.Extension -eq '.log' -or $_.Extension -eq '.md' }
    )

    foreach ($file in $files)
    {
        $destName = $Prefix + '_' + $file.Name
        Copy-Item -LiteralPath $file.FullName -Destination (Join-Path $RunRoot $destName) -Force
    }
}

function Restore-CoreBefore
{
    if (Test-Path -LiteralPath $CoreBackupPath)
    {
        Copy-Item -LiteralPath $CoreBackupPath -Destination $CorePath -Force
    }
}

function Finish-G2P3
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

    if ($Status -ne 'PASS') { Restore-CoreBefore }

    $lines = @(
        'GATE=A13-G2-P3',
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
Write-Host ' A13-G2-P3 - CORE.A REFRESH + NORMAL JWPLCBASIC VERIFY'
Write-Host '============================================================'

Push-Location $RepoRoot
try
{
    foreach ($required in @($CommonPath, $BuildScript, $VerifyScript, $CorePath))
    {
        if (-not (Test-Path -LiteralPath $required))
        {
            Finish-G2P3 -Status 'REVIEW' -Reason "REQUIRED_PATH_MISSING:$required" -HarnessFailure 'YES' -ExitCode 3
        }
    }

    $branch = (git branch --show-current).Trim()
    $head = (git rev-parse HEAD).Trim()

    if ($branch -ne $ExpectedBranch)
    {
        Finish-G2P3 -Status 'REVIEW' -Reason 'UNEXPECTED_BRANCH' -HarnessFailure 'YES' -ExitCode 4 -Extra @("BRANCH=$branch","HEAD=$head")
    }

    & git merge-base --is-ancestor $SourceFirstHead $head
    if ($LASTEXITCODE -ne 0)
    {
        Finish-G2P3 -Status 'REVIEW' -Reason 'SOURCE_FIRST_HEAD_NOT_ANCESTOR' -HarnessFailure 'YES' -ExitCode 5
    }

    $committedProduct = @(
        & git diff --name-only "$SourceFirstHead..$head" -- 'JWPLC/2.1.0/cores/jwcontrol' $CoreRelative |
            Where-Object { $_ -and $_.Trim().Length -gt 0 }
    )

    if ($committedProduct.Count -ne 0)
    {
        Finish-G2P3 -Status 'REVIEW' -Reason 'UNEXPECTED_COMMITTED_PRODUCT_CHANGE' -HarnessFailure 'YES' -ExitCode 6 -Extra @("COMMITTED_PRODUCT=$($committedProduct -join ';')")
    }

    $dirty = @(Get-A13TrackedDirty | Sort-Object)
    $staged = @(Get-A13Staged)
    $expectedDirty = @($ExpectedCandidate.Keys | Sort-Object)
    $dirtyDiff = @(Compare-Object -ReferenceObject $expectedDirty -DifferenceObject $dirty)

    if ($dirtyDiff.Count -ne 0 -or $staged.Count -ne 0)
    {
        Finish-G2P3 -Status 'REVIEW' -Reason 'CANDIDATE_SCOPE_INVALID' -HarnessFailure 'YES' -ExitCode 7 -Extra @("DIRTY=$($dirty -join ';')","STAGED=$($staged -join ';')")
    }

    & git diff --check
    if ($LASTEXITCODE -ne 0)
    {
        Finish-G2P3 -Status 'REVIEW' -Reason 'GIT_DIFF_CHECK_FAILED' -HarnessFailure 'YES' -ExitCode 8
    }

    foreach ($relative in $ExpectedCandidate.Keys)
    {
        $actual = Get-GitBlobSha -Path $relative
        if ($actual -ne $ExpectedCandidate[$relative])
        {
            Finish-G2P3 -Status 'REVIEW' -Reason 'CANDIDATE_BLOB_MISMATCH' -HarnessFailure 'YES' -ExitCode 9 -Extra @("FILE=$relative","ACTUAL=$actual","EXPECTED=$($ExpectedCandidate[$relative])")
        }
    }

    $coreBefore = Get-A13Sha256 -Path $CorePath
    if ($coreBefore -ne $ExpectedCoreBefore)
    {
        Finish-G2P3 -Status 'REVIEW' -Reason 'CORE_BEFORE_SHA_MISMATCH' -HarnessFailure 'YES' -ExitCode 10 -Extra @("CORE_SHA256=$coreBefore","EXPECTED_CORE_SHA256=$ExpectedCoreBefore")
    }

    $boardsBefore = Get-OptionalFileState -Path $BoardsLocalPath

    if (-not (Test-Path -LiteralPath $ArduinoCli))
    {
        $cliCommand = Get-Command arduino-cli -ErrorAction SilentlyContinue
        if ($null -eq $cliCommand)
        {
            Finish-G2P3 -Status 'REVIEW' -Reason 'ARDUINO_CLI_NOT_FOUND' -EnvironmentFailure 'YES' -ExitCode 11
        }
        $ArduinoCli = $cliCommand.Source
    }

    $cliVersion = @(& $ArduinoCli version 2>&1 | ForEach-Object { $_.ToString() }) -join ' '
    Write-Host "ARDUINO_CLI=$cliVersion"
    if ($cliVersion -notmatch 'Version:\s*1\.0\.2')
    {
        Finish-G2P3 -Status 'REVIEW' -Reason 'ARDUINO_CLI_VERSION_MISMATCH' -EnvironmentFailure 'YES' -ExitCode 12
    }

    $pwsh = (Get-Command pwsh -ErrorAction Stop).Source
    $pwshVersion = $PSVersionTable.PSVersion.ToString()
    Write-Host "PWSH=$pwsh"
    Write-Host "PWSH_VERSION=$pwshVersion"
    Write-Host "HEAD=$head"
    Write-Host "CORE_BEFORE_SHA256=$coreBefore"

    Copy-Item -LiteralPath $CorePath -Destination $CoreBackupPath -Force

    $buildArgs = @(
        '-NoProfile','-ExecutionPolicy','Bypass','-File',$BuildScript,
        '-ArduinoCli',$ArduinoCli,'-Targets','Basic','-Sketch','01_empty','-Jobs','0',
        '-OutputRoot',$BuildOutputRoot
    )
    $build = Invoke-A13NativeCaptured -FilePath $pwsh -Arguments $buildArgs
    $build.Output | Set-Content -LiteralPath $BuildWrapperLog -Encoding utf8
    $build.Output | ForEach-Object { Write-Host $_ }
    Copy-ChildEvidence -Root $BuildOutputRoot -Prefix 'build'

    if ($build.ExitCode -ne 0)
    {
        Finish-G2P3 -Status 'REVIEW' -Reason 'CORE_BUILD_SCRIPT_FAILED' -HarnessFailure 'YES' -ExitCode 20 -Extra @("BUILD_EXIT=$($build.ExitCode)","BUILD_LOG=$BuildWrapperLog")
    }

    $buildText = $build.Output -join [Environment]::NewLine
    if ($buildText -notmatch 'CORE_PRECOMPILED_BUILD=PASS')
    {
        Finish-G2P3 -Status 'REVIEW' -Reason 'CORE_BUILD_PASS_MARKER_MISSING' -HarnessFailure 'YES' -ExitCode 21
    }

    $coreAfterBuild = Get-A13Sha256 -Path $CorePath
    $coreBytes = (Get-Item -LiteralPath $CorePath).Length
    if ($coreAfterBuild -eq $coreBefore)
    {
        Finish-G2P3 -Status 'REVIEW' -Reason 'CORE_ARCHIVE_SHA_UNCHANGED' -HarnessFailure 'YES' -ExitCode 22 -Extra @("CORE_SHA256=$coreAfterBuild")
    }

    $verifyArgs = @(
        '-NoProfile','-ExecutionPolicy','Bypass','-File',$VerifyScript,
        '-Target','Basic','-ArduinoCli',$ArduinoCli,'-Jobs','0','-OutputRoot',$VerifyOutputRoot
    )
    $verify = Invoke-A13NativeCaptured -FilePath $pwsh -Arguments $verifyArgs
    $verify.Output | Set-Content -LiteralPath $VerifyWrapperLog -Encoding utf8
    $verify.Output | ForEach-Object { Write-Host $_ }
    Copy-ChildEvidence -Root $VerifyOutputRoot -Prefix 'verify'

    if ($verify.ExitCode -ne 0)
    {
        Finish-G2P3 -Status 'REVIEW' -Reason 'CORE_VERIFY_SCRIPT_FAILED' -HarnessFailure 'YES' -ExitCode 30 -Extra @("VERIFY_EXIT=$($verify.ExitCode)","VERIFY_LOG=$VerifyWrapperLog")
    }

    $verifyText = $verify.Output -join [Environment]::NewLine
    $verifyContract = (
        $verifyText -match 'CORE_PRECOMPILED_VERIFY_BASIC=PASS' -and
        $verifyText -match 'Using stub normalizado:\s*True' -and
        $verifyText -match 'Using core fuente:\s*False' -and
        $verifyText -match 'precompiled_core_stub\.c:\s*1' -and
        $verifyText -match 'core\.a JWPLCBASIC enlazado:\s*True'
    )
    if (-not $verifyContract)
    {
        Finish-G2P3 -Status 'REVIEW' -Reason 'NORMAL_JWPLCBASIC_LINK_CONTRACT_FAILED' -HarnessFailure 'YES' -ExitCode 31
    }

    $boardsAfter = Get-OptionalFileState -Path $BoardsLocalPath
    if (-not (Test-OptionalFileStateEqual -A $boardsBefore -B $boardsAfter))
    {
        Finish-G2P3 -Status 'REVIEW' -Reason 'BOARDS_LOCAL_CHANGED' -HarnessFailure 'YES' -ExitCode 32
    }

    $coreFinal = Get-A13Sha256 -Path $CorePath
    $dirtyFinal = @(Get-A13TrackedDirty | Sort-Object)
    $stagedFinal = @(Get-A13Staged)
    $expectedFinalDirty = @($ExpectedCandidate.Keys + $CoreRelative | Sort-Object)
    $dirtyFinalDiff = @(Compare-Object -ReferenceObject $expectedFinalDirty -DifferenceObject $dirtyFinal)

    & git diff --check
    $diffCheck = ($LASTEXITCODE -eq 0)

    if ($coreFinal -ne $coreAfterBuild -or $dirtyFinalDiff.Count -ne 0 -or $stagedFinal.Count -ne 0 -or -not $diffCheck)
    {
        Finish-G2P3 -Status 'REVIEW' -Reason 'FINAL_REPO_AUDIT_FAILED' -HarnessFailure 'YES' -ExitCode 40 -Extra @("CORE_FINAL_SHA256=$coreFinal","DIRTY=$($dirtyFinal -join ';')","STAGED=$($stagedFinal -join ';')","DIFF_CHECK=$diffCheck")
    }

    Finish-G2P3 -Status 'PASS' -Reason 'CORE_REFRESH_AND_NORMAL_LINK_PASS' -ExitCode 0 -Extra @(
        "HEAD=$head",
        "PWSH_VERSION=$pwshVersion",
        "ARDUINO_CLI=$cliVersion",
        "CORE_BEFORE_SHA256=$coreBefore",
        "CORE_AFTER_SHA256=$coreFinal",
        "CORE_AFTER_BYTES=$coreBytes",
        "BUILD_EXIT=$($build.ExitCode)",
        "VERIFY_EXIT=$($verify.ExitCode)",
        'NORMAL_JWPLCBASIC_STUB=True',
        'NORMAL_JWPLCBASIC_SOURCE_CORE=False',
        'NORMAL_JWPLCBASIC_CORE_A_LINKED=True',
        'BOARDS_LOCAL_UNCHANGED=True',
        "TRACKED_DIRTY_FINAL=$($dirtyFinal.Count)",
        "STAGED_FINAL=$($stagedFinal.Count)",
        "DIFF_CHECK_FINAL=$diffCheck",
        "BUILD_LOG=$BuildWrapperLog",
        "VERIFY_LOG=$VerifyWrapperLog"
    )
}
catch
{
    Finish-G2P3 -Status 'REVIEW' -Reason 'UNEXPECTED_GATE_EXCEPTION' -HarnessFailure 'YES' -ExitCode 90 -Extra @("EXCEPTION=$($_.Exception.Message)")
}
finally
{
    Pop-Location
}
