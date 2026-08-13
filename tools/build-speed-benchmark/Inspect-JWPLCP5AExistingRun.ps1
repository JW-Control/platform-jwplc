#requires -Version 7.4

[CmdletBinding()]
param(
    [string]$P3RunId = "20260809_190321",
    [string]$P5RunId = "20260809_214306"
)

Set-StrictMode -Version 2.0
$ErrorActionPreference = "Stop"

$ScriptRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$P3Root = Join-Path $ScriptRoot "p3-deterministic-work"
$P5Root = Join-Path $ScriptRoot "p5a-ethernet-work"

function ConvertTo-CompileDbEntries
{
    param(
        [Parameter(Mandatory = $true)][string]$Json,
        [Parameter(Mandatory = $true)][string]$SourceName
    )

    $parsed = ConvertFrom-Json -InputObject $Json
    $entries = New-Object System.Collections.Generic.List[object]

    foreach ($entry in $parsed)
    {
        [void]$entries.Add($entry)
    }

    if ($entries.Count -eq 0)
    {
        throw "compile_commands vacio: $SourceName"
    }

    foreach ($entry in $entries)
    {
        if ($null -eq $entry.PSObject.Properties['file'] -or [string]::IsNullOrWhiteSpace([string]$entry.file))
        {
            throw "Entrada sin campo file en compile_commands: $SourceName"
        }
    }

    return $entries.ToArray()
}

function Test-CompileDbParser
{
    $sample = '[{"file":"A.cpp"},{"file":"B.cpp"}]'
    $entries = @(ConvertTo-CompileDbEntries -Json $sample -SourceName "self-test")

    if ($entries.Count -ne 2 -or [string]$entries[0].file -ne "A.cpp" -or [string]$entries[1].file -ne "B.cpp")
    {
        throw "Fallo el self-test del parser de compile_commands.json."
    }
}

function Get-CompileDbMetrics
{
    param([Parameter(Mandatory = $true)][string]$BuildPath)

    $dbPath = Join-Path $BuildPath "compile_commands.json"
    if (-not (Test-Path -LiteralPath $dbPath))
    {
        throw "No existe compile_commands.json: $dbPath"
    }

    $json = Get-Content -LiteralPath $dbPath -Raw
    $entries = @(ConvertTo-CompileDbEntries -Json $json -SourceName $dbPath)
    $files = @($entries | ForEach-Object { [string]$_.file })

    return [PSCustomObject]@{
        Compiles = $entries.Count
        EthernetSource = @($files | Where-Object { $_ -match '[\\/]libraries[\\/]JWPLC_Ethernet_W5x00_Backend[\\/]' }).Count
        DisplaySource = @($files | Where-Object { $_ -match '[\\/]libraries[\\/]JWPLC_Display[\\/]' }).Count
        Stub = @($files | Where-Object { $_ -match '[\\/]cores[\\/]jwcontrol_p2[\\/]p2_core_stub\.c$' }).Count
    }
}

function Get-PreprocessCount
{
    param([Parameter(Mandatory = $true)][string]$LogPath)
    if (-not (Test-Path -LiteralPath $LogPath)) { throw "No existe log: $LogPath" }
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

function Get-ObjectTable
{
    param([Parameter(Mandatory = $true)][string]$BuildPath)

    $root = Join-Path $BuildPath "libraries"
    $table = @{}
    if (-not (Test-Path -LiteralPath $root)) { return $table }

    foreach ($file in Get-ChildItem -LiteralPath $root -Recurse -File -Filter "*.o")
    {
        $relative = $file.FullName.Substring($root.Length).TrimStart('\','/')
        if ($relative -match '^JWPLC_Display[\\/]' -or
            $relative -match '^JWPLC_Ethernet_W5x00_Backend[\\/]')
        {
            continue
        }
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

function Get-ExistingRun
{
    param(
        [Parameter(Mandatory = $true)][string]$Root,
        [Parameter(Mandatory = $true)][string]$RunId,
        [Parameter(Mandatory = $true)][string]$BuildName,
        [Parameter(Mandatory = $true)][string]$LogName
    )

    $runRoot = Join-Path $Root $RunId
    $build = Join-Path $runRoot $BuildName
    $log = Join-Path $runRoot $LogName

    if (-not (Test-Path -LiteralPath $runRoot))
    {
        throw "No existe el run solicitado: $runRoot"
    }
    if (-not (Test-Path -LiteralPath (Join-Path $build "compile_commands.json")))
    {
        throw "El run no contiene compile_commands.json: $build"
    }
    if (-not (Test-Path -LiteralPath $log))
    {
        throw "El run no contiene el log esperado: $log"
    }

    return [PSCustomObject]@{
        RunRoot = $runRoot
        BuildPath = $build
        LogPath = $log
    }
}

Test-CompileDbParser

$p3 = Get-ExistingRun -Root $P3Root -RunId $P3RunId -BuildName "p3-Basic" -LogName "p3-Basic.log"
$p5 = Get-ExistingRun -Root $P5Root -RunId $P5RunId -BuildName "p5a-Basic" -LogName "p5a-Basic.log"

$p3m = Get-CompileDbMetrics -BuildPath $p3.BuildPath
$p5m = Get-CompileDbMetrics -BuildPath $p5.BuildPath
$p3e = Get-PreprocessCount -LogPath $p3.LogPath
$p5e = Get-PreprocessCount -LogPath $p5.LogPath

Write-Host "JWPLC - inspeccion P5A existente (sin compilar)" -ForegroundColor Cyan
Write-Host ("P3 : {0}" -f $p3.RunRoot)
Write-Host ("P5A: {0}" -f $p5.RunRoot)
Write-Host ""
Write-Host ("Compile DB P3 : total={0}, Ethernet={1}, Display={2}, core stub={3}, -E={4}" -f $p3m.Compiles, $p3m.EthernetSource, $p3m.DisplaySource, $p3m.Stub, $p3e)
Write-Host ("Compile DB P5A: total={0}, Ethernet={1}, Display={2}, core stub={3}, -E={4}" -f $p5m.Compiles, $p5m.EthernetSource, $p5m.DisplaySource, $p5m.Stub, $p5e)

if ($p3m.Compiles -ne 32 -or $p3m.EthernetSource -ne 8 -or $p3m.DisplaySource -ne 0 -or $p3m.Stub -ne 1)
{
    throw "El P3 de referencia no coincide con la estructura validada esperada (32/8/0/1)."
}

if ($p5m.Compiles -ne 24 -or $p5m.EthernetSource -ne 0 -or $p5m.DisplaySource -ne 0 -or $p5m.Stub -ne 1)
{
    throw ("P5A no tiene la estructura esperada 24/0/0/1. Actual={0}/{1}/{2}/{3}" -f $p5m.Compiles, $p5m.EthernetSource, $p5m.DisplaySource, $p5m.Stub)
}

$p3Selections = Get-LibrarySelections -LogPath $p3.LogPath
$p5Selections = Get-LibrarySelections -LogPath $p5.LogPath
$onlyP3Selection = @($p3Selections | Where-Object { $p5Selections -notcontains $_ })
$onlyP5Selection = @($p5Selections | Where-Object { $p3Selections -notcontains $_ })

$p3Objects = Get-ObjectTable -BuildPath $p3.BuildPath
$p5Objects = Get-ObjectTable -BuildPath $p5.BuildPath
$common = @($p3Objects.Keys | Where-Object { $p5Objects.ContainsKey($_) })
$hashMismatch = @($common | Where-Object { $p3Objects[$_] -ne $p5Objects[$_] })
$onlyP3Objects = @($p3Objects.Keys | Where-Object { -not $p5Objects.ContainsKey($_) })
$onlyP5Objects = @($p5Objects.Keys | Where-Object { -not $p3Objects.ContainsKey($_) })

if ($onlyP3Selection.Count -ne 0 -or $onlyP5Selection.Count -ne 0)
{
    throw "Cambio la seleccion de librerias entre P3 y P5A."
}
if ($hashMismatch.Count -ne 0 -or $onlyP3Objects.Count -ne 0 -or $onlyP5Objects.Count -ne 0)
{
    throw "Cambian objetos externos a Display/Ethernet entre P3 y P5A."
}

$mapPath = Join-Path $p5.BuildPath "01_empty.ino.map"
if (-not (Test-Path -LiteralPath $mapPath)) { throw "No existe map P5A: $mapPath" }
$map = Get-Content -LiteralPath $mapPath -Raw
$archiveLinked = $map -match 'libJWPLC_Ethernet_W5x00_Backend\.a\('
$ethernetLinked = $map -match 'libJWPLC_Ethernet_W5x00_Backend\.a\(Ethernet\.cpp\.o\)'
$w5100Linked = $map -match 'libJWPLC_Ethernet_W5x00_Backend\.a\(w5100\.cpp\.o\)'

if (-not $archiveLinked -or -not $ethernetLinked -or -not $w5100Linked)
{
    throw "El map P5A no demuestra extraccion del archive Ethernet esperada."
}

$p3Bin = Get-AppBin -BuildPath $p3.BuildPath
$p5Bin = Get-AppBin -BuildPath $p5.BuildPath
$appDelta = [int64]$p5Bin.Length - [int64]$p3Bin.Length

Write-Host ""
Write-Host "=== P5A EXISTENTE: ESTRUCTURA VALIDADA ===" -ForegroundColor Green
Write-Host ("Compiles: {0} -> {1}" -f $p3m.Compiles, $p5m.Compiles)
Write-Host ("Preprocesados -E: {0} -> {1}" -f $p3e, $p5e)
Write-Host ("Ethernet source: {0} -> {1}" -f $p3m.EthernetSource, $p5m.EthernetSource)
Write-Host ("Core stub real (compile_commands): {0}" -f $p5m.Stub)
Write-Host ("App bytes: {0} -> {1} | delta={2}" -f $p3Bin.Length, $p5Bin.Length, $appDelta)
Write-Host ("Archive linked: {0} | Ethernet.cpp={1} | w5100.cpp={2}" -f $archiveLinked, $ethernetLinked, $w5100Linked)
Write-Host ("Objetos externos comunes={0}, SHA distintos={1}, solo P3={2}, solo P5A={3}" -f $common.Count, $hashMismatch.Count, $onlyP3Objects.Count, $onlyP5Objects.Count)
Write-Host ""
Write-Host "No se ejecuto ninguna compilacion." -ForegroundColor DarkGray
