<#
.SYNOPSIS
  Empaqueta el directorio vpp/ como .vpp sin firma.

.DESCRIPTION
  Genera un ZIP con extension .vpp para revisar estructura y contenido.
  OpenPLC Editor 4.2.7 stock NO aceptara este archivo si no contiene
  signature.json valido y firmado por una llave confiable para el editor.
#>

$ErrorActionPreference = "Stop"

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$root = Resolve-Path (Join-Path $scriptDir "..")
$vppRoot = Join-Path $root "vpp"
$outDir = Join-Path $root "out"
$outFile = Join-Path $outDir "jwplc-basic-openplc-2.1.0-alpha.3.unsigned.vpp"
$tempZip = Join-Path $outDir "jwplc-basic-openplc-2.1.0-alpha.3.unsigned.zip"

if (-not (Test-Path $vppRoot)) {
    throw "No existe el directorio VPP: $vppRoot"
}

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
Write-Host "Nota: OpenPLC Editor 4.2.7 stock rechazara paquetes sin signature.json valido." -ForegroundColor Yellow
