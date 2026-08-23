[CmdletBinding()]
param(
    [string]$ArduinoCli = "arduino-cli",
    [string]$Fqbn = "jwplc_local:esp32:jwplcbasic",
    [string]$Sketch = "01_empty",
    [int]$Jobs = 0,
    [string[]]$Libraries = @(
        "JW_RTC",
        "JW_FRAM",
        "JW_SD",
        "JWPLC_ModbusRTU"
    ),
    [string]$OutputRoot = ""
)

Set-StrictMode -Version 2.0
$ErrorActionPreference = "Stop"

$ScriptRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$RepoRoot = [System.IO.Path]::GetFullPath((Join-Path $ScriptRoot "..\.."))
$PlatformRoot = Join-Path $RepoRoot "JWPLC\2.1.0"
$LibraryRoot = Join-Path $PlatformRoot "libraries"
$SketchRoot = Join-Path $ScriptRoot "sketches"

if ([string]::IsNullOrWhiteSpace($OutputRoot))
{
    $OutputRoot = Join-Path $ScriptRoot "precompile-work"
}

function Invoke-NativeCaptured
{
    param(
        [Parameter(Mandatory = $true)][string]$FilePath,
        [Parameter(Mandatory = $true)][string[]]$Arguments
    )

    # Windows PowerShell 5.1 may wrap stderr from native programs as
    # NativeCommandError. Arduino CLI uses stderr for verbose output too,
    # so native success is determined only from LASTEXITCODE.
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
        Output   = $nativeOutput
    }
}

function Invoke-ArduinoCompile
{
    param(
        [Parameter(Mandatory = $true)][string]$BuildPath,
        [Parameter(Mandatory = $true)][string]$LogPath,
        [Parameter(Mandatory = $true)][string]$SketchPath
    )

    if (Test-Path $BuildPath)
    {
        Remove-Item -Path $BuildPath -Recurse -Force
    }
    New-Item -ItemType Directory -Path $BuildPath -Force | Out-Null

    $args = @(
        "compile",
        "-b", $Fqbn,
        "-j", $Jobs.ToString(),
        "-v",
        "--build-path", $BuildPath,
        "--clean",
        $SketchPath
    )

    Write-Host ""
    Write-Host ("arduino-cli {0}" -f ($args -join " ")) -ForegroundColor DarkGray

    $sw = [System.Diagnostics.Stopwatch]::StartNew()
    $result = Invoke-NativeCaptured -FilePath $ArduinoCli -Arguments $args
    $sw.Stop()

    $result.Output | Out-File -FilePath $LogPath -Encoding utf8

    if ($result.ExitCode -ne 0)
    {
        Write-Host ("Compilacion fallo (exit={0})." -f $result.ExitCode) -ForegroundColor Red
        @($result.Output | Select-Object -Last 12) | ForEach-Object { Write-Host $_ -ForegroundColor DarkYellow }
        throw "Arduino CLI fallo. Revisar: $LogPath"
    }

    Write-Host ("Compilacion OK en {0:N3} s" -f $sw.Elapsed.TotalSeconds) -ForegroundColor Green

    return [PSCustomObject]@{
        Output     = @($result.Output)
        DurationMs = [Math]::Round($sw.Elapsed.TotalMilliseconds, 3)
        BuildPath  = $BuildPath
        LogPath    = $LogPath
    }
}

function Resolve-NativeToolPath
{
    param([string]$Candidate)

    if ([string]::IsNullOrWhiteSpace($Candidate))
    {
        return $null
    }

    $normalized = $Candidate.Trim().Trim('"')

    # Arduino CLI verbose output may escape Windows separators as C:\\Users\\...
    # Normalize those sequences back to a filesystem path before Test-Path.
    while ($normalized.Contains("\\"))
    {
        $normalized = $normalized.Replace("\\", "\")
    }

    $candidates = New-Object System.Collections.Generic.List[string]
    $candidates.Add($normalized)

    if (-not [System.IO.Path]::HasExtension($normalized))
    {
        $candidates.Add($normalized + ".exe")
        $candidates.Add($normalized + ".cmd")
        $candidates.Add($normalized + ".bat")
    }

    foreach ($path in $candidates)
    {
        if (Test-Path -LiteralPath $path)
        {
            return (Resolve-Path -LiteralPath $path).Path
        }
    }

    return $null
}

function Find-Archiver
{
    param([string[]]$Output)

    foreach ($line in $Output)
    {
        $text = [string]$line
        $candidate = $null

        if ($text -match '"(?<exe>[^"]*xtensa-esp32-elf-gcc-ar(?:\.exe)?)"')
        {
            $candidate = $Matches["exe"]
        }
        elseif ($text -match '(?<exe>\S*xtensa-esp32-elf-gcc-ar(?:\.exe)?)\s+cr')
        {
            $candidate = $Matches["exe"]
        }

        if (-not [string]::IsNullOrWhiteSpace($candidate))
        {
            $resolved = Resolve-NativeToolPath -Candidate $candidate
            if (-not [string]::IsNullOrWhiteSpace($resolved))
            {
                return $resolved
            }
        }
    }

    throw "No se pudo localizar un xtensa-esp32-elf-gcc-ar existente a partir del log verbose."
}

function Get-LibraryArchivePath
{
    param([string]$LibraryName)

    return Join-Path (Join-Path (Join-Path $LibraryRoot $LibraryName) "src\esp32") ("lib{0}.a" -f $LibraryName)
}

function Assert-PrecompiledFull
{
    param([string]$LibraryName)

    $propertiesPath = Join-Path (Join-Path $LibraryRoot $LibraryName) "library.properties"
    if (-not (Test-Path $propertiesPath))
    {
        throw "No existe library.properties para $LibraryName"
    }

    $content = Get-Content -Path $propertiesPath -Raw
    if ($content -notmatch '(?m)^precompiled=full\s*$')
    {
        throw "$LibraryName no declara precompiled=full en library.properties. Ejecuta git pull antes de P1."
    }
}

if ($Jobs -lt 0)
{
    throw "Jobs debe ser 0 o mayor."
}

if ($Fqbn -notmatch ':esp32:jwplcbasic(?:core)?$')
{
    throw "P1 esta limitado por ahora a JWPLC Basic/Basic Core ESP32. FQBN recibido: $Fqbn"
}

$cliCommand = Get-Command $ArduinoCli -ErrorAction SilentlyContinue
if ($null -eq $cliCommand)
{
    throw "No se encontro '$ArduinoCli' en PATH."
}

$sketchPath = Join-Path $SketchRoot $Sketch
if (-not (Test-Path $sketchPath))
{
    throw "Sketch no encontrado: $sketchPath"
}

foreach ($library in $Libraries)
{
    Assert-PrecompiledFull -LibraryName $library
}

$runId = (Get-Date).ToString("yyyyMMdd_HHmmss")
$runRoot = Join-Path $OutputRoot $runId
$sourceBuild = Join-Path $runRoot "source-build"
$verifyBuild = Join-Path $runRoot "verify-build"
$sourceLog = Join-Path $runRoot "source-build.log"
$verifyLog = Join-Path $runRoot "verify-build.log"
$summaryPath = Join-Path $runRoot "P1_SUMMARY.md"

New-Item -ItemType Directory -Path $runRoot -Force | Out-Null

Write-Host "P1 - Generacion de librerias JWPLC precompiladas" -ForegroundColor Cyan
Write-Host ("FQBN: {0}" -f $Fqbn)
Write-Host ("Sketch fuente: {0}" -f $Sketch)
Write-Host ("Librerias: {0}" -f ($Libraries -join ", "))

# To regenerate reproducibly, remove only the P1 archives selected here.
# With precompiled=full and no compatible archive, Arduino falls back to
# compiling the library sources.
foreach ($library in $Libraries)
{
    $archivePath = Get-LibraryArchivePath -LibraryName $library
    if (Test-Path $archivePath)
    {
        Remove-Item -Path $archivePath -Force
    }
}

Write-Host ""
Write-Host "[1/3] Build fuente limpio para obtener objetos reales de Arduino CLI..." -ForegroundColor Cyan
$sourceResult = Invoke-ArduinoCompile -BuildPath $sourceBuild -LogPath $sourceLog -SketchPath $sketchPath
$archiver = Find-Archiver -Output $sourceResult.Output

Write-Host ("Archiver: {0}" -f $archiver) -ForegroundColor DarkGray

$generated = @()

Write-Host ""
Write-Host "[2/3] Creando archivos .a P1..." -ForegroundColor Cyan

foreach ($library in $Libraries)
{
    $objectRoot = Join-Path (Join-Path $sourceBuild "libraries") $library
    if (-not (Test-Path $objectRoot))
    {
        throw "No se encontro el arbol de objetos para $library en $objectRoot"
    }

    $objects = @(Get-ChildItem -Path $objectRoot -Recurse -File -Filter "*.o" | Sort-Object FullName)
    if ($objects.Count -eq 0)
    {
        throw "No se encontraron objetos .o para $library"
    }

    $archivePath = Get-LibraryArchivePath -LibraryName $library
    $archiveDir = Split-Path -Parent $archivePath
    New-Item -ItemType Directory -Path $archiveDir -Force | Out-Null

    if (Test-Path $archivePath)
    {
        Remove-Item -Path $archivePath -Force
    }

    $archiveArgs = @("crs", $archivePath) + @($objects | ForEach-Object { $_.FullName })
    $archiveResult = Invoke-NativeCaptured -FilePath $archiver -Arguments $archiveArgs

    if ($archiveResult.ExitCode -ne 0)
    {
        throw "gcc-ar fallo al generar $archivePath"
    }

    if (-not (Test-Path $archivePath))
    {
        throw "No se genero el archivo esperado: $archivePath"
    }

    $file = Get-Item $archivePath
    $sha = (Get-FileHash -Path $archivePath -Algorithm SHA256).Hash.ToLowerInvariant()

    $generated += [PSCustomObject]@{
        Library = $library
        Objects = $objects.Count
        Bytes   = [int64]$file.Length
        SHA256  = $sha
        Archive = $archivePath
    }

    Write-Host ("  {0}: {1} objeto(s), {2} bytes" -f $library, $objects.Count, $file.Length) -ForegroundColor Green
}

Write-Host ""
Write-Host "[3/3] Verificacion limpia usando precompiled=full..." -ForegroundColor Cyan
$verifyResult = Invoke-ArduinoCompile -BuildPath $verifyBuild -LogPath $verifyLog -SketchPath $sketchPath

$allCompilerInvocations = @(
    $verifyResult.Output | Where-Object { ([string]$_) -match '-MMD\s+-c\s' }
).Count

$violations = New-Object System.Collections.Generic.List[string]
foreach ($library in $Libraries)
{
    $escaped = [regex]::Escape($library)
    $compiledFromSource = @(
        $verifyResult.Output | Where-Object {
            $line = [string]$_
            ($line -match '-MMD\s+-c\s') -and
            ($line -match ("[\\/]libraries[\\/]" + $escaped + "[\\/]"))
        }
    ).Count

    if ($compiledFromSource -gt 0)
    {
        $violations.Add(("{0}: {1} compilacion(es) desde fuente" -f $library, $compiledFromSource))
    }
}

$lines = New-Object System.Collections.Generic.List[string]
$lines.Add("# P1 - generacion de librerias precompiladas")
$lines.Add("")
$lines.Add(("Run: {0}" -f $runId))
$lines.Add("")
$lines.Add(("FQBN: {0}" -f $Fqbn))
$lines.Add("")
$lines.Add(("Build fuente: {0:N3} s" -f ($sourceResult.DurationMs / 1000.0)))
$lines.Add(("Build verificacion: {0:N3} s" -f ($verifyResult.DurationMs / 1000.0)))
$lines.Add(("Compilaciones reales en verificacion: {0}" -f $allCompilerInvocations))
$lines.Add("")
$lines.Add("| Libreria | Objetos archivados | Bytes | SHA-256 |")
$lines.Add("|---|---:|---:|---|")
foreach ($row in $generated)
{
    $lines.Add(("| {0} | {1} | {2} | {3} |" -f $row.Library, $row.Objects, $row.Bytes, $row.SHA256))
}
$lines.Add("")

if ($violations.Count -eq 0)
{
    $lines.Add("Resultado: OK. Ninguna libreria P1 seleccionada se recompilo desde fuente durante la verificacion.")
}
else
{
    $lines.Add("Resultado: REVISAR. Arduino todavia recompilo librerias P1 desde fuente:")
    $lines.Add("")
    foreach ($violation in $violations)
    {
        $lines.Add(("- {0}" -f $violation))
    }
}

$lines.Add("")
$lines.Add("Los archivos .a quedan dentro de JWPLC/2.1.0/libraries/<lib>/src/esp32/ para el benchmark P1.")
$lines.Add("No hacer merge ni publicar estos binarios hasta validar Basic, Basic Core, tamanos y hardware.")
$lines | Out-File -FilePath $summaryPath -Encoding ascii

Write-Host ""
if ($violations.Count -eq 0)
{
    Write-Host "P1 generado y verificado correctamente." -ForegroundColor Green
}
else
{
    Write-Host "P1 genero archivos, pero hay recompilaciones desde fuente que debemos revisar." -ForegroundColor Yellow
}

Write-Host ("Resumen: {0}" -f $summaryPath)
Write-Host ""
Write-Host "Siguiente benchmark recomendado:" -ForegroundColor Cyan
Write-Host ".\Run-JWPLCBuildBenchmark.ps1 -PackageNamespace jwplc_local -Targets Basic -Sketches 01_empty -RunLabel alpha4-precompile-p1 -SkipExplicitBuild -SkipUploads"
Write-Host ""
Write-Host "Importante: los .a generados quedan sin commit hasta revisar los resultados P1." -ForegroundColor Yellow
