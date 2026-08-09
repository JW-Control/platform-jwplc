[CmdletBinding()]
param(
    [string[]]$Libraries = @(
        "JW_RTC",
        "JW_FRAM",
        "JW_SD",
        "JW_MatrixButtons",
        "JWPLC_ModbusRTU"
    )
)

Set-StrictMode -Version 2.0
$ErrorActionPreference = "Stop"

$ScriptRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$RepoRoot = [System.IO.Path]::GetFullPath((Join-Path $ScriptRoot "..\.."))
$LibraryRoot = Join-Path $RepoRoot "JWPLC\2.1.0\libraries"

$removed = 0

foreach ($library in $Libraries)
{
    $archive = Join-Path (Join-Path (Join-Path $LibraryRoot $library) "src\esp32") ("lib{0}.a" -f $library)

    if (Test-Path $archive)
    {
        Remove-Item -Path $archive -Force
        Write-Host ("Eliminado: {0}" -f $archive) -ForegroundColor Yellow
        $removed++
    }
    else
    {
        Write-Host ("No existe: {0}" -f $archive) -ForegroundColor DarkGray
    }
}

Write-Host ""
Write-Host ("Rollback P1 terminado. Archivos eliminados: {0}" -f $removed) -ForegroundColor Green
Write-Host "Con precompiled=full y sin .a, Arduino vuelve automáticamente a compilar las fuentes." -ForegroundColor Cyan
