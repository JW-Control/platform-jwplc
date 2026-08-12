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
$BackendRoot = Join-Path $LibrariesRoot "JWPLC_Ethernet_W5x00_Backend"
$JwEthernetRoot = Join-Path $LibrariesRoot "JWPLC_Ethernet"
$SketchPath = Join-Path $ScriptRoot "sketches\01_empty"
$OutputRoot = Join-Path $ScriptRoot "dependency-selection-work"

if ($null -eq (Get-Command $ArduinoCli -ErrorAction SilentlyContinue))
{
    throw "No se encontro arduino-cli."
}

if (-not (Test-Path -LiteralPath $BackendRoot))
{
    throw "No existe el backend vendorizado: $BackendRoot"
}

$backendPropsPath = Join-Path $BackendRoot "library.properties"
$jwPropsPath = Join-Path $JwEthernetRoot "library.properties"
$jwHeaderPath = Join-Path $JwEthernetRoot "src\JWPLC_Ethernet.h"

foreach ($path in @($backendPropsPath, $jwPropsPath, $jwHeaderPath))
{
    if (-not (Test-Path -LiteralPath $path))
    {
        throw "Falta archivo requerido: $path"
    }
}

$backendProps = Get-Content -LiteralPath $backendPropsPath -Raw
$jwProps = Get-Content -LiteralPath $jwPropsPath -Raw
$jwHeader = Get-Content -LiteralPath $jwHeaderPath -Raw

$staticOk = $true

Write-Host "JWPLC - verificacion backend Ethernet W5x00 bundled" -ForegroundColor Cyan
Write-Host ""

if ($backendProps -match '(?m)^name=JWPLC Ethernet W5x00 Backend\s*$' -and
    $backendProps -match '(?m)^version=2\.0\.2\s*$' -and
    $backendProps -match '(?m)^includes=Ethernet\.h\s*$')
{
    Write-Host "Backend metadata: OK" -ForegroundColor Green
}
else
{
    Write-Host "Backend metadata: ERROR" -ForegroundColor Red
    $staticOk = $false
}

if ($jwProps -match '(?m)^depends=JWPLC Ethernet W5x00 Backend\s*$')
{
    Write-Host "JWPLC_Ethernet depends: OK" -ForegroundColor Green
}
else
{
    Write-Host "JWPLC_Ethernet depends: ERROR" -ForegroundColor Red
    $staticOk = $false
}

$markerText = '#include <JWPLC_Bundled_Ethernet_W5x00.h>'
$ethernetText = '#include <Ethernet.h>'
$markerIndex = $jwHeader.IndexOf($markerText, [System.StringComparison]::Ordinal)
$ethernetIndex = $jwHeader.IndexOf($ethernetText, [System.StringComparison]::Ordinal)

if ($markerIndex -ge 0 -and $ethernetIndex -ge 0 -and $markerIndex -lt $ethernetIndex)
{
    Write-Host "Orden de includes JWPLC_Ethernet: OK" -ForegroundColor Green
}
else
{
    Write-Host "Orden de includes JWPLC_Ethernet: ERROR" -ForegroundColor Red
    $staticOk = $false
}

if (-not $staticOk)
{
    throw "La configuracion estatica del backend Ethernet no es valida."
}

$runId = (Get-Date).ToString("yyyyMMdd_HHmmss")
$runRoot = Join-Path $OutputRoot ("ethernet_" + $runId)
$buildPath = Join-Path $runRoot "verify-Basic"
$logPath = Join-Path $runRoot "verify-Basic.log"
New-Item -ItemType Directory -Path $buildPath -Force | Out-Null

Write-Host ""
Write-Host "Ejecutando solo library discovery; no compila objetos..." -ForegroundColor DarkGray

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

$expectedPath = [System.IO.Path]::GetFullPath($BackendRoot)
$backendLines = @($output | Where-Object { $_ -like 'Using library JWPLC Ethernet W5x00 Backend at version 2.0.2 in folder:*' })
$backendSelected = $false
foreach ($line in $backendLines)
{
    if ([string]$line -and ([string]$line).IndexOf($expectedPath, [System.StringComparison]::OrdinalIgnoreCase) -ge 0)
    {
        $backendSelected = $true
        break
    }
}

$foreignEthernet = @($output | Where-Object {
    $_ -like 'Using library Ethernet at version * in folder:*'
})

$userEthernet = @($output | Where-Object {
    $_ -match 'Using library .*Ethernet.* in folder:' -and
    $_ -match '[\\/]Arduino[\\/]libraries[\\/]Ethernet'
})

if ($backendSelected)
{
    Write-Host "JWPLC Ethernet W5x00 Backend 2.0.2: BUNDLED OK" -ForegroundColor Green
}
else
{
    Write-Host "JWPLC Ethernet W5x00 Backend 2.0.2: NO SELECCIONADO" -ForegroundColor Red
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

if (-not $backendSelected -or $foreignEthernet.Count -gt 0 -or $userEthernet.Count -gt 0)
{
    throw "La seleccion Ethernet no es reproducible todavia."
}

Write-Host "ETHERNET W5x00 BUNDLED: OK" -ForegroundColor Green
