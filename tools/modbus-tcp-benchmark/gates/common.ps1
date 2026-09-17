Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$script:G2ExpectedBranch = "v2.1.0-alpha.14/feature/modbus-tcp"
$script:G2AllowedMHz = @(14, 20, 24, 26, 30)
$script:G2HoldBudgetUs = 5000

$script:G2SpiHeaderRelative = "JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/utility/w5100.h"
$script:G2CoreRelative = "JWPLC/2.1.0/precompiled/core/JWPLCBASIC/core.a"
$script:G2SdRelative = "JWPLC/2.1.0/libraries/JW_SD/src/esp32/libJW_SD.a"
$script:G2RawFirmwareRelative = "tools/modbus-tcp-benchmark/firmware/eth14_raw_transport_server/eth14_raw_transport_server.ino"
$script:G2RawRunnerRelative = "tools/modbus-tcp-benchmark/pc/eth14_raw_transport_benchmark.py"

$script:G2CoreSha256 = "6EDF40D105936318A2FD8A84D7F0724571657910E8D92E8538640EC613F4DD68"
$script:G2SdSha256 = "E75BDE36481BF621DB37300ADEA7CF0D4A89B73ECE442A218135BD3E8E8AA5C1"
$script:G2RawFirmwareSha256 = "E77A53C1503401BD51AEFDA969C2D287DE1FB9DD349675EC7B0FD355229BF356"
$script:G2RawRunnerSha256 = "FD25997CFE777B4FF50BBC10EFE555BD6B7A8A8DCE9D4ADD1E7D2DBA6D52470F"

$repoRootOutput = @(
    & git -C $PSScriptRoot rev-parse --show-toplevel 2>$null
)

if ($LASTEXITCODE -ne 0 -or $repoRootOutput.Count -ne 1) {
    throw "G2_GIT_ROOT_NOT_FOUND"
}

$script:G2RepoRoot = $repoRootOutput[0].Trim()

function Get-G2Path {
    param(
        [Parameter(Mandatory = $true)]
        [string]$RelativePath
    )

    return Join-Path $script:G2RepoRoot ($RelativePath -replace "/", "\")
}

function Get-G2Head {
    $value = @(
        & git -C $script:G2RepoRoot rev-parse HEAD
    )

    if ($LASTEXITCODE -ne 0 -or $value.Count -ne 1) {
        throw "G2_HEAD_NOT_FOUND"
    }

    return $value[0].Trim()
}

function Get-G2Branch {
    $value = @(
        & git -C $script:G2RepoRoot branch --show-current
    )

    if ($LASTEXITCODE -ne 0 -or $value.Count -ne 1) {
        throw "G2_BRANCH_NOT_FOUND"
    }

    return $value[0].Trim()
}

function Assert-G2Branch {
    $branch = Get-G2Branch
    Write-Host "BRANCH=$branch"

    if ($branch -ne $script:G2ExpectedBranch) {
        throw "G2_BRANCH_MISMATCH"
    }
}

function Assert-G2AllowedMHz {
    param(
        [Parameter(Mandatory = $true)]
        [int]$MHz
    )

    if ($MHz -notin $script:G2AllowedMHz) {
        throw "G2_FREQUENCY_NOT_ALLOWED"
    }
}

function Get-G2TrackedDirtyPaths {
    $unstaged = @(
        & git -C $script:G2RepoRoot diff --name-only
    )

    if ($LASTEXITCODE -ne 0) {
        throw "G2_GIT_DIFF_FAILED"
    }

    $staged = @(
        & git -C $script:G2RepoRoot diff --cached --name-only
    )

    if ($LASTEXITCODE -ne 0) {
        throw "G2_GIT_DIFF_CACHED_FAILED"
    }

    return @(
        @($unstaged + $staged) |
            Where-Object { -not [string]::IsNullOrWhiteSpace($_) } |
            Sort-Object -Unique
    )
}

function Get-G2Sha256 {
    param(
        [Parameter(Mandatory = $true)]
        [string]$RelativePath
    )

    $path = Get-G2Path $RelativePath

    if (-not (Test-Path -LiteralPath $path)) {
        throw "G2_REQUIRED_FILE_MISSING=$RelativePath"
    }

    return (
        Get-FileHash -LiteralPath $path -Algorithm SHA256
    ).Hash.ToUpperInvariant()
}

function Assert-G2ProtectedArtifacts {
    $coreHash = Get-G2Sha256 $script:G2CoreRelative
    $sdHash = Get-G2Sha256 $script:G2SdRelative

    Write-Host "CORE_A_SHA256=$coreHash"
    Write-Host "LIBJW_SD_A_SHA256=$sdHash"

    if ($coreHash -ne $script:G2CoreSha256) {
        throw "G2_CORE_A_HASH_MISMATCH"
    }

    if ($sdHash -ne $script:G2SdSha256) {
        throw "G2_LIBJW_SD_A_HASH_MISMATCH"
    }
}

function Assert-G2RawBaselineArtifacts {
    $firmwareHash = Get-G2Sha256 $script:G2RawFirmwareRelative
    $runnerHash = Get-G2Sha256 $script:G2RawRunnerRelative

    Write-Host "RAW_FIRMWARE_SHA256=$firmwareHash"
    Write-Host "RAW_RUNNER_SHA256=$runnerHash"

    if ($firmwareHash -ne $script:G2RawFirmwareSha256) {
        throw "G2_RAW_FIRMWARE_HASH_MISMATCH"
    }

    if ($runnerHash -ne $script:G2RawRunnerSha256) {
        throw "G2_RAW_RUNNER_HASH_MISMATCH"
    }
}

function Get-G2SpiHz {
    $path = Get-G2Path $script:G2SpiHeaderRelative

    if (-not (Test-Path -LiteralPath $path)) {
        throw "G2_SPI_HEADER_MISSING"
    }

    $text = [System.IO.File]::ReadAllText($path)
    $pattern = '(?m)^\s*#define\s+SPI_ETHERNET_SETTINGS\s+SPISettings\((\d+),\s*MSBFIRST,\s*SPI_MODE0\)\s*$'
    $matches = [regex]::Matches($text, $pattern)

    if ($matches.Count -ne 1) {
        throw "G2_ACTIVE_SPI_SETTINGS_COUNT=$($matches.Count)"
    }

    return [int64]$matches[0].Groups[1].Value
}

function Set-G2SpiHz {
    param(
        [Parameter(Mandatory = $true)]
        [int64]$Hz
    )

    $path = Get-G2Path $script:G2SpiHeaderRelative
    $text = [System.IO.File]::ReadAllText($path)
    $pattern = '(?m)^\s*#define\s+SPI_ETHERNET_SETTINGS\s+SPISettings\((\d+),\s*MSBFIRST,\s*SPI_MODE0\)\s*$'
    $regex = [regex]::new($pattern)
    $matches = $regex.Matches($text)

    if ($matches.Count -ne 1) {
        throw "G2_ACTIVE_SPI_SETTINGS_COUNT=$($matches.Count)"
    }

    $replacement = "#define SPI_ETHERNET_SETTINGS SPISettings($Hz, MSBFIRST, SPI_MODE0)"
    $newText = $regex.Replace($text, $replacement, 1)
    $utf8NoBom = New-Object -TypeName System.Text.UTF8Encoding -ArgumentList $false
    [System.IO.File]::WriteAllText($path, $newText, $utf8NoBom)
}
