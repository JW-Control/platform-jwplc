<#
.SYNOPSIS
  Empaqueta el directorio vpp/ como .vpp sin firma.

.DESCRIPTION
  Genera un ZIP con extension .vpp para revisar estructura y contenido.
  El nombre de salida se calcula desde vpp/manifest.json:
    package.version
#>

$ErrorActionPreference = "Stop"

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$root = Resolve-Path (Join-Path $scriptDir "..")
$vppRoot = Join-Path $root "vpp"
$outDir = Join-Path $root "out"
$manifestPath = Join-Path $vppRoot "manifest.json"

if (-not (Test-Path $vppRoot)) {
    throw "No existe el directorio VPP: $vppRoot"
}

if (-not (Test-Path $manifestPath)) {
    throw "No existe manifest.json: $manifestPath"
}

$manifest = Get-Content $manifestPath -Raw | ConvertFrom-Json
$version = $manifest.package.version

if ([string]::IsNullOrWhiteSpace($version)) {
    throw "No se pudo leer package.version desde manifest.json"
}

$baseName = "jwplc-basic-openplc-$version"
$outFile = Join-Path $outDir "$baseName.unsigned.vpp"
$tempZip = Join-Path $outDir "$baseName.unsigned.zip"

New-Item -ItemType Directory -Path $outDir -Force | Out-Null
Remove-Item $outFile, $tempZip -Force -ErrorAction SilentlyContinue

Push-Location $vppRoot
try {
    Compress-Archive -Path * -DestinationPath $tempZip -Force
}
finally {
    Pop-Location
}

Rename-Item -Path $tempZip -NewName (Split-Path $outFile -Leaf) -Force

Write-Host "VPP sin firma generado:" -ForegroundColor Green
Write-Host "  $outFile"
Write-Host ""
Write-Host "Version leida desde manifest.json: $version" -ForegroundColor Cyan
Write-Host "Nota: OpenPLC Editor 4.2.7 stock rechazara paquetes sin signature.json valido." -ForegroundColor Yellow