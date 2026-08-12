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
$BusArchive = Join-Path $BusSrc "esp32\libAdafruit_BusIO.a"
$BusMarker = Join-Path $BusSrc "JWPLC_Bundled_Adafruit_BusIO.h"

$StRoot = Join-Path $LibrariesRoot "Adafruit_ST7735_and_ST7789_Library"
$StProperties = Join-Path $StRoot "library.properties"
$StArchive = Join-Path $StRoot "src\esp32\libAdafruit_ST7735_and_ST7789_Library.a"

$GfxRoot = Join-Path $LibrariesRoot "Adafruit_GFX_Library"
$GfxProperties = Join-Path $GfxRoot "library.properties"
$GfxArchive = Join-Path $GfxRoot "src\esp32\libAdafruit_GFX_Library.a"

$SketchPath = Join-Path $ScriptRoot "sketches\01_empty"
$BaselineRunId = "20260810_000915"
$BaselineRoot = Join-Path (Join-Path $ScriptRoot "p6c-busio-layout-work") $BaselineRunId
$BaselineBuild = Join-Path $BaselineRoot "p6c-layout-Basic"
$BaselineLog = Join-Path $BaselineRoot "p6c-layout-Basic.log"
$OutputRoot = Join-Path $ScriptRoot "p6c2-busio-precompiled-work"

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

    $tool = Find-ToolSibling -LogPath $LogPath -LeafNames @("xtensa-esp32-elf-gcc-ar.exe", "xtensa-esp32-elf-gcc-ar")
    if (-not [string]::IsNullOrWhiteSpace($tool)) { return $tool }
    throw "No se pudo localizar xtensa-esp32-elf-gcc-ar desde el log P6C-1."
}

function Find-Nm
{
    param([Parameter(Mandatory = $true)][string]$LogPath)
    $tool = Find-ToolSibling -LogPath $LogPath -LeafNames @("xtensa-esp32-elf-nm.exe", "xtensa-esp32-elf-nm")
    if (-not [string]::IsNullOrWhiteSpace($tool)) { return $tool }
    throw "No se pudo localizar xtensa-esp32-elf-nm desde el log P6C-1."
}

function Find-Objcopy
{
    param([Parameter(Mandatory = $true)][string]$LogPath)
    $tool = Find-ToolSibling -LogPath $LogPath -LeafNames @("xtensa-esp32-elf-objcopy.exe", "xtensa-esp32-elf-objcopy")
    if (-not [string]::IsNullOrWhiteSpace($tool)) { return $tool }
    throw "No se pudo localizar xtensa-esp32-elf-objcopy desde el log P6C-1."
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

function Get-LibrarySelections
{
    param([Parameter(Mandatory = $true)][string]$LogPath)
    return @(Get-Content -LiteralPath $LogPath |
        Where-Object { ([string]$_) -like "Using library *" } |
        ForEach-Object { ([string]$_).Trim() } |
        Sort-Object -Unique)
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
    if (($actual -join "|") -ne ($expected -join "|")) { throw ("Objetos BusIO inesperados. Actual={0}" -f ($actual -join ",")) }
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
        if ($relative -match '^Adafruit_BusIO[\\/]') { continue }
        $table[$relative] = [PSCustomObject]@{
            Path = $file.FullName
            Sha = (Get-FileHash -LiteralPath $file.FullName -Algorithm SHA256).Hash.ToLowerInvariant()
        }
    }
    return $table
}

function Test-ObjectSemanticEquivalent
{
    param(
        [Parameter(Mandatory = $true)][string]$ObjcopyPath,
        [Parameter(Mandatory = $true)][string]$ReferencePath,
        [Parameter(Mandatory = $true)][string]$CandidatePath,
        [Parameter(Mandatory = $true)][string]$TempRoot
    )

    New-Item -ItemType Directory -Path $TempRoot -Force | Out-Null
    $aStrip = Join-Path $TempRoot "a.strip.o"
    $bStrip = Join-Path $TempRoot "b.strip.o"
    $aBin = Join-Path $TempRoot "a.bin"
    $bBin = Join-Path $TempRoot "b.bin"

    foreach ($p in @($aStrip, $bStrip, $aBin, $bBin)) { if (Test-Path -LiteralPath $p) { Remove-Item -LiteralPath $p -Force } }

    $sa = Invoke-NativeCaptured -FilePath $ObjcopyPath -Arguments @("--strip-debug", "--strip-unneeded", $ReferencePath, $aStrip)
    $sb = Invoke-NativeCaptured -FilePath $ObjcopyPath -Arguments @("--strip-debug", "--strip-unneeded", $CandidatePath, $bStrip)
    $stripSame = $false
    if ($sa.ExitCode -eq 0 -and $sb.ExitCode -eq 0 -and (Test-Path -LiteralPath $aStrip) -and (Test-Path -LiteralPath $bStrip))
    {
        $stripSame = ((Get-FileHash -LiteralPath $aStrip -Algorithm SHA256).Hash -eq (Get-FileHash -LiteralPath $bStrip -Algorithm SHA256).Hash)
    }

    $ba = Invoke-NativeCaptured -FilePath $ObjcopyPath -Arguments @("-O", "binary", $ReferencePath, $aBin)
    $bb = Invoke-NativeCaptured -FilePath $ObjcopyPath -Arguments @("-O", "binary", $CandidatePath, $bBin)
    $allocSame = $false
    if ($ba.ExitCode -eq 0 -and $bb.ExitCode -eq 0 -and (Test-Path -LiteralPath $aBin) -and (Test-Path -LiteralPath $bBin))
    {
        $allocSame = ((Get-FileHash -LiteralPath $aBin -Algorithm SHA256).Hash -eq (Get-FileHash -LiteralPath $bBin -Algorithm SHA256).Hash)
    }

    return [PSCustomObject]@{ StripSame = $stripSame; AllocSame = $allocSame }
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
        if ($text -match '^(?:[0-9A-Fa-f]+\s+)?\S\s+(?<name>\S+)$') { $set[$Matches["name"]] = $true }
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

Write-Host "JWPLC - P6C-2 Adafruit BusIO precompiled pilot" -ForegroundColor Cyan
Write-Host ("PowerShell: {0} / {1}" -f $PSVersionTable.PSVersion, $PSVersionTable.PSEdition)
Write-Host ("Baseline P6C-1: {0}" -f $BaselineRoot)
Write-Host ""

foreach ($required in @($BusRoot, $BusSrc, $BusProperties, $BusMarker, $StProperties, $StArchive, $GfxProperties, $GfxArchive, $BaselineBuild, $BaselineLog, $SketchPath))
{
    if (-not (Test-Path -LiteralPath $required)) { throw "Falta requisito P6C-2: $required" }
}

if ((Get-PropertyValue -Path $StProperties -Name "precompiled") -ne "full") { throw "P6C-2 requiere ST77xx precompiled activo." }
if ((Get-PropertyValue -Path $GfxProperties -Name "precompiled") -ne "full") { throw "P6C-2 requiere GFX precompiled activo." }
if ((Get-PropertyValue -Path $BusProperties -Name "name") -ne "Adafruit BusIO") { throw "Nombre BusIO inesperado." }
if ((Get-PropertyValue -Path $BusProperties -Name "version") -ne "1.17.4") { throw "Version BusIO distinta de 1.17.4." }
if (-not [string]::IsNullOrWhiteSpace([string](Get-PropertyValue -Path $BusProperties -Name "precompiled"))) { throw "P6C-2 requiere BusIO aun sin precompiled=." }
if (Test-Path -LiteralPath $BusArchive) { throw "Ya existe archive BusIO; no se sobreescribira." }

$rootCode = @(Get-ChildItem -LiteralPath $BusRoot -File | Where-Object { $_.Extension -in @(".c", ".cc", ".cpp", ".h", ".hpp", ".S") })
$srcCode = @(Get-ChildItem -LiteralPath $BusSrc -File | Where-Object { $_.Extension -in @(".c", ".cc", ".cpp", ".h", ".hpp", ".S") })
if ($rootCode.Count -ne 0 -or $srcCode.Count -ne 10) { throw ("P6C-2 requiere BusIO src/ activo. root={0}, src={1}/10." -f $rootCode.Count, $srcCode.Count) }

$baselineMetrics = Get-CompileMetrics -BuildPath $BaselineBuild
$baselineE = Get-PreprocessCount -LogPath $BaselineLog
if ($baselineMetrics.Total -ne 16 -or $baselineMetrics.ST77xx -ne 0 -or $baselineMetrics.GFX -ne 0 -or
    $baselineMetrics.BusIO -ne 4 -or $baselineMetrics.Ethernet -ne 0 -or $baselineMetrics.Display -ne 0 -or
    $baselineMetrics.Stub -ne 1 -or $baselineE -ne 33)
{
    throw "Baseline P6C-1 no coincide con 16/ST0/GFX0/BusIO4/Eth0/Display0/stub1/-E33."
}

$sourceObjects = @(Get-BusObjects -BuildPath $BaselineBuild)
$sourceObjectBytes = [int64]0
foreach ($obj in $sourceObjects) { $sourceObjectBytes += [int64]$obj.Length }
$archiver = Find-Archiver -LogPath $BaselineLog
$nm = Find-Nm -LogPath $BaselineLog
$objcopy = Find-Objcopy -LogPath $BaselineLog

Write-Host "Quality gate P6C-1: OK | 16 compiles | ST77xx=0 | GFX=0 | BusIO=4 | -E=33" -ForegroundColor Green
Write-Host ("Objetos BusIO reutilizables: {0} | {1} bytes" -f $sourceObjects.Count, $sourceObjectBytes) -ForegroundColor Green
Write-Host ("Archiver: {0}" -f $archiver) -ForegroundColor DarkGray
Write-Host ("nm: {0}" -f $nm) -ForegroundColor DarkGray
Write-Host ("objcopy: {0}" -f $objcopy) -ForegroundColor DarkGray
Write-Host ("Archive objetivo: {0}" -f $BusArchive) -ForegroundColor DarkGray

if (-not $RunPilot)
{
    Write-Host ""
    Write-Host "=== P6C-2 PILOTO PREPARADO ===" -ForegroundColor Green
    Write-Host "No se genero archive, no se modifico library.properties y no se ejecuto ninguna compilacion."
    Write-Host "Usa -RunPilot para generar el archive y ejecutar UN SOLO cold P6C-2." -ForegroundColor DarkGray
    return
}

if ($null -eq (Get-Command $ArduinoCli -ErrorAction SilentlyContinue)) { throw "No se encontro arduino-cli." }

$originalProperties = Get-Content -LiteralPath $BusProperties -Raw
$archiveDir = Split-Path -Parent $BusArchive
$runId = (Get-Date).ToString("yyyyMMdd_HHmmss")
$runRoot = Join-Path $OutputRoot $runId
$buildPath = Join-Path $runRoot "p6c2-Basic"
$logPath = Join-Path $runRoot "p6c2-Basic.log"
$timingPath = Join-Path $runRoot "P6C2_TIMING_SECONDS.txt"
$summaryPath = Join-Path $runRoot "P6C2_SUMMARY.md"
$tempCompareRoot = Join-Path $runRoot "object-compare"
New-Item -ItemType Directory -Path $archiveDir -Force | Out-Null
New-Item -ItemType Directory -Path $buildPath -Force | Out-Null

$success = $false
try
{
    $archiveArgs = @("crs", $BusArchive) + @($sourceObjects | ForEach-Object { $_.FullName })
    $arResult = Invoke-NativeCaptured -FilePath $archiver -Arguments $archiveArgs
    if ($arResult.ExitCode -ne 0 -or -not (Test-Path -LiteralPath $BusArchive)) { throw "No se pudo generar archive BusIO." }

    $membersResult = Invoke-NativeCaptured -FilePath $archiver -Arguments @("t", $BusArchive)
    if ($membersResult.ExitCode -ne 0) { throw "No se pudo listar archive BusIO." }
    $members = @($membersResult.Output | ForEach-Object { ([string]$_).Trim() } | Where-Object { $_ -ne "" } | Sort-Object)
    $expectedMembers = @($ExpectedObjects | Sort-Object)
    if (($members -join "|") -ne ($expectedMembers -join "|")) { throw ("Archive BusIO con miembros inesperados: {0}" -f ($members -join ",")) }

    $archiveFile = Get-Item -LiteralPath $BusArchive
    $archiveSha = (Get-FileHash -LiteralPath $BusArchive -Algorithm SHA256).Hash.ToLowerInvariant()
    Write-Host ("Archive BusIO generado: {0} bytes | miembros=4 | SHA-256={1}" -f $archiveFile.Length, $archiveSha) -ForegroundColor Green

    $newProperties = $originalProperties
    if (-not $newProperties.EndsWith("`n")) { $newProperties += "`r`n" }
    $newProperties += "precompiled=full`r`n"
    Set-Content -LiteralPath $BusProperties -Value $newProperties -Encoding utf8NoBOM -NoNewline
    if ((Get-PropertyValue -Path $BusProperties -Name "precompiled") -ne "full") { throw "No se pudo activar precompiled=full en BusIO." }

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
    Write-Host "Se ejecutara UN SOLO cold P6C-2 con Adafruit BusIO precompilado." -ForegroundColor Yellow
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
        throw "Arduino CLI fallo en P6C-2."
    }

    $currentMetrics = Get-CompileMetrics -BuildPath $buildPath
    $currentE = Get-PreprocessCount -LogPath $logPath

    $baseSelections = Get-LibrarySelections -LogPath $BaselineLog
    $currSelections = Get-LibrarySelections -LogPath $logPath
    $onlyBaseSelection = @($baseSelections | Where-Object { $currSelections -notcontains $_ })
    $onlyCurrSelection = @($currSelections | Where-Object { $baseSelections -notcontains $_ })

    $baseObjects = Get-ExternalObjectTable -BuildPath $BaselineBuild
    $currObjects = Get-ExternalObjectTable -BuildPath $buildPath
    $commonObjects = @($baseObjects.Keys | Where-Object { $currObjects.ContainsKey($_) } | Sort-Object)
    $onlyBaseObjects = @($baseObjects.Keys | Where-Object { -not $currObjects.ContainsKey($_) } | Sort-Object)
    $onlyCurrObjects = @($currObjects.Keys | Where-Object { -not $baseObjects.ContainsKey($_) } | Sort-Object)
    $shaMismatch = @($commonObjects | Where-Object { $baseObjects[$_].Sha -ne $currObjects[$_].Sha })
    $semanticMismatch = New-Object System.Collections.Generic.List[string]
    foreach ($relative in $shaMismatch)
    {
        $safe = ($relative -replace '[^A-Za-z0-9_.-]', '_')
        $cmp = Test-ObjectSemanticEquivalent -ObjcopyPath $objcopy -ReferencePath $baseObjects[$relative].Path -CandidatePath $currObjects[$relative].Path -TempRoot (Join-Path $tempCompareRoot $safe)
        if (-not ($cmp.StripSame -and $cmp.AllocSame)) { [void]$semanticMismatch.Add($relative) }
    }

    $baseBin = Get-AppFile -BuildPath $BaselineBuild -Extension "bin"
    $currBin = Get-AppFile -BuildPath $buildPath -Extension "bin"
    $baseElf = Get-AppFile -BuildPath $BaselineBuild -Extension "elf"
    $currElf = Get-AppFile -BuildPath $buildPath -Extension "elf"
    $baseMap = Get-AppFile -BuildPath $BaselineBuild -Extension "map"
    $currMap = Get-AppFile -BuildPath $buildPath -Extension "map"
    $baseMapText = Get-Content -LiteralPath $baseMap.FullName -Raw
    $currMapText = Get-Content -LiteralPath $currMap.FullName -Raw

    $archiveLinked = $currMapText.Contains("libAdafruit_BusIO.a")
    $memberLinked = @{}
    foreach ($name in $ExpectedObjects) { $memberLinked[$name] = $currMapText.Contains($name) }

    $baseSymbols = Get-GlobalDefinedSymbolSet -NmPath $nm -ObjectPath $baseElf.FullName
    $currSymbols = Get-GlobalDefinedSymbolSet -NmPath $nm -ObjectPath $currElf.FullName
    $missingSymbols = @($baseSymbols.Keys | Where-Object { -not $currSymbols.ContainsKey($_) } | Sort-Object)
    $newSymbols = @($currSymbols.Keys | Where-Object { -not $baseSymbols.ContainsKey($_) } | Sort-Object)

    $allowedMissing = @{}
    foreach ($obj in $sourceObjects)
    {
        if (-not $memberLinked[$obj.Name])
        {
            $symbols = Get-GlobalDefinedSymbolSet -NmPath $nm -ObjectPath $obj.FullName
            foreach ($symbol in $symbols.Keys) { $allowedMissing[$symbol] = $true }
        }
    }
    $missingOutsideUnused = @($missingSymbols | Where-Object { -not $allowedMissing.ContainsKey($_) })

    $baseRodata = Get-MapSectionSize -MapText $baseMapText -SectionName ".flash.rodata"
    $currRodata = Get-MapSectionSize -MapText $currMapText -SectionName ".flash.rodata"
    $baseText = Get-MapSectionSize -MapText $baseMapText -SectionName ".flash.text"
    $currText = Get-MapSectionSize -MapText $currMapText -SectionName ".flash.text"
    $appDelta = [int64]$currBin.Length - [int64]$baseBin.Length
    $rodataDeltaText = if ($null -ne $baseRodata -and $null -ne $currRodata) { [string]($currRodata - $baseRodata) } else { "n/a" }
    $textDeltaText = if ($null -ne $baseText -and $null -ne $currText) { [string]($currText - $baseText) } else { "n/a" }

    $linkedNames = @($ExpectedObjects | Where-Object { $memberLinked[$_] })
    $notLinkedNames = @($ExpectedObjects | Where-Object { -not $memberLinked[$_] })

    Write-Host ""
    Write-Host ("Resultado: {0:N3} s | total={1}, ST77xx={2}, GFX={3}, BusIO={4}, Eth={5}, Display={6}, stub={7}, -E={8}" -f $seconds, $currentMetrics.Total, $currentMetrics.ST77xx, $currentMetrics.GFX, $currentMetrics.BusIO, $currentMetrics.Ethernet, $currentMetrics.Display, $currentMetrics.Stub, $currentE) -ForegroundColor Cyan
    Write-Host ("Archive linked={0} | miembros extraidos={1} | no extraidos={2}" -f $archiveLinked, ($linkedNames -join ", "), ($notLinkedNames -join ", "))
    Write-Host ("App: {0} -> {1} bytes | delta={2}" -f $baseBin.Length, $currBin.Length, $appDelta)
    Write-Host ("Secciones: .flash.rodata delta={0} | .flash.text delta={1}" -f $rodataDeltaText, $textDeltaText)
    Write-Host ("Simbolos globales: baseline={0} | actual={1} | perdidos={2} | nuevos={3} | perdidos fuera de miembros no extraidos={4}" -f $baseSymbols.Count, $currSymbols.Count, $missingSymbols.Count, $newSymbols.Count, $missingOutsideUnused.Count)
    Write-Host ("Objetos externos: comunes={0} | SHA distintos={1} | semanticamente distintos={2} | solo baseline={3} | solo actual={4}" -f $commonObjects.Count, $shaMismatch.Count, $semanticMismatch.Count, $onlyBaseObjects.Count, $onlyCurrObjects.Count)

    if ($currentMetrics.Total -ne 12 -or $currentMetrics.ST77xx -ne 0 -or $currentMetrics.GFX -ne 0 -or
        $currentMetrics.BusIO -ne 0 -or $currentMetrics.Ethernet -ne 0 -or $currentMetrics.Display -ne 0 -or
        $currentMetrics.Stub -ne 1)
    {
        throw "P6C-2 no coincide con estructura esperada 12/ST0/GFX0/BusIO0/Eth0/Display0/stub1."
    }

    # Los 4 TUs BusIO son C++, por lo que 33 -> 29 es la expectativa nominal.
    # Se acepta un valor menor si Arduino discovery elimina pasos adicionales; nunca debe quedar >=33.
    if ($currentE -ge 33 -or $currentE -gt 29) { throw ("P6C-2 no redujo -E como se esperaba. Baseline=33, esperado nominal<=29, actual={0}." -f $currentE) }

    if ($onlyBaseSelection.Count -ne 0 -or $onlyCurrSelection.Count -ne 0) { throw "P6C-2 cambio seleccion de librerias." }
    if ($onlyBaseObjects.Count -ne 0 -or $onlyCurrObjects.Count -ne 0) { throw "P6C-2 cambio inventario de objetos externos a BusIO." }
    if ($semanticMismatch.Count -ne 0) { throw ("P6C-2 cambio contenido ejecutable de objetos externos: {0}" -f ($semanticMismatch -join ", ")) }
    if (-not $archiveLinked) { throw "P6C-2 no demuestra enlace de libAdafruit_BusIO.a." }
    if (-not $memberLinked["Adafruit_SPIDevice.cpp.o"]) { throw "P6C-2 no extrajo Adafruit_SPIDevice.cpp.o del archive, inesperado para la pila TFT/SPI." }
    if ($newSymbols.Count -ne 0) { throw ("P6C-2 introdujo simbolos globales inesperados: {0}" -f ($newSymbols -join ", ")) }
    if ($missingOutsideUnused.Count -ne 0) { throw ("P6C-2 perdio simbolos fuera de miembros BusIO no extraidos: {0}" -f ($missingOutsideUnused -join ", ")) }

    $summary = @(
        "# P6C-2 - Adafruit BusIO precompilado",
        "",
        ("Run: {0}" -f $runId),
        ("Cold: {0:N3} s" -f $seconds),
        ("Compiles: {0}" -f $currentMetrics.Total),
        ("BusIO source: {0}" -f $currentMetrics.BusIO),
        ("Preprocesados g++ -E: {0}" -f $currentE),
        ("App bytes baseline/actual: {0}/{1} delta={2}" -f $baseBin.Length, $currBin.Length, $appDelta),
        ("Archive bytes: {0}" -f $archiveFile.Length),
        ("Archive SHA-256: {0}" -f $archiveSha),
        ("Archive linked: {0}" -f $archiveLinked),
        ("Miembros extraidos: {0}" -f ($linkedNames -join ", ")),
        ("Miembros no extraidos: {0}" -f ($notLinkedNames -join ", ")),
        ("Global symbols missing/new/outside unused members: {0}/{1}/{2}" -f $missingSymbols.Count, $newSymbols.Count, $missingOutsideUnused.Count),
        ("External objects common/SHA mismatch/semantic mismatch/only baseline/only current: {0}/{1}/{2}/{3}/{4}" -f $commonObjects.Count, $shaMismatch.Count, $semanticMismatch.Count, $onlyBaseObjects.Count, $onlyCurrObjects.Count),
        (".flash.rodata delta: {0}" -f $rodataDeltaText),
        (".flash.text delta: {0}" -f $textDeltaText)
    )
    $summary | Set-Content -LiteralPath $summaryPath -Encoding utf8NoBOM

    $success = $true
    Write-Host ""
    Write-Host "=== P6C-2 PRECOMPILED: VALIDADO ESTRUCTURALMENTE ===" -ForegroundColor Green
    Write-Host ("Cold candidato: {0:N3} s" -f $seconds)
    Write-Host ("Compiles: 16 -> {0} | BusIO fuente: 4 -> 0 | g++ -E: 33 -> {1}" -f $currentMetrics.Total, $currentE)
    if ($notLinkedNames.Count -gt 0)
    {
        Write-Host ("Miembros BusIO no usados quedaron fuera del enlace: {0}." -f ($notLinkedNames -join ", ")) -ForegroundColor Yellow
    }
    if ($shaMismatch.Count -gt 0)
    {
        Write-Host ("Objetos externos con SHA distinto pero equivalencia strip/alloc validada: {0}." -f $shaMismatch.Count) -ForegroundColor Yellow
    }
    Write-Host ("Artefactos: {0}" -f $runRoot) -ForegroundColor DarkGray
    Write-Host "precompiled=full y el archive BusIO quedan activos localmente. Mantener prueba fisica TFT/perifericos como gate funcional final." -ForegroundColor Yellow
}
finally
{
    if (-not $success)
    {
        Write-Host "P6C-2 no quedo validado; restaurando library.properties BusIO y retirando el archive local." -ForegroundColor Yellow
        Set-Content -LiteralPath $BusProperties -Value $originalProperties -Encoding utf8NoBOM -NoNewline
        if (Test-Path -LiteralPath $BusArchive) { Remove-Item -LiteralPath $BusArchive -Force }
    }
}
