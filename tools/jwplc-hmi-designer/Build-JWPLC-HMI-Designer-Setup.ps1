param(
    [switch]$SkipDependencies
)

$ErrorActionPreference = 'Stop'

$root = $PSScriptRoot
$electronRoot = Join-Path $root 'electron'
$packageJson = Join-Path $electronRoot 'package.json'
$packageLock = Join-Path $electronRoot 'package-lock.json'
$installerNsh = Join-Path $electronRoot 'installer.nsh'
$sourceIcon = Join-Path $root 'assets\JWPLC-HMI-Designer.ico'
$electronBuild = Join-Path $electronRoot 'build'
$electronIcon = Join-Path $electronBuild 'icon.ico'
$releaseRoot = Join-Path $electronRoot 'release'
$distRoot = Join-Path $root 'dist'
$launcherBuilder = Join-Path $root 'Build-ArduinoIDE-Launcher.ps1'

foreach ($required in @($packageJson, $packageLock, $installerNsh, $sourceIcon, $launcherBuilder)) {
    if (-not (Test-Path -LiteralPath $required -PathType Leaf)) {
        throw "Falta un archivo requerido para generar el Setup Alpha11: $required"
    }
}

$npm = Get-Command npm.cmd -ErrorAction SilentlyContinue
if (-not $npm) {
    throw 'No se encontró npm.cmd. Instala Node.js LTS para generar el Setup Electron.'
}

# build/release/dist son artefactos generados e ignorados por Git.
foreach ($generated in @($electronBuild, $releaseRoot, $distRoot)) {
    if (Test-Path -LiteralPath $generated) {
        Remove-Item -LiteralPath $generated -Recurse -Force
    }
}

New-Item -ItemType Directory -Force -Path $electronBuild | Out-Null
New-Item -ItemType Directory -Force -Path $distRoot | Out-Null
Copy-Item -LiteralPath $sourceIcon -Destination $electronIcon -Force

Write-Host '================================================' -ForegroundColor DarkCyan
Write-Host ' JWPLC HMI Designer - Setup EXE Alpha11' -ForegroundColor Cyan
Write-Host '================================================' -ForegroundColor DarkCyan
Write-Host ''
Write-Host "Icono:        $sourceIcon"
Write-Host "Lockfile:     $packageLock"
Write-Host "NSIS include: $installerNsh"
Write-Host ''

Write-Host 'Generando extensión Arduino IDE 2 que viajará dentro del Setup...' -ForegroundColor Cyan
$launcherOutput = @(& $launcherBuilder -OutputDirectory $electronBuild)
$vsix = Get-ChildItem -LiteralPath $electronBuild -Filter 'jwplc-hmi-launcher-*.vsix' -File |
    Sort-Object LastWriteTime -Descending |
    Select-Object -First 1

if (-not $vsix) {
    throw 'No se generó el VSIX de Arduino IDE requerido por el Setup.'
}

Write-Host "VSIX incluido: $($vsix.FullName)" -ForegroundColor DarkGray

Push-Location $electronRoot
try {
    if (-not $SkipDependencies) {
        Write-Host 'Instalando dependencias exactas desde package-lock.json...' -ForegroundColor Cyan
        & $npm.Source ci --no-audit --no-fund
        if ($LASTEXITCODE -ne 0) {
            throw "npm ci falló con código $LASTEXITCODE."
        }
    }

    Write-Host 'Generando instalador NSIS x64...' -ForegroundColor Cyan
    & $npm.Source run dist:setup
    if ($LASTEXITCODE -ne 0) {
        throw "electron-builder/NSIS falló con código $LASTEXITCODE."
    }
}
finally {
    Pop-Location
}

$setup = Get-ChildItem -LiteralPath $releaseRoot -Filter 'JWPLC-HMI-Designer-Setup-*.exe' -File |
    Sort-Object LastWriteTime -Descending |
    Select-Object -First 1

if (-not $setup) {
    throw "No se encontró el Setup generado en $releaseRoot."
}

$distSetup = Join-Path $distRoot $setup.Name
Copy-Item -LiteralPath $setup.FullName -Destination $distSetup -Force

$setupHash = (Get-FileHash -LiteralPath $setup.FullName -Algorithm SHA256).Hash.ToLower()
$distHash = (Get-FileHash -LiteralPath $distSetup -Algorithm SHA256).Hash.ToLower()

if ($setupHash -ne $distHash) {
    throw 'El Setup de release y la copia de dist no son idénticos.'
}

Write-Host ''
Write-Host 'Setup EXE combinado generado.' -ForegroundColor Green
Write-Host "  Release: $($setup.FullName)"
Write-Host "  Dist:    $distSetup"
Write-Host "  Bytes:   $($setup.Length)"
Write-Host "  SHA256:  $setupHash"
Write-Host "  VSIX:    $($vsix.Name)"
Write-Host ''
Write-Host 'El Setup instala JWPLC HMI Designer + extensión Arduino IDE 2.' -ForegroundColor Green
Write-Output $distSetup
