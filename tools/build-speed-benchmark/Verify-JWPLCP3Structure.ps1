[CmdletBinding()]
param(
    [string]$ReferenceBuildPath = "",
    [string]$CandidateBuildPath = ""
)

Set-StrictMode -Version 2.0
$ErrorActionPreference = "Stop"

$ScriptRoot = Split-Path -Parent $MyInvocation.MyCommand.Path

function Find-LatestBuild
{
    param([string]$RootName, [string]$BuildName)

    $root = Join-Path $ScriptRoot $RootName
    if (-not (Test-Path $root)) { return "" }

    foreach ($run in @(Get-ChildItem $root -Directory | Sort-Object LastWriteTime -Descending))
    {
        $build = Join-Path $run.FullName $BuildName
        if ((Test-Path $build) -and (Test-Path (Join-Path $build "01_empty.ino.map")))
        {
            return $build
        }
    }
    return ""
}

function Get-AppBin
{
    param([string]$BuildPath)
    $bin = Get-ChildItem $BuildPath -Filter "*.ino.bin" -File | Select-Object -First 1
    if ($null -eq $bin) { throw "No se encontro .ino.bin en $BuildPath" }
    return $bin
}

function Get-RunLog
{
    param([string]$BuildPath)
    $runDir = Split-Path -Parent $BuildPath
    $name = Split-Path -Leaf $BuildPath
    $candidate = Join-Path $runDir ($name + ".log")
    if (Test-Path $candidate) { return $candidate }

    $fallback = Get-ChildItem $runDir -Filter "*.log" -File | Select-Object -First 1
    if ($null -eq $fallback) { return "" }
    return $fallback.FullName
}

function Get-LibrarySelections
{
    param([string]$LogPath)
    if ([string]::IsNullOrWhiteSpace($LogPath) -or -not (Test-Path $LogPath)) { return @() }

    return @(
        Get-Content $LogPath |
        Where-Object { $_ -like "Using library *" } |
        ForEach-Object { $_.Trim() } |
        Sort-Object -Unique
    )
}

function Get-ObjectTable
{
    param([string]$BuildPath, [bool]$ExcludeDisplay)

    $root = Join-Path $BuildPath "libraries"
    $result = @{}
    if (-not (Test-Path $root)) { return $result }

    foreach ($file in Get-ChildItem $root -Recurse -File -Filter "*.o")
    {
        $relative = $file.FullName.Substring($root.Length).TrimStart([char[]]"\/")
        if ($ExcludeDisplay -and $relative -match '^JWPLC_Display[\\/]') { continue }
        $result[$relative] = (Get-FileHash $file.FullName -Algorithm SHA256).Hash.ToLowerInvariant()
    }
    return $result
}

if ([string]::IsNullOrWhiteSpace($ReferenceBuildPath))
{
    $ReferenceBuildPath = Find-LatestBuild -RootName "p2-verify-work" -BuildName "verify-Basic"
}
if ([string]::IsNullOrWhiteSpace($CandidateBuildPath))
{
    $CandidateBuildPath = Find-LatestBuild -RootName "p3-work" -BuildName "verify-Basic"
}

if ([string]::IsNullOrWhiteSpace($ReferenceBuildPath) -or -not (Test-Path $ReferenceBuildPath))
{
    throw "No se encontro build P2 de referencia. Usa -ReferenceBuildPath."
}
if ([string]::IsNullOrWhiteSpace($CandidateBuildPath) -or -not (Test-Path $CandidateBuildPath))
{
    throw "No se encontro build P3 candidato. Usa -CandidateBuildPath."
}

$ReferenceBuildPath = (Resolve-Path $ReferenceBuildPath).Path
$CandidateBuildPath = (Resolve-Path $CandidateBuildPath).Path

$refBin = Get-AppBin $ReferenceBuildPath
$candBin = Get-AppBin $CandidateBuildPath
$candMapPath = Join-Path $CandidateBuildPath "01_empty.ino.map"
$candMap = Get-Content $candMapPath -Raw

$displayCppLinked = $candMap -match 'libJWPLC_Display\.a\(JWPLC_Display\.cpp\.o\)'
$idleCppLinked = $candMap -match 'libJWPLC_Display\.a\(JWPLC_IdleScreen\.cpp\.o\)'
$keySymbols = @(
    "jwplcDisplayDesiredPeriod_ms",
    "jwplcDisplayBeginCallback",
    "jwplcDisplayRefreshCallback"
)
$missingSymbols = @($keySymbols | Where-Object { $candMap -notmatch [regex]::Escape($_) })

$refObjects = Get-ObjectTable -BuildPath $ReferenceBuildPath -ExcludeDisplay $true
$candObjects = Get-ObjectTable -BuildPath $CandidateBuildPath -ExcludeDisplay $true
$commonKeys = @($refObjects.Keys | Where-Object { $candObjects.ContainsKey($_) })
$hashMismatches = @($commonKeys | Where-Object { $refObjects[$_] -ne $candObjects[$_] })
$onlyReference = @($refObjects.Keys | Where-Object { -not $candObjects.ContainsKey($_) })
$onlyCandidate = @($candObjects.Keys | Where-Object { -not $refObjects.ContainsKey($_) })

$refLog = Get-RunLog $ReferenceBuildPath
$candLog = Get-RunLog $CandidateBuildPath
$refSelections = Get-LibrarySelections $refLog
$candSelections = Get-LibrarySelections $candLog
$selectionOnlyRef = @($refSelections | Where-Object { $candSelections -notcontains $_ })
$selectionOnlyCand = @($candSelections | Where-Object { $refSelections -notcontains $_ })

$sameAppBytes = ($refBin.Length -eq $candBin.Length)

Write-Host "P3 - verificacion estructural sin recompilar" -ForegroundColor Cyan
Write-Host ("Referencia: {0}" -f $ReferenceBuildPath)
Write-Host ("Candidato:  {0}" -f $CandidateBuildPath)
Write-Host ""
Write-Host ("App bytes: {0} -> {1} | iguales={2}" -f $refBin.Length, $candBin.Length, $sameAppBytes)
Write-Host ("Archive members: Display={0}, IdleScreen={1}" -f $displayCppLinked, $idleCppLinked)
Write-Host ("Objetos comunes fuera de Display: {0}" -f $commonKeys.Count)
Write-Host ("SHA distintos en objetos comunes: {0}" -f $hashMismatches.Count)
Write-Host ("Solo referencia: {0} | solo candidato: {1}" -f $onlyReference.Count, $onlyCandidate.Count)
Write-Host ("Simbolos runtime faltantes: {0}" -f $missingSymbols.Count)
Write-Host ("Diferencias de seleccion de librerias: ref={0}, cand={1}" -f $selectionOnlyRef.Count, $selectionOnlyCand.Count)

if ($hashMismatches.Count -gt 0)
{
    Write-Host "Objetos con SHA distinto:" -ForegroundColor Yellow
    $hashMismatches | ForEach-Object { Write-Host ("  {0}" -f $_) }
}
if ($onlyReference.Count -gt 0)
{
    Write-Host "Objetos solo en referencia:" -ForegroundColor Yellow
    $onlyReference | ForEach-Object { Write-Host ("  {0}" -f $_) }
}
if ($onlyCandidate.Count -gt 0)
{
    Write-Host "Objetos solo en candidato:" -ForegroundColor Yellow
    $onlyCandidate | ForEach-Object { Write-Host ("  {0}" -f $_) }
}
if ($missingSymbols.Count -gt 0)
{
    Write-Host "Simbolos faltantes:" -ForegroundColor Yellow
    $missingSymbols | ForEach-Object { Write-Host ("  {0}" -f $_) }
}
if ($selectionOnlyRef.Count -gt 0 -or $selectionOnlyCand.Count -gt 0)
{
    Write-Host "Diferencias de librerias:" -ForegroundColor Yellow
    $selectionOnlyRef | ForEach-Object { Write-Host ("  - REF:  {0}" -f $_) }
    $selectionOnlyCand | ForEach-Object { Write-Host ("  + CAND: {0}" -f $_) }
}

# JWPLC_Display desaparece como .o suelto en P3 por diseno. Fuera de esa
# libreria, el grafo de objetos debe permanecer igual.
$ok = $sameAppBytes -and
      $displayCppLinked -and
      $idleCppLinked -and
      ($hashMismatches.Count -eq 0) -and
      ($onlyReference.Count -eq 0) -and
      ($onlyCandidate.Count -eq 0) -and
      ($missingSymbols.Count -eq 0) -and
      ($selectionOnlyRef.Count -eq 0) -and
      ($selectionOnlyCand.Count -eq 0)

Write-Host ""
if ($ok)
{
    Write-Host "P3 ESTRUCTURALMENTE EQUIVALENTE: OK" -ForegroundColor Green
    Write-Host "La igualdad raw del .bin no se exige porque el archive cambia el layout de link." -ForegroundColor DarkGray
    exit 0
}

Write-Host "P3 ESTRUCTURALMENTE EQUIVALENTE: REVISAR" -ForegroundColor Red
exit 2
