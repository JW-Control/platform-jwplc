[CmdletBinding()]
param(
    [string]$PackageNamespace = "jwplc_local",
    [string[]]$Libraries = @(),
    [string]$NmPath = "",
    [string]$OutputPath = "",
    [switch]$AllowGenericGpioBridge
)

Set-StrictMode -Version 2.0
$ErrorActionPreference = "Stop"

$ScriptRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$RepoRoot = [System.IO.Path]::GetFullPath((Join-Path $ScriptRoot "..\.."))
$PlatformRoot = Join-Path $RepoRoot "JWPLC\2.1.0"
$LibraryRoot = Join-Path $PlatformRoot "libraries"
$GenericBridgePath = Join-Path $PlatformRoot "cores\esp32\jwplc-gpio-compat.c"
$BridgeCompatibleSymbols = @(
    "jwplc_pinMode",
    "jwplc_digitalWrite",
    "jwplc_digitalRead"
)

if ([string]::IsNullOrWhiteSpace($OutputPath))
{
    $OutputPath = Join-Path $ScriptRoot "PRECOMPILED_LIBRARY_AUDIT.md"
}

if ($AllowGenericGpioBridge.IsPresent -and -not (Test-Path -LiteralPath $GenericBridgePath))
{
    throw "Se solicito AllowGenericGpioBridge pero no existe el bridge esperado: $GenericBridgePath"
}

$bridgeEnabled = $AllowGenericGpioBridge.IsPresent

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
Write-Host ("Modo bridge GPIO generico: {0}" -f $(if ($bridgeEnabled) { "HABILITADO" } else { "ESTRICTO" }))
Write-Host ("Archives encontrados: {0}" -f $archives.Count)
Write-Host ""

$rows = New-Object System.Collections.Generic.List[object]
$blockingFindings = New-Object System.Collections.Generic.List[string]
$bridgeFindings = New-Object System.Collections.Generic.List[string]

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

    $bridgeJwplc = @()
    $blockingJwplc = @()

    foreach ($symbol in $externalJwplc)
    {
        if ($bridgeEnabled -and ($BridgeCompatibleSymbols -contains $symbol))
        {
            $bridgeJwplc += $symbol
        }
        else
        {
            $blockingJwplc += $symbol
        }
    }

    $status = if ($blockingJwplc.Count -gt 0) { "FAIL" } elseif ($bridgeJwplc.Count -gt 0) { "BRIDGE" } else { "PASS" }

    if ($blockingJwplc.Count -gt 0)
    {
        $blockingFindings.Add(("{0}/{1}: {2}" -f $library, $archive.Name, ($blockingJwplc -join ", ")))
        Write-Host ("[FAIL] {0}/{1}: {2}" -f $library, $archive.Name, ($blockingJwplc -join ", ")) -ForegroundColor Red
    }
    elseif ($bridgeJwplc.Count -gt 0)
    {
        $bridgeFindings.Add(("{0}/{1}: {2}" -f $library, $archive.Name, ($bridgeJwplc -join ", ")))
        Write-Host ("[BRIDGE] {0}/{1}: {2}" -f $library, $archive.Name, ($bridgeJwplc -join ", ")) -ForegroundColor Yellow
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
        BridgeJwplc       = $bridgeJwplc
        BlockingJwplc     = $blockingJwplc
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
$lines.Add(("Modo bridge GPIO generico: **{0}**" -f $(if ($bridgeEnabled) { "HABILITADO" } else { "ESTRICTO" })))
$lines.Add("")

if ($bridgeEnabled)
{
    $lines.Add('Criterio: se permiten exclusivamente `jwplc_pinMode`, `jwplc_digitalWrite` y `jwplc_digitalRead` como dependencias externas bridge-compatible. El target genérico debe aportar `cores/esp32/jwplc-gpio-compat.c`. Cualquier otro `jwplc_*` externo es bloqueante.')
}
else
{
    $lines.Add('Criterio bloqueante estricto: cualquier archive `libraries/*/src/esp32/lib*.a` reutilizable por targets con `build.mcu=esp32` no debe conservar dependencias externas `jwplc_*`.')
}

$lines.Add("")
$lines.Add('| Libreria | Archive | Undefined | Externos `jwplc_*` | Bridge GPIO | Bloqueantes | Estado |')
$lines.Add("|---|---|---:|---|---|---|---|")

foreach ($row in $rows)
{
    $externalDisplay = if ($row.ExternalJwplc.Count -eq 0) { "-" } else { $row.ExternalJwplc -join ", " }
    $bridgeDisplay = if ($row.BridgeJwplc.Count -eq 0) { "-" } else { $row.BridgeJwplc -join ", " }
    $blockingDisplay = if ($row.BlockingJwplc.Count -eq 0) { "-" } else { $row.BlockingJwplc -join ", " }
    $lines.Add(("| {0} | {1} | {2} | {3} | {4} | {5} | {6} |" -f $row.Library, $row.Archive, $row.UndefinedCount, $externalDisplay, $bridgeDisplay, $blockingDisplay, $row.Status))
}

$lines.Add("")
if ($blockingFindings.Count -eq 0)
{
    if ($bridgeFindings.Count -eq 0)
    {
        $lines.Add('Resultado global: **PASS NEUTRAL**. No se detectaron dependencias externas `jwplc_*` en los archives ESP32 presentes.')
    }
    else
    {
        $lines.Add('Resultado global: **PASS BRIDGE-COMPATIBLE**. No se detectaron dependencias `jwplc_*` bloqueantes.')
        $lines.Add("")
        $lines.Add('Archives que requieren el bridge GPIO genérico:')
        foreach ($finding in $bridgeFindings)
        {
            $lines.Add(("- {0}" -f $finding))
        }
    }
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
$lines.Add('Nota: `nm -u` reporta simbolos indefinidos por miembro del archive. Para evitar falsos positivos, esta auditoria descuenta simbolos `jwplc_*` que el mismo archive define y clasifica sólo dependencias externas reales.')

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
