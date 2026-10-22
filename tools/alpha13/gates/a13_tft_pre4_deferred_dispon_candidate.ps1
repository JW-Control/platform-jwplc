param(
    [string]$ArduinoCli = 'C:\Program Files\Arduino PLC IDE Tools\arduino-cli.exe'
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$ExpectedBranch = 'v2.1.0-alpha.13/feature/cleanup-robustness'
$ExpectedTftEspiVersion = '2.5.43'
$ExpectedJwplcTftCppBlob = '2bdb55d504cfd2a1481ff561d33535d1740bb472'
$ExpectedTftSetupBlob = '773f8123844f783bfc67c3123150c27f29f45d34'
$ExpectedTftEspiCppSha = '01ed6edb0530d38b94ddeac079ba81633aa21d77d049b12da21a37f4bec69ee1'
$ExpectedSt7789InitSha = 'e21cae2ac84285dc0e77648eca67ca753f41f7da3c271594ede750b725136c10'
$ExpectedTftEspiHeaderSha = 'b1b2789ace7ac8fd4a4c414054757d91e6e62649a23d74226ea28ceb8d6f4462'
$Fqbn = 'jwplc_local:esp32:jwplcbasic'

$ScriptDir = $PSScriptRoot
$RepoRoot = [IO.Path]::GetFullPath((Join-Path $ScriptDir '..\..\..'))
$CommonPath = Join-Path $ScriptDir 'common.ps1'
$ProbeDir = Join-Path $RepoRoot 'tools\modbus-tcp-benchmark\firmware\a14_h3e1d1_tft_espi_compile_probe'
$JwplcTftRoot = Join-Path $RepoRoot 'JWPLC\2.1.0\libraries\JWPLC_TFT'
$JwplcTftCpp = Join-Path $JwplcTftRoot 'src\JWPLC_TFT.cpp'
$TftSetup = Join-Path $JwplcTftRoot 'src\tft_setup.h'

$RunId = Get-Date -Format 'yyyyMMdd_HHmmss'
$RunRoot = Join-Path $RepoRoot ("tools\alpha13\results\tft_pre4_{0}" -f $RunId)
$BuildRoot = Join-Path $env:TEMP ("jwplc_a13_tft_pre4_probe_{0}" -f $RunId)
$CandidateRoot = Join-Path $env:TEMP ("jwplc_a13_tft_pre4_candidate_{0}" -f $RunId)
$CandidateJwplcSrc = Join-Path $CandidateRoot 'JWPLC_TFT\src'
$CandidateBackendRoot = Join-Path $CandidateRoot 'TFT_eSPI'
$CompileLog = Join-Path $RunRoot 'compile_probe.log'
$SummaryLog = Join-Path $RunRoot 'SUMMARY.log'

New-Item -ItemType Directory -Force -Path $RunRoot,$BuildRoot,$CandidateJwplcSrc,$CandidateBackendRoot | Out-Null

. $CommonPath

function Finish-TFTPre4
{
    param(
        [Parameter(Mandatory = $true)][ValidateSet('PASS','REVIEW','FAIL')][string]$Status,
        [Parameter(Mandatory = $true)][string]$Reason,
        [string]$ProductFailure = 'NO',
        [string]$HarnessFailure = 'NO',
        [string]$EnvironmentFailure = 'NO',
        [int]$ExitCode = 0,
        [string[]]$Extra = @()
    )

    $out = @(
        'GATE=A13-TFT-PRE4',
        "STATUS=$Status",
        "REASON=$Reason",
        "PRODUCT_FAILURE=$ProductFailure",
        "HARNESS_FAILURE=$HarnessFailure",
        "ENVIRONMENT_FAILURE=$EnvironmentFailure",
        "RUN_ROOT=$RunRoot",
        "SUMMARY_LOG=$SummaryLog"
    ) + $Extra

    $out | Set-Content -LiteralPath $SummaryLog -Encoding utf8
    $out | ForEach-Object { Write-Host $_ }
    exit $ExitCode
}

function Get-GitBlobSha
{
    param([Parameter(Mandatory = $true)][string]$RelativePath)
    $value = @(& git -C $RepoRoot hash-object -- $RelativePath 2>&1)
    if ($LASTEXITCODE -ne 0 -or $value.Count -ne 1)
    {
        throw "git hash-object failed: $RelativePath"
    }
    return ([string]$value[0]).Trim()
}

function Replace-Once
{
    param(
        [Parameter(Mandatory = $true)][string]$Text,
        [Parameter(Mandatory = $true)][string]$Old,
        [Parameter(Mandatory = $true)][string]$New,
        [Parameter(Mandatory = $true)][string]$Name
    )

    $count = @([regex]::Matches($Text,[regex]::Escape($Old))).Count
    if ($count -ne 1)
    {
        throw "PRE4_ANCHOR_${Name}_COUNT=$count"
    }
    return $Text.Replace($Old,$New)
}

Write-Host '============================================================'
Write-Host ' A13-TFT-PRE4 - TEMP-ONLY DEFERRED DISPON CANDIDATE'
Write-Host '============================================================'

Push-Location $RepoRoot
try
{
    $branch = (git branch --show-current).Trim()
    if ($branch -ne $ExpectedBranch)
    {
        Finish-TFTPre4 -Status 'REVIEW' -Reason 'UNEXPECTED_BRANCH' -HarnessFailure 'YES' -ExitCode 3
    }

    $porcelain = @(& git status --porcelain=v1 --untracked-files=normal)
    if ($porcelain.Count -ne 0)
    {
        Finish-TFTPre4 -Status 'REVIEW' -Reason 'WORKTREE_NOT_CLEAN' -HarnessFailure 'YES' -ExitCode 4 -Extra @(
            "STATUS=$($porcelain -join ';')"
        )
    }

    if ((Get-GitBlobSha 'JWPLC/2.1.0/libraries/JWPLC_TFT/src/JWPLC_TFT.cpp') -ne $ExpectedJwplcTftCppBlob)
    {
        Finish-TFTPre4 -Status 'REVIEW' -Reason 'JWPLC_TFT_CPP_BLOB_MISMATCH' -HarnessFailure 'YES' -ExitCode 5
    }
    if ((Get-GitBlobSha 'JWPLC/2.1.0/libraries/JWPLC_TFT/src/tft_setup.h') -ne $ExpectedTftSetupBlob)
    {
        Finish-TFTPre4 -Status 'REVIEW' -Reason 'TFT_SETUP_BLOB_MISMATCH' -HarnessFailure 'YES' -ExitCode 6
    }

    if (-not (Test-Path -LiteralPath $ArduinoCli))
    {
        $cli = Get-Command arduino-cli -ErrorAction SilentlyContinue
        if ($null -eq $cli)
        {
            Finish-TFTPre4 -Status 'REVIEW' -Reason 'ARDUINO_CLI_NOT_FOUND' -EnvironmentFailure 'YES' -ExitCode 7
        }
        $ArduinoCli = $cli.Source
    }

    $compile = Invoke-A13NativeCaptured -FilePath $ArduinoCli -Arguments @(
        'compile','--fqbn',$Fqbn,'-j','0','-v','--clean','--build-path',$BuildRoot,$ProbeDir
    )
    $compile.Output | Set-Content -LiteralPath $CompileLog -Encoding utf8
    Write-Host "PROBE_COMPILE_EXIT=$($compile.ExitCode)"
    if ($compile.ExitCode -ne 0)
    {
        Finish-TFTPre4 -Status 'REVIEW' -Reason 'BACKEND_PROBE_COMPILE_FAILED' -EnvironmentFailure 'YES' -ExitCode 8 -Extra @(
            "COMPILE_LOG=$CompileLog"
        )
    }

    $selectionLines = @($compile.Output | Where-Object { $_ -match '^Using library TFT_eSPI at version .+ in folder: .+$' })
    if ($selectionLines.Count -ne 1)
    {
        Finish-TFTPre4 -Status 'REVIEW' -Reason 'TFT_ESPI_SELECTION_AMBIGUOUS' -HarnessFailure 'YES' -ExitCode 9
    }
    $m = [regex]::Match($selectionLines[0],'^Using library TFT_eSPI at version (?<version>\S+) in folder: (?<folder>.+)$')
    if (-not $m.Success)
    {
        Finish-TFTPre4 -Status 'REVIEW' -Reason 'TFT_ESPI_SELECTION_PARSE_FAILED' -HarnessFailure 'YES' -ExitCode 10
    }

    $version = $m.Groups['version'].Value.Trim()
    $backendRoot = [IO.Path]::GetFullPath($m.Groups['folder'].Value.Trim())
    Write-Host "TFT_ESPI_VERSION=$version"
    Write-Host "TFT_ESPI_ROOT=$backendRoot"
    if ($version -ne $ExpectedTftEspiVersion)
    {
        Finish-TFTPre4 -Status 'REVIEW' -Reason 'TFT_ESPI_VERSION_MISMATCH' -EnvironmentFailure 'YES' -ExitCode 11
    }

    $backendCpp = Join-Path $backendRoot 'TFT_eSPI.cpp'
    $backendHeader = Join-Path $backendRoot 'TFT_eSPI.h'
    $backendInit = Join-Path $backendRoot 'TFT_Drivers\ST7789_Init.h'
    foreach ($p in @($backendCpp,$backendHeader,$backendInit))
    {
        if (-not (Test-Path -LiteralPath $p))
        {
            Finish-TFTPre4 -Status 'REVIEW' -Reason "BACKEND_SOURCE_MISSING:$p" -EnvironmentFailure 'YES' -ExitCode 12
        }
    }

    if ((Get-A13Sha256 $backendCpp) -ne $ExpectedTftEspiCppSha -or
        (Get-A13Sha256 $backendInit) -ne $ExpectedSt7789InitSha -or
        (Get-A13Sha256 $backendHeader) -ne $ExpectedTftEspiHeaderSha)
    {
        Finish-TFTPre4 -Status 'REVIEW' -Reason 'BACKEND_HASH_MISMATCH' -EnvironmentFailure 'YES' -ExitCode 13
    }

    Copy-Item -LiteralPath $JwplcTftCpp -Destination (Join-Path $CandidateJwplcSrc 'JWPLC_TFT.cpp') -Force
    Copy-Item -LiteralPath $TftSetup -Destination (Join-Path $CandidateJwplcSrc 'tft_setup.h') -Force
    Copy-Item -LiteralPath $backendInit -Destination (Join-Path $CandidateBackendRoot 'ST7789_Init.h') -Force

    $utf8 = New-Object Text.UTF8Encoding($false)

    $setupCandidate = [IO.File]::ReadAllText((Join-Path $CandidateJwplcSrc 'tft_setup.h'))
    $setupOld = '#define JWPLC_TFT_BACKEND_SETUP 1'
    $setupNew = $setupOld + [Environment]::NewLine + '#define JWPLC_TFT_DEFER_DISPON 1'
    $setupCandidate = Replace-Once -Text $setupCandidate -Old $setupOld -New $setupNew -Name 'SETUP_DEFINE'
    [IO.File]::WriteAllText((Join-Path $CandidateJwplcSrc 'tft_setup.h'),$setupCandidate,$utf8)

    $cppCandidate = [IO.File]::ReadAllText((Join-Path $CandidateJwplcSrc 'JWPLC_TFT.cpp'))
    $nsOld = '    static constexpr uint8_t ACTIVE_ROTATION = 1;'
    $nsNew = $nsOld + [Environment]::NewLine + '    static constexpr uint8_t ST7789_CMD_DISPON = 0x29;'
    $cppCandidate = Replace-Once -Text $cppCandidate -Old $nsOld -New $nsNew -Name 'DISPON_CONST'

    $beginOld = '    g_backend.init();' + [Environment]::NewLine + '    g_backend.setRotation(ACTIVE_ROTATION);'
    $beginNew = @(
        '    g_backend.init();',
        '    g_backend.setRotation(ACTIVE_ROTATION);',
        '    g_backend.fillScreen(JWPLC_TFT_BLACK);',
        '    g_backend.writecommand(ST7789_CMD_DISPON);',
        '    delay(120);'
    ) -join [Environment]::NewLine
    $cppCandidate = Replace-Once -Text $cppCandidate -Old $beginOld -New $beginNew -Name 'BEGIN_SEQUENCE'
    [IO.File]::WriteAllText((Join-Path $CandidateJwplcSrc 'JWPLC_TFT.cpp'),$cppCandidate,$utf8)

    $initText = [IO.File]::ReadAllText((Join-Path $CandidateBackendRoot 'ST7789_Init.h'))
    $pattern = '(?m)^(?<indent>\s*)writecommand\(ST7789_DISPON\);\s*//\s*Display on\s*\r?\n\k<indent>delay\(120\);'
    $matches = [regex]::Matches($initText,$pattern)
    if ($matches.Count -ne 2)
    {
        Finish-TFTPre4 -Status 'REVIEW' -Reason 'ST7789_DISPON_PATCH_ANCHOR_COUNT_INVALID' -HarnessFailure 'YES' -ExitCode 14 -Extra @(
            "ANCHOR_COUNT=$($matches.Count)"
        )
    }

    $replacement = @'
${indent}#ifndef JWPLC_TFT_DEFER_DISPON
${indent}writecommand(ST7789_DISPON);    // Display on
${indent}delay(120);
${indent}#endif
'@
    $initCandidate = [regex]::Replace($initText,$pattern,{
        param($match)
        $indent = $match.Groups['indent'].Value
        return ($replacement -replace '\$\{indent\}',$indent)
    })
    [IO.File]::WriteAllText((Join-Path $CandidateBackendRoot 'ST7789_Init.h'),$initCandidate,$utf8)

    $candidateCppSha = Get-A13Sha256 (Join-Path $CandidateJwplcSrc 'JWPLC_TFT.cpp')
    $candidateSetupSha = Get-A13Sha256 (Join-Path $CandidateJwplcSrc 'tft_setup.h')
    $candidateInitSha = Get-A13Sha256 (Join-Path $CandidateBackendRoot 'ST7789_Init.h')

    $contract = (
        $cppCandidate.Contains('g_backend.fillScreen(JWPLC_TFT_BLACK);') -and
        $cppCandidate.Contains('g_backend.writecommand(ST7789_CMD_DISPON);') -and
        $cppCandidate.Contains('delay(120);') -and
        $setupCandidate.Contains('#define JWPLC_TFT_DEFER_DISPON 1') -and
        ([regex]::Matches($initCandidate,'JWPLC_TFT_DEFER_DISPON')).Count -eq 2
    )
    if (-not $contract)
    {
        Finish-TFTPre4 -Status 'REVIEW' -Reason 'CANDIDATE_STATIC_CONTRACT_FAILED' -HarnessFailure 'YES' -ExitCode 15
    }

    $finalPorcelain = @(& git status --porcelain=v1 --untracked-files=normal)
    & git diff --check
    $diffCheck = ($LASTEXITCODE -eq 0)
    if ($finalPorcelain.Count -ne 0 -or -not $diffCheck)
    {
        Finish-TFTPre4 -Status 'REVIEW' -Reason 'FINAL_REPO_AUDIT_FAILED' -HarnessFailure 'YES' -ExitCode 16
    }

    Finish-TFTPre4 -Status 'PASS' -Reason 'TEMP_DEFERRED_DISPON_CANDIDATE_READY' -ExitCode 0 -Extra @(
        "TFT_ESPI_VERSION=$version",
        "TFT_ESPI_ROOT=$backendRoot",
        "CANDIDATE_ROOT=$CandidateRoot",
        "CANDIDATE_JWPLC_TFT_CPP_SHA256=$candidateCppSha",
        "CANDIDATE_TFT_SETUP_SHA256=$candidateSetupSha",
        "CANDIDATE_ST7789_INIT_SHA256=$candidateInitSha",
        'BACKEND_ORIGINAL_MUTATED=NO',
        'REPO_PRODUCT_MUTATED=NO',
        'DISPON_DEFERRED_BY_PRIVATE_MACRO=YES',
        'GRAM_BLACK_BEFORE_DISPON=YES',
        'POST_DISPON_DELAY_MS=120',
        'WORKTREE_FINAL=CLEAN',
        "DIFF_CHECK_FINAL=$diffCheck",
        "COMPILE_LOG=$CompileLog"
    )
}
catch
{
    Finish-TFTPre4 -Status 'REVIEW' -Reason 'UNEXPECTED_GATE_EXCEPTION' -HarnessFailure 'YES' -ProductFailure 'UNDETERMINED' -ExitCode 90 -Extra @(
        "EXCEPTION=$($_.Exception.Message)"
    )
}
finally
{
    Pop-Location
}
