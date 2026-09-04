param(
    [Parameter(Mandatory = $true)]
    [string]$OpenPLCRepo
)

$ErrorActionPreference = 'Stop'

function Invoke-GitText {
    param(
        [Parameter(Mandatory = $true)][string]$Repo,
        [Parameter(Mandatory = $true)][string[]]$Arguments
    )

    $output = & git -C $Repo @Arguments 2>&1
    if ($LASTEXITCODE -ne 0) {
        throw "git -C `"$Repo`" $($Arguments -join ' ') fallo:`n$($output -join "`n")"
    }
    return ($output -join "`n").Trim()
}

function Replace-SourceExactlyOnce {
    param(
        [Parameter(Mandatory = $true)][string]$Content,
        [Parameter(Mandatory = $true)][string]$Old,
        [Parameter(Mandatory = $true)][string]$New,
        [Parameter(Mandatory = $true)][string]$Label
    )

    $first = $Content.IndexOf($Old, [System.StringComparison]::Ordinal)
    if ($first -lt 0) {
        throw "No se encontro el ancla del aplicador base: $Label"
    }
    $second = $Content.IndexOf($Old, $first + $Old.Length, [System.StringComparison]::Ordinal)
    if ($second -ge 0) {
        throw "El ancla del aplicador base aparece mas de una vez: $Label"
    }

    return $Content.Substring(0, $first) + $New + $Content.Substring($first + $Old.Length)
}

$platformRepo = Invoke-GitText -Repo $PSScriptRoot -Arguments @('rev-parse', '--show-toplevel')
$sourcePath = Join-Path $platformRepo 'tools/alpha7/Apply-OpenPLC-PackageSourceSelector.ps1'

if (-not (Test-Path -LiteralPath $sourcePath)) {
    throw "No existe el aplicador base: $sourcePath"
}

$source = [System.IO.File]::ReadAllText($sourcePath)

# Fix 1: [workspace] es un nombre literal de carpeta de Next/React, no un wildcard.
$source = Replace-SourceExactlyOnce -Content $source `
    -Old 'if (-not (Test-Path $path)) {' `
    -New 'if (-not (Test-Path -LiteralPath $path)) {' `
    -Label 'Test-Path archivos esperados'
$source = Replace-SourceExactlyOnce -Content $source `
    -Old 'if (Test-Path $resolverPath) {' `
    -New 'if (Test-Path -LiteralPath $resolverPath) {' `
    -Label 'Test-Path resolver'
$source = Replace-SourceExactlyOnce -Content $source `
    -Old 'if (Test-Path $resolverTestPath) {' `
    -New 'if (Test-Path -LiteralPath $resolverTestPath) {' `
    -Label 'Test-Path resolver test'

# Fix 2: los here-strings del .ps1 y los fuentes TS/JSON pueden tener CRLF/LF
# distintos en Windows. Antes de buscar cada ancla, adaptamos Old/New al mismo
# estilo de fin de linea que el archivo destino, sin reformatear el resto.
$indexLine = '    $first = $Content.IndexOf($Old, [System.StringComparison]::Ordinal)'
$newlineAwareBlock = @'
    $newline = if ($Content.Contains("`r`n")) { "`r`n" } else { "`n" }
    $oldNormalized = $Old.Replace("`r`n", "`n").Replace("`r", "`n")
    $newNormalized = $New.Replace("`r`n", "`n").Replace("`r", "`n")
    if ($newline -eq "`r`n") {
        $oldNormalized = $oldNormalized.Replace("`n", "`r`n")
        $newNormalized = $newNormalized.Replace("`n", "`r`n")
    }
    $Old = $oldNormalized
    $New = $newNormalized

    $first = $Content.IndexOf($Old, [System.StringComparison]::Ordinal)
'@
$source = Replace-SourceExactlyOnce -Content $source `
    -Old $indexLine `
    -New $newlineAwareBlock `
    -Label 'Replace-ExactlyOnce newline normalization'

# La copia temporal vive fuera del repo. Fijamos explicitamente el root del
# platform para que el aplicador no intente inferirlo desde %TEMP%.
$platformLiteral = $platformRepo.Replace("'", "''")
$platformLookup = '$platformRepo = Get-GitOutput -Repo $PSScriptRoot -Arguments @(''rev-parse'', ''--show-toplevel'')'
$source = Replace-SourceExactlyOnce -Content $source `
    -Old $platformLookup `
    -New "`$platformRepo = '$platformLiteral'" `
    -Label 'platform repo root temporal'

$tempPath = Join-Path ([System.IO.Path]::GetTempPath()) 'JWPLC_Apply-OpenPLC-PackageSourceSelector-v3.generated.ps1'
$utf8NoBom = New-Object System.Text.UTF8Encoding($false)
[System.IO.File]::WriteAllText($tempPath, $source, $utf8NoBom)

Write-Host '============================================================' -ForegroundColor Cyan
Write-Host ' ALPHA7 - PACKAGE SOURCE SELECTOR V3' -ForegroundColor Cyan
Write-Host '============================================================' -ForegroundColor Cyan
Write-Host 'Fix activo 1: rutas [workspace] tratadas como literales.' -ForegroundColor Green
Write-Host 'Fix activo 2: anclas tolerantes a CRLF/LF, sin reformateo global.' -ForegroundColor Green
Write-Host ''

try {
    & powershell -NoProfile -ExecutionPolicy Bypass -File $tempPath -OpenPLCRepo $OpenPLCRepo
    $exitCode = $LASTEXITCODE
    if ($exitCode -ne 0) {
        throw "El aplicador corregido termino con codigo $exitCode."
    }
}
finally {
    Remove-Item -LiteralPath $tempPath -Force -ErrorAction SilentlyContinue
}
