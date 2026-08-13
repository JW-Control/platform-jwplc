#requires -Version 7.4

[CmdletBinding()]
param()

Set-StrictMode -Version 2.0
$ErrorActionPreference = "Stop"

$ScriptRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$RepoRoot = [System.IO.Path]::GetFullPath((Join-Path $ScriptRoot "..\.."))
$PlatformRoot = Join-Path $RepoRoot "JWPLC\2.1.0"
$LibrariesRoot = Join-Path $PlatformRoot "libraries"

$RunId = "20260809_224628"
$RunRoot = Join-Path (Join-Path $ScriptRoot "p5a-ethernet-work") $RunId
$BuildPath = Join-Path $RunRoot "p5a-Basic"
$LogPath = Join-Path $RunRoot "p5a-Basic.log"
$CompileDbPath = Join-Path $BuildPath "compile_commands.json"
$AuditRoot = Join-Path $ScriptRoot "p6-adafruit-work"
$ReportPath = Join-Path $AuditRoot "P6_ADAFRUIT_BASELINE_20260809_224628.md"

$Definitions = @(
    [PSCustomObject]@{
        Key = "ST77xx"
        Folder = "Adafruit_ST7735_and_ST7789_Library"
        Name = "Adafruit ST7735 and ST7789 Library"
        Version = "1.11.0"
        Marker = "JWPLC_Bundled_Adafruit_ST77xx.h"
        ExpectedSources = @(
            "Adafruit_ST7735.cpp",
            "Adafruit_ST7789.cpp",
            "Adafruit_ST7796S.cpp",
            "Adafruit_ST77xx.cpp"
        )
    },
    [PSCustomObject]@{
        Key = "GFX"
        Folder = "Adafruit_GFX_Library"
        Name = "Adafruit GFX Library"
        Version = "1.12.4"
        Marker = "JWPLC_Bundled_Adafruit_GFX.h"
        ExpectedSources = @(
            "Adafruit_GFX.cpp",
            "Adafruit_GrayOLED.cpp",
            "Adafruit_SPITFT.cpp",
            "glcdfont.c"
        )
    },
    [PSCustomObject]@{
        Key = "BusIO"
        Folder = "Adafruit_BusIO"
        Name = "Adafruit BusIO"
        Version = "1.17.4"
        Marker = "JWPLC_Bundled_Adafruit_BusIO.h"
        ExpectedSources = @(
            "Adafruit_BusIO_Register.cpp",
            "Adafruit_GenericDevice.cpp",
            "Adafruit_I2CDevice.cpp",
            "Adafruit_SPIDevice.cpp"
        )
    }
)

function ConvertTo-EntryList
{
    param([Parameter(Mandatory = $true)]$Parsed)

    $entries = New-Object System.Collections.Generic.List[object]
    foreach ($entry in $Parsed)
    {
        [void]$entries.Add($entry)
    }
    return $entries
}

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

function Get-SourceNamesFromCompileDb
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

function Get-ObjectBytes
{
    param([Parameter(Mandatory = $true)][string]$LibraryFolder)

    $root = Join-Path (Join-Path $BuildPath "libraries") $LibraryFolder
    if (-not (Test-Path -LiteralPath $root)) { return [int64]0 }

    $sum = [int64]0
    foreach ($file in Get-ChildItem -LiteralPath $root -Recurse -File -Filter "*.o")
    {
        $sum += [int64]$file.Length
    }
    return $sum
}

function Get-LibraryAudit
{
    param(
        [Parameter(Mandatory = $true)]$Definition,
        [Parameter(Mandatory = $true)][object[]]$Entries,
        [Parameter(Mandatory = $true)][string[]]$LogLines
    )

    $libraryRoot = Join-Path $LibrariesRoot $Definition.Folder
    $propertiesPath = Join-Path $libraryRoot "library.properties"
    if (-not (Test-Path -LiteralPath $libraryRoot))
    {
        throw "No existe libreria bundled: $libraryRoot"
    }
    if (-not (Test-Path -LiteralPath $propertiesPath))
    {
        throw "No existe library.properties: $propertiesPath"
    }

    $actualName = Get-PropertyValue -Path $propertiesPath -Name "name"
    $actualVersion = Get-PropertyValue -Path $propertiesPath -Name "version"
    $precompiled = Get-PropertyValue -Path $propertiesPath -Name "precompiled"

    if ($actualName -ne $Definition.Name)
    {
        throw ("Nombre inesperado en {0}: {1}" -f $Definition.Folder, $actualName)
    }
    if ($actualVersion -ne $Definition.Version)
    {
        throw ("Version inesperada en {0}: {1}" -f $Definition.Folder, $actualVersion)
    }

    $compiledSources = @(Get-SourceNamesFromCompileDb -Entries $Entries -LibraryFolder $Definition.Folder)
    $expectedSources = @($Definition.ExpectedSources | Sort-Object)
    $compiledSorted = @($compiledSources | Sort-Object)

    if (($compiledSorted -join "|") -ne ($expectedSources -join "|"))
    {
        throw ("TUs inesperados para {0}. Actual={1} Esperado={2}" -f $Definition.Key, ($compiledSorted -join ','), ($expectedSources -join ','))
    }

    $rootSources = @(Get-ChildItem -LiteralPath $libraryRoot -File |
        Where-Object { $_.Extension -in @(".c", ".cc", ".cpp", ".S") } |
        Select-Object -ExpandProperty Name |
        Sort-Object)

    $srcPath = Join-Path $libraryRoot "src"
    $srcSources = @()
    if (Test-Path -LiteralPath $srcPath)
    {
        $srcSources = @(Get-ChildItem -LiteralPath $srcPath -Recurse -File |
            Where-Object { $_.Extension -in @(".c", ".cc", ".cpp", ".S") } |
            ForEach-Object { $_.FullName.Substring($srcPath.Length).TrimStart('\','/') } |
            Sort-Object)
    }

    $markerPath = Join-Path $libraryRoot $Definition.Marker
    $markerInRoot = Test-Path -LiteralPath $markerPath
    $selectionPattern = "Using library {0} at version {1} in folder:" -f $Definition.Name, $Definition.Version
    $selectionLines = @($LogLines | Where-Object { ([string]$_).Contains($selectionPattern) })
    $selectedBundled = @($selectionLines | Where-Object { ([string]$_).Contains($libraryRoot) }).Count -ge 1

    $sourceHashes = New-Object System.Collections.Generic.List[string]
    foreach ($sourceName in $Definition.ExpectedSources)
    {
        $sourcePath = Join-Path $libraryRoot $sourceName
        if (-not (Test-Path -LiteralPath $sourcePath))
        {
            throw "Falta fuente esperado: $sourcePath"
        }
        $hash = (Get-FileHash -LiteralPath $sourcePath -Algorithm SHA256).Hash.ToLowerInvariant()
        [void]$sourceHashes.Add(("{0}: {1}" -f $sourceName, $hash))
    }

    return [PSCustomObject]@{
        Key = $Definition.Key
        Folder = $Definition.Folder
        Name = $actualName
        Version = $actualVersion
        CompiledCount = $compiledSources.Count
        CompiledSources = $compiledSources
        ObjectBytes = Get-ObjectBytes -LibraryFolder $Definition.Folder
        RootSources = $rootSources
        SrcSources = $srcSources
        MarkerInRoot = $markerInRoot
        SelectedBundled = $selectedBundled
        Precompiled = $precompiled
        PropertiesHash = (Get-FileHash -LiteralPath $propertiesPath -Algorithm SHA256).Hash.ToLowerInvariant()
        SourceHashes = @($sourceHashes)
    }
}

Write-Host "JWPLC - P6 Adafruit baseline audit / sin compilar" -ForegroundColor Cyan
Write-Host ("PowerShell: {0} / {1}" -f $PSVersionTable.PSVersion, $PSVersionTable.PSEdition)
Write-Host ("P5A referencia: {0}" -f $RunRoot)
Write-Host ""

if (-not (Test-Path -LiteralPath $CompileDbPath))
{
    throw "Falta compile_commands.json P5A: $CompileDbPath"
}
if (-not (Test-Path -LiteralPath $LogPath))
{
    throw "Falta log P5A: $LogPath"
}

$parsed = Get-Content -LiteralPath $CompileDbPath -Raw | ConvertFrom-Json
$entryList = ConvertTo-EntryList -Parsed $parsed
$entries = @($entryList)
$logLines = @(Get-Content -LiteralPath $LogPath | Where-Object { -not [string]::IsNullOrWhiteSpace([string]$_) })

if ($entries.Count -ne 24)
{
    throw ("P5A baseline debe tener 24 compiles; actual={0}" -f $entries.Count)
}

$preprocessTotal = @($logLines | Where-Object { ([string]$_) -match 'xtensa-esp32-elf-g\+\+.*\s-E\s' }).Count
if ($preprocessTotal -ne 41)
{
    throw ("P5A baseline debe tener -E=41; actual={0}" -f $preprocessTotal)
}

$audits = New-Object System.Collections.Generic.List[object]
foreach ($definition in $Definitions)
{
    [void]$audits.Add((Get-LibraryAudit -Definition $definition -Entries $entries -LogLines $logLines))
}

$adafruitCompiles = @($audits | Measure-Object -Property CompiledCount -Sum).Sum
$otherCompiles = $entries.Count - $adafruitCompiles

if ($adafruitCompiles -ne 12 -or $otherCompiles -ne 12)
{
    throw ("Distribucion inesperada P5A. Adafruit={0}, otros={1}" -f $adafruitCompiles, $otherCompiles)
}

foreach ($audit in $audits)
{
    if (-not $audit.SelectedBundled)
    {
        throw ("El log P5A no confirma seleccion bundled de {0}." -f $audit.Name)
    }
    if (-not $audit.MarkerInRoot)
    {
        throw ("Falta marker bundled en raiz para {0}." -f $audit.Name)
    }
}

Write-Host ("P5A total: {0} compiles | -E={1}" -f $entries.Count, $preprocessTotal) -ForegroundColor Green
Write-Host ("Adafruit: {0} compiles | Otros: {1}" -f $adafruitCompiles, $otherCompiles) -ForegroundColor Green
Write-Host ""

foreach ($audit in $audits)
{
    $layout = if ($audit.SrcSources.Count -gt 0) { "src/recursivo o mixto" } else { "flat/root" }
    $precompiledText = if ([string]::IsNullOrWhiteSpace([string]$audit.Precompiled)) { "no declarado" } else { [string]$audit.Precompiled }

    Write-Host ("[{0}] {1} {2}" -f $audit.Key, $audit.Name, $audit.Version) -ForegroundColor Cyan
    Write-Host ("  TUs: {0} | objetos={1} bytes | layout={2} | precompiled={3}" -f $audit.CompiledCount, $audit.ObjectBytes, $layout, $precompiledText)
    Write-Host ("  Bundled seleccionado: {0} | marker raiz: {1}" -f $audit.SelectedBundled, $audit.MarkerInRoot)
    Write-Host ("  Sources: {0}" -f ($audit.CompiledSources -join ", "))
}

New-Item -ItemType Directory -Path $AuditRoot -Force | Out-Null
$report = New-Object System.Collections.Generic.List[string]
[void]$report.Add("# P6 - Auditoria baseline Adafruit")
[void]$report.Add("")
[void]$report.Add("Referencia P5A: $RunId")
[void]$report.Add("")
[void]$report.Add("- Compiles P5A: $($entries.Count)")
[void]$report.Add("- Preprocesados -E: $preprocessTotal")
[void]$report.Add("- Compiles Adafruit: $adafruitCompiles")
[void]$report.Add("- Compiles no Adafruit: $otherCompiles")
[void]$report.Add("")
[void]$report.Add("Nota: no se atribuyen pasadas -E por libreria usando coincidencias de ruta, porque los -I del comando verbose contaminan ese conteo. compile_commands.json es la fuente para TUs.")
[void]$report.Add("")
[void]$report.Add("| Bloque | Version | TUs | Bytes .o | Layout actual | precompiled | Bundled |")
[void]$report.Add("|---|---:|---:|---:|---|---|---|")
foreach ($audit in $audits)
{
    $layout = if ($audit.SrcSources.Count -gt 0) { "src/recursivo o mixto" } else { "flat/root" }
    $precompiledText = if ([string]::IsNullOrWhiteSpace([string]$audit.Precompiled)) { "no" } else { [string]$audit.Precompiled }
    [void]$report.Add(("| {0} | {1} | {2} | {3} | {4} | {5} | {6} |" -f $audit.Key, $audit.Version, $audit.CompiledCount, $audit.ObjectBytes, $layout, $precompiledText, $audit.SelectedBundled))
}

foreach ($audit in $audits)
{
    [void]$report.Add("")
    [void]$report.Add(("## {0}" -f $audit.Key))
    [void]$report.Add("")
    [void]$report.Add(("Fuentes compilados: {0}" -f ($audit.CompiledSources -join ", ")))
    [void]$report.Add("")
    [void]$report.Add(("SHA-256 library.properties: {0}" -f $audit.PropertiesHash))
    [void]$report.Add("")
    [void]$report.Add("SHA-256 fuentes:")
    foreach ($hashLine in $audit.SourceHashes)
    {
        [void]$report.Add(("- {0}" -f $hashLine))
    }
}

[void]$report.Add("")
[void]$report.Add("## Decision de siguiente experimento")
[void]$report.Add("")
[void]$report.Add("P6A debe atacar primero Adafruit ST7735/ST7789 (ST77xx): 4 TUs. Antes del archive se debe validar una migracion controlada de layout flat a src/, porque precompiled=full requiere el formato moderno con el binario bajo src/{build.mcu}.")

$report | Set-Content -LiteralPath $ReportPath -Encoding utf8NoBOM

Write-Host ""
Write-Host "=== P6 BASELINE: VALIDADO ===" -ForegroundColor Green
Write-Host "Orden recomendado: P6A ST77xx -> P6B GFX -> P6C BusIO"
Write-Host ("Reporte: {0}" -f $ReportPath) -ForegroundColor DarkGray
Write-Host "No se ejecuto ninguna compilacion." -ForegroundColor DarkGray
