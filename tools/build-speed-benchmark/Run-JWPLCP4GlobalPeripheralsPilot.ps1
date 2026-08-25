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
$SketchPath = Join-Path $ScriptRoot "sketches\01_empty"
$BoardsLocalPath = Join-Path $PlatformRoot "boards.local.txt"
$DisplayArchive = Join-Path $PlatformRoot "libraries\JWPLC_Display\src\esp32\libJWPLC_Display.a"
$GlobalArchive = Join-Path $PlatformRoot "libraries\JWPLC_GlobalPeripherals\src\esp32\libJWPLC_GlobalPeripherals.a"
$OutputRoot = Join-Path $ScriptRoot "p4-work"

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

    $lines = @()
    if (-not [string]::IsNullOrWhiteSpace($LogPath) -and (Test-Path -LiteralPath $LogPath))
    {
        $lines = @(Get-Content -LiteralPath $LogPath)

        # 1) Preferir una invocacion real de gcc-ar si el log la contiene.
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

        # 2) Un build puede no ejecutar gcc-ar. En ese caso tomar cualquier
        #    compilador xtensa del mismo toolchain y resolver el gcc-ar hermano.
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

    # 3) Ultimo fallback para Windows: buscar el tool instalado por Arduino.
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

    throw ("No se pudo localizar xtensa-esp32-elf-gcc-ar. Log inspeccionado: {0}" -f $LogPath)
}

function Find-LatestBuild
{
    param([string]$RootName, [string]$BuildName, [string[]]$RequiredRelativePaths)
    $root = Join-Path $ScriptRoot $RootName
    if (-not (Test-Path $root)) { return $null }
    foreach ($run in @(Get-ChildItem $root -Directory | Sort-Object LastWriteTime -Descending))
    {
        $build = Join-Path $run.FullName $BuildName
        if (-not (Test-Path $build)) { continue }
        $ok = $true
        foreach ($relative in $RequiredRelativePaths)
        {
            if (-not (Test-Path (Join-Path $build $relative))) { $ok = $false; break }
        }
        if ($ok)
        {
            $log = Join-Path $run.FullName ($BuildName + ".log")
            if (-not (Test-Path $log)) { $log = Get-ChildItem $run.FullName -Filter "*.log" -File | Select-Object -First 1 -ExpandProperty FullName }
            return [PSCustomObject]@{ BuildPath = $build; LogPath = $log }
        }
    }
    return $null
}

function New-Archive
{
    param([string]$Archiver, [string]$ArchivePath, [string[]]$Objects)
    $dir = Split-Path -Parent $ArchivePath
    New-Item -ItemType Directory -Path $dir -Force | Out-Null
    if (Test-Path $ArchivePath) { Remove-Item $ArchivePath -Force }
    foreach ($obj in $Objects) { if (-not (Test-Path $obj)) { throw "Falta objeto: $obj" } }
    $result = Invoke-NativeCaptured $Archiver (@("crs", $ArchivePath) + $Objects)
    if ($result.ExitCode -ne 0 -or -not (Test-Path $ArchivePath)) { throw "No se pudo generar $ArchivePath" }
}

function Get-LibrarySelections
{
    param([string]$LogPath)
    return @(Get-Content $LogPath | Where-Object { $_ -like "Using library *" } | ForEach-Object { $_.Trim() } | Sort-Object -Unique)
}

function Get-ObjectTable
{
    param([string]$BuildPath)
    $root = Join-Path $BuildPath "libraries"
    $table = @{}
    if (-not (Test-Path $root)) { return $table }
    foreach ($file in Get-ChildItem $root -Recurse -File -Filter "*.o")
    {
        $relative = $file.FullName.Substring($root.Length).TrimStart('\','/')
        if ($relative -match '^JWPLC_Display[\\/]' -or $relative -match '^JWPLC_GlobalPeripherals[\\/]') { continue }
        $table[$relative] = (Get-FileHash $file.FullName -Algorithm SHA256).Hash.ToLowerInvariant()
    }
    return $table
}

if ($Jobs -lt 0) { throw "Jobs debe ser 0 o mayor." }
if ($null -eq (Get-Command $ArduinoCli -ErrorAction SilentlyContinue)) { throw "No se encontro arduino-cli." }
if (-not (Test-Path $BoardsLocalPath)) { throw "P4 requiere overlay P2 activo. Ejecuta Verify-JWPLCPrecompiledCore.ps1 si fue retirado." }
$boardsText = Get-Content $BoardsLocalPath -Raw
if ($boardsText -notmatch '(?m)^jwplcbasic\.build\.core=jwcontrol_precompiled_stub\s*$') { throw "P4 requiere jwplcbasic.build.core=jwcontrol_precompiled_stub." }

$p2 = Find-LatestBuild "p2-verify-work" "verify-Basic" @(
    "libraries\JWPLC_Display\JWPLC_Display.cpp.o",
    "libraries\JWPLC_Display\JWPLC_IdleScreen.cpp.o",
    "libraries\JWPLC_GlobalPeripherals\JWPLC_GlobalPeripherals.cpp.o"
)
if ($null -eq $p2) { throw "No se encontro build P2 reutilizable." }

$p3 = Find-LatestBuild "p3-work" "verify-Basic" @("01_empty.ino.map")
if ($null -eq $p3) { throw "No se encontro build P3 estructuralmente validado." }

$archiver = Find-Archiver $p2.LogPath
$displayObjects = @(
    (Join-Path $p2.BuildPath "libraries\JWPLC_Display\JWPLC_Display.cpp.o"),
    (Join-Path $p2.BuildPath "libraries\JWPLC_Display\JWPLC_IdleScreen.cpp.o")
)
$globalObjects = @((Join-Path $p2.BuildPath "libraries\JWPLC_GlobalPeripherals\JWPLC_GlobalPeripherals.cpp.o"))

Write-Host "P4 - piloto GlobalPeripherals precompilado (Basic only)" -ForegroundColor Cyan
Write-Host ("P2 objetos: {0}" -f $p2.BuildPath)
Write-Host ("P3 referencia: {0}" -f $p3.BuildPath)
Write-Host ("Archiver: {0}" -f $archiver)
Write-Host "Los archives P3/P4 se retiraran al finalizar para no contaminar Basic Core." -ForegroundColor Yellow

$runId = (Get-Date).ToString("yyyyMMdd_HHmmss")
$runRoot = Join-Path $OutputRoot $runId
$buildPath = Join-Path $runRoot "verify-Basic"
$logPath = Join-Path $runRoot "verify-Basic.log"
$summaryPath = Join-Path $runRoot "P4_SUMMARY.md"
New-Item -ItemType Directory -Path $buildPath -Force | Out-Null

try
{
    Write-Host ""
    Write-Host "[1/3] Restaurando archive P3 de Display..." -ForegroundColor Cyan
    New-Archive $archiver $DisplayArchive $displayObjects
    Write-Host ("Display archive: {0} bytes" -f (Get-Item $DisplayArchive).Length) -ForegroundColor Green

    Write-Host "[2/3] Generando archive P4 de GlobalPeripherals para Basic..." -ForegroundColor Cyan
    New-Archive $archiver $GlobalArchive $globalObjects
    Write-Host ("GlobalPeripherals archive: {0} bytes" -f (Get-Item $GlobalArchive).Length) -ForegroundColor Green

    Write-Host "[3/3] Cold build limpio P2 + P3 + P4..." -ForegroundColor Cyan
    $args = @("compile", "-b", "jwplc_local:esp32:jwplcbasic", "-j", $Jobs.ToString(), "-v", "--build-path", $buildPath, "--clean", $SketchPath)
    Write-Host ("arduino-cli {0}" -f ($args -join " ")) -ForegroundColor DarkGray
    $sw = [System.Diagnostics.Stopwatch]::StartNew()
    $native = Invoke-NativeCaptured $ArduinoCli $args
    $sw.Stop()
    @($native.Output) | Out-File $logPath -Encoding utf8
    if ($native.ExitCode -ne 0)
    {
        @($native.Output | Select-Object -Last 20) | ForEach-Object { Write-Host $_ -ForegroundColor DarkYellow }
        throw "Arduino CLI fallo. Revisar $logPath"
    }

    $compileLines = @($native.Output | Where-Object { ([string]$_) -match '-MMD\s+-c\s' })
    $preprocessLines = @($native.Output | Where-Object { ([string]$_) -match 'xtensa-esp32-elf-g\+\+.*\s-E\s' })
    $globalSource = @($compileLines | Where-Object { ([string]$_) -match 'JWPLC_GlobalPeripherals\.cpp"\s+-o\s+"' }).Count
    $displaySource = @($compileLines | Where-Object { ([string]$_) -match 'libraries[\\/]JWPLC_Display[\\/].*\.cpp"\s+-o\s+"' }).Count
    $stub = @($compileLines | Where-Object { ([string]$_) -match 'jwcontrol_precompiled_stub[\\/]p2_core_stub\.c"\s+-o\s+"' }).Count
    $globalE = @($preprocessLines | Where-Object { ([string]$_) -match 'JWPLC_GlobalPeripherals\.cpp' }).Count
    $displayE = @($preprocessLines | Where-Object { ([string]$_) -match 'JWPLC_Display\.cpp' }).Count

    $mapPath = Join-Path $buildPath "01_empty.ino.map"
    if (-not (Test-Path $mapPath)) { throw "No se genero map P4." }
    $map = Get-Content $mapPath -Raw
    $globalLinked = $map -match 'libJWPLC_GlobalPeripherals\.a\(JWPLC_GlobalPeripherals\.cpp\.o\)'
    $displayLinked = ($map -match 'libJWPLC_Display\.a\(JWPLC_Display\.cpp\.o\)') -and ($map -match 'libJWPLC_Display\.a\(JWPLC_IdleScreen\.cpp\.o\)')

    $p3Selections = Get-LibrarySelections $p3.LogPath
    $p4Selections = Get-LibrarySelections $logPath
    $selectionDiff = @($p3Selections | Where-Object { $p4Selections -notcontains $_ }).Count + @($p4Selections | Where-Object { $p3Selections -notcontains $_ }).Count

    $refObjects = Get-ObjectTable $p3.BuildPath
    $candObjects = Get-ObjectTable $buildPath
    $common = @($refObjects.Keys | Where-Object { $candObjects.ContainsKey($_) })
    $hashMismatch = @($common | Where-Object { $refObjects[$_] -ne $candObjects[$_] }).Count
    $onlyRef = @($refObjects.Keys | Where-Object { -not $candObjects.ContainsKey($_) }).Count
    $onlyCand = @($candObjects.Keys | Where-Object { -not $refObjects.ContainsKey($_) }).Count

    $p3Bin = Get-ChildItem $p3.BuildPath -Filter "*.ino.bin" -File | Select-Object -First 1
    $p4Bin = Get-ChildItem $buildPath -Filter "*.ino.bin" -File | Select-Object -First 1
    $sizeDelta = [int64]$p4Bin.Length - [int64]$p3Bin.Length

    Write-Host ""
    Write-Host ("Tiempo cold: {0:N3} s" -f $sw.Elapsed.TotalSeconds) -ForegroundColor Green
    Write-Host ("Compilaciones: total={0}, GlobalPeripherals fuente={1}, Display fuente={2}, core stub={3}" -f $compileLines.Count, $globalSource, $displaySource, $stub) -ForegroundColor Green
    Write-Host ("Preprocesados -E: total={0}, GlobalPeripherals.cpp={1}, Display.cpp={2}" -f $preprocessLines.Count, $globalE, $displayE) -ForegroundColor Green
    Write-Host ("Archives enlazados: GlobalPeripherals={0}, Display={1}" -f $globalLinked, $displayLinked)
    Write-Host ("Objetos comunes fuera de P3/P4: {0}, SHA distintos={1}, solo ref={2}, solo cand={3}" -f $common.Count, $hashMismatch, $onlyRef, $onlyCand)
    Write-Host ("Diferencias seleccion librerias={0}" -f $selectionDiff)
    Write-Host ("App bytes: {0} -> {1} (delta {2})" -f $p3Bin.Length, $p4Bin.Length, $sizeDelta)

    $ok = ($globalSource -eq 0) -and ($displaySource -eq 0) -and ($stub -eq 1) -and $globalLinked -and $displayLinked -and ($hashMismatch -eq 0) -and ($onlyRef -eq 0) -and ($onlyCand -eq 0) -and ($selectionDiff -eq 0)

    $lines = New-Object System.Collections.Generic.List[string]
    $lines.Add("# P4 - GlobalPeripherals precompilado, Basic only")
    $lines.Add("")
    $lines.Add(("Cold: {0:N3} s" -f $sw.Elapsed.TotalSeconds))
    $lines.Add(("Compiles total: {0}" -f $compileLines.Count))
    $lines.Add(("GlobalPeripherals fuente: {0}" -f $globalSource))
    $lines.Add(("Display fuente: {0}" -f $displaySource))
    $lines.Add(("Core stub: {0}" -f $stub))
    $lines.Add(("Preprocesados -E: {0}" -f $preprocessLines.Count))
    $lines.Add(("Preprocesados GlobalPeripherals.cpp: {0}" -f $globalE))
    $lines.Add(("Preprocesados Display.cpp: {0}" -f $displayE))
    $lines.Add(("Global archive linked: {0}" -f $globalLinked))
    $lines.Add(("Display archive linked: {0}" -f $displayLinked))
    $lines.Add(("Object SHA mismatches outside P3/P4: {0}" -f $hashMismatch))
    $lines.Add(("Library selection diff: {0}" -f $selectionDiff))
    $lines.Add(("App size delta vs P3: {0}" -f $sizeDelta))
    $lines.Add(("Resultado estructural: {0}" -f $(if ($ok) { "OK" } else { "REVISAR" })))
    $lines | Out-File $summaryPath -Encoding ascii

    if (-not $ok) { throw "P4 no paso la equivalencia estructural. Revisar $summaryPath" }
    Write-Host ""
    Write-Host "P4 piloto estructuralmente valido." -ForegroundColor Green
    Write-Host ("Resumen: {0}" -f $summaryPath)
}
finally
{
    Write-Host ""
    Write-Host "Retirando archives P3/P4 temporales para dejar fallback fuente seguro para Basic/Core..." -ForegroundColor Yellow
    if (Test-Path $GlobalArchive) { Remove-Item $GlobalArchive -Force }
    if (Test-Path $DisplayArchive) { Remove-Item $DisplayArchive -Force }
}
