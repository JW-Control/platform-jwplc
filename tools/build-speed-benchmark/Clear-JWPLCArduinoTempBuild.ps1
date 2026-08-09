[CmdletBinding()]
param(
    [string]$Sketch = "01_empty"
)

Set-StrictMode -Version 2.0
$ErrorActionPreference = "Stop"

$tempSketchRoot = Join-Path $env:LOCALAPPDATA "Temp\arduino\sketches"

if (-not (Test-Path $tempSketchRoot))
{
    Write-Host "No existe el directorio temporal de sketches Arduino." -ForegroundColor DarkGray
    exit 0
}

$removed = 0
$dirs = @(Get-ChildItem -Path $tempSketchRoot -Directory -ErrorAction SilentlyContinue)

foreach ($dir in $dirs)
{
    $generatedSketch = Join-Path $dir.FullName ("sketch\{0}.ino.cpp" -f $Sketch)

    if (Test-Path $generatedSketch)
    {
        Write-Host ("Eliminando build temporal: {0}" -f $dir.FullName) -ForegroundColor Yellow
        Remove-Item -Path $dir.FullName -Recurse -Force
        $removed++
    }
}

Write-Host ""
Write-Host ("Builds temporales eliminados para {0}: {1}" -f $Sketch, $removed) -ForegroundColor Green
Write-Host "No se tocaron builds de otros sketches." -ForegroundColor Cyan
