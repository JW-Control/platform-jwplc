#requires -Version 7.4

[CmdletBinding()]
param()

Set-StrictMode -Version 2.0
$ErrorActionPreference = "Stop"

$ScriptRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$RepoRoot = [System.IO.Path]::GetFullPath((Join-Path $ScriptRoot "..\.."))
$PlatformRoot = Join-Path $RepoRoot "JWPLC\2.1.0"
$LibrariesRoot = Join-Path $PlatformRoot "libraries"

$GfxRoot = Join-Path $LibrariesRoot "Adafruit_GFX_Library"
$GfxProperties = Join-Path $GfxRoot "library.properties"
$GfxSrc = Join-Path $GfxRoot "src"
$GfxMarker = Join-Path $GfxRoot "JWPLC_Bundled_Adafruit_GFX.h"
$GfxFonts = Join-Path $GfxRoot "Fonts"

$StRoot = Join-Path $LibrariesRoot "Adafruit_ST7735_and_ST7789_Library"
$StProperties = Join-Path $StRoot "library.properties"
$StArchive = Join-Path $StRoot "src\esp32\libAdafruit_ST7735_and_ST7789_Library.a"

$P6A2RunId = "20260809_232946"
$P6A2Root = Join-Path (Join-Path $ScriptRoot "p6a2-st77xx-precompiled-work") $P6A2RunId
$P6A2Build = Join-Path $P6A2Root "p6a2-Basic"
$P6A2Log = Join-Path $P6A2Root "p6a2-Basic.log"

$ReportRoot = Join-Path $ScriptRoot "p6b-gfx-work"
$ReportPath = Join-Path $ReportRoot "P6B_GFX_PREPARATION_AUDIT.md"

$ExpectedGfxSources = @(
    "Adafruit_GFX.cpp",
    "Adafruit_GrayOLED.cpp",
    "Adafruit_SPITFT.cpp",
    "glcdfont.c"
)

function Get-PropertyValue
{
    param(
        [Parameter(Mandatory = $true)][string]$Path,
        [Parameter(Mandatory = $true)][string]$Name
    )

    foreach ($line in Get-Content -LiteralPath $Path)
    {
        if ($line -match ('^' + [regex]::Escape($Name) + '=(?<value>.*)$'))
        {
            return $Matches["value"].Trim()
        }
    }
    return $null
}

function ConvertTo-EntryList
{
    param([Parameter(Mandatory = $true)]$Parsed)

    $entries = New-Object System.Collections.Generic.List[object]
    foreach ($entry in $Parsed)
    {
        if ($null -eq $entry) { throw "compile_commands.json contiene una entrada nula." }
        [void]$entries.Add($entry)
    }
    if ($entries.Count -eq 0) { throw "compile_commands.json esta vacio." }
    return $entries
}

function Get-CompileMetrics
{
    param([Parameter(Mandatory = $true)][string]$BuildPath)

    $dbPath = Join-Path $BuildPath "compile_commands.json"
    if (-not (Test-Path -LiteralPath $dbPath)) { throw "Falta compile_commands.json: $dbPath" }

    $entries = ConvertTo-EntryList -Parsed (Get-Content -LiteralPath $dbPath -Raw | ConvertFrom-Json)
    $files = @($entries | ForEach-Object { [string]$_.file })

    return [PSCustomObject]@{
        Entries = @($entries)
        Total = $entries.Count
        ST77xx = @($files | Where-Object { $_ -match '[\\/]libraries[\\/]Adafruit_ST7735_and_ST7789_Library[\\/]' }).Count
        GFX = @($files | Where-Object { $_ -match '[\\/]libraries[\\/]Adafruit_GFX_Library[\\/]' }).Count
        BusIO = @($files | Where-Object { $_ -match '[\\/]libraries[\\/]Adafruit_BusIO[\\/]' }).Count
        Ethernet = @($files | Where-Object { $_ -match '[\\/]libraries[\\/]JWPLC_Ethernet_W5x00_Backend[\\/]' }).Count
        Display = @($files | Where-Object { $_ -match '[\\/]libraries[\\/]JWPLC_Display[\\/]' }).Count
        Stub = @($files | Where-Object { $_ -match '[\\/]cores[\\/]jwcontrol_precompiled_stub[\\/]p2_core_stub\.c$' }).Count
    }
}

function Get-PreprocessCount
{
    param([Parameter(Mandatory = $true)][string]$LogPath)
    if (-not (Test-Path -LiteralPath $LogPath)) { throw "Falta log: $LogPath" }
    return @(Get-Content -LiteralPath $LogPath |
        Where-Object { ([string]$_) -match 'xtensa-esp32-elf-g\+\+.*\s-E\s' }).Count
}

function Get-CompiledSourceNames
{
    param(
        [Parameter(Mandatory = $true)][object[]]$Entries,
        [Parameter(Mandatory = $true)][string]$LibraryFolder
    )

    return @($Entries |
        ForEach-Object { [string]$_.file } |
        Where-Object { $_ -match ('[\\/]libraries[\\/]' + [regex]::Escape($LibraryFolder) + '[\\/]') } |
        ForEach-Object { [System.IO.Path]::GetFileName($_) } |
        Sort-Object -Unique)
}

Write-Host "JWPLC - P6B Adafruit GFX preparation / sin compilar" -ForegroundColor Cyan
Write-Host ("PowerShell: {0} / {1}" -f $PSVersionTable.PSVersion, $PSVersionTable.PSEdition)
Write-Host ("Baseline P6A-2: {0}" -f $P6A2Root)
Write-Host ""

foreach ($required in @($GfxRoot, $GfxProperties, $GfxMarker, $GfxFonts, $StProperties, $StArchive, $P6A2Build, $P6A2Log))
{
    if (-not (Test-Path -LiteralPath $required)) { throw "Falta requisito P6B: $required" }
}

if ((Get-PropertyValue -Path $StProperties -Name "precompiled") -ne "full")
{
    throw "P6B requiere P6A-2 activo: ST77xx no declara precompiled=full."
}

$gfxName = Get-PropertyValue -Path $GfxProperties -Name "name"
$gfxVersion = Get-PropertyValue -Path $GfxProperties -Name "version"
$gfxPrecompiled = Get-PropertyValue -Path $GfxProperties -Name "precompiled"
if ($gfxName -ne "Adafruit GFX Library") { throw "Nombre GFX inesperado: $gfxName" }
if ($gfxVersion -ne "1.12.4") { throw "Version GFX inesperada: $gfxVersion" }
if (-not [string]::IsNullOrWhiteSpace([string]$gfxPrecompiled))
{
    throw ("P6B preparacion requiere GFX aun sin precompiled=. Actual={0}" -f $gfxPrecompiled)
}
if (Test-Path -LiteralPath $GfxSrc)
{
    throw "P6B preparacion esperaba Adafruit GFX aun en layout flat/root; ya existe src/."
}

$metrics = Get-CompileMetrics -BuildPath $P6A2Build
$preprocess = Get-PreprocessCount -LogPath $P6A2Log
if ($metrics.Total -ne 20 -or $metrics.ST77xx -ne 0 -or $metrics.GFX -ne 4 -or
    $metrics.BusIO -ne 4 -or $metrics.Ethernet -ne 0 -or $metrics.Display -ne 0 -or
    $metrics.Stub -ne 1 -or $preprocess -ne 37)
{
    throw ("Baseline P6A-2 inesperado: total/ST/GFX/BusIO/Eth/Display/stub/-E={0}/{1}/{2}/{3}/{4}/{5}/{6}/{7}" -f $metrics.Total, $metrics.ST77xx, $metrics.GFX, $metrics.BusIO, $metrics.Ethernet, $metrics.Display, $metrics.Stub, $preprocess)
}

$compiledGfx = @(Get-CompiledSourceNames -Entries $metrics.Entries -LibraryFolder "Adafruit_GFX_Library")
$expectedSorted = @($ExpectedGfxSources | Sort-Object)
if (($compiledGfx -join "|") -ne ($expectedSorted -join "|"))
{
    throw ("TUs GFX inesperados. Actual={0}; esperado={1}" -f ($compiledGfx -join ","), ($expectedSorted -join ","))
}

$rootCodeFiles = @(Get-ChildItem -LiteralPath $GfxRoot -File |
    Where-Object { $_.Extension -in @(".c", ".cc", ".cpp", ".h", ".hpp", ".S") } |
    Sort-Object Name)

$rootSourceFiles = @($rootCodeFiles | Where-Object { $_.Extension -in @(".c", ".cc", ".cpp", ".S") })
$rootHeaderFiles = @($rootCodeFiles | Where-Object { $_.Extension -in @(".h", ".hpp") })

$rootSourceNames = @($rootSourceFiles | Select-Object -ExpandProperty Name | Sort-Object)
if (($rootSourceNames -join "|") -ne ($expectedSorted -join "|"))
{
    throw ("Fuentes compilables en raiz GFX no coinciden con baseline. Root={0}; baseline={1}" -f ($rootSourceNames -join ","), ($expectedSorted -join ","))
}

$fontFiles = @(Get-ChildItem -LiteralPath $GfxFonts -Recurse -File | Sort-Object FullName)
$fontCode = @($fontFiles | Where-Object { $_.Extension -in @(".c", ".cc", ".cpp", ".S") })
if ($fontCode.Count -ne 0)
{
    throw "Fonts/ contiene fuentes compilables inesperadas; revisar antes de migrar layout."
}

$rootBytes = [int64]0
foreach ($file in $rootCodeFiles) { $rootBytes += [int64]$file.Length }
$fontBytes = [int64]0
foreach ($file in $fontFiles) { $fontBytes += [int64]$file.Length }

$topDirs = @(Get-ChildItem -LiteralPath $GfxRoot -Directory | Select-Object -ExpandProperty Name | Sort-Object)

Write-Host "Quality gate P6A-2 activo: OK | 20 compiles | GFX=4 | BusIO=4 | -E=37" -ForegroundColor Green
Write-Host ("GFX: {0} {1} | layout=flat/root | precompiled=ausente" -f $gfxName, $gfxVersion) -ForegroundColor Green
Write-Host ("TUs GFX baseline: {0}" -f ($compiledGfx -join ", "))
Write-Host ("Archivos codigo/header en raiz a migrar: {0} ({1} fuentes + {2} headers) | {3} bytes" -f $rootCodeFiles.Count, $rootSourceFiles.Count, $rootHeaderFiles.Count, $rootBytes)
Write-Host ("Fonts/: {0} archivos | {1} bytes | fuentes compilables=0" -f $fontFiles.Count, $fontBytes)
Write-Host ("Directorios top-level actuales: {0}" -f ($topDirs -join ", "))
Write-Host "Marker bundled en raiz: True" -ForegroundColor Green

New-Item -ItemType Directory -Path $ReportRoot -Force | Out-Null
$report = New-Object System.Collections.Generic.List[string]
[void]$report.Add("# P6B - Preparacion Adafruit GFX")
[void]$report.Add("")
[void]$report.Add(("Baseline P6A-2: {0}" -f $P6A2RunId))
[void]$report.Add(("Compiles / -E: {0} / {1}" -f $metrics.Total, $preprocess))
[void]$report.Add(("GFX: {0} {1}" -f $gfxName, $gfxVersion))
[void]$report.Add(("TUs GFX: {0}" -f ($compiledGfx -join ", ")))
[void]$report.Add("")
[void]$report.Add("## Inventario a migrar a src/")
[void]$report.Add("")
foreach ($file in $rootCodeFiles)
{
    [void]$report.Add(("- {0} | {1} B | SHA256 {2}" -f $file.Name, $file.Length, ((Get-FileHash -LiteralPath $file.FullName -Algorithm SHA256).Hash.ToLowerInvariant())))
}
[void]$report.Add("")
[void]$report.Add(("Fonts/: {0} archivos, {1} B. Debe moverse como directorio publico bajo src/Fonts para conservar includes del tipo Fonts/..." -f $fontFiles.Count, $fontBytes))
[void]$report.Add("")
[void]$report.Add(("Directorios top-level observados: {0}" -f ($topDirs -join ", ")))
[void]$report.Add("")
[void]$report.Add("Decision propuesta: P6B-1 debe migrar solamente los archivos de codigo/header de raiz y Fonts/ a src/, manteniendo examples/fontconvert/metadata fuera de src/. Primero validar source-only; despues crear el archive P6B-2.")
$report | Set-Content -LiteralPath $ReportPath -Encoding utf8NoBOM

Write-Host ""
Write-Host "=== P6B PREPARACION: VALIDADA ===" -ForegroundColor Green
Write-Host ("Reporte: {0}" -f $ReportPath) -ForegroundColor DarkGray
Write-Host "No se movio ningun archivo y no se ejecuto ninguna compilacion." -ForegroundColor DarkGray
Write-Host "Siguiente gate: P6B-1 layout src/ source-only para Adafruit GFX." -ForegroundColor Yellow
