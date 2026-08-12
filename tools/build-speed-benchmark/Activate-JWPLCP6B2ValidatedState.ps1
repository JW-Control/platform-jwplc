#requires -Version 7.4

[CmdletBinding()]
param()

Set-StrictMode -Version 2.0
$ErrorActionPreference = "Stop"

$ScriptRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$RepoRoot = [System.IO.Path]::GetFullPath((Join-Path $ScriptRoot "..\.."))
$PlatformRoot = Join-Path $RepoRoot "JWPLC\2.1.0"
$LibrariesRoot = Join-Path $PlatformRoot "libraries"

$GfxRoot = Join-Path $LibrariesRoot "Adafruit_GFX_Library"
$GfxSrc = Join-Path $GfxRoot "src"
$GfxProperties = Join-Path $GfxRoot "library.properties"
$GfxArchive = Join-Path $GfxSrc "esp32\libAdafruit_GFX_Library.a"
$GfxFonts = Join-Path $GfxSrc "Fonts"
$GfxMarker = Join-Path $GfxSrc "JWPLC_Bundled_Adafruit_GFX.h"

$StRoot = Join-Path $LibrariesRoot "Adafruit_ST7735_and_ST7789_Library"
$StProperties = Join-Path $StRoot "library.properties"
$StArchive = Join-Path $StRoot "src\esp32\libAdafruit_ST7735_and_ST7789_Library.a"

$InspectorPath = Join-Path $ScriptRoot "Inspect-JWPLCP6B2ExistingRun.ps1"
$BaselineRunId = "20260809_234653"
$BaselineRoot = Join-Path (Join-Path $ScriptRoot "p6b-gfx-layout-work") $BaselineRunId
$BaselineBuild = Join-Path $BaselineRoot "p6b-layout-Basic"
$BaselineLog = Join-Path $BaselineRoot "p6b-layout-Basic.log"

$ExpectedObjects = @(
    "Adafruit_GFX.cpp.o",
    "Adafruit_GrayOLED.cpp.o",
    "Adafruit_SPITFT.cpp.o",
    "glcdfont.c.o"
)

function Invoke-NativeCaptured
{
    param(
        [Parameter(Mandatory = $true)][string]$FilePath,
        [Parameter(Mandatory = $true)][string[]]$Arguments
    )

    $oldErrorAction = $ErrorActionPreference
    $hadNativePreference = Test-Path variable:global:PSNativeCommandUseErrorActionPreference
    if ($hadNativePreference) { $oldNativePreference = $global:PSNativeCommandUseErrorActionPreference }

    $output = @()
    $exitCode = -1
    try
    {
        $ErrorActionPreference = "Continue"
        if ($hadNativePreference) { $global:PSNativeCommandUseErrorActionPreference = $false }
        $output = @(& $FilePath @Arguments 2>&1 | ForEach-Object { $_.ToString() })
        $exitCode = $LASTEXITCODE
    }
    finally
    {
        $ErrorActionPreference = $oldErrorAction
        if ($hadNativePreference) { $global:PSNativeCommandUseErrorActionPreference = $oldNativePreference }
    }

    return [PSCustomObject]@{ ExitCode = [int]$exitCode; Output = $output }
}

function Resolve-NativeToolPath
{
    param([Parameter(Mandatory = $true)][string]$Candidate)
    $normalized = $Candidate.Trim().Trim('"')
    while ($normalized.Contains("\\")) { $normalized = $normalized.Replace("\\", "\") }
    foreach ($path in @($normalized, ($normalized + ".exe")))
    {
        if (Test-Path -LiteralPath $path) { return (Resolve-Path -LiteralPath $path).Path }
    }
    return $null
}

function Find-Archiver
{
    param([Parameter(Mandatory = $true)][string]$LogPath)

    foreach ($line in Get-Content -LiteralPath $LogPath)
    {
        $candidate = $null
        if ($line -match '"(?<exe>[^"]*xtensa-esp32-elf-gcc-ar(?:\.exe)?)"') { $candidate = $Matches["exe"] }
        elseif ($line -match '(?<exe>\S*xtensa-esp32-elf-gcc-ar(?:\.exe)?)\s+(?:cr|crs)\b') { $candidate = $Matches["exe"] }

        if (-not [string]::IsNullOrWhiteSpace($candidate))
        {
            $resolved = Resolve-NativeToolPath -Candidate $candidate
            if (-not [string]::IsNullOrWhiteSpace($resolved)) { return $resolved }
        }
    }

    foreach ($line in Get-Content -LiteralPath $LogPath)
    {
        $compilerCandidate = $null
        if ($line -match '"(?<exe>[^"]*xtensa-esp32-elf-g\+\+(?:\.exe)?)"') { $compilerCandidate = $Matches["exe"] }
        elseif ($line -match '(?<exe>\S*xtensa-esp32-elf-g\+\+(?:\.exe)?)\s') { $compilerCandidate = $Matches["exe"] }
        if ([string]::IsNullOrWhiteSpace($compilerCandidate)) { continue }

        $compiler = Resolve-NativeToolPath -Candidate $compilerCandidate
        if ([string]::IsNullOrWhiteSpace($compiler)) { continue }
        $toolDir = Split-Path -Parent $compiler
        foreach ($leaf in @("xtensa-esp32-elf-gcc-ar.exe", "xtensa-esp32-elf-gcc-ar"))
        {
            $candidate = Join-Path $toolDir $leaf
            if (Test-Path -LiteralPath $candidate) { return (Resolve-Path -LiteralPath $candidate).Path }
        }
    }

    throw "No se pudo localizar xtensa-esp32-elf-gcc-ar desde el log P6B-1."
}

function Get-PropertyValue
{
    param(
        [Parameter(Mandatory = $true)][string]$Path,
        [Parameter(Mandatory = $true)][string]$Name
    )

    foreach ($line in Get-Content -LiteralPath $Path)
    {
        if ($line -match ('^' + [regex]::Escape($Name) + '=(?<value>.*)$')) { return $Matches["value"].Trim() }
    }
    return $null
}

Write-Host "JWPLC - activar estado validado P6B-2 / sin compilar" -ForegroundColor Cyan
Write-Host ("PowerShell: {0} / {1}" -f $PSVersionTable.PSVersion, $PSVersionTable.PSEdition)
Write-Host ""

foreach ($required in @($GfxRoot, $GfxSrc, $GfxProperties, $GfxFonts, $GfxMarker, $StProperties, $StArchive, $InspectorPath, $BaselineBuild, $BaselineLog))
{
    if (-not (Test-Path -LiteralPath $required)) { throw "Falta requisito: $required" }
}

if ((Get-PropertyValue -Path $StProperties -Name "precompiled") -ne "full")
{
    throw "P6B-2 requiere P6A-2 activo: ST77xx no declara precompiled=full."
}

Write-Host "Revalidando run P6B-2 preservado..." -ForegroundColor Cyan
& $InspectorPath
Write-Host "Quality gate P6B-2 preservado: OK" -ForegroundColor Green
Write-Host ""

if ((Get-PropertyValue -Path $GfxProperties -Name "name") -ne "Adafruit GFX Library") { throw "Nombre GFX inesperado." }
if ((Get-PropertyValue -Path $GfxProperties -Name "version") -ne "1.12.4") { throw "Version GFX distinta de 1.12.4." }

$currentPrecompiled = Get-PropertyValue -Path $GfxProperties -Name "precompiled"
if (-not [string]::IsNullOrWhiteSpace([string]$currentPrecompiled))
{
    throw ("Se esperaba precompiled ausente tras rollback P6B-2; actual={0}" -f $currentPrecompiled)
}
if (Test-Path -LiteralPath $GfxArchive)
{
    throw "Ya existe archive GFX; no se sobreescribira."
}

$fontFiles = @(Get-ChildItem -LiteralPath $GfxFonts -Recurse -File)
if ($fontFiles.Count -ne 52) { throw ("Inventario Fonts/ inesperado: {0}, esperado 52." -f $fontFiles.Count) }

$objectRoot = Join-Path $BaselineBuild "libraries\Adafruit_GFX_Library"
if (-not (Test-Path -LiteralPath $objectRoot)) { throw "Falta arbol GFX P6B-1: $objectRoot" }
$sourceObjects = @(Get-ChildItem -LiteralPath $objectRoot -Recurse -File -Filter "*.o" |
    Where-Object { $ExpectedObjects -contains $_.Name } |
    Sort-Object Name)
$actualNames = @($sourceObjects | Select-Object -ExpandProperty Name | Sort-Object)
$expectedNames = @($ExpectedObjects | Sort-Object)
if (($actualNames -join "|") -ne ($expectedNames -join "|"))
{
    throw ("Objetos P6B-1 inesperados: {0}" -f ($actualNames -join ","))
}

$objectBytes = [int64]0
foreach ($obj in $sourceObjects) { $objectBytes += [int64]$obj.Length }
$archiver = Find-Archiver -LogPath $BaselineLog
$originalProperties = Get-Content -LiteralPath $GfxProperties -Raw
$archiveDir = Split-Path -Parent $GfxArchive
New-Item -ItemType Directory -Path $archiveDir -Force | Out-Null

$success = $false
try
{
    $archiveArgs = @("crs", $GfxArchive) + @($sourceObjects | ForEach-Object { $_.FullName })
    $arResult = Invoke-NativeCaptured -FilePath $archiver -Arguments $archiveArgs
    if ($arResult.ExitCode -ne 0 -or -not (Test-Path -LiteralPath $GfxArchive))
    {
        throw "No se pudo regenerar archive GFX."
    }

    $membersResult = Invoke-NativeCaptured -FilePath $archiver -Arguments @("t", $GfxArchive)
    if ($membersResult.ExitCode -ne 0) { throw "No se pudo listar archive GFX." }
    $members = @($membersResult.Output | ForEach-Object { ([string]$_).Trim() } | Where-Object { $_ -ne "" } | Sort-Object)
    if (($members -join "|") -ne ($expectedNames -join "|"))
    {
        throw ("Archive GFX con miembros inesperados: {0}" -f ($members -join ","))
    }

    $archiveFile = Get-Item -LiteralPath $GfxArchive
    $archiveSha = (Get-FileHash -LiteralPath $GfxArchive -Algorithm SHA256).Hash.ToLowerInvariant()

    $newProperties = $originalProperties
    if (-not $newProperties.EndsWith("`n")) { $newProperties += "`r`n" }
    $newProperties += "precompiled=full`r`n"
    Set-Content -LiteralPath $GfxProperties -Value $newProperties -Encoding utf8NoBOM -NoNewline

    if ((Get-PropertyValue -Path $GfxProperties -Name "precompiled") -ne "full")
    {
        throw "No se pudo activar precompiled=full en GFX."
    }

    $success = $true
    Write-Host "Layout GFX src/: OK | marker bundled bajo src/ | Fonts/=52" -ForegroundColor Green
    Write-Host ("Objetos reutilizados: {0} | {1} bytes" -f $sourceObjects.Count, $objectBytes) -ForegroundColor Green
    Write-Host ("Archive regenerado: {0} bytes | miembros=4 | SHA-256={1}" -f $archiveFile.Length, $archiveSha) -ForegroundColor Green
    Write-Host "precompiled=full GFX: ACTIVO" -ForegroundColor Green
    Write-Host ""
    Write-Host "=== P6B-2 ESTADO VALIDADO: ACTIVO ===" -ForegroundColor Green
    Write-Host "No se ejecuto ninguna compilacion." -ForegroundColor DarkGray
    Write-Host "El package local queda listo para preparar P6C (Adafruit BusIO)." -ForegroundColor Yellow
}
finally
{
    if (-not $success)
    {
        Write-Host "No se pudo activar P6B-2; restaurando library.properties GFX y retirando el archive local." -ForegroundColor Yellow
        Set-Content -LiteralPath $GfxProperties -Value $originalProperties -Encoding utf8NoBOM -NoNewline
        if (Test-Path -LiteralPath $GfxArchive) { Remove-Item -LiteralPath $GfxArchive -Force }
    }
}
