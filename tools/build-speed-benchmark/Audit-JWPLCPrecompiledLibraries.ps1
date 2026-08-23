[CmdletBinding()]
param(
    [string]$PackageNamespace = "jwplc_local",
    [string[]]$Libraries = @(
        "JW_RTC",
        "JW_FRAM",
        "JW_SD",
        "JWPLC_ModbusRTU"
    ),
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

function Get-LibraryArchivePath
{
    param([string]$LibraryName)

    return Join-Path (Join-Path (Join-Path $LibraryRoot $LibraryName) "src\esp32") ("lib{0}.a" -f $LibraryName)
}

function Get-CoreCoupledSymbols
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

$nm = Resolve-NmTool -ExplicitPath $NmPath -Namespace $PackageNamespace
Write-Host "Auditoria de librerias precompiladas JWPLC" -ForegroundColor Cyan
Write-Host ("nm: {0}" -f $nm) -ForegroundColor DarkGray
Write-Host ("Librerias: {0}" -f ($Libraries -join ", "))
Write-Host ""

$rows = New-Object System.Collections.Generic.List[object]
$blockingFindings = New-Object System.Collections.Generic.List[string]

foreach ($library in $Libraries)
{
    $archivePath = Get-LibraryArchivePath -LibraryName $library

    if (-not (Test-Path -LiteralPath $archivePath))
    {
        $rows.Add([PSCustomObject]@{
            Library        = $library
            Archive        = $archivePath
            Exists         = $false
            UndefinedCount = 0
            CoupledSymbols = @()
            Status         = "MISSING"
        })
        $blockingFindings.Add(("{0}: archive no encontrado" -f $library))
        Write-Host ("[MISSING] {0}" -f $library) -ForegroundColor Yellow
        continue
    }

    $result = Invoke-NativeCaptured -FilePath $nm -Arguments @("-u", "-C", $archivePath)
    if ($result.ExitCode -ne 0)
    {
        throw "nm fallo para $library (exit=$($result.ExitCode))."
    }

    $undefinedLines = @(
        $result.Output | Where-Object { -not [string]::IsNullOrWhiteSpace([string]$_) }
    )
    $coupledSymbols = @(Get-CoreCoupledSymbols -NmOutput $undefinedLines)
    $status = if ($coupledSymbols.Count -eq 0) { "PASS" } else { "FAIL" }

    if ($coupledSymbols.Count -gt 0)
    {
        $blockingFindings.Add(("{0}: {1}" -f $library, ($coupledSymbols -join ", ")))
        Write-Host ("[FAIL] {0}: {1}" -f $library, ($coupledSymbols -join ", ")) -ForegroundColor Red
    }
    else
    {
        Write-Host ("[PASS] {0}: sin simbolos jwplc_* no resueltos" -f $library) -ForegroundColor Green
    }

    $rows.Add([PSCustomObject]@{
        Library        = $library
        Archive        = $archivePath
        Exists         = $true
        UndefinedCount = $undefinedLines.Count
        CoupledSymbols = $coupledSymbols
        Status         = $status
    })
}

$lines = New-Object System.Collections.Generic.List[string]
$lines.Add("# Auditoria de compatibilidad de librerias precompiladas JWPLC")
$lines.Add("")
$lines.Add(("Fecha: {0}" -f (Get-Date).ToString("yyyy-MM-dd HH:mm:ss")))
$lines.Add("")
$lines.Add(('Toolchain nm: `{0}`' -f $nm))
$lines.Add("")
$lines.Add('Criterio bloqueante: un archive `src/esp32/lib*.a` no debe depender de simbolos internos `jwplc_*` si puede ser reutilizado por targets que comparten `build.mcu=esp32` pero usan cores distintos.')
$lines.Add("")
$lines.Add('| Libreria | Archive | Undefined | Simbolos `jwplc_*` | Estado |')
$lines.Add("|---|---|---:|---|---|")

foreach ($row in $rows)
{
    $archiveDisplay = if ($row.Exists) { "presente" } else { "faltante" }
    $symbolsDisplay = if ($row.CoupledSymbols.Count -eq 0) { "-" } else { $row.CoupledSymbols -join ", " }
    $lines.Add(("| {0} | {1} | {2} | {3} | {4} |" -f $row.Library, $archiveDisplay, $row.UndefinedCount, $symbolsDisplay, $row.Status))
}

$lines.Add("")
if ($blockingFindings.Count -eq 0)
{
    $lines.Add('Resultado global: **PASS**. No se detectaron dependencias `jwplc_*` en los archives P1 auditados.')
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
$lines.Add('Nota: otros simbolos indefinidos de Arduino/ESP-IDF son normales en una libreria estatica y se resuelven durante el link final. Esta auditoria se concentra en el acoplamiento interno `jwplc_*` que causo la regresion de `JW_MatrixButtons`.')

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
