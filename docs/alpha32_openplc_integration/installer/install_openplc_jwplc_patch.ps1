<#
.SYNOPSIS
  Instala el patch de integración OpenPLC para JWPLC Basic v2.0.0.

.DESCRIPTION
  Este script copia los archivos ubicados en:
    docs/alpha32_openplc_integration/open-plc-editor/
  sobre una instalación local de OpenPLC Editor v4.

  Antes de sobrescribir archivos, crea un backup automático con timestamp.

.NOTES
  No modifica el package Arduino platform-jwplc.
  No requiere permisos de administrador si OpenPLC está instalado en una carpeta del usuario.
#>

$ErrorActionPreference = "Stop"

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
Write-Host "Se creara un backup automatico antes de sobrescribir archivos." -ForegroundColor Yellow
Write-Host ""

$defaultCandidates = @(
    "$env:USERPROFILE\OpenPLC_Editor",
    "$env:USERPROFILE\OpenPLC_Editor_save1",
    "$env:USERPROFILE\Documents\OpenPLC_Editor",
    "$env:LOCALAPPDATA\Programs\OpenPLC Editor",
    "C:\Program Files\OpenPLC Editor",
    "C:\Program Files (x86)\OpenPLC Editor"
)

Write-Host "Rutas candidatas encontradas:" -ForegroundColor Cyan
$candidateIndex = 1
$validCandidates = @()
foreach ($candidate in $defaultCandidates) {
    if (Test-Path $candidate) {
        Write-Host "  [$candidateIndex] $candidate"
        $validCandidates += $candidate
        $candidateIndex++
    }
}

if ($validCandidates.Count -eq 0) {
    Write-Host "  No se encontraron rutas candidatas automaticamente."
}

Write-Host ""
$targetRoot = Read-Host "Ingresa la ruta raiz de OpenPLC Editor"
$targetRoot = $targetRoot.Trim('"')

if ([string]::IsNullOrWhiteSpace($targetRoot)) {
    Write-Error "Ruta vacia. Instalacion cancelada."
}

if (-not (Test-Path $targetRoot)) {
    Write-Error "La ruta indicada no existe: $targetRoot"
}

# Validaciones flexibles: la estructura puede variar segun version/distribucion de OpenPLC Editor.
$expectedHints = @(
    "editor",
    "resources",
    "arduino"
)

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

foreach ($file in $files) {
    $relativePath = $file.FullName.Substring($sourceRoot.Length).TrimStart('\', '/')
    $targetFile = Join-Path $targetRoot $relativePath
    $backupFile = Join-Path $backupRoot $relativePath

    if (Test-Path $targetFile) {
        $backupDir = Split-Path -Parent $backupFile
        New-Item -ItemType Directory -Path $backupDir -Force | Out-Null
        Copy-Item -Path $targetFile -Destination $backupFile -Force
    }
}

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
