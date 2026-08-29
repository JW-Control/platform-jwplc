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
$LibrariesRoot = Join-Path $PlatformRoot "libraries"

$GfxRelative = "JWPLC/2.1.0/libraries/Adafruit_GFX_Library"
$GfxRoot = Join-Path $LibrariesRoot "Adafruit_GFX_Library"
$GfxSrc = Join-Path $GfxRoot "src"
$GfxProperties = Join-Path $GfxRoot "library.properties"
$GfxFonts = Join-Path $GfxRoot "Fonts"
$GfxSrcFonts = Join-Path $GfxSrc "Fonts"

$StRoot = Join-Path $LibrariesRoot "Adafruit_ST7735_and_ST7789_Library"
$StProperties = Join-Path $StRoot "library.properties"
$StArchive = Join-Path $StRoot "src\esp32\libAdafruit_ST7735_and_ST7789_Library.a"

$SketchPath = Join-Path $ScriptRoot "sketches\01_empty"
$BaselineRunId = "20260809_232946"
$BaselineRunRoot = Join-Path (Join-Path $ScriptRoot "p6a2-st77xx-precompiled-work") $BaselineRunId
$BaselineBuildPath = Join-Path $BaselineRunRoot "p6a2-Basic"
$BaselineLogPath = Join-Path $BaselineRunRoot "p6a2-Basic.log"
$OutputRoot = Join-Path $ScriptRoot "p6b-gfx-layout-work"

$MoveFiles = @(
    "Adafruit_GFX.cpp",
    "Adafruit_GFX.h",
    "Adafruit_GrayOLED.cpp",
    "Adafruit_GrayOLED.h",
    "Adafruit_SPITFT.cpp",
    "Adafruit_SPITFT.h",
    "Adafruit_SPITFT_Macros.h",
    "gfxfont.h",
    "glcdfont.c",
    "JWPLC_Bundled_Adafruit_GFX.h"
)

$ExpectedSources = @(
    "Adafruit_GFX.cpp",
    "Adafruit_GrayOLED.cpp",
    "Adafruit_SPITFT.cpp",
    "glcdfont.c"
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

function Test-PayloadEquivalent
{
    param(
        [Parameter(Mandatory = $true)][string]$ReferencePath,
        [Parameter(Mandatory = $true)][string]$CandidatePath
    )

    $a = [System.IO.File]::ReadAllBytes($ReferencePath)
    $b = [System.IO.File]::ReadAllBytes($CandidatePath)
    if ($a.Length -ne $b.Length -or $a.Length -lt 256) { return $false }

    $elfHashStart = 0xB0
    $elfHashEnd = 0xCF
    $tailStart = $a.Length - 33

    for ($i = 0; $i -lt $a.Length; $i++)
    {
        if (($i -ge $elfHashStart -and $i -le $elfHashEnd) -or ($i -ge $tailStart)) { continue }
        if ($a[$i] -ne $b[$i]) { return $false }
    }
    return $true
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
        if ($relative -match '^Adafruit_GFX_Library[\\/]') { continue }
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

function Get-DirectoryHashTable
{
    param([Parameter(Mandatory = $true)][string]$Root)

    if (-not (Test-Path -LiteralPath $Root)) { throw "Falta directorio esperado: $Root" }
    $table = @{}
    foreach ($file in Get-ChildItem -LiteralPath $Root -Recurse -File | Sort-Object FullName)
    {
        $relative = $file.FullName.Substring($Root.Length).TrimStart('\','/')
        $table[$relative] = (Get-FileHash -LiteralPath $file.FullName -Algorithm SHA256).Hash.ToLowerInvariant()
    }
    return $table
}

function Compare-HashTables
{
    param(
        [Parameter(Mandatory = $true)]$Before,
        [Parameter(Mandatory = $true)]$After,
        [Parameter(Mandatory = $true)][string]$Label
    )

    $beforeKeys = @($Before.Keys | Sort-Object)
    $afterKeys = @($After.Keys | Sort-Object)
    if (($beforeKeys -join "|") -ne ($afterKeys -join "|"))
    {
        throw "$Label cambio su inventario durante la migracion."
    }
    foreach ($key in $beforeKeys)
    {
        if ($Before[$key] -ne $After[$key])
        {
            throw "$Label cambio bytes durante la migracion: $key"
        }
    }
}

if ($Jobs -lt 0) { throw "Jobs debe ser 0 o mayor." }

Write-Host "JWPLC - P6B-1 Adafruit GFX source-layout pilot" -ForegroundColor Cyan
Write-Host ("PowerShell: {0} / {1}" -f $PSVersionTable.PSVersion, $PSVersionTable.PSEdition)
Write-Host ("Baseline P6A-2: {0}" -f $BaselineRunRoot)
Write-Host ""

foreach ($required in @($GfxRoot, $GfxProperties, $StProperties, $StArchive, $BaselineBuildPath, $BaselineLogPath, $SketchPath))
{
    if (-not (Test-Path -LiteralPath $required)) { throw "Falta requisito P6B-1: $required" }
}

if ((Get-PropertyValue -Path $StProperties -Name "precompiled") -ne "full")
{
    throw "P6B-1 requiere P6A-2 activo: ST77xx no declara precompiled=full."
}
if ((Get-PropertyValue -Path $GfxProperties -Name "name") -ne "Adafruit GFX Library")
{
    throw "Nombre inesperado en library.properties GFX."
}
if ((Get-PropertyValue -Path $GfxProperties -Name "version") -ne "1.12.4")
{
    throw "Version GFX distinta de 1.12.4."
}
if (-not [string]::IsNullOrWhiteSpace([string](Get-PropertyValue -Path $GfxProperties -Name "precompiled")))
{
    throw "P6B-1 requiere GFX aun sin precompiled=."
}

$baselineMetrics = Get-CompileMetrics -BuildPath $BaselineBuildPath
$baselineE = Get-PreprocessCount -LogPath $BaselineLogPath
$baselineGfxSources = @(Get-CompiledSourceNames -Entries $baselineMetrics.Entries -LibraryFolder "Adafruit_GFX_Library")
$expectedSorted = @($ExpectedSources | Sort-Object)
if ($baselineMetrics.Total -ne 20 -or $baselineMetrics.ST77xx -ne 0 -or
    $baselineMetrics.GFX -ne 4 -or $baselineMetrics.BusIO -ne 4 -or
    $baselineMetrics.Ethernet -ne 0 -or $baselineMetrics.Display -ne 0 -or
    $baselineMetrics.Stub -ne 1 -or $baselineE -ne 37 -or
    (($baselineGfxSources -join "|") -ne ($expectedSorted -join "|")))
{
    throw "Baseline P6A-2 no coincide con 20/ST0/GFX4/BusIO4/Eth0/Display0/stub1/-E37 y los 4 TUs GFX esperados."
}

$rootPresent = @($MoveFiles | Where-Object { Test-Path -LiteralPath (Join-Path $GfxRoot $_) })
$srcPresent = @($MoveFiles | Where-Object { Test-Path -LiteralPath (Join-Path $GfxSrc $_) })
$fontsRootPresent = Test-Path -LiteralPath $GfxFonts
$fontsSrcPresent = Test-Path -LiteralPath $GfxSrcFonts

$layoutState = "unknown"
if ($rootPresent.Count -eq $MoveFiles.Count -and $srcPresent.Count -eq 0 -and $fontsRootPresent -and -not $fontsSrcPresent)
{
    $layoutState = "flat"
}
elseif ($rootPresent.Count -eq 0 -and $srcPresent.Count -eq $MoveFiles.Count -and -not $fontsRootPresent -and $fontsSrcPresent)
{
    $layoutState = "src"
}
else
{
    throw ("Layout GFX mixto/incompleto. root={0}/10, src={1}/10, FontsRoot={2}, FontsSrc={3}. No se tocara nada." -f $rootPresent.Count, $srcPresent.Count, $fontsRootPresent, $fontsSrcPresent)
}

Write-Host "Quality gate P6A-2: OK | 20 compiles | ST77xx=0 | GFX=4 | BusIO=4 | -E=37" -ForegroundColor Green
Write-Host ("Layout GFX actual: {0}" -f $layoutState) -ForegroundColor Green
Write-Host ("Archivos raiz controlados: {0}" -f $MoveFiles.Count)

if (-not $RunPilot)
{
    if ($layoutState -ne "flat")
    {
        throw "El modo de preparacion esperaba GFX en layout flat antes del primer P6B-1."
    }

    $gitStatus = @(& git -C $RepoRoot status --porcelain -- $GfxRelative 2>&1 | ForEach-Object { $_.ToString() })
    if ($LASTEXITCODE -ne 0) { throw "No se pudo consultar git status para Adafruit GFX." }
    if ($gitStatus.Count -ne 0)
    {
        throw ("Adafruit GFX tiene cambios locales previos. P6B-1 no modificara nada:`n{0}" -f ($gitStatus -join [Environment]::NewLine))
    }

    $fileHashes = Get-FileHashTable -Root $GfxRoot -Names $MoveFiles
    $fontHashes = Get-DirectoryHashTable -Root $GfxFonts
    Write-Host ("Hashes archivos raiz: OK | {0}/10" -f $fileHashes.Count) -ForegroundColor Green
    Write-Host ("Hashes Fonts/: OK | {0} archivos" -f $fontHashes.Count) -ForegroundColor Green
    Write-Host ""
    Write-Host "=== P6B-1 PREPARACION: OK ===" -ForegroundColor Green
    Write-Host "No se movio ningun archivo y no se ejecuto ninguna compilacion."
    Write-Host "Para aplicar layout src/ y ejecutar un unico cold de equivalencia, usa -RunPilot." -ForegroundColor DarkGray
    return
}

if ($null -eq (Get-Command $ArduinoCli -ErrorAction SilentlyContinue))
{
    throw "No se encontro arduino-cli."
}

if ($layoutState -eq "flat")
{
    $gitStatus = @(& git -C $RepoRoot status --porcelain -- $GfxRelative 2>&1 | ForEach-Object { $_.ToString() })
    if ($LASTEXITCODE -ne 0) { throw "No se pudo consultar git status para Adafruit GFX." }
    if ($gitStatus.Count -ne 0)
    {
        throw ("Adafruit GFX tiene cambios locales previos. P6B-1 no modificara nada:`n{0}" -f ($gitStatus -join [Environment]::NewLine))
    }

    $beforeFileHashes = Get-FileHashTable -Root $GfxRoot -Names $MoveFiles
    $beforeFontHashes = Get-DirectoryHashTable -Root $GfxFonts

    New-Item -ItemType Directory -Path $GfxSrc -Force | Out-Null
    foreach ($name in $MoveFiles)
    {
        Move-Item -LiteralPath (Join-Path $GfxRoot $name) -Destination (Join-Path $GfxSrc $name)
    }
    Move-Item -LiteralPath $GfxFonts -Destination $GfxSrcFonts

    $afterFileHashes = Get-FileHashTable -Root $GfxSrc -Names $MoveFiles
    $afterFontHashes = Get-DirectoryHashTable -Root $GfxSrcFonts
    Compare-HashTables -Before $beforeFileHashes -After $afterFileHashes -Label "Archivos GFX raiz"
    Compare-HashTables -Before $beforeFontHashes -After $afterFontHashes -Label "Fonts GFX"

    Write-Host ("Migracion local flat -> src/: 10 archivos + Fonts/ ({0} archivos) movidos sin cambios de contenido." -f $afterFontHashes.Count) -ForegroundColor Green
}
else
{
    Write-Host "Layout GFX src/ ya estaba aplicado; se reutilizara para el cold P6B-1." -ForegroundColor Yellow
}

$runId = (Get-Date).ToString("yyyyMMdd_HHmmss")
$runRoot = Join-Path $OutputRoot $runId
$buildPath = Join-Path $runRoot "p6b-layout-Basic"
$logPath = Join-Path $runRoot "p6b-layout-Basic.log"
$timingPath = Join-Path $runRoot "P6B_LAYOUT_TIMING_SECONDS.txt"
$summaryPath = Join-Path $runRoot "P6B_LAYOUT_SUMMARY.md"
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
Write-Host "Se ejecutara UN SOLO cold P6B-1 con GFX aun desde fuente, ahora bajo src/." -ForegroundColor Yellow
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
    throw "Arduino CLI fallo. Layout local GFX src/ y artefactos quedaron preservados para diagnostico."
}

$currentMetrics = Get-CompileMetrics -BuildPath $buildPath
$currentE = Get-PreprocessCount -LogPath $logPath
$currentGfxSources = @(Get-CompiledSourceNames -Entries $currentMetrics.Entries -LibraryFolder "Adafruit_GFX_Library")
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
$sameBytes = $baselineBin.Length -eq $currentBin.Length
$samePayload = $false
if ($sameBytes)
{
    $samePayload = Test-PayloadEquivalent -ReferencePath $baselineBin.FullName -CandidatePath $currentBin.FullName
}

$summary = @(
    "# P6B-1 - Adafruit GFX layout src/ desde fuente",
    "",
    ("Run: {0}" -f $runId),
    ("Cold: {0:N3} s" -f $seconds),
    ("Compiles: {0}" -f $currentMetrics.Total),
    ("ST77xx source: {0}" -f $currentMetrics.ST77xx),
    ("GFX source: {0}" -f $currentMetrics.GFX),
    ("BusIO source: {0}" -f $currentMetrics.BusIO),
    ("Ethernet source: {0}" -f $currentMetrics.Ethernet),
    ("Display source: {0}" -f $currentMetrics.Display),
    ("Core stub: {0}" -f $currentMetrics.Stub),
    ("Preprocesados -E: {0}" -f $currentE),
    ("GFX TUs: {0}" -f ($currentGfxSources -join ", ")),
    ("App bytes baseline/actual: {0}/{1}" -f $baselineBin.Length, $currentBin.Length),
    ("Payload equivalente: {0}" -f $samePayload),
    ("Selecciones solo baseline/actual: {0}/{1}" -f $onlyBaselineSelection.Count, $onlyCurrentSelection.Count),
    ("Objetos externos comunes={0}, SHA distintos={1}, solo baseline={2}, solo actual={3}" -f $commonObjects.Count, $objectMismatch.Count, $onlyBaselineObjects.Count, $onlyCurrentObjects.Count)
)
$summary | Set-Content -LiteralPath $summaryPath -Encoding utf8NoBOM

Write-Host ""
Write-Host ("Resultado: {0:N3} s | total={1}, ST77xx={2}, GFX={3}, BusIO={4}, Eth={5}, Display={6}, stub={7}, -E={8}" -f $seconds, $currentMetrics.Total, $currentMetrics.ST77xx, $currentMetrics.GFX, $currentMetrics.BusIO, $currentMetrics.Ethernet, $currentMetrics.Display, $currentMetrics.Stub, $currentE) -ForegroundColor Cyan
Write-Host ("GFX TUs: {0}" -f ($currentGfxSources -join ", "))
Write-Host ("App: {0} -> {1} bytes | payload equivalente={2}" -f $baselineBin.Length, $currentBin.Length, $samePayload)
Write-Host ("Objetos externos: comunes={0}, SHA distintos={1}, solo baseline={2}, solo actual={3}" -f $commonObjects.Count, $objectMismatch.Count, $onlyBaselineObjects.Count, $onlyCurrentObjects.Count)

if ($currentMetrics.Total -ne 20 -or $currentMetrics.ST77xx -ne 0 -or
    $currentMetrics.GFX -ne 4 -or $currentMetrics.BusIO -ne 4 -or
    $currentMetrics.Ethernet -ne 0 -or $currentMetrics.Display -ne 0 -or
    $currentMetrics.Stub -ne 1 -or (($currentGfxSources -join "|") -ne ($expectedSorted -join "|")))
{
    throw "P6B-1 cambio la estructura de compilacion esperada. Revisar artefactos antes de continuar."
}
if ($currentE -ne 37)
{
    throw ("P6B-1 cambio las pasadas -E; baseline=37, actual={0}. Revisar antes de continuar." -f $currentE)
}
if ($onlyBaselineSelection.Count -ne 0 -or $onlyCurrentSelection.Count -ne 0)
{
    throw "P6B-1 cambio la seleccion de librerias."
}
if ($objectMismatch.Count -ne 0 -or $onlyBaselineObjects.Count -ne 0 -or $onlyCurrentObjects.Count -ne 0)
{
    throw "P6B-1 cambio objetos externos a Adafruit GFX."
}
if (-not $sameBytes -or -not $samePayload)
{
    throw "P6B-1 cambio tamano o payload ejecutable de la app. No avanzar a precompiled hasta analizarlo."
}

Write-Host ""
Write-Host "=== P6B-1 SOURCE LAYOUT: VALIDADO ===" -ForegroundColor Green
Write-Host ("Cold source-layout: {0:N3} s" -f $seconds)
Write-Host ("Compiles: 20 | GFX fuente=4 | -E=37 | app={0} B" -f $currentBin.Length)
Write-Host ("Artefactos: {0}" -f $runRoot) -ForegroundColor DarkGray
Write-Host "El layout GFX src/ queda aplicado localmente y aun NO usa precompiled=full." -ForegroundColor Yellow
