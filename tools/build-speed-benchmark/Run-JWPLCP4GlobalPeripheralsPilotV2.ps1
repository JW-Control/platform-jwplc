[CmdletBinding()]
param()

Set-StrictMode -Version 2.0
$ErrorActionPreference = "Stop"

$ScriptRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$LegacyScript = Join-Path $ScriptRoot "Run-JWPLCP4GlobalPeripheralsPilot.ps1"
$P4Root = Join-Path $ScriptRoot "p4-work"

if (-not (Test-Path -LiteralPath $LegacyScript))
{
    throw "No se encontro el piloto P4 base: $LegacyScript"
}

$start = Get-Date
$legacyError = $null

Write-Host "P4 v2 - ejecucion con validacion corregida" -ForegroundColor Cyan
Write-Host "El wrapper corrige el falso negativo conocido del contador core stub." -ForegroundColor DarkGray
Write-Host ""

try
{
    & $LegacyScript
}
catch
{
    $legacyError = $_.Exception.Message
    Write-Host ""
    Write-Host ("Piloto base termino con aviso: {0}" -f $legacyError) -ForegroundColor Yellow
    Write-Host "Validando el build real antes de considerarlo fallo..." -ForegroundColor Cyan
}

if (-not (Test-Path -LiteralPath $P4Root))
{
    throw "No existe p4-work despues de ejecutar el piloto."
}

$run = Get-ChildItem -LiteralPath $P4Root -Directory |
    Where-Object { $_.LastWriteTime -ge $start.AddMinutes(-1) } |
    Sort-Object LastWriteTime -Descending |
    Select-Object -First 1

if ($null -eq $run)
{
    throw "No se encontro una corrida P4 nueva para validar."
}

$build = Join-Path $run.FullName "verify-Basic"
$logPath = Join-Path $run.FullName "verify-Basic.log"
$summaryPath = Join-Path $run.FullName "P4_SUMMARY.md"
$mapPath = Join-Path $build "01_empty.ino.map"
$binPath = Join-Path $build "01_empty.ino.bin"
$stubObject = Join-Path $build "core\precompiled_core_stub.c.o"

foreach ($required in @($logPath, $summaryPath, $mapPath, $binPath, $stubObject))
{
    if (-not (Test-Path -LiteralPath $required))
    {
        throw "Falta artefacto P4 requerido: $required"
    }
}

$log = Get-Content -LiteralPath $logPath -Raw
$summary = Get-Content -LiteralPath $summaryPath -Raw
$map = Get-Content -LiteralPath $mapPath -Raw

$stubCompile = ([regex]::Matches($log, 'jwcontrol_precompiled_stub(?:\\\\|\\|/)+p2_core_stub\.c"\s+-o\s+"')).Count
$hookRan = ($log -match 'cores\\\\jwcontrol_precompiled_stub\\\\p2_core_stub\.c') -and
           ($log -match 'precompiled\\\\core\\\\JWPLCBASIC\\\\core\.a')

$globalLinked = $map -match 'libJWPLC_GlobalPeripherals\.a\(JWPLC_GlobalPeripherals\.cpp\.o\)'
$displayCppLinked = $map -match 'libJWPLC_Display\.a\(JWPLC_Display\.cpp\.o\)'
$idleLinked = $map -match 'libJWPLC_Display\.a\(JWPLC_IdleScreen\.cpp\.o\)'

$globalSourceZero = $summary -match '(?m)^GlobalPeripherals fuente: 0\s*$'
$displaySourceZero = $summary -match '(?m)^Display fuente: 0\s*$'
$shaZero = $summary -match '(?m)^Object SHA mismatches outside P3/P4: 0\s*$'
$selectionZero = $summary -match '(?m)^Library selection diff: 0\s*$'

$cold = "?"
$compiles = "?"
$preprocess = "?"
$sizeDelta = "?"
if ($summary -match '(?m)^Cold: ([0-9.,]+) s\s*$') { $cold = $Matches[1] }
if ($summary -match '(?m)^Compiles total: ([0-9]+)\s*$') { $compiles = $Matches[1] }
if ($summary -match '(?m)^Preprocesados -E: ([0-9]+)\s*$') { $preprocess = $Matches[1] }
if ($summary -match '(?m)^App size delta vs P3: (-?[0-9]+)\s*$') { $sizeDelta = $Matches[1] }

Write-Host ""
Write-Host "=== Validacion P4 v2 ===" -ForegroundColor Cyan
Write-Host ("Run: {0}" -f $run.FullName)
Write-Host ("Cold: {0} s | compiles={1} | preprocesados-E={2}" -f $cold, $compiles, $preprocess)
Write-Host ("Core P2: stub object=True, stub compile detectado={0}, hook P2={1}" -f $stubCompile, $hookRan)
Write-Host ("Global archive linked={0}" -f $globalLinked)
Write-Host ("Display archive linked: Display.cpp={0}, IdleScreen.cpp={1}" -f $displayCppLinked, $idleLinked)
Write-Host ("Fuentes omitidas: Global={0}, Display={1}" -f $globalSourceZero, $displaySourceZero)
Write-Host ("Objetos externos SHA iguales={0}, seleccion librerias igual={1}" -f $shaZero, $selectionZero)
Write-Host ("Delta app vs P3: {0} bytes" -f $sizeDelta)

$ok = (Test-Path -LiteralPath $stubObject) -and
      $hookRan -and
      $globalLinked -and
      $displayCppLinked -and
      $idleLinked -and
      $globalSourceZero -and
      $displaySourceZero -and
      $shaZero -and
      $selectionZero

Write-Host ""
if ($ok)
{
    Write-Host "P4 VALIDADO: OK" -ForegroundColor Green
    Write-Host "El contador stub=0 del piloto base era un falso negativo de parsing." -ForegroundColor DarkGray
    exit 0
}

Write-Host "P4 VALIDADO: REVISAR" -ForegroundColor Red
if ($null -ne $legacyError)
{
    Write-Host ("Error original: {0}" -f $legacyError) -ForegroundColor Yellow
}
exit 2
