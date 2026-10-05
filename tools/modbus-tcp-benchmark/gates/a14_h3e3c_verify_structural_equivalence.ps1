param(
    [Parameter(Mandatory = $true)]
    [string]$SourceBuild,

    [Parameter(Mandatory = $true)]
    [string]$CandidateBuild,

    [Parameter(Mandatory = $true)]
    [string]$SourceLog,

    [Parameter(Mandatory = $true)]
    [string]$CandidateLog,

    [Parameter(Mandatory = $true)]
    [string]$Archiver,

    [Parameter(Mandatory = $true)]
    [int64]$FlashDelta
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"
. (Join-Path $PSScriptRoot "common.ps1")

foreach ($required in @(
    $SourceBuild,
    $CandidateBuild,
    $SourceLog,
    $CandidateLog,
    $Archiver
)) {
    if (-not (Test-Path -LiteralPath $required)) {
        throw "H3E3C_STRUCT_REQUIRED_PATH_MISSING=$required"
    }
}

function Get-H3E3CStructSingleFile {
    param(
        [string]$Root,
        [string]$Filter,
        [string]$Kind
    )

    [object[]]$files = @(
        Get-ChildItem -LiteralPath $Root -File -Filter $Filter -ErrorAction SilentlyContinue
    )

    if ($files.Count -ne 1) {
        throw "H3E3C_STRUCT_$($Kind)_COUNT_INVALID=$($files.Count)"
    }

    return $files[0].FullName
}

function Get-H3E3CStructLibraryNames {
    param([string]$LogPath)

    [string[]]$names = @(
        Get-Content -LiteralPath $LogPath |
            ForEach-Object {
                $line = [string]$_

                if ($line -match '^Using library (?<name>.+?) at version ') {
                    $name = $Matches["name"].Trim()

                    if ($name -ne "JWPLC_Display") {
                        $name
                    }
                }
            } |
            Sort-Object -Unique
    )

    return $names
}

function Get-H3E3CStructExternalObjects {
    param([string]$BuildRoot)

    $libraryRoot =
        Join-Path $BuildRoot "libraries"

    $table = @{}

    if (-not (Test-Path -LiteralPath $libraryRoot)) {
        return $table
    }

    [object[]]$objects = @(
        Get-ChildItem -LiteralPath $libraryRoot -Recurse -File -Filter "*.o" -ErrorAction SilentlyContinue
    )

    foreach ($object in $objects) {
        $relative =
            $object.FullName.Substring(
                $libraryRoot.Length
            ).TrimStart([char[]]"\/")

        $normalized =
            $relative.Replace("\", "/")

        if ($normalized -match '^JWPLC_Display/') {
            continue
        }

        $table[$normalized] =
            (Get-G2Sha256Path $object.FullName).ToUpperInvariant()
    }

    return $table
}

function Get-H3E3CStructLinkedDisplayMembers {
    param([string]$MapPath)

    $mapText =
        [IO.File]::ReadAllText($MapPath)

    [string[]]$members = @(
        [regex]::Matches(
            $mapText,
            '(?:lib)?JWPLC_Display\.a\((?<member>[^)]+\.cpp\.o)\)'
        ) |
            ForEach-Object {
                $_.Groups["member"].Value
            } |
            Sort-Object -Unique
    )

    return $members
}

function Get-H3E3CStructNormalizedSymbols {
    param(
        [string]$NmPath,
        [string]$ElfPath
    )

    $previousPreference =
        $ErrorActionPreference

    try {
        $ErrorActionPreference =
            "Continue"

        [object[]]$nmOutput =
            @(& $NmPath "-S" "--defined-only" $ElfPath 2>&1)

        $nmExit =
            [int]$LASTEXITCODE
    }
    finally {
        $ErrorActionPreference =
            $previousPreference
    }

    if ($nmExit -ne 0) {
        throw "H3E3C_STRUCT_NM_FAILED=$ElfPath"
    }

    [string[]]$normalized = @(
        foreach ($lineObject in $nmOutput) {
            $line =
                ([string]$lineObject).Trim()

            if ($line -match '^[0-9A-Fa-f]+\s+(?<size>[0-9A-Fa-f]+)\s+(?<type>\S)\s+(?<name>.+)$') {
                $size =
                    $Matches["size"].ToUpperInvariant()

                $type =
                    $Matches["type"]

                $name =
                    $Matches["name"]

                "$size|$type|$name"
            }
        }
    )

    return @(
        $normalized |
            Sort-Object
    )
}

$sourceMap =
    Get-H3E3CStructSingleFile -Root $SourceBuild -Filter "*.map" -Kind "SOURCE_MAP"

$candidateMap =
    Get-H3E3CStructSingleFile -Root $CandidateBuild -Filter "*.map" -Kind "CANDIDATE_MAP"

[string[]]$sourceMembers =
    @(Get-H3E3CStructLinkedDisplayMembers -MapPath $sourceMap)

[string[]]$candidateMembers =
    @(Get-H3E3CStructLinkedDisplayMembers -MapPath $candidateMap)

[object[]]$memberDiff = @(
    Compare-Object -ReferenceObject $sourceMembers -DifferenceObject $candidateMembers
)

$memberParity =
    if ($sourceMembers.Count -gt 0 -and $memberDiff.Count -eq 0) {
        "PASS"
    }
    else {
        "FAIL"
    }

Write-Host "H3E3C_SOURCE_LINKED_DISPLAY_MEMBER_COUNT=$($sourceMembers.Count)"
Write-Host "H3E3C_CANDIDATE_LINKED_DISPLAY_MEMBER_COUNT=$($candidateMembers.Count)"
Write-Host "H3E3C_LINKED_DISPLAY_MEMBER_PARITY=$memberParity"

$sourceMembers |
    ForEach-Object {
        Write-Host "H3E3C_SOURCE_LINKED_DISPLAY_MEMBER=$_"
    }

$candidateMembers |
    ForEach-Object {
        Write-Host "H3E3C_CANDIDATE_LINKED_DISPLAY_MEMBER=$_"
    }

if ($memberParity -ne "PASS") {
    throw "H3E3C_LINKED_DISPLAY_MEMBER_PARITY_FAILED"
}

$sourceObjects =
    Get-H3E3CStructExternalObjects -BuildRoot $SourceBuild

$candidateObjects =
    Get-H3E3CStructExternalObjects -BuildRoot $CandidateBuild

[string[]]$sourceKeys = @(
    $sourceObjects.Keys |
        Sort-Object
)

[string[]]$candidateKeys = @(
    $candidateObjects.Keys |
        Sort-Object
)

[object[]]$objectSetDiff = @(
    Compare-Object -ReferenceObject $sourceKeys -DifferenceObject $candidateKeys
)

[string[]]$objectHashDiff = @(
    foreach ($key in $sourceKeys) {
        if ($candidateObjects.ContainsKey($key) -and
            $sourceObjects[$key] -ne $candidateObjects[$key]) {
            $key
        }
    }
)

$objectSetParity =
    if ($objectSetDiff.Count -eq 0) {
        "PASS"
    }
    else {
        "FAIL"
    }

Write-Host "H3E3C_EXTERNAL_OBJECT_SOURCE_COUNT=$($sourceKeys.Count)"
Write-Host "H3E3C_EXTERNAL_OBJECT_CANDIDATE_COUNT=$($candidateKeys.Count)"
Write-Host "H3E3C_EXTERNAL_OBJECT_SET_PARITY=$objectSetParity"
Write-Host "H3E3C_EXTERNAL_OBJECT_HASH_MISMATCH_COUNT=$($objectHashDiff.Count)"

if ($objectSetDiff.Count -ne 0 -or
    $objectHashDiff.Count -ne 0) {
    throw "H3E3C_EXTERNAL_OBJECT_PARITY_FAILED"
}

[string[]]$sourceLibraries =
    @(Get-H3E3CStructLibraryNames -LogPath $SourceLog)

[string[]]$candidateLibraries =
    @(Get-H3E3CStructLibraryNames -LogPath $CandidateLog)

[object[]]$libraryDiff = @(
    Compare-Object -ReferenceObject $sourceLibraries -DifferenceObject $candidateLibraries
)

$libraryParity =
    if ($libraryDiff.Count -eq 0) {
        "PASS"
    }
    else {
        "FAIL"
    }

Write-Host "H3E3C_NON_DISPLAY_LIBRARY_SOURCE_COUNT=$($sourceLibraries.Count)"
Write-Host "H3E3C_NON_DISPLAY_LIBRARY_CANDIDATE_COUNT=$($candidateLibraries.Count)"
Write-Host "H3E3C_NON_DISPLAY_LIBRARY_SELECTION_PARITY=$libraryParity"

if ($libraryParity -ne "PASS") {
    throw "H3E3C_LIBRARY_SELECTION_PARITY_FAILED"
}

$toolDir =
    Split-Path -Parent $Archiver

$nm = $null

foreach ($name in @(
    "xtensa-esp32-elf-nm.exe",
    "xtensa-esp32-elf-nm"
)) {
    $candidatePath =
        Join-Path $toolDir $name

    if (Test-Path -LiteralPath $candidatePath) {
        $nm =
            (Resolve-Path -LiteralPath $candidatePath).Path
        break
    }
}

if ([string]::IsNullOrWhiteSpace($nm)) {
    throw "H3E3C_STRUCT_NM_NOT_FOUND"
}

$sourceElf =
    Get-H3E3CStructSingleFile -Root $SourceBuild -Filter "*.elf" -Kind "SOURCE_ELF"

$candidateElf =
    Get-H3E3CStructSingleFile -Root $CandidateBuild -Filter "*.elf" -Kind "CANDIDATE_ELF"

[string[]]$sourceSymbols =
    @(Get-H3E3CStructNormalizedSymbols -NmPath $nm -ElfPath $sourceElf)

[string[]]$candidateSymbols =
    @(Get-H3E3CStructNormalizedSymbols -NmPath $nm -ElfPath $candidateElf)

[object[]]$symbolDiff = @(
    Compare-Object -ReferenceObject $sourceSymbols -DifferenceObject $candidateSymbols
)

$symbolParity =
    if ($symbolDiff.Count -eq 0) {
        "PASS"
    }
    else {
        "FAIL"
    }

Write-Host "H3E3C_NM=$nm"
Write-Host "H3E3C_SOURCE_DEFINED_SYMBOL_COUNT=$($sourceSymbols.Count)"
Write-Host "H3E3C_CANDIDATE_DEFINED_SYMBOL_COUNT=$($candidateSymbols.Count)"
Write-Host "H3E3C_DEFINED_SYMBOL_NAME_TYPE_SIZE_PARITY=$symbolParity"

if ($symbolParity -ne "PASS") {
    $symbolDiff |
        Select-Object -First 40 |
        ForEach-Object {
            Write-Host "H3E3C_SYMBOL_DIFF=$($_.SideIndicator):$($_.InputObject)"
        }

    throw "H3E3C_DEFINED_SYMBOL_PARITY_FAILED"
}

$flashClassification =
    if ($FlashDelta -eq 0) {
        "EXACT"
    }
    else {
        "LINK_LAYOUT_ONLY"
    }

Write-Host "H3E3C_FLASH_DELTA_CLASSIFICATION=$flashClassification"
Write-Host "H3E3C_STRUCTURAL_EQUIVALENCE=PASS"
