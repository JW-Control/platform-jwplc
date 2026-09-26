param(
    [switch]$Commit
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"
. (Join-Path $PSScriptRoot "common.ps1")

$expectedHashes = [ordered]@{
    "JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/Dns.cpp" = "5D67E2FE8F2333A9A8AAD2A290A26DBAE92761DF11661C4305F8EC9D09604A60"
    "JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/Dns.h" = "B92718E94D549D60A7F2E648DAAD2A18DF4FAB94936BFEB279025911949AAFED"
    "JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/EthernetClient.cpp" = "6ADD2904893DA8C3A61C19FC91BF146C40038834458BC9F3084339462C9A9770"
    "JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/EthernetUdp.cpp" = "F5AE79FA9E9642A670D0AF23E029621711D2DD857B776A5208CF2866EDB496E5"
    "JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/jwplc_ethernet_async_tx.cpp" = "CC8EEE779FC932C5A8FA0175ABBF0421AA8C95279B4260FADAC1276E7BD74E1C"
    "JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/jwplc_ethernet_async_tx.h" = "C1C5615DFF167F3572FBEA85CFFE1A7F5051732E2BD446387F1025A1D981FB47"
    "JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/JWPLC_W5x00_Ethernet.h" = "7A5923105CFEEF07CD4BACEB72854397B9E9389772F97DF6B9DF3AB5A4D0F15A"
    "JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/socket.cpp" = "6E55F494F5E43B6BCF327F40C268AB6FF8F739331C96C328E9A0BEC0DA38654A"
    "JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/utility/w5100.cpp" = "9F94AAC1BB25966C18EDFD6A5C5D5908A9DBD4CF11B6E9BC099E10B28CEF272F"
    "JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/utility/w5100.h" = "9A833532C44E0CFCD66429A764E8BDBF62A8871838B0355565BC0A63043455FC"
    "tools/modbus-tcp-benchmark/firmware/eth14_raw_transport_server/eth14_raw_transport_server.ino" = "9A0C6CF27E44A61DC97686FB1A769EBBBB34D036CDF8D0CA0EEAB597E699AB6B"
}

$expectedPaths = @($expectedHashes.Keys) | Sort-Object

function Get-NB3HStagedPaths {
    $paths = @(& git -C $script:G2RepoRoot diff --cached --name-only)
    if ($LASTEXITCODE -ne 0) {
        throw "A14_NB3H_GIT_DIFF_CACHED_NAME_ONLY_FAILED"
    }

    return @(
        $paths |
            Where-Object { -not [string]::IsNullOrWhiteSpace($_) } |
            Sort-Object -Unique
    )
}

function Get-NB3HUnstagedPaths {
    $paths = @(& git -C $script:G2RepoRoot diff --name-only)
    if ($LASTEXITCODE -ne 0) {
        throw "A14_NB3H_GIT_DIFF_NAME_ONLY_FAILED"
    }

    return @(
        $paths |
            Where-Object { -not [string]::IsNullOrWhiteSpace($_) } |
            Sort-Object -Unique
    )
}

function Assert-NB3HExactPaths {
    param(
        [Parameter(Mandatory = $true)][string[]]$Actual,
        [Parameter(Mandatory = $true)][string]$Label
    )

    Write-Host ("{0}_COUNT={1}" -f $Label, $Actual.Count)
    foreach ($item in $Actual) {
        Write-Host ("{0}={1}" -f $Label, $item)
    }

    if ($Actual.Count -ne $expectedPaths.Count) {
        throw ("A14_NB3H_{0}_COUNT_INVALID={1}" -f $Label, $Actual.Count)
    }

    for ($i = 0; $i -lt $expectedPaths.Count; ++$i) {
        if ($Actual[$i] -ne $expectedPaths[$i]) {
            throw ("A14_NB3H_{0}_PATH_INVALID={1}" -f $Label, $Actual[$i])
        }
    }
}

function Invoke-NB3HGitCheck {
    param(
        [Parameter(Mandatory = $true)][string[]]$Arguments,
        [Parameter(Mandatory = $true)][string]$Marker
    )

    $tempLog = Join-Path $env:TEMP ("jwplc_nb3h_gitcheck_{0}_{1}.log" -f $Marker, [guid]::NewGuid().ToString("N"))
    $previousPreference = $ErrorActionPreference

    try {
        $ErrorActionPreference = "Continue"
        & git -C $script:G2RepoRoot @Arguments *> $tempLog
        $exitCode = [int]$LASTEXITCODE
    }
    finally {
        $ErrorActionPreference = $previousPreference
    }

    Write-Host ("{0}_EXIT={1}" -f $Marker, $exitCode)

    if (Test-Path -LiteralPath $tempLog) {
        $lines = @(Get-Content -LiteralPath $tempLog)
        Write-Host ("{0}_OUTPUT_LINES={1}" -f $Marker, $lines.Count)
        foreach ($line in $lines) {
            Write-Host ("{0}_OUTPUT={1}" -f $Marker, $line)
        }
        Remove-Item -LiteralPath $tempLog -Force
    }

    if ($exitCode -ne 0) {
        throw ("A14_NB3H_{0}_FAILED={1}" -f $Marker, $exitCode)
    }
}

function Get-NB3HFileSha256 {
    param(
        [Parameter(Mandatory = $true)][string]$Path
    )

    $stream = [System.IO.File]::OpenRead($Path)
    $sha256 = $null

    try {
        $sha256 = [System.Security.Cryptography.SHA256]::Create()
        $bytes = $sha256.ComputeHash($stream)
    }
    finally {
        if ($null -ne $sha256) {
            $sha256.Dispose()
        }
        $stream.Dispose()
    }

    return ([System.BitConverter]::ToString($bytes)).Replace("-", "").ToUpperInvariant()
}

function Write-NB3HStagedSnapshot {
    $timestamp = Get-Date -Format "yyyyMMdd_HHmmss"
    $root = Join-Path $env:TEMP ("jwplc_a14_nb3h_snapshot_{0}" -f $timestamp)
    New-Item -ItemType Directory -Force -Path $root | Out-Null

    $nameStatusPath = Join-Path $root "staged_name_status.txt"
    $numStatPath = Join-Path $root "staged_numstat.txt"
    $statPath = Join-Path $root "staged_stat.txt"
    $patchPath = Join-Path $root "staged.patch"

    $nameStatus = @(& git -C $script:G2RepoRoot diff --cached --name-status)
    if ($LASTEXITCODE -ne 0) { throw "A14_NB3H_NAME_STATUS_FAILED" }
    $nameStatus | Set-Content -LiteralPath $nameStatusPath -Encoding UTF8

    $numStat = @(& git -C $script:G2RepoRoot diff --cached --numstat)
    if ($LASTEXITCODE -ne 0) { throw "A14_NB3H_NUMSTAT_FAILED" }
    $numStat | Set-Content -LiteralPath $numStatPath -Encoding UTF8

    $stat = @(& git -C $script:G2RepoRoot diff --cached --stat)
    if ($LASTEXITCODE -ne 0) { throw "A14_NB3H_STAT_FAILED" }
    $stat | Set-Content -LiteralPath $statPath -Encoding UTF8

    $patch = @(& git -C $script:G2RepoRoot diff --cached --binary)
    if ($LASTEXITCODE -ne 0) { throw "A14_NB3H_PATCH_FAILED" }
    $patch | Set-Content -LiteralPath $patchPath -Encoding UTF8

    Write-Host "SNAPSHOT_DIR=$root"
    Write-Host "SNAPSHOT_NAME_STATUS=$nameStatusPath"
    Write-Host "SNAPSHOT_NUMSTAT=$numStatPath"
    Write-Host "SNAPSHOT_STAT=$statPath"
    Write-Host "SNAPSHOT_PATCH=$patchPath"
    Write-Host "SNAPSHOT_PATCH_SHA256=$(Get-NB3HFileSha256 -Path $patchPath)"

    Write-Host ""
    Write-Host "=== STAGED NAME-STATUS ==="
    $nameStatus | ForEach-Object { Write-Host $_ }

    Write-Host ""
    Write-Host "=== STAGED NUMSTAT ==="
    $numStat | ForEach-Object { Write-Host $_ }

    Write-Host ""
    Write-Host "=== STAGED STAT ==="
    $stat | ForEach-Object { Write-Host $_ }
}

Write-Host "============================================================"
Write-Host " A14 NB3-H - SOURCE SNAPSHOT / COMMIT CANDIDATE"
Write-Host "============================================================"

Assert-G2Branch
$headBefore = Get-G2Head
Write-Host "HEAD_BEFORE=$headBefore"

if ($Commit) {
    Write-Host "MODE=COMMIT"
}
else {
    Write-Host "MODE=SNAPSHOT_STAGE"
}

Write-Host "UPLOAD=NO"
Write-Host "PUSH=NO"
Write-Host "SOURCE_CONTENT_MUTATION=NO"

$spiHz = Get-G2SpiHz
Write-Host "EFFECTIVE_SPI_HZ=$spiHz"
if ($spiHz -ne 26000000) {
    throw "A14_NB3H_SPI_FREQUENCY_MISMATCH=$spiHz"
}

Assert-G2ProtectedArtifacts

foreach ($entry in $expectedHashes.GetEnumerator()) {
    $actual = Get-G2Sha256 $entry.Key
    Write-Host "CANDIDATE_SHA256=$($entry.Key)=$actual"

    if ($actual -ne $entry.Value) {
        throw "A14_NB3H_HASH_MISMATCH=$($entry.Key)"
    }
}

$stagedBefore = @(Get-NB3HStagedPaths)
$unstagedBefore = @(Get-NB3HUnstagedPaths)

Write-Host "STAGED_COUNT_BEFORE=$($stagedBefore.Count)"
Write-Host "UNSTAGED_COUNT_BEFORE=$($unstagedBefore.Count)"

if ($Commit) {
    Assert-NB3HExactPaths -Actual $stagedBefore -Label "STAGED_BEFORE"

    if ($unstagedBefore.Count -ne 0) {
        $unstagedBefore | ForEach-Object { Write-Host "UNSTAGED_UNEXPECTED=$_" }
        throw "A14_NB3H_COMMIT_REQUIRES_ZERO_UNSTAGED=$($unstagedBefore.Count)"
    }

    Invoke-NB3HGitCheck -Arguments @("diff", "--cached", "--check") -Marker "GIT_DIFF_CACHED_CHECK"
}
else {
    if ($stagedBefore.Count -eq 0) {
        Assert-NB3HExactPaths -Actual $unstagedBefore -Label "UNSTAGED_BEFORE"
        Invoke-NB3HGitCheck -Arguments @("diff", "--check") -Marker "GIT_DIFF_CHECK"

        $gitAddArgs = @("-C", $script:G2RepoRoot, "add", "--") + $expectedPaths
        $previousPreference = $ErrorActionPreference

        try {
            $ErrorActionPreference = "Continue"
            & git @gitAddArgs
            $addExit = [int]$LASTEXITCODE
        }
        finally {
            $ErrorActionPreference = $previousPreference
        }

        Write-Host "GIT_ADD_EXIT=$addExit"
        Write-Host "GIT_ADD_SCOPE=EXPLICIT_11_PATHS"

        if ($addExit -ne 0) {
            throw "A14_NB3H_GIT_ADD_FAILED=$addExit"
        }
    }
    elseif ($stagedBefore.Count -eq $expectedPaths.Count -and $unstagedBefore.Count -eq 0) {
        Assert-NB3HExactPaths -Actual $stagedBefore -Label "STAGED_RESUME"
        Write-Host "STAGING_MODE=RESUME_ALREADY_STAGED"
    }
    else {
        throw "A14_NB3H_MIXED_INDEX_STATE_NOT_ALLOWED"
    }
}

$stagedReady = @(Get-NB3HStagedPaths)
$unstagedReady = @(Get-NB3HUnstagedPaths)

Assert-NB3HExactPaths -Actual $stagedReady -Label "STAGED_READY"

if ($unstagedReady.Count -ne 0) {
    $unstagedReady | ForEach-Object { Write-Host "UNSTAGED_AFTER_STAGE=$_" }
    throw "A14_NB3H_UNSTAGED_AFTER_STAGE=$($unstagedReady.Count)"
}

Invoke-NB3HGitCheck -Arguments @("diff", "--cached", "--check") -Marker "GIT_DIFF_CACHED_CHECK"
Write-NB3HStagedSnapshot

if (-not $Commit) {
    Write-Host ""
    Write-Host "NB3_H_SOURCE_HASHES=PASS"
    Write-Host "NB3_H_EXPLICIT_STAGE=PASS"
    Write-Host "NB3_H_STAGED_DIFF_CHECK=PASS"
    Write-Host "NB3_H_SOURCE_SNAPSHOT=PASS"
    Write-Host "NB3_H_PRODUCT_COMMIT=NOT_EXECUTED"
    Write-Host "A14_NB3_H_PHASE=READY_FOR_REVIEW"
    Write-Host "NEXT_ACTION=RETURN_THIS_OUTPUT_TO_CHAT"
    exit 0
}

$commitMessage = "feat(alpha14): consolidar candidato NB3 Ethernet"
$commitBody = "Consolida el candidato NB3 validado a 26 MHz con primitives W5x00 acotadas, UDP SEND cooperativo, DNS async, parsePacket endurecido y waits TCP legacy acotados."

$previousPreference = $ErrorActionPreference
try {
    $ErrorActionPreference = "Continue"
    & git -C $script:G2RepoRoot commit -m $commitMessage -m $commitBody
    $commitExit = [int]$LASTEXITCODE
}
finally {
    $ErrorActionPreference = $previousPreference
}

Write-Host "PRODUCT_COMMIT_EXIT=$commitExit"

if ($commitExit -ne 0) {
    throw "A14_NB3H_PRODUCT_COMMIT_FAILED=$commitExit"
}

$headAfter = Get-G2Head
Write-Host "HEAD_AFTER=$headAfter"

if ($headAfter -eq $headBefore) {
    throw "A14_NB3H_HEAD_DID_NOT_ADVANCE"
}

$trackedDirtyAfter = @(Get-G2TrackedDirtyPaths)
$stagedAfter = @(Get-NB3HStagedPaths)

Write-Host "TRACKED_DIRTY_COUNT_AFTER=$($trackedDirtyAfter.Count)"
Write-Host "STAGED_COUNT_AFTER=$($stagedAfter.Count)"

if ($trackedDirtyAfter.Count -ne 0 -or $stagedAfter.Count -ne 0) {
    throw "A14_NB3H_POST_COMMIT_TRACKED_TREE_NOT_CLEAN"
}

Assert-G2ProtectedArtifacts

foreach ($entry in $expectedHashes.GetEnumerator()) {
    $actual = Get-G2Sha256 $entry.Key
    if ($actual -ne $entry.Value) {
        throw "A14_NB3H_POST_COMMIT_HASH_MISMATCH=$($entry.Key)"
    }
}

Write-Host ""
Write-Host "NB3_H_SOURCE_HASHES=PASS"
Write-Host "NB3_H_EXPLICIT_STAGE=PASS"
Write-Host "NB3_H_STAGED_DIFF_CHECK=PASS"
Write-Host "NB3_H_SOURCE_SNAPSHOT=PASS"
Write-Host "NB3_H_PRODUCT_COMMIT=PASS"
Write-Host "NB3_H_TRACKED_TREE_CLEAN=PASS"
Write-Host "A14_NB3_H_SOURCE_SNAPSHOT_AND_COMMIT=PASS"
Write-Host "PUSH=NO"
Write-Host "NEXT_ACTION=RETURN_THIS_OUTPUT_TO_CHAT_BEFORE_PUSH"
