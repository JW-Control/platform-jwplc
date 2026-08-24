#requires -Version 7.4

[CmdletBinding()]
param()

Set-StrictMode -Version 2.0
$ErrorActionPreference = "Stop"

$ScriptRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$RepoRoot = [System.IO.Path]::GetFullPath((Join-Path $ScriptRoot "..\.."))
$PlatformRoot = Join-Path $RepoRoot "JWPLC\2.1.0"
$LibrariesRoot = Join-Path $PlatformRoot "libraries"

$BusRoot = Join-Path $LibrariesRoot "Adafruit_BusIO"
$BusProperties = Join-Path $BusRoot "library.properties"
$BusSrc = Join-Path $BusRoot "src"
$BusMarker = Join-Path $BusRoot "JWPLC_Bundled_Adafruit_BusIO.h"

$GfxRoot = Join-Path $LibrariesRoot "Adafruit_GFX_Library"
$GfxProperties = Join-Path $GfxRoot "library.properties"
$GfxArchive = Join-Path $GfxRoot "src\esp32\libAdafruit_GFX_Library.a"

$StRoot = Join-Path $LibrariesRoot "Adafruit_ST7735_and_ST7789_Library"
$StProperties = Join-Path $StRoot "library.properties"
$StArchive = Join-Path $StRoot "src\esp32\libAdafruit_ST7735_and_ST7789_Library.a"

$P6B2RunId = "20260809_235326"
$P6B2Root = Join-Path (Join-Path $ScriptRoot "p6b2-gfx-precompiled-work") $P6B2RunId
$P6B2Build = Join-Path $P6B2Root "p6b2-Basic"
$P6B2Log = Join-Path $P6B2Root "p6b2-Basic.log"
$P6B2Inspector = Join-Path $ScriptRoot "Inspect-JWPLCP6B2ExistingRun.ps1"

$ReportRoot = Join-Path $ScriptRoot "p6c-busio-work"
$ReportPath = Join-Path $ReportRoot "P6C_BUSIO_PREPARATION_AUDIT.md"

$ExpectedBusSources = @(
    "Adafruit_BusIO_Register.cpp",
    "Adafruit_GenericDevice.cpp",
    "Adafruit_I2CDevice.cpp",
    "Adafruit_SPIDevice.cpp"
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
        if ($null -eq $entry) { throw "compile_commands.json contiene entrada nula." }
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

Write-Host "JWPLC - P6C Adafruit BusIO preparation / sin compilar" -ForegroundColor Cyan
Write-Host ("PowerShell: {0} / {1}" -f $PSVersionTable.PSVersion, $PSVersionTable.PSEdition)
Write-Host ("Baseline P6B-2: {0}" -f $P6B2Root)
Write-Host ""

foreach ($required in @(
    $BusRoot, $BusProperties, $BusMarker,
    $GfxProperties, $GfxArchive,
    $StProperties, $StArchive,
    $P6B2Build, $P6B2Log, $P6B2Inspector
))
{
    if (-not (Test-Path -LiteralPath $required)) { throw "Falta requisito P6C: $required" }
}

if ((Get-PropertyValue -Path $StProperties -Name "precompiled") -ne "full")
{
    throw "P6C requiere P6A-2 activo: ST77xx no declara precompiled=full."
}
if ((Get-PropertyValue -Path $GfxProperties -Name "precompiled") -ne "full")
{
    throw "P6C requiere P6B-2 activo: GFX no declara precompiled=full."
}

Write-Host "Revalidando run P6B-2 preservado..." -ForegroundColor Cyan
& $P6B2Inspector
Write-Host "Quality gate P6B-2 preservado: OK" -ForegroundColor Green
Write-Host ""

$busName = Get-PropertyValue -Path $BusProperties -Name "name"
$busVersion = Get-PropertyValue -Path $BusProperties -Name "version"
$busPrecompiled = Get-PropertyValue -Path $BusProperties -Name "precompiled"
if ($busName -ne "Adafruit BusIO") { throw "Nombre BusIO inesperado: $busName" }
if ($busVersion -ne "1.17.4") { throw "Version BusIO inesperada: $busVersion" }
if (-not [string]::IsNullOrWhiteSpace([string]$busPrecompiled))
{
    throw ("P6C preparacion requiere BusIO aun sin precompiled=. Actual={0}" -f $busPrecompiled)
}
if (Test-Path -LiteralPath $BusSrc)
{
    throw "P6C preparacion esperaba Adafruit BusIO aun en layout flat/root; ya existe src/."
}

$metrics = Get-CompileMetrics -BuildPath $P6B2Build
$preprocess = Get-PreprocessCount -LogPath $P6B2Log
if ($metrics.Total -ne 16 -or $metrics.ST77xx -ne 0 -or $metrics.GFX -ne 0 -or
    $metrics.BusIO -ne 4 -or $metrics.Ethernet -ne 0 -or $metrics.Display -ne 0 -or
    $metrics.Stub -ne 1 -or $preprocess -ne 33)
{
    throw ("Baseline P6B-2 inesperado: total/ST/GFX/BusIO/Eth/Display/stub/-E={0}/{1}/{2}/{3}/{4}/{5}/{6}/{7}" -f $metrics.Total, $metrics.ST77xx, $metrics.GFX, $metrics.BusIO, $metrics.Ethernet, $metrics.Display, $metrics.Stub, $preprocess)
}

$compiledBus = @(Get-CompiledSourceNames -Entries $metrics.Entries -LibraryFolder "Adafruit_BusIO")
$expectedSorted = @($ExpectedBusSources | Sort-Object)
if (($compiledBus -join "|") -ne ($expectedSorted -join "|"))
{
    throw ("TUs BusIO inesperados. Actual={0}; esperado={1}" -f ($compiledBus -join ","), ($expectedSorted -join ","))
}

$rootCodeFiles = @(Get-ChildItem -LiteralPath $BusRoot -File |
    Where-Object { $_.Extension -in @(".c", ".cc", ".cpp", ".h", ".hpp", ".S") } |
    Sort-Object Name)
$rootSourceFiles = @($rootCodeFiles | Where-Object { $_.Extension -in @(".c", ".cc", ".cpp", ".S") })
$rootHeaderFiles = @($rootCodeFiles | Where-Object { $_.Extension -in @(".h", ".hpp") })
$rootSourceNames = @($rootSourceFiles | Select-Object -ExpandProperty Name | Sort-Object)
if (($rootSourceNames -join "|") -ne ($expectedSorted -join "|"))
{
    throw ("Fuentes compilables en raiz BusIO no coinciden con baseline. Root={0}; baseline={1}" -f ($rootSourceNames -join ","), ($expectedSorted -join ","))
}

$rootBytes = [int64]0
foreach ($file in $rootCodeFiles) { $rootBytes += [int64]$file.Length }
$topDirs = @(Get-ChildItem -LiteralPath $BusRoot -Directory | Select-Object -ExpandProperty Name | Sort-Object)

Write-Host "Quality gate P6B-2 activo: OK | 16 compiles | BusIO=4 | ST77xx=0 | GFX=0 | -E=33" -ForegroundColor Green
Write-Host ("BusIO: {0} {1} | layout=flat/root | precompiled=ausente" -f $busName, $busVersion) -ForegroundColor Green
Write-Host ("TUs BusIO baseline: {0}" -f ($compiledBus -join ", "))
Write-Host ("Archivos codigo/header en raiz a migrar: {0} ({1} fuentes + {2} headers) | {3} bytes" -f $rootCodeFiles.Count, $rootSourceFiles.Count, $rootHeaderFiles.Count, $rootBytes)
Write-Host ("Directorios top-level actuales: {0}" -f ($topDirs -join ", "))
Write-Host "Marker bundled en raiz: True" -ForegroundColor Green

New-Item -ItemType Directory -Path $ReportRoot -Force | Out-Null
$report = New-Object System.Collections.Generic.List[string]
[void]$report.Add("# P6C - Preparacion Adafruit BusIO")
[void]$report.Add("")
[void]$report.Add(("Baseline P6B-2: {0}" -f $P6B2RunId))
[void]$report.Add(("Compiles / -E: {0} / {1}" -f $metrics.Total, $preprocess))
[void]$report.Add(("BusIO: {0} {1}" -f $busName, $busVersion))
[void]$report.Add(("TUs BusIO: {0}" -f ($compiledBus -join ", ")))
[void]$report.Add("")
[void]$report.Add("## Inventario raiz a migrar a src/")
[void]$report.Add("")
foreach ($file in $rootCodeFiles)
{
    [void]$report.Add(("- {0} | {1} B | SHA256 {2}" -f $file.Name, $file.Length, ((Get-FileHash -LiteralPath $file.FullName -Algorithm SHA256).Hash.ToLowerInvariant())))
}
[void]$report.Add("")
[void]$report.Add(("Directorios top-level observados: {0}" -f ($topDirs -join ", ")))
[void]$report.Add("")
[void]$report.Add("Decision propuesta: P6C-1 migra solamente codigo/header de raiz a src/, conserva examples/ y metadata fuera de src/, valida source-only con un unico cold y recien despues evalua P6C-2 precompiled.")
$report | Set-Content -LiteralPath $ReportPath -Encoding utf8NoBOM

Write-Host ""
Write-Host "=== P6C PREPARACION: VALIDADA ===" -ForegroundColor Green
Write-Host ("Reporte: {0}" -f $ReportPath) -ForegroundColor DarkGray
Write-Host "No se movio ningun archivo y no se ejecuto ninguna compilacion." -ForegroundColor DarkGray
Write-Host "Siguiente gate: P6C-1 layout src/ source-only para Adafruit BusIO." -ForegroundColor Yellow
