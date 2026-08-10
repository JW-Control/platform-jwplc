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
$OutputRoot = Join-Path $ScriptRoot "p5a-ethernet-work"

function Invoke-NativeCaptured
{
    param([string]$FilePath, [string[]]$Arguments)
    $old = $ErrorActionPreference
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
        $ErrorActionPreference = $old
    }
    return [PSCustomObject]@{ ExitCode = [int]$exitCode; Output = $output }
}

function Resolve-NativeToolPath
{
    param([string]$Candidate)
    if ([string]::IsNullOrWhiteSpace($Candidate)) { return $null }
    $normalized = $Candidate.Trim().Trim('"')
    while ($normalized.Contains("\\")) { $normalized = $normalized.Replace("\\", "\") }
    $paths = New-Object System.Collections.Generic.List[string]
    $paths.Add($normalized)
    if (-not [System.IO.Path]::HasExtension($normalized))
    {
        $paths.Add($normalized + ".exe")
        $paths.Add($normalized + ".cmd")
        $paths.Add($normalized + ".bat")
    }
    foreach ($path in $paths)
    {
        if (Test-Path -LiteralPath $path) { return (Resolve-Path -LiteralPath $path).Path }
    }
    return $null
}

function Find-Archiver
{
    param([string]$LogPath)
    foreach ($line in @(Get-Content -LiteralPath $LogPath))
    {
        $candidate = $null
        if ($line -match '"(?<exe>[^"]*xtensa-esp32-elf-gcc-ar(?:\.exe)?)"') { $candidate = $Matches["exe"] }
        elseif ($line -match '(?<exe>\S*xtensa-esp32-elf-gcc-ar(?:\.exe)?)\s+(?:cr|crs)\b') { $candidate = $Matches["exe"] }
        if (-not [string]::IsNullOrWhiteSpace($candidate))
        {
            $resolved = Resolve-NativeToolPath $candidate
            if (-not [string]::IsNullOrWhiteSpace($resolved)) { return $resolved }
        }
    }

    foreach ($line in @(Get-Content -LiteralPath $LogPath))
    {
        $candidate = $null
        if ($line -match '"(?<exe>[^"]*xtensa-esp32-elf-g\+\+(?:\.exe)?)"') { $candidate = $Matches["exe"] }
        elseif ($line -match '(?<exe>\S*xtensa-esp32-elf-g\+\+(?:\.exe)?)\s') { $candidate = $Matches["exe"] }
        if ([string]::IsNullOrWhiteSpace($candidate)) { continue }
        $compiler = Resolve-NativeToolPath $candidate
        if ([string]::IsNullOrWhiteSpace($compiler)) { continue }
        $toolDir = Split-Path -Parent $compiler
        foreach ($leaf in @("xtensa-esp32-elf-gcc-ar.exe", "xtensa-esp32-elf-gcc-ar"))
        {
            $sibling = Join-Path $toolDir $leaf
            if (Test-Path -LiteralPath $sibling) { return (Resolve-Path -LiteralPath $sibling).Path }
        }
    }
    throw "No se pudo localizar xtensa-esp32-elf-gcc-ar."
}

function Find-LatestP3DeterministicRun
{
    $root = Join-Path $ScriptRoot "p3-deterministic-work"
    if (-not (Test-Path -LiteralPath $root)) { return $null }
    foreach ($run in @(Get-ChildItem -LiteralPath $root -Directory | Sort-Object LastWriteTime -Descending))
    {
        $build = Join-Path $run.FullName "p3-Basic"
        $log = Join-Path $run.FullName "p3-Basic.log"
        $summary = Join-Path $run.FullName "P3_DETERMINISTIC_SUMMARY.md"
        $ethObjects = Join-Path $build "libraries\JWPLC_Ethernet_W5x00_Backend"
        if ((Test-Path -LiteralPath $build) -and (Test-Path -LiteralPath $log) -and
            (Test-Path -LiteralPath $summary) -and (Test-Path -LiteralPath $ethObjects))
        {
            $objects = @(Get-ChildItem -LiteralPath $ethObjects -Recurse -File -Filter "*.o")
            if ($objects.Count -eq 8)
            {
                return [PSCustomObject]@{
                    RunRoot = $run.FullName
                    BuildPath = $build
                    LogPath = $log
                    SummaryPath = $summary
                    EthernetObjects = $objects
                }
            }
        }
    }
    return $null
}

function Get-CompileSource
{
    param([string]$Line)
    $m = [regex]::Match($Line, '\s"(?<src>[^"]+\.(?:cpp|c|cc|S))"\s+-o\s+"')
    if ($m.Success) { return $m.Groups["src"].Value }
    $m = [regex]::Match($Line, '\s(?<src>\S+\.(?:cpp|c|cc|S))\s+-o\s+')
    if ($m.Success) { return $m.Groups["src"].Value }
    return ""
}

function Get-Metrics
{
    param([object[]]$Output)
    $compileLines = @($Output | Where-Object { ([string]$_) -match '-MMD\s+-c\s' })
    $eLines = @($Output | Where-Object { ([string]$_) -match 'xtensa-esp32-elf-g\+\+.*\s-E\s' })
    $ethCompiles = 0
    $displayCompiles = 0
    $stubCompiles = 0
    foreach ($line in $compileLines)
    {
        $src = Get-CompileSource -Line ([string]$line)
        if ($src -match '[\\/]libraries[\\/]JWPLC_Ethernet_W5x00_Backend[\\/]') { $ethCompiles++ }
        if ($src -match '[\\/]libraries[\\/]JWPLC_Display[\\/]') { $displayCompiles++ }
        if ($src -match '[\\/]cores[\\/]jwcontrol_p2[\\/]p2_core_stub\.c$') { $stubCompiles++ }
    }
    $ethE = @($eLines | Where-Object { ([string]$_) -match 'JWPLC_Ethernet_W5x00_Backend' }).Count
    return [PSCustomObject]@{
        Compiles = $compileLines.Count
        Preprocess = $eLines.Count
        EthernetSource = $ethCompiles
        EthernetE = $ethE
        DisplaySource = $displayCompiles
        Stub = $stubCompiles
    }
}

function Get-LibrarySelections
{
    param([object[]]$Output)
    return @($Output | Where-Object { ([string]$_) -like "Using library *" } | ForEach-Object { ([string]$_).Trim() } | Sort-Object -Unique)
}

function Get-ObjectTable
{
    param([string]$BuildPath)
    $root = Join-Path $BuildPath "libraries"
    $table = @{}
    if (-not (Test-Path -LiteralPath $root)) { return $table }
    foreach ($file in Get-ChildItem -LiteralPath $root -Recurse -File -Filter "*.o")
    {
        $relative = $file.FullName.Substring($root.Length).TrimStart('\','/')
        if ($relative -match '^JWPLC_Display[\\/]' -or $relative -match '^JWPLC_Ethernet_W5x00_Backend[\\/]') { continue }
        $table[$relative] = (Get-FileHash -LiteralPath $file.FullName -Algorithm SHA256).Hash.ToLowerInvariant()
    }
    return $table
}

function Get-AppBin
{
    param([string]$BuildPath)
    $bin = Get-ChildItem -LiteralPath $BuildPath -Filter "*.ino.bin" -File | Select-Object -First 1
    if ($null -eq $bin) { throw "No se encontro app .bin en $BuildPath" }
    return $bin
}

if ($Jobs -lt 0) { throw "Jobs debe ser 0 o mayor." }
if ($null -eq (Get-Command $ArduinoCli -ErrorAction SilentlyContinue)) { throw "No se encontro arduino-cli." }
if (-not (Test-Path -LiteralPath $BoardsLocalPath)) { throw "Falta overlay P2 boards.local.txt." }
if (-not (Test-Path -LiteralPath $CoreArchivePath)) { throw "Falta core P2." }
if (-not (Test-Path -LiteralPath $DisplayArchivePath)) { throw "P5A requiere libJWPLC_Display.a determinista activo." }

$propsPath = Join-Path $EthernetRoot "library.properties"
$props = Get-Content -LiteralPath $propsPath -Raw
if ($props -notmatch '(?m)^precompiled=full\s*$') { throw "Backend Ethernet no declara precompiled=full. Ejecuta git pull." }

$p3 = Find-LatestP3DeterministicRun
if ($null -eq $p3) { throw "No se encontro un run P3 determinista reutilizable con 8 objetos Ethernet." }

$p3Summary = Get-Content -LiteralPath $p3.SummaryPath -Raw
$p3Seconds = $null
if ($p3Summary -match '(?m)^P3 cold:\s*(?<seconds>[0-9]+(?:\.[0-9]+)?)\s*s\s*$')
{
    $p3Seconds = [double]::Parse($Matches["seconds"], [System.Globalization.CultureInfo]::InvariantCulture)
}

Write-Host "P5A - Ethernet W5x00 2.0.2 precompilado" -ForegroundColor Cyan
Write-Host ("P3 reutilizado: {0}" -f $p3.RunRoot)
Write-Host ("Objetos Ethernet fuente: {0}" -f $p3.EthernetObjects.Count)
if ($null -ne $p3Seconds) { Write-Host ("P3 referencia: {0:N3} s" -f $p3Seconds) }
Write-Host "Se ejecutara un solo cold adicional." -ForegroundColor Yellow
Write-Host ""

$success = $false
try
{
    if (Test-Path -LiteralPath $EthernetArchivePath) { Remove-Item -LiteralPath $EthernetArchivePath -Force }
    $archiveDir = Split-Path -Parent $EthernetArchivePath
    New-Item -ItemType Directory -Path $archiveDir -Force | Out-Null

    Write-Host "[1/3] Generando archive Ethernet desde objetos P3 deterministas..." -ForegroundColor Cyan
    $archiver = Find-Archiver -LogPath $p3.LogPath
    $objects = @($p3.EthernetObjects | Sort-Object FullName | ForEach-Object { $_.FullName })
    $ar = Invoke-NativeCaptured -FilePath $archiver -Arguments (@("crs", $EthernetArchivePath) + $objects)
    if ($ar.ExitCode -ne 0 -or -not (Test-Path -LiteralPath $EthernetArchivePath)) { throw "No se pudo generar archive Ethernet." }
    Write-Host ("Ethernet archive: {0} bytes" -f (Get-Item -LiteralPath $EthernetArchivePath).Length) -ForegroundColor Green

    $runId = (Get-Date).ToString("yyyyMMdd_HHmmss")
    $runRoot = Join-Path $OutputRoot $runId
    $buildPath = Join-Path $runRoot "p5a-Basic"
    $logPath = Join-Path $runRoot "p5a-Basic.log"
    $summaryPath = Join-Path $runRoot "P5A_ETHERNET_SUMMARY.md"
    New-Item -ItemType Directory -Path $buildPath -Force | Out-Null

    Write-Host "[2/3] Cold P5A: P3 + Ethernet precompilado..." -ForegroundColor Cyan
    $args = @("compile", "-b", "jwplc_local:esp32:jwplcbasic", "-j", $Jobs.ToString(), "-v", "--build-path", $buildPath, "--clean", $SketchPath)
    Write-Host ("arduino-cli {0}" -f ($args -join " ")) -ForegroundColor DarkGray
    $sw = [System.Diagnostics.Stopwatch]::StartNew()
    $native = Invoke-NativeCaptured -FilePath $ArduinoCli -Arguments $args
    $sw.Stop()
    @($native.Output) | Out-File -LiteralPath $logPath -Encoding utf8
    if ($native.ExitCode -ne 0)
    {
        @($native.Output | Select-Object -Last 25) | ForEach-Object { Write-Host $_ -ForegroundColor DarkYellow }
        throw "Arduino CLI fallo. Revisar $logPath"
    }

    $metrics = Get-Metrics -Output $native.Output
    if ($metrics.EthernetSource -ne 0) { throw ("P5A recompilo {0} TUs Ethernet." -f $metrics.EthernetSource) }
    if ($metrics.DisplaySource -ne 0) { throw "P5A recompilo Display." }
    if ($metrics.Stub -ne 1) { throw ("P5A esperaba 1 core stub, obtuvo {0}." -f $metrics.Stub) }

    Write-Host "[3/3] Comparando estructura contra P3 determinista..." -ForegroundColor Cyan
    $p3LogOutput = @(Get-Content -LiteralPath $p3.LogPath)
    $p3Selections = Get-LibrarySelections -Output $p3LogOutput
    $p5Selections = Get-LibrarySelections -Output $native.Output
    $selectionOnlyP3 = @($p3Selections | Where-Object { $p5Selections -notcontains $_ })
    $selectionOnlyP5 = @($p5Selections | Where-Object { $p3Selections -notcontains $_ })
    if ($selectionOnlyP3.Count -ne 0 -or $selectionOnlyP5.Count -ne 0) { throw "Cambio la seleccion de librerias entre P3 y P5A." }

    $p3Objects = Get-ObjectTable -BuildPath $p3.BuildPath
    $p5Objects = Get-ObjectTable -BuildPath $buildPath
    $common = @($p3Objects.Keys | Where-Object { $p5Objects.ContainsKey($_) })
    $hashMismatch = @($common | Where-Object { $p3Objects[$_] -ne $p5Objects[$_] })
    $onlyP3 = @($p3Objects.Keys | Where-Object { -not $p5Objects.ContainsKey($_) })
    $onlyP5 = @($p5Objects.Keys | Where-Object { -not $p3Objects.ContainsKey($_) })
    if ($hashMismatch.Count -ne 0 -or $onlyP3.Count -ne 0 -or $onlyP5.Count -ne 0) { throw "Cambian objetos externos a Ethernet/Display." }

    $mapPath = Join-Path $buildPath "01_empty.ino.map"
    if (-not (Test-Path -LiteralPath $mapPath)) { throw "No se genero map P5A." }
    $map = Get-Content -LiteralPath $mapPath -Raw
    $archiveLinked = $map -match 'libJWPLC_Ethernet_W5x00_Backend\.a\('
    $ethernetMember = $map -match 'libJWPLC_Ethernet_W5x00_Backend\.a\(Ethernet\.cpp\.o\)'
    $w5100Member = $map -match 'libJWPLC_Ethernet_W5x00_Backend\.a\(w5100\.cpp\.o\)'
    if (-not $archiveLinked -or -not $ethernetMember -or -not $w5100Member) { throw "Archive Ethernet no fue extraido como se esperaba." }

    $p3Bin = Get-AppBin -BuildPath $p3.BuildPath
    $p5Bin = Get-AppBin -BuildPath $buildPath
    $appDelta = [int64]$p5Bin.Length - [int64]$p3Bin.Length
    $seconds = $sw.Elapsed.TotalSeconds
    $timeDelta = $null
    $timePct = $null
    if ($null -ne $p3Seconds -and $p3Seconds -gt 0)
    {
        $timeDelta = $seconds - $p3Seconds
        $timePct = 100.0 * $timeDelta / $p3Seconds
    }

    Write-Host ""
    Write-Host "=== P5A ETHERNET PRECOMPILADO ===" -ForegroundColor Cyan
    if ($null -ne $p3Seconds) { Write-Host ("P3 referencia : {0:N3} s | compiles=32 | -E=49 | Ethernet source=8" -f $p3Seconds) }
    Write-Host ("P5A          : {0:N3} s | compiles={1} | -E={2} | Ethernet source={3} | Ethernet -E={4}" -f $seconds, $metrics.Compiles, $metrics.Preprocess, $metrics.EthernetSource, $metrics.EthernetE)
    if ($null -ne $timeDelta) { Write-Host ("Delta tiempo   : {0:+0.000;-0.000;0.000} s ({1:+0.00;-0.00;0.00}%)" -f $timeDelta, $timePct) }
    Write-Host ("App bytes      : {0} -> {1} | delta={2}" -f $p3Bin.Length, $p5Bin.Length, $appDelta)
    Write-Host ("Archive linked : {0} | Ethernet.cpp={1} | w5100.cpp={2}" -f $archiveLinked, $ethernetMember, $w5100Member)
    Write-Host ("Objetos externos comunes={0}, SHA distintos={1}, solo P3={2}, solo P5A={3}" -f $common.Count, $hashMismatch.Count, $onlyP3.Count, $onlyP5.Count)

    $summary = New-Object System.Collections.Generic.List[string]
    $summary.Add("# P5A - Ethernet W5x00 precompilado")
    $summary.Add("")
    $summary.Add(("P3 run referencia: {0}" -f $p3.RunRoot))
    if ($null -ne $p3Seconds) { $summary.Add(("P3 cold: {0:N3} s" -f $p3Seconds)) }
    $summary.Add(("P5A cold: {0:N3} s" -f $seconds))
    $summary.Add(("P5A compiles: {0}" -f $metrics.Compiles))
    $summary.Add(("P5A preprocesados -E: {0}" -f $metrics.Preprocess))
    $summary.Add(("P5A Ethernet source: {0}" -f $metrics.EthernetSource))
    $summary.Add(("P5A Ethernet -E: {0}" -f $metrics.EthernetE))
    if ($null -ne $timeDelta) { $summary.Add(("Delta tiempo: {0:N3} s ({1:N2}%)" -f $timeDelta, $timePct)) }
    $summary.Add(("App P3 bytes: {0}" -f $p3Bin.Length))
    $summary.Add(("App P5A bytes: {0}" -f $p5Bin.Length))
    $summary.Add(("Delta app bytes: {0}" -f $appDelta))
    $summary.Add(("Archive enlazado: {0}" -f $archiveLinked))
    $summary.Add(("Objetos externos SHA distintos: {0}" -f $hashMismatch.Count))
    $summary.Add(("Diferencias seleccion librerias: {0}" -f ($selectionOnlyP3.Count + $selectionOnlyP5.Count)))
    $summary | Out-File -LiteralPath $summaryPath -Encoding ascii

    $success = $true
    Write-Host ""
    Write-Host "P5A ETHERNET: OK" -ForegroundColor Green
    Write-Host ("Resumen: {0}" -f $summaryPath)
    Write-Host "Archive Ethernet queda activo para evaluar el siguiente paso." -ForegroundColor Green
}
finally
{
    if (-not $success -and (Test-Path -LiteralPath $EthernetArchivePath))
    {
        Write-Host "P5A fallo; retirando archive Ethernet para volver al fallback fuente." -ForegroundColor Yellow
        Remove-Item -LiteralPath $EthernetArchivePath -Force
    }
}
