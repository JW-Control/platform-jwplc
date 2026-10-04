param()

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"
. (Join-Path $PSScriptRoot "common.ps1")

$rawRelative = "tools/modbus-tcp-benchmark/firmware/eth14_raw_transport_server/eth14_raw_transport_server.ino"
$expectedRawSha256 = "9A0C6CF27E44A61DC97686FB1A769EBBBB34D036CDF8D0CA0EEAB597E699AB6B"

function Remove-CppCommentsPreserveStrings {
    param([Parameter(Mandatory = $true)][string]$Text)

    $sb = New-Object System.Text.StringBuilder
    $inBlockComment = $false
    $inLineComment = $false
    $inString = $false
    $inChar = $false
    $escape = $false

    for ($i = 0; $i -lt $Text.Length; ++$i) {
        $ch = $Text[$i]
        $next = if ($i + 1 -lt $Text.Length) { $Text[$i + 1] } else { [char]0 }

        if ($inLineComment) {
            if ($ch -eq [char]10) {
                $inLineComment = $false
                [void]$sb.Append($ch)
            }
            elseif ($ch -eq [char]13) {
                [void]$sb.Append($ch)
            }
            else {
                [void]$sb.Append(' ')
            }
            continue
        }

        if ($inBlockComment) {
            if ($ch -eq '*' -and $next -eq '/') {
                [void]$sb.Append(' ')
                [void]$sb.Append(' ')
                ++$i
                $inBlockComment = $false
            }
            elseif ($ch -eq [char]10 -or $ch -eq [char]13) {
                [void]$sb.Append($ch)
            }
            else {
                [void]$sb.Append(' ')
            }
            continue
        }

        if ($inString) {
            [void]$sb.Append($ch)

            if ($escape) {
                $escape = $false
            }
            elseif ($ch -eq '\\') {
                $escape = $true
            }
            elseif ($ch -eq '"') {
                $inString = $false
            }

            continue
        }

        if ($inChar) {
            [void]$sb.Append($ch)

            if ($escape) {
                $escape = $false
            }
            elseif ($ch -eq '\\') {
                $escape = $true
            }
            elseif ($ch -eq "'") {
                $inChar = $false
            }

            continue
        }

        if ($ch -eq '/' -and $next -eq '/') {
            [void]$sb.Append(' ')
            [void]$sb.Append(' ')
            ++$i
            $inLineComment = $true
            continue
        }

        if ($ch -eq '/' -and $next -eq '*') {
            [void]$sb.Append(' ')
            [void]$sb.Append(' ')
            ++$i
            $inBlockComment = $true
            continue
        }

        if ($ch -eq '"') {
            $inString = $true
            [void]$sb.Append($ch)
            continue
        }

        if ($ch -eq "'") {
            $inChar = $true
            [void]$sb.Append($ch)
            continue
        }

        [void]$sb.Append($ch)
    }

    if ($inBlockComment) {
        throw "D24_UNTERMINATED_BLOCK_COMMENT"
    }

    return $sb.ToString()
}

function Count-Literal {
    param(
        [Parameter(Mandatory = $true)][string]$Text,
        [Parameter(Mandatory = $true)][string]$Needle
    )

    $count = 0
    $offset = 0

    while ($true) {
        $index = $Text.IndexOf($Needle, $offset, [System.StringComparison]::Ordinal)
        if ($index -lt 0) { break }

        ++$count
        $offset = $index + $Needle.Length
    }

    return $count
}

Write-Host "============================================================"
Write-Host " A14 D24 - RAW CALL OWNERSHIP CLOSURE"
Write-Host "============================================================"

Assert-G2Branch

$head = Get-G2Head
$effectiveHz = Get-G2SpiHz
$dirty = @(Get-G2TrackedDirtyPaths)
$staged = @(& git -C $script:G2RepoRoot diff --cached --name-only)

if ($LASTEXITCODE -ne 0) {
    throw "D24_CACHED_DIFF_FAILED"
}

Write-Host "HEAD=$head"
Write-Host "EFFECTIVE_SPI_HZ=$effectiveHz"
Write-Host "TRACKED_DIRTY_COUNT=$($dirty.Count)"
Write-Host "STAGED_COUNT=$($staged.Count)"
Write-Host "SOURCE_MUTATION=NO"
Write-Host "UPLOAD=NO"
Write-Host "BENCHMARK=NO"

if ($effectiveHz -ne 26000000) {
    throw "D24_EXPECTED_FROZEN_26MHZ"
}

if ($dirty.Count -ne 0) {
    $dirty | ForEach-Object { Write-Host "DIRTY=$_" }
    throw "D24_TRACKED_TREE_NOT_CLEAN"
}

if ($staged.Count -ne 0) {
    throw "D24_INDEX_NOT_CLEAN"
}

Assert-G2ProtectedArtifacts

$rawSha = Get-G2Sha256 $rawRelative
Write-Host "RAW_FIRMWARE_SHA256=$rawSha"

if ($rawSha -ne $expectedRawSha256) {
    throw "D24_RAW_FIRMWARE_HASH_MISMATCH"
}

$rawPath = Get-G2Path $rawRelative
$rawText = [System.IO.File]::ReadAllText($rawPath)
$codeText = Remove-CppCommentsPreserveStrings -Text $rawText

$rawEthernetDotPattern = '(?<![A-Za-z0-9_])Ethernet\.'
$rawEthernetBeginPattern = '(?<![A-Za-z0-9_])Ethernet\.begin\('

$fullEthernetDotCount = @([regex]::Matches($rawText, $rawEthernetDotPattern)).Count
$codeEthernetDotCount = @([regex]::Matches($codeText, $rawEthernetDotPattern)).Count
$fullEthernetBeginCount = @([regex]::Matches($rawText, $rawEthernetBeginPattern)).Count
$codeEthernetBeginCount = @([regex]::Matches($codeText, $rawEthernetBeginPattern)).Count
$commentOnlyEthernetDotCount = $fullEthernetDotCount - $codeEthernetDotCount

Write-Host "RAW_ETHERNET_DOT_PATTERN=$rawEthernetDotPattern"
Write-Host "RAW_ETHERNET_BEGIN_PATTERN=$rawEthernetBeginPattern"

Write-Host ""
Write-Host "=== D24 OWNERSHIP EVIDENCE ==="
Write-Host "FULL_TEXT_ETHERNET_DOT_COUNT=$fullEthernetDotCount"
Write-Host "CODE_ONLY_ETHERNET_DOT_COUNT=$codeEthernetDotCount"
Write-Host "COMMENT_ONLY_ETHERNET_DOT_COUNT=$commentOnlyEthernetDotCount"
Write-Host "FULL_TEXT_ETHERNET_BEGIN_COUNT=$fullEthernetBeginCount"
Write-Host "CODE_ONLY_ETHERNET_BEGIN_COUNT=$codeEthernetBeginCount"

$rawLines = $rawText -split "\r?\n"
$matches = New-Object System.Collections.Generic.List[string]

for ($i = 0; $i -lt $rawLines.Count; ++$i) {
    if ([regex]::IsMatch($rawLines[$i], $rawEthernetDotPattern)) {
        $matches.Add(("{0}:{1}" -f ($i + 1), $rawLines[$i].Trim()))
    }
}

Write-Host "FULL_TEXT_ETHERNET_DOT_LINE_COUNT=$($matches.Count)"
$matches | ForEach-Object { Write-Host "ETHERNET_DOT_MATCH=$_" }

if ($fullEthernetDotCount -ne 1) {
    throw "D24_FULL_TEXT_ETHERNET_DOT_COUNT_CHANGED=$fullEthernetDotCount"
}

if ($codeEthernetDotCount -ne 0) {
    throw "D24_REAL_OTHER_RAW_CALLS_REMAIN=$codeEthernetDotCount"
}

if ($commentOnlyEthernetDotCount -ne 1) {
    throw "D24_COMMENT_ONLY_COUNT_INVALID=$commentOnlyEthernetDotCount"
}

if ($fullEthernetBeginCount -ne 1 -or $codeEthernetBeginCount -ne 0) {
    throw "D24_ETHERNET_BEGIN_CLASSIFICATION_INVALID"
}

if ($matches.Count -ne 1) {
    throw "D24_MATCH_LINE_COUNT_INVALID=$($matches.Count)"
}

if (-not $matches[0].Contains("NO llamar Ethernet.begin();")) {
    throw "D24_EXPECTED_COMMENT_MATCH_NOT_FOUND"
}

Write-Host ""
Write-Host "D24_PREVIOUS_OTHER_RAW_CALL_COUNT=1"
Write-Host "D24_REAL_CODE_OTHER_RAW_CALL_COUNT=0"
Write-Host "D24_COMMENT_ONLY_FALSE_POSITIVE_COUNT=1"
Write-Host "D24_ROOT_CAUSE=API_SUBSTRING_COUNT_MATCHED_COMMENT"
Write-Host "D24_PREVENTION_RULE=F021"
Write-Host "D24_OTHER_RAW_CALL_COUNT=0_CLOSED"
Write-Host "A14_D24_RAW_CALL_OWNERSHIP_CLOSURE=PASS"
Write-Host "NEXT_ACTION=RETURN_OUTPUT_TO_CHAT"
