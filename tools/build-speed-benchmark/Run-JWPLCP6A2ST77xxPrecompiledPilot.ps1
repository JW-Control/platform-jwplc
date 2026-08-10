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
$LibraryRoot = Join-Path $PlatformRoot "libraries\Adafruit_ST7735_and_ST7789_Library"
$SrcRoot = Join-Path $LibraryRoot "src"
$PropertiesPath = Join-Path $LibraryRoot "library.properties"
$ArchivePath = Join-Path $SrcRoot "esp32\libAdafruit_ST7735_and_ST7789_Library.a"
$SketchPath = Join-Path $ScriptRoot "sketches\01_empty"
$P6A1InspectorPath = Join-Path $ScriptRoot "Inspect-JWPLCP6AExistingLayoutRun.ps1"

$BaselineRunId = "20260809_231953"
$BaselineRunRoot = Join-Path (Join-Path $ScriptRoot "p6a-st77xx-layout-work") $BaselineRunId
$BaselineBuildPath = Join-Path $BaselineRunRoot "p6a-layout-Basic"
$BaselineLogPath = Join-Path $BaselineRunRoot "p6a-layout-Basic.log"
$OutputRoot = Join-Path $ScriptRoot "p6a2-st77xx-precompiled-work"

$MovedFiles = @(
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

$ExpectedObjectNames = @(
    "Adafruit_ST7735.cpp.o",
    "Adafruit_ST7789.cpp.o",
    "Adafruit_ST7796S.cpp.o",
    "Adafruit_ST77xx.cpp.o"
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

function Resolve-NativeToolPath
{
    param([Parameter(Mandatory = $true)][string]$Candidate)

    $normalized = $Candidate.Trim().Trim('"')
    while ($normalized.Contains("\\")) { $normalized = $normalized.Replace("\\", "\") }

    foreach ($path in @($normalized, ($normalized + ".exe")))
    {
        if (Test-Path -LiteralPath $path)
        {
            return (Resolve-Path -LiteralPath $path).Path
        }
    }
    return $null
}

function Find-Archiver
{
    param([Parameter(Mandatory = $true)][string]$LogPath)

    if (-not (Test-Path -LiteralPath $LogPath)) { throw "No existe log P6A-1: $LogPath" }
    $lines = @(Get-Content -LiteralPath $LogPath)

    foreach ($line in $lines)
    {
        $candidate = $null
        if ($line -match '"(?<exe>[^"]*xtensa-esp32-elf-gcc-ar(?:\.exe)?)"')
        {
            $candidate = $Matches["exe"]
        }
        elseif ($line -match '(?<exe>\S*xtensa-esp32-elf-gcc-ar(?:\.exe)?)\s+(?:cr|crs)\b')
        {
            $candidate = $Matches["exe"]
        }

        if (-not [string]::IsNullOrWhiteSpace($candidate))
        {
            $resolved = Resolve-NativeToolPath -Candidate $candidate
            if (-not [string]::IsNullOrWhiteSpace($resolved)) { return $resolved }
        }
    }

    foreach ($line in $lines)
    {
        $compilerCandidate = $null
        if ($line -match '"(?<exe>[^"]*xtensa-esp32-elf-g\+\+(?:\.exe)?)"')
        {
            $compilerCandidate = $Matches["exe"]
        }
        elseif ($line -match '(?<exe>\S*xtensa-esp32-elf-g\+\+(?:\.exe)?)\s')
        {
            $compilerCandidate = $Matches["exe"]
        }

        if ([string]::IsNullOrWhiteSpace($compilerCandidate)) { continue }
        $compiler = Resolve-NativeToolPath -Candidate $compilerCandidate
        if ([string]::IsNullOrWhiteSpace($compiler)) { continue }
        $toolDir = Split-Path -Parent $compiler

        foreach ($leaf in @("xtensa-esp32-elf-gcc-ar.exe", "xtensa-esp32-elf-gcc-ar"))
        {
            $sibling = Join-Path $toolDir $leaf
            if (Test-Path -LiteralPath $sibling)
            {
                return (Resolve-Path -LiteralPath $sibling).Path
            }
        }
    }

    throw "No se pudo localizar xtensa-esp32-elf-gcc-ar desde el log P6A-1."
}

function ConvertTo-EntryList
{
    param([Parameter(Mandatory = $true)]$Parsed)

    $entries = New-Object System.Collections.Generic.List[object]
    foreach ($entry in $Parsed)
    {
        if ($null -eq $entry) { throw "Entrada nula en compile_commands.json." }
        [void]$entries.Add($entry)
    }
    if ($entries.Count -eq 0) { throw "compile_commands.json vacio." }
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

function Get-ExternalObjectTable
{
    param([Parameter(Mandatory = $true)][string]$BuildPath)

    $root = Join-Path $BuildPath "libraries"
    $table = @{}
    foreach ($file in Get-ChildItem -LiteralPath $root -Recurse -File -Filter "*.o")
    {
        $relative = $file.FullName.Substring($root.Length).TrimStart('\','/')
        if ($relative -match '^Adafruit_ST7735_and_ST7789_Library[\\/]') { continue }
        $table[$relative] = (Get-FileHash -LiteralPath $file.FullName -Algorithm SHA256).Hash.ToLowerInvariant()
    }
    return $table
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

function Get-ST77xxObjects
{
    $root = Join-Path $BaselineBuildPath "libraries\Adafruit_ST7735_and_ST7789_Library"
    if (-not (Test-Path -LiteralPath $root)) { throw "Falta arbol ST77xx P6A-1: $root" }

    $objects = @(Get-ChildItem -LiteralPath $root -Recurse -File -Filter "*.o" |
        Where-Object { $ExpectedObjectNames -contains $_.Name } |
        Sort-Object Name)

    $actualNames = @($objects | Select-Object -ExpandProperty Name | Sort-Object)
    $expectedNames = @($ExpectedObjectNames | Sort-Object)
    if (($actualNames -join "|") -ne ($expectedNames -join "|"))
    {
        throw ("Objetos ST77xx P6A-1 inesperados. Actual={0}" -f ($actualNames -join ","))
    }
    return $objects
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

Write-Host "JWPLC - P6A-2 ST77xx precompiled pilot" -ForegroundColor Cyan
Write-Host ("PowerShell: {0} / {1}" -f $PSVersionTable.PSVersion, $PSVersionTable.PSEdition)
Write-Host ("Baseline P6A-1: {0}" -f $BaselineRunRoot)
Write-Host ""

foreach ($required in @($LibraryRoot, $SrcRoot, $PropertiesPath, $BaselineBuildPath, $BaselineLogPath, $P6A1InspectorPath))
{
    if (-not (Test-Path -LiteralPath $required)) { throw "Falta requisito P6A-2: $required" }
}

Write-Host "Revalidando P6A-1 existente..." -ForegroundColor Cyan
& $P6A1InspectorPath
Write-Host "Quality gate P6A-1 existente: OK" -ForegroundColor Green
Write-Host ""

$rootPresent = @($MovedFiles | Where-Object { Test-Path -LiteralPath (Join-Path $LibraryRoot $_) })
$srcPresent = @($MovedFiles | Where-Object { Test-Path -LiteralPath (Join-Path $SrcRoot $_) })
if ($rootPresent.Count -ne 0 -or $srcPresent.Count -ne $MovedFiles.Count)
{
    throw ("P6A-2 requiere layout src/ completo. root={0}/9, src={1}/9" -f $rootPresent.Count, $srcPresent.Count)
}

$precompiledValue = Get-PropertyValue -Path $PropertiesPath -Name "precompiled"
if (-not [string]::IsNullOrWhiteSpace([string]$precompiledValue))
{
    throw ("P6A-2 preparacion esperaba precompiled ausente; actual={0}" -f $precompiledValue)
}

if (Test-Path -LiteralPath $ArchivePath)
{
    throw "Ya existe archive ST77xx P6A-2. No se sobreescribira durante preparacion."
}

$baselineMetrics = Get-CompileMetrics -BuildPath $BaselineBuildPath
$baselineE = Get-PreprocessCount -LogPath $BaselineLogPath
if ($baselineMetrics.Total -ne 24 -or $baselineMetrics.ST77xx -ne 4 -or
    $baselineMetrics.GFX -ne 4 -or $baselineMetrics.BusIO -ne 4 -or
    $baselineMetrics.Ethernet -ne 0 -or $baselineMetrics.Display -ne 0 -or
    $baselineMetrics.Stub -ne 1 -or $baselineE -ne 41)
{
    throw "Baseline P6A-1 no coincide con 24/ST4/GFX4/BusIO4/Eth0/Display0/stub1/-E41."
}

$sourceObjects = @(Get-ST77xxObjects)
$sourceObjectBytes = [int64]0
foreach ($obj in $sourceObjects) { $sourceObjectBytes += [int64]$obj.Length }
$archiver = Find-Archiver -LogPath $BaselineLogPath

Write-Host "Layout src/: OK | 9/9 archivos" -ForegroundColor Green
Write-Host "precompiled: ausente" -ForegroundColor Green
Write-Host ("Objetos reutilizables P6A-1: {0} | {1} bytes" -f $sourceObjects.Count, $sourceObjectBytes) -ForegroundColor Green
Write-Host ("Archiver: {0}" -f $archiver) -ForegroundColor DarkGray
Write-Host ("Archive objetivo: {0}" -f $ArchivePath) -ForegroundColor DarkGray

if (-not $RunPilot)
{
    Write-Host ""
    Write-Host "=== P6A-2 PREPARACION: OK ===" -ForegroundColor Green
    Write-Host "No se genero archive, no se modifico library.properties y no se ejecuto ninguna compilacion."
    Write-Host "Para generar el archive y ejecutar un unico cold P6A-2, usa -RunPilot." -ForegroundColor DarkGray
    return
}

if ($null -eq (Get-Command $ArduinoCli -ErrorAction SilentlyContinue))
{
    throw "No se encontro arduino-cli."
}

$originalProperties = Get-Content -LiteralPath $PropertiesPath -Raw
$archiveDir = Split-Path -Parent $ArchivePath
$runId = (Get-Date).ToString("yyyyMMdd_HHmmss")
$runRoot = Join-Path $OutputRoot $runId
$buildPath = Join-Path $runRoot "p6a2-Basic"
$logPath = Join-Path $runRoot "p6a2-Basic.log"
$timingPath = Join-Path $runRoot "P6A2_TIMING_SECONDS.txt"
$summaryPath = Join-Path $runRoot "P6A2_SUMMARY.md"
New-Item -ItemType Directory -Path $buildPath -Force | Out-Null
New-Item -ItemType Directory -Path $archiveDir -Force | Out-Null

$success = $false
try
{
    $archiveArgs = @("crs", $ArchivePath) + @($sourceObjects | ForEach-Object { $_.FullName })
    $arResult = Invoke-NativeCaptured -FilePath $archiver -Arguments $archiveArgs
    if ($arResult.ExitCode -ne 0 -or -not (Test-Path -LiteralPath $ArchivePath))
    {
        throw "No se pudo generar archive ST77xx."
    }

    $membersResult = Invoke-NativeCaptured -FilePath $archiver -Arguments @("t", $ArchivePath)
    if ($membersResult.ExitCode -ne 0) { throw "No se pudo listar archive ST77xx." }
    $members = @($membersResult.Output | ForEach-Object { ([string]$_).Trim() } | Where-Object { $_ -ne "" } | Sort-Object)
    $expectedMembers = @($ExpectedObjectNames | Sort-Object)
    if (($members -join "|") -ne ($expectedMembers -join "|"))
    {
        throw ("Archive ST77xx con miembros inesperados: {0}" -f ($members -join ","))
    }

    $archiveFile = Get-Item -LiteralPath $ArchivePath
    $archiveSha = (Get-FileHash -LiteralPath $ArchivePath -Algorithm SHA256).Hash.ToLowerInvariant()
    Write-Host ("Archive ST77xx generado: {0} bytes | miembros=4 | SHA-256={1}" -f $archiveFile.Length, $archiveSha) -ForegroundColor Green

    $newProperties = $originalProperties
    if (-not $newProperties.EndsWith("`n")) { $newProperties += "`r`n" }
    $newProperties += "precompiled=full`r`n"
    Set-Content -LiteralPath $PropertiesPath -Value $newProperties -Encoding utf8NoBOM -NoNewline

    if ((Get-PropertyValue -Path $PropertiesPath -Name "precompiled") -ne "full")
    {
        throw "No se pudo activar precompiled=full en library.properties."
    }

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
    Write-Host "Se ejecutara UN SOLO cold P6A-2 con ST77xx precompilado." -ForegroundColor Yellow
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
        throw "Arduino CLI fallo en P6A-2."
    }

    $currentMetrics = Get-CompileMetrics -BuildPath $buildPath
    $currentE = Get-PreprocessCount -LogPath $logPath
    $baselineSelections = Get-LibrarySelections -LogPath $BaselineLogPath
    $currentSelections = Get-LibrarySelections -LogPath $logPath
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

    $mapFile = Get-ChildItem -LiteralPath $buildPath -Filter "*.map" -File | Select-Object -First 1
    if ($null -eq $mapFile) { throw "No se encontro .map P6A-2." }
    $mapText = Get-Content -LiteralPath $mapFile.FullName -Raw
    $archiveLinked = $mapText.Contains("libAdafruit_ST7735_and_ST7789_Library.a")
    $st7789Linked = $mapText.Contains("Adafruit_ST7789.cpp.o")
    $st77xxLinked = $mapText.Contains("Adafruit_ST77xx.cpp.o")

    $summary = @(
        "# P6A-2 - ST77xx precompilado",
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
        ("App bytes baseline/actual: {0}/{1}" -f $baselineBin.Length, $currentBin.Length),
        ("Payload equivalente: {0}" -f $samePayload),
        ("Archive bytes: {0}" -f $archiveFile.Length),
        ("Archive SHA-256: {0}" -f $archiveSha),
        ("Archive linked: {0}" -f $archiveLinked),
        ("ST7789 object linked: {0}" -f $st7789Linked),
        ("ST77xx object linked: {0}" -f $st77xxLinked),
        ("Selecciones solo baseline/actual: {0}/{1}" -f $onlyBaselineSelection.Count, $onlyCurrentSelection.Count),
        ("Objetos externos comunes={0}, SHA distintos={1}, solo baseline={2}, solo actual={3}" -f $commonObjects.Count, $objectMismatch.Count, $onlyBaselineObjects.Count, $onlyCurrentObjects.Count)
    )
    $summary | Set-Content -LiteralPath $summaryPath -Encoding utf8NoBOM

    Write-Host ""
    Write-Host ("Resultado: {0:N3} s | total={1}, ST77xx={2}, GFX={3}, BusIO={4}, Eth={5}, Display={6}, stub={7}, -E={8}" -f $seconds, $currentMetrics.Total, $currentMetrics.ST77xx, $currentMetrics.GFX, $currentMetrics.BusIO, $currentMetrics.Ethernet, $currentMetrics.Display, $currentMetrics.Stub, $currentE) -ForegroundColor Cyan
    Write-Host ("App: {0} -> {1} bytes | payload equivalente={2}" -f $baselineBin.Length, $currentBin.Length, $samePayload)
    Write-Host ("Archive linked={0} | ST7789={1} | ST77xx={2}" -f $archiveLinked, $st7789Linked, $st77xxLinked)
    Write-Host ("Objetos externos: comunes={0}, SHA distintos={1}, solo baseline={2}, solo actual={3}" -f $commonObjects.Count, $objectMismatch.Count, $onlyBaselineObjects.Count, $onlyCurrentObjects.Count)

    if ($currentMetrics.Total -ne 20 -or $currentMetrics.ST77xx -ne 0 -or
        $currentMetrics.GFX -ne 4 -or $currentMetrics.BusIO -ne 4 -or
        $currentMetrics.Ethernet -ne 0 -or $currentMetrics.Display -ne 0 -or
        $currentMetrics.Stub -ne 1)
    {
        throw "P6A-2 no coincide con estructura esperada 20/ST0/GFX4/BusIO4/Eth0/Display0/stub1."
    }
    if ($onlyBaselineSelection.Count -ne 0 -or $onlyCurrentSelection.Count -ne 0)
    {
        throw "P6A-2 cambio seleccion de librerias."
    }
    if ($objectMismatch.Count -ne 0 -or $onlyBaselineObjects.Count -ne 0 -or $onlyCurrentObjects.Count -ne 0)
    {
        throw "P6A-2 cambio objetos externos a ST77xx."
    }
    if (-not $sameBytes -or -not $samePayload)
    {
        throw "P6A-2 cambio tamano o payload ejecutable de la app."
    }
    if (-not $archiveLinked -or -not $st7789Linked -or -not $st77xxLinked)
    {
        throw "P6A-2 no demuestra enlace correcto del archive ST77xx."
    }

    $success = $true
    Write-Host ""
    Write-Host "=== P6A-2 PRECOMPILED: VALIDADO ===" -ForegroundColor Green
    Write-Host ("Cold candidato: {0:N3} s" -f $seconds)
    Write-Host ("Compiles: 24 -> {0} | ST77xx fuente: 4 -> 0 | -E: 41 -> {1}" -f $currentMetrics.Total, $currentE)
    Write-Host ("Artefactos: {0}" -f $runRoot) -ForegroundColor DarkGray
    Write-Host "precompiled=full y el archive quedan activos localmente para el siguiente gate." -ForegroundColor Yellow
}
finally
{
    if (-not $success)
    {
        Write-Host "P6A-2 no quedo validado; restaurando library.properties y retirando el archive local." -ForegroundColor Yellow
        Set-Content -LiteralPath $PropertiesPath -Value $originalProperties -Encoding utf8NoBOM -NoNewline
        if (Test-Path -LiteralPath $ArchivePath) { Remove-Item -LiteralPath $ArchivePath -Force }
    }
}
