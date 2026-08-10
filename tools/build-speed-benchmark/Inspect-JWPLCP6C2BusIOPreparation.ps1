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
$BusSrc = Join-Path $BusRoot "src"
$BusProperties = Join-Path $BusRoot "library.properties"
$BusArchive = Join-Path $BusSrc "esp32\libAdafruit_BusIO.a"
$BusMarker = Join-Path $BusSrc "JWPLC_Bundled_Adafruit_BusIO.h"

$StRoot = Join-Path $LibrariesRoot "Adafruit_ST7735_and_ST7789_Library"
$StProperties = Join-Path $StRoot "library.properties"
$StArchive = Join-Path $StRoot "src\esp32\libAdafruit_ST7735_and_ST7789_Library.a"

$GfxRoot = Join-Path $LibrariesRoot "Adafruit_GFX_Library"
$GfxProperties = Join-Path $GfxRoot "library.properties"
$GfxArchive = Join-Path $GfxRoot "src\esp32\libAdafruit_GFX_Library.a"

$BaselineRunId = "20260810_000915"
$BaselineRoot = Join-Path (Join-Path $ScriptRoot "p6c-busio-layout-work") $BaselineRunId
$BaselineBuild = Join-Path $BaselineRoot "p6c-layout-Basic"
$BaselineLog = Join-Path $BaselineRoot "p6c-layout-Basic.log"
$ReportRoot = Join-Path $ScriptRoot "p6c2-busio-precompiled-work"
$ReportPath = Join-Path $ReportRoot "P6C2_BUSIO_PREPARATION_AUDIT.md"

$ExpectedObjects = @(
    "Adafruit_BusIO_Register.cpp.o",
    "Adafruit_GenericDevice.cpp.o",
    "Adafruit_I2CDevice.cpp.o",
    "Adafruit_SPIDevice.cpp.o"
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

    $output = @()
    $exitCode = -1
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

    foreach ($line in Get-Content -LiteralPath $LogPath)
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

    foreach ($line in Get-Content -LiteralPath $LogPath)
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
            $candidate = Join-Path $toolDir $leaf
            if (Test-Path -LiteralPath $candidate) { return (Resolve-Path -LiteralPath $candidate).Path }
        }
    }

    throw "No se pudo localizar xtensa-esp32-elf-gcc-ar desde el log P6C-1."
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
    if (-not (Test-Path -LiteralPath $LogPath)) { throw "Falta log: $LogPath" }
    return @(Get-Content -LiteralPath $LogPath | Where-Object { ([string]$_) -match 'xtensa-esp32-elf-g\+\+.*\s-E\s' }).Count
}

function Get-BusObjects
{
    param([Parameter(Mandatory = $true)][string]$BuildPath)

    $root = Join-Path $BuildPath "libraries\Adafruit_BusIO"
    if (-not (Test-Path -LiteralPath $root)) { throw "Falta arbol BusIO baseline: $root" }

    $objects = @(Get-ChildItem -LiteralPath $root -Recurse -File -Filter "*.o" |
        Where-Object { $ExpectedObjects -contains $_.Name } |
        Sort-Object Name)
    $actual = @($objects | Select-Object -ExpandProperty Name | Sort-Object)
    $expected = @($ExpectedObjects | Sort-Object)
    if (($actual -join "|") -ne ($expected -join "|"))
    {
        throw ("Objetos BusIO inesperados. Actual={0}" -f ($actual -join ","))
    }
    return $objects
}

Write-Host "JWPLC - P6C-2 Adafruit BusIO precompiled preparation / sin compilar" -ForegroundColor Cyan
Write-Host ("PowerShell: {0} / {1}" -f $PSVersionTable.PSVersion, $PSVersionTable.PSEdition)
Write-Host ("Baseline P6C-1: {0}" -f $BaselineRoot)
Write-Host ""

foreach ($required in @($BusRoot, $BusSrc, $BusProperties, $BusMarker, $StProperties, $StArchive, $GfxProperties, $GfxArchive, $BaselineBuild, $BaselineLog))
{
    if (-not (Test-Path -LiteralPath $required)) { throw "Falta requisito P6C-2: $required" }
}

if ((Get-PropertyValue -Path $StProperties -Name "precompiled") -ne "full") { throw "P6C-2 requiere ST77xx precompiled activo." }
if ((Get-PropertyValue -Path $GfxProperties -Name "precompiled") -ne "full") { throw "P6C-2 requiere GFX precompiled activo." }
if ((Get-PropertyValue -Path $BusProperties -Name "name") -ne "Adafruit BusIO") { throw "Nombre BusIO inesperado." }
if ((Get-PropertyValue -Path $BusProperties -Name "version") -ne "1.17.4") { throw "Version BusIO distinta de 1.17.4." }
if (-not [string]::IsNullOrWhiteSpace([string](Get-PropertyValue -Path $BusProperties -Name "precompiled"))) { throw "P6C-2 requiere BusIO aun sin precompiled=." }
if (Test-Path -LiteralPath $BusArchive) { throw "Ya existe archive BusIO; no se sobreescribira en preparacion." }

$rootCode = @(Get-ChildItem -LiteralPath $BusRoot -File | Where-Object { $_.Extension -in @(".c", ".cc", ".cpp", ".h", ".hpp", ".S") })
$srcCode = @(Get-ChildItem -LiteralPath $BusSrc -File | Where-Object { $_.Extension -in @(".c", ".cc", ".cpp", ".h", ".hpp", ".S") } | Sort-Object Name)
if ($rootCode.Count -ne 0 -or $srcCode.Count -ne 10)
{
    throw ("P6C-2 requiere layout BusIO src/ activo. root={0}, src={1}/10." -f $rootCode.Count, $srcCode.Count)
}

$metrics = Get-CompileMetrics -BuildPath $BaselineBuild
$preprocess = Get-PreprocessCount -LogPath $BaselineLog
if ($metrics.Total -ne 16 -or $metrics.ST77xx -ne 0 -or $metrics.GFX -ne 0 -or
    $metrics.BusIO -ne 4 -or $metrics.Ethernet -ne 0 -or $metrics.Display -ne 0 -or
    $metrics.Stub -ne 1 -or $preprocess -ne 33)
{
    throw ("Baseline P6C-1 inesperado: total/ST/GFX/BusIO/Eth/Display/stub/-E={0}/{1}/{2}/{3}/{4}/{5}/{6}/{7}" -f $metrics.Total, $metrics.ST77xx, $metrics.GFX, $metrics.BusIO, $metrics.Ethernet, $metrics.Display, $metrics.Stub, $preprocess)
}

$objects = @(Get-BusObjects -BuildPath $BaselineBuild)
$objectBytes = [int64]0
foreach ($obj in $objects) { $objectBytes += [int64]$obj.Length }
$archiver = Find-Archiver -LogPath $BaselineLog

Write-Host "Quality gate P6C-1: OK | 16 compiles | ST77xx=0 | GFX=0 | BusIO=4 | -E=33" -ForegroundColor Green
Write-Host "Layout BusIO src/: OK | marker bundled bajo src/" -ForegroundColor Green
Write-Host ("Objetos reutilizables P6C-1: {0} | {1} bytes" -f $objects.Count, $objectBytes) -ForegroundColor Green
foreach ($obj in $objects)
{
    Write-Host ("  {0}: {1} bytes" -f $obj.Name, $obj.Length) -ForegroundColor DarkGray
}
Write-Host ("Archiver: {0}" -f $archiver) -ForegroundColor DarkGray
Write-Host ("Archive objetivo: {0}" -f $BusArchive) -ForegroundColor DarkGray

New-Item -ItemType Directory -Path $ReportRoot -Force | Out-Null
$report = @(
    "# P6C-2 - Preparacion Adafruit BusIO precompilado",
    "",
    ("Baseline P6C-1: {0}" -f $BaselineRunId),
    ("Compiles / -E: {0} / {1}" -f $metrics.Total, $preprocess),
    ("Objetos BusIO: {0}" -f $objects.Count),
    ("Bytes objetos BusIO: {0}" -f $objectBytes),
    ("Archive objetivo: {0}" -f $BusArchive),
    "",
    "Objetivo del siguiente piloto: generar libAdafruit_BusIO.a con exactamente los 4 objetos validados, activar precompiled=full y ejecutar un unico cold. Gate estructural esperado: 16 -> 12 compiles y BusIO fuente 4 -> 0."
)
$report | Set-Content -LiteralPath $ReportPath -Encoding utf8NoBOM

Write-Host ""
Write-Host "=== P6C-2 PREPARACION: OK ===" -ForegroundColor Green
Write-Host ("Reporte: {0}" -f $ReportPath) -ForegroundColor DarkGray
Write-Host "No se genero archive, no se modifico library.properties y no se ejecuto ninguna compilacion." -ForegroundColor DarkGray
Write-Host "Siguiente paso: piloto P6C-2 precompiled con un unico cold." -ForegroundColor Yellow
