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

$BusRoot = Join-Path $LibrariesRoot "Adafruit_BusIO"
$BusSrc = Join-Path $BusRoot "src"
$BusProperties = Join-Path $BusRoot "library.properties"
$BusMarkerName = "JWPLC_Bundled_Adafruit_BusIO.h"
$BusRelative = "JWPLC/2.1.0/libraries/Adafruit_BusIO"

$StRoot = Join-Path $LibrariesRoot "Adafruit_ST7735_and_ST7789_Library"
$StProperties = Join-Path $StRoot "library.properties"
$StArchive = Join-Path $StRoot "src\esp32\libAdafruit_ST7735_and_ST7789_Library.a"

$GfxRoot = Join-Path $LibrariesRoot "Adafruit_GFX_Library"
$GfxProperties = Join-Path $GfxRoot "library.properties"
$GfxArchive = Join-Path $GfxRoot "src\esp32\libAdafruit_GFX_Library.a"

$SketchPath = Join-Path $ScriptRoot "sketches\01_empty"
$BaselineRunId = "20260809_235326"
$BaselineRoot = Join-Path (Join-Path $ScriptRoot "p6b2-gfx-precompiled-work") $BaselineRunId
$BaselineBuild = Join-Path $BaselineRoot "p6b2-Basic"
$BaselineLog = Join-Path $BaselineRoot "p6b2-Basic.log"
$OutputRoot = Join-Path $ScriptRoot "p6c-busio-layout-work"

$ExpectedSources = @(
    "Adafruit_BusIO_Register.cpp",
    "Adafruit_GenericDevice.cpp",
    "Adafruit_I2CDevice.cpp",
    "Adafruit_SPIDevice.cpp"
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
        if ($relative -match '^Adafruit_BusIO[\\/]') { continue }
        $table[$relative] = (Get-FileHash -LiteralPath $file.FullName -Algorithm SHA256).Hash.ToLowerInvariant()
    }
    return $table
}

function Get-HashTable
{
    param([Parameter(Mandatory = $true)][System.IO.FileInfo[]]$Files)
    $table = @{}
    foreach ($file in $Files)
    {
        $table[$file.Name] = (Get-FileHash -LiteralPath $file.FullName -Algorithm SHA256).Hash.ToLowerInvariant()
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
    if (($beforeKeys -join "|") -ne ($afterKeys -join "|")) { throw "$Label cambio inventario durante la migracion." }
    foreach ($key in $beforeKeys)
    {
        if ($Before[$key] -ne $After[$key]) { throw "$Label cambio bytes durante la migracion: $key" }
    }
}

if ($Jobs -lt 0) { throw "Jobs debe ser 0 o mayor." }

Write-Host "JWPLC - P6C-1 Adafruit BusIO source-layout pilot" -ForegroundColor Cyan
Write-Host ("PowerShell: {0} / {1}" -f $PSVersionTable.PSVersion, $PSVersionTable.PSEdition)
Write-Host ("Baseline P6B-2: {0}" -f $BaselineRoot)
Write-Host ""

foreach ($required in @($BusRoot, $BusProperties, $StProperties, $StArchive, $GfxProperties, $GfxArchive, $BaselineBuild, $BaselineLog, $SketchPath))
{
    if (-not (Test-Path -LiteralPath $required)) { throw "Falta requisito P6C-1: $required" }
}

if ((Get-PropertyValue -Path $StProperties -Name "precompiled") -ne "full") { throw "P6C-1 requiere ST77xx precompiled activo." }
if ((Get-PropertyValue -Path $GfxProperties -Name "precompiled") -ne "full") { throw "P6C-1 requiere GFX precompiled activo." }
if ((Get-PropertyValue -Path $BusProperties -Name "name") -ne "Adafruit BusIO") { throw "Nombre BusIO inesperado." }
if ((Get-PropertyValue -Path $BusProperties -Name "version") -ne "1.17.4") { throw "Version BusIO distinta de 1.17.4." }
if (-not [string]::IsNullOrWhiteSpace([string](Get-PropertyValue -Path $BusProperties -Name "precompiled"))) { throw "P6C-1 requiere BusIO aun sin precompiled=." }

$baselineMetrics = Get-CompileMetrics -BuildPath $BaselineBuild
$baselineE = Get-PreprocessCount -LogPath $BaselineLog
$baselineBusSources = @(Get-CompiledSourceNames -Entries $baselineMetrics.Entries -LibraryFolder "Adafruit_BusIO")
$expectedSourcesSorted = @($ExpectedSources | Sort-Object)
if ($baselineMetrics.Total -ne 16 -or $baselineMetrics.ST77xx -ne 0 -or $baselineMetrics.GFX -ne 0 -or
    $baselineMetrics.BusIO -ne 4 -or $baselineMetrics.Ethernet -ne 0 -or $baselineMetrics.Display -ne 0 -or
    $baselineMetrics.Stub -ne 1 -or $baselineE -ne 33 -or (($baselineBusSources -join "|") -ne ($expectedSourcesSorted -join "|")))
{
    throw "Baseline P6B-2 no coincide con 16/ST0/GFX0/BusIO4/Eth0/Display0/stub1/-E33 y los 4 TUs BusIO esperados."
}

$rootCode = @(Get-ChildItem -LiteralPath $BusRoot -File |
    Where-Object { $_.Extension -in @(".c", ".cc", ".cpp", ".h", ".hpp", ".S") } |
    Sort-Object Name)
$rootSources = @($rootCode | Where-Object { $_.Extension -in @(".c", ".cc", ".cpp", ".S") })
$rootHeaders = @($rootCode | Where-Object { $_.Extension -in @(".h", ".hpp") })
$rootSourceNames = @($rootSources | Select-Object -ExpandProperty Name | Sort-Object)

$srcCode = @()
if (Test-Path -LiteralPath $BusSrc)
{
    $srcCode = @(Get-ChildItem -LiteralPath $BusSrc -File |
        Where-Object { $_.Extension -in @(".c", ".cc", ".cpp", ".h", ".hpp", ".S") } |
        Sort-Object Name)
}

$layoutState = "unknown"
if ($rootCode.Count -eq 10 -and $srcCode.Count -eq 0 -and (($rootSourceNames -join "|") -eq ($expectedSourcesSorted -join "|")))
{
    $layoutState = "flat"
}
elseif ($rootCode.Count -eq 0 -and $srcCode.Count -eq 10)
{
    $srcSourceNames = @($srcCode | Where-Object { $_.Extension -in @(".c", ".cc", ".cpp", ".S") } | Select-Object -ExpandProperty Name | Sort-Object)
    if (($srcSourceNames -join "|") -ne ($expectedSourcesSorted -join "|")) { throw "Layout src/ BusIO tiene fuentes inesperadas." }
    $layoutState = "src"
}
else
{
    throw ("Layout BusIO mixto/incompleto. root={0}/10, src={1}/10. No se tocara nada." -f $rootCode.Count, $srcCode.Count)
}

$markerInRoot = Test-Path -LiteralPath (Join-Path $BusRoot $BusMarkerName)
$markerInSrc = Test-Path -LiteralPath (Join-Path $BusSrc $BusMarkerName)
if ($layoutState -eq "flat" -and (-not $markerInRoot -or $markerInSrc)) { throw "Marker BusIO no coincide con layout flat." }
if ($layoutState -eq "src" -and ($markerInRoot -or -not $markerInSrc)) { throw "Marker BusIO no coincide con layout src/." }

Write-Host "Quality gate P6B-2: OK | 16 compiles | ST77xx=0 | GFX=0 | BusIO=4 | -E=33" -ForegroundColor Green
Write-Host ("Layout BusIO actual: {0}" -f $layoutState) -ForegroundColor Green
Write-Host ("Archivos raiz controlados: 10 ({0} fuentes + {1} headers)" -f $rootSources.Count, $rootHeaders.Count)

if (-not $RunPilot)
{
    if ($layoutState -ne "flat") { throw "Preparacion P6C-1 esperaba BusIO aun en layout flat." }

    $gitStatus = @(& git -C $RepoRoot status --porcelain -- $BusRelative 2>&1 | ForEach-Object { $_.ToString() })
    if ($LASTEXITCODE -ne 0) { throw "No se pudo consultar git status para Adafruit_BusIO." }
    if ($gitStatus.Count -ne 0)
    {
        throw ("Adafruit_BusIO tiene cambios locales previos. No se movera nada:`n{0}" -f ($gitStatus -join "`n"))
    }

    $hashes = Get-HashTable -Files $rootCode
    Write-Host "Hashes archivos raiz: OK | 10/10" -ForegroundColor Green
    Write-Host ""
    Write-Host "=== P6C-1 PREPARACION: OK ===" -ForegroundColor Green
    Write-Host "No se movio ningun archivo y no se ejecuto ninguna compilacion."
    Write-Host "Para aplicar layout src/ y ejecutar un unico cold de equivalencia, usa -RunPilot." -ForegroundColor DarkGray
    return
}

if ($layoutState -ne "flat") { throw "P6C-1 -RunPilot solo puede iniciar desde layout flat." }
if ($null -eq (Get-Command $ArduinoCli -ErrorAction SilentlyContinue)) { throw "No se encontro arduino-cli." }

$beforeHashes = Get-HashTable -Files $rootCode
$runId = (Get-Date).ToString("yyyyMMdd_HHmmss")
$runRoot = Join-Path $OutputRoot $runId
$buildPath = Join-Path $runRoot "p6c-layout-Basic"
$logPath = Join-Path $runRoot "p6c-layout-Basic.log"
$timingPath = Join-Path $runRoot "P6C1_TIMING_SECONDS.txt"
$summaryPath = Join-Path $runRoot "P6C1_SUMMARY.md"
New-Item -ItemType Directory -Path $BusSrc -Force | Out-Null
New-Item -ItemType Directory -Path $buildPath -Force | Out-Null

$moved = $false
$success = $false
try
{
    foreach ($file in $rootCode)
    {
        Move-Item -LiteralPath $file.FullName -Destination (Join-Path $BusSrc $file.Name)
    }
    $moved = $true

    $afterFiles = @(Get-ChildItem -LiteralPath $BusSrc -File |
        Where-Object { $_.Extension -in @(".c", ".cc", ".cpp", ".h", ".hpp", ".S") } |
        Sort-Object Name)
    if ($afterFiles.Count -ne 10) { throw ("Migracion BusIO dejo {0} archivos en src/, esperado 10." -f $afterFiles.Count) }
    $afterHashes = Get-HashTable -Files $afterFiles
    Compare-HashTables -Before $beforeHashes -After $afterHashes -Label "BusIO"

    Write-Host "Migracion local flat -> src/: 10 archivos movidos sin cambios de contenido." -ForegroundColor Green
    Write-Host ""
    Write-Host "Se ejecutara UN SOLO cold P6C-1 con BusIO aun desde fuente, ahora bajo src/." -ForegroundColor Yellow

    $arguments = @(
        "compile",
        "-b", "jwplc_local:esp32:jwplcbasic",
        "-j", $Jobs.ToString(),
        "-v",
        "--build-path", $buildPath,
        "--clean",
        $SketchPath
    )
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
        throw "Arduino CLI fallo en P6C-1."
    }

    $currentMetrics = Get-CompileMetrics -BuildPath $buildPath
    $currentE = Get-PreprocessCount -LogPath $logPath
    $currentBusSources = @(Get-CompiledSourceNames -Entries $currentMetrics.Entries -LibraryFolder "Adafruit_BusIO")

    $baselineSelections = Get-LibrarySelections -LogPath $BaselineLog
    $currentSelections = Get-LibrarySelections -LogPath $logPath
    $onlyBaselineSelection = @($baselineSelections | Where-Object { $currentSelections -notcontains $_ })
    $onlyCurrentSelection = @($currentSelections | Where-Object { $baselineSelections -notcontains $_ })

    $baselineObjects = Get-ExternalObjectTable -BuildPath $BaselineBuild
    $currentObjects = Get-ExternalObjectTable -BuildPath $buildPath
    $commonObjects = @($baselineObjects.Keys | Where-Object { $currentObjects.ContainsKey($_) })
    $objectMismatch = @($commonObjects | Where-Object { $baselineObjects[$_] -ne $currentObjects[$_] })
    $onlyBaselineObjects = @($baselineObjects.Keys | Where-Object { -not $currentObjects.ContainsKey($_) })
    $onlyCurrentObjects = @($currentObjects.Keys | Where-Object { -not $baselineObjects.ContainsKey($_) })

    $baselineBin = Get-AppBin -BuildPath $BaselineBuild
    $currentBin = Get-AppBin -BuildPath $buildPath
    $sameBytes = $baselineBin.Length -eq $currentBin.Length
    $samePayload = $false
    if ($sameBytes) { $samePayload = Test-PayloadEquivalent -ReferencePath $baselineBin.FullName -CandidatePath $currentBin.FullName }

    Write-Host ""
    Write-Host ("Resultado: {0:N3} s | total={1}, ST77xx={2}, GFX={3}, BusIO={4}, Eth={5}, Display={6}, stub={7}, -E={8}" -f $seconds, $currentMetrics.Total, $currentMetrics.ST77xx, $currentMetrics.GFX, $currentMetrics.BusIO, $currentMetrics.Ethernet, $currentMetrics.Display, $currentMetrics.Stub, $currentE) -ForegroundColor Cyan
    Write-Host ("BusIO TUs: {0}" -f ($currentBusSources -join ", "))
    Write-Host ("App: {0} -> {1} bytes | payload equivalente={2}" -f $baselineBin.Length, $currentBin.Length, $samePayload)
    Write-Host ("Objetos externos: comunes={0}, SHA distintos={1}, solo baseline={2}, solo actual={3}" -f $commonObjects.Count, $objectMismatch.Count, $onlyBaselineObjects.Count, $onlyCurrentObjects.Count)

    if ($currentMetrics.Total -ne 16 -or $currentMetrics.ST77xx -ne 0 -or $currentMetrics.GFX -ne 0 -or
        $currentMetrics.BusIO -ne 4 -or $currentMetrics.Ethernet -ne 0 -or $currentMetrics.Display -ne 0 -or
        $currentMetrics.Stub -ne 1 -or $currentE -ne 33)
    {
        throw "P6C-1 no conserva estructura 16/ST0/GFX0/BusIO4/Eth0/Display0/stub1/-E33."
    }
    if (($currentBusSources -join "|") -ne ($expectedSourcesSorted -join "|")) { throw "P6C-1 cambio TUs BusIO." }
    if ($onlyBaselineSelection.Count -ne 0 -or $onlyCurrentSelection.Count -ne 0) { throw "P6C-1 cambio seleccion de librerias." }
    if ($objectMismatch.Count -ne 0 -or $onlyBaselineObjects.Count -ne 0 -or $onlyCurrentObjects.Count -ne 0) { throw "P6C-1 cambio objetos externos a BusIO." }
    if (-not $sameBytes -or -not $samePayload) { throw "P6C-1 cambio tamano o payload ejecutable de la app." }

    $summary = @(
        "# P6C-1 - Adafruit BusIO source layout",
        "",
        ("Run: {0}" -f $runId),
        ("Cold: {0:N3} s" -f $seconds),
        ("Compiles: {0}" -f $currentMetrics.Total),
        ("BusIO source: {0}" -f $currentMetrics.BusIO),
        ("Preprocesados g++ -E: {0}" -f $currentE),
        ("App bytes baseline/actual: {0}/{1}" -f $baselineBin.Length, $currentBin.Length),
        ("Payload equivalente: {0}" -f $samePayload),
        ("Objetos externos comunes/SHA distintos/solo baseline/solo actual: {0}/{1}/{2}/{3}" -f $commonObjects.Count, $objectMismatch.Count, $onlyBaselineObjects.Count, $onlyCurrentObjects.Count)
    )
    $summary | Set-Content -LiteralPath $summaryPath -Encoding utf8NoBOM

    $success = $true
    Write-Host ""
    Write-Host "=== P6C-1 SOURCE LAYOUT: VALIDADO ===" -ForegroundColor Green
    Write-Host ("Cold source-layout: {0:N3} s" -f $seconds)
    Write-Host ("Compiles: 16 | BusIO fuente=4 | -E=33 | app={0} B" -f $currentBin.Length)
    Write-Host ("Artefactos: {0}" -f $runRoot) -ForegroundColor DarkGray
    Write-Host "El layout BusIO src/ queda aplicado localmente y aun NO usa precompiled=full." -ForegroundColor Yellow
}
finally
{
    if (-not $success -and $moved)
    {
        Write-Host "P6C-1 no quedo validado; restaurando layout BusIO flat/root." -ForegroundColor Yellow
        foreach ($name in @($beforeHashes.Keys))
        {
            $srcPath = Join-Path $BusSrc $name
            if (Test-Path -LiteralPath $srcPath) { Move-Item -LiteralPath $srcPath -Destination (Join-Path $BusRoot $name) -Force }
        }
        if ((Test-Path -LiteralPath $BusSrc) -and @(Get-ChildItem -LiteralPath $BusSrc -Force).Count -eq 0)
        {
            Remove-Item -LiteralPath $BusSrc -Force
        }
    }
}
