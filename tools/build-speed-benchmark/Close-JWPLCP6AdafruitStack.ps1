#requires -Version 7.4

[CmdletBinding()]
param()

Set-StrictMode -Version 2.0
$ErrorActionPreference = "Stop"

$ScriptRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$RepoRoot = [System.IO.Path]::GetFullPath((Join-Path $ScriptRoot "..\.."))
$LibrariesRoot = Join-Path $RepoRoot "JWPLC\2.1.0\libraries"

$StRoot = Join-Path $LibrariesRoot "Adafruit_ST7735_and_ST7789_Library"
$StProperties = Join-Path $StRoot "library.properties"
$StArchive = Join-Path $StRoot "src\esp32\libAdafruit_ST7735_and_ST7789_Library.a"

$GfxRoot = Join-Path $LibrariesRoot "Adafruit_GFX_Library"
$GfxProperties = Join-Path $GfxRoot "library.properties"
$GfxArchive = Join-Path $GfxRoot "src\esp32\libAdafruit_GFX_Library.a"

$BusRoot = Join-Path $LibrariesRoot "Adafruit_BusIO"
$BusProperties = Join-Path $BusRoot "library.properties"
$BusArchive = Join-Path $BusRoot "src\esp32\libAdafruit_BusIO.a"

$P5Root = Join-Path $ScriptRoot "p5a-ethernet-work\20260809_224628"
$P6ARoot = Join-Path $ScriptRoot "p6a2-st77xx-precompiled-work\20260809_232946"
$P6BRoot = Join-Path $ScriptRoot "p6b2-gfx-precompiled-work\20260809_235326"
$P6CRoot = Join-Path $ScriptRoot "p6c2-busio-precompiled-work\20260810_002028"

$P6CBuild = Join-Path $P6CRoot "p6c2-Basic"
$P6CLog = Join-Path $P6CRoot "p6c2-Basic.log"
$P6CTiming = Join-Path $P6CRoot "P6C2_TIMING_SECONDS.txt"
$ReportPath = Join-Path $ScriptRoot "P6_ADAFRUIT_FINAL_AUDIT.md"

$ExpectedST = @(
    "Adafruit_ST7735.cpp.o",
    "Adafruit_ST7789.cpp.o",
    "Adafruit_ST7796S.cpp.o",
    "Adafruit_ST77xx.cpp.o"
)
$ExpectedGFX = @(
    "Adafruit_GFX.cpp.o",
    "Adafruit_GrayOLED.cpp.o",
    "Adafruit_SPITFT.cpp.o",
    "glcdfont.c.o"
)
$ExpectedBus = @(
    "Adafruit_BusIO_Register.cpp.o",
    "Adafruit_GenericDevice.cpp.o",
    "Adafruit_I2CDevice.cpp.o",
    "Adafruit_SPIDevice.cpp.o"
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
        $compiler = $null
        if ($line -match '"(?<exe>[^"]*xtensa-esp32-elf-g\+\+(?:\.exe)?)"') { $compiler = Resolve-NativeToolPath -Candidate $Matches["exe"] }
        if ([string]::IsNullOrWhiteSpace($compiler)) { continue }
        $candidate = Join-Path (Split-Path -Parent $compiler) "xtensa-esp32-elf-gcc-ar.exe"
        if (Test-Path -LiteralPath $candidate) { return (Resolve-Path -LiteralPath $candidate).Path }
    }
    throw "No se pudo localizar xtensa-esp32-elf-gcc-ar desde el log P6C-2."
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

function ConvertTo-EntryList
{
    param([Parameter(Mandatory = $true)]$Parsed)
    $entries = New-Object System.Collections.Generic.List[object]
    foreach ($entry in $Parsed)
    {
        if ($null -eq $entry) { throw "compile_commands.json contiene entrada nula." }
        [void]$entries.Add($entry)
    }
    if ($entries.Count -eq 0) { throw "compile_commands.json vacio." }
    return $entries
}

function Get-CompileMetrics
{
    param([Parameter(Mandatory = $true)][string]$BuildPath)
    $db = Join-Path $BuildPath "compile_commands.json"
    if (-not (Test-Path -LiteralPath $db)) { throw "Falta compile_commands.json: $db" }
    $entries = ConvertTo-EntryList -Parsed (Get-Content -LiteralPath $db -Raw | ConvertFrom-Json)
    $files = @($entries | ForEach-Object { [string]$_.file })
    return [PSCustomObject]@{
        Total = $entries.Count
        ST77xx = @($files | Where-Object { $_ -match '[\\/]libraries[\\/]Adafruit_ST7735_and_ST7789_Library[\\/]' }).Count
        GFX = @($files | Where-Object { $_ -match '[\\/]libraries[\\/]Adafruit_GFX_Library[\\/]' }).Count
        BusIO = @($files | Where-Object { $_ -match '[\\/]libraries[\\/]Adafruit_BusIO[\\/]' }).Count
        Ethernet = @($files | Where-Object { $_ -match '[\\/]libraries[\\/]JWPLC_Ethernet_W5x00_Backend[\\/]' }).Count
        Display = @($files | Where-Object { $_ -match '[\\/]libraries[\\/]JWPLC_Display[\\/]' }).Count
        Stub = @($files | Where-Object { $_ -match '[\\/]cores[\\/]jwcontrol_precompiled_stub[\\/]p2_core_stub\.c$' }).Count
    }
}

function Get-PreprocessCount
{
    param([Parameter(Mandatory = $true)][string]$LogPath)
    return @(Get-Content -LiteralPath $LogPath | Where-Object { ([string]$_) -match 'xtensa-esp32-elf-g\+\+.*\s-E\s' }).Count
}

function Assert-ArchiveMembers
{
    param(
        [Parameter(Mandatory = $true)][string]$Archiver,
        [Parameter(Mandatory = $true)][string]$Archive,
        [Parameter(Mandatory = $true)][string[]]$Expected,
        [Parameter(Mandatory = $true)][string]$Label
    )
    $result = Invoke-NativeCaptured -FilePath $Archiver -Arguments @("t", $Archive)
    if ($result.ExitCode -ne 0) { throw "No se pudo listar archive $Label." }
    $actual = @($result.Output | ForEach-Object { ([string]$_).Trim() } | Where-Object { $_ -ne "" } | Sort-Object)
    $expectedSorted = @($Expected | Sort-Object)
    if (($actual -join "|") -ne ($expectedSorted -join "|"))
    {
        throw ("Archive {0} inesperado. Actual={1}" -f $Label, ($actual -join ", "))
    }
    return $actual
}

function Read-Timing
{
    param([Parameter(Mandatory = $true)][string]$Path)
    if (-not (Test-Path -LiteralPath $Path)) { throw "Falta timing: $Path" }
    return [double]::Parse((Get-Content -LiteralPath $Path -Raw).Trim(), [Globalization.CultureInfo]::InvariantCulture)
}

function Percent-Reduction
{
    param([double]$From, [double]$To)
    return (($From - $To) / $From) * 100.0
}

Write-Host "JWPLC - cierre P6 Adafruit full stack / sin compilar" -ForegroundColor Cyan
Write-Host ("PowerShell: {0} / {1}" -f $PSVersionTable.PSVersion, $PSVersionTable.PSEdition)
Write-Host ""

foreach ($required in @($StProperties, $StArchive, $GfxProperties, $GfxArchive, $BusProperties, $BusArchive, $P6CBuild, $P6CLog, $P6CTiming))
{
    if (-not (Test-Path -LiteralPath $required)) { throw "Falta requisito P6D: $required" }
}

if ((Get-PropertyValue -Path $StProperties -Name "precompiled") -ne "full") { throw "ST77xx no esta activo con precompiled=full." }
if ((Get-PropertyValue -Path $GfxProperties -Name "precompiled") -ne "full") { throw "GFX no esta activo con precompiled=full." }
if ((Get-PropertyValue -Path $BusProperties -Name "precompiled") -ne "full") { throw "BusIO no esta activo con precompiled=full." }

$metrics = Get-CompileMetrics -BuildPath $P6CBuild
$preprocess = Get-PreprocessCount -LogPath $P6CLog
if ($metrics.Total -ne 12 -or $metrics.ST77xx -ne 0 -or $metrics.GFX -ne 0 -or $metrics.BusIO -ne 0 -or
    $metrics.Ethernet -ne 0 -or $metrics.Display -ne 0 -or $metrics.Stub -ne 1 -or $preprocess -ne 29)
{
    throw ("P6 full stack inesperado: total/ST/GFX/BusIO/Eth/Display/stub/-E={0}/{1}/{2}/{3}/{4}/{5}/{6}/{7}" -f $metrics.Total, $metrics.ST77xx, $metrics.GFX, $metrics.BusIO, $metrics.Ethernet, $metrics.Display, $metrics.Stub, $preprocess)
}

$archiver = Find-Archiver -LogPath $P6CLog
[void](Assert-ArchiveMembers -Archiver $archiver -Archive $StArchive -Expected $ExpectedST -Label "ST77xx")
[void](Assert-ArchiveMembers -Archiver $archiver -Archive $GfxArchive -Expected $ExpectedGFX -Label "GFX")
[void](Assert-ArchiveMembers -Archiver $archiver -Archive $BusArchive -Expected $ExpectedBus -Label "BusIO")

$mapFile = Get-ChildItem -LiteralPath $P6CBuild -Filter "*.map" -File | Select-Object -First 1
$binFile = Get-ChildItem -LiteralPath $P6CBuild -Filter "*.ino.bin" -File | Select-Object -First 1
if ($null -eq $mapFile -or $null -eq $binFile) { throw "Faltan .map/.bin P6C-2." }
$mapText = Get-Content -LiteralPath $mapFile.FullName -Raw
foreach ($needle in @("libAdafruit_ST7735_and_ST7789_Library.a", "libAdafruit_GFX_Library.a", "libAdafruit_BusIO.a"))
{
    if (-not $mapText.Contains($needle)) { throw "P6C-2 no demuestra enlace de $needle." }
}
if ($binFile.Length -ne 404912) { throw ("Tamano app P6C-2 inesperado: {0} bytes." -f $binFile.Length) }

$p5 = Read-Timing -Path (Join-Path $P5Root "P5A_TIMING_SECONDS.txt")
$p6a = Read-Timing -Path (Join-Path $P6ARoot "P6A2_TIMING_SECONDS.txt")
$p6b = Read-Timing -Path (Join-Path $P6BRoot "P6B2_TIMING_SECONDS.txt")
$p6c = Read-Timing -Path $P6CTiming

$p5Reduction = Percent-Reduction -From $p5 -To $p6c
$p6aReduction = Percent-Reduction -From $p6a -To $p6c
$p6bReduction = Percent-Reduction -From $p6b -To $p6c

Write-Host "Estado local: ST77xx + GFX + BusIO con precompiled=full" -ForegroundColor Green
Write-Host "Archives: 4/4 miembros cada uno | contenido esperado" -ForegroundColor Green
Write-Host ("Estructura final: {0} compiles | ST={1} | GFX={2} | BusIO={3} | -E={4} | app={5} B" -f $metrics.Total, $metrics.ST77xx, $metrics.GFX, $metrics.BusIO, $preprocess, $binFile.Length) -ForegroundColor Green
Write-Host ""
Write-Host ("P5A Ethernet: {0:N3} s" -f $p5)
Write-Host ("P6A-2 ST77xx: {0:N3} s" -f $p6a)
Write-Host ("P6B-2 GFX: {0:N3} s" -f $p6b)
Write-Host ("P6C-2 BusIO/full stack: {0:N3} s" -f $p6c) -ForegroundColor Cyan
Write-Host ("Reduccion P5A -> P6 final: {0:N3} s ({1:N2} %)" -f ($p5 - $p6c), $p5Reduction) -ForegroundColor Green

$report = @(
    "# P6 - Cierre del stack Adafruit precompilado",
    "",
    "Estado: VALIDADO ESTRUCTURALMENTE para JWPLC Basic.",
    "",
    "## Estado final",
    "",
    "- ST77xx: precompiled=full, archive de 4 miembros.",
    "- Adafruit GFX: precompiled=full, archive de 4 miembros.",
    "- Adafruit BusIO: precompiled=full, archive de 4 miembros.",
    "- Cold final preservado: $($p6c.ToString('N3')) s.",
    "- Compiles: 12.",
    "- ST77xx/GFX/BusIO desde fuente: 0/0/0.",
    "- Preprocesados g++ -E: 29.",
    "- App: 404912 B.",
    "",
    "## Tiempos preservados",
    "",
    "| Etapa | Cold | Reduccion vs etapa anterior relevante |",
    "|---|---:|---:|",
    ("| P5A Ethernet | {0:N3} s | - |" -f $p5),
    ("| P6A-2 ST77xx | {0:N3} s | {1:N2}% vs P5A |" -f $p6a, (Percent-Reduction -From $p5 -To $p6a)),
    ("| P6B-2 GFX | {0:N3} s | {1:N2}% vs P6A-2 |" -f $p6b, (Percent-Reduction -From $p6a -To $p6b)),
    ("| P6C-2 BusIO / full stack | {0:N3} s | {1:N2}% vs P6B-2 |" -f $p6c, $p6bReduction),
    "",
    ("Reduccion acumulada P5A -> P6 final: {0:N3} s ({1:N2}%)." -f ($p5 - $p6c), $p5Reduction),
    ("Reduccion P6A-2 -> P6 final: {0:N3} s ({1:N2}%)." -f ($p6a - $p6c), $p6aReduction),
    "",
    "## Conclusion P6D",
    "",
    "P6D no requiere un cold adicional: el run P6C-2 ya compila con las tres capas Adafruit precompiladas simultaneamente y constituye la validacion full-stack para JWPLC Basic.",
    "",
    "Gate funcional pendiente antes del cierre del alpha: prueba fisica TFT/perifericos y validacion de compatibilidad de los archives/configuracion en las variantes que correspondan, incluida Basic Core cuando aplique.",
    ""
)
$report | Set-Content -LiteralPath $ReportPath -Encoding utf8NoBOM

Write-Host ""
Write-Host "=== P6D FULL ADAFRUIT STACK: CERRADO / VALIDADO ESTRUCTURALMENTE ===" -ForegroundColor Green
Write-Host ("Reporte: {0}" -f $ReportPath) -ForegroundColor DarkGray
Write-Host "No se ejecuto ninguna compilacion." -ForegroundColor DarkGray
Write-Host "Siguiente fase recomendada: consolidar tabla formal de tiempos y resolver pendientes de cierre del alpha." -ForegroundColor Yellow
