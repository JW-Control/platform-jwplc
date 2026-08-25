#requires -Version 7.4

[CmdletBinding()]
param(
    [string]$ArduinoCli = "arduino-cli",
    [int]$Jobs = 0,
    [switch]$RunPilot
)

Set-StrictMode -Version 2.0
$ErrorActionPreference = "Stop"

$ScriptRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$RepoRoot = [System.IO.Path]::GetFullPath((Join-Path $ScriptRoot "..\.."))
$PlatformRoot = Join-Path $RepoRoot "JWPLC\2.1.0"
$LibraryRelative = "JWPLC/2.1.0/libraries/Adafruit_ST7735_and_ST7789_Library"
$LibraryRoot = Join-Path $PlatformRoot "libraries\Adafruit_ST7735_and_ST7789_Library"
$PropertiesPath = Join-Path $LibraryRoot "library.properties"
$SrcRoot = Join-Path $LibraryRoot "src"
$SketchPath = Join-Path $ScriptRoot "sketches\01_empty"
$BaselineRunId = "20260809_224628"
$BaselineRunRoot = Join-Path (Join-Path $ScriptRoot "p5a-ethernet-work") $BaselineRunId
$BaselineBuildPath = Join-Path $BaselineRunRoot "p5a-Basic"
$BaselineLogPath = Join-Path $BaselineRunRoot "p5a-Basic.log"
$OutputRoot = Join-Path $ScriptRoot "p6a-st77xx-layout-work"

$MoveFiles = @(
    "Adafruit_ST7735.cpp",
    "Adafruit_ST7735.h",
    "Adafruit_ST7789.cpp",
    "Adafruit_ST7789.h",
    "Adafruit_ST7796S.cpp",
    "Adafruit_ST7796S.h",
    "Adafruit_ST77xx.cpp",
    "Adafruit_ST77xx.h",
    "JWPLC_Bundled_Adafruit_ST77xx.h"
)

function Invoke-NativeCaptured
{
    param(
        [Parameter(Mandatory = $true)][string]$FilePath,
        [Parameter(Mandatory = $true)][string[]]$Arguments
    )

    $oldErrorAction = $ErrorActionPreference
    $hadNativePreference = Test-Path variable:global:PSNativeCommandUseErrorActionPreference
    if ($hadNativePreference)
    {
        $oldNativePreference = $global:PSNativeCommandUseErrorActionPreference
    }

    $output = @()
    $exitCode = -1
    try
    {
        $ErrorActionPreference = "Continue"
        if ($hadNativePreference)
        {
            $global:PSNativeCommandUseErrorActionPreference = $false
        }
        $output = @(& $FilePath @Arguments 2>&1 | ForEach-Object { $_.ToString() })
        $exitCode = $LASTEXITCODE
    }
    finally
    {
        $ErrorActionPreference = $oldErrorAction
        if ($hadNativePreference)
        {
            $global:PSNativeCommandUseErrorActionPreference = $oldNativePreference
        }
    }

    return [PSCustomObject]@{
        ExitCode = [int]$exitCode
        Output = $output
    }
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
    if ($entries.Count -eq 0) { throw "compile_commands.json no contiene entradas." }
    return $entries
}

function Get-CompileMetrics
{
    param([Parameter(Mandatory = $true)][string]$BuildPath)

    $dbPath = Join-Path $BuildPath "compile_commands.json"
    if (-not (Test-Path -LiteralPath $dbPath))
    {
        throw "No existe compile_commands.json: $dbPath"
    }

    $parsed = Get-Content -LiteralPath $dbPath -Raw | ConvertFrom-Json
    $entries = ConvertTo-EntryList -Parsed $parsed
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
    if (-not (Test-Path -LiteralPath $LogPath)) { throw "No existe log: $LogPath" }
    return @(Get-Content -LiteralPath $LogPath |
        Where-Object { ([string]$_) -match 'xtensa-esp32-elf-g\+\+.*\s-E\s' }).Count
}

function Get-LibrarySelections
{
    param([Parameter(Mandatory = $true)][string]$LogPath)
    return @(Get-Content -LiteralPath $LogPath |
        Where-Object { ([string]$_) -like "Using library *" } |
        ForEach-Object { ([string]$_).Trim() } |
        Sort-Object -Unique)
}

function Get-AppBin
{
    param([Parameter(Mandatory = $true)][string]$BuildPath)
    $bin = Get-ChildItem -LiteralPath $BuildPath -Filter "*.ino.bin" -File | Select-Object -First 1
    if ($null -eq $bin) { throw "No se encontro app .bin en $BuildPath" }
    return $bin
}

function Get-ExternalObjectTable
{
    param([Parameter(Mandatory = $true)][string]$BuildPath)

    $root = Join-Path $BuildPath "libraries"
    $table = @{}
    if (-not (Test-Path -LiteralPath $root)) { return $table }

    foreach ($file in Get-ChildItem -LiteralPath $root -Recurse -File -Filter "*.o")
    {
        $relative = $file.FullName.Substring($root.Length).TrimStart('\','/')
        if ($relative -match '^Adafruit_ST7735_and_ST7789_Library[\\/]') { continue }
        $table[$relative] = (Get-FileHash -LiteralPath $file.FullName -Algorithm SHA256).Hash.ToLowerInvariant()
    }
    return $table
}

function Get-FileHashTable
{
    param(
        [Parameter(Mandatory = $true)][string]$Root,
        [Parameter(Mandatory = $true)][string[]]$Names
    )

    $table = @{}
    foreach ($name in $Names)
    {
        $path = Join-Path $Root $name
        if (-not (Test-Path -LiteralPath $path)) { throw "Falta archivo esperado: $path" }
        $table[$name] = (Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash.ToLowerInvariant()
    }
    return $table
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

if ($Jobs -lt 0) { throw "Jobs debe ser 0 o mayor." }

Write-Host "JWPLC - P6A ST77xx source-layout pilot" -ForegroundColor Cyan
Write-Host ("PowerShell: {0} / {1}" -f $PSVersionTable.PSVersion, $PSVersionTable.PSEdition)
Write-Host ("Baseline P5A: {0}" -f $BaselineRunRoot)
Write-Host ""

if (-not (Test-Path -LiteralPath $LibraryRoot)) { throw "No existe libreria ST77xx bundled: $LibraryRoot" }
if (-not (Test-Path -LiteralPath $PropertiesPath)) { throw "Falta library.properties ST77xx." }
if ((Get-PropertyValue -Path $PropertiesPath -Name "name") -ne "Adafruit ST7735 and ST7789 Library")
{
    throw "Nombre inesperado en library.properties ST77xx."
}
if ((Get-PropertyValue -Path $PropertiesPath -Name "version") -ne "1.11.0")
{
    throw "Version ST77xx distinta de 1.11.0."
}
if (-not [string]::IsNullOrWhiteSpace([string](Get-PropertyValue -Path $PropertiesPath -Name "precompiled")))
{
    throw "P6A-1 requiere ST77xx aun sin precompiled=."
}

$baselineMetrics = Get-CompileMetrics -BuildPath $BaselineBuildPath
$baselineE = Get-PreprocessCount -LogPath $BaselineLogPath
if ($baselineMetrics.Total -ne 24 -or
    $baselineMetrics.ST77xx -ne 4 -or
    $baselineMetrics.GFX -ne 4 -or
    $baselineMetrics.BusIO -ne 4 -or
    $baselineMetrics.Ethernet -ne 0 -or
    $baselineMetrics.Display -ne 0 -or
    $baselineMetrics.Stub -ne 1 -or
    $baselineE -ne 41)
{
    throw ("Baseline P5A inesperado. Actual total/ST/GFX/BusIO/Eth/Display/stub/-E={0}/{1}/{2}/{3}/{4}/{5}/{6}/{7}" -f $baselineMetrics.Total, $baselineMetrics.ST77xx, $baselineMetrics.GFX, $baselineMetrics.BusIO, $baselineMetrics.Ethernet, $baselineMetrics.Display, $baselineMetrics.Stub, $baselineE)
}

$rootPresent = @($MoveFiles | Where-Object { Test-Path -LiteralPath (Join-Path $LibraryRoot $_) })
$srcPresent = @($MoveFiles | Where-Object { Test-Path -LiteralPath (Join-Path $SrcRoot $_) })

$layoutState = "unknown"
if ($rootPresent.Count -eq $MoveFiles.Count -and $srcPresent.Count -eq 0)
{
    $layoutState = "flat"
}
elseif ($rootPresent.Count -eq 0 -and $srcPresent.Count -eq $MoveFiles.Count)
{
    $layoutState = "src"
}
else
{
    throw ("Layout ST77xx mixto/incompleto. root={0}/{1}, src={2}/{1}. No se tocara nada." -f $rootPresent.Count, $MoveFiles.Count, $srcPresent.Count)
}

Write-Host ("Quality gate baseline: OK | 24 compiles | ST77xx=4 | GFX=4 | BusIO=4 | -E=41") -ForegroundColor Green
Write-Host ("Layout ST77xx actual: {0}" -f $layoutState) -ForegroundColor Green
Write-Host ("Archivos controlados: {0}" -f $MoveFiles.Count)

if (-not $RunPilot)
{
    if ($layoutState -ne "flat")
    {
        throw "El modo de preparacion esperaba layout flat antes del primer P6A-1."
    }

    $gitStatus = @(& git -C $RepoRoot status --porcelain -- $LibraryRelative 2>&1 | ForEach-Object { $_.ToString() })
    if ($LASTEXITCODE -ne 0) { throw "No se pudo consultar git status para ST77xx." }
    if ($gitStatus.Count -ne 0)
    {
        throw ("La libreria ST77xx tiene cambios locales previos. P6A no modificara nada:`n{0}" -f ($gitStatus -join [Environment]::NewLine))
    }

    $hashes = Get-FileHashTable -Root $LibraryRoot -Names $MoveFiles
    Write-Host "Hashes de los 9 archivos fuente/header: OK" -ForegroundColor Green
    Write-Host ""
    Write-Host "=== P6A-1 PREPARACION: OK ===" -ForegroundColor Green
    Write-Host "No se movio ningun archivo y no se ejecuto ninguna compilacion."
    Write-Host "Para aplicar layout src/ y ejecutar un unico cold de equivalencia, usa -RunPilot." -ForegroundColor DarkGray
    return
}

if ($null -eq (Get-Command $ArduinoCli -ErrorAction SilentlyContinue))
{
    throw "No se encontro arduino-cli."
}

$beforeHashes = $null
if ($layoutState -eq "flat")
{
    $gitStatus = @(& git -C $RepoRoot status --porcelain -- $LibraryRelative 2>&1 | ForEach-Object { $_.ToString() })
    if ($LASTEXITCODE -ne 0) { throw "No se pudo consultar git status para ST77xx." }
    if ($gitStatus.Count -ne 0)
    {
        throw ("La libreria ST77xx tiene cambios locales previos. P6A no modificara nada:`n{0}" -f ($gitStatus -join [Environment]::NewLine))
    }

    $beforeHashes = Get-FileHashTable -Root $LibraryRoot -Names $MoveFiles
    New-Item -ItemType Directory -Path $SrcRoot -Force | Out-Null
    foreach ($name in $MoveFiles)
    {
        Move-Item -LiteralPath (Join-Path $LibraryRoot $name) -Destination (Join-Path $SrcRoot $name)
    }

    $afterHashes = Get-FileHashTable -Root $SrcRoot -Names $MoveFiles
    foreach ($name in $MoveFiles)
    {
        if ($beforeHashes[$name] -ne $afterHashes[$name])
        {
            throw "Cambio de bytes durante migracion src/: $name"
        }
    }
    Write-Host "Migracion local flat -> src/: 9 archivos movidos sin cambios de contenido." -ForegroundColor Green
}
else
{
    Write-Host "Layout src/ ya estaba aplicado; se reutilizara para el cold P6A-1." -ForegroundColor Yellow
}

$runId = (Get-Date).ToString("yyyyMMdd_HHmmss")
$runRoot = Join-Path $OutputRoot $runId
$buildPath = Join-Path $runRoot "p6a-layout-Basic"
$logPath = Join-Path $runRoot "p6a-layout-Basic.log"
$timingPath = Join-Path $runRoot "P6A_LAYOUT_TIMING_SECONDS.txt"
$summaryPath = Join-Path $runRoot "P6A_LAYOUT_SUMMARY.md"
New-Item -ItemType Directory -Path $buildPath -Force | Out-Null

$arguments = @(
    "compile",
    "-b", "jwplc_local:esp32:jwplcbasic",
    "-j", $Jobs.ToString(),
    "-v",
    "--build-path", $buildPath,
    "--clean",
    $SketchPath
)

Write-Host ""
Write-Host "Se ejecutara UN SOLO cold P6A-1 con ST77xx aun desde fuente, ahora bajo src/." -ForegroundColor Yellow
Write-Host ("arduino-cli {0}" -f ($arguments -join " ")) -ForegroundColor DarkGray

$stopwatch = [System.Diagnostics.Stopwatch]::StartNew()
$native = Invoke-NativeCaptured -FilePath $ArduinoCli -Arguments $arguments
$stopwatch.Stop()
$seconds = $stopwatch.Elapsed.TotalSeconds
@($native.Output) | Out-File -LiteralPath $logPath -Encoding utf8
("{0:R}" -f $seconds) | Set-Content -LiteralPath $timingPath -Encoding ascii
Write-Host ("Tiempo bruto preservado: {0:N3} s" -f $seconds) -ForegroundColor Green

if ($native.ExitCode -ne 0)
{
    @($native.Output | Select-Object -Last 30) | ForEach-Object { Write-Host $_ -ForegroundColor DarkYellow }
    throw "Arduino CLI fallo. Layout local src/ y artefactos quedaron preservados para diagnostico."
}

$currentMetrics = Get-CompileMetrics -BuildPath $buildPath
$currentE = Get-PreprocessCount -LogPath $logPath
$currentSelections = Get-LibrarySelections -LogPath $logPath
$baselineSelections = Get-LibrarySelections -LogPath $BaselineLogPath
$onlyBaselineSelection = @($baselineSelections | Where-Object { $currentSelections -notcontains $_ })
$onlyCurrentSelection = @($currentSelections | Where-Object { $baselineSelections -notcontains $_ })

$baselineObjects = Get-ExternalObjectTable -BuildPath $BaselineBuildPath
$currentObjects = Get-ExternalObjectTable -BuildPath $buildPath
$commonObjects = @($baselineObjects.Keys | Where-Object { $currentObjects.ContainsKey($_) })
$objectMismatch = @($commonObjects | Where-Object { $baselineObjects[$_] -ne $currentObjects[$_] })
$onlyBaselineObjects = @($baselineObjects.Keys | Where-Object { -not $currentObjects.ContainsKey($_) })
$onlyCurrentObjects = @($currentObjects.Keys | Where-Object { -not $baselineObjects.ContainsKey($_) })

$baselineBin = Get-AppBin -BuildPath $BaselineBuildPath
$currentBin = Get-AppBin -BuildPath $buildPath
$baselineBinHash = (Get-FileHash -LiteralPath $baselineBin.FullName -Algorithm SHA256).Hash.ToLowerInvariant()
$currentBinHash = (Get-FileHash -LiteralPath $currentBin.FullName -Algorithm SHA256).Hash.ToLowerInvariant()
$appSame = $baselineBinHash -eq $currentBinHash
$appDelta = [int64]$currentBin.Length - [int64]$baselineBin.Length

$summary = @(
    "# P6A-1 - ST77xx layout src/ desde fuente",
    "",
    "Run: $runId",
    "",
    ("Cold: {0:N3} s" -f $seconds),
    ("Compiles: {0}" -f $currentMetrics.Total),
    ("ST77xx: {0}" -f $currentMetrics.ST77xx),
    ("GFX: {0}" -f $currentMetrics.GFX),
    ("BusIO: {0}" -f $currentMetrics.BusIO),
    ("Ethernet source: {0}" -f $currentMetrics.Ethernet),
    ("Display source: {0}" -f $currentMetrics.Display),
    ("Core stub: {0}" -f $currentMetrics.Stub),
    ("Preprocesados -E: {0}" -f $currentE),
    ("App bytes baseline/actual: {0}/{1} delta={2}" -f $baselineBin.Length, $currentBin.Length, $appDelta),
    ("App SHA-256 identico: {0}" -f $appSame),
    ("Selecciones solo baseline/actual: {0}/{1}" -f $onlyBaselineSelection.Count, $onlyCurrentSelection.Count),
    ("Objetos externos comunes={0}, SHA distintos={1}, solo baseline={2}, solo actual={3}" -f $commonObjects.Count, $objectMismatch.Count, $onlyBaselineObjects.Count, $onlyCurrentObjects.Count)
)
$summary | Set-Content -LiteralPath $summaryPath -Encoding utf8NoBOM

Write-Host ""
Write-Host ("Resultado: {0:N3} s | total={1}, ST77xx={2}, GFX={3}, BusIO={4}, Eth={5}, Display={6}, stub={7}, -E={8}" -f $seconds, $currentMetrics.Total, $currentMetrics.ST77xx, $currentMetrics.GFX, $currentMetrics.BusIO, $currentMetrics.Ethernet, $currentMetrics.Display, $currentMetrics.Stub, $currentE) -ForegroundColor Cyan
Write-Host ("App: {0} -> {1} bytes | delta={2} | SHA identico={3}" -f $baselineBin.Length, $currentBin.Length, $appDelta, $appSame)
Write-Host ("Objetos externos: comunes={0}, SHA distintos={1}, solo baseline={2}, solo actual={3}" -f $commonObjects.Count, $objectMismatch.Count, $onlyBaselineObjects.Count, $onlyCurrentObjects.Count)

if ($currentMetrics.Total -ne 24 -or
    $currentMetrics.ST77xx -ne 4 -or
    $currentMetrics.GFX -ne 4 -or
    $currentMetrics.BusIO -ne 4 -or
    $currentMetrics.Ethernet -ne 0 -or
    $currentMetrics.Display -ne 0 -or
    $currentMetrics.Stub -ne 1)
{
    throw "P6A-1 cambio la estructura de compilacion esperada. Revisar artefactos antes de continuar."
}
if ($onlyBaselineSelection.Count -ne 0 -or $onlyCurrentSelection.Count -ne 0)
{
    throw "P6A-1 cambio la seleccion de librerias."
}
if ($objectMismatch.Count -ne 0 -or $onlyBaselineObjects.Count -ne 0 -or $onlyCurrentObjects.Count -ne 0)
{
    throw "P6A-1 cambio objetos externos a ST77xx."
}
if (-not $appSame)
{
    throw "P6A-1 compila estructuralmente, pero el app .bin no es identico al P5A. No avanzar a precompiled hasta analizarlo."
}

Write-Host ""
Write-Host "=== P6A-1 SOURCE LAYOUT: VALIDADO ===" -ForegroundColor Green
Write-Host ("App binario identico al P5A: {0}" -f $appSame)
Write-Host ("Preprocesados -E: {0} (baseline P5A: 41)" -f $currentE)
Write-Host ("Artefactos: {0}" -f $runRoot) -ForegroundColor DarkGray
Write-Host "El layout src/ queda aplicado localmente y aun NO usa precompiled=full." -ForegroundColor Yellow
