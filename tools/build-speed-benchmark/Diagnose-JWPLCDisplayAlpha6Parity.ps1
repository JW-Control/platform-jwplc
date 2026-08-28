[CmdletBinding()]
param(
    [string]$RunPath = ""
)

Set-StrictMode -Version 2.0
$ErrorActionPreference = "Stop"

$ScriptRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$RepoRoot = [System.IO.Path]::GetFullPath((Join-Path $ScriptRoot "..\.."))
Set-Location $RepoRoot

function Get-LatestRun
{
    if (-not [string]::IsNullOrWhiteSpace($RunPath))
    {
        return (Resolve-Path -LiteralPath $RunPath).Path
    }

    $run = Get-ChildItem (Join-Path $ScriptRoot "results") -Directory -Filter "alpha6-display-final-*" |
        Sort-Object LastWriteTime -Descending |
        Select-Object -First 1
    if ($null -eq $run) { throw "No se encontro run alpha6-display-final-*" }
    return $run.FullName
}

function Get-OneFile
{
    param([string]$Dir, [string]$Filter)
    $f = Get-ChildItem -LiteralPath $Dir -File -Filter $Filter | Select-Object -First 1
    if ($null -eq $f) { throw "No se encontro $Filter en $Dir" }
    return $f
}

function Resolve-ToolDir
{
    param([string]$LogPath)
    foreach ($line in Get-Content -LiteralPath $LogPath)
    {
        if ($line -match '"(?<exe>[^"]*xtensa-esp32-elf-g\+\+(?:\.exe)?)"')
        {
            $exe = $Matches["exe"]
            if (Test-Path -LiteralPath $exe) { return (Split-Path -Parent (Resolve-Path -LiteralPath $exe).Path) }
        }
    }
    throw "No se pudo localizar el toolchain desde $LogPath"
}

function Resolve-Tool
{
    param([string]$Dir, [string]$Base)
    foreach ($name in @($Base + ".exe", $Base))
    {
        $p = Join-Path $Dir $name
        if (Test-Path -LiteralPath $p) { return (Resolve-Path -LiteralPath $p).Path }
    }
    throw "No se encontro $Base en $Dir"
}

function Invoke-Lines
{
    param([string]$Tool, [string[]]$Args)
    $out = @(& $Tool @Args 2>&1 | ForEach-Object { $_.ToString() })
    if ($LASTEXITCODE -ne 0) { throw "$Tool fallo: $($Args -join ' ')" }
    return $out
}

function Get-Sections
{
    param([string]$SizeTool, [string]$Elf)
    $table = @{}
    foreach ($line in Invoke-Lines -Tool $SizeTool -Args @("-A", $Elf))
    {
        if ($line -match '^\s*(?<name>\S+)\s+(?<size>\d+)\s+(?<addr>\d+)\s*$')
        {
            $table[$Matches["name"]] = [PSCustomObject]@{
                Size = [int64]$Matches["size"]
                Address = [int64]$Matches["addr"]
            }
        }
    }
    return $table
}

function Get-Symbols
{
    param([string]$NmTool, [string]$Elf)
    $table = @{}
    foreach ($line in Invoke-Lines -Tool $NmTool -Args @("-C", "-S", "--size-sort", "--defined-only", $Elf))
    {
        if ($line -match '^\s*(?<addr>[0-9a-fA-F]+)\s+(?<size>[0-9a-fA-F]+)\s+(?<type>\S)\s+(?<name>.+)$')
        {
            $name = $Matches["name"].Trim()
            $key = $Matches["type"] + "|" + $name
            if (-not $table.ContainsKey($key))
            {
                $table[$key] = [PSCustomObject]@{
                    Name = $name
                    Type = $Matches["type"]
                    Address = [Convert]::ToInt64($Matches["addr"], 16)
                    Size = [Convert]::ToInt64($Matches["size"], 16)
                }
            }
        }
    }
    return $table
}

$run = Get-LatestRun
$sourceDir = Join-Path $run "source"
$archiveDir = Join-Path $run "archive"
$sourceLog = Join-Path $run "source.log"
$archiveLog = Join-Path $run "archive.log"

foreach ($p in @($sourceDir, $archiveDir, $sourceLog, $archiveLog))
{
    if (-not (Test-Path -LiteralPath $p)) { throw "Falta: $p" }
}

$sourceBin = Get-OneFile -Dir $sourceDir -Filter "*.ino.bin"
$archiveBin = Get-OneFile -Dir $archiveDir -Filter "*.ino.bin"
$sourceElf = Get-OneFile -Dir $sourceDir -Filter "*.ino.elf"
$archiveElf = Get-OneFile -Dir $archiveDir -Filter "*.ino.elf"

$toolDir = Resolve-ToolDir -LogPath $sourceLog
$sizeTool = Resolve-Tool -Dir $toolDir -Base "xtensa-esp32-elf-size"
$nmTool = Resolve-Tool -Dir $toolDir -Base "xtensa-esp32-elf-nm"

Write-Host "=== ALPHA6 - DIAGNOSTICO PARIDAD DISPLAY ===" -ForegroundColor Cyan
Write-Host "RUN=$run"
Write-Host ("SOURCE_BIN_BYTES={0}" -f $sourceBin.Length)
Write-Host ("ARCHIVE_BIN_BYTES={0}" -f $archiveBin.Length)
Write-Host ("BIN_DELTA_BYTES={0}" -f ($archiveBin.Length - $sourceBin.Length))

$sourceSections = Get-Sections -SizeTool $sizeTool -Elf $sourceElf.FullName
$archiveSections = Get-Sections -SizeTool $sizeTool -Elf $archiveElf.FullName
$sectionNames = @($sourceSections.Keys + $archiveSections.Keys | Sort-Object -Unique)
$sectionDiffs = @()
foreach ($name in $sectionNames)
{
    $s = if ($sourceSections.ContainsKey($name)) { $sourceSections[$name].Size } else { -1 }
    $a = if ($archiveSections.ContainsKey($name)) { $archiveSections[$name].Size } else { -1 }
    if ($s -ne $a)
    {
        $sectionDiffs += [PSCustomObject]@{ Section=$name; Source=$s; Archive=$a; Delta=($a-$s) }
    }
}

Write-Host "`n=== SECTION SIZE DIFFS ==="
if ($sectionDiffs.Count -eq 0) { Write-Host "SECTION_SIZE_DIFFS=0" -ForegroundColor Green }
else { $sectionDiffs | Sort-Object Section | Format-Table -AutoSize }

$sourceSymbols = Get-Symbols -NmTool $nmTool -Elf $sourceElf.FullName
$archiveSymbols = Get-Symbols -NmTool $nmTool -Elf $archiveElf.FullName
$common = @($sourceSymbols.Keys | Where-Object { $archiveSymbols.ContainsKey($_) })
$onlySource = @($sourceSymbols.Keys | Where-Object { -not $archiveSymbols.ContainsKey($_) })
$onlyArchive = @($archiveSymbols.Keys | Where-Object { -not $sourceSymbols.ContainsKey($_) })
$sizeChanged = @()
$addressDeltas = @{}

foreach ($key in $common)
{
    $s = $sourceSymbols[$key]
    $a = $archiveSymbols[$key]
    if ($s.Size -ne $a.Size)
    {
        $sizeChanged += [PSCustomObject]@{ Symbol=$s.Name; SourceSize=$s.Size; ArchiveSize=$a.Size; Delta=($a.Size-$s.Size) }
    }
    $delta = $a.Address - $s.Address
    $dkey = $delta.ToString()
    if (-not $addressDeltas.ContainsKey($dkey)) { $addressDeltas[$dkey] = 0 }
    $addressDeltas[$dkey]++
}

Write-Host "`n=== SYMBOL SUMMARY ==="
Write-Host ("COMMON_SYMBOLS={0}" -f $common.Count)
Write-Host ("ONLY_SOURCE_SYMBOLS={0}" -f $onlySource.Count)
Write-Host ("ONLY_ARCHIVE_SYMBOLS={0}" -f $onlyArchive.Count)
Write-Host ("SYMBOL_SIZE_CHANGES={0}" -f $sizeChanged.Count)

if ($sizeChanged.Count -gt 0)
{
    $sizeChanged | Sort-Object { [math]::Abs($_.Delta) } -Descending | Select-Object -First 20 | Format-Table -AutoSize
}

Write-Host "`n=== ADDRESS DELTA DISTRIBUTION ==="
$addressDeltas.GetEnumerator() |
    ForEach-Object { [PSCustomObject]@{ DeltaBytes=[int64]$_.Key; Symbols=$_.Value } } |
    Sort-Object Symbols -Descending |
    Select-Object -First 12 |
    Format-Table -AutoSize

if ($onlySource.Count -gt 0)
{
    Write-Host "`nONLY SOURCE (primeros 20):"
    $onlySource | ForEach-Object { $sourceSymbols[$_].Name } | Select-Object -First 20
}
if ($onlyArchive.Count -gt 0)
{
    Write-Host "`nONLY ARCHIVE (primeros 20):"
    $onlyArchive | ForEach-Object { $archiveSymbols[$_].Name } | Select-Object -First 20
}

Write-Host "`n=== SIZE REPORTADO ==="
Select-String -LiteralPath $sourceLog,$archiveLog -Pattern "Sketch uses|Global variables use|Using precompiled library.*JWPLC_Display" |
    ForEach-Object { $_.Line }

Write-Host "`nALPHA6_DISPLAY_PARITY_DIAG=DONE" -ForegroundColor Green
