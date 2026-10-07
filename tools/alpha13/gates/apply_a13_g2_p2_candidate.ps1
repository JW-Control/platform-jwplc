param()

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$ExpectedBranch = 'v2.1.0-alpha.13/feature/cleanup-robustness'
$RepoRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..\..'))
$PatchPath = Join-Path $PSScriptRoot '..\candidates\a13_g2_p2_tca_startup.patch'
$CorePath = Join-Path $RepoRoot 'JWPLC\2.1.0\precompiled\core\JWPLCBASIC\core.a'

$ExpectedBaseline = @{
    'JWPLC/2.1.0/cores/jwcontrol/peripherals_init.cpp' = '9ab559459bee69917f78297dd496db346c0bf27c'
    'JWPLC/2.1.0/cores/jwcontrol/jwplc_peripherals.cpp' = '771cf1f5ad5e2444eacbe1afe2cd6a20364beed4'
    'JWPLC/2.1.0/cores/jwcontrol/jwplc_peripherals.h' = '9b0cd29fc235be96af771dd6bbdebfb7c3206225'
}

$ExpectedCandidate = @{
    'JWPLC/2.1.0/cores/jwcontrol/peripherals_init.cpp' = '23efb3935a34e6b5649875b57c804e60538827cd'
    'JWPLC/2.1.0/cores/jwcontrol/jwplc_peripherals.cpp' = '875a50fd64552e8c4a4300e07b9494d2c32605d7'
    'JWPLC/2.1.0/cores/jwcontrol/jwplc_peripherals.h' = '288667f1caa08142e2a155b8c85f24b2aa5beb44'
}

$ExpectedCoreSha256 = '78d0c0ab14f156b96116529e88872340d51877af40d24ba3559f6081e0bf34fb'

function Get-Blob([string]$RelativePath)
{
    $value = @(& git -C $RepoRoot hash-object -- $RelativePath 2>&1)
    if ($LASTEXITCODE -ne 0 -or $value.Count -ne 1)
    {
        throw "git hash-object fallo para $RelativePath"
    }
    return ([string]$value[0]).Trim()
}

function Get-Sha256([string]$Path)
{
    return (Get-FileHash -Algorithm SHA256 -LiteralPath $Path).Hash.ToLowerInvariant()
}

Push-Location $RepoRoot
try
{
    $branch = (git branch --show-current).Trim()
    if ($branch -ne $ExpectedBranch)
    {
        throw "UNEXPECTED_BRANCH=$branch"
    }

    $dirty = @(git diff --name-only | Where-Object { $_ })
    $staged = @(git diff --cached --name-only | Where-Object { $_ })

    if ($dirty.Count -ne 0 -or $staged.Count -ne 0)
    {
        throw "WORKTREE_NOT_CLEAN dirty=$($dirty.Count) staged=$($staged.Count)"
    }

    foreach ($relative in $ExpectedBaseline.Keys)
    {
        $actual = Get-Blob $relative
        if ($actual -ne $ExpectedBaseline[$relative])
        {
            throw "BASELINE_BLOB_MISMATCH $relative actual=$actual expected=$($ExpectedBaseline[$relative])"
        }
    }

    $coreSha = Get-Sha256 $CorePath
    if ($coreSha -ne $ExpectedCoreSha256)
    {
        throw "CORE_ARCHIVE_BASELINE_MISMATCH actual=$coreSha expected=$ExpectedCoreSha256"
    }

    & git apply --check -- $PatchPath
    if ($LASTEXITCODE -ne 0)
    {
        throw 'GIT_APPLY_CHECK_FAILED'
    }

    & git apply -- $PatchPath
    if ($LASTEXITCODE -ne 0)
    {
        throw 'GIT_APPLY_FAILED'
    }

    foreach ($relative in $ExpectedCandidate.Keys)
    {
        $actual = Get-Blob $relative
        if ($actual -ne $ExpectedCandidate[$relative])
        {
            throw "CANDIDATE_BLOB_MISMATCH $relative actual=$actual expected=$($ExpectedCandidate[$relative])"
        }
    }

    & git diff --check
    if ($LASTEXITCODE -ne 0)
    {
        throw 'GIT_DIFF_CHECK_FAILED'
    }

    $dirtyAfter = @(git diff --name-only | Where-Object { $_ } | Sort-Object)
    $stagedAfter = @(git diff --cached --name-only | Where-Object { $_ })
    $expectedDirty = @(
        'JWPLC/2.1.0/cores/jwcontrol/jwplc_peripherals.cpp',
        'JWPLC/2.1.0/cores/jwcontrol/jwplc_peripherals.h',
        'JWPLC/2.1.0/cores/jwcontrol/peripherals_init.cpp'
    ) | Sort-Object

    $scopeDiff = @(Compare-Object -ReferenceObject $expectedDirty -DifferenceObject $dirtyAfter)
    if ($scopeDiff.Count -ne 0 -or $stagedAfter.Count -ne 0)
    {
        throw "CANDIDATE_SCOPE_INVALID dirty=$($dirtyAfter -join ';') staged=$($stagedAfter -join ';')"
    }

    Write-Host 'A13_G2_CANDIDATE_APPLY=PASS'
    Write-Host "CORE_SHA256=$coreSha"
    Write-Host "DIRTY_COUNT=$($dirtyAfter.Count)"
    $dirtyAfter | ForEach-Object { Write-Host "DIRTY_FILE=$_" }
    Write-Host 'STAGED_COUNT=0'
    Write-Host 'GIT_DIFF_CHECK=PASS'
    Write-Host 'DO_NOT_COMMIT=YES'
}
catch
{
    Write-Host 'A13_G2_CANDIDATE_APPLY=FAIL'
    Write-Host "ERROR=$($_.Exception.Message)"
    $restore = @(
        'JWPLC/2.1.0/cores/jwcontrol/peripherals_init.cpp',
        'JWPLC/2.1.0/cores/jwcontrol/jwplc_peripherals.cpp',
        'JWPLC/2.1.0/cores/jwcontrol/jwplc_peripherals.h'
    )
    & git checkout -- $restore
    exit 1
}
finally
{
    Pop-Location
}
