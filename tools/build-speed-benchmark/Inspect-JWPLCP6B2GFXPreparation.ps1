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
$GfxSrc = Join-Path $GfxRoot "src"
$GfxProperties = Join-Path $GfxRoot "library.properties"
$GfxArchive = Join-Path $GfxSrc "esp32\libAdafruit_GFX_Library.a"
$GfxFonts = Join-Path $GfxSrc "Fonts"

$StRoot = Join-Path $LibrariesRoot "Adafruit_ST7735_and_ST7789_Library"
$StProperties = Join-Path $StRoot "library.properties"
$StArchive = Join-Path $StRoot "src\esp32\libAdafruit_ST7735_and_ST7789_Library.a"

$BaselineRunId = "20260809_234653"
$BaselineRoot = Join-Path (Join-Path $ScriptRoot "p6b-gfx-layout-work") $BaselineRunId
$BaselineBuild = Join-Path $BaselineRoot "p6b-layout-Basic"
$BaselineLog = Join-Path $BaselineRoot "p6b-layout-Basic.log"
$ReportRoot = Join-Path $ScriptRoot "p6b2-gfx-precompiled-work"
$ReportPath = Join-Path $ReportRoot "P6B2_GFX_PREPARATION_AUDIT.md"

$ExpectedObjects = @(
    "Adafruit_GFX.cpp.o",
    "Adafruit_GrayOLED.cpp.o",
    "Adafruit_SPITFT.cpp.o",
    "glcdfont.c.o"
)

function Invoke-NativeCaptured
{
    param(
        [Parameter(Mandatory = $true)][string]$FilePath,
        [Parameter(Mandatory = $true)][string[]]$Arguments
    )

    $oldErrorAction = $ErrorActionPreference
    $hadNativePreference = Test-Path variable:global:PSNativeCommandUseErrorActionPreference
    if ($hadNativePreference) { $oldNativePreference = $global:PSNativeCommandUseErrorActionPreference }

    try
    {
        $ErrorActionPreference = "Continue"
        if ($hadNativePreference) { $global:PSNativeCommandUseErrorActionPreference = $false }
        $output = @(& $FilePath @Arguments 2>&1 | ForEach-Object { $_.ToString() })
        $exitCode = $LASTEXITCODE
    }
    finally
    {
        $ErrorActionPreference = $oldErrorAction
        if ($hadNativePreference) { $global:PSNativeCommandUseErrorActionPreference = $oldNativePreference }
    }

    return [PSCustomObject]@{ ExitCode = [int]$exitCode; Output = $output }
}

function Resolve-NativeToolPath
{
    param([Parameter(Mandatory = $true)][string]$Candidate)
    $normalized = $Candidate.Trim().Trim('"')
    while ($normalized.Contains("\\")) { $normalized = $normalized.Replace("\\", "\") }
    foreach ($path in @($normalized, ($normalized + ".exe")))
    {
        if (Test-Path -LiteralPath $path) { return (Resolve-Path -LiteralPath $path).Path }
    }
    return $null
}

function Find-Archiver
{
    param([Parameter(Mandatory = $true)][string]$LogPath)

    if (-not (Test-Path -LiteralPath $LogPath)) { throw "Falta log baseline: $LogPath" }
    $lines = @(Get-Content -LiteralPath $LogPath)

    foreach ($line in $lines)
    {
        $candidate = $null
        if ($line -match '"(?<exe>[^"]*xtensa-esp32-elf-gcc-ar(?:\.exe)?)"') { $candidate = $Matches["exe"] }
        elseif ($line -match '(?<exe>\S*xtensa-esp32-elf-gcc-ar(?:\.exe)?)\s+(?:cr|crs)\b') { $candidate = $Matches["exe"] }

        if (-not [string]::IsNullOrWhiteSpace($candidate))
        {
            $resolved = Resolve-NativeToolPath -Candidate $candidate
            if (-not [string]::IsNullOrWhiteSpace($resolved)) { return $resolved }
        }
    }

    foreach ($line in $lines)
    {
        $compilerCandidate = $null
        if ($line -match '"(?<exe>[^"]*xtensa-esp32-elf-g\+\+(?:\.exe)?)"') { $compilerCandidate = $Matches["exe"] }
        elseif ($line -match '(?<exe>\S*xtensa-esp32-elf-g\+\+(?:\.exe)?)\s') { $compilerCandidate = $Matches["exe"] }
        if ([string]::IsNullOrWhiteSpace($compilerCandidate)) { continue }

        $compiler = Resolve-NativeToolPath -Candidate $compilerCandidate
        if ([string]::IsNullOrWhiteSpace($compiler)) { continue }
        $toolDir = Split-Path -Parent $compiler
        foreach ($leaf in @("xtensa-esp32-elf-gcc-ar.exe", "xtensa-esp32-elf-gcc-ar"))
        {
            $sibling = Join-Path $toolDir $leaf
            if (Test-Path -LiteralPath $sibling) { return (Resolve-Path -LiteralPath $sibling).Path }
        }
    }

    throw "No se pudo localizar xtensa-esp32-elf-gcc-ar desde el log P6B-1."
}

function Get-PropertyValue
{
    param(
        [Parameter(Mandatory = $true)][string]$Path,
        [Parameter(Mandatory = $true)][string]$Name
    )
    foreach ($line in Get-Content -LiteralPath $Path)
    {
        if ($line -match ('^' + [regex]::Escape($Name) + '=(?<value>.*)$')) { return $Matches["value"].Trim() }
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
    if ($entries.Count -eq 0) { throw "compile_commands.json vacio." }
    return $entries
}

function Get-CompileMetrics
{
    param([Parameter(Mandatory = $true)][string]$BuildPath)
    $db = Join-Path $BuildPath "compile_commands.json"
    if (-not (Test-Path -LiteralPath $db)) { throw "Falta compile_commands.json: $db" }
    $entries = ConvertTo-EntryList -Parsed (Get-Content -LiteralPath $db -Raw | ConvertFrom-Json)
    $files = @($entries | ForEach-Object { [string]$_.file })
    return [PSCustomObject]@{
        Entries = @($entries)
        Total = $entries.Count
        ST77xx = @($files | Where-Object { $_ -match '[\\/]libraries[\\/]Adafruit_ST7735_and_ST7789_Library[\\/]' }).Count
        GFX = @($files | Where-Object { $_ -match '[\\/]libraries[\\/]Adafruit_GFX_Library[\\/]' }).Count
        BusIO = @($files | Where-Object { $_ -match '[\\/]libraries[\\/]Adafruit_BusIO[\\/]' }).Count
        Ethernet = @($files | Where-Object { $_ -match '[\\/]libraries[\\/]JWPLC_Ethernet_W5x00_Backend[\\/]' }).Count
        Display = @($files | Where-Object { $_ -match '[\\/]libraries[\\/]JWPLC_Display[\\/]' }).Count
        Stub = @($files | Where-Object { $_ -match '[\\/]cores[\\/]jwcontrol_p2[\\/]p2_core_stub\.c$' }).Count
    }
}

function Get-PreprocessCount
{
    param([Parameter(Mandatory = $true)][string]$LogPath)
    return @(Get-Content -LiteralPath $LogPath | Where-Object { ([string]$_) -match 'xtensa-esp32-elf-g\+\+.*\s-E\s' }).Count
}

function Get-GfxObjects
{
    param([Parameter(Mandatory = $true)][string]$BuildPath)
    $root = Join-Path $BuildPath "libraries\Adafruit_GFX_Library"
    if (-not (Test-Path -LiteralPath $root)) { throw "Falta arbol GFX baseline: $root" }
    $objects = @(Get-ChildItem -LiteralPath $root -Recurse -File -Filter "*.o" |
        Where-Object { $ExpectedObjects -contains $_.Name } |
        Sort-Object Name)
    $names = @($objects | Select-Object -ExpandProperty Name | Sort-Object)
    $expected = @($ExpectedObjects | Sort-Object)
    if (($names -join "|") -ne ($expected -join "|"))
    {
        throw ("Objetos GFX inesperados. Actual={0}" -f ($names -join ","))
    }
    return $objects
}

Write-Host "JWPLC - P6B-2 Adafruit GFX precompiled preparation / sin compilar" -ForegroundColor Cyan
Write-Host ("PowerShell: {0} / {1}" -f $PSVersionTable.PSVersion, $PSVersionTable.PSEdition)
Write-Host ("Baseline P6B-1: {0}" -f $BaselineRoot)
Write-Host ""

foreach ($required in @($GfxRoot, $GfxSrc, $GfxProperties, $GfxFonts, $StProperties, $StArchive, $BaselineBuild, $BaselineLog))
{
    if (-not (Test-Path -LiteralPath $required)) { throw "Falta requisito P6B-2: $required" }
}

if ((Get-PropertyValue -Path $StProperties -Name "precompiled") -ne "full") { throw "P6B-2 requiere P6A-2 activo." }
if ((Get-PropertyValue -Path $GfxProperties -Name "name") -ne "Adafruit GFX Library") { throw "Nombre GFX inesperado." }
if ((Get-PropertyValue -Path $GfxProperties -Name "version") -ne "1.12.4") { throw "Version GFX distinta de 1.12.4." }
if (-not [string]::IsNullOrWhiteSpace([string](Get-PropertyValue -Path $GfxProperties -Name "precompiled"))) { throw "P6B-2 requiere GFX aun sin precompiled=." }
if (Test-Path -LiteralPath $GfxArchive) { throw "Ya existe archive GFX; no se sobreescribira en preparacion." }

$metrics = Get-CompileMetrics -BuildPath $BaselineBuild
$preprocess = Get-PreprocessCount -LogPath $BaselineLog
if ($metrics.Total -ne 20 -or $metrics.ST77xx -ne 0 -or $metrics.GFX -ne 4 -or
    $metrics.BusIO -ne 4 -or $metrics.Ethernet -ne 0 -or $metrics.Display -ne 0 -or
    $metrics.Stub -ne 1 -or $preprocess -ne 37)
{
    throw ("Baseline P6B-1 inesperado: total/ST/GFX/BusIO/Eth/Display/stub/-E={0}/{1}/{2}/{3}/{4}/{5}/{6}/{7}" -f $metrics.Total, $metrics.ST77xx, $metrics.GFX, $metrics.BusIO, $metrics.Ethernet, $metrics.Display, $metrics.Stub, $preprocess)
}

$objects = @(Get-GfxObjects -BuildPath $BaselineBuild)
$objectBytes = [int64]0
foreach ($obj in $objects) { $objectBytes += [int64]$obj.Length }
$archiver = Find-Archiver -LogPath $BaselineLog

$marker = Join-Path $GfxSrc "JWPLC_Bundled_Adafruit_GFX.h"
if (-not (Test-Path -LiteralPath $marker)) { throw "Marker bundled GFX no esta bajo src/." }
$fontFiles = @(Get-ChildItem -LiteralPath $GfxFonts -Recurse -File)
if ($fontFiles.Count -ne 52) { throw ("Inventario Fonts/ inesperado: {0}, esperado 52." -f $fontFiles.Count) }

Write-Host "Quality gate P6B-1: OK | 20 compiles | ST77xx=0 | GFX=4 | BusIO=4 | -E=37" -ForegroundColor Green
Write-Host "Layout GFX src/: OK | marker bundled bajo src/ | Fonts/=52" -ForegroundColor Green
Write-Host ("Objetos reutilizables P6B-1: {0} | {1} bytes" -f $objects.Count, $objectBytes) -ForegroundColor Green
foreach ($obj in $objects)
{
    Write-Host ("  {0}: {1} bytes" -f $obj.Name, $obj.Length) -ForegroundColor DarkGray
}
Write-Host ("Archiver: {0}" -f $archiver) -ForegroundColor DarkGray
Write-Host ("Archive objetivo: {0}" -f $GfxArchive) -ForegroundColor DarkGray

New-Item -ItemType Directory -Path $ReportRoot -Force | Out-Null
$report = @(
    "# P6B-2 - Preparacion Adafruit GFX precompilado",
    "",
    ("Baseline P6B-1: {0}" -f $BaselineRunId),
    ("Compiles / -E: {0} / {1}" -f $metrics.Total, $preprocess),
    ("Objetos GFX: {0}" -f $objects.Count),
    ("Bytes objetos GFX: {0}" -f $objectBytes),
    ("Archive objetivo: {0}" -f $GfxArchive),
    "",
    "Objetivo del siguiente piloto: generar libAdafruit_GFX_Library.a con exactamente los 4 objetos validados, activar precompiled=full y ejecutar un unico cold. Gate esperado: 20 -> 16 compiles y GFX fuente 4 -> 0."
)
$report | Set-Content -LiteralPath $ReportPath -Encoding utf8NoBOM

Write-Host ""
Write-Host "=== P6B-2 PREPARACION: OK ===" -ForegroundColor Green
Write-Host ("Reporte: {0}" -f $ReportPath) -ForegroundColor DarkGray
Write-Host "No se genero archive, no se modifico library.properties y no se ejecuto ninguna compilacion." -ForegroundColor DarkGray
Write-Host "Siguiente paso: piloto P6B-2 precompiled con un unico cold." -ForegroundColor Yellow
