#requires -Version 7.4

[CmdletBinding()]
param()

Set-StrictMode -Version 2.0
$ErrorActionPreference = "Stop"

$ScriptRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$RepoRoot = [System.IO.Path]::GetFullPath((Join-Path $ScriptRoot "..\.."))
$GfxRoot = Join-Path $RepoRoot "JWPLC\2.1.0\libraries\Adafruit_GFX_Library"
$GfxSource = Join-Path $GfxRoot "src\Adafruit_GFX.cpp"

$BaselineRunId = "20260809_234653"
$CurrentRunId = "20260809_235326"
$BaselineRoot = Join-Path (Join-Path $ScriptRoot "p6b-gfx-layout-work") $BaselineRunId
$CurrentRoot = Join-Path (Join-Path $ScriptRoot "p6b2-gfx-precompiled-work") $CurrentRunId
$BaselineBuild = Join-Path $BaselineRoot "p6b-layout-Basic"
$CurrentBuild = Join-Path $CurrentRoot "p6b2-Basic"
$BaselineLog = Join-Path $BaselineRoot "p6b-layout-Basic.log"
$CurrentLog = Join-Path $CurrentRoot "p6b2-Basic.log"
$TimingPath = Join-Path $CurrentRoot "P6B2_TIMING_SECONDS.txt"

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

function Get-DiscoveryPreprocessSources
{
    param([Parameter(Mandatory = $true)][string]$LogPath)
    $result = New-Object System.Collections.Generic.List[string]
    foreach ($line in Get-Content -LiteralPath $LogPath)
    {
        $text = [string]$line
        if ($text -notmatch 'xtensa-esp32-elf-g\+\+.*\s-E\s') { continue }
        if ($text -match '\s(?<src>[A-Za-z]:\\[^\r\n]+?\.(?:cpp|c))\s+-o\s+nul\s*$')
        {
            [void]$result.Add([System.IO.Path]::GetFileName($Matches["src"]))
        }
    }
    return @($result)
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
        if ($relative -match '^Adafruit_GFX_Library[\\/]') { continue }
        $table[$relative] = (Get-FileHash -LiteralPath $file.FullName -Algorithm SHA256).Hash.ToLowerInvariant()
    }
    return $table
}

function Get-AppFile
{
    param(
        [Parameter(Mandatory = $true)][string]$BuildPath,
        [Parameter(Mandatory = $true)][string]$Extension
    )
    $file = Get-ChildItem -LiteralPath $BuildPath -Filter ("*.ino." + $Extension) -File | Select-Object -First 1
    if ($null -eq $file) { throw "No se encontro .ino.$Extension en $BuildPath" }
    return $file
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
        if ($line -match '"(?<exe>[^"]*xtensa-esp32-elf-g\+\+(?:\.exe)?)"') { $compilerCandidate = $Matches["exe"] }
        elseif ($line -match '(?<exe>\S*xtensa-esp32-elf-g\+\+(?:\.exe)?)\s') { $compilerCandidate = $Matches["exe"] }
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
    throw "No se pudo localizar xtensa-esp32-elf-nm."
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

function Get-GlobalDefinedSymbolSet
{
    param(
        [Parameter(Mandatory = $true)][string]$NmPath,
        [Parameter(Mandatory = $true)][string]$ElfPath
    )
    $native = Invoke-NativeCaptured -FilePath $NmPath -Arguments @("-g", "--defined-only", $ElfPath)
    if ($native.ExitCode -ne 0) { throw "nm fallo para $ElfPath" }
    $set = @{}
    foreach ($line in $native.Output)
    {
        $text = ([string]$line).Trim()
        if ($text -match '^(?:[0-9A-Fa-f]+\s+)?\S\s+(?<name>\S+)$') { $set[$Matches["name"]] = $true }
    }
    return $set
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

Write-Host "JWPLC - inspeccion P6B-2 existente / sin compilar" -ForegroundColor Cyan
Write-Host ("P6B1: {0}" -f $BaselineRoot)
Write-Host ("P6B2: {0}" -f $CurrentRoot)
Write-Host ""

foreach ($required in @($BaselineBuild, $CurrentBuild, $BaselineLog, $CurrentLog, $TimingPath, $GfxSource))
{
    if (-not (Test-Path -LiteralPath $required)) { throw "Falta artefacto requerido: $required" }
}

$baseMetrics = Get-CompileMetrics -BuildPath $BaselineBuild
$currMetrics = Get-CompileMetrics -BuildPath $CurrentBuild
$baseE = Get-PreprocessCount -LogPath $BaselineLog
$currE = Get-PreprocessCount -LogPath $CurrentLog

if ($baseMetrics.Total -ne 20 -or $currMetrics.Total -ne 16 -or
    $baseMetrics.ST77xx -ne 0 -or $currMetrics.ST77xx -ne 0 -or
    $baseMetrics.GFX -ne 4 -or $currMetrics.GFX -ne 0 -or
    $baseMetrics.BusIO -ne 4 -or $currMetrics.BusIO -ne 4 -or
    $baseMetrics.Ethernet -ne 0 -or $currMetrics.Ethernet -ne 0 -or
    $baseMetrics.Display -ne 0 -or $currMetrics.Display -ne 0 -or
    $baseMetrics.Stub -ne 1 -or $currMetrics.Stub -ne 1 -or
    $baseE -ne 37 -or $currE -ne 33)
{
    throw "Estructura P6B-1/P6B-2 inesperada."
}

$baseDiscovery = @(Get-DiscoveryPreprocessSources -LogPath $BaselineLog)
$gfxDiscovery = @($baseDiscovery | Where-Object { $_ -in @("Adafruit_GFX.cpp", "Adafruit_GrayOLED.cpp", "Adafruit_SPITFT.cpp", "glcdfont.c") } | Sort-Object -Unique)
if (($gfxDiscovery -join "|") -ne ((@("Adafruit_GFX.cpp", "Adafruit_GrayOLED.cpp", "Adafruit_SPITFT.cpp", "glcdfont.c") | Sort-Object) -join "|"))
{
    throw ("El baseline no demuestra los 4 preprocesados discovery de GFX. Actual={0}" -f ($gfxDiscovery -join ","))
}

$gfxText = Get-Content -LiteralPath $GfxSource -Raw
if ($gfxText -notmatch '#include\s+"glcdfont\.c"')
{
    throw "Adafruit_GFX.cpp ya no incluye glcdfont.c textualmente; revisar supuesto P6B-2."
}

$baseSelections = Get-LibrarySelections -LogPath $BaselineLog
$currSelections = Get-LibrarySelections -LogPath $CurrentLog
$onlyBaseSelection = @($baseSelections | Where-Object { $currSelections -notcontains $_ })
$onlyCurrSelection = @($currSelections | Where-Object { $baseSelections -notcontains $_ })
if ($onlyBaseSelection.Count -ne 0 -or $onlyCurrSelection.Count -ne 0) { throw "Cambio en seleccion de librerias." }

$baseObjects = Get-ExternalObjectTable -BuildPath $BaselineBuild
$currObjects = Get-ExternalObjectTable -BuildPath $CurrentBuild
$commonObjects = @($baseObjects.Keys | Where-Object { $currObjects.ContainsKey($_) })
$objectMismatch = @($commonObjects | Where-Object { $baseObjects[$_] -ne $currObjects[$_] })
$onlyBaseObjects = @($baseObjects.Keys | Where-Object { -not $currObjects.ContainsKey($_) })
$onlyCurrObjects = @($currObjects.Keys | Where-Object { -not $baseObjects.ContainsKey($_) })
if ($objectMismatch.Count -ne 0 -or $onlyBaseObjects.Count -ne 0 -or $onlyCurrObjects.Count -ne 0) { throw "Cambios en objetos externos a GFX." }

$baseBin = Get-AppFile -BuildPath $BaselineBuild -Extension "bin"
$currBin = Get-AppFile -BuildPath $CurrentBuild -Extension "bin"
$baseElf = Get-AppFile -BuildPath $BaselineBuild -Extension "elf"
$currElf = Get-AppFile -BuildPath $CurrentBuild -Extension "elf"
$baseMap = Get-AppFile -BuildPath $BaselineBuild -Extension "map"
$currMap = Get-AppFile -BuildPath $CurrentBuild -Extension "map"
$baseMapText = Get-Content -LiteralPath $baseMap.FullName -Raw
$currMapText = Get-Content -LiteralPath $currMap.FullName -Raw

$archiveLinked = $currMapText.Contains("libAdafruit_GFX_Library.a")
$gfxLinked = $currMapText.Contains("Adafruit_GFX.cpp.o")
$spiLinked = $currMapText.Contains("Adafruit_SPITFT.cpp.o")
$grayLinked = $currMapText.Contains("Adafruit_GrayOLED.cpp.o")
$fontObjectLinked = $currMapText.Contains("glcdfont.c.o")
if (-not $archiveLinked -or -not $gfxLinked -or -not $spiLinked -or $grayLinked -or $fontObjectLinked)
{
    throw "Extraccion de miembros GFX inesperada en P6B-2."
}
if (-not $baseMapText.Contains("Adafruit_GrayOLED.cpp.o") -or -not $baseMapText.Contains("glcdfont.c.o"))
{
    throw "Baseline P6B-1 no contiene GrayOLED/glcdfont como objetos directos."
}

if ($baseBin.Length -ne 405712 -or $currBin.Length -ne 405472)
{
    throw ("Tamano app inesperado. P6B1={0}, P6B2={1}" -f $baseBin.Length, $currBin.Length)
}

$baseRodata = Get-MapSectionSize -MapText $baseMapText -SectionName ".flash.rodata"
$currRodata = Get-MapSectionSize -MapText $currMapText -SectionName ".flash.rodata"
$baseText = Get-MapSectionSize -MapText $baseMapText -SectionName ".flash.text"
$currText = Get-MapSectionSize -MapText $currMapText -SectionName ".flash.text"
if (($currRodata - $baseRodata) -ne -244 -or ($currText - $baseText) -ne 0)
{
    throw ("Deltas de seccion inesperados. rodata={0}, text={1}" -f ($currRodata - $baseRodata), ($currText - $baseText))
}

$nm = Find-Nm -LogPath $BaselineLog
$baseSymbols = Get-GlobalDefinedSymbolSet -NmPath $nm -ElfPath $baseElf.FullName
$currSymbols = Get-GlobalDefinedSymbolSet -NmPath $nm -ElfPath $currElf.FullName
$missing = @($baseSymbols.Keys | Where-Object { -not $currSymbols.ContainsKey($_) })
$new = @($currSymbols.Keys | Where-Object { -not $baseSymbols.ContainsKey($_) })
if ($missing.Count -ne 0 -or $new.Count -ne 0 -or $baseSymbols.Count -ne 2782 -or $currSymbols.Count -ne 2782)
{
    throw ("Conjunto de simbolos globales cambio. baseline={0}, actual={1}, perdidos={2}, nuevos={3}" -f $baseSymbols.Count, $currSymbols.Count, $missing.Count, $new.Count)
}

$seconds = [double]::Parse((Get-Content -LiteralPath $TimingPath -Raw).Trim(), [System.Globalization.CultureInfo]::InvariantCulture)
if ([Math]::Abs($seconds - 77.907) -gt 0.01)
{
    Write-Host ("Aviso: timing preservado={0:N3} s; se esperaba aproximadamente 77.907 s por consola." -f $seconds) -ForegroundColor Yellow
}

Write-Host "Estructura: 20 -> 16 compiles | GFX 4 -> 0 | BusIO=4 | ST77xx=0 | Eth=0 | Display=0 | stub=1 | -E 37 -> 33" -ForegroundColor Green
Write-Host ("Discovery GFX baseline: {0}" -f ($gfxDiscovery -join ", ")) -ForegroundColor Green
Write-Host "glcdfont.c: preprocesado como TU durante discovery, pero tambien incluido textualmente por Adafruit_GFX.cpp." -ForegroundColor Green
Write-Host ("Objetos externos: comunes={0} | SHA distintos=0 | solo P6B1=0 | solo P6B2=0" -f $commonObjects.Count) -ForegroundColor Green
Write-Host ("Archive: GFX={0} | SPITFT={1} | GrayOLED extraido={2} | glcdfont.c.o extraido={3}" -f $gfxLinked, $spiLinked, $grayLinked, $fontObjectLinked)
Write-Host ("App: {0} -> {1} bytes | delta={2}" -f $baseBin.Length, $currBin.Length, ($currBin.Length - $baseBin.Length))
Write-Host ("Secciones: .flash.rodata delta={0} | .flash.text delta={1}" -f ($currRodata - $baseRodata), ($currText - $baseText))
Write-Host ("Simbolos globales: {0} -> {1} | perdidos=0 | nuevos=0" -f $baseSymbols.Count, $currSymbols.Count) -ForegroundColor Green
Write-Host ("Cold preservado P6B-2: {0:N3} s" -f $seconds) -ForegroundColor Green
Write-Host ""
Write-Host "=== P6B-2 EXISTENTE: VALIDADO ESTRUCTURALMENTE ===" -ForegroundColor Green
Write-Host "El -E correcto es 33: Arduino discovery preprocesaba los cuatro archivos GFX, incluido glcdfont.c, usando g++ -x c++." -ForegroundColor Yellow
Write-Host "glcdfont.c.o no necesita extraerse del archive porque Adafruit_GFX.cpp ya incorpora glcdfont.c mediante #include; GrayOLED tampoco es usado por este firmware." -ForegroundColor Yellow
Write-Host "No se ejecutaron compilaciones. Mantener prueba fisica TFT como gate funcional final." -ForegroundColor DarkGray
