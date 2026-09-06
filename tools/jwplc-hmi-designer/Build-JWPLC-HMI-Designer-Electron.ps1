param(
    [switch]$SkipDependencies
)

$ErrorActionPreference = 'Stop'

$root = $PSScriptRoot
$electronRoot = Join-Path $root 'electron'
$sourceIcon = Join-Path $root 'assets\JWPLC-HMI-Designer.ico'
$electronBuild = Join-Path $electronRoot 'build'
$electronIcon = Join-Path $electronBuild 'icon.ico'
$releaseRoot = Join-Path $electronRoot 'release'
$distRoot = Join-Path $root 'dist'
$targetExe = Join-Path $distRoot 'JWPLC-HMI-Designer-Electron.exe'

if (-not (Test-Path -LiteralPath (Join-Path $electronRoot 'package.json') -PathType Leaf)) {
    throw "No se encontró electron\package.json."
}
if (-not (Test-Path -LiteralPath $sourceIcon -PathType Leaf)) {
    throw "No se encontró el icono del Designer: $sourceIcon"
}

$npm = Get-Command npm.cmd -ErrorAction SilentlyContinue
if (-not $npm) {
    throw 'No se encontró npm.cmd. Instala Node.js LTS para generar el wrapper Electron.'
}

New-Item -ItemType Directory -Force -Path $electronBuild | Out-Null
New-Item -ItemType Directory -Force -Path $distRoot | Out-Null
Copy-Item -LiteralPath $sourceIcon -Destination $electronIcon -Force

Write-Host '================================================' -ForegroundColor DarkCyan
Write-Host ' JWPLC HMI Designer - Build Electron Alpha11' -ForegroundColor Cyan
Write-Host '================================================' -ForegroundColor DarkCyan
Write-Host ''
Write-Host "Icono nativo: $sourceIcon"
Write-Host "Destino:      $targetExe"
Write-Host ''

Push-Location $electronRoot
try {
    if (-not $SkipDependencies) {
        Write-Host 'Instalando/verificando dependencias Electron...' -ForegroundColor Cyan
        & $npm.Source install --no-audit --no-fund
        if ($LASTEXITCODE -ne 0) {
            throw "npm install falló con código $LASTEXITCODE."
        }
    }

    Write-Host 'Generando ejecutable portable nativo...' -ForegroundColor Cyan
    & $npm.Source run dist:portable
    if ($LASTEXITCODE -ne 0) {
        throw "electron-builder falló con código $LASTEXITCODE."
    }
}
finally {
    Pop-Location
}

$artifact = Get-ChildItem -LiteralPath $releaseRoot -Filter 'JWPLC-HMI-Designer-Portable-*.exe' -File |
    Sort-Object LastWriteTime -Descending |
    Select-Object -First 1

if (-not $artifact) {
    throw "No se encontró el ejecutable portable generado en $releaseRoot."
}

Copy-Item -LiteralPath $artifact.FullName -Destination $targetExe -Force

Write-Host ''
Write-Host 'Build Electron completado.' -ForegroundColor Green
Write-Host "  Artifact: $($artifact.FullName)"
Write-Host "  Instalador usará: $targetExe"
Write-Host ''
Write-Host 'Siguiente paso:' -ForegroundColor Yellow
Write-Host '  .\Install-JWPLC-HMI-Designer.cmd'
