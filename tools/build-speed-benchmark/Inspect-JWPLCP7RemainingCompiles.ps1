#requires -Version 7.4

[CmdletBinding()]
param()

Set-StrictMode -Version 2.0
$ErrorActionPreference = "Stop"

$ScriptRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$RepoRoot = [System.IO.Path]::GetFullPath((Join-Path $ScriptRoot "..\.."))
$LibrariesRoot = Join-Path $RepoRoot "JWPLC\2.1.0\libraries"
$RunRoot = Join-Path $ScriptRoot "p6c2-busio-precompiled-work\20260810_002028"
$BuildPath = Join-Path $RunRoot "p6c2-Basic"
$CompileDb = Join-Path $BuildPath "compile_commands.json"
$LogPath = Join-Path $RunRoot "p6c2-Basic.log"
$ReportPath = Join-Path $ScriptRoot "P7_REMAINING_COMPILES_AUDIT.md"

function Get-PropertyValue
{
    param(
        [Parameter(Mandatory = $true)][string]$Path,
        [Parameter(Mandatory = $true)][string]$Name
    )
    if (-not (Test-Path -LiteralPath $Path)) { return $null }
    foreach ($line in Get-Content -LiteralPath $Path)
    {
        if ($line -match ('^' + [regex]::Escape($Name) + '=(?<value>.*)$'))
        {
            return $Matches["value"].Trim()
        }
    }
    return $null
}

function Get-PreprocessCount
{
    param([Parameter(Mandatory = $true)][string]$Path)
    return @(Get-Content -LiteralPath $Path | Where-Object { ([string]$_) -match 'xtensa-esp32-elf-g\+\+.*\s-E\s' }).Count
}

function Get-Category
{
    param([Parameter(Mandatory = $true)][string]$Source)

    if ($Source -match '[\\/]libraries[\\/](?<lib>[^\\/]+)[\\/]')
    {
        return [PSCustomObject]@{ Type = "library"; Name = $Matches["lib"] }
    }
    if ($Source -match '[\\/]cores[\\/](?<core>[^\\/]+)[\\/]')
    {
        return [PSCustomObject]@{ Type = "core"; Name = $Matches["core"] }
    }
    if ($Source -match '[\\/]variants[\\/](?<variant>[^\\/]+)[\\/]')
    {
        return [PSCustomObject]@{ Type = "variant"; Name = $Matches["variant"] }
    }
    if ($Source -match '[\\/]sketch[\\/]' -or $Source -match '\.ino\.cpp$')
    {
        return [PSCustomObject]@{ Type = "sketch"; Name = "sketch" }
    }
    return [PSCustomObject]@{ Type = "other"; Name = "other" }
}

if (-not (Test-Path -LiteralPath $CompileDb)) { throw "Falta compile_commands.json P6C-2: $CompileDb" }
if (-not (Test-Path -LiteralPath $LogPath)) { throw "Falta log P6C-2: $LogPath" }

$entriesRaw = Get-Content -LiteralPath $CompileDb -Raw | ConvertFrom-Json
$entries = @($entriesRaw)
if ($entries.Count -ne 12) { throw ("Se esperaban 12 compiles en P6 final; actual={0}." -f $entries.Count) }
$preprocess = Get-PreprocessCount -Path $LogPath
if ($preprocess -ne 29) { throw ("Se esperaban 29 invocaciones g++ -E; actual={0}." -f $preprocess) }

$rows = New-Object System.Collections.Generic.List[object]
foreach ($entry in $entries)
{
    $source = [string]$entry.file
    $cat = Get-Category -Source $source
    $leaf = Split-Path -Leaf $source
    $relative = $source
    if ($source.StartsWith($RepoRoot, [StringComparison]::OrdinalIgnoreCase))
    {
        $relative = $source.Substring($RepoRoot.Length).TrimStart('\','/')
    }

    [void]$rows.Add([PSCustomObject]@{
        Type = $cat.Type
        Group = $cat.Name
        Source = $relative
        Leaf = $leaf
    })
}

Write-Host "JWPLC - auditoria P7 de compiles restantes / sin compilar" -ForegroundColor Cyan
Write-Host ("P6 final: {0} compiles | g++ -E={1}" -f $entries.Count, $preprocess) -ForegroundColor Green
Write-Host ""

$groups = @($rows | Group-Object Type, Group | Sort-Object Name)
foreach ($group in $groups)
{
    Write-Host ("[{0}] {1} TU(s)" -f $group.Name, $group.Count) -ForegroundColor Yellow
    foreach ($row in @($group.Group | Sort-Object Source))
    {
        Write-Host ("  - {0}" -f $row.Source)
    }
    Write-Host ""
}

$libraryRows = @($rows | Where-Object Type -eq "library")
$libraryGroups = @($libraryRows | Group-Object Group | Sort-Object Count -Descending, Name)

Write-Host "=== LIBRERIAS QUE AUN COMPILAN DESDE FUENTE ===" -ForegroundColor Cyan
$reportLines = New-Object System.Collections.Generic.List[string]
[void]$reportLines.Add("# P7 - Auditoria de compiles restantes tras P6")
[void]$reportLines.Add("")
[void]$reportLines.Add(("P6 final: **{0} compiles**, **{1} invocaciones `g++ -E`**." -f $entries.Count, $preprocess))
[void]$reportLines.Add("")
[void]$reportLines.Add("## Unidades de traduccion restantes")
[void]$reportLines.Add("")
[void]$reportLines.Add("| Tipo | Grupo | Fuente |")
[void]$reportLines.Add("|---|---|---|")
foreach ($row in @($rows | Sort-Object Type, Group, Source))
{
    [void]$reportLines.Add(("| {0} | {1} | `{2}` |" -f $row.Type, $row.Group, $row.Source.Replace('|','\|')))
}

[void]$reportLines.Add("")
[void]$reportLines.Add("## Librerias que aun compilan desde fuente")
[void]$reportLines.Add("")
[void]$reportLines.Add("| Libreria | TUs actuales | precompiled | Fuentes distribuidas | Lectura |")
[void]$reportLines.Add("|---|---:|---|---:|---|")

foreach ($libGroup in $libraryGroups)
{
    $libName = [string]$libGroup.Name
    $libRoot = Join-Path $LibrariesRoot $libName
    $props = Join-Path $libRoot "library.properties"
    $precompiled = [string](Get-PropertyValue -Path $props -Name "precompiled")
    if ([string]::IsNullOrWhiteSpace($precompiled)) { $precompiled = "ausente" }

    $distributedSources = 0
    if (Test-Path -LiteralPath $libRoot)
    {
        $distributedSources = @(Get-ChildItem -LiteralPath $libRoot -Recurse -File |
            Where-Object { $_.Extension -in @('.c','.cc','.cpp','.S') -and $_.FullName -notmatch '[\\/]examples[\\/]' }).Count
    }

    $reading = if ($precompiled -eq "full") { "Ya marcada precompiled; revisar por que aun compila." } else { "Candidata a evaluar; no implica que convenga precompilarla." }
    Write-Host ("{0}: TUs={1} | precompiled={2} | fuentes distribuidas={3}" -f $libName, $libGroup.Count, $precompiled, $distributedSources) -ForegroundColor Green
    [void]$reportLines.Add(("| {0} | {1} | {2} | {3} | {4} |" -f $libName, $libGroup.Count, $precompiled, $distributedSources, $reading))
}

$nonLibraryCount = @($rows | Where-Object Type -ne "library").Count
Write-Host ""
Write-Host ("Resumen: librerias fuente={0} TU(s) | no-libreria={1} TU(s) | total={2}" -f $libraryRows.Count, $nonLibraryCount, $entries.Count) -ForegroundColor Green
Write-Host "No se ejecuto ninguna compilacion." -ForegroundColor DarkGray

[void]$reportLines.Add("")
[void]$reportLines.Add("## Nota")
[void]$reportLines.Add("")
[void]$reportLines.Add("Este inventario identifica candidatos. La decision de precompilar debe hacerse por biblioteca, con gate estructural y prueba funcional cuando corresponda; no se debe asumir que reducir un TU siempre mejora el tiempo total.")
$reportLines | Set-Content -LiteralPath $ReportPath -Encoding utf8NoBOM
Write-Host ("Reporte: {0}" -f $ReportPath) -ForegroundColor DarkGray
