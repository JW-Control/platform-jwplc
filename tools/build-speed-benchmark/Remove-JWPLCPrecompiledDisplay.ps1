[CmdletBinding()]
param()

Set-StrictMode -Version 2.0
$ErrorActionPreference = "Stop"

$ScriptRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$RepoRoot = [System.IO.Path]::GetFullPath((Join-Path $ScriptRoot "..\.."))
$ArchivePath = Join-Path $RepoRoot "JWPLC\2.1.0\libraries\JWPLC_Display\src\esp32\libJWPLC_Display.a"

if (Test-Path $ArchivePath)
{
    Remove-Item $ArchivePath -Force
    Write-Host ("P3 removido: {0}" -f $ArchivePath) -ForegroundColor Green
}
else
{
    Write-Host "P3 ya estaba inactivo: no existe libJWPLC_Display.a." -ForegroundColor DarkGray
}

Write-Host "JWPLC_Display mantiene precompiled=full y volvera automaticamente a compilar desde fuentes." -ForegroundColor Cyan
