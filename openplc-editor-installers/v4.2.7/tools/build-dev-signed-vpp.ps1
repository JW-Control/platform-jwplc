<#
.SYNOPSIS
  Genera un VPP firmado con llave local de desarrollo JW Control.

.DESCRIPTION
  Este script es para laboratorio. Crea una llave Ed25519 local si no existe,
  genera signature.json dentro de vpp/ y empaqueta el contenido como .vpp.

  IMPORTANTE: OpenPLC Editor stock seguira rechazando este paquete si no confia
  en la llave publica generada. Este flujo sirve para builds propios del editor
  o para preparar el paquete antes de solicitar firma upstream.
#>

$ErrorActionPreference = "Stop"

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$root = Resolve-Path (Join-Path $scriptDir "..")
$vppRoot = Join-Path $root "vpp"
$keysDir = Join-Path $root "keys"
$outDir = Join-Path $root "out"
$privateKey = Join-Path $keysDir "jw-control-dev-private.pem"
$manifestPath = Join-Path $vppRoot "manifest.json"
$manifest = Get-Content $manifestPath -Raw | ConvertFrom-Json
$packageVersion = $manifest.package.version
$outZip = Join-Path $outDir "jwplc-basic-openplc-$packageVersion.dev-signed.zip"
$outVpp = Join-Path $outDir "jwplc-basic-openplc-$packageVersion.dev-signed.vpp"

if (-not (Test-Path $vppRoot)) {
    throw "No existe el directorio VPP: $vppRoot"
}

if (-not (Get-Command node -ErrorAction SilentlyContinue)) {
    throw "Node.js no esta disponible en PATH. Instala Node.js o ejecuta desde una terminal donde node este disponible."
}

if (-not (Test-Path $privateKey)) {
    Write-Host "No existe llave dev. Generando llave Ed25519 local..." -ForegroundColor Yellow
    node (Join-Path $scriptDir "generate-dev-keypair.mjs") $keysDir
}

Write-Host "Generando signature.json con llave dev..." -ForegroundColor Cyan
node (Join-Path $scriptDir "sign-vpp-package.mjs") $vppRoot "jw-control-dev" $privateKey

New-Item -ItemType Directory -Path $outDir -Force | Out-Null
Remove-Item $outZip, $outVpp -Force -ErrorAction SilentlyContinue

Push-Location $vppRoot
try {
    Compress-Archive -Path * -DestinationPath $outZip -Force
}
finally {
    Pop-Location
}

Rename-Item -Path $outZip -NewName (Split-Path $outVpp -Leaf) -Force

Write-Host "VPP firmado con llave dev generado:" -ForegroundColor Green
Write-Host "  $outVpp"
Write-Host ""
Write-Host "Advertencia: OpenPLC Editor stock no aceptara este VPP si no confia en jw-control-dev-public.pem." -ForegroundColor Yellow

