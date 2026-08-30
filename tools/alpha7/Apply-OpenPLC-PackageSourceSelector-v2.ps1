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

$platformRepo = Invoke-GitText -Repo $PSScriptRoot -Arguments @('rev-parse', '--show-toplevel')
$sourcePath = Join-Path $platformRepo 'tools/alpha7/Apply-OpenPLC-PackageSourceSelector.ps1'

if (-not (Test-Path -LiteralPath $sourcePath)) {
    throw "No existe el aplicador base: $sourcePath"
}

$source = [System.IO.File]::ReadAllText($sourcePath)

# El aplicador base fue correcto en su logica, pero PowerShell interpreta
# [workspace] como wildcard cuando Test-Path no recibe -LiteralPath.
# Ejecutamos una copia temporal corregida para mantener ambos repos limpios
# durante las validaciones iniciales del propio aplicador.
$source = $source.Replace(
    'if (-not (Test-Path $path)) {',
    'if (-not (Test-Path -LiteralPath $path)) {'
)
$source = $source.Replace(
    'if (Test-Path $resolverPath) {',
    'if (Test-Path -LiteralPath $resolverPath) {'
)
$source = $source.Replace(
    'if (Test-Path $resolverTestPath) {',
    'if (Test-Path -LiteralPath $resolverTestPath) {'
)

$platformLiteral = $platformRepo.Replace("'", "''")
$source = $source.Replace(
    '$platformRepo = Get-GitOutput -Repo $PSScriptRoot -Arguments @(''rev-parse'', ''--show-toplevel'')',
    "`$platformRepo = '$platformLiteral'"
)

$tempPath = Join-Path ([System.IO.Path]::GetTempPath()) 'JWPLC_Apply-OpenPLC-PackageSourceSelector-v2.generated.ps1'
$utf8NoBom = New-Object System.Text.UTF8Encoding($false)
[System.IO.File]::WriteAllText($tempPath, $source, $utf8NoBom)

Write-Host '============================================================' -ForegroundColor Cyan
Write-Host ' ALPHA7 - PACKAGE SOURCE SELECTOR V2' -ForegroundColor Cyan
Write-Host '============================================================' -ForegroundColor Cyan
Write-Host 'Fix activo: rutas [workspace] tratadas como literales.' -ForegroundColor Green
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
