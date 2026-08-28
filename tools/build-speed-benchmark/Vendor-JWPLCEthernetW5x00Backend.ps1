[CmdletBinding()]
param(
    [string]$Version = "2.0.2",
    [switch]$Force
)

Set-StrictMode -Version 2.0
$ErrorActionPreference = "Stop"

$ScriptRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$RepoRoot = [System.IO.Path]::GetFullPath((Join-Path $ScriptRoot "..\.."))
$UnifiedRoot = Join-Path $RepoRoot "JWPLC\2.1.0\libraries\JWPLC_Ethernet"
$UpstreamDoc = Join-Path $UnifiedRoot "third_party\arduino-ethernet-2.0.2\UPSTREAM.md"

Write-Host "JWPLC - vendor Ethernet W5x00 legacy" -ForegroundColor Yellow
Write-Host ""
Write-Host "Este script ya no modifica el repositorio." -ForegroundColor Yellow
Write-Host ""
Write-Host "Desde Alpha6, Arduino Ethernet/W5x00 esta consolidado dentro de:" -ForegroundColor Cyan
Write-Host ("  {0}" -f $UnifiedRoot)
Write-Host ""
Write-Host "La antigua libreria JWPLC_Ethernet_W5x00_Backend fue retirada deliberadamente." -ForegroundColor Cyan
Write-Host "Recrearla volveria a introducir una segunda libreria Ethernet y rompería el contrato de seleccion unificada." -ForegroundColor Cyan
Write-Host ""

if (Test-Path -LiteralPath $UpstreamDoc)
{
    Write-Host ("Metadata upstream conservada en: {0}" -f $UpstreamDoc) -ForegroundColor DarkGray
}

if ($Force)
{
    Write-Warning "-Force ya no tiene efecto. No se recreara el backend separado."
}

if ($Version -ne "2.0.2")
{
    Write-Warning ("Version solicitada: {0}. La base actualmente documentada sigue siendo Arduino Ethernet 2.0.2." -f $Version)
}

throw "VENDORIZADO_LEGACY_BLOQUEADO: usar JWPLC_Ethernet unificado; cualquier actualización upstream debe integrarse y validarse dentro de esa libreria."
