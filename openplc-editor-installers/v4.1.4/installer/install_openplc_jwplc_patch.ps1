<#
.SYNOPSIS
  Instala el patch de integracion OpenPLC para JWPLC Basic v2.0.0.

.DESCRIPTION
  Este script copia los archivos ubicados en:
    installers/openplc-jwplc-basic-v2.0.0/open-plc-editor/
  sobre una instalacion local de OpenPLC Editor v4.

  Antes de sobrescribir archivos, crea un backup automatico con timestamp.

.NOTES
  No modifica el package Arduino platform-jwplc.
  No modifica el registro de Windows.
  No instala servicios.
  No requiere permisos de administrador si OpenPLC esta en una carpeta del usuario.
#>

$ErrorActionPreference = "Stop"

function Test-OpenPLCPatchRoot {
    param([string]$Path)

    if ([string]::IsNullOrWhiteSpace($Path)) {
        return $false
    }

    $resources = Join-Path $Path "resources\sources"
    return (Test-Path $resources)
}

function Resolve-OpenPLCPatchRoot {
    param([string]$InputPath)

    $InputPath = $InputPath.Trim('"')

    $candidates = @(
        $InputPath,
        (Join-Path $InputPath "editor\arduino"),
        (Join-Path $InputPath "editor")
    )

    foreach ($candidate in $candidates) {
        if (Test-OpenPLCPatchRoot $candidate) {
            return (Resolve-Path $candidate).Path
        }
    }

    return $InputPath
}

Write-Host "==============================================" -ForegroundColor Cyan
Write-Host " JWPLC Basic - OpenPLC Editor v4 Patch Installer" -ForegroundColor Cyan
Write-Host "==============================================" -ForegroundColor Cyan
Write-Host ""

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$integrationRoot = Resolve-Path (Join-Path $scriptDir "..")
$sourceRoot = Join-Path $integrationRoot "open-plc-editor"

if (-not (Test-Path $sourceRoot)) {
    Write-Error "No se encontro la carpeta origen: $sourceRoot"
}

Write-Host "Este instalador copiara los archivos del patch JWPLC sobre OpenPLC Editor." -ForegroundColor Yellow
Write-Host "Se creara un backup automatico antes de sobrescribir archivos existentes." -ForegroundColor Yellow
Write-Host ""

$defaultCandidates = @(
    "$env:USERPROFILE\OpenPLC_Editor",
    "$env:USERPROFILE\OpenPLC_Editor_save1",
    "$env:USERPROFILE\Documents\OpenPLC_Editor",
    "$env:USERPROFILE\AppData\Local\Programs\OpenPLC Editor",
    "$env:LOCALAPPDATA\Programs\OpenPLC Editor",
    "C:\Program Files\OpenPLC Editor",
    "C:\Program Files (x86)\OpenPLC Editor"
)

Write-Host "Rutas candidatas encontradas:" -ForegroundColor Cyan
$candidateIndex = 1
$validCandidates = @()
foreach ($candidate in $defaultCandidates) {
    if (Test-Path $candidate) {
        $resolvedCandidate = Resolve-OpenPLCPatchRoot $candidate
        if (Test-OpenPLCPatchRoot $resolvedCandidate) {
            Write-Host "  [$candidateIndex] $resolvedCandidate"
            $validCandidates += $resolvedCandidate
            $candidateIndex++
        }
    }
}

if ($validCandidates.Count -eq 0) {
    Write-Host "  No se encontraron rutas candidatas automaticamente."
}

Write-Host ""
Write-Host "Ingresa la ruta raiz de OpenPLC Editor o el numero de una ruta candidata." -ForegroundColor Cyan
$targetInput = Read-Host "Ruta o numero"

if ([string]::IsNullOrWhiteSpace($targetInput)) {
    Write-Error "Entrada vacia. Instalacion cancelada."
}

[int]$selectedIndex = 0
if ([int]::TryParse($targetInput, [ref]$selectedIndex) -and $selectedIndex -ge 1 -and $selectedIndex -le $validCandidates.Count) {
    $targetRoot = $validCandidates[$selectedIndex - 1]
}
else {
    $targetRoot = Resolve-OpenPLCPatchRoot $targetInput
}

$targetRoot = $targetRoot.Trim('"')

if (-not (Test-Path $targetRoot)) {
    Write-Error "La ruta indicada no existe: $targetRoot"
}

if (-not (Test-OpenPLCPatchRoot $targetRoot)) {
    Write-Host "" -ForegroundColor Yellow
    Write-Host "ADVERTENCIA: no se encontro 'resources\sources' dentro de la ruta destino." -ForegroundColor Yellow
    Write-Host "Ruta destino actual: $targetRoot" -ForegroundColor Yellow
    Write-Host "Si la ruta no es correcta, el patch se copiara en una ubicacion incorrecta." -ForegroundColor Yellow
    $forceContinue = Read-Host "Continuar de todos modos? (S/N)"
    if ($forceContinue -notin @("S", "s", "SI", "Si", "si", "Y", "y")) {
        Write-Host "Instalacion cancelada por seguridad." -ForegroundColor Yellow
        exit 0
    }
}

Write-Host ""
Write-Host "Origen : $sourceRoot" -ForegroundColor Gray
Write-Host "Destino: $targetRoot" -ForegroundColor Gray
Write-Host ""

$confirm = Read-Host "Continuar y aplicar patch? (S/N)"
if ($confirm -notin @("S", "s", "SI", "Si", "si", "Y", "y")) {
    Write-Host "Instalacion cancelada por el usuario." -ForegroundColor Yellow
    exit 0
}

$timestamp = Get-Date -Format "yyyyMMdd_HHmmss"
$backupRoot = Join-Path $targetRoot "backup_jwplc_openplc_$timestamp"
New-Item -ItemType Directory -Path $backupRoot -Force | Out-Null

Write-Host "Creando backup en:" -ForegroundColor Cyan
Write-Host "  $backupRoot"

$files = Get-ChildItem -Path $sourceRoot -Recurse -File
if ($files.Count -eq 0) {
    Write-Error "No hay archivos para copiar en: $sourceRoot"
}

$backedUp = 0
foreach ($file in $files) {
    $relativePath = $file.FullName.Substring($sourceRoot.Length).TrimStart('\', '/')
    $targetFile = Join-Path $targetRoot $relativePath
    $backupFile = Join-Path $backupRoot $relativePath

    if (Test-Path $targetFile) {
        $backupDir = Split-Path -Parent $backupFile
        New-Item -ItemType Directory -Path $backupDir -Force | Out-Null
        Copy-Item -Path $targetFile -Destination $backupFile -Force
        $backedUp++
    }
}

Write-Host "Archivos existentes respaldados: $backedUp" -ForegroundColor Cyan
Write-Host "Copiando archivos..." -ForegroundColor Cyan

foreach ($file in $files) {
    $relativePath = $file.FullName.Substring($sourceRoot.Length).TrimStart('\', '/')
    $targetFile = Join-Path $targetRoot $relativePath
    $targetDir = Split-Path -Parent $targetFile

    New-Item -ItemType Directory -Path $targetDir -Force | Out-Null
    Copy-Item -Path $file.FullName -Destination $targetFile -Force
    Write-Host "  OK -> $relativePath" -ForegroundColor Green
}

Write-Host ""
Write-Host "Patch OpenPLC para JWPLC Basic instalado correctamente." -ForegroundColor Green
Write-Host "Backup creado en:" -ForegroundColor Cyan
Write-Host "  $backupRoot"
Write-Host ""
Write-Host "Siguiente paso:" -ForegroundColor Cyan
Write-Host "  Abrir OpenPLC Editor y seleccionar JWPLC BASIC [2.0.0]."
Write-Host ""
Read-Host "Presiona ENTER para cerrar"
