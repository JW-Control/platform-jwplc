param()

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$ExpectedBranch = 'v2.1.0-alpha.13/feature/cleanup-robustness'
$RepoRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..\..'))
$CommonPath = Join-Path $PSScriptRoot 'common.ps1'
$CorePath = Join-Path $RepoRoot 'JWPLC\2.1.0\precompiled\core\JWPLCBASIC\core.a'

$InitRelative = 'JWPLC/2.1.0/cores/jwcontrol/peripherals_init.cpp'
$PerRelative = 'JWPLC/2.1.0/cores/jwcontrol/jwplc_peripherals.cpp'
$HdrRelative = 'JWPLC/2.1.0/cores/jwcontrol/jwplc_peripherals.h'

$ExpectedBaseline = @{
    $InitRelative = '9ab559459bee69917f78297dd496db346c0bf27c'
    $PerRelative  = '771cf1f5ad5e2444eacbe1afe2cd6a20364beed4'
    $HdrRelative  = '9b0cd29fc235be96af771dd6bbdebfb7c3206225'
}

$ExpectedCandidate = @{
    $InitRelative = '23efb3935a34e6b5649875b57c804e60538827cd'
    $PerRelative  = 'c3d53566d274e95b7dda111327140b5393f7db35'
    $HdrRelative  = '288667f1caa08142e2a155b8c85f24b2aa5beb44'
}

$ExpectedCoreSha256 = '78d0c0ab14f156b96116529e88872340d51877af40d24ba3559f6081e0bf34fb'

if (-not (Test-Path -LiteralPath $CommonPath))
{
    throw "COMMON_PS1_NOT_FOUND=$CommonPath"
}

. $CommonPath

function Get-Blob
{
    param([Parameter(Mandatory = $true)][string]$RelativePath)

    $value = @(& git -C $RepoRoot hash-object -- $RelativePath 2>&1)
    if ($LASTEXITCODE -ne 0 -or $value.Count -ne 1)
    {
        throw "git hash-object fallo para $RelativePath"
    }

    return ([string]$value[0]).Trim()
}

function Get-RawBlob
{
    param([Parameter(Mandatory = $true)][string]$Path)

    $value = @(& git -C $RepoRoot hash-object --no-filters -- $Path 2>&1)
    if ($LASTEXITCODE -ne 0 -or $value.Count -ne 1)
    {
        throw "git hash-object fallo para $Path"
    }

    return ([string]$value[0]).Trim()
}

function Read-NormalizedUtf8
{
    param([Parameter(Mandatory = $true)][string]$Path)

    $lf = [string][char]10
    $crlf = ([string][char]13) + $lf
    $text = [Text.Encoding]::UTF8.GetString([IO.File]::ReadAllBytes($Path))
    return $text.Replace($crlf, $lf)
}

function Write-Utf8Lf
{
    param(
        [Parameter(Mandatory = $true)][string]$Path,
        [Parameter(Mandatory = $true)][string]$Text
    )

    $lf = [string][char]10
    $crlf = ([string][char]13) + $lf
    $utf8NoBom = New-Object Text.UTF8Encoding($false)
    [IO.File]::WriteAllText($Path, $Text.Replace($crlf, $lf), $utf8NoBom)
}

function Replace-ExactOnce
{
    param(
        [Parameter(Mandatory = $true)][string]$Text,
        [Parameter(Mandatory = $true)][string]$Old,
        [Parameter(Mandatory = $true)][string]$New,
        [Parameter(Mandatory = $true)][string]$Label
    )

    $count = @([regex]::Matches($Text, [regex]::Escape($Old))).Count
    if ($count -ne 1)
    {
        throw "$Label expected exactly once, got $count"
    }

    return $Text.Replace($Old, $New)
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
        $actual = Get-Blob -RelativePath $relative
        if ($actual -ne $ExpectedBaseline[$relative])
        {
            throw "BASELINE_BLOB_MISMATCH $relative actual=$actual expected=$($ExpectedBaseline[$relative])"
        }
    }

    $coreSha = Get-A13Sha256 -Path $CorePath
    if ($coreSha -ne $ExpectedCoreSha256)
    {
        throw "CORE_ARCHIVE_BASELINE_MISMATCH actual=$coreSha expected=$ExpectedCoreSha256"
    }

    $initPath = Join-Path $RepoRoot $InitRelative
    $perPath = Join-Path $RepoRoot $PerRelative
    $hdrPath = Join-Path $RepoRoot $HdrRelative
    $lf = [string][char]10

    $init = Read-NormalizedUtf8 -Path $initPath
    $per = Read-NormalizedUtf8 -Path $perPath
    $hdr = Read-NormalizedUtf8 -Path $hdrPath

    $initOld = @(
        '    jwplcSystemClearOutputShadow();',
        '',
        '    (void)TCA6424A_writeBank(TCA6424A_DEFAULT_ADDRESS, 1, 0x00);',
        '    (void)TCA6424A_writeBank(TCA6424A_DEFAULT_ADDRESS, 2, 0x00);',
        '',
        '    (void)TCA6424A_setBankDirection(TCA6424A_DEFAULT_ADDRESS, 0, 0xFF);',
        '    (void)TCA6424A_setBankDirection(TCA6424A_DEFAULT_ADDRESS, 1, 0x00);',
        '    (void)TCA6424A_setBankDirection(TCA6424A_DEFAULT_ADDRESS, 2, 0xFF);',
        '',
        '    gpio_set_level((gpio_num_t)EN_IO, 1);',
        '    vTaskDelay(pdMS_TO_TICKS(2));',
        '',
        '    g_jwplc_peripherals_initialized = true;',
        '#else',
        '    jwplcSystemInitState();',
        '    g_jwplc_peripherals_initialized = true;'
    ) -join $lf

    $initNew = @(
        '    jwplcSystemClearOutputShadow();',
        '',
        '    if (!TCA6424A_writeBank(TCA6424A_DEFAULT_ADDRESS, 1, 0x00) ||',
        '        !TCA6424A_writeBank(TCA6424A_DEFAULT_ADDRESS, 2, 0x00) ||',
        '        !TCA6424A_setBankDirection(TCA6424A_DEFAULT_ADDRESS, 0, 0xFF) ||',
        '        !TCA6424A_setBankDirection(TCA6424A_DEFAULT_ADDRESS, 1, 0x00) ||',
        '        !TCA6424A_setBankDirection(TCA6424A_DEFAULT_ADDRESS, 2, 0xFF))',
        '    {',
        '        return;',
        '    }',
        '',
        '    gpio_set_level((gpio_num_t)EN_IO, 1);',
        '    vTaskDelay(pdMS_TO_TICKS(2));',
        '',
        '    g_jwplc_peripherals_initialized = true;',
        '    jwplcSystemSetIOReady(true);',
        '#else',
        '    jwplcSystemInitState();',
        '    g_jwplc_peripherals_initialized = true;',
        '    jwplcSystemSetIOReady(true);'
    ) -join $lf

    $init = Replace-ExactOnce -Text $init -Old $initOld -New $initNew -Label 'INIT_CONFIG_BLOCK'

    $perOldReady = '    g_ioState.initialized = true;'
    $perNewReady = @(
        '    // El snapshot existe desde este punto, pero las E/S todavia no estan',
        '    // listas hasta que initPeripherals() complete I2C/TCA y habilite EN_IO.',
        '    g_ioState.initialized = false;'
    ) -join $lf
    $per = Replace-ExactOnce -Text $per -Old $perOldReady -New $perNewReady -Label 'IO_INITIALIZED_STATE'

    $perOldSetter = 'void jwplcSystemSetOutputShadow(uint8_t bank1, uint8_t bank2)'
    $perNewSetter = @(
        'void jwplcSystemSetIOReady(bool ready)',
        '{',
        '    g_ioState.initialized = ready;',
        '}',
        '',
        'void jwplcSystemSetOutputShadow(uint8_t bank1, uint8_t bank2)'
    ) -join $lf
    $per = Replace-ExactOnce -Text $per -Old $perOldSetter -New $perNewSetter -Label 'IO_READY_SETTER'

    $hdrOld = @(
        'void jwplcSystemInitState(void);',
        'void jwplcSystemScanIO(void);'
    ) -join $lf
    $hdrNew = @(
        'void jwplcSystemInitState(void);',
        'void jwplcSystemSetIOReady(bool ready);',
        'void jwplcSystemScanIO(void);'
    ) -join $lf
    $hdr = Replace-ExactOnce -Text $hdr -Old $hdrOld -New $hdrNew -Label 'IO_READY_DECLARATION'

    $tempRoot = Join-Path $env:TEMP (
        'jwplc_a13_g2_p2_candidate_' + [Guid]::NewGuid().ToString('N')
    )
    New-Item -ItemType Directory -Path $tempRoot -Force | Out-Null

    try
    {
        $tempInit = Join-Path $tempRoot 'peripherals_init.cpp'
        $tempPer = Join-Path $tempRoot 'jwplc_peripherals.cpp'
        $tempHdr = Join-Path $tempRoot 'jwplc_peripherals.h'

        Write-Utf8Lf -Path $tempInit -Text $init
        Write-Utf8Lf -Path $tempPer -Text $per
        Write-Utf8Lf -Path $tempHdr -Text $hdr

        $tempCandidates = @(
            [PSCustomObject]@{ Name = $InitRelative; Path = $tempInit; Expected = $ExpectedCandidate[$InitRelative] },
            [PSCustomObject]@{ Name = $PerRelative; Path = $tempPer; Expected = $ExpectedCandidate[$PerRelative] },
            [PSCustomObject]@{ Name = $HdrRelative; Path = $tempHdr; Expected = $ExpectedCandidate[$HdrRelative] }
        )

        foreach ($candidate in $tempCandidates)
        {
            $tempBlob = Get-RawBlob -Path $candidate.Path
            if ($tempBlob -ne $candidate.Expected)
            {
                throw "PREAPPLY_CANDIDATE_BLOB_MISMATCH $($candidate.Name) actual=$tempBlob expected=$($candidate.Expected)"
            }
        }

        [IO.File]::WriteAllBytes($initPath, [IO.File]::ReadAllBytes($tempInit))
        [IO.File]::WriteAllBytes($perPath, [IO.File]::ReadAllBytes($tempPer))
        [IO.File]::WriteAllBytes($hdrPath, [IO.File]::ReadAllBytes($tempHdr))
    }
    finally
    {
        if (Test-Path -LiteralPath $tempRoot)
        {
            Remove-Item -LiteralPath $tempRoot -Recurse -Force
        }
    }

    foreach ($relative in $ExpectedCandidate.Keys)
    {
        $actual = Get-Blob -RelativePath $relative
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
        $HdrRelative,
        $PerRelative,
        $InitRelative
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
        $InitRelative,
        $PerRelative,
        $HdrRelative
    )
    & git checkout -- $restore
    exit 1
}
finally
{
    Pop-Location
}
