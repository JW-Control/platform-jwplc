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
$CoreRoot = Join-Path $PlatformRoot "cores\jwcontrol"

$UnifiedRoot = Join-Path $LibrariesRoot "JWPLC_Ethernet"
$LegacyBackendRoot = Join-Path $LibrariesRoot "JWPLC_Ethernet_W5x00_Backend"

$PropsPath = Join-Path $UnifiedRoot "library.properties"
$JwHeaderPath = Join-Path $UnifiedRoot "src\JWPLC_Ethernet.h"
$W5HeaderPath = Join-Path $UnifiedRoot "src\JWPLC_W5x00_Ethernet.h"
$LegacyMarkerPath = Join-Path $UnifiedRoot "src\JWPLC_Bundled_Ethernet_W5x00.h"
$LegacyArchivePath = Join-Path $UnifiedRoot "src\esp32\libJWPLC_Ethernet_W5x00_Backend.a"

$ArduinoHeaderPath = Join-Path $CoreRoot "Arduino.h"
$DisplayAutoPath = Join-Path $LibrariesRoot "JWPLC_Display\src\JWPLC_Display_Auto.h"
$GlobalAutoPath = Join-Path $LibrariesRoot "JWPLC_GlobalPeripherals\src\JWPLC_GlobalPeripherals_Auto.h"
$GlobalHeaderPath = Join-Path $LibrariesRoot "JWPLC_GlobalPeripherals\src\JWPLC_GlobalPeripherals.h"

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

$requiredPaths = @(
    $PropsPath,
    $JwHeaderPath,
    $W5HeaderPath,
    $ArduinoHeaderPath,
    $DisplayAutoPath,
    $GlobalAutoPath,
    $GlobalHeaderPath
)

foreach ($path in $requiredPaths)
{
    if (-not (Test-Path -LiteralPath $path))
    {
        throw "Falta archivo requerido: $path"
    }
}

$props = Get-Content -LiteralPath $PropsPath -Raw
$jwHeader = Get-Content -LiteralPath $JwHeaderPath -Raw
$arduinoHeader = Get-Content -LiteralPath $ArduinoHeaderPath -Raw
$displayAuto = Get-Content -LiteralPath $DisplayAutoPath -Raw
$globalAuto = Get-Content -LiteralPath $GlobalAutoPath -Raw
$globalHeader = Get-Content -LiteralPath $GlobalHeaderPath -Raw

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

if (Test-Path -LiteralPath $LegacyMarkerPath)
{
    Write-Host "Marker legacy: PRESENTE" -ForegroundColor Red
    $staticOk = $false
}
else
{
    Write-Host "Marker legacy: AUSENTE OK" -ForegroundColor Green
}

if (Test-Path -LiteralPath $LegacyArchivePath)
{
    Write-Host "Archive backend legacy: PRESENTE" -ForegroundColor Red
    $staticOk = $false
}
else
{
    Write-Host "Archive backend legacy: AUSENTE OK" -ForegroundColor Green
}

Write-Host ""
Write-Host "Verificando cadena de autoload JWPLC..." -ForegroundColor Cyan

$autoloadChecks = @(
    @{
        Name = "Arduino.h -> JWPLC_Display_Auto.h"
        Ok = ($arduinoHeader -match '#include\s+<JWPLC_Display_Auto\.h>')
    },
    @{
        Name = "Display_Auto -> GlobalPeripherals_Auto"
        Ok = ($displayAuto -match '#include\s+<JWPLC_GlobalPeripherals_Auto\.h>')
    },
    @{
        Name = "GlobalPeripherals_Auto -> GlobalPeripherals"
        Ok = ($globalAuto -match '#include\s+<JWPLC_GlobalPeripherals\.h>')
    },
    @{
        Name = "GlobalPeripherals -> JWPLC_Ethernet"
        Ok = ($globalHeader -match '#include\s+<JWPLC_Ethernet\.h>')
    }
)

foreach ($check in $autoloadChecks)
{
    if ($check.Ok)
    {
        Write-Host ("{0}: OK" -f $check.Name) -ForegroundColor Green
    }
    else
    {
        Write-Host ("{0}: ERROR" -f $check.Name) -ForegroundColor Red
        $staticOk = $false
    }
}

if ($globalAuto -match '#if\s+!JWPLC_LIBRARY_DISCOVERY_PHASE')
{
    Write-Host "Discovery liviano GlobalPeripherals_Auto: OK" -ForegroundColor Green
}
else
{
    Write-Host "Discovery liviano GlobalPeripherals_Auto: NO DETECTADO" -ForegroundColor Red
    $staticOk = $false
}

if (-not $staticOk)
{
    throw "La estructura estatica/autoload de JWPLC_Ethernet unificado no es valida."
}

Write-Host "AUTOLOAD_CHAIN_STATIC=PASS" -ForegroundColor Green

$runId = (Get-Date).ToString("yyyyMMdd_HHmmss")
$runRoot = Join-Path $OutputRoot ("ethernet_unified_" + $runId)
$probeRoot = Join-Path $runRoot "probe_jwplc_ethernet_unified"
$probeSketch = Join-Path $probeRoot "probe_jwplc_ethernet_unified.ino"
$buildPath = Join-Path $runRoot "verify-Basic"
$logPath = Join-Path $runRoot "verify-Basic.log"

New-Item -ItemType Directory -Path $probeRoot -Force | Out-Null
New-Item -ItemType Directory -Path $buildPath -Force | Out-Null

# Este probe es deliberadamente explicito. Su objetivo es verificar la seleccion
# de la libreria Arduino y detectar homonimos/legacy. El autoload se comprueba
# arriba como cadena estatica y se valida funcionalmente con el acceptance Alpha6.
$probeText = @"
#include <JWPLC_Ethernet.h>

void setup()
{
    (void)JWPLC_Ethernet.isEnabled();
}

void loop()
{
}
"@

$utf8NoBom = New-Object System.Text.UTF8Encoding($false)
[System.IO.File]::WriteAllText($probeSketch, $probeText, $utf8NoBom)

Write-Host ""
Write-Host "Ejecutando library discovery con probe Ethernet explicito; no compila firmware final..." -ForegroundColor DarkGray

$args = @(
    "compile",
    "-b", "jwplc_local:esp32:jwplcbasic",
    "-v",
    "--build-path", $buildPath,
    "--only-compilation-database",
    $probeRoot
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
