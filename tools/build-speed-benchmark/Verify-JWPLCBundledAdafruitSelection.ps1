[CmdletBinding()]
param(
    [string]$ArduinoCli = "arduino-cli"
)

Set-StrictMode -Version 2.0
$ErrorActionPreference = "Stop"

$ScriptRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$RepoRoot = [System.IO.Path]::GetFullPath((Join-Path $ScriptRoot "..\.."))
$PlatformRoot = Join-Path $RepoRoot "JWPLC\2.1.0"
$SketchPath = Join-Path $ScriptRoot "sketches\01_empty"
$OutputRoot = Join-Path $ScriptRoot "dependency-selection-work"

if ($null -eq (Get-Command $ArduinoCli -ErrorAction SilentlyContinue))
{
    throw "No se encontro arduino-cli."
}

$runId = (Get-Date).ToString("yyyyMMdd_HHmmss")
$runRoot = Join-Path $OutputRoot $runId
$buildPath = Join-Path $runRoot "verify-Basic"
$logPath = Join-Path $runRoot "verify-Basic.log"
New-Item -ItemType Directory -Path $buildPath -Force | Out-Null

Write-Host "JWPLC - verificacion de Adafruit bundled" -ForegroundColor Cyan
Write-Host "Solo ejecuta library discovery; no compila objetos." -ForegroundColor DarkGray
Write-Host ""

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
    $output | Select-Object -Last 30 | ForEach-Object { Write-Host $_ -ForegroundColor DarkYellow }
    throw "Arduino CLI fallo durante discovery. Revisar $logPath"
}

$expected = @(
    [PSCustomObject]@{
        Name = "Adafruit ST7735 and ST7789 Library"
        Folder = "Adafruit_ST7735_and_ST7789_Library"
    },
    [PSCustomObject]@{
        Name = "Adafruit GFX Library"
        Folder = "Adafruit_GFX_Library"
    },
    [PSCustomObject]@{
        Name = "Adafruit BusIO"
        Folder = "Adafruit_BusIO"
    }
)

$allOk = $true
foreach ($item in $expected)
{
    $line = @($output | Where-Object { $_ -like ("Using library " + $item.Name + " at version *") } | Select-Object -Last 1)
    if ($line.Count -eq 0)
    {
        Write-Host ("{0}: NO DETECTADA" -f $item.Name) -ForegroundColor Red
        $allOk = $false
        continue
    }

    $text = [string]$line[0]
    $expectedPath = [System.IO.Path]::GetFullPath((Join-Path $PlatformRoot ("libraries\" + $item.Folder)))
    $isBundled = $text.IndexOf($expectedPath, [System.StringComparison]::OrdinalIgnoreCase) -ge 0

    if ($isBundled)
    {
        Write-Host ("{0}: BUNDLED OK" -f $item.Name) -ForegroundColor Green
    }
    else
    {
        Write-Host ("{0}: INCORRECTA" -f $item.Name) -ForegroundColor Red
        Write-Host ("  {0}" -f $text) -ForegroundColor Yellow
        $allOk = $false
    }
}

$userAdafruit = @($output | Where-Object {
    $_ -match 'Using library Adafruit ' -and
    $_ -match '[\\/]Arduino[\\/]libraries[\\/]Adafruit'
})

Write-Host ""
Write-Host ("Log: {0}" -f $logPath)
if ($userAdafruit.Count -gt 0)
{
    Write-Host "Se detectaron Adafruit del sketchbook:" -ForegroundColor Red
    $userAdafruit | ForEach-Object { Write-Host ("  {0}" -f $_) }
    $allOk = $false
}

if (-not $allOk)
{
    throw "La seleccion Adafruit no es reproducible todavia."
}

Write-Host "ADAFRUIT BUNDLED: OK" -ForegroundColor Green
