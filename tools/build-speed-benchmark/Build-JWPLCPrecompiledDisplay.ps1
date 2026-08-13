[CmdletBinding()]
param(
    [string]$ArduinoCli = "arduino-cli",
    [int]$Jobs = 0,
    [string]$SourceBuildPath = "",
    [string]$OutputRoot = ""
)

Set-StrictMode -Version 2.0
$ErrorActionPreference = "Stop"

$ScriptRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$RepoRoot = [System.IO.Path]::GetFullPath((Join-Path $ScriptRoot "..\.."))
$PlatformRoot = Join-Path $RepoRoot "JWPLC\2.1.0"
$LibraryRoot = Join-Path $PlatformRoot "libraries\JWPLC_Display"
$ArchivePath = Join-Path $LibraryRoot "src\esp32\libJWPLC_Display.a"
$BoardsLocalPath = Join-Path $PlatformRoot "boards.local.txt"
$CoreArchivePath = Join-Path $PlatformRoot "precompiled\core\JWPLCBASIC\core.a"
$SketchPath = Join-Path $ScriptRoot "sketches\01_empty"

if ([string]::IsNullOrWhiteSpace($OutputRoot))
{
    $OutputRoot = Join-Path $ScriptRoot "p3-work"
}

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

    if (-not (Test-Path $LogPath)) { throw "No existe log para localizar gcc-ar: $LogPath" }
    foreach ($line in Get-Content $LogPath)
    {
        $candidate = $null
        if ($line -match '"(?<exe>[^"]*xtensa-esp32-elf-gcc-ar(?:\.exe)?)"')
        {
            $candidate = $Matches["exe"]
        }
        elseif ($line -match '(?<exe>\S*xtensa-esp32-elf-gcc-ar(?:\.exe)?)\s+cr')
        {
            $candidate = $Matches["exe"]
        }

        if (-not [string]::IsNullOrWhiteSpace($candidate))
        {
            $resolved = Resolve-NativeToolPath -Candidate $candidate
            if (-not [string]::IsNullOrWhiteSpace($resolved)) { return $resolved }
        }
    }
    throw "No se pudo localizar xtensa-esp32-elf-gcc-ar desde $LogPath"
}

function Get-AppBin
{
    param([string]$BuildPath)
    $bin = Get-ChildItem $BuildPath -Filter "*.ino.bin" -File | Select-Object -First 1
    if ($null -eq $bin) { throw "No se encontro .ino.bin en $BuildPath" }
    return $bin
}

function Test-PayloadEquivalent
{
    param([string]$ReferencePath, [string]$CandidatePath)

    $a = [System.IO.File]::ReadAllBytes($ReferencePath)
    $b = [System.IO.File]::ReadAllBytes($CandidatePath)
    if ($a.Length -ne $b.Length -or $a.Length -lt 256) { return $false }

    $elfHashStart = 0xB0
    $elfHashEnd = 0xCF
    $tailStart = $a.Length - 33

    for ($i = 0; $i -lt $a.Length; $i++)
    {
        if (($i -ge $elfHashStart -and $i -le $elfHashEnd) -or ($i -ge $tailStart)) { continue }
        if ($a[$i] -ne $b[$i]) { return $false }
    }
    return $true
}

function Find-LatestReusableP2Build
{
    $roots = @(
        (Join-Path $ScriptRoot "p2-verify-work"),
        (Join-Path $ScriptRoot "p2-work")
    )

    foreach ($root in $roots)
    {
        if (-not (Test-Path $root)) { continue }
        foreach ($run in @(Get-ChildItem $root -Directory | Sort-Object LastWriteTime -Descending))
        {
            $build = Join-Path $run.FullName "verify-Basic"
            $displayRoot = Join-Path $build "libraries\JWPLC_Display"
            $obj1 = Join-Path $displayRoot "JWPLC_Display.cpp.o"
            $obj2 = Join-Path $displayRoot "JWPLC_IdleScreen.cpp.o"
            $bin = Get-ChildItem $build -Filter "*.ino.bin" -File -ErrorAction SilentlyContinue | Select-Object -First 1
            $log = Join-Path $run.FullName "verify-Basic.log"

            if ((Test-Path $obj1) -and (Test-Path $obj2) -and ($null -ne $bin) -and (Test-Path $log))
            {
                return [PSCustomObject]@{ BuildPath = $build; LogPath = $log }
            }
        }
    }
    return $null
}

if ($Jobs -lt 0) { throw "Jobs debe ser 0 o mayor." }
if ($null -eq (Get-Command $ArduinoCli -ErrorAction SilentlyContinue)) { throw "No se encontro arduino-cli." }
if (-not (Test-Path $CoreArchivePath)) { throw "P3 requiere el core P2 validado: $CoreArchivePath" }
if (-not (Test-Path $BoardsLocalPath)) { throw "P3 requiere overlay P2 activo en boards.local.txt." }
$boardsLocal = Get-Content $BoardsLocalPath -Raw
if ($boardsLocal -notmatch '(?m)^jwplcbasic\.build\.core=jwcontrol_p2\s*$')
{
    throw "P3 requiere jwplcbasic.build.core=jwcontrol_p2. Ejecuta Verify-JWPLCPrecompiledCore.ps1 primero."
}

$propertiesPath = Join-Path $LibraryRoot "library.properties"
if ((Get-Content $propertiesPath -Raw) -notmatch '(?m)^precompiled=full\s*$')
{
    throw "JWPLC_Display no declara precompiled=full. Ejecuta git pull."
}

$sourceInfo = $null
if ([string]::IsNullOrWhiteSpace($SourceBuildPath))
{
    $sourceInfo = Find-LatestReusableP2Build
    if ($null -eq $sourceInfo) { throw "No se encontro un build P2 reutilizable con los objetos de JWPLC_Display." }
    $SourceBuildPath = $sourceInfo.BuildPath
}
else
{
    $SourceBuildPath = (Resolve-Path $SourceBuildPath).Path
    $runDir = Split-Path -Parent $SourceBuildPath
    $sourceInfo = [PSCustomObject]@{ BuildPath = $SourceBuildPath; LogPath = (Join-Path $runDir "verify-Basic.log") }
}

$displayObjects = @(
    (Join-Path $SourceBuildPath "libraries\JWPLC_Display\JWPLC_Display.cpp.o"),
    (Join-Path $SourceBuildPath "libraries\JWPLC_Display\JWPLC_IdleScreen.cpp.o")
)
foreach ($obj in $displayObjects)
{
    if (-not (Test-Path $obj)) { throw "Falta objeto P3: $obj" }
}

$referenceBin = Get-AppBin -BuildPath $SourceBuildPath
$archiver = Find-Archiver -LogPath $sourceInfo.LogPath
$archiveDir = Split-Path -Parent $ArchivePath
New-Item -ItemType Directory -Path $archiveDir -Force | Out-Null
if (Test-Path $ArchivePath) { Remove-Item $ArchivePath -Force }

Write-Host "P3 - JWPLC_Display precompilado" -ForegroundColor Cyan
Write-Host ("Objetos reutilizados desde: {0}" -f $SourceBuildPath)
Write-Host ("Archiver: {0}" -f $archiver)
Write-Host ""
Write-Host "[1/2] Generando libJWPLC_Display.a sin recompilar fuentes..." -ForegroundColor Cyan

$archiveArgs = @("crs", $ArchivePath) + $displayObjects
$arResult = Invoke-NativeCaptured -FilePath $archiver -Arguments $archiveArgs
if ($arResult.ExitCode -ne 0 -or -not (Test-Path $ArchivePath))
{
    throw "No se pudo generar $ArchivePath"
}

$archiveFile = Get-Item $ArchivePath
$archiveSHA = (Get-FileHash $ArchivePath -Algorithm SHA256).Hash.ToLowerInvariant()
Write-Host ("Archive: {0} bytes | SHA-256 {1}" -f $archiveFile.Length, $archiveSHA) -ForegroundColor Green

$runId = (Get-Date).ToString("yyyyMMdd_HHmmss")
$runRoot = Join-Path $OutputRoot $runId
$buildPath = Join-Path $runRoot "verify-Basic"
$logPath = Join-Path $runRoot "verify-Basic.log"
$summaryPath = Join-Path $runRoot "P3_SUMMARY.md"
New-Item -ItemType Directory -Path $buildPath -Force | Out-Null

$success = $false
try
{
    Write-Host ""
    Write-Host "[2/2] Cold build limpio con P2 + P3..." -ForegroundColor Cyan
    $args = @("compile", "-b", "jwplc_local:esp32:jwplcbasic", "-j", $Jobs.ToString(), "-v", "--build-path", $buildPath, "--clean", $SketchPath)
    Write-Host ("arduino-cli {0}" -f ($args -join " ")) -ForegroundColor DarkGray

    $sw = [System.Diagnostics.Stopwatch]::StartNew()
    $native = Invoke-NativeCaptured -FilePath $ArduinoCli -Arguments $args
    $sw.Stop()
    @($native.Output) | Out-File $logPath -Encoding utf8
    if ($native.ExitCode -ne 0) { throw "Arduino CLI fallo. Revisar $logPath" }

    $compileLines = @($native.Output | Where-Object { ([string]$_) -match '-MMD\s+-c\s' })
    $allCompiles = $compileLines.Count
    $displaySourcePattern = '"[^"]*[\\/]+libraries[\\/]+JWPLC_Display[\\/]+[^"]+\.cpp"\s+-o\s+"'
    $displaySourceCompiles = @($compileLines | Where-Object { ([string]$_) -match $displaySourcePattern }).Count
    $stubPattern = '"[^"]*[\\/]+cores[\\/]+jwcontrol_p2[\\/]+p2_core_stub\.c"\s+-o\s+"'
    $stubCompiles = @($compileLines | Where-Object { ([string]$_) -match $stubPattern }).Count

    $preprocessLines = @($native.Output | Where-Object { ([string]$_) -match 'xtensa-esp32-elf-g\+\+.*\s-E\s' })
    $preprocessPasses = $preprocessLines.Count
    $displayPreprocessPasses = @($preprocessLines | Where-Object { ([string]$_) -match 'JWPLC_Display\.cpp' }).Count

    $candidateBin = Get-AppBin -BuildPath $buildPath
    $sameBytes = ($candidateBin.Length -eq $referenceBin.Length)
    $sameRawSHA = ((Get-FileHash $candidateBin.FullName -Algorithm SHA256).Hash -eq (Get-FileHash $referenceBin.FullName -Algorithm SHA256).Hash)
    $samePayload = Test-PayloadEquivalent -ReferencePath $referenceBin.FullName -CandidatePath $candidateBin.FullName

    Write-Host ""
    Write-Host ("Tiempo: {0:N3} s" -f $sw.Elapsed.TotalSeconds) -ForegroundColor Green
    Write-Host ("Compilaciones: total={0}, JWPLC_Display fuente={1}, core stub={2}" -f $allCompiles, $displaySourceCompiles, $stubCompiles) -ForegroundColor Green
    Write-Host ("Preprocesados -E: total={0}, JWPLC_Display.cpp={1}" -f $preprocessPasses, $displayPreprocessPasses) -ForegroundColor Green
    Write-Host ("App bytes iguales={0}, SHA raw igual={1}, payload igual={2}" -f $sameBytes, $sameRawSHA, $samePayload) -ForegroundColor DarkGray

    if ($displaySourceCompiles -ne 0) { throw "P3 todavia compilo fuentes de JWPLC_Display." }
    if ($stubCompiles -ne 1) { throw ("P3 esperaba 1 core stub, obtuvo {0}." -f $stubCompiles) }
    if (-not $sameBytes) { throw "P3 cambio el tamano de la app." }
    if (-not $samePayload) { throw "P3 cambio bytes de payload fuera de hashes." }

    $lines = New-Object System.Collections.Generic.List[string]
    $lines.Add("# P3 - JWPLC_Display precompilado")
    $lines.Add("")
    $lines.Add(("Objetos reutilizados: {0}" -f $SourceBuildPath))
    $lines.Add(("Archive bytes: {0}" -f $archiveFile.Length))
    $lines.Add(("Archive SHA-256: {0}" -f $archiveSHA))
    $lines.Add(("Cold build: {0:N3} s" -f $sw.Elapsed.TotalSeconds))
    $lines.Add(("Compiles total: {0}" -f $allCompiles))
    $lines.Add(("Display fuente: {0}" -f $displaySourceCompiles))
    $lines.Add(("Core stub: {0}" -f $stubCompiles))
    $lines.Add(("Preprocesados -E total: {0}" -f $preprocessPasses))
    $lines.Add(("Preprocesados JWPLC_Display.cpp: {0}" -f $displayPreprocessPasses))
    $lines.Add(("App bytes iguales: {0}" -f $sameBytes))
    $lines.Add(("SHA raw igual: {0}" -f $sameRawSHA))
    $lines.Add(("Payload igual fuera de hashes: {0}" -f $samePayload))
    $lines | Out-File $summaryPath -Encoding ascii

    $success = $true
    Write-Host ""
    Write-Host "P3 generado y verificado correctamente." -ForegroundColor Green
    Write-Host ("Resumen: {0}" -f $summaryPath)
    Write-Host "libJWPLC_Display.a queda activo para el siguiente benchmark." -ForegroundColor Green
}
finally
{
    if (-not $success -and (Test-Path $ArchivePath))
    {
        Write-Host "P3 fallo; eliminando libJWPLC_Display.a para volver al fallback fuente." -ForegroundColor Yellow
        Remove-Item $ArchivePath -Force
    }
}
