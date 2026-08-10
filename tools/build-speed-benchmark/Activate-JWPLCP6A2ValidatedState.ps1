#requires -Version 7.4

[CmdletBinding()]
param()

Set-StrictMode -Version 2.0
$ErrorActionPreference = "Stop"

$ScriptRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$RepoRoot = [System.IO.Path]::GetFullPath((Join-Path $ScriptRoot "..\.."))
$PlatformRoot = Join-Path $RepoRoot "JWPLC\2.1.0"
$LibraryRoot = Join-Path $PlatformRoot "libraries\Adafruit_ST7735_and_ST7789_Library"
$SrcRoot = Join-Path $LibraryRoot "src"
$PropertiesPath = Join-Path $LibraryRoot "library.properties"
$ArchivePath = Join-Path $SrcRoot "esp32\libAdafruit_ST7735_and_ST7789_Library.a"
$InspectorPath = Join-Path $ScriptRoot "Inspect-JWPLCP6A2ExistingRun.ps1"

$P6A1RunId = "20260809_231953"
$P6A1RunRoot = Join-Path (Join-Path $ScriptRoot "p6a-st77xx-layout-work") $P6A1RunId
$P6A1BuildPath = Join-Path $P6A1RunRoot "p6a-layout-Basic"
$P6A1LogPath = Join-Path $P6A1RunRoot "p6a-layout-Basic.log"

$MovedFiles = @(
    "Adafruit_ST7735.cpp",
    "Adafruit_ST7735.h",
    "Adafruit_ST7789.cpp",
    "Adafruit_ST7789.h",
    "Adafruit_ST7796S.cpp",
    "Adafruit_ST7796S.h",
    "Adafruit_ST77xx.cpp",
    "Adafruit_ST77xx.h",
    "JWPLC_Bundled_Adafruit_ST77xx.h"
)

$ExpectedObjectNames = @(
    "Adafruit_ST7735.cpp.o",
    "Adafruit_ST7789.cpp.o",
    "Adafruit_ST7796S.cpp.o",
    "Adafruit_ST77xx.cpp.o"
)

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

function Resolve-NativeToolPath
{
    param([Parameter(Mandatory = $true)][string]$Candidate)

    $normalized = $Candidate.Trim().Trim('"')
    while ($normalized.Contains("\\")) { $normalized = $normalized.Replace("\\", "\") }

    foreach ($path in @($normalized, ($normalized + ".exe")))
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

    if (-not (Test-Path -LiteralPath $LogPath)) { throw "No existe log P6A-1: $LogPath" }
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

    throw "No se pudo localizar xtensa-esp32-elf-gcc-ar desde el log P6A-1."
}

function Get-PropertyValue
{
    param(
        [Parameter(Mandatory = $true)][string]$Path,
        [Parameter(Mandatory = $true)][string]$Name
    )

    foreach ($line in Get-Content -LiteralPath $Path)
    {
        if ($line -match ('^' + [regex]::Escape($Name) + '=(?<value>.*)$'))
        {
            return $Matches["value"].Trim()
        }
    }
    return $null
}

Write-Host "JWPLC - activar estado validado P6A-2 / sin compilar" -ForegroundColor Cyan
Write-Host ("PowerShell: {0} / {1}" -f $PSVersionTable.PSVersion, $PSVersionTable.PSEdition)
Write-Host ""

foreach ($required in @($LibraryRoot, $SrcRoot, $PropertiesPath, $InspectorPath, $P6A1BuildPath, $P6A1LogPath))
{
    if (-not (Test-Path -LiteralPath $required)) { throw "Falta requisito: $required" }
}

Write-Host "Revalidando run P6A-2 preservado..." -ForegroundColor Cyan
& $InspectorPath
Write-Host "Quality gate P6A-2 preservado: OK" -ForegroundColor Green
Write-Host ""

$rootPresent = @($MovedFiles | Where-Object { Test-Path -LiteralPath (Join-Path $LibraryRoot $_) })
$srcPresent = @($MovedFiles | Where-Object { Test-Path -LiteralPath (Join-Path $SrcRoot $_) })
if ($rootPresent.Count -ne 0 -or $srcPresent.Count -ne $MovedFiles.Count)
{
    throw ("Se requiere layout src/ validado. root={0}/9, src={1}/9" -f $rootPresent.Count, $srcPresent.Count)
}

$currentPrecompiled = Get-PropertyValue -Path $PropertiesPath -Name "precompiled"
if (-not [string]::IsNullOrWhiteSpace([string]$currentPrecompiled))
{
    throw ("Se esperaba precompiled ausente tras rollback del piloto; actual={0}" -f $currentPrecompiled)
}
if (Test-Path -LiteralPath $ArchivePath)
{
    throw "Ya existe el archive ST77xx; no se sobreescribira."
}

$objectRoot = Join-Path $P6A1BuildPath "libraries\Adafruit_ST7735_and_ST7789_Library"
$sourceObjects = @(Get-ChildItem -LiteralPath $objectRoot -Recurse -File -Filter "*.o" |
    Where-Object { $ExpectedObjectNames -contains $_.Name } |
    Sort-Object Name)
$actualNames = @($sourceObjects | Select-Object -ExpandProperty Name | Sort-Object)
$expectedNames = @($ExpectedObjectNames | Sort-Object)
if (($actualNames -join "|") -ne ($expectedNames -join "|"))
{
    throw ("Objetos P6A-1 inesperados: {0}" -f ($actualNames -join ","))
}

$objectBytes = [int64]0
foreach ($obj in $sourceObjects) { $objectBytes += [int64]$obj.Length }
$archiver = Find-Archiver -LogPath $P6A1LogPath
$originalProperties = Get-Content -LiteralPath $PropertiesPath -Raw
$archiveDir = Split-Path -Parent $ArchivePath
New-Item -ItemType Directory -Path $archiveDir -Force | Out-Null

$success = $false
try
{
    $archiveArgs = @("crs", $ArchivePath) + @($sourceObjects | ForEach-Object { $_.FullName })
    $arResult = Invoke-NativeCaptured -FilePath $archiver -Arguments $archiveArgs
    if ($arResult.ExitCode -ne 0 -or -not (Test-Path -LiteralPath $ArchivePath))
    {
        throw "No se pudo regenerar archive ST77xx."
    }

    $membersResult = Invoke-NativeCaptured -FilePath $archiver -Arguments @("t", $ArchivePath)
    if ($membersResult.ExitCode -ne 0) { throw "No se pudo listar archive ST77xx." }
    $members = @($membersResult.Output | ForEach-Object { ([string]$_).Trim() } | Where-Object { $_ -ne "" } | Sort-Object)
    if (($members -join "|") -ne ($expectedNames -join "|"))
    {
        throw ("Archive ST77xx con miembros inesperados: {0}" -f ($members -join ","))
    }

    $archiveFile = Get-Item -LiteralPath $ArchivePath
    $archiveSha = (Get-FileHash -LiteralPath $ArchivePath -Algorithm SHA256).Hash.ToLowerInvariant()

    $newProperties = $originalProperties
    if (-not $newProperties.EndsWith("`n")) { $newProperties += "`r`n" }
    $newProperties += "precompiled=full`r`n"
    Set-Content -LiteralPath $PropertiesPath -Value $newProperties -Encoding utf8NoBOM -NoNewline

    if ((Get-PropertyValue -Path $PropertiesPath -Name "precompiled") -ne "full")
    {
        throw "No se pudo activar precompiled=full."
    }

    $success = $true
    Write-Host ("Layout src/: OK | 9/9 archivos") -ForegroundColor Green
    Write-Host ("Objetos reutilizados: {0} | {1} bytes" -f $sourceObjects.Count, $objectBytes) -ForegroundColor Green
    Write-Host ("Archive regenerado: {0} bytes | miembros=4 | SHA-256={1}" -f $archiveFile.Length, $archiveSha) -ForegroundColor Green
    Write-Host "precompiled=full: ACTIVO" -ForegroundColor Green
    Write-Host ""
    Write-Host "=== P6A-2 ESTADO VALIDADO: ACTIVO ===" -ForegroundColor Green
    Write-Host "No se ejecuto ninguna compilacion." -ForegroundColor DarkGray
    Write-Host "El package local queda listo para preparar P6B (Adafruit GFX)." -ForegroundColor Yellow
}
finally
{
    if (-not $success)
    {
        Set-Content -LiteralPath $PropertiesPath -Value $originalProperties -Encoding utf8NoBOM -NoNewline
        if (Test-Path -LiteralPath $ArchivePath) { Remove-Item -LiteralPath $ArchivePath -Force }
    }
}
