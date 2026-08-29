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

function Get-MapData
{
    param([string]$MapPath)

    $sections = @{}
    $symbols = @{}
    $fills = New-Object System.Collections.Generic.List[object]

    foreach ($line in Get-Content -LiteralPath $MapPath)
    {
        # Secciones de salida del linker. Exigimos que empiecen en columna 0 para
        # no confundir subsecciones de objetos de entrada con secciones finales.
        if ($line -match '^(?<name>\.[^\s]+)\s+0x(?<addr>[0-9a-fA-F]+)\s+0x(?<size>[0-9a-fA-F]+)(?:\s|$)')
        {
            $name = $Matches["name"]
            if (-not $sections.ContainsKey($name))
            {
                $sections[$name] = [PSCustomObject]@{
                    Address = [Convert]::ToInt64($Matches["addr"], 16)
                    Size = [Convert]::ToInt64($Matches["size"], 16)
                }
            }
            continue
        }

        if ($line -match '^\s+\*fill\*\s+0x(?<addr>[0-9a-fA-F]+)\s+0x(?<size>[0-9a-fA-F]+)')
        {
            $fills.Add([PSCustomObject]@{
                Address = [Convert]::ToInt64($Matches["addr"], 16)
                Size = [Convert]::ToInt64($Matches["size"], 16)
            })
            continue
        }

        # El map de GNU ld lista simbolos definidos como: 0xADDR NOMBRE.
        # Solo guardamos nombres de una sola palabra para evitar asignaciones y
        # texto descriptivo del script de linker.
        if ($line -match '^\s+0x(?<addr>[0-9a-fA-F]+)\s+(?<name>[^\s=]+)\s*$')
        {
            $name = $Matches["name"]
            if ($name -notmatch '^(?:0x|\.|\*|LOAD$|PROVIDE|ASSERT)')
            {
                if (-not $symbols.ContainsKey($name))
                {
                    $symbols[$name] = [Convert]::ToInt64($Matches["addr"], 16)
                }
            }
        }
    }

    return [PSCustomObject]@{
        Sections = $sections
        Symbols = $symbols
        Fills = $fills
    }
}

function Get-BinaryAlignmentProbe
{
    param([string]$SourcePath, [string]$ArchivePath)

    $s = [System.IO.File]::ReadAllBytes($SourcePath)
    $a = [System.IO.File]::ReadAllBytes($ArchivePath)
    $min = [Math]::Min($s.Length, $a.Length)

    $first = -1
    for ($i = 0; $i -lt $min; $i++)
    {
        if ($s[$i] -ne $a[$i]) { $first = $i; break }
    }

    if ($first -lt 0)
    {
        return [PSCustomObject]@{ FirstDiff=-1; BestShift=0; Matches=0; Compared=0 }
    }

    # Busca si, despues de la primera diferencia, uno de los payloads vuelve a
    # sincronizarse con un pequeno desplazamiento. Es diagnostico, no criterio PASS.
    $start = [Math]::Min($min - 1, $first + 256)
    $window = 8192
    $bestShift = 0
    $bestMatches = -1
    $bestCompared = 0

    for ($shift = -32; $shift -le 32; $shift++)
    {
        $matches = 0
        $compared = 0
        for ($j = 0; $j -lt $window; $j++)
        {
            $si = $start + $j
            $ai = $si + $shift
            if ($si -lt 0 -or $si -ge $s.Length -or $ai -lt 0 -or $ai -ge $a.Length) { continue }
            $compared++
            if ($s[$si] -eq $a[$ai]) { $matches++ }
        }

        if ($matches -gt $bestMatches)
        {
            $bestMatches = $matches
            $bestCompared = $compared
            $bestShift = $shift
        }
    }

    return [PSCustomObject]@{
        FirstDiff = $first
        BestShift = $bestShift
        Matches = $bestMatches
        Compared = $bestCompared
    }
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
$sourceMap = Get-OneFile -Dir $sourceDir -Filter "*.ino.map"
$archiveMap = Get-OneFile -Dir $archiveDir -Filter "*.ino.map"

Write-Host "=== ALPHA6 - DIAGNOSTICO PARIDAD DISPLAY ===" -ForegroundColor Cyan
Write-Host "RUN=$run"
Write-Host "MODE=MAP_ONLY_NO_BINUTILS"
Write-Host ("SOURCE_BIN_BYTES={0}" -f $sourceBin.Length)
Write-Host ("ARCHIVE_BIN_BYTES={0}" -f $archiveBin.Length)
Write-Host ("BIN_DELTA_BYTES={0}" -f ($archiveBin.Length - $sourceBin.Length))

$sourceData = Get-MapData -MapPath $sourceMap.FullName
$archiveData = Get-MapData -MapPath $archiveMap.FullName

$sectionNames = @($sourceData.Sections.Keys + $archiveData.Sections.Keys | Sort-Object -Unique)
$sectionDiffs = @()
foreach ($name in $sectionNames)
{
    $sSize = if ($sourceData.Sections.ContainsKey($name)) { $sourceData.Sections[$name].Size } else { -1 }
    $aSize = if ($archiveData.Sections.ContainsKey($name)) { $archiveData.Sections[$name].Size } else { -1 }
    $sAddr = if ($sourceData.Sections.ContainsKey($name)) { $sourceData.Sections[$name].Address } else { -1 }
    $aAddr = if ($archiveData.Sections.ContainsKey($name)) { $archiveData.Sections[$name].Address } else { -1 }

    if ($sSize -ne $aSize -or $sAddr -ne $aAddr)
    {
        $sectionDiffs += [PSCustomObject]@{
            Section = $name
            SourceSize = $sSize
            ArchiveSize = $aSize
            DeltaSize = $aSize - $sSize
            SourceAddr = if ($sAddr -ge 0) { ('0x{0:X}' -f $sAddr) } else { '-' }
            ArchiveAddr = if ($aAddr -ge 0) { ('0x{0:X}' -f $aAddr) } else { '-' }
            DeltaAddr = if ($sAddr -ge 0 -and $aAddr -ge 0) { $aAddr - $sAddr } else { 0 }
        }
    }
}

Write-Host "`n=== OUTPUT SECTION DIFFS ==="
if ($sectionDiffs.Count -eq 0)
{
    Write-Host "OUTPUT_SECTION_DIFFS=0" -ForegroundColor Green
}
else
{
    $sectionDiffs | Sort-Object Section | Format-Table -AutoSize
}

$sourceFillTotal = [int64](($sourceData.Fills | Measure-Object -Property Size -Sum).Sum)
$archiveFillTotal = [int64](($archiveData.Fills | Measure-Object -Property Size -Sum).Sum)
Write-Host "`n=== LINKER FILL ==="
Write-Host ("SOURCE_FILL_BYTES={0}" -f $sourceFillTotal)
Write-Host ("ARCHIVE_FILL_BYTES={0}" -f $archiveFillTotal)
Write-Host ("FILL_DELTA_BYTES={0}" -f ($archiveFillTotal - $sourceFillTotal))

$commonSymbols = @($sourceData.Symbols.Keys | Where-Object { $archiveData.Symbols.ContainsKey($_) })
$onlySource = @($sourceData.Symbols.Keys | Where-Object { -not $archiveData.Symbols.ContainsKey($_) })
$onlyArchive = @($archiveData.Symbols.Keys | Where-Object { -not $sourceData.Symbols.ContainsKey($_) })
$addressDeltas = @{}
foreach ($name in $commonSymbols)
{
    $delta = $archiveData.Symbols[$name] - $sourceData.Symbols[$name]
    $key = $delta.ToString()
    if (-not $addressDeltas.ContainsKey($key)) { $addressDeltas[$key] = 0 }
    $addressDeltas[$key]++
}

Write-Host "`n=== MAP SYMBOL SUMMARY ==="
Write-Host ("COMMON_SYMBOLS={0}" -f $commonSymbols.Count)
Write-Host ("ONLY_SOURCE_SYMBOLS={0}" -f $onlySource.Count)
Write-Host ("ONLY_ARCHIVE_SYMBOLS={0}" -f $onlyArchive.Count)

Write-Host "`n=== ADDRESS DELTA DISTRIBUTION ==="
$addressDeltas.GetEnumerator() |
    ForEach-Object { [PSCustomObject]@{ DeltaBytes=[int64]$_.Key; Symbols=$_.Value } } |
    Sort-Object Symbols -Descending |
    Select-Object -First 12 |
    Format-Table -AutoSize

if ($onlySource.Count -gt 0)
{
    Write-Host "`nONLY SOURCE (primeros 20):"
    $onlySource | Sort-Object | Select-Object -First 20
}
if ($onlyArchive.Count -gt 0)
{
    Write-Host "`nONLY ARCHIVE (primeros 20):"
    $onlyArchive | Sort-Object | Select-Object -First 20
}

$probe = Get-BinaryAlignmentProbe -SourcePath $sourceBin.FullName -ArchivePath $archiveBin.FullName
Write-Host "`n=== BINARY ALIGNMENT PROBE ==="
Write-Host ("FIRST_DIFF_OFFSET={0}" -f $probe.FirstDiff)
Write-Host ("BEST_LOCAL_SHIFT={0}" -f $probe.BestShift)
Write-Host ("BEST_LOCAL_MATCHES={0}/{1}" -f $probe.Matches, $probe.Compared)

Write-Host "`n=== SIZE REPORTADO ==="
Select-String -LiteralPath $sourceLog,$archiveLog -Pattern "Sketch uses|Global variables use|Using precompiled library.*JWPLC_Display" |
    ForEach-Object { $_.Line }

Write-Host "`nALPHA6_DISPLAY_PARITY_DIAG=DONE" -ForegroundColor Green
