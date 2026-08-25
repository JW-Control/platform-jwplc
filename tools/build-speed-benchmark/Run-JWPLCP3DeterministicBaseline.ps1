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
$OutputRoot = Join-Path $ScriptRoot "p3-deterministic-work"

$P1Libraries = @(
    "JW_RTC",
    "JW_FRAM",
    "JW_SD",
    "JW_MatrixButtons",
    "JWPLC_ModbusRTU"
)

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

    $lines = @()
    if (Test-Path -LiteralPath $LogPath)
    {
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

function Invoke-ColdBuild
{
    param(
        [string]$BuildPath,
        [string]$LogPath
    )

    if (Test-Path -LiteralPath $BuildPath) { Remove-Item -LiteralPath $BuildPath -Recurse -Force }
    New-Item -ItemType Directory -Path $BuildPath -Force | Out-Null

    $args = @(
        "compile",
        "-b", "jwplc_local:esp32:jwplcbasic",
        "-j", $Jobs.ToString(),
        "-v",
        "--build-path", $BuildPath,
        "--clean",
        $SketchPath
    )

    Write-Host ("arduino-cli {0}" -f ($args -join " ")) -ForegroundColor DarkGray
    $sw = [System.Diagnostics.Stopwatch]::StartNew()
    $native = Invoke-NativeCaptured -FilePath $ArduinoCli -Arguments $args
    $sw.Stop()
    @($native.Output) | Out-File -LiteralPath $LogPath -Encoding utf8

    if ($native.ExitCode -ne 0)
    {
        @($native.Output | Select-Object -Last 25) | ForEach-Object { Write-Host $_ -ForegroundColor DarkYellow }
        throw "Arduino CLI fallo. Revisar $LogPath"
    }

    return [PSCustomObject]@{
        Output = @($native.Output)
        Seconds = $sw.Elapsed.TotalSeconds
    }
}

function Get-BuildMetrics
{
    param([object[]]$Output)

    $compileLines = @($Output | Where-Object { ([string]$_) -match '-MMD\s+-c\s' })
    $preprocessLines = @($Output | Where-Object { ([string]$_) -match 'xtensa-esp32-elf-g\+\+.*\s-E\s' })

    return [PSCustomObject]@{
        Compiles = $compileLines.Count
        Preprocess = $preprocessLines.Count
        DisplaySource = @($compileLines | Where-Object { ([string]$_) -match 'libraries[\\/]+JWPLC_Display[\\/]+.*\.cpp"\s+-o\s+"' }).Count
        DisplayE = @($preprocessLines | Where-Object { ([string]$_) -match 'JWPLC_Display\.cpp' }).Count
        Stub = @($compileLines | Where-Object { ([string]$_) -match 'jwcontrol_precompiled_stub[\\/]+p2_core_stub\.c' }).Count
    }
}

function Get-LibrarySelections
{
    param([object[]]$Output)
    return @($Output | Where-Object { ([string]$_) -like "Using library *" } | ForEach-Object { ([string]$_).Trim() } | Sort-Object -Unique)
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
            throw ("{0}: {1} no proviene del package. Linea: {2}" -f $Label, $item.Name, $line[0])
        }
    }

    $wrongEthernet = @($Output | Where-Object { ([string]$_) -match '^Using library Ethernet at version ' })
    if ($wrongEthernet.Count -gt 0)
    {
        throw ("{0}: se selecciono una libreria homonima Ethernet inesperada: {1}" -f $Label, $wrongEthernet[0])
    }

    $userExternal = @($Output | Where-Object {
        ([string]$_) -like "Using library *" -and
        (([string]$_) -match '[\\/]Arduino[\\/]libraries[\\/](Adafruit|Ethernet)')
    })
    if ($userExternal.Count -gt 0)
    {
        throw ("{0}: se seleccionaron dependencias del sketchbook: {1}" -f $Label, ($userExternal -join ' | '))
    }
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

if ($Jobs -lt 0) { throw "Jobs debe ser 0 o mayor." }
if ($null -eq (Get-Command $ArduinoCli -ErrorAction SilentlyContinue)) { throw "No se encontro arduino-cli." }

if (-not (Test-Path -LiteralPath $BoardsLocalPath)) { throw "Falta boards.local.txt con overlay P2 activo." }
$boardsText = Get-Content -LiteralPath $BoardsLocalPath -Raw
if ($boardsText -notmatch '(?m)^jwplcbasic\.build\.core=jwcontrol_precompiled_stub\s*$')
{
    throw "P3 determinista requiere jwplcbasic.build.core=jwcontrol_precompiled_stub."
}
if (-not (Test-Path -LiteralPath $CoreArchivePath)) { throw "Falta core P2: $CoreArchivePath" }

foreach ($lib in $P1Libraries)
{
    $archive = Join-Path $LibrariesRoot ($lib + "\src\esp32\lib" + $lib + ".a")
    if (-not (Test-Path -LiteralPath $archive)) { throw ("Falta archive P1: {0}" -f $archive) }
}

$displayProps = Join-Path $LibrariesRoot "JWPLC_Display\library.properties"
if ((Get-Content -LiteralPath $displayProps -Raw) -notmatch '(?m)^precompiled=full\s*$')
{
    throw "JWPLC_Display no declara precompiled=full."
}

$ethernetProps = Join-Path $LibrariesRoot "JWPLC_Ethernet\library.properties"
if ((Get-Content -LiteralPath $ethernetProps -Raw) -notmatch '(?m)^depends=JWPLC Ethernet W5x00 Backend\s*$')
{
    throw "JWPLC_Ethernet no esta fijado al backend W5x00 bundled."
}

$runId = (Get-Date).ToString("yyyyMMdd_HHmmss")
$runRoot = Join-Path $OutputRoot $runId
$sourceBuild = Join-Path $runRoot "source-Basic"
$sourceLog = Join-Path $runRoot "source-Basic.log"
$p3Build = Join-Path $runRoot "p3-Basic"
$p3Log = Join-Path $runRoot "p3-Basic.log"
$summaryPath = Join-Path $runRoot "P3_DETERMINISTIC_SUMMARY.md"
New-Item -ItemType Directory -Path $runRoot -Force | Out-Null

$success = $false
try
{
    if (Test-Path -LiteralPath $DisplayArchivePath)
    {
        Write-Host "Retirando archive Display previo para obtener fuente determinista..." -ForegroundColor Yellow
        Remove-Item -LiteralPath $DisplayArchivePath -Force
    }

    Write-Host "P3 determinista - Adafruit + Ethernet bundled" -ForegroundColor Cyan
    Write-Host "Ejecutara dos cold builds limpios." -ForegroundColor Yellow
    Write-Host ""

    Write-Host "[1/4] Cold fuente: P1 + P2, Display desde source..." -ForegroundColor Cyan
    $source = Invoke-ColdBuild -BuildPath $sourceBuild -LogPath $sourceLog
    $sourceMetrics = Get-BuildMetrics -Output $source.Output
    Assert-DeterministicLibraries -Output $source.Output -Label "Fuente"
    if ($sourceMetrics.DisplaySource -ne 2) { throw ("Fuente esperaba 2 TUs Display, obtuvo {0}." -f $sourceMetrics.DisplaySource) }
    if ($sourceMetrics.Stub -ne 1) { throw ("Fuente esperaba 1 core stub, obtuvo {0}." -f $sourceMetrics.Stub) }

    Write-Host ("Fuente: {0:N3} s | compiles={1} | -E={2}" -f $source.Seconds, $sourceMetrics.Compiles, $sourceMetrics.Preprocess) -ForegroundColor Green

    Write-Host "[2/4] Generando JWPLC_Display.a desde objetos deterministas..." -ForegroundColor Cyan
    $displayObjects = @(
        (Join-Path $sourceBuild "libraries\JWPLC_Display\JWPLC_Display.cpp.o"),
        (Join-Path $sourceBuild "libraries\JWPLC_Display\JWPLC_IdleScreen.cpp.o")
    )
    foreach ($obj in $displayObjects) { if (-not (Test-Path -LiteralPath $obj)) { throw "Falta objeto Display: $obj" } }

    $archiver = Find-Archiver -LogPath $sourceLog
    $archiveDir = Split-Path -Parent $DisplayArchivePath
    New-Item -ItemType Directory -Path $archiveDir -Force | Out-Null
    $ar = Invoke-NativeCaptured -FilePath $archiver -Arguments (@("crs", $DisplayArchivePath) + $displayObjects)
    if ($ar.ExitCode -ne 0 -or -not (Test-Path -LiteralPath $DisplayArchivePath)) { throw "No se pudo generar libJWPLC_Display.a" }
    Write-Host ("Display archive: {0} bytes" -f (Get-Item -LiteralPath $DisplayArchivePath).Length) -ForegroundColor Green

    Write-Host "[3/4] Cold P3 determinista..." -ForegroundColor Cyan
    $p3 = Invoke-ColdBuild -BuildPath $p3Build -LogPath $p3Log
    $p3Metrics = Get-BuildMetrics -Output $p3.Output
    Assert-DeterministicLibraries -Output $p3.Output -Label "P3"

    if ($p3Metrics.DisplaySource -ne 0) { throw "P3 recompilo fuentes de JWPLC_Display." }
    if ($p3Metrics.Stub -ne 1) { throw ("P3 esperaba 1 core stub, obtuvo {0}." -f $p3Metrics.Stub) }

    $mapPath = Join-Path $p3Build "01_empty.ino.map"
    if (-not (Test-Path -LiteralPath $mapPath)) { throw "No se genero map P3." }
    $map = Get-Content -LiteralPath $mapPath -Raw
    $displayObjectLinked = $map -match 'libJWPLC_Display\.a\(JWPLC_Display\.cpp\.o\)'
    $idleObjectLinked = $map -match 'libJWPLC_Display\.a\(JWPLC_IdleScreen\.cpp\.o\)'
    if (-not $displayObjectLinked -or -not $idleObjectLinked) { throw "El linker no extrajo ambos objetos del archive Display." }

    Write-Host "[4/4] Comparando estructura fuente vs P3..." -ForegroundColor Cyan
    $sourceSelections = Get-LibrarySelections -Output $source.Output
    $p3Selections = Get-LibrarySelections -Output $p3.Output
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
    $timeDelta = $p3.Seconds - $source.Seconds
    $timePct = if ($source.Seconds -gt 0) { 100.0 * $timeDelta / $source.Seconds } else { 0.0 }

    Write-Host ""
    Write-Host "=== P3 DETERMINISTA ===" -ForegroundColor Cyan
    Write-Host ("Fuente : {0:N3} s | compiles={1} | -E={2} | Display source={3}" -f $source.Seconds, $sourceMetrics.Compiles, $sourceMetrics.Preprocess, $sourceMetrics.DisplaySource)
    Write-Host ("P3     : {0:N3} s | compiles={1} | -E={2} | Display source={3}" -f $p3.Seconds, $p3Metrics.Compiles, $p3Metrics.Preprocess, $p3Metrics.DisplaySource)
    Write-Host ("Delta tiempo P3 vs fuente: {0:+0.000;-0.000;0.000} s ({1:+0.00;-0.00;0.00}%)" -f $timeDelta, $timePct)
    Write-Host ("App bytes: {0} -> {1} | delta={2}" -f $sourceBin.Length, $p3Bin.Length, $appDelta)
    Write-Host ("Archive members: Display={0}, IdleScreen={1}" -f $displayObjectLinked, $idleObjectLinked)
    Write-Host ("Objetos externos comunes={0}, SHA distintos={1}, solo fuente={2}, solo P3={3}" -f $common.Count, $hashMismatch.Count, $onlySource.Count, $onlyP3.Count)
    Write-Host "Adafruit bundled: OK | Ethernet W5x00 bundled: OK" -ForegroundColor Green

    $summary = New-Object System.Collections.Generic.List[string]
    $summary.Add("# P3 determinista - JWPLC Basic")
    $summary.Add("")
    $summary.Add("Dependencias forzadas desde el package: Adafruit ST77xx, Adafruit GFX, Adafruit BusIO y JWPLC Ethernet W5x00 Backend 2.0.2.")
    $summary.Add("")
    $summary.Add(("Fuente cold: {0:N3} s" -f $source.Seconds))
    $summary.Add(("Fuente compiles: {0}" -f $sourceMetrics.Compiles))
    $summary.Add(("Fuente preprocesados -E: {0}" -f $sourceMetrics.Preprocess))
    $summary.Add(("P3 cold: {0:N3} s" -f $p3.Seconds))
    $summary.Add(("P3 compiles: {0}" -f $p3Metrics.Compiles))
    $summary.Add(("P3 preprocesados -E: {0}" -f $p3Metrics.Preprocess))
    $summary.Add(("Delta tiempo P3 vs fuente: {0:N3} s ({1:N2}%)" -f $timeDelta, $timePct))
    $summary.Add(("App fuente bytes: {0}" -f $sourceBin.Length))
    $summary.Add(("App P3 bytes: {0}" -f $p3Bin.Length))
    $summary.Add(("Delta app bytes: {0}" -f $appDelta))
    $summary.Add(("Display archive member enlazado: {0}" -f $displayObjectLinked))
    $summary.Add(("IdleScreen archive member enlazado: {0}" -f $idleObjectLinked))
    $summary.Add(("Objetos externos SHA distintos: {0}" -f $hashMismatch.Count))
    $summary.Add(("Objetos solo fuente: {0}" -f $onlySource.Count))
    $summary.Add(("Objetos solo P3: {0}" -f $onlyP3.Count))
    $summary.Add(("Diferencias seleccion librerias: {0}" -f ($selectionOnlySource.Count + $selectionOnlyP3.Count)))
    $summary | Out-File -LiteralPath $summaryPath -Encoding ascii

    $success = $true
    Write-Host ""
    Write-Host "P3 DETERMINISTA: OK" -ForegroundColor Green
    Write-Host ("Resumen: {0}" -f $summaryPath)
    Write-Host "libJWPLC_Display.a queda activo para continuar con P5." -ForegroundColor Green
}
finally
{
    if (-not $success -and (Test-Path -LiteralPath $DisplayArchivePath))
    {
        Write-Host "Fallo el baseline; retirando Display archive para fallback fuente seguro." -ForegroundColor Yellow
        Remove-Item -LiteralPath $DisplayArchivePath -Force
    }
}
