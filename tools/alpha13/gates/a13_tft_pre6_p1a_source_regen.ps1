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
$P0ProofPath = Join-Path $RepoRoot 'docs\v2.1.0-alpha.13\A13_TFT_PRE6_P0_CLOSURE_20261009.md'
$Pre4ProofPath = Join-Path $RepoRoot 'docs\v2.1.0-alpha.13\A13_TFT_PRE4_CANDIDATE_20261009.md'
$Pre3ProofPath = Join-Path $RepoRoot 'docs\v2.1.0-alpha.13\A13_TFT_PRE3_SOURCE_ATTRIBUTION_20261009.md'

$ExpectedP0 = 'ff9dd89cb267bc270d2ca6fa2d0f1362b76595b8dc6b49f764d05a8e28bb6705'
$ExpectedCpp = '494440b00e74e74a7b23975420574e035ef2138fdf5a0cea2fed51c986c29d25'
$ExpectedSetup = '8fa079444ca130772d3a642eaf814035100bc098032b403c922c458d351ae5e1'
$ExpectedInit = '44873be82fe836084934a328df77f098e9ab88d212dd1d570da5e8aac74671bc'
$ExpectedOldTft = '5d860a131811dd9a7eb6fa55f5674b1d78b0de7dfaf8748ce18a60ceed2d3738'
$ExpectedOldCore = '6f328eeb796091070c8d852a2c0e90f71047d8d2b5786fe3cd5079b2fb6ff983'
$ExpectedOldDisplay = 'c960d718433e29a40e3cc55bc745c9a2e121ee1ee598ec72c04872327592dc02'
$ExpectedBranch = 'v2.1.0-alpha.13/feature/cleanup-robustness'
$ExpectedP0ProofBlob = '15a33a9bb686fdfb4b22277e792fd194669c1010'
$ExpectedPre4ProofBlob = 'f9f9b2b683d7fcd85644e50a3a55f8bf54f1a673'
$ExpectedPre3ProofBlob = '53f636a7c90a38929402e5696c89fe9ffd8277cd'
$ExpectedP0Bytes = 1091942
$ExpectedP0Head = '89e461123474310e8528e4daf50c97262a86390b'
$ExpectedBackendVersion = '2.5.43'
$ExpectedBackendCppSha = '01ed6edb0530d38b94ddeac079ba81633aa21d77d049b12da21a37f4bec69ee1'
$ExpectedBackendHeaderSha = 'b1b2789ace7ac8fd4a4c414054757d91e6e62649a23d74226ea28ceb8d6f4462'
$ExpectedBackendInitSha = 'e21cae2ac84285dc0e77648eca67ca753f41f7da3c271594ede750b725136c10'

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
function Verified-Document {
    param(
        [Parameter(Mandatory = $true)][string]$Path,
        [Parameter(Mandatory = $true)][string]$BlobSha,
        [Parameter(Mandatory = $true)][string]$Gate,
        [Parameter(Mandatory = $true)][string]$Reason
    )
    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) {
        Stop-P1A "VERSIONED_PROOF_MISSING:$Gate"
    }
    $blob = @(& git -C $RepoRoot hash-object -- $Path)
    if ($LASTEXITCODE -ne 0 -or $blob.Count -ne 1 -or $blob[0].Trim() -ne $BlobSha) {
        Stop-P1A "VERSIONED_PROOF_BLOB_MISMATCH:$Gate"
    }
    $body = [IO.File]::ReadAllText($Path)
    if ((Read-Key -Text $body -Key 'GATE') -cne $Gate -or
        (Read-Key -Text $body -Key 'STATUS') -cne 'PASS' -or
        (Read-Key -Text $body -Key 'REASON') -cne $Reason) {
        Stop-P1A "VERSIONED_PROOF_CONTENT_INVALID:$Gate"
    }
    Write-Host "VERSIONED_PROOF_GATE=$Gate"
    Write-Host "VERSIONED_PROOF_BLOB=$($blob[0].Trim())"
    Write-Host "VERSIONED_PROOF_PATH=$Path"
    return $body
}
function Require-ProofField {
    param([string]$Proof,[string]$Key,[string]$Expected)
    $actual = Read-Key -Text $Proof -Key $Key
    if ($actual -cne $Expected) {
        Stop-P1A "VERSIONED_PROOF_VALUE_MISMATCH:$Key actual=$actual"
    }
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

    $script:Phase = 'P0_DURABLE_PROVENANCE'
    Assert-ProofParser
    $p0Text = Verified-Document -Path $P0ProofPath -BlobSha $ExpectedP0ProofBlob -Gate 'A13-TFT-PRE6-P0' -Reason 'TEMP_PRECOMPILED_ARCHIVE_QUALIFIED'
    Require-ProofField -Proof $p0Text -Key 'HEAD' -Expected $ExpectedP0Head
    Require-ProofField -Proof $p0Text -Key 'CANDIDATE_ARCHIVE_SHA256' -Expected $ExpectedP0
    Require-ProofField -Proof $p0Text -Key 'CANDIDATE_ARCHIVE_BYTES' -Expected ([string]$ExpectedP0Bytes)
    Require-ProofField -Proof $p0Text -Key 'ARCHIVE_MEMBER_COUNT' -Expected '2'
    Require-ProofField -Proof $p0Text -Key 'ARCHIVE_MEMBER_PARITY' -Expected 'PASS'
    Require-ProofField -Proof $p0Text -Key 'WORKTREE_FINAL' -Expected 'CLEAN'
    foreach ($caseName in @('DIRECT_TFT','DISPLAY_INTEGRATION','NORMAL_AUTOLOAD')) {
        $casePattern = '(?m)^\|\s*' + $caseName + '\s*\|\s*0\s*\|\s*seleccionado\s*\|\s*0\s*\|\s*0\s*\|\s*no seleccionada\s*\|[ \t]*\r?$'
        if (-not [regex]::IsMatch($p0Text,$casePattern)) {
            Stop-P1A "VERSIONED_P0_CASE_NOT_PROVEN:$caseName"
        }
    }
    foreach ($key in @('TFT_PRECOMPILED','STUB_CORE','CORE_ARCHIVE_LINKED')) {
        Require-ProofField -Proof $p0Text -Key $key -Expected 'True'
    }

    # Never synthesize the missing historical SUMMARY.log. Validate the
    # actual binary P0 reported against the signed-off versioned evidence.
    $p0Name = 'jwplc_a13_tft_pre6_p0_20261009_123147'
    $p0Archive = Join-Path $env:TEMP ($p0Name + '\libraries\JWPLC_TFT\src\esp32\libJWPLC_TFT.a')
    if (-not (Test-Path -LiteralPath $p0Archive -PathType Leaf)) {
        Stop-P1A "P0_ARCHIVE_NOT_FOUND:$p0Archive"
    }
    $p0Bytes = (Get-Item -LiteralPath $p0Archive).Length
    $p0Sha = Get-A13Sha256 -Path $p0Archive
    Write-Host "P0_DURABLE_PROOF_SOURCE=$P0ProofPath"
    Write-Host "P0_HISTORICAL_SUMMARY=NOT_REQUIRED_MISSING"
    Write-Host "P0_ARCHIVE_PATH=$p0Archive"
    Write-Host "P0_ARCHIVE_BYTES=$p0Bytes"
    Write-Host "P0_ARCHIVE_SHA256=$p0Sha"
    if ($p0Bytes -ne $ExpectedP0Bytes -or $p0Sha -cne $ExpectedP0) {
        Stop-P1A 'P0_ARCHIVE_IDENTITY_MISMATCH'
    }
    Write-Host 'P0_HANDOFF_RECOVERED=YES'
    Write-Host 'P0_PROOF_SOURCE=VERSIONED_CLOSURE_AND_ARCHIVE_SHA256'

    $script:Phase = 'PRE4_PRE3_DURABLE_PROVENANCE'
    $pre4Text = Verified-Document -Path $Pre4ProofPath -BlobSha $ExpectedPre4ProofBlob -Gate 'A13-TFT-PRE4' -Reason 'TEMP_DEFERRED_DISPON_CANDIDATE_READY'
    Require-ProofField -Proof $pre4Text -Key 'CANDIDATE_JWPLC_TFT_CPP_SHA256' -Expected $ExpectedCpp
    Require-ProofField -Proof $pre4Text -Key 'CANDIDATE_TFT_SETUP_SHA256' -Expected $ExpectedSetup
    Require-ProofField -Proof $pre4Text -Key 'CANDIDATE_ST7789_INIT_SHA256' -Expected $ExpectedInit
    Require-ProofField -Proof $pre4Text -Key 'BACKEND_ORIGINAL_MUTATED' -Expected 'NO'
    Require-ProofField -Proof $pre4Text -Key 'REPO_PRODUCT_MUTATED' -Expected 'NO'

    $pre3Text = Verified-Document -Path $Pre3ProofPath -BlobSha $ExpectedPre3ProofBlob -Gate 'A13-TFT-PRE3' -Reason 'TFT_ESPI_2543_SOURCE_ATTRIBUTED'
    Require-ProofField -Proof $pre3Text -Key 'TFT_ESPI_SELECTED_VERSION' -Expected $ExpectedBackendVersion
    Require-ProofField -Proof $pre3Text -Key 'TFT_ESPI_CPP_SHA256' -Expected $ExpectedBackendCppSha
    Require-ProofField -Proof $pre3Text -Key 'TFT_ESPI_HEADER_SHA256' -Expected $ExpectedBackendHeaderSha
    Require-ProofField -Proof $pre3Text -Key 'ST7789_INIT_SHA256' -Expected $ExpectedBackendInitSha
    $backend = Read-Key -Text $pre3Text -Key 'TFT_ESPI_SELECTED_ROOT'
    if ([string]::IsNullOrWhiteSpace($backend) -or
        -not (Test-Path -LiteralPath $backend -PathType Container)) {
        Stop-P1A 'TFT_ESPI_SOURCE_ROOT_NOT_FOUND'
    }
    foreach ($srcCheck in @(
        [pscustomobject]@{Path=(Join-Path $backend 'TFT_eSPI.cpp');Sha=$ExpectedBackendCppSha},
        [pscustomobject]@{Path=(Join-Path $backend 'TFT_eSPI.h');Sha=$ExpectedBackendHeaderSha},
        [pscustomobject]@{Path=(Join-Path $backend 'TFT_Drivers\ST7789_Init.h');Sha=$ExpectedBackendInitSha}
    )) {
        if (-not (Test-Path -LiteralPath $srcCheck.Path -PathType Leaf) -or
            (Get-A13Sha256 -Path $srcCheck.Path) -cne $srcCheck.Sha) {
            Stop-P1A "TFT_ESPI_BACKEND_SHA_MISMATCH:$($srcCheck.Path)"
        }
    }
    Write-Host "PRE4_PROOF_SOURCE=$Pre4ProofPath"
    Write-Host "PRE3_PROOF_SOURCE=$Pre3ProofPath"
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
        "P0_PROOF_SOURCE=$P0ProofPath",
        "PRE4_PROOF_SOURCE=$Pre4ProofPath",
        "PRE3_PROOF_SOURCE=$Pre3ProofPath",
        "P0_ARCHIVE_SHA256=$p0Sha",
        "P0_ARCHIVE_BYTES=$p0Bytes",
        'P0_HISTORICAL_SUMMARY=NOT_REQUIRED_MISSING',
        'P0_HANDOFF_RECOVERED=YES',
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
