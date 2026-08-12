#requires -Version 7.4

[CmdletBinding()]
param()

Set-StrictMode -Version 2.0
$ErrorActionPreference = "Stop"

$ScriptRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$BaselineRunId = "20260809_224628"
$CurrentRunId = "20260809_231953"
$BaselineRoot = Join-Path (Join-Path $ScriptRoot "p5a-ethernet-work") $BaselineRunId
$CurrentRoot = Join-Path (Join-Path $ScriptRoot "p6a-st77xx-layout-work") $CurrentRunId
$BaselineBuild = Join-Path $BaselineRoot "p5a-Basic"
$CurrentBuild = Join-Path $CurrentRoot "p6a-layout-Basic"
$BaselineLog = Join-Path $BaselineRoot "p5a-Basic.log"
$CurrentLog = Join-Path $CurrentRoot "p6a-layout-Basic.log"

function ConvertTo-EntryList
{
    param([Parameter(Mandatory = $true)]$Parsed)
    $entries = New-Object System.Collections.Generic.List[object]
    foreach ($entry in $Parsed)
    {
        if ($null -eq $entry) { throw "Entrada nula en compile_commands.json." }
        [void]$entries.Add($entry)
    }
    if ($entries.Count -eq 0) { throw "compile_commands.json vacio." }
    return $entries
}

function Get-CompileMetrics
{
    param([Parameter(Mandatory = $true)][string]$BuildPath)
    $dbPath = Join-Path $BuildPath "compile_commands.json"
    if (-not (Test-Path -LiteralPath $dbPath)) { throw "Falta compile_commands.json: $dbPath" }
    $entries = ConvertTo-EntryList -Parsed (Get-Content -LiteralPath $dbPath -Raw | ConvertFrom-Json)
    $files = @($entries | ForEach-Object { [string]$_.file })
    return [PSCustomObject]@{
        Total = $entries.Count
        ST77xx = @($files | Where-Object { $_ -match '[\\/]libraries[\\/]Adafruit_ST7735_and_ST7789_Library[\\/]' }).Count
        GFX = @($files | Where-Object { $_ -match '[\\/]libraries[\\/]Adafruit_GFX_Library[\\/]' }).Count
        BusIO = @($files | Where-Object { $_ -match '[\\/]libraries[\\/]Adafruit_BusIO[\\/]' }).Count
        Ethernet = @($files | Where-Object { $_ -match '[\\/]libraries[\\/]JWPLC_Ethernet_W5x00_Backend[\\/]' }).Count
        Display = @($files | Where-Object { $_ -match '[\\/]libraries[\\/]JWPLC_Display[\\/]' }).Count
        Stub = @($files | Where-Object { $_ -match '[\\/]cores[\\/]jwcontrol_p2[\\/]p2_core_stub\.c$' }).Count
    }
}

function Get-PreprocessCount
{
    param([Parameter(Mandatory = $true)][string]$LogPath)
    return @(Get-Content -LiteralPath $LogPath | Where-Object { ([string]$_) -match 'xtensa-esp32-elf-g\+\+.*\s-E\s' }).Count
}

function Get-LibrarySelections
{
    param([Parameter(Mandatory = $true)][string]$LogPath)
    return @(Get-Content -LiteralPath $LogPath |
        Where-Object { ([string]$_) -like "Using library *" } |
        ForEach-Object { ([string]$_).Trim() } |
        Sort-Object -Unique)
}

function Get-ExternalObjectTable
{
    param([Parameter(Mandatory = $true)][string]$BuildPath)
    $root = Join-Path $BuildPath "libraries"
    $table = @{}
    foreach ($file in Get-ChildItem -LiteralPath $root -Recurse -File -Filter "*.o")
    {
        $relative = $file.FullName.Substring($root.Length).TrimStart('\','/')
        if ($relative -match '^Adafruit_ST7735_and_ST7789_Library[\\/]') { continue }
        $table[$relative] = (Get-FileHash -LiteralPath $file.FullName -Algorithm SHA256).Hash.ToLowerInvariant()
    }
    return $table
}

function Get-AppBin
{
    param([Parameter(Mandatory = $true)][string]$BuildPath)
    $bin = Get-ChildItem -LiteralPath $BuildPath -Filter "*.ino.bin" -File | Select-Object -First 1
    if ($null -eq $bin) { throw "No se encontro app .bin en $BuildPath" }
    return $bin
}

function Get-AppElf
{
    param([Parameter(Mandatory = $true)][string]$BuildPath)
    $elf = Get-ChildItem -LiteralPath $BuildPath -Filter "*.ino.elf" -File | Select-Object -First 1
    if ($null -eq $elf) { throw "No se encontro app .elf en $BuildPath" }
    return $elf
}

function Convert-BytesToHex
{
    param([Parameter(Mandatory = $true)][byte[]]$Bytes)
    return ([BitConverter]::ToString($Bytes)).Replace("-", "").ToLowerInvariant()
}

function Get-ByteSlice
{
    param(
        [Parameter(Mandatory = $true)][byte[]]$Bytes,
        [Parameter(Mandatory = $true)][int]$Offset,
        [Parameter(Mandatory = $true)][int]$Count
    )
    $slice = New-Object byte[] $Count
    [Array]::Copy($Bytes, $Offset, $slice, 0, $Count)
    return $slice
}

function Get-Sha256Bytes
{
    param([Parameter(Mandatory = $true)][byte[]]$Bytes)
    $sha = [System.Security.Cryptography.SHA256]::Create()
    try { return $sha.ComputeHash($Bytes) }
    finally { $sha.Dispose() }
}

Write-Host "JWPLC - inspeccion P6A-1 existente / sin compilar" -ForegroundColor Cyan
Write-Host ("P5A : {0}" -f $BaselineRoot)
Write-Host ("P6A1: {0}" -f $CurrentRoot)
Write-Host ""

foreach ($required in @($BaselineBuild, $CurrentBuild, $BaselineLog, $CurrentLog))
{
    if (-not (Test-Path -LiteralPath $required)) { throw "Falta artefacto requerido: $required" }
}

$baseMetrics = Get-CompileMetrics -BuildPath $BaselineBuild
$currMetrics = Get-CompileMetrics -BuildPath $CurrentBuild
$baseE = Get-PreprocessCount -LogPath $BaselineLog
$currE = Get-PreprocessCount -LogPath $CurrentLog

if ($baseMetrics.Total -ne 24 -or $currMetrics.Total -ne 24 -or
    $baseMetrics.ST77xx -ne 4 -or $currMetrics.ST77xx -ne 4 -or
    $baseMetrics.GFX -ne 4 -or $currMetrics.GFX -ne 4 -or
    $baseMetrics.BusIO -ne 4 -or $currMetrics.BusIO -ne 4 -or
    $baseMetrics.Ethernet -ne 0 -or $currMetrics.Ethernet -ne 0 -or
    $baseMetrics.Display -ne 0 -or $currMetrics.Display -ne 0 -or
    $baseMetrics.Stub -ne 1 -or $currMetrics.Stub -ne 1 -or
    $baseE -ne 41 -or $currE -ne 41)
{
    throw "Estructura P5A/P6A-1 inesperada."
}

$baseSelections = Get-LibrarySelections -LogPath $BaselineLog
$currSelections = Get-LibrarySelections -LogPath $CurrentLog
$onlyBaseSelection = @($baseSelections | Where-Object { $currSelections -notcontains $_ })
$onlyCurrSelection = @($currSelections | Where-Object { $baseSelections -notcontains $_ })
if ($onlyBaseSelection.Count -ne 0 -or $onlyCurrSelection.Count -ne 0)
{
    throw "Cambio en seleccion de librerias entre P5A y P6A-1."
}

$baseObjects = Get-ExternalObjectTable -BuildPath $BaselineBuild
$currObjects = Get-ExternalObjectTable -BuildPath $CurrentBuild
$commonObjects = @($baseObjects.Keys | Where-Object { $currObjects.ContainsKey($_) })
$objectMismatch = @($commonObjects | Where-Object { $baseObjects[$_] -ne $currObjects[$_] })
$onlyBaseObjects = @($baseObjects.Keys | Where-Object { -not $currObjects.ContainsKey($_) })
$onlyCurrObjects = @($currObjects.Keys | Where-Object { -not $baseObjects.ContainsKey($_) })
if ($objectMismatch.Count -ne 0 -or $onlyBaseObjects.Count -ne 0 -or $onlyCurrObjects.Count -ne 0)
{
    throw "Cambios en objetos externos a ST77xx."
}

$baseBin = Get-AppBin -BuildPath $BaselineBuild
$currBin = Get-AppBin -BuildPath $CurrentBuild
$baseElf = Get-AppElf -BuildPath $BaselineBuild
$currElf = Get-AppElf -BuildPath $CurrentBuild
$baseBytes = [System.IO.File]::ReadAllBytes($baseBin.FullName)
$currBytes = [System.IO.File]::ReadAllBytes($currBin.FullName)

if ($baseBytes.Length -ne $currBytes.Length -or $baseBytes.Length -ne 406032)
{
    throw ("Tamano app inesperado. P5A={0}, P6A1={1}" -f $baseBytes.Length, $currBytes.Length)
}

$elfHashOffset = 0xB0
$elfHashLength = 32
$tailLength = 33
$tailOffset = $baseBytes.Length - $tailLength
$diffTotal = 0
$diffElfHash = 0
$diffTail = 0
$diffPayload = 0
for ($i = 0; $i -lt $baseBytes.Length; $i++)
{
    if ($baseBytes[$i] -eq $currBytes[$i]) { continue }
    $diffTotal++
    if ($i -ge $elfHashOffset -and $i -lt ($elfHashOffset + $elfHashLength)) { $diffElfHash++; continue }
    if ($i -ge $tailOffset) { $diffTail++; continue }
    $diffPayload++
}

$baseEmbeddedElfHash = Convert-BytesToHex -Bytes (Get-ByteSlice -Bytes $baseBytes -Offset $elfHashOffset -Count $elfHashLength)
$currEmbeddedElfHash = Convert-BytesToHex -Bytes (Get-ByteSlice -Bytes $currBytes -Offset $elfHashOffset -Count $elfHashLength)
$baseElfHash = (Get-FileHash -LiteralPath $baseElf.FullName -Algorithm SHA256).Hash.ToLowerInvariant()
$currElfHash = (Get-FileHash -LiteralPath $currElf.FullName -Algorithm SHA256).Hash.ToLowerInvariant()

if ($baseEmbeddedElfHash -ne $baseElfHash -or $currEmbeddedElfHash -ne $currElfHash)
{
    throw "El bloque 0xB0..0xCF no coincide con SHA-256 del ELF correspondiente."
}

$baseImagePrefix = Get-ByteSlice -Bytes $baseBytes -Offset 0 -Count ($baseBytes.Length - 32)
$currImagePrefix = Get-ByteSlice -Bytes $currBytes -Offset 0 -Count ($currBytes.Length - 32)
$baseTailHash = Convert-BytesToHex -Bytes (Get-ByteSlice -Bytes $baseBytes -Offset ($baseBytes.Length - 32) -Count 32)
$currTailHash = Convert-BytesToHex -Bytes (Get-ByteSlice -Bytes $currBytes -Offset ($currBytes.Length - 32) -Count 32)
$baseCalculatedTailHash = Convert-BytesToHex -Bytes (Get-Sha256Bytes -Bytes $baseImagePrefix)
$currCalculatedTailHash = Convert-BytesToHex -Bytes (Get-Sha256Bytes -Bytes $currImagePrefix)
if ($baseTailHash -ne $baseCalculatedTailHash -or $currTailHash -ne $currCalculatedTailHash)
{
    throw "El SHA-256 final de la imagen no valida."
}

if ($diffPayload -ne 0 -or $diffElfHash -ne 32 -or $diffTail -ne 33 -or $diffTotal -ne 65)
{
    throw ("Diferencias app fuera del patron esperado. total={0}, elfHash={1}, tail={2}, payload={3}" -f $diffTotal, $diffElfHash, $diffTail, $diffPayload)
}

Write-Host ("Estructura: 24 compiles | ST77xx=4 | GFX=4 | BusIO=4 | Eth=0 | Display=0 | stub=1 | -E=41") -ForegroundColor Green
Write-Host ("Objetos externos: comunes={0} | SHA distintos=0 | solo P5A=0 | solo P6A1=0" -f $commonObjects.Count) -ForegroundColor Green
Write-Host ("App: {0} bytes -> {1} bytes" -f $baseBytes.Length, $currBytes.Length)
Write-Host ("Diferencias app: total={0} | ELF hash={1} bytes | cola checksum/hash={2} bytes | payload={3} bytes" -f $diffTotal, $diffElfHash, $diffTail, $diffPayload)
Write-Host ("ELF SHA embebido valido: P5A=True | P6A1=True") -ForegroundColor Green
Write-Host ("SHA final de imagen valido: P5A=True | P6A1=True") -ForegroundColor Green
Write-Host ""
Write-Host "=== P6A-1 EXISTENTE: VALIDADO ===" -ForegroundColor Green
Write-Host "El contenido ejecutable de la app es byte-a-byte equivalente." -ForegroundColor Green
Write-Host "Las 65 diferencias son hashes/checksum derivados del ELF y de la propia imagen." -ForegroundColor Yellow
Write-Host "No se ejecuto ninguna compilacion." -ForegroundColor DarkGray
