param(
    [int]$ExpectedMHz = 26,
    [int]$FromChunks = 8,
    [int]$ToChunks = 6
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"
. (Join-Path $PSScriptRoot "common.ps1")

Write-Host "============================================================"
Write-Host " G3 TRANSITION TCP RX CHUNKS"
Write-Host "============================================================"

Assert-G2Branch
Assert-G2AllowedMHz -MHz $ExpectedMHz

if ($FromChunks -lt 1 -or $ToChunks -lt 1 -or $FromChunks -eq $ToChunks) {
    throw "G3_INVALID_CHUNK_TRANSITION"
}

$head = Get-G2Head
$currentHz = Get-G2SpiHz
$expectedHz = [int64]$ExpectedMHz * 1000000
$dirtyBefore = @(Get-G2TrackedDirtyPaths)
$stagedBefore = @(& git -C $script:G2RepoRoot diff --cached --name-only)

if ($LASTEXITCODE -ne 0) {
    throw "G3_TRANSITION_CACHED_DIFF_BEFORE_FAILED"
}

Write-Host "HEAD=$head"
Write-Host "EXPECTED_MHZ=$ExpectedMHz"
Write-Host "CURRENT_SPI_HZ=$currentHz"
Write-Host "FROM_CHUNKS=$FromChunks"
Write-Host "TO_CHUNKS=$ToChunks"
Write-Host "TRACKED_DIRTY_BEFORE=$($dirtyBefore.Count)"
$dirtyBefore | ForEach-Object { Write-Host "DIRTY_BEFORE=$_" }
Write-Host "STAGED_COUNT_BEFORE=$($stagedBefore.Count)"

if ($currentHz -ne $expectedHz) {
    throw "G3_TRANSITION_SPI_FREQUENCY_MISMATCH"
}

$expectedDirty = @(
    $script:G2SpiHeaderRelative,
    $script:G2RawFirmwareRelative
) | Sort-Object

if ($dirtyBefore.Count -ne 2) {
    throw "G3_TRANSITION_DIRTY_COUNT_BEFORE_INVALID=$($dirtyBefore.Count)"
}

for ($i = 0; $i -lt $expectedDirty.Count; ++$i) {
    if ($dirtyBefore[$i] -ne $expectedDirty[$i]) {
        throw "G3_TRANSITION_DIRTY_PATH_BEFORE_INVALID=$($dirtyBefore[$i])"
    }
}

if ($stagedBefore.Count -ne 0) {
    throw "G3_TRANSITION_INDEX_NOT_CLEAN_BEFORE"
}

$spiNumstat = @(& git -C $script:G2RepoRoot diff --numstat -- $script:G2SpiHeaderRelative)
$firmwareNumstatBefore = @(& git -C $script:G2RepoRoot diff --numstat -- $script:G2RawFirmwareRelative)

if ($spiNumstat.Count -ne 1 -or $spiNumstat[0] -notmatch '^1\s+1\s+') {
    throw "G3_TRANSITION_SPI_DIFF_NOT_1_1"
}

if ($firmwareNumstatBefore.Count -ne 1 -or $firmwareNumstatBefore[0] -notmatch '^1\s+1\s+') {
    throw "G3_TRANSITION_FIRMWARE_DIFF_BEFORE_NOT_1_1"
}

Write-Host "SPI_DIFF_NUMSTAT=$($spiNumstat[0])"
Write-Host "FIRMWARE_DIFF_NUMSTAT_BEFORE=$($firmwareNumstatBefore[0])"

Assert-G2ProtectedArtifacts

$runnerHash = Get-G2Sha256 $script:G2RawRunnerRelative
Write-Host "RAW_RUNNER_SHA256=$runnerHash"

if ($runnerHash -ne $script:G2RawRunnerSha256) {
    throw "G3_TRANSITION_RAW_RUNNER_HASH_MISMATCH"
}

$firmwarePath = Get-G2Path $script:G2RawFirmwareRelative
$text = [System.IO.File]::ReadAllText($firmwarePath)
$pattern = '(?m)^(\s*static constexpr uint8_t TCP_RX_MAX_CHUNKS_PER_LOCK = )(\d+)(;\s*)$'
$matches = @([regex]::Matches($text, $pattern))

Write-Host "TCP_RX_CHUNK_DECLARATION_COUNT=$($matches.Count)"

if ($matches.Count -ne 1) {
    throw "G3_TRANSITION_TCP_RX_CHUNK_DECLARATION_COUNT=$($matches.Count)"
}

$currentChunks = [int]$matches[0].Groups[2].Value
Write-Host "CURRENT_CHUNKS=$currentChunks"

if ($currentChunks -ne $FromChunks) {
    throw "G3_TRANSITION_FROM_CHUNKS_MISMATCH=$currentChunks"
}

$beforeHash = Get-G2Sha256 $script:G2RawFirmwareRelative
Write-Host "G3_FIRMWARE_SHA256_BEFORE=$beforeHash"

$chunkGroup = $matches[0].Groups[2]
$newText = (
    $text.Substring(0, $chunkGroup.Index) +
    $ToChunks.ToString([System.Globalization.CultureInfo]::InvariantCulture) +
    $text.Substring($chunkGroup.Index + $chunkGroup.Length)
)

$utf8NoBom = New-Object -TypeName System.Text.UTF8Encoding -ArgumentList $false
[System.IO.File]::WriteAllText($firmwarePath, $newText, $utf8NoBom)

$verifyText = [System.IO.File]::ReadAllText($firmwarePath)
$verifyMatches = @([regex]::Matches($verifyText, $pattern))

if ($verifyMatches.Count -ne 1) {
    throw "G3_TRANSITION_VERIFY_DECLARATION_COUNT=$($verifyMatches.Count)"
}

$effectiveChunks = [int]$verifyMatches[0].Groups[2].Value
Write-Host "EFFECTIVE_CHUNKS=$effectiveChunks"

if ($effectiveChunks -ne $ToChunks) {
    throw "G3_TRANSITION_EFFECTIVE_CHUNKS_MISMATCH"
}

& git -C $script:G2RepoRoot diff --check
if ($LASTEXITCODE -ne 0) {
    throw "G3_TRANSITION_DIFF_CHECK_FAILED"
}

$dirtyAfter = @(Get-G2TrackedDirtyPaths)
Write-Host "TRACKED_DIRTY_AFTER=$($dirtyAfter.Count)"
$dirtyAfter | ForEach-Object { Write-Host "DIRTY_AFTER=$_" }

if ($dirtyAfter.Count -ne 2) {
    throw "G3_TRANSITION_DIRTY_COUNT_AFTER_INVALID=$($dirtyAfter.Count)"
}

for ($i = 0; $i -lt $expectedDirty.Count; ++$i) {
    if ($dirtyAfter[$i] -ne $expectedDirty[$i]) {
        throw "G3_TRANSITION_DIRTY_PATH_AFTER_INVALID=$($dirtyAfter[$i])"
    }
}

$firmwareNumstatAfter = @(& git -C $script:G2RepoRoot diff --numstat -- $script:G2RawFirmwareRelative)

if ($LASTEXITCODE -ne 0 -or $firmwareNumstatAfter.Count -ne 1) {
    throw "G3_TRANSITION_FIRMWARE_NUMSTAT_AFTER_FAILED"
}

Write-Host "FIRMWARE_DIFF_NUMSTAT_AFTER=$($firmwareNumstatAfter[0])"

if ($firmwareNumstatAfter[0] -notmatch '^1\s+1\s+') {
    throw "G3_TRANSITION_FIRMWARE_DIFF_AFTER_NOT_1_1"
}

$afterHash = Get-G2Sha256 $script:G2RawFirmwareRelative
Write-Host "G3_FIRMWARE_SHA256_AFTER=$afterHash"
Write-Host "RAW_RUNNER_SHA256=$runnerHash"

Assert-G2ProtectedArtifacts

$stagedAfter = @(& git -C $script:G2RepoRoot diff --cached --name-only)

if ($LASTEXITCODE -ne 0) {
    throw "G3_TRANSITION_CACHED_DIFF_AFTER_FAILED"
}

Write-Host "STAGED_COUNT_AFTER=$($stagedAfter.Count)"

if ($stagedAfter.Count -ne 0) {
    throw "G3_TRANSITION_INDEX_NOT_CLEAN_AFTER"
}

Write-Host "G3_STATIC_CHANGE=${FromChunks}_TO_${ToChunks}_CHUNKS_ONLY"
Write-Host "G3_TRANSITION_TCP_RX_CHUNKS=PASS"
