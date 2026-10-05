param(
    [string]$ResultRoot = "",
    [ValidateSet("Pending", "Active")]
    [string]$ExpectedActivationState = "Pending"
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$repo = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot "..\..\.."))
$expectedBranch = "v2.1.0-alpha.12/feature/modbus-tcp"

function Get-Sha256Lower {
    param([string]$Path)

    $stream = [IO.File]::OpenRead($Path)
    $sha256 = [Security.Cryptography.SHA256]::Create()

    try {
        return (
            [BitConverter]::ToString(
                $sha256.ComputeHash($stream)
            ).Replace("-", "").ToLowerInvariant()
        )
    }
    finally {
        $stream.Dispose()
        $sha256.Dispose()
    }
}

function Get-LastGitCommit {
    param([string]$RelativePath)

    [string[]]$lines = @(
        & git -C $repo log -1 --format="%ct|%H" -- $RelativePath
    )

    if ($LASTEXITCODE -ne 0) {
        throw ("A12_P7_GIT_LOG_FAILED=" + $RelativePath)
    }

    if ($lines.Count -ne 1 -or [string]::IsNullOrWhiteSpace($lines[0])) {
        throw ("A12_P7_GIT_LOG_EMPTY=" + $RelativePath)
    }

    $parts = $lines[0].Split("|")

    if ($parts.Count -ne 2) {
        throw ("A12_P7_GIT_LOG_PARSE_FAILED=" + $RelativePath)
    }

    return [pscustomobject]@{
        Epoch = [int64]$parts[0]
        Commit = $parts[1].Trim()
    }
}

function Get-LatestSourceCommit {
    param([string]$SourceRelativeRoot)

    [string[]]$trackedFiles = @(
        & git -C $repo ls-files -- $SourceRelativeRoot
    )

    if ($LASTEXITCODE -ne 0) {
        throw ("A12_P7_GIT_LS_FILES_FAILED=" + $SourceRelativeRoot)
    }

    [string[]]$sourceFiles = @(
        $trackedFiles |
            Where-Object {
                $_ -match '(?i)\.(c|cc|cpp|cxx|s|S|h|hh|hpp|hxx)$'
            }
    )

    if ($sourceFiles.Count -eq 0) {
        throw ("A12_P7_SOURCE_FILE_SET_EMPTY=" + $SourceRelativeRoot)
    }

    $latestEpoch = [int64]-1
    $latestCommit = ""
    $latestFile = ""

    foreach ($sourceFile in $sourceFiles) {
        $info = Get-LastGitCommit -RelativePath $sourceFile

        if ($info.Epoch -gt $latestEpoch) {
            $latestEpoch = $info.Epoch
            $latestCommit = $info.Commit
            $latestFile = $sourceFile
        }
    }

    return [pscustomobject]@{
        Epoch = $latestEpoch
        Commit = $latestCommit
        File = $latestFile
        FileCount = $sourceFiles.Count
    }
}

function Read-PropertyFlag {
    param(
        [string]$PropertiesPath,
        [string]$Key,
        [string]$ExpectedValue
    )

    if (-not (Test-Path -LiteralPath $PropertiesPath)) {
        return $false
    }

    [string[]]$lines = @(
        Get-Content -LiteralPath $PropertiesPath
    )

    foreach ($line in $lines) {
        if ($line.Trim() -ieq ($Key + "=" + $ExpectedValue)) {
            return $true
        }
    }

    return $false
}

$branch = (& git -C $repo branch --show-current).Trim()
$head = (& git -C $repo rev-parse HEAD).Trim()

if ($branch -ne $expectedBranch) {
    throw "A12_P7_BRANCH_MISMATCH"
}

[string[]]$dirty = @(& git -C $repo diff --name-only)
[string[]]$staged = @(& git -C $repo diff --cached --name-only)

if ($dirty.Count -ne 0) {
    $dirty | ForEach-Object { Write-Host ("ENTRY_DIRTY=" + $_) }
    throw "A12_P7_TRACKED_TREE_NOT_CLEAN"
}

if ($staged.Count -ne 0) {
    throw "A12_P7_INDEX_NOT_CLEAN"
}

$boardsPath = Join-Path $repo "JWPLC\2.1.0\boards.txt"

if (-not (Test-Path -LiteralPath $boardsPath)) {
    throw "A12_P7_BOARDS_TXT_MISSING"
}

$boardsText = [IO.File]::ReadAllText($boardsPath)
$coreStubEnabled = $boardsText.Contains("jwplcbasic.build.core=jwcontrol_precompiled_stub")
$coreArchiveLinked = $boardsText.Contains('jwplcbasic.build.extra_libs="{runtime.platform.path}/precompiled/core/JWPLCBASIC/core.a"')

Write-Host ("CORE_STUB_ENABLED=" + [string]$coreStubEnabled)
Write-Host ("CORE_ARCHIVE_LINKED=" + [string]$coreArchiveLinked)

if (-not $coreStubEnabled -or -not $coreArchiveLinked) {
    throw "A12_P7_CORE_PRECOMPILED_POLICY_NOT_ACTIVE"
}

[object[]]$entries = @(
    [pscustomobject]@{
        Name = "core"
        ArchiveRel = "JWPLC/2.1.0/precompiled/core/JWPLCBASIC/core.a"
        SourceRel = "JWPLC/2.1.0/cores/jwcontrol"
        PropertiesRel = ""
        ExpectedSha = "78d0c0ab14f156b96116529e88872340d51877af40d24ba3559f6081e0bf34fb"
        FinalPrecompiled = $true
        FinalDotA = $false
        ActivationPendingAllowed = $false
        Class = "REGENERATED"
    },
    [pscustomobject]@{
        Name = "JWPLC_ModbusRTU"
        ArchiveRel = "JWPLC/2.1.0/libraries/JWPLC_ModbusRTU/src/esp32/libJWPLC_ModbusRTU.a"
        SourceRel = "JWPLC/2.1.0/libraries/JWPLC_ModbusRTU/src"
        PropertiesRel = "JWPLC/2.1.0/libraries/JWPLC_ModbusRTU/library.properties"
        ExpectedSha = "424ed3f462bb57cce0019d690486e612d5f243c40ae62d44d3ad973b0a521085"
        FinalPrecompiled = $true
        FinalDotA = $false
        ActivationPendingAllowed = $true
        Class = "REGENERATED"
    },
    [pscustomobject]@{
        Name = "SPI"
        ArchiveRel = "JWPLC/2.1.0/libraries/SPI/src/esp32/libSPI.a"
        SourceRel = "JWPLC/2.1.0/libraries/SPI/src"
        PropertiesRel = "JWPLC/2.1.0/libraries/SPI/library.properties"
        ExpectedSha = "b433758b746380bf8d1ea102aca1d1637b5a8cab50616b56024f4b022a916445"
        FinalPrecompiled = $true
        FinalDotA = $false
        ActivationPendingAllowed = $true
        Class = "REGENERATED"
    },
    [pscustomobject]@{
        Name = "JW_SD"
        ArchiveRel = "JWPLC/2.1.0/libraries/JW_SD/src/esp32/libJW_SD.a"
        SourceRel = "JWPLC/2.1.0/libraries/JW_SD/src"
        PropertiesRel = "JWPLC/2.1.0/libraries/JW_SD/library.properties"
        ExpectedSha = "1b9619ba37295782ade1edc6f06439a38e7e534fc08f5b05757c9af7a645acc0"
        FinalPrecompiled = $true
        FinalDotA = $false
        ActivationPendingAllowed = $true
        Class = "REGENERATED"
    },
    [pscustomobject]@{
        Name = "JWPLC_Display"
        ArchiveRel = "JWPLC/2.1.0/libraries/JWPLC_Display/src/esp32/libJWPLC_Display.a"
        SourceRel = "JWPLC/2.1.0/libraries/JWPLC_Display/src"
        PropertiesRel = "JWPLC/2.1.0/libraries/JWPLC_Display/library.properties"
        ExpectedSha = "c960d718433e29a40e3cc55bc745c9a2e121ee1ee598ec72c04872327592dc02"
        FinalPrecompiled = $true
        FinalDotA = $true
        ActivationPendingAllowed = $true
        Class = "REGENERATED"
    },
    [pscustomobject]@{
        Name = "JWPLC_TFT"
        ArchiveRel = "JWPLC/2.1.0/libraries/JWPLC_TFT/src/esp32/libJWPLC_TFT.a"
        SourceRel = "JWPLC/2.1.0/libraries/JWPLC_TFT/src"
        PropertiesRel = "JWPLC/2.1.0/libraries/JWPLC_TFT/library.properties"
        ExpectedSha = "5d860a131811dd9a7eb6fa55f5674b1d78b0de7dfaf8748ce18a60ceed2d3738"
        FinalPrecompiled = $true
        FinalDotA = $true
        ActivationPendingAllowed = $true
        Class = "REQUALIFIED"
    },
    [pscustomobject]@{
        Name = "Adafruit_BusIO"
        ArchiveRel = "JWPLC/2.1.0/libraries/Adafruit_BusIO/src/esp32/libAdafruit_BusIO.a"
        SourceRel = "JWPLC/2.1.0/libraries/Adafruit_BusIO/src"
        PropertiesRel = "JWPLC/2.1.0/libraries/Adafruit_BusIO/library.properties"
        ExpectedSha = ""
        FinalPrecompiled = $true
        FinalDotA = $false
        ActivationPendingAllowed = $false
        Class = "RETAINED"
    },
    [pscustomobject]@{
        Name = "FS"
        ArchiveRel = "JWPLC/2.1.0/libraries/FS/src/esp32/libFS.a"
        SourceRel = "JWPLC/2.1.0/libraries/FS/src"
        PropertiesRel = "JWPLC/2.1.0/libraries/FS/library.properties"
        ExpectedSha = ""
        FinalPrecompiled = $true
        FinalDotA = $false
        ActivationPendingAllowed = $false
        Class = "RETAINED"
    },
    [pscustomobject]@{
        Name = "JW_FRAM"
        ArchiveRel = "JWPLC/2.1.0/libraries/JW_FRAM/src/esp32/libJW_FRAM.a"
        SourceRel = "JWPLC/2.1.0/libraries/JW_FRAM/src"
        PropertiesRel = "JWPLC/2.1.0/libraries/JW_FRAM/library.properties"
        ExpectedSha = ""
        FinalPrecompiled = $true
        FinalDotA = $false
        ActivationPendingAllowed = $false
        Class = "RETAINED"
    },
    [pscustomobject]@{
        Name = "JW_MatrixButtons"
        ArchiveRel = "JWPLC/2.1.0/libraries/JW_MatrixButtons/src/esp32/libJW_MatrixButtons.a"
        SourceRel = "JWPLC/2.1.0/libraries/JW_MatrixButtons/src"
        PropertiesRel = "JWPLC/2.1.0/libraries/JW_MatrixButtons/library.properties"
        ExpectedSha = ""
        FinalPrecompiled = $true
        FinalDotA = $false
        ActivationPendingAllowed = $false
        Class = "RETAINED"
    },
    [pscustomobject]@{
        Name = "SD"
        ArchiveRel = "JWPLC/2.1.0/libraries/SD/src/esp32/libSD.a"
        SourceRel = "JWPLC/2.1.0/libraries/SD/src"
        PropertiesRel = "JWPLC/2.1.0/libraries/SD/library.properties"
        ExpectedSha = ""
        FinalPrecompiled = $true
        FinalDotA = $false
        ActivationPendingAllowed = $false
        Class = "RETAINED"
    },
    [pscustomobject]@{
        Name = "Wire"
        ArchiveRel = "JWPLC/2.1.0/libraries/Wire/src/esp32/libWire.a"
        SourceRel = "JWPLC/2.1.0/libraries/Wire/src"
        PropertiesRel = "JWPLC/2.1.0/libraries/Wire/library.properties"
        ExpectedSha = ""
        FinalPrecompiled = $true
        FinalDotA = $false
        ActivationPendingAllowed = $false
        Class = "RETAINED"
    }
)

[object[]]$sourceOnlyEntries = @(
    [pscustomobject]@{
        Name = "JW_RTC"
        PropertiesRel = "JWPLC/2.1.0/libraries/JW_RTC/library.properties"
        UnexpectedArchiveRel = "JWPLC/2.1.0/libraries/JW_RTC/src/esp32/libJW_RTC.a"
        Decision = "SOURCE_ONLY_INTENTIONAL_RTC_AUDIT"
    },
    [pscustomobject]@{
        Name = "JWPLC_GlobalPeripherals"
        PropertiesRel = "JWPLC/2.1.0/libraries/JWPLC_GlobalPeripherals/library.properties"
        UnexpectedArchiveRel = "JWPLC/2.1.0/libraries/JWPLC_GlobalPeripherals/src/esp32/libJWPLC_GlobalPeripherals.a"
        Decision = "SOURCE_ONLY_INTENTIONAL_P4_NOT_ADOPTED"
    },
    [pscustomobject]@{
        Name = "JWPLC_Ethernet"
        PropertiesRel = "JWPLC/2.1.0/libraries/JWPLC_Ethernet/library.properties"
        UnexpectedArchiveRel = "JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/esp32/libJWPLC_Ethernet.a"
        Decision = "SOURCE_ONLY_CURRENT_ALPHA12_IMPLEMENTATION"
    },
    [pscustomobject]@{
        Name = "JWPLC_RS485"
        PropertiesRel = "JWPLC/2.1.0/libraries/JWPLC_RS485/library.properties"
        UnexpectedArchiveRel = "JWPLC/2.1.0/libraries/JWPLC_RS485/src/esp32/libJWPLC_RS485.a"
        Decision = "SOURCE_ONLY_CURRENT_ALPHA12_IMPLEMENTATION"
    }
)

$stamp = Get-Date -Format "yyyyMMdd_HHmmss"

if ([string]::IsNullOrWhiteSpace($ResultRoot)) {
    $ResultRoot = Join-Path $repo ("tools\alpha12\results\precompiled_global_audit_" + $stamp)
}

New-Item -ItemType Directory -Force -Path $ResultRoot | Out-Null

[object[]]$rows = @()
[string[]]$activationPending = @()
[string[]]$failures = @()

[object[]]$sourceOnlyRows = @()

foreach ($sourceOnlyEntry in $sourceOnlyEntries) {
    $propertiesPath = Join-Path $repo $sourceOnlyEntry.PropertiesRel
    $unexpectedArchivePath = Join-Path $repo $sourceOnlyEntry.UnexpectedArchiveRel

    if (-not (Test-Path -LiteralPath $propertiesPath)) {
        $failures += ("SOURCE_ONLY_PROPERTIES_MISSING:" + $sourceOnlyEntry.Name)
        continue
    }

    $precompiledFull = Read-PropertyFlag -PropertiesPath $propertiesPath -Key "precompiled" -ExpectedValue "full"
    $unexpectedArchiveExists = Test-Path -LiteralPath $unexpectedArchivePath

    if ($precompiledFull) {
        $failures += ("SOURCE_ONLY_PRECOMPILED_FLAG_PRESENT:" + $sourceOnlyEntry.Name)
    }

    if ($unexpectedArchiveExists) {
        $failures += ("SOURCE_ONLY_UNEXPECTED_ARCHIVE_PRESENT:" + $sourceOnlyEntry.Name)
    }

    $sourceOnlyRows += [pscustomobject]@{
        Name = $sourceOnlyEntry.Name
        Decision = $sourceOnlyEntry.Decision
        PrecompiledFull = $precompiledFull
        UnexpectedArchiveExists = $unexpectedArchiveExists
        PolicyPass = (-not $precompiledFull -and -not $unexpectedArchiveExists)
    }
}

Write-Host "=============================================================================="
Write-Host " ALPHA12 - P7A GLOBAL PRECOMPILED ARCHIVE AUDIT"
Write-Host "=============================================================================="
Write-Host ("BRANCH=" + $branch)
Write-Host ("HEAD=" + $head)
Write-Host ("RESULT_ROOT=" + $ResultRoot)
Write-Host ("ARCHIVE_INVENTORY_COUNT=" + [string]$entries.Count)
Write-Host ("EXPECTED_ACTIVATION_STATE=" + $ExpectedActivationState)

foreach ($entry in $entries) {
    if (-not [string]::IsNullOrWhiteSpace($entry.ExpectedSha)) {
        $expectedShaFormatPass = (
            $entry.ExpectedSha.Length -eq 64 -and
            $entry.ExpectedSha -match '^[0-9a-fA-F]+$'
        )

        if (-not $expectedShaFormatPass) {
            throw ("A12_P7_EXPECTED_SHA_LITERAL_INVALID:" + $entry.Name + ":" + $entry.ExpectedSha)
        }
    }

    $archivePath = Join-Path $repo $entry.ArchiveRel

    if (-not (Test-Path -LiteralPath $archivePath)) {
        $failures += ("MISSING_ARCHIVE:" + $entry.Name)
        continue
    }

    $archiveSha = Get-Sha256Lower -Path $archivePath
    $archiveBytes = (Get-Item -LiteralPath $archivePath).Length

    $expectedShaPass = $true

    if (-not [string]::IsNullOrWhiteSpace($entry.ExpectedSha)) {
        $expectedShaPass = $archiveSha -eq $entry.ExpectedSha

        if (-not $expectedShaPass) {
            $failures += ("EXPECTED_SHA_MISMATCH:" + $entry.Name)
        }
    }

    $archiveCommit = Get-LastGitCommit -RelativePath $entry.ArchiveRel
    $sourceCommit = Get-LatestSourceCommit -SourceRelativeRoot $entry.SourceRel
    $sourceFreshnessPass = $archiveCommit.Epoch -ge $sourceCommit.Epoch

    if (-not $sourceFreshnessPass) {
        $failures += ("SOURCE_NEWER_THAN_ARCHIVE:" + $entry.Name)
    }

    $precompiledFull = $true
    $dotALinkage = $false
    $activationState = "CORE_ACTIVE"

    if (-not [string]::IsNullOrWhiteSpace($entry.PropertiesRel)) {
        $propertiesPath = Join-Path $repo $entry.PropertiesRel

        if (-not (Test-Path -LiteralPath $propertiesPath)) {
            $failures += ("MISSING_PROPERTIES:" + $entry.Name)
            continue
        }

        $precompiledFull = Read-PropertyFlag -PropertiesPath $propertiesPath -Key "precompiled" -ExpectedValue "full"
        $dotALinkage = Read-PropertyFlag -PropertiesPath $propertiesPath -Key "dot_a_linkage" -ExpectedValue "true"

        if ($entry.FinalPrecompiled -and -not $precompiledFull) {
            if ($entry.ActivationPendingAllowed) {
                $activationState = "ACTIVATION_PENDING"
                $activationPending += $entry.Name
            }
            else {
                $activationState = "UNEXPECTED_INACTIVE"
                $failures += ("PRECOMPILED_FULL_MISSING:" + $entry.Name)
            }
        }
        else {
            $activationState = "ACTIVE"
        }

        if ($entry.FinalDotA -and -not $dotALinkage) {
            if ($entry.ActivationPendingAllowed) {
                if (-not $activationPending.Contains($entry.Name)) {
                    $activationPending += $entry.Name
                }
                $activationState = "ACTIVATION_PENDING"
            }
            else {
                $failures += ("DOT_A_LINKAGE_MISSING:" + $entry.Name)
            }
        }
    }

    $row = [pscustomobject]@{
        Name = $entry.Name
        Class = $entry.Class
        ArchiveBytes = $archiveBytes
        ArchiveSha256 = $archiveSha
        ExpectedShaChecked = -not [string]::IsNullOrWhiteSpace($entry.ExpectedSha)
        ExpectedShaPass = $expectedShaPass
        ArchiveCommit = $archiveCommit.Commit
        ArchiveCommitEpoch = $archiveCommit.Epoch
        LatestSourceCommit = $sourceCommit.Commit
        LatestSourceEpoch = $sourceCommit.Epoch
        LatestSourceFile = $sourceCommit.File
        SourceFileCount = $sourceCommit.FileCount
        SourceFreshnessPass = $sourceFreshnessPass
        PrecompiledFull = $precompiledFull
        DotALinkage = $dotALinkage
        ActivationState = $activationState
    }

    $rows += $row

    Write-Host ""
    Write-Host ("ARCHIVE_NAME=" + $entry.Name)
    Write-Host ("ARCHIVE_CLASS=" + $entry.Class)
    Write-Host ("ARCHIVE_BYTES=" + [string]$archiveBytes)
    Write-Host ("ARCHIVE_SHA256=" + $archiveSha)
    Write-Host ("EXPECTED_SHA_PASS=" + [string]$expectedShaPass)
    Write-Host ("SOURCE_FRESHNESS_PASS=" + [string]$sourceFreshnessPass)
    Write-Host ("PRECOMPILED_FULL=" + [string]$precompiledFull)
    Write-Host ("DOT_A_LINKAGE=" + [string]$dotALinkage)
    Write-Host ("ACTIVATION_STATE=" + $activationState)
}

$csvPath = Join-Path $ResultRoot "archive_inventory.csv"
$rows | Export-Csv -LiteralPath $csvPath -NoTypeInformation -Encoding UTF8

$activationPending = @($activationPending | Sort-Object -Unique)
$failures = @($failures | Sort-Object -Unique)

Write-Host ""
Write-Host ("SOURCE_ONLY_POLICY_COUNT=" + [string]$sourceOnlyRows.Count)
$sourceOnlyRows | ForEach-Object {
    Write-Host ("SOURCE_ONLY=" + $_.Name + " DECISION=" + $_.Decision + " PASS=" + [string]$_.PolicyPass)
}

Write-Host ("AUDITED_ARCHIVE_COUNT=" + [string]$rows.Count)
Write-Host ("ACTIVATION_PENDING_COUNT=" + [string]$activationPending.Count)
$activationPending | ForEach-Object { Write-Host ("ACTIVATION_PENDING=" + $_) }
Write-Host ("FAILURE_COUNT=" + [string]$failures.Count)
$failures | ForEach-Object { Write-Host ("P7_FAILURE=" + $_) }

if ($rows.Count -ne $entries.Count) {
    throw "A12_P7_ARCHIVE_INVENTORY_INCOMPLETE"
}

if ($sourceOnlyRows.Count -ne $sourceOnlyEntries.Count) {
    throw "A12_P7_SOURCE_ONLY_INVENTORY_INCOMPLETE"
}

if ($failures.Count -ne 0) {
    throw "A12_P7_GLOBAL_ARCHIVE_AUDIT_FAILED"
}

[string[]]$expectedPending = @(
    "JWPLC_Display",
    "JWPLC_ModbusRTU",
    "JWPLC_TFT",
    "JW_SD",
    "SPI"
) | Sort-Object

if ($ExpectedActivationState -eq "Pending") {
    if (Compare-Object -ReferenceObject $expectedPending -DifferenceObject $activationPending) {
        Write-Host ("EXPECTED_ACTIVATION_PENDING=" + ($expectedPending -join ","))
        Write-Host ("ACTUAL_ACTIVATION_PENDING=" + ($activationPending -join ","))
        throw "A12_P7_ACTIVATION_PENDING_SET_UNEXPECTED"
    }
}
else {
    if ($activationPending.Count -ne 0) {
        Write-Host ("EXPECTED_ACTIVATION_PENDING=")
        Write-Host ("ACTUAL_ACTIVATION_PENDING=" + ($activationPending -join ","))
        throw "A12_P7_POST_FREEZE_ACTIVATION_NOT_ACTIVE"
    }

    [string[]]$notActive = @(
        $rows |
            Where-Object { $_.ActivationState -notin @("ACTIVE", "CORE_ACTIVE") } |
            ForEach-Object { $_.Name }
    )

    if ($notActive.Count -ne 0) {
        Write-Host ("P7_POST_FREEZE_NOT_ACTIVE=" + ($notActive -join ","))
        throw "A12_P7_POST_FREEZE_POLICY_NOT_ACTIVE"
    }
}

[string[]]$finalDirty = @(& git -C $repo diff --name-only)
[string[]]$finalStaged = @(& git -C $repo diff --cached --name-only)

Write-Host ("FINAL_TRACKED_DIRTY_COUNT=" + [string]$finalDirty.Count)
Write-Host ("FINAL_STAGED_COUNT=" + [string]$finalStaged.Count)

if ($finalDirty.Count -ne 0 -or $finalStaged.Count -ne 0) {
    throw "A12_P7_REPOSITORY_MUTATED"
}

@(
    "ALPHA12_P7A_GLOBAL_PRECOMPILED_ARCHIVE_AUDIT=PASS"
    "BRANCH=$branch"
    "HEAD=$head"
    "AUDITED_ARCHIVE_COUNT=$($rows.Count)"
    "KNOWN_REGENERATED_IDENTITIES=PASS"
    "SOURCE_FRESHNESS=PASS"
    "RETAINED_ARCHIVE_POLICY=PASS"
    "SOURCE_ONLY_POLICY=PASS"
    "SOURCE_ONLY_POLICY_COUNT=$($sourceOnlyRows.Count)"
    "SOURCE_ONLY=$($sourceOnlyRows.Name -join ',')"
    "CORE_PRECOMPILED_POLICY=PASS"
    "ACTIVATION_PENDING_COUNT=$($activationPending.Count)"
    "ACTIVATION_PENDING=$($activationPending -join ',')"
    "PRECOMPILED_FREEZE=NOT_YET"
    "NEXT=P7B_RELEASE_LIKE_PRECOMPILED_ACTIVATION"
    "FINAL_TRACKED_DIRTY_COUNT=0"
    "RESULT_ROOT=$ResultRoot"
) | Set-Content -LiteralPath (Join-Path $ResultRoot "SUMMARY.txt") -Encoding UTF8

Write-Host ""
Write-Host "=============================================================================="
Write-Host "ALPHA12_P7A_GLOBAL_PRECOMPILED_ARCHIVE_AUDIT=PASS"
Write-Host ("AUDITED_ARCHIVE_COUNT=" + [string]$rows.Count)
Write-Host "KNOWN_REGENERATED_IDENTITIES=PASS"
Write-Host "SOURCE_FRESHNESS=PASS"
Write-Host "RETAINED_ARCHIVE_POLICY=PASS"
Write-Host "SOURCE_ONLY_POLICY=PASS"
Write-Host ("SOURCE_ONLY_POLICY_COUNT=" + [string]$sourceOnlyRows.Count)
Write-Host ("SOURCE_ONLY=" + ($sourceOnlyRows.Name -join ","))
Write-Host "CORE_PRECOMPILED_POLICY=PASS"
Write-Host ("ACTIVATION_PENDING_COUNT=" + [string]$activationPending.Count)
Write-Host ("ACTIVATION_PENDING=" + ($activationPending -join ","))
Write-Host "PRECOMPILED_FREEZE=NOT_YET"
Write-Host "NEXT=P7B_RELEASE_LIKE_PRECOMPILED_ACTIVATION"
Write-Host "FINAL_TRACKED_DIRTY_COUNT=0"
Write-Host ("RESULT_ROOT=" + $ResultRoot)
