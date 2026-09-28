param(
    [string]$MasterPort = "COM14",
    [string]$SlavePort = "COM4",
    [string]$ReuseSourceSetupRoot = "",
    [switch]$PreflightOnly,
    [switch]$AllowDirtyCoreCandidate
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

. (Join-Path $PSScriptRoot "common.ps1")

Write-Host "============================================================"
Write-Host " A14 H3E.1B.2-A - GENERATE DISPLAY ARCHIVE CANDIDATE"
Write-Host "============================================================"

Assert-G2Branch

$expectedCoreHash = "4BFF8C8241DA2E8BD0E1BBA99835ADDF91B9085A05C4DFBD05C339B824794566"
$expectedRtuArchiveHash = "444BE3A04079A579252B2737FE6070E00ADCA949FD176880588FE69561B2A79F"
$expectedHistoricalDisplayHash = "2974D42C847C1B7C7AB3A7B74DA42E2F17969FB852B47A8D434F57F70DA924AF"

$rtuArchiveRelative = "JWPLC/2.1.0/libraries/JWPLC_ModbusRTU/src/esp32/libJWPLC_ModbusRTU.a"
$displayArchiveRelative = "JWPLC/2.1.0/libraries/JWPLC_Display/src/esp32/libJWPLC_Display.a"
$displaySourceRelative = "JWPLC/2.1.0/libraries/JWPLC_Display/src"

$displayArchivePath = Get-G2Path $displayArchiveRelative
$displaySourceRoot = Get-G2Path $displaySourceRelative
$sourceSetup = Join-Path $PSScriptRoot "a14_h3e1b1_display_linkage_setup.ps1"

if (-not (Test-Path -LiteralPath $sourceSetup)) {
    throw "H3E1B2_SOURCE_SETUP_MISSING"
}

if (-not (Test-Path -LiteralPath $displayArchivePath)) {
    throw "H3E1B2_HISTORICAL_DISPLAY_ARCHIVE_MISSING"
}

[string[]]$dirty = @(Get-G2TrackedDirtyPaths)
[string[]]$staged = @(& git -C $script:G2RepoRoot diff --cached --name-only)

if (-not $AllowDirtyCoreCandidate) {
    if ($dirty.Count -ne 0) {
        $dirty | ForEach-Object { Write-Host "DIRTY=$_" }
        throw "H3E1B2_WORKTREE_NOT_CLEAN"
    }
}
else {
    if ($dirty.Count -ne 1 -or $dirty[0].Replace("\", "/") -ne $script:G2CoreRelative) {
        $dirty | ForEach-Object { Write-Host "DIRTY=$_" }
        throw "H3E1B2_EXPECTED_ONLY_DIRTY_CORE_A"
    }
}

if ($staged.Count -ne 0) {
    throw "H3E1B2_INDEX_NOT_CLEAN"
}

$coreHash = Get-G2Sha256 $script:G2CoreRelative
$rtuHash = Get-G2Sha256 $rtuArchiveRelative
$historicalDisplayHash = Get-G2Sha256 $displayArchiveRelative

Write-Host "CORE_A_SHA256=$coreHash"
Write-Host "MODBUS_RTU_ARCHIVE_SHA256=$rtuHash"
Write-Host "HISTORICAL_DISPLAY_ARCHIVE_SHA256=$historicalDisplayHash"

if ($coreHash -ne $expectedCoreHash) {
    throw "H3E1B2_CORE_HASH_INVALID"
}
if ($rtuHash -ne $expectedRtuArchiveHash) {
    throw "H3E1B2_RTU_ARCHIVE_HASH_INVALID"
}
if ($historicalDisplayHash -ne $expectedHistoricalDisplayHash) {
    throw "H3E1B2_HISTORICAL_DISPLAY_HASH_INVALID"
}

[object[]]$sourceCpp = @(Get-ChildItem -LiteralPath $displaySourceRoot -File -Filter "*.cpp" | Sort-Object Name)
[string[]]$sourceNames = @($sourceCpp | ForEach-Object { $_.Name })

[string[]]$requiredSourceNames = @(
    "JWPLC_Display.cpp",
    "JWPLC_Display_H3E1_Profile.cpp",
    "JWPLC_IdleScreen.cpp",
    "JWPLC_UI.cpp",
    "JWPLC_UI_API.cpp",
    "JWPLC_UI_Pages.cpp",
    "JWPLC_UI_PixelMap.cpp"
)

Write-Host "DISPLAY_SOURCE_TU_COUNT=$($sourceNames.Count)"
$sourceNames | ForEach-Object { Write-Host "DISPLAY_SOURCE_TU=$_" }

if ($sourceNames.Count -ne $requiredSourceNames.Count) {
    throw "H3E1B2_DISPLAY_SOURCE_TU_COUNT_UNEXPECTED"
}

[object[]]$sourceDiff = @(Compare-Object -ReferenceObject $requiredSourceNames -DifferenceObject $sourceNames)

if ($sourceDiff.Count -ne 0) {
    $sourceDiff | ForEach-Object { Write-Host "DISPLAY_SOURCE_TU_DIFF=$_" }
    throw "H3E1B2_DISPLAY_SOURCE_TU_SET_UNEXPECTED"
}

function Resolve-NativeToolPath {
    param([string]$Candidate)

    if ([string]::IsNullOrWhiteSpace($Candidate)) {
        return $null
    }

    $normalized = $Candidate.Trim().Trim('"')

    foreach ($path in @($normalized, ($normalized + ".exe"), ($normalized + ".cmd"), ($normalized + ".bat"))) {
        if (Test-Path -LiteralPath $path) {
            return (Resolve-Path -LiteralPath $path).Path
        }
    }

    return $null
}

function Resolve-ArchiverFromLog {
    param(
        [Parameter(Mandatory = $true)]
        [string]$LogPath
    )

    if (Test-Path -LiteralPath $LogPath) {
        foreach ($line in Get-Content -LiteralPath $LogPath) {
        $candidate = $null

        if ($line -match '"(?<exe>[^"]*xtensa-esp32-elf-gcc-ar(?:\.exe)?)"') {
            $candidate = $Matches["exe"]
        }
        elseif ($line -match '(?<exe>\S*xtensa-esp32-elf-gcc-ar(?:\.exe)?)\s+(?:cr|crs)\b') {
            $candidate = $Matches["exe"]
        }

        if (-not [string]::IsNullOrWhiteSpace($candidate)) {
            $resolved = Resolve-NativeToolPath -Candidate $candidate
            if (-not [string]::IsNullOrWhiteSpace($resolved)) {
                Write-Host "ARCHIVER_RESOLUTION=COMPILE_LOG"
                return $resolved
            }
        }
    }

    }

    if (Test-Path -LiteralPath $LogPath) {
        foreach ($line in Get-Content -LiteralPath $LogPath) {
        $compilerCandidate = $null

        if ($line -match '"(?<exe>[^"]*xtensa-esp32-elf-g\+\+(?:\.exe)?)"') {
            $compilerCandidate = $Matches["exe"]
        }
        elseif ($line -match '(?<exe>\S*xtensa-esp32-elf-g\+\+(?:\.exe)?)\s') {
            $compilerCandidate = $Matches["exe"]
        }

        if ([string]::IsNullOrWhiteSpace($compilerCandidate)) {
            continue
        }

        $compiler = Resolve-NativeToolPath -Candidate $compilerCandidate
        if ([string]::IsNullOrWhiteSpace($compiler)) {
            continue
        }

        $toolDir = Split-Path -Parent $compiler
        foreach ($leaf in @("xtensa-esp32-elf-gcc-ar.exe", "xtensa-esp32-elf-gcc-ar")) {
            $sibling = Join-Path $toolDir $leaf
            if (Test-Path -LiteralPath $sibling) {
                Write-Host "ARCHIVER_RESOLUTION=COMPILER_SIBLING"
                return (Resolve-Path -LiteralPath $sibling).Path
            }
        }
    }

    }

    if (-not [string]::IsNullOrWhiteSpace($env:LOCALAPPDATA)) {
        $packagesRoot = Join-Path $env:LOCALAPPDATA "Arduino15\packages"

        foreach ($namespace in @("jwplc_local", "jwplc")) {
            $espX32Root = Join-Path $packagesRoot ($namespace + "\tools\esp-x32")
            if (-not (Test-Path -LiteralPath $espX32Root)) {
                continue
            }

            $preferredRoot = Join-Path $espX32Root "2601"
            if (Test-Path -LiteralPath $preferredRoot) {
                $preferred = Get-ChildItem -LiteralPath $preferredRoot -Recurse -File -Filter "xtensa-esp32-elf-gcc-ar.exe" -ErrorAction SilentlyContinue | Select-Object -First 1
                if ($null -ne $preferred) {
                    Write-Host "ARCHIVER_RESOLUTION=ARDUINO15_ESP_X32_2601"
                    Write-Host "ARCHIVER_NAMESPACE=$namespace"
                    Write-Host "ARCHIVER_TOOL_VERSION=2601"
                    return $preferred.FullName
                }
            }

            $found = Get-ChildItem -LiteralPath $espX32Root -Recurse -File -Filter "xtensa-esp32-elf-gcc-ar.exe" -ErrorAction SilentlyContinue | Sort-Object LastWriteTime -Descending | Select-Object -First 1
            if ($null -ne $found) {
                Write-Host "ARCHIVER_RESOLUTION=ARDUINO15_ESP_X32_FALLBACK"
                Write-Host "ARCHIVER_NAMESPACE=$namespace"
                return $found.FullName
            }
        }
    }

    throw "H3E1B2_ARCHIVER_NOT_FOUND"
}

function Invoke-NativeCaptured {
    param(
        [Parameter(Mandatory = $true)]
        [string]$FilePath,

        [Parameter(Mandatory = $true)]
        [string[]]$Arguments
    )

    $oldPreference = $ErrorActionPreference
    try {
        $ErrorActionPreference = "Continue"
        [object[]]$lines = @(& $FilePath @Arguments 2>&1)
        $exitCode = [int]$LASTEXITCODE
    }
    finally {
        $ErrorActionPreference = $oldPreference
    }

    return [PSCustomObject]@{
        ExitCode = $exitCode
        Lines = $lines
    }
}

Write-Host ""
Write-Host "=== SOURCE SETUP STATIC PREFLIGHT ==="

[object[]]$sourcePreflight = @(
    & $sourceSetup -MasterPort $MasterPort -SlavePort $SlavePort -DisplayLinkage SOURCE -PreflightOnly -AllowDirtyCoreCandidate *>&1
)

$sourcePreflight | ForEach-Object { Write-Host $_ }

$sourcePreflightText = ($sourcePreflight | ForEach-Object { $_.ToString() }) -join [Environment]::NewLine

if (-not $sourcePreflightText.Contains("A14_H3E1B1_PREFLIGHT_ONLY=PASS")) {
    throw "H3E1B2_SOURCE_SETUP_PREFLIGHT_FAILED"
}
if (-not $sourcePreflightText.Contains("DISPLAY_LINKAGE=SOURCE")) {
    throw "H3E1B2_SOURCE_SETUP_LINKAGE_MISMATCH"
}

Write-Host ""
Write-Host "=== ARCHIVER STATIC PREFLIGHT ==="

$archiverPreflight = Resolve-ArchiverFromLog -LogPath (Join-Path $env:TEMP "__jwplc_h3e1b2_no_compile_log__")
Write-Host "H3E1B2_ARCHIVER_PREFLIGHT=$archiverPreflight"

$archiverVersion = Invoke-NativeCaptured -FilePath $archiverPreflight -Arguments @("--version")
$archiverVersion.Lines | Select-Object -First 3 | ForEach-Object { Write-Host "ARCHIVER_VERSION=$_" }

if ($archiverVersion.ExitCode -ne 0) {
    throw "H3E1B2_ARCHIVER_PREFLIGHT_EXEC_FAILED"
}

if ([IO.Path]::GetFileName($archiverPreflight) -ne "xtensa-esp32-elf-gcc-ar.exe") {
    throw "H3E1B2_ARCHIVER_PREFLIGHT_FILENAME_INVALID"
}

Write-Host "H3E1B2_ARCHIVER_PREFLIGHT=PASS"
Write-Host "H3E1B2_STATIC_PREFLIGHT=PASS"
Write-Host "CANDIDATE_MUTATES_REPO_ARCHIVE=NO"

if ($PreflightOnly) {
    Write-Host "PREFLIGHT_COMPILES=NO"
    Write-Host "PREFLIGHT_UPLOADS=NO"
    Write-Host "PREFLIGHT_GENERATES_CANDIDATE=NO"
    Write-Output "A14_H3E1B2_CANDIDATE_PREFLIGHT_ONLY=PASS"
    return
}

$candidateRoot = Join-Path $env:TEMP "jwplc_a14_h3e1b2_candidate_current"

if (Test-Path -LiteralPath $candidateRoot) {
    Remove-Item -LiteralPath $candidateRoot -Recurse -Force
}

New-Item -ItemType Directory -Force -Path $candidateRoot | Out-Null

$candidatePath = Join-Path $candidateRoot "libJWPLC_Display.candidate.a"
$manifestPath = Join-Path $candidateRoot "candidate_manifest.json"
$extractRoot = Join-Path $candidateRoot "members"

New-Item -ItemType Directory -Force -Path $extractRoot | Out-Null

Write-Host ""
Write-Host "=== RESOLVE VALIDATED SOURCE OBJECTS ==="

$sourceSetupRoot = $null

if (-not [string]::IsNullOrWhiteSpace($ReuseSourceSetupRoot)) {
    $sourceSetupRoot = [IO.Path]::GetFullPath($ReuseSourceSetupRoot)
    Write-Host "SOURCE_OBJECT_ORIGIN=REUSE_EXISTING_SETUP"
    Write-Host "SOURCE_SETUP_ROOT=$sourceSetupRoot"

    if (-not (Test-Path -LiteralPath $sourceSetupRoot)) {
        throw "H3E1B2_REUSE_SOURCE_SETUP_ROOT_MISSING"
    }
}
else {
    Write-Host "SOURCE_OBJECT_ORIGIN=NEW_SOURCE_SETUP"

    [object[]]$setupLines = @(
        & $sourceSetup -MasterPort $MasterPort -SlavePort $SlavePort -DisplayLinkage SOURCE -SetupOnly -AllowDirtyCoreCandidate *>&1
    )

    $setupLines | ForEach-Object { Write-Host $_ }
    $setupText = ($setupLines | ForEach-Object { $_.ToString() }) -join [Environment]::NewLine

    if (-not $setupText.Contains("A14_H3E1B1_SETUP_ONLY=PASS")) {
        throw "H3E1B2_SOURCE_SETUP_FAILED"
    }
    if (-not $setupText.Contains("DISPLAY_LINKAGE_PROOF=SOURCE_OBJECTS_PRESENT")) {
        throw "H3E1B2_SOURCE_OBJECT_PROOF_MISSING"
    }
    if (-not $setupText.Contains("H3E1B1_DISPLAY_ARCHIVE_RESTORED=YES")) {
        throw "H3E1B2_HISTORICAL_ARCHIVE_RESTORE_CONFIRMATION_MISSING"
    }

    $rootMatch = [regex]::Match($setupText, '(?m)^TEMP_ROOT=(.+?)\r?$')
    if (-not $rootMatch.Success) {
        throw "H3E1B2_SOURCE_TEMP_ROOT_MISSING"
    }

    $sourceSetupRoot = $rootMatch.Groups[1].Value.Trim()
}

$sourceBuild = Join-Path $sourceSetupRoot "build_master_source_core"
$sourceCompileLog = Join-Path $sourceSetupRoot "compile_master_source_core.log"
if (-not (Test-Path -LiteralPath $sourceBuild)) {
    throw "H3E1B2_SOURCE_BUILD_MISSING"
}
if (-not (Test-Path -LiteralPath $sourceCompileLog)) {
    throw "H3E1B2_SOURCE_COMPILE_LOG_MISSING"
}

Write-Host "SOURCE_BUILD=$sourceBuild"
Write-Host "SOURCE_COMPILE_LOG=$sourceCompileLog"

[string[]]$objectPaths = @()
[hashtable]$objectHashes = @{}

foreach ($sourceName in $sourceNames) {
    $objectName = $sourceName + ".o"

    [object[]]$matches = @(
        Get-ChildItem -LiteralPath $sourceBuild -Recurse -File -ErrorAction SilentlyContinue |
            Where-Object { $_.Name -eq $objectName }
    )

    Write-Host "SOURCE_OBJECT $objectName=$($matches.Count)"

    if ($matches.Count -ne 1) {
        throw "H3E1B2_SOURCE_OBJECT_CARDINALITY_INVALID=$objectName"
    }

    $objectPath = $matches[0].FullName
    $objectPaths += $objectPath
    $objectHashes[$objectName] = (Get-FileHash -LiteralPath $objectPath -Algorithm SHA256).Hash.ToUpperInvariant()
}

if ($objectPaths.Count -ne $sourceNames.Count) {
    throw "H3E1B2_SOURCE_OBJECT_COUNT_MISMATCH"
}

$archiver = Resolve-ArchiverFromLog -LogPath $sourceCompileLog
Write-Host "ARCHIVER=$archiver"

if (Test-Path -LiteralPath $candidatePath) {
    Remove-Item -LiteralPath $candidatePath -Force
}

Write-Host ""
Write-Host "=== BUILD CANDIDATE ARCHIVE ==="

$arResult = Invoke-NativeCaptured -FilePath $archiver -Arguments (@("crs", $candidatePath) + $objectPaths)
$arResult.Lines | ForEach-Object { Write-Host $_ }

if ($arResult.ExitCode -ne 0 -or -not (Test-Path -LiteralPath $candidatePath)) {
    throw "H3E1B2_CANDIDATE_BUILD_FAILED"
}

$listResult = Invoke-NativeCaptured -FilePath $archiver -Arguments @("t", $candidatePath)

if ($listResult.ExitCode -ne 0) {
    throw "H3E1B2_CANDIDATE_LIST_FAILED"
}

[string[]]$candidateMembers = @(
    $listResult.Lines |
        ForEach-Object { $_.ToString().Trim() } |
        Where-Object { -not [string]::IsNullOrWhiteSpace($_) }
)

[string[]]$expectedMembers = @($sourceNames | ForEach-Object { $_ + ".o" })
[object[]]$memberDiff = @(Compare-Object -ReferenceObject $expectedMembers -DifferenceObject $candidateMembers)

Write-Host "CANDIDATE_MEMBER_COUNT=$($candidateMembers.Count)"
$candidateMembers | ForEach-Object { Write-Host "CANDIDATE_MEMBER=$_" }

if ($memberDiff.Count -ne 0) {
    $memberDiff | ForEach-Object { Write-Host "CANDIDATE_MEMBER_DIFF=$_" }
    throw "H3E1B2_CANDIDATE_MEMBER_SET_INVALID"
}

$oldLocation = Get-Location
try {
    Set-Location $extractRoot
    $extractResult = Invoke-NativeCaptured -FilePath $archiver -Arguments @("x", $candidatePath)
}
finally {
    Set-Location $oldLocation
}

if ($extractResult.ExitCode -ne 0) {
    throw "H3E1B2_CANDIDATE_EXTRACT_FAILED"
}

foreach ($member in $expectedMembers) {
    $extractedPath = Join-Path $extractRoot $member

    if (-not (Test-Path -LiteralPath $extractedPath)) {
        throw "H3E1B2_EXTRACTED_MEMBER_MISSING=$member"
    }

    $sourceHash = $objectHashes[$member]
    $extractedHash = (Get-FileHash -LiteralPath $extractedPath -Algorithm SHA256).Hash.ToUpperInvariant()

    if ($sourceHash -ne $extractedHash) {
        throw "H3E1B2_MEMBER_BYTE_PARITY_FAILED=$member"
    }
}

$candidateFile = Get-Item -LiteralPath $candidatePath
$candidateHash = (Get-FileHash -LiteralPath $candidatePath -Algorithm SHA256).Hash.ToUpperInvariant()

$manifest = [ordered]@{
    schema = "jwplc-a14-h3e1b2-display-candidate-v1"
    branch = (Get-G2Branch)
    head = (Get-G2Head)
    historical_display_sha256 = $historicalDisplayHash
    candidate_sha256 = $candidateHash
    candidate_bytes = [int64]$candidateFile.Length
    source_tu_count = [int]$sourceNames.Count
    source_tus = $sourceNames
    source_object_sha256 = $objectHashes
    source_setup_root = $sourceSetupRoot
    source_object_origin = $(
        if ([string]::IsNullOrWhiteSpace($ReuseSourceSetupRoot)) {
            "NEW_SOURCE_SETUP"
        }
        else {
            "REUSE_EXISTING_SETUP"
        }
    )
    archiver = $archiver
}

$manifestJson = $manifest | ConvertTo-Json -Depth 8

[System.IO.File]::WriteAllText(
    $manifestPath,
    $manifestJson,
    (New-Object System.Text.UTF8Encoding($false))
)

if ((Get-G2Sha256 $displayArchiveRelative) -ne $historicalDisplayHash) {
    throw "H3E1B2_HISTORICAL_ARCHIVE_CHANGED_DURING_GENERATION"
}
if ((Get-G2Sha256 $script:G2CoreRelative) -ne $coreHash) {
    throw "H3E1B2_CORE_CHANGED_DURING_GENERATION"
}
if ((Get-G2Sha256 $rtuArchiveRelative) -ne $rtuHash) {
    throw "H3E1B2_RTU_ARCHIVE_CHANGED_DURING_GENERATION"
}

Write-Host ""
Write-Host "H3E1B2_CANDIDATE_PATH=$candidatePath"
Write-Host "H3E1B2_CANDIDATE_MANIFEST=$manifestPath"
Write-Host "H3E1B2_CANDIDATE_SHA256=$candidateHash"
Write-Host "H3E1B2_CANDIDATE_BYTES=$($candidateFile.Length)"
Write-Host "H3E1B2_CANDIDATE_SOURCE_TUS=$($sourceNames.Count)"
Write-Host "H3E1B2_CANDIDATE_MEMBERS_EXACT=PASS"
Write-Host "H3E1B2_CANDIDATE_MEMBER_BYTE_PARITY=PASS"
Write-Host "H3E1B2_HISTORICAL_ARCHIVE_PRESERVED=YES"
Write-Output "A14_H3E1B2_CANDIDATE_GENERATION=PASS"
