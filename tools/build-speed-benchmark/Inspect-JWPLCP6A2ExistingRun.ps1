#requires -Version 7.4

[CmdletBinding()]
param()

Set-StrictMode -Version 2.0
$ErrorActionPreference = "Stop"

$ScriptRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$BaselineRunId = "20260809_231953"
$CurrentRunId = "20260809_232946"
$BaselineRoot = Join-Path (Join-Path $ScriptRoot "p6a-st77xx-layout-work") $BaselineRunId
$CurrentRoot = Join-Path (Join-Path $ScriptRoot "p6a2-st77xx-precompiled-work") $CurrentRunId
$BaselineBuild = Join-Path $BaselineRoot "p6a-layout-Basic"
$CurrentBuild = Join-Path $CurrentRoot "p6a2-Basic"
$BaselineLog = Join-Path $BaselineRoot "p6a-layout-Basic.log"
$CurrentLog = Join-Path $CurrentRoot "p6a2-Basic.log"
$TimingPath = Join-Path $CurrentRoot "P6A2_TIMING_SECONDS.txt"

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
        Stub = @($files | Where-Object { $_ -match '[\\/]cores[\\/]jwcontrol_precompiled_stub[\\/]p2_core_stub\.c$' }).Count
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

function Get-AppMap
{
    param([Parameter(Mandatory = $true)][string]$BuildPath)
    $map = Get-ChildItem -LiteralPath $BuildPath -Filter "*.map" -File | Select-Object -First 1
    if ($null -eq $map) { throw "No se encontro app .map en $BuildPath" }
    return $map
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

function Find-Nm
{
    param([Parameter(Mandatory = $true)][string]$LogPath)
    foreach ($line in Get-Content -LiteralPath $LogPath)
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
        foreach ($leaf in @("xtensa-esp32-elf-nm.exe", "xtensa-esp32-elf-nm"))
        {
            $candidate = Join-Path $toolDir $leaf
            if (Test-Path -LiteralPath $candidate) { return (Resolve-Path -LiteralPath $candidate).Path }
        }
    }
    throw "No se pudo localizar xtensa-esp32-elf-nm desde el log."
}

function Invoke-NativeCaptured
{
    param(
        [Parameter(Mandatory = $true)][string]$FilePath,
        [Parameter(Mandatory = $true)][string[]]$Arguments
    )
    $oldErrorAction = $ErrorActionPreference
    $hadNativePreference = Test-Path variable:global:PSNativeCommandUseErrorActionPreference
    if ($hadNativePreference) { $oldNativePreference = $global:PSNativeCommandUseErrorActionPreference }
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

function Get-NmRecords
{
    param(
        [Parameter(Mandatory = $true)][string]$NmPath,
        [Parameter(Mandatory = $true)][string]$ElfPath
    )
    $result = Invoke-NativeCaptured -FilePath $NmPath -Arguments @("-S", "--defined-only", $ElfPath)
    if ($result.ExitCode -ne 0) { throw "nm fallo para $ElfPath" }
    $records = New-Object System.Collections.Generic.List[object]
    foreach ($line in $result.Output)
    {
        if ($line -match '^(?<addr>[0-9A-Fa-f]+)\s+(?<size>[0-9A-Fa-f]+)\s+(?<type>\S)\s+(?<name>.+)$')
        {
            [void]$records.Add([PSCustomObject]@{
                Size = [Convert]::ToInt64($Matches["size"], 16)
                Type = $Matches["type"]
                Name = $Matches["name"]
            })
        }
        elseif ($line -match '^(?<addr>[0-9A-Fa-f]+)\s+(?<type>\S)\s+(?<name>.+)$')
        {
            [void]$records.Add([PSCustomObject]@{
                Size = [int64]-1
                Type = $Matches["type"]
                Name = $Matches["name"]
            })
        }
    }
    return $records
}

function Get-NmSignatureTable
{
    param([Parameter(Mandatory = $true)][object[]]$Records)
    $table = @{}
    foreach ($group in @($Records | Group-Object { "{0}|{1}" -f $_.Type, $_.Name }))
    {
        $sizes = @($group.Group | ForEach-Object { [int64]$_.Size } | Sort-Object)
        $table[$group.Name] = ($sizes -join ",")
    }
    return $table
}

function Get-MapSectionSize
{
    param(
        [Parameter(Mandatory = $true)][string]$MapText,
        [Parameter(Mandatory = $true)][string]$SectionName
    )
    $pattern = '(?m)^' + [regex]::Escape($SectionName) + '\s+0x[0-9A-Fa-f]+\s+0x(?<size>[0-9A-Fa-f]+)\s*$'
    $match = [regex]::Match($MapText, $pattern)
    if (-not $match.Success) { throw "No se encontro seccion $SectionName en .map" }
    return [Convert]::ToInt64($match.Groups["size"].Value, 16)
}

Write-Host "JWPLC - inspeccion P6A-2 existente / sin compilar" -ForegroundColor Cyan
Write-Host ("P6A1: {0}" -f $BaselineRoot)
Write-Host ("P6A2: {0}" -f $CurrentRoot)
Write-Host ""

foreach ($required in @($BaselineBuild, $CurrentBuild, $BaselineLog, $CurrentLog, $TimingPath))
{
    if (-not (Test-Path -LiteralPath $required)) { throw "Falta artefacto requerido: $required" }
}

$baseMetrics = Get-CompileMetrics -BuildPath $BaselineBuild
$currMetrics = Get-CompileMetrics -BuildPath $CurrentBuild
$baseE = Get-PreprocessCount -LogPath $BaselineLog
$currE = Get-PreprocessCount -LogPath $CurrentLog

if ($baseMetrics.Total -ne 24 -or $currMetrics.Total -ne 20 -or
    $baseMetrics.ST77xx -ne 4 -or $currMetrics.ST77xx -ne 0 -or
    $baseMetrics.GFX -ne 4 -or $currMetrics.GFX -ne 4 -or
    $baseMetrics.BusIO -ne 4 -or $currMetrics.BusIO -ne 4 -or
    $baseMetrics.Ethernet -ne 0 -or $currMetrics.Ethernet -ne 0 -or
    $baseMetrics.Display -ne 0 -or $currMetrics.Display -ne 0 -or
    $baseMetrics.Stub -ne 1 -or $currMetrics.Stub -ne 1 -or
    $baseE -ne 41 -or $currE -ne 37)
{
    throw "Estructura P6A-1/P6A-2 inesperada."
}

$baseSelections = Get-LibrarySelections -LogPath $BaselineLog
$currSelections = Get-LibrarySelections -LogPath $CurrentLog
$onlyBaseSelection = @($baseSelections | Where-Object { $currSelections -notcontains $_ })
$onlyCurrSelection = @($currSelections | Where-Object { $baseSelections -notcontains $_ })
if ($onlyBaseSelection.Count -ne 0 -or $onlyCurrSelection.Count -ne 0)
{
    throw "Cambio en seleccion de librerias entre P6A-1 y P6A-2."
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
if ($baseBin.Length -ne 406032 -or $currBin.Length -ne 405712)
{
    throw ("Tamano app inesperado. P6A1={0}, P6A2={1}" -f $baseBin.Length, $currBin.Length)
}

$baseMap = Get-AppMap -BuildPath $BaselineBuild
$currMap = Get-AppMap -BuildPath $CurrentBuild
$baseMapText = Get-Content -LiteralPath $baseMap.FullName -Raw
$currMapText = Get-Content -LiteralPath $currMap.FullName -Raw

if (-not $currMapText.Contains("libAdafruit_ST7735_and_ST7789_Library.a") -or
    -not $currMapText.Contains("Adafruit_ST7789.cpp.o") -or
    -not $currMapText.Contains("Adafruit_ST77xx.cpp.o"))
{
    throw "P6A-2 no demuestra enlace de ST7789/ST77xx desde el archive."
}
if ($currMapText.Contains("Adafruit_ST7735.cpp.o") -or $currMapText.Contains("Adafruit_ST7796S.cpp.o"))
{
    throw "P6A-2 extrajo miembros ST7735/ST7796S aunque el autoload no los usa."
}
if (-not $baseMapText.Contains("Adafruit_ST7735.cpp.o") -or -not $baseMapText.Contains("Adafruit_ST7796S.cpp.o"))
{
    throw "P6A-1 baseline no contiene los objetos ST7735/ST7796S esperados."
}

$baseRodata = Get-MapSectionSize -MapText $baseMapText -SectionName ".flash.rodata"
$currRodata = Get-MapSectionSize -MapText $currMapText -SectionName ".flash.rodata"
$baseText = Get-MapSectionSize -MapText $baseMapText -SectionName ".flash.text"
$currText = Get-MapSectionSize -MapText $currMapText -SectionName ".flash.text"
if (($currRodata - $baseRodata) -ne -312 -or ($currText - $baseText) -ne -4)
{
    throw ("Deltas de seccion inesperados. rodata={0}, text={1}" -f ($currRodata - $baseRodata), ($currText - $baseText))
}

$ehSt7735 = [regex]::Matches($baseMapText, '(?m)^\s*\.eh_frame\s+0x[0-9A-Fa-f]+\s+0xa0\s+.*Adafruit_ST7735\.cpp\.o\s*$').Count
$ehSt7796 = [regex]::Matches($baseMapText, '(?m)^\s*\.eh_frame\s+0x[0-9A-Fa-f]+\s+0x88\s+.*Adafruit_ST7796S\.cpp\.o\s*$').Count
if ($ehSt7735 -ne 1 -or $ehSt7796 -ne 1)
{
    throw "No se pudo atribuir el recorte rodata a .eh_frame de ST7735/ST7796S."
}

$baseElf = Get-AppElf -BuildPath $BaselineBuild
$currElf = Get-AppElf -BuildPath $CurrentBuild
$nm = Find-Nm -LogPath $CurrentLog
$baseNm = @(Get-NmRecords -NmPath $nm -ElfPath $baseElf.FullName)
$currNm = @(Get-NmRecords -NmPath $nm -ElfPath $currElf.FullName)
$baseSig = Get-NmSignatureTable -Records $baseNm
$currSig = Get-NmSignatureTable -Records $currNm

$onlyBaseSymbols = @($baseSig.Keys | Where-Object { -not $currSig.ContainsKey($_) })
$onlyCurrSymbols = @($currSig.Keys | Where-Object { -not $baseSig.ContainsKey($_) })
if ($onlyBaseSymbols.Count -ne 0 -or $onlyCurrSymbols.Count -ne 0)
{
    throw ("Cambio en conjunto de simbolos definidos. solo P6A1={0}, solo P6A2={1}" -f $onlyBaseSymbols.Count, $onlyCurrSymbols.Count)
}

$sizeDiffs = New-Object System.Collections.Generic.List[string]
foreach ($key in $baseSig.Keys)
{
    if ($baseSig[$key] -ne $currSig[$key])
    {
        [void]$sizeDiffs.Add(("{0}: {1} -> {2}" -f $key, $baseSig[$key], $currSig[$key]))
    }
}
$expectedDestructorKey = "W|_ZN15Adafruit_ST7789D0Ev"
if ($sizeDiffs.Count -ne 1 -or -not $baseSig.ContainsKey($expectedDestructorKey) -or
    $baseSig[$expectedDestructorKey] -ne "18" -or $currSig[$expectedDestructorKey] -ne "15")
{
    throw ("Diferencias de tamano de simbolos inesperadas: {0}" -f ($sizeDiffs -join "; "))
}

$timingText = (Get-Content -LiteralPath $TimingPath -Raw).Trim()
$timing = [double]::Parse($timingText, [System.Globalization.CultureInfo]::InvariantCulture)

Write-Host ("Estructura: 24 -> 20 compiles | ST77xx 4 -> 0 | GFX=4 | BusIO=4 | Eth=0 | Display=0 | stub=1 | -E 41 -> 37") -ForegroundColor Green
Write-Host ("Objetos externos: comunes={0} | SHA distintos=0 | solo P6A1=0 | solo P6A2=0" -f $commonObjects.Count) -ForegroundColor Green
Write-Host ("Archive: ST7789=True | ST77xx=True | ST7735 extraido=False | ST7796S extraido=False") -ForegroundColor Green
Write-Host ("App: {0} -> {1} bytes | delta={2}" -f $baseBin.Length, $currBin.Length, ($currBin.Length - $baseBin.Length))
Write-Host (".flash.rodata: {0} -> {1} | delta={2}" -f $baseRodata, $currRodata, ($currRodata - $baseRodata))
Write-Host (".flash.text: {0} -> {1} | delta={2}" -f $baseText, $currText, ($currText - $baseText))
Write-Host "Recorte baseline identificado: .eh_frame ST7735=160 B + ST7796S=136 B; el resto corresponde a alineacion/relajacion de enlace." -ForegroundColor Yellow
Write-Host ("Simbolos definidos: P6A1={0} | P6A2={1} | perdidos=0 | nuevos=0" -f $baseNm.Count, $currNm.Count) -ForegroundColor Green
Write-Host "Unico cambio de tamano de simbolo: destructor weak Adafruit_ST7789 18 -> 15 B (relajacion del linker)." -ForegroundColor Yellow
Write-Host ("Cold preservado P6A-2: {0:N3} s" -f $timing) -ForegroundColor Green
Write-Host ""
Write-Host "=== P6A-2 EXISTENTE: VALIDADO ESTRUCTURALMENTE ===" -ForegroundColor Green
Write-Host "La reduccion de 320 B no corresponde a perdida de simbolos del firmware: el archive deja de extraer ST7735/ST7796S no usados." -ForegroundColor Green
Write-Host "No se ejecutaron compilaciones. Mantener prueba fisica TFT como gate funcional final." -ForegroundColor DarkGray
