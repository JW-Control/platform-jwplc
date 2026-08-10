#requires -Version 7.4

[CmdletBinding()]
param(
    [string]$ArduinoCli = "arduino-cli",
    [int]$Jobs = 0,
    [switch]$RunCold
)

Set-StrictMode -Version 2.0
$ErrorActionPreference = "Stop"

$ScriptRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$RepoRoot = [System.IO.Path]::GetFullPath((Join-Path $ScriptRoot "..\.."))
$PlatformRoot = Join-Path $RepoRoot "JWPLC\2.1.0"
$SketchPath = Join-Path $ScriptRoot "sketches\01_empty"
$InspectorPath = Join-Path $ScriptRoot "Inspect-JWPLCP5AExistingRun.ps1"
$OutputRoot = Join-Path $ScriptRoot "p5a-ethernet-work"
$EthernetRoot = Join-Path $PlatformRoot "libraries\JWPLC_Ethernet_W5x00_Backend"
$EthernetArchivePath = Join-Path $EthernetRoot "src\esp32\libJWPLC_Ethernet_W5x00_Backend.a"
$DisplayArchivePath = Join-Path $PlatformRoot "libraries\JWPLC_Display\src\esp32\libJWPLC_Display.a"
$CoreArchivePath = Join-Path $PlatformRoot "precompiled\core\JWPLCBASIC\core.a"
$BoardsLocalPath = Join-Path $PlatformRoot "boards.local.txt"

function ConvertTo-EntryList
{
    param([Parameter(Mandatory = $true)]$Parsed)

    $entries = New-Object System.Collections.Generic.List[object]
    foreach ($entry in $Parsed)
    {
        [void]$entries.Add($entry)
    }
    return $entries
}

function Test-CompileDbParser
{
    $sample = '[{"file":"a.cpp"},{"file":"b.cpp"}]' | ConvertFrom-Json
    $entries = ConvertTo-EntryList -Parsed $sample
    if ($entries.Count -ne 2 -or [string]$entries[0].file -ne "a.cpp" -or [string]$entries[1].file -ne "b.cpp")
    {
        throw "Self-test del parser JSON fallo. No se ejecutara ningun cold."
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

    $parsed = Get-Content -LiteralPath $dbPath -Raw | ConvertFrom-Json
    $entries = ConvertTo-EntryList -Parsed $parsed
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

    if (-not (Test-Path -LiteralPath $LogPath))
    {
        throw "No existe log: $LogPath"
    }

    return @(Get-Content -LiteralPath $LogPath |
        Where-Object { ([string]$_) -match 'xtensa-esp32-elf-g\+\+.*\s-E\s' }).Count
}

function Invoke-NativeCaptured
{
    param(
        [Parameter(Mandatory = $true)][string]$FilePath,
        [Parameter(Mandatory = $true)][string[]]$Arguments
    )

    $oldErrorAction = $ErrorActionPreference
    $hadNativePreference = Test-Path variable:global:PSNativeCommandUseErrorActionPreference
    if ($hadNativePreference)
    {
        $oldNativePreference = $global:PSNativeCommandUseErrorActionPreference
    }

    $output = @()
    $exitCode = -1
    try
    {
        $ErrorActionPreference = "Continue"
        if ($hadNativePreference)
        {
            $global:PSNativeCommandUseErrorActionPreference = $false
        }
        $output = @(& $FilePath @Arguments 2>&1 | ForEach-Object { $_.ToString() })
        $exitCode = $LASTEXITCODE
    }
    finally
    {
        $ErrorActionPreference = $oldErrorAction
        if ($hadNativePreference)
        {
            $global:PSNativeCommandUseErrorActionPreference = $oldNativePreference
        }
    }

    return [PSCustomObject]@{
        ExitCode = [int]$exitCode
        Output = $output
    }
}

function Get-AppBin
{
    param([Parameter(Mandatory = $true)][string]$BuildPath)

    $bin = Get-ChildItem -LiteralPath $BuildPath -Filter "*.ino.bin" -File | Select-Object -First 1
    if ($null -eq $bin)
    {
        throw "No se encontro app .bin en $BuildPath"
    }
    return $bin
}

if ($Jobs -lt 0)
{
    throw "Jobs debe ser 0 o mayor."
}

Write-Host "JWPLC - P5A timing-only / PowerShell 7" -ForegroundColor Cyan
Write-Host ("PowerShell: {0} / {1}" -f $PSVersionTable.PSVersion, $PSVersionTable.PSEdition)
Write-Host ""

Test-CompileDbParser
Write-Host "Self-test parser JSON: OK" -ForegroundColor Green

if ($null -eq (Get-Command $ArduinoCli -ErrorAction SilentlyContinue))
{
    throw "No se encontro arduino-cli."
}
if (-not (Test-Path -LiteralPath $InspectorPath))
{
    throw "Falta inspector P5A: $InspectorPath"
}
if (-not (Test-Path -LiteralPath $BoardsLocalPath))
{
    throw "Falta boards.local.txt del overlay P2."
}
if (-not (Test-Path -LiteralPath $CoreArchivePath))
{
    throw "Falta core P2: $CoreArchivePath"
}
if (-not (Test-Path -LiteralPath $DisplayArchivePath))
{
    throw "Falta Display archive P3: $DisplayArchivePath"
}
if (-not (Test-Path -LiteralPath $EthernetArchivePath))
{
    throw "Falta archive Ethernet P5A: $EthernetArchivePath"
}

$ethernetPropertiesPath = Join-Path $EthernetRoot "library.properties"
if ((Get-Content -LiteralPath $ethernetPropertiesPath -Raw) -notmatch '(?m)^precompiled=full\s*$')
{
    throw "Backend Ethernet no declara precompiled=full."
}

Write-Host "Ejecutando quality gate P5A existente (sin compilar)..." -ForegroundColor Cyan
& $InspectorPath
Write-Host "Quality gate P5A existente: OK" -ForegroundColor Green

if (-not $RunCold)
{
    Write-Host ""
    Write-Host "VALIDACION SOLAMENTE: OK. No se ejecuto ninguna compilacion." -ForegroundColor Green
    Write-Host "Para ejecutar un unico cold medido, vuelve a lanzar con -RunCold." -ForegroundColor DarkGray
    return
}

$runId = (Get-Date).ToString("yyyyMMdd_HHmmss")
$runRoot = Join-Path $OutputRoot $runId
$buildPath = Join-Path $runRoot "p5a-Basic"
$logPath = Join-Path $runRoot "p5a-Basic.log"
$timingPath = Join-Path $runRoot "P5A_TIMING_SECONDS.txt"
$rawMetricsPath = Join-Path $runRoot "P5A_RAW_METRICS.json"
$summaryPath = Join-Path $runRoot "P5A_TIMING_SUMMARY.md"
New-Item -ItemType Directory -Path $buildPath -Force | Out-Null

$arguments = @(
    "compile",
    "-b", "jwplc_local:esp32:jwplcbasic",
    "-j", $Jobs.ToString(),
    "-v",
    "--build-path", $buildPath,
    "--clean",
    $SketchPath
)

Write-Host ""
Write-Host "Se ejecutara UN SOLO cold P5A para obtener tiempo oficial." -ForegroundColor Yellow
Write-Host ("arduino-cli {0}" -f ($arguments -join " ")) -ForegroundColor DarkGray

$stopwatch = [System.Diagnostics.Stopwatch]::StartNew()
$native = Invoke-NativeCaptured -FilePath $ArduinoCli -Arguments $arguments
$stopwatch.Stop()
$seconds = $stopwatch.Elapsed.TotalSeconds

@($native.Output) | Out-File -LiteralPath $logPath -Encoding utf8
("{0:R}" -f $seconds) | Set-Content -LiteralPath $timingPath -Encoding ascii

Write-Host ("Tiempo bruto preservado inmediatamente: {0:N3} s" -f $seconds) -ForegroundColor Green
Write-Host ("Log: {0}" -f $logPath) -ForegroundColor DarkGray
Write-Host ("Timing: {0}" -f $timingPath) -ForegroundColor DarkGray

if ($native.ExitCode -ne 0)
{
    @($native.Output | Select-Object -Last 25) | ForEach-Object { Write-Host $_ -ForegroundColor DarkYellow }
    throw "Arduino CLI fallo. El tiempo y el log quedaron preservados."
}

$metrics = Get-CompileDbMetrics -BuildPath $buildPath
$preprocess = Get-PreprocessCount -LogPath $logPath
$appBin = Get-AppBin -BuildPath $buildPath

$raw = [PSCustomObject]@{
    RunId = $runId
    Seconds = $seconds
    Compiles = $metrics.Compiles
    EthernetSource = $metrics.EthernetSource
    DisplaySource = $metrics.DisplaySource
    Stub = $metrics.Stub
    Preprocess = $preprocess
    AppBytes = [int64]$appBin.Length
    PowerShellVersion = [string]$PSVersionTable.PSVersion
    PSEdition = [string]$PSVersionTable.PSEdition
}
$raw | ConvertTo-Json | Set-Content -LiteralPath $rawMetricsPath -Encoding ascii

$summary = @(
    "# P5A Ethernet - timing cold",
    "",
    "Run: $runId",
    "",
    ("Cold: {0:N3} s" -f $seconds),
    ("Compiles: {0}" -f $metrics.Compiles),
    ("Ethernet source: {0}" -f $metrics.EthernetSource),
    ("Display source: {0}" -f $metrics.DisplaySource),
    ("Core stub: {0}" -f $metrics.Stub),
    ("Preprocesados -E: {0}" -f $preprocess),
    ("App bytes: {0}" -f $appBin.Length),
    ("PowerShell: {0} / {1}" -f $PSVersionTable.PSVersion, $PSVersionTable.PSEdition)
)
$summary | Set-Content -LiteralPath $summaryPath -Encoding utf8NoBOM

Write-Host ""
Write-Host ("Resultado medido: {0:N3} s | total={1}, Ethernet={2}, Display={3}, stub={4}, -E={5}, app={6} bytes" -f $seconds, $metrics.Compiles, $metrics.EthernetSource, $metrics.DisplaySource, $metrics.Stub, $preprocess, $appBin.Length) -ForegroundColor Cyan

if ($metrics.Compiles -ne 24 -or
    $metrics.EthernetSource -ne 0 -or
    $metrics.DisplaySource -ne 0 -or
    $metrics.Stub -ne 1 -or
    $preprocess -ne 41)
{
    throw ("El cold termino, pero la estructura P5A no coincide con 24/0/0/1/-E41. Actual={0}/{1}/{2}/{3}/{4}. Tiempo preservado: {5:N3} s" -f $metrics.Compiles, $metrics.EthernetSource, $metrics.DisplaySource, $metrics.Stub, $preprocess, $seconds)
}

Write-Host ""
Write-Host "=== P5A TIMING: VALIDADO ===" -ForegroundColor Green
Write-Host ("Cold oficial candidato: {0:N3} s" -f $seconds)
Write-Host ("Compiles: {0} | -E: {1} | App: {2} bytes" -f $metrics.Compiles, $preprocess, $appBin.Length)
Write-Host ("Artefactos: {0}" -f $runRoot) -ForegroundColor DarkGray
