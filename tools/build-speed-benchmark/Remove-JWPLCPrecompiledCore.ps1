[CmdletBinding()]
param(
    [switch]$DeleteArchives
)

Set-StrictMode -Version 2.0
$ErrorActionPreference = "Stop"

$ScriptRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$RepoRoot = [System.IO.Path]::GetFullPath((Join-Path $ScriptRoot "..\.."))
$PlatformRoot = Join-Path $RepoRoot "JWPLC\2.1.0"
$BoardsLocalPath = Join-Path $PlatformRoot "boards.local.txt"

function Remove-P2Block
{
    param([string]$Text)

    if ([string]::IsNullOrEmpty($Text))
    {
        return ""
    }

    $pattern = '(?ms)^# BEGIN JWPLC_P2_PRECOMPILED_CORE\r?\n.*?^# END JWPLC_P2_PRECOMPILED_CORE\r?\n?'
    return [regex]::Replace($Text, $pattern, "").TrimEnd()
}

if (Test-Path $BoardsLocalPath)
{
    $existing = Get-Content -Path $BoardsLocalPath -Raw
    $clean = Remove-P2Block -Text $existing

    if ([string]::IsNullOrWhiteSpace($clean))
    {
        Remove-Item -Path $BoardsLocalPath -Force
        Write-Host "boards.local.txt eliminado: no quedaba contenido fuera de P2." -ForegroundColor Green
    }
    else
    {
        ($clean + [Environment]::NewLine) | Out-File -FilePath $BoardsLocalPath -Encoding ascii
        Write-Host "Bloque P2 eliminado de boards.local.txt." -ForegroundColor Green
    }
}
else
{
    Write-Host "boards.local.txt no existe; P2 ya estaba desactivado." -ForegroundColor DarkGray
}

if ($DeleteArchives)
{
    $archiveRoot = Join-Path $PlatformRoot "precompiled\core"
    if (Test-Path $archiveRoot)
    {
        Remove-Item -Path $archiveRoot -Recurse -Force
        Write-Host ("Archives P2 eliminados: {0}" -f $archiveRoot) -ForegroundColor Yellow
    }
}

Write-Host ""
Write-Host "Rollback P2 terminado. JWPLC vuelve a build.core=jwcontrol desde boards.txt." -ForegroundColor Cyan
if (-not $DeleteArchives)
{
    Write-Host "Los core.a generados se conservaron inactivos para inspeccion." -ForegroundColor DarkGray
}
