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

function Normalize-PathText
{
    param([string]$Text)

    if ([string]::IsNullOrWhiteSpace($Text))
    {
        return ""
    }

    return $Text.Replace('\', '/').ToLowerInvariant()
}

function Get-JsonPropertyValue
{
    param(
        [object]$Object,
        [string]$Name
    )

    if ($null -eq $Object)
    {
        return $null
    }

    $property = $Object.PSObject.Properties[$Name]
    if ($null -eq $property)
    {
        return $null
    }

    return $property.Value
}

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

# Probe explicito para aislar la seleccion de la libreria. El autoload real se
# comprueba arriba y su funcionamiento se valida con el acceptance Alpha6.
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
Write-Host "Generando compilation database con probe Ethernet explicito..." -ForegroundColor DarkGray

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

# Arduino CLI 1.0.x no siempre imprime las lineas 'Using library ...' cuando
# --only-compilation-database esta activo. La fuente de verdad en este modo es
# compile_commands.json. El formato admite tanto 'command' como 'arguments'.
$compdbFile = Get-ChildItem -LiteralPath $buildPath -Recurse -File -Filter "compile_commands.json" -ErrorAction SilentlyContinue |
    Select-Object -First 1

if ($null -eq $compdbFile)
{
    throw "Arduino CLI termino en 0 pero no genero compile_commands.json en $buildPath"
}

try
{
    $compdb = @(Get-Content -LiteralPath $compdbFile.FullName -Raw | ConvertFrom-Json)
}
catch
{
    throw "No se pudo interpretar compile_commands.json: $($compdbFile.FullName)"
}

if ($compdb.Count -eq 0)
{
    throw "compile_commands.json esta vacio: $($compdbFile.FullName)"
}

$evidence = New-Object System.Collections.Generic.List[string]
$entriesWithCommand = 0
$entriesWithArguments = 0
$entriesWithFile = 0

foreach ($entry in $compdb)
{
    $fileValue = Get-JsonPropertyValue -Object $entry -Name "file"
    $commandValue = Get-JsonPropertyValue -Object $entry -Name "command"
    $argumentsValue = Get-JsonPropertyValue -Object $entry -Name "arguments"

    if ($null -ne $fileValue)
    {
        ++$entriesWithFile
        $evidence.Add([string]$fileValue)
    }

    if ($null -ne $commandValue)
    {
        ++$entriesWithCommand
        $evidence.Add([string]$commandValue)
    }

    if ($null -ne $argumentsValue)
    {
        ++$entriesWithArguments
        $evidence.Add((@($argumentsValue) -join ' '))
    }
}

$evidenceText = Normalize-PathText -Text ($evidence -join "`n")
$expectedPath = Normalize-PathText -Text ([System.IO.Path]::GetFullPath($UnifiedRoot))
$expectedSrcPath = Normalize-PathText -Text ([System.IO.Path]::GetFullPath((Join-Path $UnifiedRoot "src")))

# Para este checker de seleccion basta con que la compilation database use la
# ruta exacta de JWPLC_Ethernet/src. El conteo de TUs reales se valida luego en
# la compilacion completa del acceptance.
$jwSelected =
    $evidenceText.Contains($expectedPath) -and
    $evidenceText.Contains($expectedSrcPath)

$legacySelected = $evidenceText -match 'jwplc_ethernet_w5x00_backend'

$userEthernetSelected =
    $evidenceText -match '/documents/arduino/libraries/ethernet/' -or
    $evidenceText -match '/documentos/arduino/libraries/ethernet/'

$foreignCandidates = New-Object System.Collections.Generic.List[string]
foreach ($entry in $compdb)
{
    $candidateParts = New-Object System.Collections.Generic.List[string]

    $fileValue = Get-JsonPropertyValue -Object $entry -Name "file"
    $commandValue = Get-JsonPropertyValue -Object $entry -Name "command"
    $argumentsValue = Get-JsonPropertyValue -Object $entry -Name "arguments"

    if ($null -ne $fileValue)
    {
        $candidateParts.Add([string]$fileValue)
    }

    if ($null -ne $commandValue)
    {
        $candidateParts.Add([string]$commandValue)
    }

    if ($null -ne $argumentsValue)
    {
        $candidateParts.Add((@($argumentsValue) -join ' '))
    }

    $candidate = Normalize-PathText -Text ($candidateParts -join ' ')

    if (($candidate -match '/libraries/ethernet/') -and
        (-not $candidate.Contains($expectedPath)) -and
        (-not ($candidate -match '/documents/arduino/libraries/ethernet/')) -and
        (-not ($candidate -match '/documentos/arduino/libraries/ethernet/')))
    {
        $foreignCandidates.Add($candidate)
    }
}

$foreignEthernetSelected = $foreignCandidates.Count -gt 0

Write-Host ("Compilation database: {0}" -f $compdbFile.FullName)
Write-Host ("Compilation units: {0}" -f $compdb.Count)
Write-Host ("Schema: file={0} | command={1} | arguments={2}" -f $entriesWithFile, $entriesWithCommand, $entriesWithArguments)

if ($jwSelected)
{
    Write-Host "JWPLC_Ethernet unificado: SELECCIONADO OK" -ForegroundColor Green
}
else
{
    Write-Host "JWPLC_Ethernet unificado: NO DETECTADO EN COMPILE DATABASE" -ForegroundColor Red
}

if (-not $legacySelected)
{
    Write-Host "Backend legacy separado: NO SELECCIONADO OK" -ForegroundColor Green
}
else
{
    Write-Host "Backend legacy separado: DETECTADO" -ForegroundColor Red
}

if (-not $foreignEthernetSelected)
{
    Write-Host "Ethernet homonima externa/Espressif: NO SELECCIONADA" -ForegroundColor Green
}
else
{
    Write-Host "Ethernet homonima externa/Espressif: DETECTADA" -ForegroundColor Red
}

if (-not $userEthernetSelected)
{
    Write-Host "Ethernet del sketchbook: IGNORADA OK" -ForegroundColor Green
}
else
{
    Write-Host "Ethernet del sketchbook: DETECTADA" -ForegroundColor Red
}

Write-Host ""
Write-Host ("Log: {0}" -f $logPath)

if (-not $jwSelected -or
    $legacySelected -or
    $foreignEthernetSelected -or
    $userEthernetSelected)
{
    Write-Host ""
    Write-Host "Evidencia Ethernet encontrada en compile database:" -ForegroundColor Yellow
    @($evidence | Where-Object { (Normalize-PathText -Text $_) -match 'ethernet' } | Select-Object -First 16) |
        ForEach-Object { Write-Host ("  {0}" -f $_) -ForegroundColor DarkYellow }

    throw "La seleccion Ethernet unificada no es reproducible todavia."
}

Write-Host "JWPLC_ETHERNET_UNIFIED_SELECTION=PASS" -ForegroundColor Green
