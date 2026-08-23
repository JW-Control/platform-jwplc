[CmdletBinding()]
param(
    [string]$PackageNamespace = "jwplc_local",
    [string[]]$Libraries = @(),
    [string]$NmPath = "",
    [string]$OutputPath = ""
)

Set-StrictMode -Version 2.0
$ErrorActionPreference = "Stop"

$ScriptRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$RepoRoot = [System.IO.Path]::GetFullPath((Join-Path $ScriptRoot "..\.."))
$PlatformRoot = Join-Path $RepoRoot "JWPLC\2.1.0"
$LibraryRoot = Join-Path $PlatformRoot "libraries"

if ([string]::IsNullOrWhiteSpace($OutputPath))
{
    $OutputPath = Join-Path $ScriptRoot "PRECOMPILED_LIBRARY_AUDIT.md"
}

function Resolve-NmTool
{
    param(
        [string]$ExplicitPath,
        [string]$Namespace
    )

    if (-not [string]::IsNullOrWhiteSpace($ExplicitPath))
    {
        $candidate = $ExplicitPath.Trim().Trim('"')
        if (-not (Test-Path -LiteralPath $candidate))
        {
            throw "No existe NmPath: $candidate"
        }
        return (Resolve-Path -LiteralPath $candidate).Path
    }

    $command = Get-Command "xtensa-esp32-elf-nm" -ErrorAction SilentlyContinue
    if ($null -ne $command)
    {
        return $command.Source
    }

    if ([string]::IsNullOrWhiteSpace($env:LOCALAPPDATA))
    {
        throw "LOCALAPPDATA no esta definido y no se pudo localizar xtensa-esp32-elf-nm en PATH."
    }

    $toolRoot = Join-Path $env:LOCALAPPDATA ("Arduino15\packages\{0}\tools\esp-x32" -f $Namespace)
    if (-not (Test-Path -LiteralPath $toolRoot))
    {
        throw "No existe el arbol de toolchain esperado: $toolRoot"
    }

    $candidates = @(
        Get-ChildItem -Path $toolRoot -Recurse -File -ErrorAction SilentlyContinue |
            Where-Object { $_.Name -ieq "xtensa-esp32-elf-nm.exe" -or $_.Name -ieq "xtensa-esp32-elf-nm" } |
            Sort-Object FullName -Descending
    )

    if ($candidates.Count -eq 0)
    {
        throw "No se encontro xtensa-esp32-elf-nm dentro de $toolRoot"
    }

    return $candidates[0].FullName
}

function Invoke-NativeCaptured
{
    param(
        [Parameter(Mandatory = $true)][string]$FilePath,
        [Parameter(Mandatory = $true)][string[]]$Arguments
    )

    $previousPreference = $ErrorActionPreference
    $nativeOutput = @()
    $exitCode = -1

    try
    {
        $ErrorActionPreference = "Continue"
        $nativeOutput = @(& $FilePath @Arguments 2>&1 | ForEach-Object { $_.ToString() })
        $exitCode = $LASTEXITCODE
    }
    finally
    {
        $ErrorActionPreference = $previousPreference
    }

    return [PSCustomObject]@{
        ExitCode = [int]$exitCode
        Output   = @($nativeOutput)
    }
}

function Get-JwplcSymbols
{
    param([string[]]$NmOutput)

    $symbols = New-Object System.Collections.Generic.HashSet[string]

    foreach ($line in $NmOutput)
    {
        $text = [string]$line
        $matches = [regex]::Matches($text, '\bjwplc_[A-Za-z0-9_]+\b')
        foreach ($match in $matches)
        {
            [void]$symbols.Add($match.Value)
        }
    }

    return @($symbols | Sort-Object)
}

function Get-ArchiveTargets
{
    $allArchives = @(
        Get-ChildItem -Path $LibraryRoot -Recurse -File -Filter "lib*.a" -ErrorAction SilentlyContinue |
            Where-Object {
                $_.Directory.Name -ieq "esp32" -and
                $null -ne $_.Directory.Parent -and
                $_.Directory.Parent.Name -ieq "src" -and
                $null -ne $_.Directory.Parent.Parent
            } |
            Sort-Object FullName
    )

    if ($Libraries.Count -eq 0)
    {
        return @($allArchives)
    }

    $requested = New-Object System.Collections.Generic.HashSet[string]([System.StringComparer]::OrdinalIgnoreCase)
    foreach ($library in $Libraries)
    {
        if (-not [string]::IsNullOrWhiteSpace($library))
        {
            [void]$requested.Add($library)
        }
    }

    return @(
        $allArchives | Where-Object {
            $requested.Contains($_.Directory.Parent.Parent.Name)
        }
    )
}

$nm = Resolve-NmTool -ExplicitPath $NmPath -Namespace $PackageNamespace
$archives = @(Get-ArchiveTargets)

if ($archives.Count -eq 0)
{
    throw "No se encontraron archives lib*.a bajo libraries/*/src/esp32 para auditar."
}

Write-Host "Auditoria global de librerias precompiladas JWPLC" -ForegroundColor Cyan
Write-Host ("nm: {0}" -f $nm) -ForegroundColor DarkGray
Write-Host ("Archives encontrados: {0}" -f $archives.Count)
Write-Host ""

$rows = New-Object System.Collections.Generic.List[object]
$blockingFindings = New-Object System.Collections.Generic.List[string]

foreach ($archive in $archives)
{
    $library = $archive.Directory.Parent.Parent.Name

    $undefinedResult = Invoke-NativeCaptured -FilePath $nm -Arguments @("-u", "-C", $archive.FullName)
    if ($undefinedResult.ExitCode -ne 0)
    {
        throw "nm -u fallo para $library/$($archive.Name) (exit=$($undefinedResult.ExitCode))."
    }

    $definedResult = Invoke-NativeCaptured -FilePath $nm -Arguments @("--defined-only", "-C", $archive.FullName)
    if ($definedResult.ExitCode -ne 0)
    {
        throw "nm --defined-only fallo para $library/$($archive.Name) (exit=$($definedResult.ExitCode))."
    }

    $undefinedLines = @(
        $undefinedResult.Output | Where-Object { -not [string]::IsNullOrWhiteSpace([string]$_) }
    )
    $undefinedJwplc = @(Get-JwplcSymbols -NmOutput $undefinedLines)
    $definedJwplc = @(Get-JwplcSymbols -NmOutput $definedResult.Output)

    $definedSet = New-Object System.Collections.Generic.HashSet[string]([System.StringComparer]::Ordinal)
    foreach ($symbol in $definedJwplc)
    {
        [void]$definedSet.Add($symbol)
    }

    $externalJwplc = @(
        $undefinedJwplc | Where-Object { -not $definedSet.Contains($_) }
    )

    $status = if ($externalJwplc.Count -eq 0) { "PASS" } else { "FAIL" }

    if ($externalJwplc.Count -gt 0)
    {
        $blockingFindings.Add(("{0}/{1}: {2}" -f $library, $archive.Name, ($externalJwplc -join ", ")))
        Write-Host ("[FAIL] {0}/{1}: {2}" -f $library, $archive.Name, ($externalJwplc -join ", ")) -ForegroundColor Red
    }
    else
    {
        Write-Host ("[PASS] {0}/{1}" -f $library, $archive.Name) -ForegroundColor Green
    }

    $rows.Add([PSCustomObject]@{
        Library           = $library
        Archive           = $archive.Name
        UndefinedCount    = $undefinedLines.Count
        DefinedJwplc      = $definedJwplc
        ExternalJwplc     = $externalJwplc
        Status            = $status
    })
}

$lines = New-Object System.Collections.Generic.List[string]
$lines.Add("# Auditoria global de compatibilidad de librerias precompiladas JWPLC")
$lines.Add("")
$lines.Add(("Fecha: {0}" -f (Get-Date).ToString("yyyy-MM-dd HH:mm:ss")))
$lines.Add("")
$lines.Add(('Toolchain nm: `{0}`' -f $nm))
$lines.Add("")
$lines.Add(("Archives auditados: {0}" -f $rows.Count))
$lines.Add("")
$lines.Add('Criterio bloqueante: cualquier archive `libraries/*/src/esp32/lib*.a` reutilizable por targets con `build.mcu=esp32` no debe conservar dependencias externas `jwplc_*` generadas por los remapeos del core JWPLC.')
$lines.Add("")
$lines.Add('| Libreria | Archive | Undefined | Dependencias externas `jwplc_*` | Estado |')
$lines.Add("|---|---|---:|---|---|")

foreach ($row in $rows)
{
    $symbolsDisplay = if ($row.ExternalJwplc.Count -eq 0) { "-" } else { $row.ExternalJwplc -join ", " }
    $lines.Add(("| {0} | {1} | {2} | {3} | {4} |" -f $row.Library, $row.Archive, $row.UndefinedCount, $symbolsDisplay, $row.Status))
}

$lines.Add("")
if ($blockingFindings.Count -eq 0)
{
    $lines.Add('Resultado global: **PASS**. No se detectaron dependencias externas `jwplc_*` en los archives ESP32 presentes.')
}
else
{
    $lines.Add("Resultado global: **FAIL / REVISAR**.")
    $lines.Add("")
    foreach ($finding in $blockingFindings)
    {
        $lines.Add(("- {0}" -f $finding))
    }
}

$lines.Add("")
$lines.Add('Nota: `nm -u` reporta simbolos indefinidos por miembro del archive. Para evitar falsos positivos, esta auditoria descuenta simbolos `jwplc_*` que el mismo archive define y bloquea solo dependencias externas reales.')

$parent = Split-Path -Parent $OutputPath
if (-not [string]::IsNullOrWhiteSpace($parent) -and -not (Test-Path -LiteralPath $parent))
{
    New-Item -ItemType Directory -Path $parent -Force | Out-Null
}

$lines | Out-File -FilePath $OutputPath -Encoding utf8
Write-Host ""
Write-Host ("Informe: {0}" -f $OutputPath)

if ($blockingFindings.Count -gt 0)
{
    exit 2
}

exit 0
