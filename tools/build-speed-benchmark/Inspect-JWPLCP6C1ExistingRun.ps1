#requires -Version 7.4

[CmdletBinding()]
param()

Set-StrictMode -Version 2.0
$ErrorActionPreference = "Stop"

$ScriptRoot = Split-Path -Parent $MyInvocation.MyCommand.Path

$BaselineRunId = "20260809_235326"
$CurrentRunId = "20260810_000915"
$BaselineRoot = Join-Path (Join-Path $ScriptRoot "p6b2-gfx-precompiled-work") $BaselineRunId
$CurrentRoot = Join-Path (Join-Path $ScriptRoot "p6c-busio-layout-work") $CurrentRunId
$BaselineBuild = Join-Path $BaselineRoot "p6b2-Basic"
$CurrentBuild = Join-Path $CurrentRoot "p6c-layout-Basic"
$BaselineLog = Join-Path $BaselineRoot "p6b2-Basic.log"
$CurrentLog = Join-Path $CurrentRoot "p6c-layout-Basic.log"
$TimingPath = Join-Path $CurrentRoot "P6C1_TIMING_SECONDS.txt"

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

function Find-ToolSibling
{
    param(
        [Parameter(Mandatory = $true)][string]$LogPath,
        [Parameter(Mandatory = $true)][string[]]$LeafNames
    )

    foreach ($line in Get-Content -LiteralPath $LogPath)
    {
        $compilerCandidate = $null
        if ($line -match '"(?<exe>[^"]*xtensa-esp32-elf-g\+\+(?:\.exe)?)"') { $compilerCandidate = $Matches["exe"] }
        elseif ($line -match '(?<exe>\S*xtensa-esp32-elf-g\+\+(?:\.exe)?)\s') { $compilerCandidate = $Matches["exe"] }
        if ([string]::IsNullOrWhiteSpace($compilerCandidate)) { continue }

        $compiler = Resolve-NativeToolPath -Candidate $compilerCandidate
        if ([string]::IsNullOrWhiteSpace($compiler)) { continue }
        $toolDir = Split-Path -Parent $compiler
        foreach ($leaf in $LeafNames)
        {
            $candidate = Join-Path $toolDir $leaf
            if (Test-Path -LiteralPath $candidate) { return (Resolve-Path -LiteralPath $candidate).Path }
        }
    }

    throw ("No se pudo localizar herramienta sibling: {0}" -f ($LeafNames -join ", "))
}

function ConvertTo-EntryList
{
    param([Parameter(Mandatory = $true)]$Parsed)
    $entries = New-Object System.Collections.Generic.List[object]
    foreach ($entry in $Parsed)
    {
        if ($null -eq $entry) { throw "compile_commands.json contiene una entrada nula." }
        [void]$entries.Add($entry)
    }
    if ($entries.Count -eq 0) { throw "compile_commands.json esta vacio." }
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
        Entries = @($entries)
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
    if (-not (Test-Path -LiteralPath $root)) { return $table }

    foreach ($file in Get-ChildItem -LiteralPath $root -Recurse -File -Filter "*.o")
    {
        $relative = $file.FullName.Substring($root.Length).TrimStart('\','/')
        if ($relative -match '^Adafruit_BusIO[\\/]') { continue }
        $table[$relative] = [PSCustomObject]@{
            Path = $file.FullName
            Sha = (Get-FileHash -LiteralPath $file.FullName -Algorithm SHA256).Hash.ToLowerInvariant()
            Length = [int64]$file.Length
        }
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

function Test-PayloadEquivalent
{
    param(
        [Parameter(Mandatory = $true)][string]$ReferencePath,
        [Parameter(Mandatory = $true)][string]$CandidatePath
    )

    $a = [System.IO.File]::ReadAllBytes($ReferencePath)
    $b = [System.IO.File]::ReadAllBytes($CandidatePath)
    if ($a.Length -ne $b.Length -or $a.Length -lt 256) { return $false }

    $elfHashStart = 0xB0
    $elfHashEnd = 0xCF
    $tailStart = $a.Length - 33

    for ($i = 0; $i -lt $a.Length; $i++)
    {
        if (($i -ge $elfHashStart -and $i -le $elfHashEnd) -or ($i -ge $tailStart)) { continue }
        if ($a[$i] -ne $b[$i]) { return $false }
    }
    return $true
}

function Get-GlobalDefinedSymbolSet
{
    param(
        [Parameter(Mandatory = $true)][string]$NmPath,
        [Parameter(Mandatory = $true)][string]$ObjectPath
    )

    $result = Invoke-NativeCaptured -FilePath $NmPath -Arguments @("-g", "--defined-only", $ObjectPath)
    if ($result.ExitCode -ne 0) { throw "nm fallo para $ObjectPath" }

    $set = @{}
    foreach ($line in $result.Output)
    {
        $text = ([string]$line).Trim()
        if ($text -match '^(?:[0-9A-Fa-f]+\s+)?\S\s+(?<name>\S+)$')
        {
            $set[$Matches["name"]] = $true
        }
    }
    return $set
}

function Get-EntryForObject
{
    param(
        [Parameter(Mandatory = $true)][object[]]$Entries,
        [Parameter(Mandatory = $true)][string]$ObjectRelative
    )

    $leaf = [System.IO.Path]::GetFileName($ObjectRelative)
    $sourceLeaf = $leaf -replace '\.o$', ''
    return @($Entries | Where-Object { [System.IO.Path]::GetFileName([string]$_.file) -eq $sourceLeaf } | Select-Object -First 1)
}

Write-Host "JWPLC - inspeccion P6C-1 existente / sin compilar" -ForegroundColor Cyan
Write-Host ("P6B2: {0}" -f $BaselineRoot)
Write-Host ("P6C1: {0}" -f $CurrentRoot)
Write-Host ""

foreach ($required in @($BaselineBuild, $CurrentBuild, $BaselineLog, $CurrentLog, $TimingPath))
{
    if (-not (Test-Path -LiteralPath $required)) { throw "Falta artefacto requerido: $required" }
}

$baseMetrics = Get-CompileMetrics -BuildPath $BaselineBuild
$currMetrics = Get-CompileMetrics -BuildPath $CurrentBuild
$baseE = Get-PreprocessCount -LogPath $BaselineLog
$currE = Get-PreprocessCount -LogPath $CurrentLog

if ($baseMetrics.Total -ne 16 -or $currMetrics.Total -ne 16 -or
    $baseMetrics.ST77xx -ne 0 -or $currMetrics.ST77xx -ne 0 -or
    $baseMetrics.GFX -ne 0 -or $currMetrics.GFX -ne 0 -or
    $baseMetrics.BusIO -ne 4 -or $currMetrics.BusIO -ne 4 -or
    $baseMetrics.Ethernet -ne 0 -or $currMetrics.Ethernet -ne 0 -or
    $baseMetrics.Display -ne 0 -or $currMetrics.Display -ne 0 -or
    $baseMetrics.Stub -ne 1 -or $currMetrics.Stub -ne 1 -or
    $baseE -ne 33 -or $currE -ne 33)
{
    throw "Estructura P6B-2/P6C-1 inesperada."
}

$baseSelections = Get-LibrarySelections -LogPath $BaselineLog
$currSelections = Get-LibrarySelections -LogPath $CurrentLog
$onlyBaseSelection = @($baseSelections | Where-Object { $currSelections -notcontains $_ })
$onlyCurrSelection = @($currSelections | Where-Object { $baseSelections -notcontains $_ })
if ($onlyBaseSelection.Count -ne 0 -or $onlyCurrSelection.Count -ne 0)
{
    throw "Cambio en seleccion de librerias entre P6B-2 y P6C-1."
}

$baseObjects = Get-ExternalObjectTable -BuildPath $BaselineBuild
$currObjects = Get-ExternalObjectTable -BuildPath $CurrentBuild
$commonObjects = @($baseObjects.Keys | Where-Object { $currObjects.ContainsKey($_) } | Sort-Object)
$objectMismatch = @($commonObjects | Where-Object { $baseObjects[$_].Sha -ne $currObjects[$_].Sha })
$onlyBaseObjects = @($baseObjects.Keys | Where-Object { -not $currObjects.ContainsKey($_) } | Sort-Object)
$onlyCurrObjects = @($currObjects.Keys | Where-Object { -not $baseObjects.ContainsKey($_) } | Sort-Object)

$baseBin = Get-AppFile -BuildPath $BaselineBuild -Extension "bin"
$currBin = Get-AppFile -BuildPath $CurrentBuild -Extension "bin"
$payloadEquivalent = Test-PayloadEquivalent -ReferencePath $baseBin.FullName -CandidatePath $currBin.FullName
if ($baseBin.Length -ne 405472 -or $currBin.Length -ne 405472 -or -not $payloadEquivalent)
{
    throw ("App P6C-1 no conserva payload esperado. bytes={0}/{1}, payload={2}" -f $baseBin.Length, $currBin.Length, $payloadEquivalent)
}

$nm = Find-ToolSibling -LogPath $CurrentLog -LeafNames @("xtensa-esp32-elf-nm.exe", "xtensa-esp32-elf-nm")
$objcopy = Find-ToolSibling -LogPath $CurrentLog -LeafNames @("xtensa-esp32-elf-objcopy.exe", "xtensa-esp32-elf-objcopy")
$baseElf = Get-AppFile -BuildPath $BaselineBuild -Extension "elf"
$currElf = Get-AppFile -BuildPath $CurrentBuild -Extension "elf"
$baseSymbols = Get-GlobalDefinedSymbolSet -NmPath $nm -ObjectPath $baseElf.FullName
$currSymbols = Get-GlobalDefinedSymbolSet -NmPath $nm -ObjectPath $currElf.FullName
$missingSymbols = @($baseSymbols.Keys | Where-Object { -not $currSymbols.ContainsKey($_) } | Sort-Object)
$newSymbols = @($currSymbols.Keys | Where-Object { -not $baseSymbols.ContainsKey($_) } | Sort-Object)
if ($missingSymbols.Count -ne 0 -or $newSymbols.Count -ne 0)
{
    throw ("P6C-1 cambio simbolos globales del ELF. perdidos={0}, nuevos={1}" -f $missingSymbols.Count, $newSymbols.Count)
}

Write-Host ("Estructura: 16 -> 16 compiles | BusIO 4 -> 4 | ST77xx=0 | GFX=0 | Eth=0 | Display=0 | stub=1 | -E 33 -> 33")
Write-Host ("App: {0} -> {1} bytes | payload equivalente={2}" -f $baseBin.Length, $currBin.Length, $payloadEquivalent)
Write-Host ("Simbolos globales ELF: {0} -> {1} | perdidos=0 | nuevos=0" -f $baseSymbols.Count, $currSymbols.Count)
Write-Host ("Objetos externos: comunes={0} | SHA distintos={1} | solo P6B2={2} | solo P6C1={3}" -f $commonObjects.Count, $objectMismatch.Count, $onlyBaseObjects.Count, $onlyCurrObjects.Count)

if ($onlyBaseObjects.Count -ne 0 -or $onlyCurrObjects.Count -ne 0)
{
    throw "P6C-1 cambio el inventario de objetos externos a BusIO."
}

if ($objectMismatch.Count -eq 0)
{
    Write-Host "No hay objetos externos distintos; el gate anterior fue inconsistente con los artefactos preservados." -ForegroundColor Yellow
}
else
{
    Write-Host ""
    Write-Host "Objetos externos con SHA distinto:" -ForegroundColor Yellow

    $allExplained = $true
    $tempRoot = Join-Path $CurrentRoot "p6c1-object-diagnostics"
    New-Item -ItemType Directory -Path $tempRoot -Force | Out-Null

    foreach ($relative in $objectMismatch)
    {
        $baseObj = $baseObjects[$relative]
        $currObj = $currObjects[$relative]
        Write-Host ("  {0}" -f $relative) -ForegroundColor Yellow
        Write-Host ("    bytes: {0} -> {1}" -f $baseObj.Length, $currObj.Length)
        Write-Host ("    SHA P6B2: {0}" -f $baseObj.Sha) -ForegroundColor DarkGray
        Write-Host ("    SHA P6C1: {0}" -f $currObj.Sha) -ForegroundColor DarkGray

        $safeName = ($relative -replace '[^A-Za-z0-9_.-]', '_')
        $baseStripped = Join-Path $tempRoot ("base_" + $safeName)
        $currStripped = Join-Path $tempRoot ("curr_" + $safeName)
        $baseBinary = Join-Path $tempRoot ("base_" + $safeName + ".bin")
        $currBinary = Join-Path $tempRoot ("curr_" + $safeName + ".bin")

        $stripA = Invoke-NativeCaptured -FilePath $objcopy -Arguments @("--strip-debug", "--strip-unneeded", $baseObj.Path, $baseStripped)
        $stripB = Invoke-NativeCaptured -FilePath $objcopy -Arguments @("--strip-debug", "--strip-unneeded", $currObj.Path, $currStripped)
        $strippedEqual = $false
        if ($stripA.ExitCode -eq 0 -and $stripB.ExitCode -eq 0 -and (Test-Path -LiteralPath $baseStripped) -and (Test-Path -LiteralPath $currStripped))
        {
            $strippedEqual = ((Get-FileHash -LiteralPath $baseStripped -Algorithm SHA256).Hash -eq (Get-FileHash -LiteralPath $currStripped -Algorithm SHA256).Hash)
        }

        $binA = Invoke-NativeCaptured -FilePath $objcopy -Arguments @("-O", "binary", $baseObj.Path, $baseBinary)
        $binB = Invoke-NativeCaptured -FilePath $objcopy -Arguments @("-O", "binary", $currObj.Path, $currBinary)
        $allocBinaryEqual = $false
        if ($binA.ExitCode -eq 0 -and $binB.ExitCode -eq 0 -and (Test-Path -LiteralPath $baseBinary) -and (Test-Path -LiteralPath $currBinary))
        {
            $allocBinaryEqual = ((Get-FileHash -LiteralPath $baseBinary -Algorithm SHA256).Hash -eq (Get-FileHash -LiteralPath $currBinary -Algorithm SHA256).Hash)
        }

        Write-Host ("    strip-debug/unneeded identico={0} | imagen alloc binaria identica={1}" -f $strippedEqual, $allocBinaryEqual)

        if (-not $strippedEqual -and -not $allocBinaryEqual)
        {
            $allExplained = $false
        }
    }

    if (-not $allExplained)
    {
        Write-Host "Alguno de los objetos externos conserva diferencias en secciones alloc; no se cerrara P6C-1 solo con este inspector." -ForegroundColor Yellow
    }
    else
    {
        Write-Host "Las diferencias de SHA de objetos externos desaparecen al retirar metadata/no afectan su imagen allocatable." -ForegroundColor Green
    }
}

$cold = [double](Get-Content -LiteralPath $TimingPath -Raw).Trim()
Write-Host ("Cold preservado P6C-1: {0:N3} s" -f $cold)
Write-Host ""
Write-Host "=== P6C-1 EXISTENTE: DIAGNOSTICO COMPLETADO ===" -ForegroundColor Green
Write-Host "No se ejecutaron compilaciones." -ForegroundColor DarkGray
Write-Host "El firmware final conserva tamaño, payload y simbolos globales; revisar arriba la causa exacta del objeto externo distinto." -ForegroundColor Yellow
