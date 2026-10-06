param(
    [switch]$PreflightOnly,
    [switch]$Commit
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"
. (Join-Path $PSScriptRoot "common.ps1")

$expectedCoreRelative =
    "JWPLC/2.1.0/precompiled/core/JWPLCBASIC/core.a"

$expectedModbusRelative =
    "JWPLC/2.1.0/libraries/JWPLC_ModbusRTU/src/esp32/libJWPLC_ModbusRTU.a"

$expectedCoreSha =
    "4BFF8C8241DA2E8BD0E1BBA99835ADDF91B9085A05C4DFBD05C339B824794566"

$expectedModbusSha =
    "486BE38AE088B94898E516FFBC125855F22C2EC5EE8A6FA9E10F35D7CAC3A3BE"

$commitMessage =
    "build(alpha14): adoptar core y Modbus RTU precompilados cualificados"

function Get-H3EPromoteSha256 {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Path
    )

    $stream = [IO.File]::OpenRead($Path)

    try {
        $sha = [Security.Cryptography.SHA256]::Create()

        try {
            $hash = $sha.ComputeHash($stream)
        }
        finally {
            $sha.Dispose()
        }
    }
    finally {
        $stream.Dispose()
    }

    return ([BitConverter]::ToString($hash) -replace "-", "").ToUpperInvariant()
}

function Get-H3EPromoteDirty {
    [string[]]$paths = @(
        & git -C $script:G2RepoRoot diff --name-only
    )

    if ($LASTEXITCODE -ne 0) {
        throw "H3E_PROMOTE_GIT_DIRTY_QUERY_FAILED"
    }

    return @(
        $paths |
            Where-Object { -not [string]::IsNullOrWhiteSpace($_) } |
            ForEach-Object { $_.Replace("\", "/") } |
            Sort-Object
    )
}

function Get-H3EPromoteStaged {
    [string[]]$paths = @(
        & git -C $script:G2RepoRoot diff --cached --name-only
    )

    if ($LASTEXITCODE -ne 0) {
        throw "H3E_PROMOTE_GIT_STAGED_QUERY_FAILED"
    }

    return @(
        $paths |
            Where-Object { -not [string]::IsNullOrWhiteSpace($_) } |
            ForEach-Object { $_.Replace("\", "/") } |
            Sort-Object
    )
}

function Assert-H3EPromoteExactSet {
    param(
        [Parameter(Mandatory = $true)]
        [string[]]$Actual,

        [Parameter(Mandatory = $true)]
        [string[]]$Expected,

        [Parameter(Mandatory = $true)]
        [string]$Label
    )

    [object[]]$diff = @(
        Compare-Object -ReferenceObject $Expected -DifferenceObject $Actual
    )

    Write-Host ("{0}_COUNT={1}" -f $Label, $Actual.Count)

    foreach ($path in $Actual) {
        Write-Host ("{0}={1}" -f $Label, $path)
    }

    if ($diff.Count -ne 0) {
        throw ("{0}_SCOPE_INVALID" -f $Label)
    }
}

Write-Host "============================================================"
Write-Host " A14 H3E - PROMOTE QUALIFIED PRECOMPILED ARTIFACTS"
Write-Host " EXACT SHA + EXACT DIRTY SCOPE + CONTROLLED COMMIT"
Write-Host "============================================================"

Assert-G2Branch

$corePath = Get-G2Path $expectedCoreRelative
$modbusPath = Get-G2Path $expectedModbusRelative

foreach ($required in @($corePath, $modbusPath)) {
    if (-not (Test-Path -LiteralPath $required -PathType Leaf)) {
        throw "H3E_PROMOTE_REQUIRED_ARTIFACT_MISSING=$required"
    }
}

$coreSha = Get-H3EPromoteSha256 -Path $corePath
$modbusSha = Get-H3EPromoteSha256 -Path $modbusPath

Write-Host "H3E_PROMOTE_HEAD=$(Get-G2Head)"
Write-Host "H3E_PROMOTE_CORE_SHA256=$coreSha"
Write-Host "H3E_PROMOTE_MODBUS_RTU_SHA256=$modbusSha"

if ($coreSha -ne $expectedCoreSha) {
    throw "H3E_PROMOTE_CORE_SHA_MISMATCH"
}

if ($modbusSha -ne $expectedModbusSha) {
    throw "H3E_PROMOTE_MODBUS_RTU_SHA_MISMATCH"
}

[string[]]$expectedDirty = @(
    $expectedCoreRelative,
    $expectedModbusRelative
) | Sort-Object

[string[]]$dirty = @(Get-H3EPromoteDirty)
[string[]]$staged = @(Get-H3EPromoteStaged)

Assert-H3EPromoteExactSet -Actual $dirty -Expected $expectedDirty -Label "H3E_PROMOTE_DIRTY"

Write-Host "H3E_PROMOTE_STAGED_COUNT=$($staged.Count)"

if ($staged.Count -ne 0) {
    foreach ($path in $staged) {
        Write-Host "H3E_PROMOTE_STAGED=$path"
    }

    throw "H3E_PROMOTE_INDEX_NOT_CLEAN"
}

Write-Host "H3E_PROMOTE_ARTIFACT_IDENTITY=PASS"
Write-Host "H3E_PROMOTE_DIRTY_SCOPE=PASS"
Write-Host "H3E_PROMOTE_INDEX=PASS"

if ($PreflightOnly) {
    Write-Host "H3E_PROMOTE_PREFLIGHT_STAGES=NO"
    Write-Host "H3E_PROMOTE_PREFLIGHT_COMMITS=NO"
    Write-Host "H3E_PROMOTE_PREFLIGHT_PUSHES=NO"
    Write-Host "A14_H3E_PROMOTE_QUALIFIED_ARTIFACTS_PREFLIGHT=PASS"
    return
}

if (-not $Commit) {
    throw "H3E_PROMOTE_COMMIT_SWITCH_REQUIRED"
}

& git -C $script:G2RepoRoot add -- $expectedCoreRelative $expectedModbusRelative

if ($LASTEXITCODE -ne 0) {
    throw "H3E_PROMOTE_GIT_ADD_FAILED"
}

[string[]]$postStageDirty = @(Get-H3EPromoteDirty)
[string[]]$postStage = @(Get-H3EPromoteStaged)

Write-Host "H3E_PROMOTE_POST_STAGE_UNSTAGED_COUNT=$($postStageDirty.Count)"

Assert-H3EPromoteExactSet -Actual $postStage -Expected $expectedDirty -Label "H3E_PROMOTE_POST_STAGE"

if ($postStageDirty.Count -ne 0) {
    foreach ($path in $postStageDirty) {
        Write-Host "H3E_PROMOTE_POST_STAGE_UNSTAGED=$path"
    }

    & git -C $script:G2RepoRoot reset HEAD -- $expectedCoreRelative $expectedModbusRelative | Out-Null
    throw "H3E_PROMOTE_UNSTAGED_REMAINS_AFTER_STAGE"
}

Write-Host "H3E_PROMOTE_STAGE=PASS"

& git -C $script:G2RepoRoot commit -m $commitMessage

if ($LASTEXITCODE -ne 0) {
    throw "H3E_PROMOTE_COMMIT_FAILED"
}

$commitSha = Get-G2Head
[string[]]$finalDirty = @(Get-H3EPromoteDirty)
[string[]]$finalStaged = @(Get-H3EPromoteStaged)

Write-Host "H3E_PROMOTE_COMMIT_SHA=$commitSha"
Write-Host "H3E_PROMOTE_FINAL_DIRTY_COUNT=$($finalDirty.Count)"
Write-Host "H3E_PROMOTE_FINAL_STAGED_COUNT=$($finalStaged.Count)"

if ($finalDirty.Count -ne 0 -or $finalStaged.Count -ne 0) {
    throw "H3E_PROMOTE_POST_COMMIT_WORKTREE_NOT_CLEAN"
}

Write-Host "H3E_PROMOTE_COMMIT=PASS"
Write-Host "H3E_PROMOTE_PUSH=NOT_EXECUTED"
Write-Host "A14_H3E_PROMOTE_QUALIFIED_ARTIFACTS=PASS"
Write-Host "NEXT=PUSH_COMMIT_THEN_RETURN_OUTPUT_FOR_H3E_DOCUMENTAL_CLOSURE"
