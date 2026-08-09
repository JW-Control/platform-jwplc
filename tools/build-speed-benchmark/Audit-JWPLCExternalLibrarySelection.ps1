[CmdletBinding()]
param(
    [string]$LogPath = ""
)

Set-StrictMode -Version 2.0
$ErrorActionPreference = "Stop"

$ScriptRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$RepoRoot = [System.IO.Path]::GetFullPath((Join-Path $ScriptRoot "..\.."))
$PlatformLibraries = Join-Path $RepoRoot "JWPLC\2.1.0\libraries"

function Find-LatestLog
{
    foreach ($rootName in @("p4-work", "p3-work", "p2-verify-work"))
    {
        $root = Join-Path $ScriptRoot $rootName
        if (-not (Test-Path -LiteralPath $root)) { continue }
        foreach ($run in @(Get-ChildItem -LiteralPath $root -Directory | Sort-Object LastWriteTime -Descending))
        {
            $log = Get-ChildItem -LiteralPath $run.FullName -Filter "*.log" -File -ErrorAction SilentlyContinue | Select-Object -First 1
            if ($null -ne $log) { return $log.FullName }
        }
    }
    return ""
}

function Get-LibraryProperties
{
    param([string]$LibraryPath)
    $result = @{}
    $props = Join-Path $LibraryPath "library.properties"
    if (-not (Test-Path -LiteralPath $props)) { return $result }
    foreach ($line in Get-Content -LiteralPath $props)
    {
        if ($line -match '^\s*([^#=]+?)\s*=\s*(.*)$')
        {
            $result[$Matches[1].Trim()] = $Matches[2].Trim()
        }
    }
    return $result
}

function Get-CodeTable
{
    param([string]$LibraryPath)
    $table = @{}
    if (-not (Test-Path -LiteralPath $LibraryPath)) { return $table }

    $extensions = @(".h", ".hpp", ".hh", ".c", ".cpp", ".cc", ".cxx", ".S")
    foreach ($file in Get-ChildItem -LiteralPath $LibraryPath -Recurse -File)
    {
        if ($extensions -notcontains $file.Extension) { continue }
        if ($file.FullName -match '[\\/](examples|extras|docs|test|tests)[\\/]') { continue }
        $relative = $file.FullName.Substring($LibraryPath.Length).TrimStart('\','/').Replace('\','/')
        $table[$relative] = (Get-FileHash -LiteralPath $file.FullName -Algorithm SHA256).Hash.ToLowerInvariant()
    }
    return $table
}

function Compare-CodeTrees
{
    param([string]$A, [string]$B)
    if (-not (Test-Path -LiteralPath $A) -or -not (Test-Path -LiteralPath $B))
    {
        return [PSCustomObject]@{ Comparable = $false; Same = $false; Common = 0; Different = 0; OnlyA = 0; OnlyB = 0 }
    }

    $ta = Get-CodeTable $A
    $tb = Get-CodeTable $B
    $common = @($ta.Keys | Where-Object { $tb.ContainsKey($_) })
    $different = @($common | Where-Object { $ta[$_] -ne $tb[$_] })
    $onlyA = @($ta.Keys | Where-Object { -not $tb.ContainsKey($_) })
    $onlyB = @($tb.Keys | Where-Object { -not $ta.ContainsKey($_) })
    $same = ($different.Count -eq 0 -and $onlyA.Count -eq 0 -and $onlyB.Count -eq 0)
    return [PSCustomObject]@{ Comparable = $true; Same = $same; Common = $common.Count; Different = $different.Count; OnlyA = $onlyA.Count; OnlyB = $onlyB.Count }
}

if ([string]::IsNullOrWhiteSpace($LogPath)) { $LogPath = Find-LatestLog }
if ([string]::IsNullOrWhiteSpace($LogPath) -or -not (Test-Path -LiteralPath $LogPath))
{
    throw "No se encontro un log de build. Usa -LogPath si deseas indicar uno manualmente."
}
$LogPath = (Resolve-Path -LiteralPath $LogPath).Path

$selected = @{}
foreach ($line in Get-Content -LiteralPath $LogPath)
{
    if ($line -match '^Using library (.+?) at version (.+?) in folder: (.+)$')
    {
        $selected[$Matches[1].Trim()] = [PSCustomObject]@{
            Name = $Matches[1].Trim()
            Version = $Matches[2].Trim()
            Path = $Matches[3].Trim()
        }
    }
}

$targets = @(
    [PSCustomObject]@{ Name = "Adafruit ST7735 and ST7789 Library"; BundledDir = "Adafruit_ST7735_and_ST7789_Library"; RequiredHeader = "Adafruit_ST7789.h" },
    [PSCustomObject]@{ Name = "Adafruit GFX Library"; BundledDir = "Adafruit_GFX_Library"; RequiredHeader = "Adafruit_GFX.h" },
    [PSCustomObject]@{ Name = "Adafruit BusIO"; BundledDir = "Adafruit_BusIO"; RequiredHeader = "Adafruit_SPIDevice.h" },
    [PSCustomObject]@{ Name = "Ethernet"; BundledDir = "Ethernet"; RequiredHeader = "Ethernet.h" }
)

Write-Host "JWPLC - auditor de seleccion de librerias externas" -ForegroundColor Cyan
Write-Host ("Log: {0}" -f $LogPath)
Write-Host ""

foreach ($target in $targets)
{
    Write-Host ("=== {0} ===" -f $target.Name) -ForegroundColor Cyan
    $bundle = Join-Path $PlatformLibraries $target.BundledDir
    $bundleProps = Get-LibraryProperties $bundle

    if (-not $selected.ContainsKey($target.Name))
    {
        Write-Host "Seleccionada por build: NO DETECTADA" -ForegroundColor Yellow
        Write-Host ("Bundled: {0}" -f $bundle)
        Write-Host ""
        continue
    }

    $sel = $selected[$target.Name]
    $selProps = Get-LibraryProperties $sel.Path
    $selVersion = $sel.Version
    $bundleVersion = if ($bundleProps.ContainsKey("version")) { $bundleProps["version"] } else { "?" }
    $selArch = if ($selProps.ContainsKey("architectures")) { $selProps["architectures"] } else { "?" }
    $bundleArch = if ($bundleProps.ContainsKey("architectures")) { $bundleProps["architectures"] } else { "?" }

    Write-Host ("Seleccionada: {0}" -f $sel.Path)
    Write-Host ("Version seleccionada: {0} | bundled: {1}" -f $selVersion, $bundleVersion)
    Write-Host ("architectures seleccionada: {0} | bundled: {1}" -f $selArch, $bundleArch)
    Write-Host ("Bundled: {0}" -f $bundle)

    $selectedHeader = Get-ChildItem -LiteralPath $sel.Path -Recurse -File -Filter $target.RequiredHeader -ErrorAction SilentlyContinue | Select-Object -First 1
    $bundledHeader = Get-ChildItem -LiteralPath $bundle -Recurse -File -Filter $target.RequiredHeader -ErrorAction SilentlyContinue | Select-Object -First 1
    Write-Host ("Header {0}: seleccionada={1}, bundled={2}" -f $target.RequiredHeader, ($null -ne $selectedHeader), ($null -ne $bundledHeader))

    $comparison = Compare-CodeTrees $sel.Path $bundle
    if ($comparison.Comparable)
    {
        Write-Host ("Arbol de codigo identico={0} | comunes={1} distintos={2} solo seleccionada={3} solo bundled={4}" -f $comparison.Same, $comparison.Common, $comparison.Different, $comparison.OnlyA, $comparison.OnlyB)
    }
    else
    {
        Write-Host "No fue posible comparar ambos arboles de codigo." -ForegroundColor Yellow
    }

    if ($target.Name -eq "Ethernet" -and $null -eq $bundledHeader)
    {
        Write-Host "NOTA: la carpeta bundled Ethernet no proporciona Ethernet.h; corresponde al backend ETH.h del core ESP32, no a Arduino Ethernet clasica." -ForegroundColor Yellow
    }
    Write-Host ""
}

Write-Host "Auditoria terminada. No se modifico ningun archivo ni se ejecuto compilacion." -ForegroundColor Green
