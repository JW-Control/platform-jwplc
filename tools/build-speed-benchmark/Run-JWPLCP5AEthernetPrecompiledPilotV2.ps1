[CmdletBinding()]
param(
    [string]$ArduinoCli = "arduino-cli",
    [int]$Jobs = 0
)

Set-StrictMode -Version 2.0
$ErrorActionPreference = "Stop"

$ScriptRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$RepoRoot = [System.IO.Path]::GetFullPath((Join-Path $ScriptRoot "..\.."))
$PlatformRoot = Join-Path $RepoRoot "JWPLC\2.1.0"
$LibrariesRoot = Join-Path $PlatformRoot "libraries"
$SketchPath = Join-Path $ScriptRoot "sketches\01_empty"
$BoardsLocalPath = Join-Path $PlatformRoot "boards.local.txt"
$CoreArchivePath = Join-Path $PlatformRoot "precompiled\core\JWPLCBASIC\core.a"
$DisplayArchivePath = Join-Path $LibrariesRoot "JWPLC_Display\src\esp32\libJWPLC_Display.a"
$EthernetRoot = Join-Path $LibrariesRoot "JWPLC_Ethernet_W5x00_Backend"
$EthernetArchivePath = Join-Path $EthernetRoot "src\esp32\libJWPLC_Ethernet_W5x00_Backend.a"
$P3Root = Join-Path $ScriptRoot "p3-deterministic-work"
$OutputRoot = Join-Path $ScriptRoot "p5a-ethernet-work"

function Invoke-NativeCaptured
{
    param(
        [Parameter(Mandatory = $true)][string]$FilePath,
        [Parameter(Mandatory = $true)][string[]]$Arguments
    )

    $oldPreference = $ErrorActionPreference
    $output = @()
    $exitCode = -1
    try
    {
        $ErrorActionPreference = "Continue"
        $output = @(& $FilePath @Arguments 2>&1 | ForEach-Object { $_.ToString() })
        $exitCode = $LASTEXITCODE
    }
    finally
    {
        $ErrorActionPreference = $oldPreference
    }

    return [PSCustomObject]@{
        ExitCode = [int]$exitCode
        Output = $output
    }
}

function Resolve-NativeToolPath
{
    param([string]$Candidate)

    if ([string]::IsNullOrWhiteSpace($Candidate)) { return $null }
    $normalized = $Candidate.Trim().Trim('"')
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
    param([Parameter(Mandatory = $true)][string]$LogPath)

    if (-not (Test-Path -LiteralPath $LogPath)) { throw "No existe log P3: $LogPath" }
    $lines = @(Get-Content -LiteralPath $LogPath)

    foreach ($line in $lines)
    {
        $candidate = $null
        if ($line -match '"(?<exe>[^"]*xtensa-esp32-elf-gcc-ar(?:\.exe)?)"')
        {
            $candidate = $Matches["exe"]
        }
        elseif ($line -match '(?<exe>\S*xtensa-esp32-elf-gcc-ar(?:\.exe)?)\s+(?:cr|crs)\b')
        {
            $candidate = $Matches["exe"]
        }

        if (-not [string]::IsNullOrWhiteSpace($candidate))
        {
            $resolved = Resolve-NativeToolPath -Candidate $candidate
            if (-not [string]::IsNullOrWhiteSpace($resolved)) { return $resolved }
        }
    }

    foreach ($line in $lines)
    {
        $compilerCandidate = $null
        if ($line -match '"(?<exe>[^"]*xtensa-esp32-elf-g\+\+(?:\.exe)?)"')
        {
            $compilerCandidate = $Matches["exe"]
        }
        elseif ($line -match '(?<exe>\S*xtensa-esp32-elf-g\+\+(?:\.exe)?)\s')
        {
            $compilerCandidate = $Matches["exe"]
        }

        if ([string]::IsNullOrWhiteSpace($compilerCandidate)) { continue }
        $compiler = Resolve-NativeToolPath -Candidate $compilerCandidate
        if ([string]::IsNullOrWhiteSpace($compiler)) { continue }

        $toolDir = Split-Path -Parent $compiler
        foreach ($leaf in @("xtensa-esp32-elf-gcc-ar.exe", "xtensa-esp32-elf-gcc-ar"))
        {
            $sibling = Join-Path $toolDir $leaf
            if (Test-Path -LiteralPath $sibling)
            {
                return (Resolve-Path -LiteralPath $sibling).Path
            }
        }
    }

    throw "No se pudo localizar xtensa-esp32-elf-gcc-ar."
}

function Get-CompileDbMetrics
{
    param([Parameter(Mandatory = $true)][string]$BuildPath)

    $dbPath = Join-Path $BuildPath "compile_commands.json"
    if (-not (Test-Path -LiteralPath $dbPath))
    {
        throw "No existe compile_commands.json: $dbPath"
    }

    $entries = @(Get-Content -LiteralPath $dbPath -Raw | ConvertFrom-Json)
    $files = @($entries | ForEach-Object { [string]$_.file })

    return [PSCustomObject]@{
        Compiles = $entries.Count
        EthernetSource = @($files | Where-Object { $_ -match '[\\/]libraries[\\/]JWPLC_Ethernet_W5x00_Backend[\\/]' }).Count
        DisplaySource = @($files | Where-Object { $_ -match '[\\/]libraries[\\/]JWPLC_Display[\\/]' }).Count
        Stub = @($files | Where-Object { $_ -match '[\\/]cores[\\/]jwcontrol_precompiled_stub[\\/]p2_core_stub\.c$' }).Count
    }
}

function Get-PreprocessCount
{
    param([Parameter(Mandatory = $true)][string]$LogPath)
    return @(Get-Content -LiteralPath $LogPath | Where-Object { ([string]$_) -match 'xtensa-esp32-elf-g\+\+.*\s-E\s' }).Count
}

function Get-LibrarySelections
{
    param([Parameter(Mandatory = $true)][string]$LogPath)
    return @(Get-Content -LiteralPath $LogPath |
        Where-Object { ([string]$_) -like "Using library *" } |
        ForEach-Object { ([string]$_).Trim() } |
        Sort-Object -Unique)
}

function Get-ObjectTable
{
    param([Parameter(Mandatory = $true)][string]$BuildPath)

    $root = Join-Path $BuildPath "libraries"
    $table = @{}
    if (-not (Test-Path -LiteralPath $root)) { return $table }

    foreach ($file in Get-ChildItem -LiteralPath $root -Recurse -File -Filter "*.o")
    {
        $relative = $file.FullName.Substring($root.Length).TrimStart('\','/')
        if ($relative -match '^JWPLC_Display[\\/]' -or
            $relative -match '^JWPLC_Ethernet_W5x00_Backend[\\/]')
        {
            continue
        }
        $table[$relative] = (Get-FileHash -LiteralPath $file.FullName -Algorithm SHA256).Hash.ToLowerInvariant()
    }
    return $table
}

function Get-AppBin
{
    param([Parameter(Mandatory = $true)][string]$BuildPath)
    $bin = Get-ChildItem -LiteralPath $BuildPath -Filter "*.ino.bin" -File | Select-Object -First 1
    if ($null -eq $bin) { throw "No se encontro app .bin en $BuildPath" }
    return $bin
}

function Find-LatestP3
{
    if (-not (Test-Path -LiteralPath $P3Root)) { return $null }

    foreach ($run in @(Get-ChildItem -LiteralPath $P3Root -Directory | Sort-Object LastWriteTime -Descending))
    {
        $build = Join-Path $run.FullName "p3-Basic"
        $log = Join-Path $run.FullName "p3-Basic.log"
        $summary = Join-Path $run.FullName "P3_DETERMINISTIC_SUMMARY.md"
        $ethernetObjectsRoot = Join-Path $build "libraries\JWPLC_Ethernet_W5x00_Backend"

        if (-not (Test-Path -LiteralPath (Join-Path $build "compile_commands.json"))) { continue }
        if (-not (Test-Path -LiteralPath $log)) { continue }
        if (-not (Test-Path -LiteralPath $summary)) { continue }
        if (-not (Test-Path -LiteralPath $ethernetObjectsRoot)) { continue }

        $objects = @(Get-ChildItem -LiteralPath $ethernetObjectsRoot -Recurse -File -Filter "*.o")
        if ($objects.Count -ne 8) { continue }

        return [PSCustomObject]@{
            RunRoot = $run.FullName
            BuildPath = $build
            LogPath = $log
            SummaryPath = $summary
            EthernetObjects = $objects
        }
    }
    return $null
}

if ($Jobs -lt 0) { throw "Jobs debe ser 0 o mayor." }
if ($null -eq (Get-Command $ArduinoCli -ErrorAction SilentlyContinue)) { throw "No se encontro arduino-cli." }
if (-not (Test-Path -LiteralPath $BoardsLocalPath)) { throw "Falta boards.local.txt del overlay P2." }
if (-not (Test-Path -LiteralPath $CoreArchivePath)) { throw "Falta core P2." }
if (-not (Test-Path -LiteralPath $DisplayArchivePath)) { throw "Falta Display archive P3 determinista." }

$ethernetProperties = Join-Path $EthernetRoot "library.properties"
if ((Get-Content -LiteralPath $ethernetProperties -Raw) -notmatch '(?m)^precompiled=full\s*$')
{
    throw "Backend Ethernet no declara precompiled=full."
}

$p3 = Find-LatestP3
if ($null -eq $p3) { throw "No se encontro P3 determinista reutilizable." }

$p3Metrics = Get-CompileDbMetrics -BuildPath $p3.BuildPath
$p3Preprocess = Get-PreprocessCount -LogPath $p3.LogPath

Write-Host "P5A v2 - Ethernet W5x00 precompilado" -ForegroundColor Cyan
Write-Host ("P3 reutilizado: {0}" -f $p3.RunRoot)
Write-Host ""
Write-Host "Quality gate previo (sin compilar):" -ForegroundColor Cyan
Write-Host ("P3 compile DB: total={0}, Ethernet={1}, Display={2}, core stub={3}, -E={4}" -f $p3Metrics.Compiles, $p3Metrics.EthernetSource, $p3Metrics.DisplaySource, $p3Metrics.Stub, $p3Preprocess)

if ($p3Metrics.Compiles -ne 32 -or
    $p3Metrics.EthernetSource -ne 8 -or
    $p3Metrics.DisplaySource -ne 0 -or
    $p3Metrics.Stub -ne 1)
{
    throw "Quality gate P3 fallo. Esperado 32/8/0/1. No se ejecutara ningun cold."
}

Write-Host "Quality gate P3: OK" -ForegroundColor Green

$p3SummaryText = Get-Content -LiteralPath $p3.SummaryPath -Raw
$p3Seconds = $null
if ($p3SummaryText -match '(?m)^P3 cold:\s*(?<seconds>[0-9]+(?:\.[0-9]+)?)\s*s\s*$')
{
    $p3Seconds = [double]::Parse($Matches["seconds"], [System.Globalization.CultureInfo]::InvariantCulture)
}

$runId = (Get-Date).ToString("yyyyMMdd_HHmmss")
$runRoot = Join-Path $OutputRoot $runId
$buildPath = Join-Path $runRoot "p5a-Basic"
$logPath = Join-Path $runRoot "p5a-Basic.log"
$timingPath = Join-Path $runRoot "P5A_TIMING_SECONDS.txt"
$rawMetricsPath = Join-Path $runRoot "P5A_RAW_METRICS.json"
$summaryPath = Join-Path $runRoot "P5A_ETHERNET_SUMMARY.md"
New-Item -ItemType Directory -Path $runRoot -Force | Out-Null

$success = $false
$archiveGenerated = $false
try
{
    if (Test-Path -LiteralPath $EthernetArchivePath)
    {
        Remove-Item -LiteralPath $EthernetArchivePath -Force
    }

    $archiveDir = Split-Path -Parent $EthernetArchivePath
    New-Item -ItemType Directory -Path $archiveDir -Force | Out-Null

    Write-Host ""
    Write-Host "[1/3] Generando archive Ethernet desde 8 objetos P3 validados..." -ForegroundColor Cyan
    $archiver = Find-Archiver -LogPath $p3.LogPath
    $objectPaths = @($p3.EthernetObjects | Sort-Object FullName | ForEach-Object { $_.FullName })
    $archiveResult = Invoke-NativeCaptured -FilePath $archiver -Arguments (@("crs", $EthernetArchivePath) + $objectPaths)
    if ($archiveResult.ExitCode -ne 0 -or -not (Test-Path -LiteralPath $EthernetArchivePath))
    {
        throw "No se pudo generar archive Ethernet."
    }
    $archiveGenerated = $true
    Write-Host ("Ethernet archive: {0} bytes" -f (Get-Item -LiteralPath $EthernetArchivePath).Length) -ForegroundColor Green

    Write-Host "[2/3] Cold P5A v2..." -ForegroundColor Cyan
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
    Write-Host ("arduino-cli {0}" -f ($arguments -join " ")) -ForegroundColor DarkGray

    $stopwatch = [System.Diagnostics.Stopwatch]::StartNew()
    $native = Invoke-NativeCaptured -FilePath $ArduinoCli -Arguments $arguments
    $stopwatch.Stop()

    @($native.Output) | Out-File -LiteralPath $logPath -Encoding utf8
    $seconds = $stopwatch.Elapsed.TotalSeconds
    ("{0:R}" -f $seconds) | Set-Content -LiteralPath $timingPath -Encoding ascii

    if ($native.ExitCode -ne 0)
    {
        @($native.Output | Select-Object -Last 25) | ForEach-Object { Write-Host $_ -ForegroundColor DarkYellow }
        throw "Arduino CLI fallo. Tiempo y log quedaron preservados en $runRoot"
    }

    $metrics = Get-CompileDbMetrics -BuildPath $buildPath
    $preprocess = Get-PreprocessCount -LogPath $logPath
    [PSCustomObject]@{
        Seconds = $seconds
        Compiles = $metrics.Compiles
        EthernetSource = $metrics.EthernetSource
        DisplaySource = $metrics.DisplaySource
        Stub = $metrics.Stub
        Preprocess = $preprocess
    } | ConvertTo-Json | Set-Content -LiteralPath $rawMetricsPath -Encoding ascii

    Write-Host ("Resultado bruto preservado: {0:N3} s | total={1}, Ethernet={2}, Display={3}, stub={4}, -E={5}" -f $seconds, $metrics.Compiles, $metrics.EthernetSource, $metrics.DisplaySource, $metrics.Stub, $preprocess) -ForegroundColor Green

    $expectedCompiles = $p3Metrics.Compiles - $p3Metrics.EthernetSource
    if ($metrics.Compiles -ne $expectedCompiles) { throw ("Compiles inesperados. Esperado={0}, actual={1}." -f $expectedCompiles, $metrics.Compiles) }
    if ($metrics.EthernetSource -ne 0) { throw ("P5A recompilo {0} TUs Ethernet." -f $metrics.EthernetSource) }
    if ($metrics.DisplaySource -ne 0) { throw ("P5A recompilo {0} TUs Display." -f $metrics.DisplaySource) }
    if ($metrics.Stub -ne 1) { throw ("P5A esperaba 1 core stub real en compile_commands, obtuvo {0}." -f $metrics.Stub) }

    Write-Host "[3/3] Validando seleccion, objetos y link map..." -ForegroundColor Cyan
    $p3Selections = Get-LibrarySelections -LogPath $p3.LogPath
    $p5Selections = Get-LibrarySelections -LogPath $logPath
    $onlyP3Selection = @($p3Selections | Where-Object { $p5Selections -notcontains $_ })
    $onlyP5Selection = @($p5Selections | Where-Object { $p3Selections -notcontains $_ })
    if ($onlyP3Selection.Count -ne 0 -or $onlyP5Selection.Count -ne 0)
    {
        throw "Cambio la seleccion de librerias entre P3 y P5A."
    }

    $p3Objects = Get-ObjectTable -BuildPath $p3.BuildPath
    $p5Objects = Get-ObjectTable -BuildPath $buildPath
    $common = @($p3Objects.Keys | Where-Object { $p5Objects.ContainsKey($_) })
    $hashMismatch = @($common | Where-Object { $p3Objects[$_] -ne $p5Objects[$_] })
    $onlyP3Objects = @($p3Objects.Keys | Where-Object { -not $p5Objects.ContainsKey($_) })
    $onlyP5Objects = @($p5Objects.Keys | Where-Object { -not $p3Objects.ContainsKey($_) })
    if ($hashMismatch.Count -ne 0 -or $onlyP3Objects.Count -ne 0 -or $onlyP5Objects.Count -ne 0)
    {
        throw "Cambian objetos externos a Display/Ethernet entre P3 y P5A."
    }

    $mapPath = Join-Path $buildPath "01_empty.ino.map"
    if (-not (Test-Path -LiteralPath $mapPath)) { throw "No se genero map P5A." }
    $mapText = Get-Content -LiteralPath $mapPath -Raw
    $archiveLinked = $mapText -match 'libJWPLC_Ethernet_W5x00_Backend\.a\('
    $ethernetLinked = $mapText -match 'libJWPLC_Ethernet_W5x00_Backend\.a\(Ethernet\.cpp\.o\)'
    $w5100Linked = $mapText -match 'libJWPLC_Ethernet_W5x00_Backend\.a\(w5100\.cpp\.o\)'
    if (-not $archiveLinked -or -not $ethernetLinked -or -not $w5100Linked)
    {
        throw "El map no demuestra extraccion del archive Ethernet esperada."
    }

    $p3Bin = Get-AppBin -BuildPath $p3.BuildPath
    $p5Bin = Get-AppBin -BuildPath $buildPath
    $appDelta = [int64]$p5Bin.Length - [int64]$p3Bin.Length

    $timeDelta = $null
    $timePct = $null
    if ($null -ne $p3Seconds -and $p3Seconds -gt 0)
    {
        $timeDelta = $seconds - $p3Seconds
        $timePct = 100.0 * $timeDelta / $p3Seconds
    }

    Write-Host ""
    Write-Host "=== P5A ETHERNET PRECOMPILADO V2 ===" -ForegroundColor Cyan
    if ($null -ne $p3Seconds)
    {
        Write-Host ("P3 referencia : {0:N3} s | compiles={1} | -E={2} | Ethernet source={3}" -f $p3Seconds, $p3Metrics.Compiles, $p3Preprocess, $p3Metrics.EthernetSource)
    }
    Write-Host ("P5A v2       : {0:N3} s | compiles={1} | -E={2} | Ethernet source={3}" -f $seconds, $metrics.Compiles, $preprocess, $metrics.EthernetSource)
    if ($null -ne $timeDelta)
    {
        Write-Host ("Delta tiempo  : {0:+0.000;-0.000;0.000} s ({1:+0.00;-0.00;0.00}%)" -f $timeDelta, $timePct)
    }
    Write-Host ("Core stub      : {0}" -f $metrics.Stub)
    Write-Host ("App bytes      : {0} -> {1} | delta={2}" -f $p3Bin.Length, $p5Bin.Length, $appDelta)
    Write-Host ("Archive linked : {0} | Ethernet.cpp={1} | w5100.cpp={2}" -f $archiveLinked, $ethernetLinked, $w5100Linked)
    Write-Host ("Objetos externos comunes={0}, SHA distintos={1}, solo P3={2}, solo P5A={3}" -f $common.Count, $hashMismatch.Count, $onlyP3Objects.Count, $onlyP5Objects.Count)

    $summary = New-Object System.Collections.Generic.List[string]
    $summary.Add("# P5A v2 - Ethernet W5x00 precompilado")
    $summary.Add("")
    $summary.Add(("P3 run: {0}" -f $p3.RunRoot))
    if ($null -ne $p3Seconds) { $summary.Add(("P3 cold: {0:N3} s" -f $p3Seconds)) }
    $summary.Add(("P5A cold: {0:N3} s" -f $seconds))
    $summary.Add(("P5A compiles: {0}" -f $metrics.Compiles))
    $summary.Add(("P5A preprocesados -E: {0}" -f $preprocess))
    $summary.Add(("P5A Ethernet source: {0}" -f $metrics.EthernetSource))
    $summary.Add(("P5A Display source: {0}" -f $metrics.DisplaySource))
    $summary.Add(("P5A core stub: {0}" -f $metrics.Stub))
    $summary.Add(("App P3 bytes: {0}" -f $p3Bin.Length))
    $summary.Add(("App P5A bytes: {0}" -f $p5Bin.Length))
    $summary.Add(("Delta app bytes: {0}" -f $appDelta))
    $summary.Add(("Archive linked: {0}" -f $archiveLinked))
    $summary.Add(("Objetos externos SHA distintos: {0}" -f $hashMismatch.Count))
    $summary.Add(("Diferencias seleccion librerias: {0}" -f ($onlyP3Selection.Count + $onlyP5Selection.Count)))
    $summary | Out-File -LiteralPath $summaryPath -Encoding ascii

    $success = $true
    Write-Host ""
    Write-Host "P5A ETHERNET V2: OK" -ForegroundColor Green
    Write-Host ("Resumen: {0}" -f $summaryPath)
}
finally
{
    if (-not $success -and $archiveGenerated -and (Test-Path -LiteralPath $EthernetArchivePath))
    {
        $backupPath = Join-Path $runRoot "libJWPLC_Ethernet_W5x00_Backend.failed.a"
        Copy-Item -LiteralPath $EthernetArchivePath -Destination $backupPath -Force -ErrorAction SilentlyContinue
        Remove-Item -LiteralPath $EthernetArchivePath -Force -ErrorAction SilentlyContinue
        Write-Host ("P5A v2 fallo. Archive retirado; copia forense: {0}" -f $backupPath) -ForegroundColor Yellow
    }
}
