Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

. (Join-Path $PSScriptRoot "common.ps1")

Write-Host "============================================================"
Write-Host " A14 P5-E2 - CACHED SOCKET STATE CANDIDATE PREFLIGHT"
Write-Host "============================================================"

Assert-G2Branch

$head = Get-G2Head
$spiHz = Get-G2SpiHz
$dirty = @(Get-G2TrackedDirtyPaths)
$staged = @(
    & git -C $script:G2RepoRoot diff --cached --name-only
)

if ($LASTEXITCODE -ne 0) {
    throw "P5E2_CACHED_DIFF_FAILED"
}

Write-Host "HEAD=$head"
Write-Host "W5500_SPI_HZ=$spiHz"
Write-Host "TRACKED_DIRTY_COUNT=$($dirty.Count)"
Write-Host "STAGED_COUNT=$($staged.Count)"
Write-Host "PRODUCT_MUTATION=JWPLC_MODBUS_TCP_INTERNAL_ONLY"
Write-Host "PUBLIC_API_CHANGE=NO"
Write-Host "CORE_A_REBUILD_REQUIRED=NO"

if ($spiHz -ne 26000000) {
    throw "P5E2_EXPECTED_26MHZ"
}

if ($dirty.Count -ne 0) {
    $dirty | ForEach-Object { Write-Host "DIRTY=$_" }
    throw "P5E2_TRACKED_TREE_NOT_CLEAN"
}

if ($staged.Count -ne 0) {
    $staged | ForEach-Object { Write-Host "STAGED=$_" }
    throw "P5E2_INDEX_NOT_CLEAN"
}

Assert-G2ProtectedArtifacts

$source = Get-G2Path "JWPLC/2.1.0/libraries/JWPLC_ModbusTCP/src/JWPLC_ModbusTCP.cpp"
$text = [System.IO.File]::ReadAllText($source)

$checks = @(
    [PSCustomObject]@{
        Label = "DEFER_CONNECTED_WHEN_RX_PENDING"
        Pass = $text.Contains("P5-E2: connected() implica otra consulta")
    },
    [PSCustomObject]@{
        Label = "CACHE_AVAILABLE_BYTES_AFTER_READ"
        Pass = $text.Contains("availableBytes -= received;")
    },
    [PSCustomObject]@{
        Label = "REFRESH_AVAILABLE_ONLY_WHEN_EXHAUSTED"
        Pass = $text.Contains("Sólo refrescar hardware cuando agotamos el saldo")
    }
)

foreach ($check in $checks) {
    Write-Host "$($check.Label)=$($check.Pass)"

    if (-not $check.Pass) {
        throw "P5E2_SOURCE_CONTRACT_FAILED_$($check.Label)"
    }
}

Write-Host "A14_P5E2_CACHED_SOCKET_STATE_PREFLIGHT=PASS"
