[CmdletBinding()]
param(
    [string]$LogPath = ""
)

Set-StrictMode -Version 2.0
$ErrorActionPreference = "Stop"

$ScriptRoot = Split-Path -Parent $MyInvocation.MyCommand.Path

function Find-LatestP3Log
{
    $root = Join-Path $ScriptRoot "p3-work"
    if (-not (Test-Path $root))
    {
        return ""
    }

    foreach ($run in @(Get-ChildItem $root -Directory | Sort-Object LastWriteTime -Descending))
    {
        $candidate = Join-Path $run.FullName "verify-Basic.log"
        if (Test-Path $candidate)
        {
            return $candidate
        }
    }

    return ""
}

if ([string]::IsNullOrWhiteSpace($LogPath))
{
    $LogPath = Find-LatestP3Log
}

if ([string]::IsNullOrWhiteSpace($LogPath) -or -not (Test-Path $LogPath))
{
    throw "No se encontro verify-Basic.log de P3. Usa -LogPath con la ruta exacta."
}

$LogPath = (Resolve-Path $LogPath).Path
$lines = @(Get-Content $LogPath)

Write-Host "P3 - diagnostico rapido" -ForegroundColor Cyan
Write-Host ("Log: {0}" -f $LogPath)
Write-Host ""

$patterns = @(
    'undefined reference',
    'multiple definition',
    'cannot find',
    'No such file',
    'fatal error',
    'collect2:',
    'ld returned',
    'error:'
)

$hits = New-Object System.Collections.Generic.List[string]
foreach ($line in $lines)
{
    foreach ($pattern in $patterns)
    {
        if ($line -match $pattern)
        {
            $hits.Add([string]$line)
            break
        }
    }
}

Write-Host "=== Errores detectados ===" -ForegroundColor Yellow
if ($hits.Count -gt 0)
{
    $hits | Select-Object -Unique | ForEach-Object { Write-Host $_ }
}
else
{
    Write-Host "No se detectaron patrones clasicos; mostrando las ultimas 80 lineas." -ForegroundColor DarkYellow
    $lines | Select-Object -Last 80 | ForEach-Object { Write-Host $_ }
}

Write-Host ""
Write-Host "=== Evidencia JWPLC_Display precompilado ===" -ForegroundColor Yellow
$displayLines = @($lines | Where-Object {
    ([string]$_) -match 'JWPLC_Display|libJWPLC_Display|precompiled'
})
if ($displayLines.Count -gt 0)
{
    $displayLines | Select-Object -Last 40 | ForEach-Object { Write-Host $_ }
}
else
{
    Write-Host "No aparecieron lineas relacionadas con JWPLC_Display/precompiled."
}

Write-Host ""
Write-Host "=== Comando de enlace final ===" -ForegroundColor Yellow
$linkLines = @($lines | Where-Object {
    $s = [string]$_
    ($s -match 'xtensa-esp32-elf-g\+\+') -and ($s -match '--start-group')
})
if ($linkLines.Count -gt 0)
{
    $linkLines | Select-Object -Last 1 | ForEach-Object { Write-Host $_ }
}
else
{
    Write-Host "No se encontro comando de enlace con --start-group."
}

Write-Host ""
Write-Host "Copia y comparte desde 'Errores detectados' hasta el final." -ForegroundColor Green
