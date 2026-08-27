[CmdletBinding()]
param(
    [string]$ArduinoCli = "arduino-cli"
)

Set-StrictMode -Version 2.0
$ErrorActionPreference = "Stop"

$ScriptRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$RepoRoot = [System.IO.Path]::GetFullPath((Join-Path $ScriptRoot "..\.."))
$PlatformRoot = Join-Path $RepoRoot "JWPLC\2.1.0"
$LibrariesRoot = Join-Path $PlatformRoot "libraries"
$UnifiedRoot = Join-Path $LibrariesRoot "JWPLC_Ethernet"
$LegacyBackendRoot = Join-Path $LibrariesRoot "JWPLC_Ethernet_W5x00_Backend"
$SketchPath = Join-Path $ScriptRoot "sketches\01_empty"
$OutputRoot = Join-Path $ScriptRoot "dependency-selection-work"

if ($null -eq (Get-Command $ArduinoCli -ErrorAction SilentlyContinue))
{
    throw "No se encontro arduino-cli."
}

if (-not (Test-Path -LiteralPath $UnifiedRoot))
{
    throw "No existe JWPLC_Ethernet: $UnifiedRoot"
}

if (Test-Path -LiteralPath $LegacyBackendRoot)
{
    throw "La libreria legacy JWPLC_Ethernet_W5x00_Backend reaparecio: $LegacyBackendRoot"
}

$propsPath = Join-Path $UnifiedRoot "library.properties"
$jwHeaderPath = Join-Path $UnifiedRoot "src\JWPLC_Ethernet.h"
$w5HeaderPath = Join-Path $UnifiedRoot "src\JWPLC_W5x00_Ethernet.h"
$legacyMarkerPath = Join-Path $UnifiedRoot "src\JWPLC_Bundled_Ethernet_W5x00.h"
$legacyArchivePath = Join-Path $UnifiedRoot "src\esp32\libJWPLC_Ethernet_W5x00_Backend.a"

foreach ($path in @($propsPath, $jwHeaderPath, $w5HeaderPath))
{
    if (-not (Test-Path -LiteralPath $path))
    {
        throw "Falta archivo requerido: $path"
    }
}

$props = Get-Content -LiteralPath $propsPath -Raw
$jwHeader = Get-Content -LiteralPath $jwHeaderPath -Raw
$staticOk = $true

Write-Host "JWPLC - verificacion Ethernet W5x00 unificado" -ForegroundColor Cyan
Write-Host ""

if ($props -match '(?m)^name=JWPLC_Ethernet\s*$' -and
    $props -match '(?m)^includes=JWPLC_Ethernet\.h\s*$')
{
    Write-Host "Metadata JWPLC_Ethernet: OK" -ForegroundColor Green
}
else
{
    Write-Host "Metadata JWPLC_Ethernet: ERROR" -ForegroundColor Red
    $staticOk = $false
}

if ($props -match '(?m)^depends=JWPLC Ethernet W5x00 Backend\s*$')
{
    Write-Host "Dependencia legacy backend: PRESENTE" -ForegroundColor Red
    $staticOk = $false
}
else
{
    Write-Host "Dependencia legacy backend: AUSENTE OK" -ForegroundColor Green
}

if ($jwHeader -match '#include\s+[\"<]JWPLC_W5x00_Ethernet\.h[\">]')
{
    Write-Host "Backend W5x00 interno: OK" -ForegroundColor Green
}
else
{
    Write-Host "Backend W5x00 interno: NO INCLUIDO" -ForegroundColor Red
    $staticOk = $false
}

if ($jwHeader -match '#include\s+<Ethernet\.h>' -or
    $jwHeader -match 'JWPLC_Bundled_Ethernet_W5x00')
{
    Write-Host "Includes legacy/ambiguos: PRESENTES" -ForegroundColor Red
    $staticOk = $false
}
else
{
    Write-Host "Includes legacy/ambiguos: AUSENTES OK" -ForegroundColor Green
}

if (Test-Path -LiteralPath $legacyMarkerPath)
{
    Write-Host "Marker legacy: PRESENTE" -ForegroundColor Red
    $staticOk = $false
}
else
{
    Write-Host "Marker legacy: AUSENTE OK" -ForegroundColor Green
}

if (Test-Path -LiteralPath $legacyArchivePath)
{
    Write-Host "Archive backend legacy: PRESENTE" -ForegroundColor Red
    $staticOk = $false
}
else
{
    Write-Host "Archive backend legacy: AUSENTE OK" -ForegroundColor Green
}

if (-not $staticOk)
{
    throw "La estructura estatica de JWPLC_Ethernet unificado no es valida."
}

$runId = (Get-Date).ToString("yyyyMMdd_HHmmss")
$runRoot = Join-Path $OutputRoot ("ethernet_unified_" + $runId)
$buildPath = Join-Path $runRoot "verify-Basic"
$logPath = Join-Path $runRoot "verify-Basic.log"
New-Item -ItemType Directory -Path $buildPath -Force | Out-Null

Write-Host ""
Write-Host "Ejecutando library discovery; no compila firmware final..." -ForegroundColor DarkGray

$args = @(
    "compile",
    "-b", "jwplc_local:esp32:jwplcbasic",
    "-v",
    "--build-path", $buildPath,
    "--only-compilation-database",
    $SketchPath
)

$old = $ErrorActionPreference
$output = @()
$exitCode = -1
try
{
    $ErrorActionPreference = "Continue"
    $output = @(& $ArduinoCli @args 2>&1 | ForEach-Object { $_.ToString() })
    $exitCode = $LASTEXITCODE
}
finally
{
    $ErrorActionPreference = $old
}

$output | Out-File $logPath -Encoding utf8
if ($exitCode -ne 0)
{
    $output | Select-Object -Last 40 | ForEach-Object { Write-Host $_ -ForegroundColor DarkYellow }
    throw "Arduino CLI fallo durante discovery. Revisar $logPath"
}

$expectedPath = [System.IO.Path]::GetFullPath($UnifiedRoot)
$jwLines = @($output | Where-Object { $_ -like 'Using library JWPLC_Ethernet at version * in folder:*' })
$jwSelected = $false
foreach ($line in $jwLines)
{
    if ([string]$line -and ([string]$line).IndexOf($expectedPath, [System.StringComparison]::OrdinalIgnoreCase) -ge 0)
    {
        $jwSelected = $true
        break
    }
}

$legacySelected = @($output | Where-Object {
    $_ -match 'JWPLC Ethernet W5x00 Backend|JWPLC_Ethernet_W5x00_Backend'
})

$foreignEthernet = @($output | Where-Object {
    $_ -like 'Using library Ethernet at version * in folder:*'
})

$userEthernet = @($output | Where-Object {
    $_ -match 'Using library .*Ethernet.* in folder:' -and
    $_ -match '[\\/]Arduino[\\/]libraries[\\/]Ethernet'
})

if ($jwSelected)
{
    Write-Host "JWPLC_Ethernet unificado: SELECCIONADO OK" -ForegroundColor Green
}
else
{
    Write-Host "JWPLC_Ethernet unificado: NO SELECCIONADO" -ForegroundColor Red
}

if ($legacySelected.Count -eq 0)
{
    Write-Host "Backend legacy separado: NO SELECCIONADO OK" -ForegroundColor Green
}
else
{
    Write-Host "Backend legacy separado: DETECTADO" -ForegroundColor Red
    $legacySelected | ForEach-Object { Write-Host ("  {0}" -f $_) -ForegroundColor Yellow }
}

if ($foreignEthernet.Count -eq 0)
{
    Write-Host "Ethernet homonima externa/Espressif: NO SELECCIONADA" -ForegroundColor Green
}
else
{
    Write-Host "Ethernet homonima externa/Espressif: DETECTADA" -ForegroundColor Red
    $foreignEthernet | ForEach-Object { Write-Host ("  {0}" -f $_) -ForegroundColor Yellow }
}

if ($userEthernet.Count -eq 0)
{
    Write-Host "Ethernet del sketchbook: IGNORADA OK" -ForegroundColor Green
}
else
{
    Write-Host "Ethernet del sketchbook: DETECTADA" -ForegroundColor Red
    $userEthernet | ForEach-Object { Write-Host ("  {0}" -f $_) -ForegroundColor Yellow }
}

Write-Host ""
Write-Host ("Log: {0}" -f $logPath)

if (-not $jwSelected -or
    $legacySelected.Count -gt 0 -or
    $foreignEthernet.Count -gt 0 -or
    $userEthernet.Count -gt 0)
{
    throw "La seleccion Ethernet unificada no es reproducible todavia."
}

Write-Host "JWPLC_ETHERNET_UNIFIED_SELECTION=PASS" -ForegroundColor Green
