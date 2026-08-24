[CmdletBinding()]
param(
    [string]$ArduinoCli = "arduino-cli",
    [int]$Jobs = 0,
    [string]$RunId = ""
)

Set-StrictMode -Version 2.0
$ErrorActionPreference = "Stop"

$ScriptRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$RepoRoot = [System.IO.Path]::GetFullPath((Join-Path $ScriptRoot "..\.."))
$PlatformRoot = Join-Path $RepoRoot "JWPLC\2.1.0"
$LibrariesRoot = Join-Path $PlatformRoot "libraries"
$SketchPath = Join-Path $ScriptRoot "sketches\01_empty"
$OutputRoot = Join-Path $ScriptRoot "p3-deterministic-work"
$DisplayArchivePath = Join-Path $LibrariesRoot "JWPLC_Display\src\esp32\libJWPLC_Display.a"

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

    if (Test-Path -LiteralPath $LogPath)
    {
        foreach ($line in Get-Content -LiteralPath $LogPath)
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

        foreach ($line in Get-Content -LiteralPath $LogPath)
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
                if (Test-Path -LiteralPath $sibling) { return (Resolve-Path -LiteralPath $sibling).Path }
            }
        }
    }

    if (-not [string]::IsNullOrWhiteSpace($env:LOCALAPPDATA))
    {
        $packagesRoot = Join-Path $env:LOCALAPPDATA "Arduino15\packages"
        foreach ($namespace in @("jwplc_local", "jwplc"))
        {
            $espX32Root = Join-Path $packagesRoot ($namespace + "\tools\esp-x32")
            if (-not (Test-Path -LiteralPath $espX32Root)) { continue }

            $found = Get-ChildItem -LiteralPath $espX32Root -Recurse -File -Filter "xtensa-esp32-elf-gcc-ar.exe" -ErrorAction SilentlyContinue |
                Sort-Object LastWriteTime -Descending |
                Select-Object -First 1
            if ($null -ne $found) { return $found.FullName }
        }
    }

    throw "No se pudo localizar xtensa-esp32-elf-gcc-ar."
}

function Get-BuildMetrics
{
    param([object[]]$Output)

    $compileLines = @($Output | Where-Object { ([string]$_) -match '-MMD\s+-c\s' })
    $preprocessLines = @($Output | Where-Object { ([string]$_) -match 'xtensa-esp32-elf-g\+\+.*\s-E\s' })

    $displaySourcePattern = '"[^"]*[\\/]+libraries[\\/]+JWPLC_Display[\\/]+src[\\/]+JWPLC_(?:Display|IdleScreen)\.cpp"\s+-o\s+"'
    $stubPattern = '"[^"]*[\\/]+cores[\\/]+jwcontrol_precompiled_stub[\\/]+p2_core_stub\.c"\s+-o\s+"'

    return [PSCustomObject]@{
        Compiles = $compileLines.Count
        Preprocess = $preprocessLines.Count
        DisplaySource = @($compileLines | Where-Object { ([string]$_) -match $displaySourcePattern }).Count
        Stub = @($compileLines | Where-Object { ([string]$_) -match $stubPattern }).Count
    }
}

function Assert-DeterministicLibraries
{
    param([object[]]$Output, [string]$Label)

    $expected = @(
        [PSCustomObject]@{ Name = "Adafruit ST7735 and ST7789 Library"; Folder = "Adafruit_ST7735_and_ST7789_Library" },
        [PSCustomObject]@{ Name = "Adafruit GFX Library"; Folder = "Adafruit_GFX_Library" },
        [PSCustomObject]@{ Name = "Adafruit BusIO"; Folder = "Adafruit_BusIO" },
        [PSCustomObject]@{ Name = "JWPLC Ethernet W5x00 Backend"; Folder = "JWPLC_Ethernet_W5x00_Backend" }
    )

    foreach ($item in $expected)
    {
        $line = @($Output | Where-Object { ([string]$_) -like ("Using library " + $item.Name + " at version *") } | Select-Object -Last 1)
        if ($line.Count -eq 0) { throw ("{0}: no se detecto {1}." -f $Label, $item.Name) }

        $expectedPath = [System.IO.Path]::GetFullPath((Join-Path $LibrariesRoot $item.Folder))
        if (([string]$line[0]).IndexOf($expectedPath, [System.StringComparison]::OrdinalIgnoreCase) -lt 0)
        {
            throw ("{0}: {1} no proviene del package." -f $Label, $item.Name)
        }
    }

    $wrongEthernet = @($Output | Where-Object { ([string]$_) -match '^Using library Ethernet at version ' })
    if ($wrongEthernet.Count -gt 0) { throw ("{0}: se selecciono Ethernet homonima externa." -f $Label) }

    $userExternal = @($Output | Where-Object {
        ([string]$_) -like "Using library *" -and
        (([string]$_) -match '[\\/]Arduino[\\/]libraries[\\/](Adafruit|Ethernet)')
    })
    if ($userExternal.Count -gt 0) { throw ("{0}: se seleccionaron dependencias del sketchbook." -f $Label) }
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
        if ($relative -match '^JWPLC_Display[\\/]') { continue }
        $table[$relative] = (Get-FileHash $file.FullName -Algorithm SHA256).Hash.ToLowerInvariant()
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

function Find-ResumeRun
{
    if (-not [string]::IsNullOrWhiteSpace($RunId))
    {
        $requested = Join-Path $OutputRoot $RunId
        if (-not (Test-Path -LiteralPath $requested)) { throw "No existe run solicitado: $requested" }
        return $requested
    }

    if (-not (Test-Path -LiteralPath $OutputRoot)) { throw "No existe $OutputRoot" }

    foreach ($run in @(Get-ChildItem -LiteralPath $OutputRoot -Directory | Sort-Object Name -Descending))
    {
        $source = Join-Path $run.FullName "source-Basic"
        $log = Join-Path $run.FullName "source-Basic.log"
        $display1 = Join-Path $source "libraries\JWPLC_Display\JWPLC_Display.cpp.o"
        $display2 = Join-Path $source "libraries\JWPLC_Display\JWPLC_IdleScreen.cpp.o"
        $bin = Get-ChildItem -LiteralPath $source -Filter "*.ino.bin" -File -ErrorAction SilentlyContinue | Select-Object -First 1

        if ((Test-Path -LiteralPath $log) -and (Test-Path -LiteralPath $display1) -and (Test-Path -LiteralPath $display2) -and ($null -ne $bin))
        {
            return $run.FullName
        }
    }

    throw "No se encontro un source-Basic determinista reutilizable."
}

if ($Jobs -lt 0) { throw "Jobs debe ser 0 o mayor." }
if ($null -eq (Get-Command $ArduinoCli -ErrorAction SilentlyContinue)) { throw "No se encontro arduino-cli." }

$runRoot = Find-ResumeRun
$sourceBuild = Join-Path $runRoot "source-Basic"
$sourceLog = Join-Path $runRoot "source-Basic.log"
$p3Build = Join-Path $runRoot "p3-Basic"
$p3Log = Join-Path $runRoot "p3-Basic.log"
$summaryPath = Join-Path $runRoot "P3_DETERMINISTIC_SUMMARY.md"

$sourceOutput = @(Get-Content -LiteralPath $sourceLog)
$sourceMetrics = Get-BuildMetrics -Output $sourceOutput
Assert-DeterministicLibraries -Output $sourceOutput -Label "Fuente reutilizada"

$sourceDisplay1 = Join-Path $sourceBuild "libraries\JWPLC_Display\JWPLC_Display.cpp.o"
$sourceDisplay2 = Join-Path $sourceBuild "libraries\JWPLC_Display\JWPLC_IdleScreen.cpp.o"
$coreStubObject = Join-Path $sourceBuild "core\precompiled_core_stub.c.o"

if (-not (Test-Path -LiteralPath $sourceDisplay1) -or -not (Test-Path -LiteralPath $sourceDisplay2))
{
    throw "El source reutilizado no contiene ambos objetos Display."
}
if (-not (Test-Path -LiteralPath $coreStubObject)) { throw "El source reutilizado no contiene core stub P2." }
if ($sourceMetrics.DisplaySource -ne 2) { throw ("Parser corregido esperaba 2 TUs Display, obtuvo {0}." -f $sourceMetrics.DisplaySource) }

Write-Host "P3 determinista - reanudacion" -ForegroundColor Cyan
Write-Host ("Run reutilizado: {0}" -f $runRoot)
Write-Host ("Fuente valida: compiles={0} | -E={1} | Display source={2}" -f $sourceMetrics.Compiles, $sourceMetrics.Preprocess, $sourceMetrics.DisplaySource) -ForegroundColor Green
Write-Host "No se repetira el cold fuente." -ForegroundColor Yellow
Write-Host ""

$success = $false
try
{
    if (Test-Path -LiteralPath $DisplayArchivePath) { Remove-Item -LiteralPath $DisplayArchivePath -Force }

    Write-Host "[1/3] Generando JWPLC_Display.a desde objetos deterministas existentes..." -ForegroundColor Cyan
    $archiver = Find-Archiver -LogPath $sourceLog
    $archiveDir = Split-Path -Parent $DisplayArchivePath
    New-Item -ItemType Directory -Path $archiveDir -Force | Out-Null

    $ar = Invoke-NativeCaptured -FilePath $archiver -Arguments @("crs", $DisplayArchivePath, $sourceDisplay1, $sourceDisplay2)
    if ($ar.ExitCode -ne 0 -or -not (Test-Path -LiteralPath $DisplayArchivePath)) { throw "No se pudo generar libJWPLC_Display.a" }
    Write-Host ("Display archive: {0} bytes" -f (Get-Item -LiteralPath $DisplayArchivePath).Length) -ForegroundColor Green

    Write-Host "[2/3] Cold P3 determinista..." -ForegroundColor Cyan
    if (Test-Path -LiteralPath $p3Build) { Remove-Item -LiteralPath $p3Build -Recurse -Force }
    New-Item -ItemType Directory -Path $p3Build -Force | Out-Null

    $args = @(
        "compile",
        "-b", "jwplc_local:esp32:jwplcbasic",
        "-j", $Jobs.ToString(),
        "-v",
        "--build-path", $p3Build,
        "--clean",
        $SketchPath
    )
    Write-Host ("arduino-cli {0}" -f ($args -join " ")) -ForegroundColor DarkGray

    $sw = [System.Diagnostics.Stopwatch]::StartNew()
    $native = Invoke-NativeCaptured -FilePath $ArduinoCli -Arguments $args
    $sw.Stop()
    @($native.Output) | Out-File -LiteralPath $p3Log -Encoding utf8
    if ($native.ExitCode -ne 0)
    {
        @($native.Output | Select-Object -Last 25) | ForEach-Object { Write-Host $_ -ForegroundColor DarkYellow }
        throw "Arduino CLI fallo. Revisar $p3Log"
    }

    $p3Metrics = Get-BuildMetrics -Output $native.Output
    Assert-DeterministicLibraries -Output $native.Output -Label "P3"
    if ($p3Metrics.DisplaySource -ne 0) { throw ("P3 recompilo {0} TUs Display." -f $p3Metrics.DisplaySource) }
    if ($p3Metrics.Stub -ne 1) { throw ("P3 esperaba 1 core stub, obtuvo {0}." -f $p3Metrics.Stub) }

    $mapPath = Join-Path $p3Build "01_empty.ino.map"
    if (-not (Test-Path -LiteralPath $mapPath)) { throw "No se genero map P3." }
    $map = Get-Content -LiteralPath $mapPath -Raw
    $displayLinked = $map -match 'libJWPLC_Display\.a\(JWPLC_Display\.cpp\.o\)'
    $idleLinked = $map -match 'libJWPLC_Display\.a\(JWPLC_IdleScreen\.cpp\.o\)'
    if (-not $displayLinked -or -not $idleLinked) { throw "El linker no extrajo ambos objetos Display." }

    Write-Host "[3/3] Comparando estructura fuente vs P3..." -ForegroundColor Cyan
    $sourceSelections = Get-LibrarySelections -Output $sourceOutput
    $p3Selections = Get-LibrarySelections -Output $native.Output
    $selectionOnlySource = @($sourceSelections | Where-Object { $p3Selections -notcontains $_ })
    $selectionOnlyP3 = @($p3Selections | Where-Object { $sourceSelections -notcontains $_ })

    $sourceObjects = Get-ObjectTable -BuildPath $sourceBuild
    $p3Objects = Get-ObjectTable -BuildPath $p3Build
    $common = @($sourceObjects.Keys | Where-Object { $p3Objects.ContainsKey($_) })
    $hashMismatch = @($common | Where-Object { $sourceObjects[$_] -ne $p3Objects[$_] })
    $onlySource = @($sourceObjects.Keys | Where-Object { -not $p3Objects.ContainsKey($_) })
    $onlyP3 = @($p3Objects.Keys | Where-Object { -not $sourceObjects.ContainsKey($_) })

    if ($selectionOnlySource.Count -ne 0 -or $selectionOnlyP3.Count -ne 0) { throw "Cambio la seleccion de librerias entre fuente y P3." }
    if ($hashMismatch.Count -ne 0 -or $onlySource.Count -ne 0 -or $onlyP3.Count -ne 0) { throw "Cambian objetos externos a Display entre fuente y P3." }

    $sourceBin = Get-AppBin -BuildPath $sourceBuild
    $p3Bin = Get-AppBin -BuildPath $p3Build
    $appDelta = [int64]$p3Bin.Length - [int64]$sourceBin.Length

    Write-Host ""
    Write-Host "=== P3 DETERMINISTA REANUDADO ===" -ForegroundColor Cyan
    Write-Host ("Fuente : tiempo N/D (run previo) | compiles={0} | -E={1} | Display source={2}" -f $sourceMetrics.Compiles, $sourceMetrics.Preprocess, $sourceMetrics.DisplaySource)
    Write-Host ("P3     : {0:N3} s | compiles={1} | -E={2} | Display source={3}" -f $sw.Elapsed.TotalSeconds, $p3Metrics.Compiles, $p3Metrics.Preprocess, $p3Metrics.DisplaySource)
    Write-Host ("App bytes: {0} -> {1} | delta={2}" -f $sourceBin.Length, $p3Bin.Length, $appDelta)
    Write-Host ("Archive members: Display={0}, IdleScreen={1}" -f $displayLinked, $idleLinked)
    Write-Host ("Objetos externos comunes={0}, SHA distintos={1}, solo fuente={2}, solo P3={3}" -f $common.Count, $hashMismatch.Count, $onlySource.Count, $onlyP3.Count)
    Write-Host "Adafruit bundled: OK | Ethernet W5x00 bundled: OK" -ForegroundColor Green

    $summary = New-Object System.Collections.Generic.List[string]
    $summary.Add("# P3 determinista reanudado - JWPLC Basic")
    $summary.Add("")
    $summary.Add(("Run fuente reutilizado: {0}" -f (Split-Path -Leaf $runRoot)))
    $summary.Add("Fuente cold tiempo: N/D; la ejecucion original termino tras un falso negativo del parser.")
    $summary.Add(("Fuente compiles: {0}" -f $sourceMetrics.Compiles))
    $summary.Add(("Fuente preprocesados -E: {0}" -f $sourceMetrics.Preprocess))
    $summary.Add(("Fuente Display source: {0}" -f $sourceMetrics.DisplaySource))
    $summary.Add(("P3 cold: {0:N3} s" -f $sw.Elapsed.TotalSeconds))
    $summary.Add(("P3 compiles: {0}" -f $p3Metrics.Compiles))
    $summary.Add(("P3 preprocesados -E: {0}" -f $p3Metrics.Preprocess))
    $summary.Add(("P3 Display source: {0}" -f $p3Metrics.DisplaySource))
    $summary.Add(("App fuente bytes: {0}" -f $sourceBin.Length))
    $summary.Add(("App P3 bytes: {0}" -f $p3Bin.Length))
    $summary.Add(("Delta app bytes: {0}" -f $appDelta))
    $summary.Add(("Display archive member enlazado: {0}" -f $displayLinked))
    $summary.Add(("IdleScreen archive member enlazado: {0}" -f $idleLinked))
    $summary.Add(("Objetos externos SHA distintos: {0}" -f $hashMismatch.Count))
    $summary.Add(("Objetos solo fuente: {0}" -f $onlySource.Count))
    $summary.Add(("Objetos solo P3: {0}" -f $onlyP3.Count))
    $summary.Add(("Diferencias seleccion librerias: {0}" -f ($selectionOnlySource.Count + $selectionOnlyP3.Count)))
    $summary | Out-File -LiteralPath $summaryPath -Encoding ascii

    $success = $true
    Write-Host ""
    Write-Host "P3 DETERMINISTA REANUDADO: OK" -ForegroundColor Green
    Write-Host ("Resumen: {0}" -f $summaryPath)
    Write-Host "libJWPLC_Display.a queda activo para continuar con P5." -ForegroundColor Green
}
finally
{
    if (-not $success -and (Test-Path -LiteralPath $DisplayArchivePath))
    {
        Write-Host "Fallo la reanudacion; retirando Display archive para fallback fuente seguro." -ForegroundColor Yellow
        Remove-Item -LiteralPath $DisplayArchivePath -Force
    }
}
