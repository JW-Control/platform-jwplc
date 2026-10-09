param([string]$PythonExe = '')
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$RepoRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..\..'))
$ResultsRoot = Join-Path $RepoRoot 'tools\alpha13\results'
$Generator = Join-Path $PSScriptRoot 'a13_tft_pre6_p1_source_regen.py'
$Common = Join-Path $PSScriptRoot 'common.ps1'
$OldTft = Join-Path $RepoRoot 'JWPLC\2.1.0\libraries\JWPLC_TFT\src\esp32\libJWPLC_TFT.a'
$OldCore = Join-Path $RepoRoot 'JWPLC\2.1.0\precompiled\core\JWPLCBASIC\core.a'
$OldDisplay = Join-Path $RepoRoot 'JWPLC\2.1.0\libraries\JWPLC_Display\src\esp32\libJWPLC_Display.a'

$ExpectedP0 = 'ff9dd89cb267bc270d2ca6fa2d0f1362b76595b8dc6b49f764d05a8e28bb6705'
$ExpectedCpp = '494440b00e74e74a7b23975420574e035ef2138fdf5a0cea2fed51c986c29d25'
$ExpectedSetup = '8fa079444ca130772d3a642eaf814035100bc098032b403c922c458d351ae5e1'
$ExpectedInit = '44873be82fe836084934a328df77f098e9ab88d212dd1d570da5e8aac74671bc'
$ExpectedOldTft = '5d860a131811dd9a7eb6fa55f5674b1d78b0de7dfaf8748ce18a60ceed2d3738'
$ExpectedOldCore = '6f328eeb796091070c8d852a2c0e90f71047d8d2b5786fe3cd5079b2fb6ff983'
$ExpectedOldDisplay = 'c960d718433e29a40e3cc55bc745c9a2e121ee1ee598ec72c04872327592dc02'
$ExpectedBranch = 'v2.1.0-alpha.13/feature/cleanup-robustness'

$RunId = Get-Date -Format 'yyyyMMdd_HHmmss'
$RunRoot = Join-Path $ResultsRoot ('tft_pre6_p1a_' + $RunId)
$TempRoot = Join-Path $env:TEMP ('jwplc_a13_tft_pre6_p1a_' + $RunId)
$Summary = Join-Path $RunRoot 'SUMMARY.log'
$RegenLog = Join-Path $RunRoot 'regen.log'
New-Item -ItemType Directory -Force -Path $RunRoot | Out-Null
. $Common

$script:Phase = 'PREFLIGHT'
function Finish-P1A {
    param([string]$Status,[string]$Reason,[string]$Harness='NO',[int]$ExitCode=0,[string[]]$Extra=@())
    $lines = @(
        'GATE=A13-TFT-PRE6-P1A',
        "STATUS=$Status",
        "REASON=$Reason",
        "HARNESS_FAILURE=$Harness",
        'PRODUCT_FAILURE=NO',
        'UPLOAD_EXECUTED=NO',
        'COMPILE_EXECUTED=NO',
        'PRODUCT_REPO_MODIFIED=NO',
        "PHASE=$script:Phase",
        "RUN_ROOT=$RunRoot",
        "SUMMARY_LOG=$Summary"
    ) + $Extra
    $lines | Set-Content -LiteralPath $Summary -Encoding utf8
    $lines | ForEach-Object { Write-Host $_ }
    exit $ExitCode
}
function Stop-P1A {
    param([string]$Reason)
    Finish-P1A -Status 'REVIEW' -Reason $Reason -Harness 'YES' -ExitCode 3
}
function Guard-Integrity {
    $dirty = @(& git -C $RepoRoot status --porcelain=v1 --untracked-files=normal)
    if ($dirty.Count -ne 0) { Stop-P1A ('WORKTREE_NOT_CLEAN:' + ($dirty -join ';')) }
    if ((Get-A13Sha256 -Path $OldTft) -ne $ExpectedOldTft -or
        (Get-A13Sha256 -Path $OldCore) -ne $ExpectedOldCore -or
        (Get-A13Sha256 -Path $OldDisplay) -ne $ExpectedOldDisplay) {
        Stop-P1A 'OFFICIAL_ARCHIVE_SHA_MISMATCH'
    }
    & git -C $RepoRoot diff --check
    if ($LASTEXITCODE -ne 0) { Stop-P1A 'GIT_DIFF_CHECK_FAILED' }
}
function Read-Key {
    param(
        [Parameter(Mandatory = $true)][string]$Text,
        [Parameter(Mandatory = $true)][string]$Key
    )

    # This is a real invocation, not a bare token after 'return'.
    # Match unique canonical KEY=VALUE lines only.
    $value = Get-A13LogValue -Text $Text -Key $Key
    return $value
}
function Assert-ProofParser {
    $fixture = @(
        'GATE=A13-TFT-PRE6-P0',
        'STATUS=PASS',
        'REASON=TEMP_PRECOMPILED_ARCHIVE_QUALIFIED',
        "CANDIDATE_ARCHIVE_SHA256=$ExpectedP0",
        'ARCHIVE_MEMBER_PARITY=PASS',
        'COMPILE_CASES_PASS=3'
    ) -join "`n"
    $expect = @{
        'STATUS' = 'PASS'
        'REASON' = 'TEMP_PRECOMPILED_ARCHIVE_QUALIFIED'
        'CANDIDATE_ARCHIVE_SHA256' = $ExpectedP0
        'ARCHIVE_MEMBER_PARITY' = 'PASS'
        'COMPILE_CASES_PASS' = '3'
    }
    foreach ($key in $expect.Keys) {
        $got = Read-Key -Text $fixture -Key $key
        if ($got -cne $expect[$key]) {
            Stop-P1A "PROOF_PARSER_SELF_TEST_FAILED:$key"
        }
    }
    Write-Host 'PROOF_PARSER_SELF_TEST=PASS'
}
function Latest-Proof {
    param(
        [string]$Filter,
        [string]$ExpectedReason,
        [string]$HashKey,
        [string]$ExpectedHash
    )
    if (-not (Test-Path -LiteralPath $ResultsRoot)) {
        Stop-P1A "RESULTS_ROOT_NOT_FOUND:$ResultsRoot"
    }
    $dirs = @(
        Get-ChildItem -LiteralPath $ResultsRoot -Directory -Filter $Filter -ErrorAction SilentlyContinue |
            Sort-Object Name -Descending
    )
    Write-Host "PROOF_SEARCH_FILTER=$Filter"
    Write-Host "PROOF_SEARCH_DIRECTORY_COUNT=$($dirs.Count)"
    foreach ($dir in $dirs) {
        $path = Join-Path $dir.FullName 'SUMMARY.log'
        if (-not (Test-Path -LiteralPath $path)) {
            Write-Host "PROOF_SUMMARY_MISSING=$path"
            continue
        }
        $body = [IO.File]::ReadAllText($path)
        $status = Read-Key -Text $body -Key 'STATUS'
        $reason = Read-Key -Text $body -Key 'REASON'
        $sha = Read-Key -Text $body -Key $HashKey

        Write-Host "PROOF_SUMMARY_CANDIDATE=$path"
        Write-Host "PROOF_STATUS=$status"
        Write-Host "PROOF_REASON=$reason"
        Write-Host "PROOF_SHA=$sha"

        if ($status -ceq 'PASS' -and $reason -ceq $ExpectedReason -and
            $sha -ceq $ExpectedHash) {
            Write-Host "PROOF_MATCH=YES"
            return [pscustomobject]@{ Path=$path; Text=$body }
        }
        Write-Host "PROOF_MATCH=NO"
    }
    return $null
}

Write-Host '============================================================'
Write-Host ' A13-TFT-PRE6-P1A - INDEPENDENT SOURCE REGEN'
Write-Host '============================================================'
Push-Location $RepoRoot
try {
    foreach ($p in @($Common,$Generator,$OldTft,$OldCore,$OldDisplay)) {
        if (-not (Test-Path -LiteralPath $p)) { Stop-P1A ('MISSING_PATH:'+$p) }
    }
    $branch = (& git branch --show-current).Trim()
    $head = (& git rev-parse HEAD).Trim()
    if ($branch -ne $ExpectedBranch) { Stop-P1A 'UNEXPECTED_BRANCH' }
    Guard-Integrity

    $script:Phase = 'P0_PROVENANCE'
    Assert-ProofParser
    $p0 = Latest-Proof 'tft_pre6_p0_*' 'TEMP_PRECOMPILED_ARCHIVE_QUALIFIED' 'CANDIDATE_ARCHIVE_SHA256' $ExpectedP0
    if ($null -eq $p0) { Stop-P1A 'P0_PASS_PROOF_NOT_FOUND' }
    if ((Read-Key $p0.Text 'COMPILE_CASES_PASS') -ne '3' -or
        (Read-Key $p0.Text 'ARCHIVE_MEMBER_PARITY') -ne 'PASS') {
        Stop-P1A 'P0_PROOF_CONTRACT_FAILED'
    }
    $p0Archive = Read-Key $p0.Text 'CANDIDATE_ARCHIVE'
    if (-not (Test-Path -LiteralPath $p0Archive)) { Stop-P1A 'P0_ARCHIVE_NOT_FOUND' }
    if ((Get-A13Sha256 -Path $p0Archive) -ne $ExpectedP0) { Stop-P1A 'P0_ARCHIVE_HASH_CHANGED' }

    $script:Phase = 'PRE4_PROVENANCE'
    $pre4 = Latest-Proof 'tft_pre4_*' 'TEMP_DEFERRED_DISPON_CANDIDATE_READY' 'CANDIDATE_JWPLC_TFT_CPP_SHA256' $ExpectedCpp
    if ($null -eq $pre4) { Stop-P1A 'PRE4_PASS_PROOF_NOT_FOUND' }
    if ((Read-Key $pre4.Text 'CANDIDATE_TFT_SETUP_SHA256') -ne $ExpectedSetup -or
        (Read-Key $pre4.Text 'CANDIDATE_ST7789_INIT_SHA256') -ne $ExpectedInit) {
        Stop-P1A 'PRE4_PROOF_CONTRACT_FAILED'
    }
    $backend = Read-Key $pre4.Text 'TFT_ESPI_ROOT'
    if ([string]::IsNullOrWhiteSpace($backend) -or -not (Test-Path -LiteralPath $backend)) {
        Stop-P1A 'TFT_ESPI_SOURCE_NOT_FOUND'
    }
    Write-Host "P0_SUMMARY=$($p0.Path)"
    Write-Host "PRE4_SUMMARY=$($pre4.Path)"
    Write-Host "TFT_ESPI_SOURCE=$backend"

    if ([string]::IsNullOrWhiteSpace($PythonExe)) {
        $command = Get-Command python.exe -ErrorAction SilentlyContinue
        if ($null -eq $command) { $command = Get-Command python -ErrorAction SilentlyContinue }
        if ($null -eq $command) { Stop-P1A 'PYTHON_NOT_FOUND' }
        $PythonExe = $command.Source
    }

    $script:Phase = 'SOURCE_REGEN'
    $run = Invoke-A13NativeCaptured -FilePath $PythonExe -Arguments @('-B',$Generator,'--repo',$RepoRoot,'--backend',$backend,'--output',$TempRoot)
    $run.Output | Set-Content -LiteralPath $RegenLog -Encoding utf8
    $run.Output | ForEach-Object { Write-Host $_ }
    if ($run.ExitCode -ne 0) { Stop-P1A 'SOURCE_REGEN_PYTHON_FAILED' }
    $out = $run.Output -join [Environment]::NewLine
    if ((Read-Key $out 'STATUS') -ne 'PASS' -or
        (Read-Key $out 'REASON') -ne 'CANONICAL_SOURCE_TRANSFORMATION_REPRODUCED') {
        Stop-P1A 'SOURCE_REGEN_OUTPUT_INVALID'
    }
    foreach ($check in @(
        [pscustomobject]@{Path=(Join-Path $TempRoot 'libraries\JWPLC_TFT\src\JWPLC_TFT.cpp');Sha=$ExpectedCpp},
        [pscustomobject]@{Path=(Join-Path $TempRoot 'libraries\JWPLC_TFT\src\tft_setup.h');Sha=$ExpectedSetup},
        [pscustomobject]@{Path=(Join-Path $TempRoot 'libraries\TFT_eSPI\TFT_Drivers\ST7789_Init.h');Sha=$ExpectedInit}
    )) {
        if (-not (Test-Path -LiteralPath $check.Path) -or (Get-A13Sha256 -Path $check.Path) -ne $check.Sha) {
            Stop-P1A ('OUTPUT_FILE_HASH_MISMATCH:'+$check.Path)
        }
    }
    if (-not (Test-Path -LiteralPath (Join-Path $TempRoot 'MANIFEST.json'))) {
        Stop-P1A 'OUTPUT_MANIFEST_MISSING'
    }

    $script:Phase = 'FINAL_AUDIT'
    Guard-Integrity
    Finish-P1A -Status 'PASS' -Reason 'CANONICAL_SOURCE_TRANSFORM_REPRODUCED' -Extra @(
        "HEAD=$head",
        "P0_SUMMARY=$($p0.Path)",
        "PRE4_SUMMARY=$($pre4.Path)",
        "TEMP_ROOT=$TempRoot",
        "CANDIDATE_JWPLC_TFT_CPP_SHA256=$ExpectedCpp",
        "CANDIDATE_TFT_SETUP_SHA256=$ExpectedSetup",
        "CANDIDATE_ST7789_INIT_SHA256=$ExpectedInit",
        'GLOBAL_TFT_ESPI_MUTATED=NO',
        'OFFICIAL_ARCHIVES_PRESERVED=YES',
        'WORKTREE_FINAL=CLEAN',
        'NEXT=TFT_PRE6_P1B_COMPILE_REGENERATED_SOURCES'
    )
}
catch {
    Finish-P1A -Status 'REVIEW' -Reason 'UNEXPECTED_GATE_EXCEPTION' -Harness 'YES' -ExitCode 90 -Extra @(
        "EXCEPTION_TYPE=$($_.Exception.GetType().FullName)",
        "EXCEPTION_LINE=$($_.InvocationInfo.ScriptLineNumber)",
        "EXCEPTION=$($_.Exception.Message)"
    )
}
finally { Pop-Location }
