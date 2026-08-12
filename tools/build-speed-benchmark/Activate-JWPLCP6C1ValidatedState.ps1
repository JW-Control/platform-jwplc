#requires -Version 7.4

[CmdletBinding()]
param()

Set-StrictMode -Version 2.0
$ErrorActionPreference = "Stop"

$ScriptRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$RepoRoot = [System.IO.Path]::GetFullPath((Join-Path $ScriptRoot "..\.."))
$PlatformRoot = Join-Path $RepoRoot "JWPLC\2.1.0"
$LibrariesRoot = Join-Path $PlatformRoot "libraries"

$BusRoot = Join-Path $LibrariesRoot "Adafruit_BusIO"
$BusSrc = Join-Path $BusRoot "src"
$BusProperties = Join-Path $BusRoot "library.properties"
$BusRelative = "JWPLC/2.1.0/libraries/Adafruit_BusIO"
$BusMarkerName = "JWPLC_Bundled_Adafruit_BusIO.h"

$StRoot = Join-Path $LibrariesRoot "Adafruit_ST7735_and_ST7789_Library"
$StProperties = Join-Path $StRoot "library.properties"
$StArchive = Join-Path $StRoot "src\esp32\libAdafruit_ST7735_and_ST7789_Library.a"

$GfxRoot = Join-Path $LibrariesRoot "Adafruit_GFX_Library"
$GfxProperties = Join-Path $GfxRoot "library.properties"
$GfxArchive = Join-Path $GfxRoot "src\esp32\libAdafruit_GFX_Library.a"

$InspectorPath = Join-Path $ScriptRoot "Inspect-JWPLCP6C1ExistingRun.ps1"

$ExpectedSources = @(
    "Adafruit_BusIO_Register.cpp",
    "Adafruit_GenericDevice.cpp",
    "Adafruit_I2CDevice.cpp",
    "Adafruit_SPIDevice.cpp"
)

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

function Get-CodeFiles
{
    param([Parameter(Mandatory = $true)][string]$Root)

    if (-not (Test-Path -LiteralPath $Root)) { return @() }
    return @(Get-ChildItem -LiteralPath $Root -File |
        Where-Object { $_.Extension -in @(".c", ".cc", ".cpp", ".h", ".hpp", ".S") } |
        Sort-Object Name)
}

function Get-HashTable
{
    param([Parameter(Mandatory = $true)][System.IO.FileInfo[]]$Files)

    $table = @{}
    foreach ($file in $Files)
    {
        $table[$file.Name] = (Get-FileHash -LiteralPath $file.FullName -Algorithm SHA256).Hash.ToLowerInvariant()
    }
    return $table
}

function Compare-HashTables
{
    param(
        [Parameter(Mandatory = $true)]$Before,
        [Parameter(Mandatory = $true)]$After
    )

    $beforeKeys = @($Before.Keys | Sort-Object)
    $afterKeys = @($After.Keys | Sort-Object)
    if (($beforeKeys -join "|") -ne ($afterKeys -join "|"))
    {
        throw "BusIO cambio inventario durante la activacion."
    }

    foreach ($key in $beforeKeys)
    {
        if ($Before[$key] -ne $After[$key])
        {
            throw "BusIO cambio bytes durante la activacion: $key"
        }
    }
}

Write-Host "JWPLC - activar estado validado P6C-1 / sin compilar" -ForegroundColor Cyan
Write-Host ("PowerShell: {0} / {1}" -f $PSVersionTable.PSVersion, $PSVersionTable.PSEdition)
Write-Host ""

foreach ($required in @($BusRoot, $BusProperties, $StProperties, $StArchive, $GfxProperties, $GfxArchive, $InspectorPath))
{
    if (-not (Test-Path -LiteralPath $required)) { throw "Falta requisito P6C-1: $required" }
}

if ((Get-PropertyValue -Path $StProperties -Name "precompiled") -ne "full")
{
    throw "P6C-1 requiere ST77xx precompiled activo."
}
if ((Get-PropertyValue -Path $GfxProperties -Name "precompiled") -ne "full")
{
    throw "P6C-1 requiere GFX precompiled activo."
}
if ((Get-PropertyValue -Path $BusProperties -Name "name") -ne "Adafruit BusIO")
{
    throw "Nombre BusIO inesperado."
}
if ((Get-PropertyValue -Path $BusProperties -Name "version") -ne "1.17.4")
{
    throw "Version BusIO distinta de 1.17.4."
}
if (-not [string]::IsNullOrWhiteSpace([string](Get-PropertyValue -Path $BusProperties -Name "precompiled")))
{
    throw "P6C-1 debe seguir source-only; precompiled= no debe estar activo todavia."
}

Write-Host "Revalidando run P6C-1 preservado..." -ForegroundColor Cyan
& $InspectorPath
Write-Host "Quality gate P6C-1 preservado: OK" -ForegroundColor Green
Write-Host ""

$gitStatus = @(& git -C $RepoRoot status --porcelain -- $BusRelative 2>&1 | ForEach-Object { $_.ToString() })
if ($LASTEXITCODE -ne 0) { throw "No se pudo consultar git status para Adafruit_BusIO." }
if ($gitStatus.Count -ne 0)
{
    throw ("Adafruit_BusIO tiene cambios locales previos; no se movera nada:`n{0}" -f ($gitStatus -join "`n"))
}

$rootCode = @(Get-CodeFiles -Root $BusRoot)
$srcCode = @(Get-CodeFiles -Root $BusSrc)
$rootSources = @($rootCode | Where-Object { $_.Extension -in @(".c", ".cc", ".cpp", ".S") } | Select-Object -ExpandProperty Name | Sort-Object)
$expectedSourcesSorted = @($ExpectedSources | Sort-Object)

if ($rootCode.Count -ne 10 -or $srcCode.Count -ne 0)
{
    throw ("Se esperaba rollback flat/root antes de activar P6C-1. root={0}/10, src={1}/0." -f $rootCode.Count, $srcCode.Count)
}
if (($rootSources -join "|") -ne ($expectedSourcesSorted -join "|"))
{
    throw ("Fuentes BusIO inesperadas en raiz: {0}" -f ($rootSources -join ", "))
}
if (-not (Test-Path -LiteralPath (Join-Path $BusRoot $BusMarkerName)))
{
    throw "Falta marker bundled BusIO en raiz antes de la activacion."
}

$beforeHashes = Get-HashTable -Files $rootCode
New-Item -ItemType Directory -Path $BusSrc -Force | Out-Null

$moved = $false
$success = $false
try
{
    foreach ($file in $rootCode)
    {
        Move-Item -LiteralPath $file.FullName -Destination (Join-Path $BusSrc $file.Name)
    }
    $moved = $true

    $afterFiles = @(Get-CodeFiles -Root $BusSrc)
    if ($afterFiles.Count -ne 10)
    {
        throw ("Activacion P6C-1 dejo {0} archivos en src/, esperado 10." -f $afterFiles.Count)
    }

    $afterHashes = Get-HashTable -Files $afterFiles
    Compare-HashTables -Before $beforeHashes -After $afterHashes

    $afterSources = @($afterFiles | Where-Object { $_.Extension -in @(".c", ".cc", ".cpp", ".S") } | Select-Object -ExpandProperty Name | Sort-Object)
    if (($afterSources -join "|") -ne ($expectedSourcesSorted -join "|"))
    {
        throw "Las fuentes BusIO bajo src/ no coinciden con las cuatro esperadas."
    }
    if (Test-Path -LiteralPath (Join-Path $BusRoot $BusMarkerName))
    {
        throw "El marker bundled BusIO permanecio en raiz."
    }
    if (-not (Test-Path -LiteralPath (Join-Path $BusSrc $BusMarkerName)))
    {
        throw "El marker bundled BusIO no quedo bajo src/."
    }

    $success = $true
    Write-Host "Layout BusIO flat -> src/: ACTIVO | 10 archivos | hashes preservados" -ForegroundColor Green
    Write-Host "BusIO: source-only | precompiled= ausente" -ForegroundColor Green
    Write-Host "ST77xx/GFX precompiled: ACTIVOS" -ForegroundColor Green
    Write-Host ""
    Write-Host "=== P6C-1 ESTADO VALIDADO: ACTIVO ===" -ForegroundColor Green
    Write-Host "No se ejecuto ninguna compilacion." -ForegroundColor DarkGray
    Write-Host "El package local queda listo para preparar P6C-2 (Adafruit BusIO precompilado)." -ForegroundColor Yellow
}
finally
{
    if (-not $success -and $moved)
    {
        Write-Host "No se pudo activar P6C-1; restaurando layout BusIO flat/root." -ForegroundColor Yellow
        foreach ($file in @(Get-CodeFiles -Root $BusSrc))
        {
            $destination = Join-Path $BusRoot $file.Name
            if (Test-Path -LiteralPath $destination)
            {
                throw "No se puede restaurar porque ya existe: $destination"
            }
            Move-Item -LiteralPath $file.FullName -Destination $destination
        }

        if ((Test-Path -LiteralPath $BusSrc) -and @(Get-ChildItem -LiteralPath $BusSrc -Force).Count -eq 0)
        {
            Remove-Item -LiteralPath $BusSrc -Force
        }
    }
}
