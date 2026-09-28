param(
    [string]$MasterPort = "COM14",
    [string]$SlavePort = "COM4",
    [string]$CandidatePath = "",
    [string]$CandidateManifestPath = "",
    [switch]$PreflightOnly
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

. (Join-Path $PSScriptRoot "common.ps1")

Write-Host "============================================================"
Write-Host " A14 H3E.1B.2-B - CANDIDATE DISPLAY ARCHIVE QUALIFICATION"
Write-Host "============================================================"

Assert-G2Branch

$expectedCoreHash = "4BFF8C8241DA2E8BD0E1BBA99835ADDF91B9085A05C4DFBD05C339B824794566"
$expectedRtuArchiveHash = "444BE3A04079A579252B2737FE6070E00ADCA949FD176880588FE69561B2A79F"
$expectedHistoricalDisplayHash = "2974D42C847C1B7C7AB3A7B74DA42E2F17969FB852B47A8D434F57F70DA924AF"
$expectedCandidateHash = "B451A055E983B12B9BB2FBF134B8311B88E4CAB36B11EB89611CC36FAAF02BD0"
$expectedCandidateBytes = 970776
$candidateSourceCommit = "9fc49d8ac1325cf55f2d8c860285a219ca927eda"

$sourceDisplayAvgUs = 8575.0
$sourceDisplayMaxUs = 9840.0
$sourceRtuHz = 772.962
$sourceTcpAvgUs = 1219.3
$sourceTcpP99Us = 9817.5
$sourceServiceGapMaxUs = 17315.0
$avgTolerancePct = 10.0

$rtuArchiveRelative = "JWPLC/2.1.0/libraries/JWPLC_ModbusRTU/src/esp32/libJWPLC_ModbusRTU.a"
$displayArchiveRelative = "JWPLC/2.1.0/libraries/JWPLC_Display/src/esp32/libJWPLC_Display.a"
$displaySourceRelative = "JWPLC/2.1.0/libraries/JWPLC_Display/src"

$rtuArchivePath = Get-G2Path $rtuArchiveRelative
$displayArchivePath = Get-G2Path $displayArchiveRelative
$displaySourcePath = Get-G2Path $displaySourceRelative

$legGate = Join-Path $PSScriptRoot "a14_h3e1b1_display_linkage_leg.ps1"
$setupGate = Join-Path $PSScriptRoot "a14_h3e1b1_display_linkage_setup.ps1"

if ([string]::IsNullOrWhiteSpace($CandidatePath)) {
    $CandidatePath = Join-Path $env:TEMP "jwplc_a14_h3e1b2_candidate_current\libJWPLC_Display.candidate.a"
}
if ([string]::IsNullOrWhiteSpace($CandidateManifestPath)) {
    $CandidateManifestPath = Join-Path $env:TEMP "jwplc_a14_h3e1b2_candidate_current\candidate_manifest.json"
}

$CandidatePath = [IO.Path]::GetFullPath($CandidatePath)
$CandidateManifestPath = [IO.Path]::GetFullPath($CandidateManifestPath)

foreach ($required in @(
    $rtuArchivePath,
    $displayArchivePath,
    $displaySourcePath,
    $legGate,
    $setupGate,
    $CandidatePath,
    $CandidateManifestPath
)) {
    if (-not (Test-Path -LiteralPath $required)) {
        throw "H3E1B2B_REQUIRED_PATH_MISSING=$required"
    }
}

[string[]]$dirty = @(Get-G2TrackedDirtyPaths)
[string[]]$staged = @(& git -C $script:G2RepoRoot diff --cached --name-only)

if ($dirty.Count -ne 1 -or
    $dirty[0].Replace("\", "/") -ne $script:G2CoreRelative) {
    $dirty | ForEach-Object { Write-Host "DIRTY=$_" }
    throw "H3E1B2B_EXPECTED_ONLY_DIRTY_CORE_A"
}

if ($staged.Count -ne 0) {
    throw "H3E1B2B_INDEX_NOT_CLEAN"
}

$coreHashBefore = Get-G2Sha256 $script:G2CoreRelative
$rtuHashBefore = Get-G2Sha256 $rtuArchiveRelative
$historicalDisplayHashBefore = Get-G2Sha256 $displayArchiveRelative

if ($coreHashBefore -ne $expectedCoreHash) {
    throw "H3E1B2B_CORE_HASH_INVALID"
}
if ($rtuHashBefore -ne $expectedRtuArchiveHash) {
    throw "H3E1B2B_RTU_ARCHIVE_HASH_INVALID"
}
if ($historicalDisplayHashBefore -ne $expectedHistoricalDisplayHash) {
    throw "H3E1B2B_HISTORICAL_DISPLAY_HASH_INVALID"
}

$candidateFile = Get-Item -LiteralPath $CandidatePath
$candidateHash = (Get-FileHash -LiteralPath $CandidatePath -Algorithm SHA256).Hash.ToUpperInvariant()

Write-Host "CANDIDATE_PATH=$CandidatePath"
Write-Host "CANDIDATE_MANIFEST_PATH=$CandidateManifestPath"
Write-Host "CANDIDATE_SHA256=$candidateHash"
Write-Host "CANDIDATE_BYTES=$($candidateFile.Length)"

if ($candidateHash -ne $expectedCandidateHash) {
    throw "H3E1B2B_CANDIDATE_HASH_INVALID"
}
if ([int64]$candidateFile.Length -ne [int64]$expectedCandidateBytes) {
    throw "H3E1B2B_CANDIDATE_SIZE_INVALID"
}

$manifest = Get-Content -LiteralPath $CandidateManifestPath -Raw | ConvertFrom-Json

if ([string]$manifest.schema -ne "jwplc-a14-h3e1b2-display-candidate-v1") {
    throw "H3E1B2B_MANIFEST_SCHEMA_INVALID"
}
if ([string]$manifest.candidate_sha256 -ne $expectedCandidateHash) {
    throw "H3E1B2B_MANIFEST_CANDIDATE_HASH_INVALID"
}
if ([int64]$manifest.candidate_bytes -ne [int64]$expectedCandidateBytes) {
    throw "H3E1B2B_MANIFEST_CANDIDATE_SIZE_INVALID"
}
if ([int]$manifest.source_tu_count -ne 7) {
    throw "H3E1B2B_MANIFEST_SOURCE_TU_COUNT_INVALID"
}
if ([string]$manifest.historical_display_sha256 -ne $expectedHistoricalDisplayHash) {
    throw "H3E1B2B_MANIFEST_HISTORICAL_HASH_INVALID"
}

[string[]]$expectedTus = @(
    "JWPLC_Display.cpp",
    "JWPLC_Display_H3E1_Profile.cpp",
    "JWPLC_IdleScreen.cpp",
    "JWPLC_UI.cpp",
    "JWPLC_UI_API.cpp",
    "JWPLC_UI_Pages.cpp",
    "JWPLC_UI_PixelMap.cpp"
)

[string[]]$manifestTus = @(
    $manifest.source_tus | ForEach-Object { [string]$_ }
)

[object[]]$tuDiff = @(
    Compare-Object -ReferenceObject $expectedTus -DifferenceObject $manifestTus
)

if ($tuDiff.Count -ne 0) {
    $tuDiff | ForEach-Object { Write-Host "MANIFEST_TU_DIFF=$_" }
    throw "H3E1B2B_MANIFEST_TU_SET_INVALID"
}

$oldPreference = $ErrorActionPreference
try {
    $ErrorActionPreference = "Continue"
    & git -C $script:G2RepoRoot diff --quiet $candidateSourceCommit HEAD -- $displaySourceRelative
    $sourceDiffExit = [int]$LASTEXITCODE
}
finally {
    $ErrorActionPreference = $oldPreference
}

Write-Host "CANDIDATE_SOURCE_COMMIT=$candidateSourceCommit"
Write-Host "CURRENT_HEAD=$(Get-G2Head)"
Write-Host "DISPLAY_SOURCE_CHANGED_SINCE_CANDIDATE=$($sourceDiffExit -ne 0)"

if ($sourceDiffExit -ne 0) {
    throw "H3E1B2B_DISPLAY_SOURCE_CHANGED_SINCE_CANDIDATE"
}

$setupText = [IO.File]::ReadAllText($setupGate)
$legText = [IO.File]::ReadAllText($legGate)

foreach ($contract in @(
    "ExpectedDisplayArchiveHash",
    "AdditionalAllowedDirtyPaths"
)) {
    $setupHas = $setupText.Contains($contract)
    $legHas = $legText.Contains($contract)

    Write-Host "SETUP_PARAMETER_CONTRACT $contract=$setupHas"
    Write-Host "LEG_PARAMETER_CONTRACT $contract=$legHas"

    if (-not $setupHas -or -not $legHas) {
        throw "H3E1B2B_PARAMETER_CONTRACT_MISSING=$contract"
    }
}

Write-Host ""
Write-Host "=== HISTORICAL LEG STATIC PREFLIGHT ==="

[object[]]$historicalPreflight = @(
    & $legGate -MasterPort $MasterPort -SlavePort $SlavePort -DisplayLinkage ARCHIVE -PreflightOnly *>&1
)

$historicalPreflight | ForEach-Object { Write-Host $_ }

$historicalPreflightText =
    ($historicalPreflight | ForEach-Object { $_.ToString() }) -join [Environment]::NewLine

if (-not $historicalPreflightText.Contains("A14_H3E1B1_LINKAGE_LEG_PREFLIGHT_ONLY=PASS")) {
    throw "H3E1B2B_HISTORICAL_LEG_PREFLIGHT_FAILED"
}

Write-Host "H3E1B2B_STATIC_PREFLIGHT=PASS"
Write-Host "CANDIDATE_INSTALL_POLICY=TEMPORARY_AROUND_VALIDATED_LEG"
Write-Host "HISTORICAL_ARCHIVE_RESTORE_AFTER_LEG=YES"
Write-Host "CANDIDATE_MUTATES_REPO_PERMANENTLY=NO"
Write-Host "SOURCE_BASELINE_DISPLAY_AVG_US=$sourceDisplayAvgUs"
Write-Host "SOURCE_BASELINE_DISPLAY_MAX_US=$sourceDisplayMaxUs"
Write-Host "EQUIVALENCE_AVG_TOLERANCE_PCT=$avgTolerancePct"

if ($PreflightOnly) {
    Write-Host "PREFLIGHT_INSTALLS_CANDIDATE=NO"
    Write-Host "PREFLIGHT_COMPILES=NO"
    Write-Host "PREFLIGHT_UPLOADS=NO"
    Write-Host "PREFLIGHT_RUNS_300S=NO"
    Write-Output "A14_H3E1B2B_CANDIDATE_ARCHIVE_PREFLIGHT_ONLY=PASS"
    return
}

function Get-RequiredMatch {
    param(
        [Parameter(Mandatory = $true)][string]$Text,
        [Parameter(Mandatory = $true)][string]$Pattern,
        [Parameter(Mandatory = $true)][string]$Label
    )

    $match = [regex]::Match(
        $Text,
        $Pattern,
        [Text.RegularExpressions.RegexOptions]::Multiline
    )

    if (-not $match.Success) {
        throw ("H3E1B2B_PARSE_MISSING_" + $Label)
    }

    return $match.Groups[1].Value.Trim()
}

function Get-OutputDouble {
    param(
        [Parameter(Mandatory = $true)][string]$Text,
        [Parameter(Mandatory = $true)][string]$Key
    )

    $escaped = [regex]::Escape($Key)

    $value = Get-RequiredMatch -Text $Text -Pattern ("^" + $escaped + "=([0-9]+(?:\.[0-9]+)?)\r?$") -Label $Key

    return [double]::Parse(
        $value,
        [Globalization.CultureInfo]::InvariantCulture
    )
}

function Get-StageMetric {
    param(
        [Parameter(Mandatory = $true)][string]$Text,
        [Parameter(Mandatory = $true)][string]$Stage,
        [Parameter(Mandatory = $true)]
        [ValidateSet("CALLS", "TOTAL_US", "AVG_US", "MAX_US")]
        [string]$Metric
    )

    $pattern =
        "^RTUH3E0B_STAGE NAME=" +
        [regex]::Escape($Stage) +
        " CALLS=([0-9]+) TOTAL_US=([0-9]+) AVG_US=([0-9]+) MAX_US=([0-9]+)\r?$"

    $match = [regex]::Match(
        $Text,
        $pattern,
        [Text.RegularExpressions.RegexOptions]::Multiline
    )

    if (-not $match.Success) {
        throw ("H3E1B2B_STAGE_PARSE_MISSING_" + $Stage + "_" + $Metric)
    }

    $index = switch ($Metric) {
        "CALLS" { 1 }
        "TOTAL_US" { 2 }
        "AVG_US" { 3 }
        "MAX_US" { 4 }
    }

    return [double]::Parse(
        $match.Groups[$index].Value,
        [Globalization.CultureInfo]::InvariantCulture
    )
}

$timestamp = Get-Date -Format "yyyyMMdd_HHmmss"
$tempRoot = Join-Path $env:TEMP ("jwplc_a14_h3e1b2b_{0}" -f $timestamp)
New-Item -ItemType Directory -Force -Path $tempRoot | Out-Null

$historicalBackup = Join-Path $tempRoot "libJWPLC_Display.historical.a"
$legOutputLog = Join-Path $tempRoot "candidate_leg_output.log"

[IO.File]::WriteAllBytes(
    $historicalBackup,
    [IO.File]::ReadAllBytes($displayArchivePath)
)

$backupHash = (Get-FileHash -LiteralPath $historicalBackup -Algorithm SHA256).Hash.ToUpperInvariant()

if ($backupHash -ne $expectedHistoricalDisplayHash) {
    throw "H3E1B2B_HISTORICAL_BACKUP_HASH_INVALID"
}

Write-Host "H3E1B2B_TEMP_ROOT=$tempRoot"
Write-Host "HISTORICAL_BACKUP=$historicalBackup"

$legSucceeded = $false
$restoreError = $null

try {
    Write-Host ""
    Write-Host "=== INSTALL CANDIDATE TEMPORARILY ==="

    [IO.File]::WriteAllBytes(
        $displayArchivePath,
        [IO.File]::ReadAllBytes($CandidatePath)
    )

    $installedHash = Get-G2Sha256 $displayArchiveRelative
    Write-Host "INSTALLED_CANDIDATE_SHA256=$installedHash"

    if ($installedHash -ne $expectedCandidateHash) {
        throw "H3E1B2B_INSTALLED_CANDIDATE_HASH_INVALID"
    }

    [string[]]$candidateDirty = @(
        Get-G2TrackedDirtyPaths |
            ForEach-Object { $_.Replace("\", "/") } |
            Sort-Object
    )

    [string[]]$expectedCandidateDirty = @(
        $script:G2CoreRelative.Replace("\", "/"),
        $displayArchiveRelative.Replace("\", "/")
    ) | Sort-Object

    if ($candidateDirty.Count -ne $expectedCandidateDirty.Count) {
        $candidateDirty | ForEach-Object { Write-Host "CANDIDATE_DIRTY=$_" }
        throw "H3E1B2B_CANDIDATE_DIRTY_COUNT_INVALID"
    }

    for ($i = 0; $i -lt $expectedCandidateDirty.Count; ++$i) {
        if ($candidateDirty[$i] -ne $expectedCandidateDirty[$i]) {
            throw "H3E1B2B_CANDIDATE_DIRTY_SCOPE_INVALID"
        }
    }

    Write-Host "CANDIDATE_DIRTY_SCOPE=PASS"

    Write-Host ""
    Write-Host "=== RUN VALIDATED ARCHIVE LEG WITH CANDIDATE ==="

    & $legGate -MasterPort $MasterPort -SlavePort $SlavePort -DisplayLinkage ARCHIVE -ExpectedDisplayArchiveHash $expectedCandidateHash -AdditionalAllowedDirtyPaths $displayArchiveRelative *>&1 |
        Tee-Object -FilePath $legOutputLog


    $legTextOutput = [IO.File]::ReadAllText($legOutputLog)

    if (-not $legTextOutput.Contains("A14_H3E1B1_DISPLAY_LINKAGE_LEG_GATE=PASS")) {
        throw "H3E1B2B_CANDIDATE_LEG_PASS_MISSING"
    }
    if (-not $legTextOutput.Contains("DISPLAY_LINKAGE_PROOF=ARCHIVE_NO_SOURCE_OBJECTS")) {
        throw "H3E1B2B_ARCHIVE_LINKAGE_PROOF_MISSING"
    }
    if (-not $legTextOutput.Contains("MASTER_DISPLAY_SOURCE_OBJECT_COUNT=0")) {
        throw "H3E1B2B_SOURCE_OBJECT_ZERO_PROOF_MISSING"
    }
    if (-not $legTextOutput.Contains("DISPLAY_ARCHIVE_SHA256=$expectedCandidateHash")) {
        throw "H3E1B2B_CANDIDATE_HASH_CONFIRMATION_MISSING"
    }

    $legSucceeded = $true
}
finally {
    try {
        [IO.File]::WriteAllBytes(
            $displayArchivePath,
            [IO.File]::ReadAllBytes($historicalBackup)
        )

        $restoredHash = Get-G2Sha256 $displayArchiveRelative
        Write-Host "RESTORED_HISTORICAL_DISPLAY_SHA256=$restoredHash"

        if ($restoredHash -ne $expectedHistoricalDisplayHash) {
            throw "H3E1B2B_HISTORICAL_RESTORE_HASH_INVALID"
        }

        Write-Host "H3E1B2B_HISTORICAL_ARCHIVE_RESTORED=YES"
    }
    catch {
        $restoreError = $_
    }
}

if ($null -ne $restoreError) {
    throw $restoreError
}

if (-not $legSucceeded) {
    throw "H3E1B2B_CANDIDATE_LEG_DID_NOT_COMPLETE"
}

$legTextOutput = [IO.File]::ReadAllText($legOutputLog)

$runnerLog = Get-RequiredMatch -Text $legTextOutput -Pattern "^H3E1B1_RUNNER_LOG=(.+?)\r?$" -Label "RUNNER_LOG"

if (-not (Test-Path -LiteralPath $runnerLog)) {
    throw "H3E1B2B_RUNNER_LOG_MISSING"
}

$runText = [IO.File]::ReadAllText($runnerLog)

$candidateDisplayAvgUs = Get-StageMetric -Text $runText -Stage "SYS_DISPLAY" -Metric "AVG_US"
$candidateDisplayMaxUs = Get-StageMetric -Text $runText -Stage "SYS_DISPLAY" -Metric "MAX_US"
$candidateRtuHz = Get-OutputDouble -Text $runText -Key "RTUH3E0B_RTU_HZ"
$candidateTcpAvgUs = Get-OutputDouble -Text $runText -Key "RTUH3E0B_TCP_AVG_US"
$candidateTcpP99Us = Get-OutputDouble -Text $runText -Key "RTUH3E0B_TCP_P99_US"
$candidateServiceGapMaxUs = Get-OutputDouble -Text $runText -Key "RTUH3E0B_RTU_SERVICE_GAP_MAX_US"

$runtimeClean = $runText.Contains("RTUH3E0B_RUNTIME_CLEAN=YES")

$avgDeltaPct =
    100.0 * ($candidateDisplayAvgUs - $sourceDisplayAvgUs) / $sourceDisplayAvgUs

$maxDeltaPct =
    100.0 * ($candidateDisplayMaxUs - $sourceDisplayMaxUs) / $sourceDisplayMaxUs

$avgEquivalent =
    [Math]::Abs($avgDeltaPct) -le $avgTolerancePct

$performanceEquivalent = $avgEquivalent
$releaseEquivalent = $performanceEquivalent -and $runtimeClean

Write-Host ""
Write-Host "============================================================"
Write-Host " H3E1B2 SOURCE VS REGENERATED ARCHIVE"
Write-Host "============================================================"

Write-Host "H3E1B2_SOURCE_DISPLAY_AVG_US=$sourceDisplayAvgUs"
Write-Host "H3E1B2_CANDIDATE_DISPLAY_AVG_US=$candidateDisplayAvgUs"
Write-Host "H3E1B2_SOURCE_DISPLAY_MAX_US=$sourceDisplayMaxUs"
Write-Host "H3E1B2_CANDIDATE_DISPLAY_MAX_US=$candidateDisplayMaxUs"
Write-Host ("H3E1B2_DISPLAY_AVG_DELTA_PCT=" + $avgDeltaPct.ToString("F3", [Globalization.CultureInfo]::InvariantCulture))
Write-Host ("H3E1B2_DISPLAY_MAX_DELTA_PCT=" + $maxDeltaPct.ToString("F3", [Globalization.CultureInfo]::InvariantCulture))
Write-Host "H3E1B2_AVG_EQUIVALENT=$(if ($avgEquivalent) { 'YES' } else { 'NO' })"
Write-Host "H3E1B2_PERFORMANCE_EQUIVALENT=$(if ($performanceEquivalent) { 'YES' } else { 'NO' })"
Write-Host "H3E1B2_RUNTIME_CLEAN=$(if ($runtimeClean) { 'YES' } else { 'NO' })"
Write-Host "H3E1B2_RELEASE_EQUIVALENT=$(if ($releaseEquivalent) { 'YES' } else { 'NO' })"

Write-Host "H3E1B2_SOURCE_RTU_HZ=$sourceRtuHz"
Write-Host "H3E1B2_CANDIDATE_RTU_HZ=$candidateRtuHz"
Write-Host "H3E1B2_SOURCE_TCP_AVG_US=$sourceTcpAvgUs"
Write-Host "H3E1B2_CANDIDATE_TCP_AVG_US=$candidateTcpAvgUs"
Write-Host "H3E1B2_SOURCE_TCP_P99_US=$sourceTcpP99Us"
Write-Host "H3E1B2_CANDIDATE_TCP_P99_US=$candidateTcpP99Us"
Write-Host "H3E1B2_SOURCE_SERVICE_GAP_MAX_US=$sourceServiceGapMaxUs"
Write-Host "H3E1B2_CANDIDATE_SERVICE_GAP_MAX_US=$candidateServiceGapMaxUs"

$coreHashFinal = Get-G2Sha256 $script:G2CoreRelative
$rtuHashFinal = Get-G2Sha256 $rtuArchiveRelative
$displayHashFinal = Get-G2Sha256 $displayArchiveRelative

if ($coreHashFinal -ne $coreHashBefore) {
    throw "H3E1B2B_FINAL_CORE_HASH_CHANGED"
}
if ($rtuHashFinal -ne $rtuHashBefore) {
    throw "H3E1B2B_FINAL_RTU_HASH_CHANGED"
}
if ($displayHashFinal -ne $expectedHistoricalDisplayHash) {
    throw "H3E1B2B_FINAL_HISTORICAL_DISPLAY_NOT_RESTORED"
}

[string[]]$finalDirty = @(Get-G2TrackedDirtyPaths)
[string[]]$finalStaged = @(& git -C $script:G2RepoRoot diff --cached --name-only)

if ($finalDirty.Count -ne 1 -or
    $finalDirty[0].Replace("\", "/") -ne $script:G2CoreRelative) {
    $finalDirty | ForEach-Object { Write-Host "FINAL_DIRTY=$_" }
    throw "H3E1B2B_FINAL_DIRTY_SCOPE_INVALID"
}

if ($finalStaged.Count -ne 0) {
    throw "H3E1B2B_FINAL_INDEX_NOT_CLEAN"
}

Write-Host "H3E1B2_CANDIDATE_SHA256=$expectedCandidateHash"
Write-Host "H3E1B2_HISTORICAL_ARCHIVE_SHA256=$expectedHistoricalDisplayHash"
Write-Host "H3E1B2_CANDIDATE_INSTALLED_PERMANENTLY=NO"
Write-Host "H3E1B2_DIAGNOSTIC_CAPTURE_VALID=YES"
Write-Host "A14_H3E1B2B_CANDIDATE_ARCHIVE_QUALIFICATION_GATE=PASS"
Write-Host "NEXT=RETURN_OUTPUT_TO_CHAT_FOR_ARCHIVE_ADOPTION_DECISION"
