<#
.SYNOPSIS
  Aplica el patch minimo requerido para JWPLC Basic sobre OpenPLC Editor 4.2.7.

.DESCRIPTION
  OpenPLC Editor 4.2.7 ya carga placas externas mediante VPP, pero el runtime
  Arduino/Baremetal incluido por defecto sigue declarando pinMask_* como uint8_t.
  JWPLC Basic usa pines virtuales uint16_t (0x22xx), por lo que se requiere
  adaptar Baremetal.ino y reutilizar la adaptacion Modbus TCP/W5500 ya validada.

  Este script:
  - detecta la raiz de OpenPLC Editor;
  - crea backup con timestamp;
  - corrige los extern pinMask_* de Baremetal.ino para JWPLC_BASIC;
  - copia ModbusSlave.cpp/.h adaptados desde installers/openplc-jwplc-basic-v2.0.0.

.NOTES
  No modifica platform-jwplc.
  No modifica boards.txt/platform.txt del package Arduino.
  No firma ni instala el VPP.
#>

$ErrorActionPreference = "Stop"

function Test-OpenPLCRoot {
    param([string]$Path)
    if ([string]::IsNullOrWhiteSpace($Path)) { return $false }
    return (Test-Path (Join-Path $Path "resources\sources\Baremetal\Baremetal.ino"))
}

function Resolve-OpenPLCRoot {
    param([string]$InputPath)
    $InputPath = $InputPath.Trim('"')
    $candidates = @(
        $InputPath,
        (Join-Path $InputPath "editor\arduino"),
        (Join-Path $InputPath "editor")
    )
    foreach ($candidate in $candidates) {
        if (Test-OpenPLCRoot $candidate) { return (Resolve-Path $candidate).Path }
    }
    return $InputPath
}

function Get-RepoRoot {
    $here = Resolve-Path (Join-Path $PSScriptRoot "..")
    $cursor = $here.Path
    while ($true) {
        if (Test-Path (Join-Path $cursor ".git")) { return $cursor }
        $parent = Split-Path -Parent $cursor
        if ($parent -eq $cursor -or [string]::IsNullOrWhiteSpace($parent)) { break }
        $cursor = $parent
    }
    return (Resolve-Path (Join-Path $PSScriptRoot "..\..\.." )).Path
}

Write-Host "==============================================" -ForegroundColor Cyan
Write-Host " JWPLC Basic - OpenPLC Editor 4.2.7 Patch" -ForegroundColor Cyan
Write-Host "==============================================" -ForegroundColor Cyan
Write-Host ""

$repoRoot = Get-RepoRoot
$legacyPatchRoot = Join-Path $repoRoot "installers\openplc-jwplc-basic-v2.0.0\open-plc-editor"
$legacyBaremetal = Join-Path $legacyPatchRoot "resources\sources\Baremetal"

if (-not (Test-Path $legacyBaremetal)) {
    throw "No se encontro el patch validado previo: $legacyBaremetal"
}

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
$validCandidates = @()
$idx = 1
foreach ($candidate in $defaultCandidates) {
    if (Test-Path $candidate) {
        $resolved = Resolve-OpenPLCRoot $candidate
        if (Test-OpenPLCRoot $resolved) {
            Write-Host "  [$idx] $resolved"
            $validCandidates += $resolved
            $idx++
        }
    }
}
if ($validCandidates.Count -eq 0) { Write-Host "  No se encontraron rutas candidatas automaticamente." }

Write-Host ""
$targetInput = Read-Host "Ingresa ruta raiz de OpenPLC Editor 4.2.7 o numero de ruta candidata"
if ([string]::IsNullOrWhiteSpace($targetInput)) { throw "Entrada vacia. Instalacion cancelada." }

[int]$selectedIndex = 0
if ([int]::TryParse($targetInput, [ref]$selectedIndex) -and $selectedIndex -ge 1 -and $selectedIndex -le $validCandidates.Count) {
    $targetRoot = $validCandidates[$selectedIndex - 1]
}
else {
    $targetRoot = Resolve-OpenPLCRoot $targetInput
}

if (-not (Test-OpenPLCRoot $targetRoot)) {
    throw "La ruta no parece ser una raiz valida de OpenPLC Editor 4.2.7: $targetRoot"
}

$timestamp = Get-Date -Format "yyyyMMdd_HHmmss"
$backupRoot = Join-Path $targetRoot "backup_jwplc_openplc_427_$timestamp"
New-Item -ItemType Directory -Path $backupRoot -Force | Out-Null

$targetBaremetal = Join-Path $targetRoot "resources\sources\Baremetal"
$filesToBackup = @(
    (Join-Path $targetBaremetal "Baremetal.ino"),
    (Join-Path $targetBaremetal "ModbusSlave.cpp"),
    (Join-Path $targetBaremetal "ModbusSlave.h")
)

foreach ($file in $filesToBackup) {
    if (Test-Path $file) {
        Copy-Item $file -Destination (Join-Path $backupRoot (Split-Path -Leaf $file)) -Force
    }
}

Write-Host "Backup creado en: $backupRoot" -ForegroundColor Cyan

# 1) Corregir Baremetal.ino para que JWPLC use uint16_t en pinMask_*.
$baremetalPath = Join-Path $targetBaremetal "Baremetal.ino"
$baremetal = Get-Content $baremetalPath -Raw
$oldBlock = @'
extern uint8_t pinMask_DIN[];
extern uint8_t pinMask_AIN[];
extern uint8_t pinMask_DOUT[];
extern uint8_t pinMask_AOUT[];
'@
$newBlock = @'
#if defined(JWPLC_BASIC)
extern uint16_t pinMask_DIN[];
extern uint16_t pinMask_AIN[];
extern uint16_t pinMask_DOUT[];
extern uint16_t pinMask_AOUT[];
#else
extern uint8_t pinMask_DIN[];
extern uint8_t pinMask_AIN[];
extern uint8_t pinMask_DOUT[];
extern uint8_t pinMask_AOUT[];
#endif
'@

if ($baremetal.Contains($newBlock)) {
    Write-Host "Baremetal.ino ya tenia patch uint16_t JWPLC." -ForegroundColor Yellow
}
elseif ($baremetal.Contains($oldBlock)) {
    $baremetal = $baremetal.Replace($oldBlock, $newBlock)
    Set-Content -Path $baremetalPath -Value $baremetal -Encoding UTF8
    Write-Host "OK Baremetal.ino: pinMask_* compatible con JWPLC_BASIC." -ForegroundColor Green
}
else {
    Write-Host "ADVERTENCIA: no se encontro bloque pinMask_* esperado en Baremetal.ino." -ForegroundColor Yellow
    Write-Host "Revisar manualmente si OpenPLC cambio el runtime Baremetal." -ForegroundColor Yellow
}

# 2) Copiar ModbusSlave adaptado previamente para W5500/JWPLC_Ethernet.
Copy-Item -Path (Join-Path $legacyBaremetal "ModbusSlave.cpp") -Destination (Join-Path $targetBaremetal "ModbusSlave.cpp") -Force
Copy-Item -Path (Join-Path $legacyBaremetal "ModbusSlave.h") -Destination (Join-Path $targetBaremetal "ModbusSlave.h") -Force
Write-Host "OK ModbusSlave.cpp/.h: adaptacion JWPLC Ethernet/W5500 copiada." -ForegroundColor Green

Write-Host ""
Write-Host "Patch OpenPLC 4.2.7 aplicado." -ForegroundColor Green
Write-Host "Siguiente paso: importar/instalar el paquete VPP JWPLC cuando este firmado." -ForegroundColor Cyan
