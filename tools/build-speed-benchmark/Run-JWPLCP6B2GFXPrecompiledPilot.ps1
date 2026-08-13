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

$GfxRoot = Join-Path $LibrariesRoot "Adafruit_GFX_Library"
$GfxSrc = Join-Path $GfxRoot "src"
$GfxProperties = Join-Path $GfxRoot "library.properties"
$GfxArchive = Join-Path $GfxSrc "esp32\libAdafruit_GFX_Library.a"
$GfxFonts = Join-Path $GfxSrc "Fonts"
$GfxMarker = Join-Path $GfxSrc "JWPLC_Bundled_Adafruit_GFX.h"

$StRoot = Join-Path $LibrariesRoot "Adafruit_ST7735_and_ST7789_Library"
$StProperties = Join-Path $StRoot "library.properties"
$StArchive = Join-Path $StRoot "src\esp32\libAdafruit_ST7735_and_ST7789_Library.a"

$SketchPath = Join-Path $ScriptRoot "sketches\01_empty"
$BaselineRunId = "20260809_234653"
$BaselineRoot = Join-Path (Join-Path $ScriptRoot "p6b-gfx-layout-work") $BaselineRunId
$BaselineBuild = Join-Path $BaselineRoot "p6b-layout-Basic"
$BaselineLog = Join-Path $BaselineRoot "p6b-layout-Basic.log"
$OutputRoot = Join-Path $ScriptRoot "p6b2-gfx-precompiled-work"

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

function Find-ToolSibling
{
    param(
        [Parameter(Mandatory = $true)][string]$LogPath,
        [Parameter(Mandatory = $true)][string[]]$LeafNames
    )

    foreach ($line in Get-Content -LiteralPath $LogPath)
    {
        $compilerCandidate = $null
        if ($line -match '"(?<exe>[^"]*xtensa-esp32-elf-g\+\+(?:\.exe)?)"') { $compilerCandidate = $Matches["exe"] }
        elseif ($line -match '(?<exe>\S*xtensa-esp32-elf-g\+\+(?:\.exe)?)\s') { $compilerCandidate = $Matches["exe"] }
        if ([string]::IsNullOrWhiteSpace($compilerCandidate)) { continue }

        $compiler = Resolve-NativeToolPath -Candidate $compilerCandidate
        if ([string]::IsNullOrWhiteSpace($compiler)) { continue }
        $toolDir = Split-Path -Parent $compiler
        foreach ($leaf in $LeafNames)
        {
            $candidate = Join-Path $toolDir $leaf
            if (Test-Path -LiteralPath $candidate) { return (Resolve-Path -LiteralPath $candidate).Path }
        }
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

    $sibling = Find-ToolSibling -LogPath $LogPath -LeafNames @("xtensa-esp32-elf-gcc-ar.exe", "xtensa-esp32-elf-gcc-ar")
    if (-not [string]::IsNullOrWhiteSpace($sibling)) { return $sibling }
    throw "No se pudo localizar xtensa-esp32-elf-gcc-ar desde el log P6B-1."
}

function Find-Nm
{
    param([Parameter(Mandatory = $true)][string]$LogPath)
    $sibling = Find-ToolSibling -LogPath $LogPath -LeafNames @("xtensa-esp32-elf-nm.exe", "xtensa-esp32-elf-nm")
    if (-not [string]::IsNullOrWhiteSpace($sibling)) { return $sibling }
    throw "No se pudo localizar xtensa-esp32-elf-nm desde el log P6B-1."
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

function Get-LibrarySelections
{
    param([Parameter(Mandatory = $true)][string]$LogPath)
    return @(Get-Content -LiteralPath $LogPath |
        Where-Object { ([string]$_) -like "Using library *" } |
        ForEach-Object { ([string]$_).Trim() } |
        Sort-Object -Unique)
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

function Get-AppFile
{
    param(
        [Parameter(Mandatory = $true)][string]$BuildPath,
        [Parameter(Mandatory = $true)][string]$Extension
    )
    $file = Get-ChildItem -LiteralPath $BuildPath -Filter ("*.ino." + $Extension) -File | Select-Object -First 1
    if ($null -eq $file) { throw "No se encontro .ino.$Extension en $BuildPath" }
    return $file
}

function Get-GlobalDefinedSymbolSet
{
    param(
        [Parameter(Mandatory = $true)][string]$NmPath,
        [Parameter(Mandatory = $true)][string]$ObjectPath
    )

    $result = Invoke-NativeCaptured -FilePath $NmPath -Arguments @("-g", "--defined-only", $ObjectPath)
    if ($result.ExitCode -ne 0) { throw "nm fallo para $ObjectPath" }

    $set = @{}
    foreach ($line in $result.Output)
    {
        $text = ([string]$line).Trim()
        if ($text -match '^(?:[0-9A-Fa-f]+\s+)?\S\s+(?<name>\S+)$')
        {
            $set[$Matches["name"]] = $true
        }
    }
    return $set
}

function Get-MapSectionSize
{
    param(
        [Parameter(Mandatory = $true)][string]$MapText,
        [Parameter(Mandatory = $true)][string]$SectionName
    )
    $pattern = '(?m)^' + [regex]::Escape($SectionName) + '\s+0x[0-9A-Fa-f]+\s+0x(?<size>[0-9A-Fa-f]+)\s*$'
    $match = [regex]::Match($MapText, $pattern)
    if (-not $match.Success) { return $null }
    return [Convert]::ToInt64($match.Groups["size"].Value, 16)
}

if ($Jobs -lt 0) { throw "Jobs debe ser 0 o mayor." }

Write-Host "JWPLC - P6B-2 Adafruit GFX precompiled pilot" -ForegroundColor Cyan
Write-Host ("PowerShell: {0} / {1}" -f $PSVersionTable.PSVersion, $PSVersionTable.PSEdition)
Write-Host ("Baseline P6B-1: {0}" -f $BaselineRoot)
Write-Host ""

foreach ($required in @($GfxRoot, $GfxSrc, $GfxProperties, $GfxFonts, $GfxMarker, $StProperties, $StArchive, $BaselineBuild, $BaselineLog, $SketchPath))
{
    if (-not (Test-Path -LiteralPath $required)) { throw "Falta requisito P6B-2: $required" }
}

if ((Get-PropertyValue -Path $StProperties -Name "precompiled") -ne "full") { throw "P6B-2 requiere P6A-2 activo." }
if ((Get-PropertyValue -Path $GfxProperties -Name "name") -ne "Adafruit GFX Library") { throw "Nombre GFX inesperado." }
if ((Get-PropertyValue -Path $GfxProperties -Name "version") -ne "1.12.4") { throw "Version GFX distinta de 1.12.4." }
if (-not [string]::IsNullOrWhiteSpace([string](Get-PropertyValue -Path $GfxProperties -Name "precompiled"))) { throw "P6B-2 requiere GFX aun sin precompiled=." }
if (Test-Path -LiteralPath $GfxArchive) { throw "Ya existe archive GFX; no se sobreescribira." }

$fontFiles = @(Get-ChildItem -LiteralPath $GfxFonts -Recurse -File)
if ($fontFiles.Count -ne 52) { throw ("Inventario Fonts/ inesperado: {0}, esperado 52." -f $fontFiles.Count) }

$baselineMetrics = Get-CompileMetrics -BuildPath $BaselineBuild
$baselineE = Get-PreprocessCount -LogPath $BaselineLog
if ($baselineMetrics.Total -ne 20 -or $baselineMetrics.ST77xx -ne 0 -or $baselineMetrics.GFX -ne 4 -or
    $baselineMetrics.BusIO -ne 4 -or $baselineMetrics.Ethernet -ne 0 -or $baselineMetrics.Display -ne 0 -or
    $baselineMetrics.Stub -ne 1 -or $baselineE -ne 37)
{
    throw "Baseline P6B-1 no coincide con 20/ST0/GFX4/BusIO4/Eth0/Display0/stub1/-E37."
}

$sourceObjects = @(Get-GfxObjects -BuildPath $BaselineBuild)
$sourceObjectBytes = [int64]0
foreach ($obj in $sourceObjects) { $sourceObjectBytes += [int64]$obj.Length }
$archiver = Find-Archiver -LogPath $BaselineLog
$nm = Find-Nm -LogPath $BaselineLog

Write-Host "Quality gate P6B-1: OK | 20 compiles | ST77xx=0 | GFX=4 | BusIO=4 | -E=37" -ForegroundColor Green
Write-Host ("Objetos GFX reutilizables: {0} | {1} bytes" -f $sourceObjects.Count, $sourceObjectBytes) -ForegroundColor Green
Write-Host ("Archiver: {0}" -f $archiver) -ForegroundColor DarkGray
Write-Host ("nm: {0}" -f $nm) -ForegroundColor DarkGray
Write-Host ("Archive objetivo: {0}" -f $GfxArchive) -ForegroundColor DarkGray

if (-not $RunPilot)
{
    Write-Host ""
    Write-Host "=== P6B-2 PILOTO PREPARADO ===" -ForegroundColor Green
    Write-Host "No se genero archive, no se modifico library.properties y no se ejecuto ninguna compilacion."
    Write-Host "Usa -RunPilot para generar el archive y ejecutar UN SOLO cold P6B-2." -ForegroundColor DarkGray
    return
}

if ($null -eq (Get-Command $ArduinoCli -ErrorAction SilentlyContinue)) { throw "No se encontro arduino-cli." }

$originalProperties = Get-Content -LiteralPath $GfxProperties -Raw
$archiveDir = Split-Path -Parent $GfxArchive
$runId = (Get-Date).ToString("yyyyMMdd_HHmmss")
$runRoot = Join-Path $OutputRoot $runId
$buildPath = Join-Path $runRoot "p6b2-Basic"
$logPath = Join-Path $runRoot "p6b2-Basic.log"
$timingPath = Join-Path $runRoot "P6B2_TIMING_SECONDS.txt"
$summaryPath = Join-Path $runRoot "P6B2_SUMMARY.md"
New-Item -ItemType Directory -Path $archiveDir -Force | Out-Null
New-Item -ItemType Directory -Path $buildPath -Force | Out-Null

$success = $false
try
{
    $archiveArgs = @("crs", $GfxArchive) + @($sourceObjects | ForEach-Object { $_.FullName })
    $arResult = Invoke-NativeCaptured -FilePath $archiver -Arguments $archiveArgs
    if ($arResult.ExitCode -ne 0 -or -not (Test-Path -LiteralPath $GfxArchive)) { throw "No se pudo generar archive GFX." }

    $membersResult = Invoke-NativeCaptured -FilePath $archiver -Arguments @("t", $GfxArchive)
    if ($membersResult.ExitCode -ne 0) { throw "No se pudo listar archive GFX." }
    $members = @($membersResult.Output | ForEach-Object { ([string]$_).Trim() } | Where-Object { $_ -ne "" } | Sort-Object)
    $expectedMembers = @($ExpectedObjects | Sort-Object)
    if (($members -join "|") -ne ($expectedMembers -join "|"))
    {
        throw ("Archive GFX con miembros inesperados: {0}" -f ($members -join ","))
    }

    $archiveFile = Get-Item -LiteralPath $GfxArchive
    $archiveSha = (Get-FileHash -LiteralPath $GfxArchive -Algorithm SHA256).Hash.ToLowerInvariant()
    Write-Host ("Archive GFX generado: {0} bytes | miembros=4 | SHA-256={1}" -f $archiveFile.Length, $archiveSha) -ForegroundColor Green

    $newProperties = $originalProperties
    if (-not $newProperties.EndsWith("`n")) { $newProperties += "`r`n" }
    $newProperties += "precompiled=full`r`n"
    Set-Content -LiteralPath $GfxProperties -Value $newProperties -Encoding utf8NoBOM -NoNewline
    if ((Get-PropertyValue -Path $GfxProperties -Name "precompiled") -ne "full") { throw "No se pudo activar precompiled=full en GFX." }

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
    Write-Host "Se ejecutara UN SOLO cold P6B-2 con Adafruit GFX precompilado." -ForegroundColor Yellow
    Write-Host ("arduino-cli {0}" -f ($arguments -join " ")) -ForegroundColor DarkGray

    $sw = [System.Diagnostics.Stopwatch]::StartNew()
    $native = Invoke-NativeCaptured -FilePath $ArduinoCli -Arguments $arguments
    $sw.Stop()
    $seconds = $sw.Elapsed.TotalSeconds
    @($native.Output) | Out-File -LiteralPath $logPath -Encoding utf8
    ("{0:R}" -f $seconds) | Set-Content -LiteralPath $timingPath -Encoding ascii
    Write-Host ("Tiempo bruto preservado: {0:N3} s" -f $seconds) -ForegroundColor Green

    if ($native.ExitCode -ne 0)
    {
        @($native.Output | Select-Object -Last 30) | ForEach-Object { Write-Host $_ -ForegroundColor DarkYellow }
        throw "Arduino CLI fallo en P6B-2."
    }

    $currentMetrics = Get-CompileMetrics -BuildPath $buildPath
    $currentE = Get-PreprocessCount -LogPath $logPath

    $baseSelections = Get-LibrarySelections -LogPath $BaselineLog
    $currSelections = Get-LibrarySelections -LogPath $logPath
    $onlyBaseSelection = @($baseSelections | Where-Object { $currSelections -notcontains $_ })
    $onlyCurrSelection = @($currSelections | Where-Object { $baseSelections -notcontains $_ })

    $baseObjects = Get-ExternalObjectTable -BuildPath $BaselineBuild
    $currObjects = Get-ExternalObjectTable -BuildPath $buildPath
    $commonObjects = @($baseObjects.Keys | Where-Object { $currObjects.ContainsKey($_) })
    $objectMismatch = @($commonObjects | Where-Object { $baseObjects[$_] -ne $currObjects[$_] })
    $onlyBaseObjects = @($baseObjects.Keys | Where-Object { -not $currObjects.ContainsKey($_) })
    $onlyCurrObjects = @($currObjects.Keys | Where-Object { -not $baseObjects.ContainsKey($_) })

    $baseBin = Get-AppFile -BuildPath $BaselineBuild -Extension "bin"
    $currBin = Get-AppFile -BuildPath $buildPath -Extension "bin"
    $baseElf = Get-AppFile -BuildPath $BaselineBuild -Extension "elf"
    $currElf = Get-AppFile -BuildPath $buildPath -Extension "elf"
    $baseMap = Get-AppFile -BuildPath $BaselineBuild -Extension "map"
    $currMap = Get-AppFile -BuildPath $buildPath -Extension "map"
    $baseMapText = Get-Content -LiteralPath $baseMap.FullName -Raw
    $currMapText = Get-Content -LiteralPath $currMap.FullName -Raw

    $archiveLinked = $currMapText.Contains("libAdafruit_GFX_Library.a")
    $gfxLinked = $currMapText.Contains("Adafruit_GFX.cpp.o")
    $spiTftLinked = $currMapText.Contains("Adafruit_SPITFT.cpp.o")
    $fontLinked = $currMapText.Contains("glcdfont.c.o")
    $grayLinked = $currMapText.Contains("Adafruit_GrayOLED.cpp.o")

    $baseSymbols = Get-GlobalDefinedSymbolSet -NmPath $nm -ObjectPath $baseElf.FullName
    $currSymbols = Get-GlobalDefinedSymbolSet -NmPath $nm -ObjectPath $currElf.FullName
    $grayObject = @($sourceObjects | Where-Object { $_.Name -eq "Adafruit_GrayOLED.cpp.o" })[0]
    $graySymbols = Get-GlobalDefinedSymbolSet -NmPath $nm -ObjectPath $grayObject.FullName

    $missingSymbols = @($baseSymbols.Keys | Where-Object { -not $currSymbols.ContainsKey($_) } | Sort-Object)
    $newSymbols = @($currSymbols.Keys | Where-Object { -not $baseSymbols.ContainsKey($_) } | Sort-Object)
    $missingOutsideGray = @($missingSymbols | Where-Object { -not $graySymbols.ContainsKey($_) })

    $baseRodata = Get-MapSectionSize -MapText $baseMapText -SectionName ".flash.rodata"
    $currRodata = Get-MapSectionSize -MapText $currMapText -SectionName ".flash.rodata"
    $baseText = Get-MapSectionSize -MapText $baseMapText -SectionName ".flash.text"
    $currText = Get-MapSectionSize -MapText $currMapText -SectionName ".flash.text"

    $appDelta = [int64]$currBin.Length - [int64]$baseBin.Length
    $rodataDeltaText = "n/a"
    $textDeltaText = "n/a"
    if ($null -ne $baseRodata -and $null -ne $currRodata) { $rodataDeltaText = [string]($currRodata - $baseRodata) }
    if ($null -ne $baseText -and $null -ne $currText) { $textDeltaText = [string]($currText - $baseText) }

    Write-Host ""
    Write-Host ("Resultado: {0:N3} s | total={1}, ST77xx={2}, GFX={3}, BusIO={4}, Eth={5}, Display={6}, stub={7}, -E={8}" -f $seconds, $currentMetrics.Total, $currentMetrics.ST77xx, $currentMetrics.GFX, $currentMetrics.BusIO, $currentMetrics.Ethernet, $currentMetrics.Display, $currentMetrics.Stub, $currentE) -ForegroundColor Cyan
    Write-Host ("Archive: GFX={0} | SPITFT={1} | glcdfont={2} | GrayOLED extraido={3}" -f $gfxLinked, $spiTftLinked, $fontLinked, $grayLinked)
    Write-Host ("App: {0} -> {1} bytes | delta={2}" -f $baseBin.Length, $currBin.Length, $appDelta)
    Write-Host ("Secciones: .flash.rodata delta={0} | .flash.text delta={1}" -f $rodataDeltaText, $textDeltaText)
    Write-Host ("Simbolos globales: baseline={0} | actual={1} | perdidos={2} | nuevos={3} | perdidos fuera de GrayOLED={4}" -f $baseSymbols.Count, $currSymbols.Count, $missingSymbols.Count, $newSymbols.Count, $missingOutsideGray.Count)
    Write-Host ("Objetos externos: comunes={0}, SHA distintos={1}, solo baseline={2}, solo actual={3}" -f $commonObjects.Count, $objectMismatch.Count, $onlyBaseObjects.Count, $onlyCurrObjects.Count)

    if ($currentMetrics.Total -ne 16 -or $currentMetrics.ST77xx -ne 0 -or $currentMetrics.GFX -ne 0 -or
        $currentMetrics.BusIO -ne 4 -or $currentMetrics.Ethernet -ne 0 -or $currentMetrics.Display -ne 0 -or
        $currentMetrics.Stub -ne 1)
    {
        throw "P6B-2 no coincide con estructura esperada 16/ST0/GFX0/BusIO4/Eth0/Display0/stub1."
    }

    # El contador -E actual solo cuenta invocaciones g++. GFX elimina 3 .cpp y 1 .c,
    # por lo que el valor esperado es 37 -> 34, no 33.
    if ($currentE -ne 34) { throw ("P6B-2 esperaba -E=34 al retirar 3 TUs C++ de GFX; actual={0}." -f $currentE) }

    if ($onlyBaseSelection.Count -ne 0 -or $onlyCurrSelection.Count -ne 0) { throw "P6B-2 cambio seleccion de librerias." }
    if ($objectMismatch.Count -ne 0 -or $onlyBaseObjects.Count -ne 0 -or $onlyCurrObjects.Count -ne 0) { throw "P6B-2 cambio objetos externos a GFX." }
    if (-not $archiveLinked -or -not $gfxLinked -or -not $spiTftLinked -or -not $fontLinked) { throw "P6B-2 no demuestra enlace de GFX/SPITFT/glcdfont desde el archive." }
    if ($newSymbols.Count -ne 0) { throw ("P6B-2 introdujo simbolos globales inesperados: {0}" -f ($newSymbols -join ", ")) }
    if ($missingOutsideGray.Count -ne 0) { throw ("P6B-2 perdio simbolos fuera de Adafruit_GrayOLED: {0}" -f ($missingOutsideGray -join ", ")) }
    if ($grayLinked -and $missingSymbols.Count -ne 0) { throw "P6B-2 extrajo GrayOLED pero aun asi perdio simbolos globales del baseline." }

    $summary = @(
        "# P6B-2 - Adafruit GFX precompilado",
        "",
        ("Run: {0}" -f $runId),
        ("Cold: {0:N3} s" -f $seconds),
        ("Compiles: {0}" -f $currentMetrics.Total),
        ("ST77xx source: {0}" -f $currentMetrics.ST77xx),
        ("GFX source: {0}" -f $currentMetrics.GFX),
        ("BusIO source: {0}" -f $currentMetrics.BusIO),
        ("Preprocesados g++ -E: {0}" -f $currentE),
        ("App bytes baseline/actual: {0}/{1} delta={2}" -f $baseBin.Length, $currBin.Length, $appDelta),
        ("Archive bytes: {0}" -f $archiveFile.Length),
        ("Archive SHA-256: {0}" -f $archiveSha),
        ("Archive linked: {0}" -f $archiveLinked),
        ("Adafruit_GFX linked: {0}" -f $gfxLinked),
        ("Adafruit_SPITFT linked: {0}" -f $spiTftLinked),
        ("glcdfont linked: {0}" -f $fontLinked),
        ("Adafruit_GrayOLED linked: {0}" -f $grayLinked),
        ("Global symbols missing/new/outside GrayOLED: {0}/{1}/{2}" -f $missingSymbols.Count, $newSymbols.Count, $missingOutsideGray.Count),
        ("External objects common/mismatch/only baseline/only current: {0}/{1}/{2}/{3}" -f $commonObjects.Count, $objectMismatch.Count, $onlyBaseObjects.Count, $onlyCurrObjects.Count),
        (".flash.rodata delta: {0}" -f $rodataDeltaText),
        (".flash.text delta: {0}" -f $textDeltaText)
    )
    $summary | Set-Content -LiteralPath $summaryPath -Encoding utf8NoBOM

    $success = $true
    Write-Host ""
    Write-Host "=== P6B-2 PRECOMPILED: VALIDADO ESTRUCTURALMENTE ===" -ForegroundColor Green
    Write-Host ("Cold candidato: {0:N3} s" -f $seconds)
    Write-Host ("Compiles: 20 -> {0} | GFX fuente: 4 -> 0 | g++ -E: 37 -> {1}" -f $currentMetrics.Total, $currentE)
    if (-not $grayLinked)
    {
        Write-Host ("GrayOLED no usado quedo fuera del enlace; simbolos perdidos atribuibles a GrayOLED: {0}." -f $missingSymbols.Count) -ForegroundColor Yellow
    }
    Write-Host ("Artefactos: {0}" -f $runRoot) -ForegroundColor DarkGray
    Write-Host "precompiled=full y el archive GFX quedan activos localmente. Mantener prueba fisica TFT como gate funcional final." -ForegroundColor Yellow
}
finally
{
    if (-not $success)
    {
        Write-Host "P6B-2 no quedo validado; restaurando library.properties GFX y retirando el archive local." -ForegroundColor Yellow
        Set-Content -LiteralPath $GfxProperties -Value $originalProperties -Encoding utf8NoBOM -NoNewline
        if (Test-Path -LiteralPath $GfxArchive) { Remove-Item -LiteralPath $GfxArchive -Force }
    }
}
