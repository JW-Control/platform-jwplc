[CmdletBinding()]
param(
    [string]$ArduinoCli = "C:\Users\jeykc\AppData\Local\Programs\arduino-ide\resources\app\lib\backend\resources\arduino-cli.exe",
    [string]$Fqbn = "jwplc_local:esp32:jwplcbasic"
)

Set-StrictMode -Version 2.0
$ErrorActionPreference = "Stop"

$ScriptRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$RepoRoot = [System.IO.Path]::GetFullPath((Join-Path $ScriptRoot "..\.."))
$LibraryRoot = Join-Path $RepoRoot "JWPLC\2.1.0\libraries\JWPLC_Display"
$PropertiesPath = Join-Path $LibraryRoot "library.properties"
$ArchivePath = Join-Path $LibraryRoot "src\esp32\libJWPLC_Display.a"
$SketchPath = Join-Path $ScriptRoot "sketches\17_alpha6_err_code_visual"
$RunId = (Get-Date).ToString("yyyyMMdd_HHmmss")
$RunRoot = Join-Path $ScriptRoot ("results\alpha6-display-final-integrated-" + $RunId)
$SourceBuild = Join-Path $RunRoot "source"
$ArchiveBuild = Join-Path $RunRoot "archive"
$ExtractDir = Join-Path $RunRoot "archive-members"
$SourceLog = Join-Path $RunRoot "source.log"
$ArchiveLog = Join-Path $RunRoot "archive.log"
$BackupArchive = Join-Path ([System.IO.Path]::GetTempPath()) ("jwplc-display-old-" + $RunId + ".a")
$Utf8NoBom = New-Object System.Text.UTF8Encoding($false)

function Invoke-Captured
{
    param([string]$FilePath, [string[]]$Arguments)

    $old = $ErrorActionPreference
    $output = @()
    $exitCode = -1
    try
    {
        $ErrorActionPreference = "Continue"
        $output = @(& $FilePath @Arguments 2>&1 | ForEach-Object { $_.ToString() })
        $exitCode = $LASTEXITCODE
    }
    finally
    {
        $ErrorActionPreference = $old
    }

    return [PSCustomObject]@{ ExitCode = [int]$exitCode; Output = $output }
}

function Resolve-Archiver
{
    param([string[]]$Lines)

    foreach ($line in $Lines)
    {
        $candidate = $null
        if ($line -match '"(?<exe>[^"]*xtensa-esp32-elf-gcc-ar(?:\.exe)?)"')
        {
            $candidate = $Matches["exe"]
        }
        elseif ($line -match '(?<exe>\S*xtensa-esp32-elf-gcc-ar(?:\.exe)?)\s')
        {
            $candidate = $Matches["exe"]
        }

        if (-not [string]::IsNullOrWhiteSpace($candidate) -and (Test-Path -LiteralPath $candidate))
        {
            return (Resolve-Path -LiteralPath $candidate).Path
        }
    }

    foreach ($line in $Lines)
    {
        if ($line -match '"(?<exe>[^"]*xtensa-esp32-elf-g\+\+(?:\.exe)?)"')
        {
            $gxx = $Matches["exe"]
            $dir = Split-Path -Parent $gxx
            foreach ($name in @("xtensa-esp32-elf-gcc-ar.exe", "xtensa-esp32-elf-gcc-ar"))
            {
                $candidate = Join-Path $dir $name
                if (Test-Path -LiteralPath $candidate)
                {
                    return (Resolve-Path -LiteralPath $candidate).Path
                }
            }
        }
    }

    throw "No se pudo localizar xtensa-esp32-elf-gcc-ar desde el build source."
}

function Get-OneFile
{
    param([string]$Dir, [string]$Filter)
    $file = Get-ChildItem -LiteralPath $Dir -File -Filter $Filter | Select-Object -First 1
    if ($null -eq $file) { throw "No se encontro $Filter en $Dir" }
    return $file
}

function Get-MapData
{
    param([string]$MapPath)

    $sections = @{}
    $symbols = @{}
    $fills = New-Object System.Collections.Generic.List[object]

    foreach ($line in Get-Content -LiteralPath $MapPath)
    {
        if ($line -match '^(?<name>\.[^\s]+)\s+0x(?<addr>[0-9a-fA-F]+)\s+0x(?<size>[0-9a-fA-F]+)(?:\s|$)')
        {
            $name = $Matches["name"]
            if (-not $sections.ContainsKey($name))
            {
                $sections[$name] = [PSCustomObject]@{
                    Address = [Convert]::ToInt64($Matches["addr"], 16)
                    Size = [Convert]::ToInt64($Matches["size"], 16)
                }
            }
            continue
        }

        if ($line -match '^\s+\*fill\*\s+0x(?<addr>[0-9a-fA-F]+)\s+0x(?<size>[0-9a-fA-F]+)')
        {
            $fills.Add([PSCustomObject]@{
                Address = [Convert]::ToInt64($Matches["addr"], 16)
                Size = [Convert]::ToInt64($Matches["size"], 16)
            })
            continue
        }

        if ($line -match '^\s+0x(?<addr>[0-9a-fA-F]+)\s+(?<name>[^\s=]+)\s*$')
        {
            $name = $Matches["name"]
            if ($name -notmatch '^(?:0x|\.|\*|LOAD$|PROVIDE|ASSERT)')
            {
                if (-not $symbols.ContainsKey($name))
                {
                    $symbols[$name] = [Convert]::ToInt64($Matches["addr"], 16)
                }
            }
        }
    }

    return [PSCustomObject]@{
        Sections = $sections
        Symbols = $symbols
        Fills = $fills
    }
}

function Get-AppUsage
{
    param([object[]]$Output)
    foreach ($line in $Output)
    {
        if ([string]$line -match '(?:Sketch uses|El Sketch usa)\s+(?<n>\d+)\s+bytes')
        {
            return [int64]$Matches["n"]
        }
    }
    throw "No se pudo leer uso de flash desde la salida Arduino CLI."
}

function Get-RamUsage
{
    param([object[]]$Output)
    foreach ($line in $Output)
    {
        if ([string]$line -match '(?:Global variables use|Las variables Globales usan)\s+(?<n>\d+)\s+bytes')
        {
            return [int64]$Matches["n"]
        }
    }
    throw "No se pudo leer uso de RAM desde la salida Arduino CLI."
}

function Get-ArchivePropertiesText
{
    param([string]$SourceText, [string]$LineEnding)

    $withoutPrecompiled = [regex]::Replace(
        $SourceText,
        '(?m)^precompiled=full\s*\r?\n?',
        '')

    $ldflagsRegex = [regex]'(?m)^ldflags='
    if ($ldflagsRegex.IsMatch($withoutPrecompiled))
    {
        return $ldflagsRegex.Replace(
            $withoutPrecompiled,
            ("precompiled=full" + $LineEnding + "ldflags="),
            1)
    }

    $suffix = if ($withoutPrecompiled.EndsWith($LineEnding)) { "" } else { $LineEnding }
    return $withoutPrecompiled + $suffix + "precompiled=full" + $LineEnding
}

Set-Location $RepoRoot

$branch = (git branch --show-current).Trim()
$expectedBranch = "v2.1.0-alpha.6/integration/rebase-alpha5-final"
if ($branch -ne $expectedBranch)
{
    throw "Branch incorrecto: $branch. Esperado: $expectedBranch"
}

if (@(git status --porcelain).Count -ne 0)
{
    git status --short
    throw "Repo no limpio antes de regenerar Display."
}

if (-not (Test-Path -LiteralPath $ArduinoCli)) { throw "No existe Arduino CLI: $ArduinoCli" }
if (-not (Test-Path -LiteralPath $PropertiesPath)) { throw "Falta $PropertiesPath" }
if (-not (Test-Path -LiteralPath $SketchPath)) { throw "Falta sketch 17: $SketchPath" }

$originalPropertiesBytes = [System.IO.File]::ReadAllBytes($PropertiesPath)
$originalPropertiesText = [System.Text.Encoding]::UTF8.GetString($originalPropertiesBytes)
$lineEnding = if ($originalPropertiesText.Contains("`r`n")) { "`r`n" } else { "`n" }
$sourcePropertiesText = [regex]::Replace(
    $originalPropertiesText,
    '(?m)^precompiled=full\s*\r?\n?',
    '')
$archivePropertiesText = Get-ArchivePropertiesText -SourceText $sourcePropertiesText -LineEnding $lineEnding

$hadOldArchive = Test-Path -LiteralPath $ArchivePath
if ($hadOldArchive)
{
    Copy-Item -LiteralPath $ArchivePath -Destination $BackupArchive -Force
}

New-Item -ItemType Directory -Path $SourceBuild -Force | Out-Null
New-Item -ItemType Directory -Path $ArchiveBuild -Force | Out-Null
New-Item -ItemType Directory -Path $ExtractDir -Force | Out-Null

$success = $false
try
{
    Write-Host "=== ALPHA6 - FINALIZAR JWPLC_Display SOBRE ALPHA5 FINAL ===" -ForegroundColor Cyan
    Write-Host "Branch : $branch"
    Write-Host ("HEAD   : {0}" -f ((git rev-parse --short=8 HEAD).Trim()))
    Write-Host ("Inicio : {0}" -f $(if ($originalPropertiesText -match '(?m)^precompiled=full\s*$') { "precompiled" } else { "source fallback" }))

    # 1) Build source real. La rama integrada puede partir del fallback source de Alpha5.
    [System.IO.File]::WriteAllText($PropertiesPath, $sourcePropertiesText, $Utf8NoBom)
    if (Test-Path -LiteralPath $ArchivePath) { Remove-Item -LiteralPath $ArchivePath -Force }

    Write-Host "[1/4] Cold compile desde source..." -ForegroundColor Cyan
    $sourceArgs = @("compile", "-b", $Fqbn, "-v", "--clean", "--build-path", $SourceBuild, $SketchPath)
    $sourceResult = Invoke-Captured -FilePath $ArduinoCli -Arguments $sourceArgs
    @($sourceResult.Output) | Out-File -LiteralPath $SourceLog -Encoding utf8
    if ($sourceResult.ExitCode -ne 0) { throw "Build source fallo. Revisar $SourceLog" }

    $displayObj = Join-Path $SourceBuild "libraries\JWPLC_Display\JWPLC_Display.cpp.o"
    $idleObj = Join-Path $SourceBuild "libraries\JWPLC_Display\JWPLC_IdleScreen.cpp.o"
    $sourceObjects = @($displayObj, $idleObj)
    foreach ($obj in $sourceObjects)
    {
        if (-not (Test-Path -LiteralPath $obj)) { throw "Falta objeto Display compilado desde source: $obj" }
    }
    $sourceDisplayCompiles = $sourceObjects.Count
    if ($sourceDisplayCompiles -ne 2) { throw "Se esperaban exactamente 2 objetos source de Display." }
    Write-Host ("Source Display objects: {0}/2" -f $sourceDisplayCompiles) -ForegroundColor Green

    # 2) Crear el archive exclusivamente con los dos objetos recien compilados.
    $archiver = Resolve-Archiver -Lines $sourceResult.Output
    New-Item -ItemType Directory -Path (Split-Path -Parent $ArchivePath) -Force | Out-Null

    Write-Host "[2/4] Generando y verificando libJWPLC_Display.a..." -ForegroundColor Cyan
    $arResult = Invoke-Captured -FilePath $archiver -Arguments @("crs", $ArchivePath, $displayObj, $idleObj)
    if ($arResult.ExitCode -ne 0 -or -not (Test-Path -LiteralPath $ArchivePath))
    {
        throw "No se pudo generar libJWPLC_Display.a"
    }

    $oldLocation = Get-Location
    try
    {
        Set-Location $ExtractDir
        $extractResult = Invoke-Captured -FilePath $archiver -Arguments @("x", $ArchivePath)
    }
    finally
    {
        Set-Location $oldLocation
    }
    if ($extractResult.ExitCode -ne 0) { throw "No se pudieron extraer miembros del archive final." }

    foreach ($name in @("JWPLC_Display.cpp.o", "JWPLC_IdleScreen.cpp.o"))
    {
        $sourceObject = Join-Path (Join-Path $SourceBuild "libraries\JWPLC_Display") $name
        $archiveObject = Join-Path $ExtractDir $name
        if (-not (Test-Path -LiteralPath $archiveObject)) { throw "Falta miembro extraido: $archiveObject" }
        $sourceHash = (Get-FileHash -LiteralPath $sourceObject -Algorithm SHA256).Hash
        $archiveHash = (Get-FileHash -LiteralPath $archiveObject -Algorithm SHA256).Hash
        if ($sourceHash -ne $archiveHash) { throw "Miembro $name no coincide con el objeto source." }
    }
    $archiveMembersEquivalent = $true
    $archiveFile = Get-Item -LiteralPath $ArchivePath
    $archiveSha = (Get-FileHash -LiteralPath $ArchivePath -Algorithm SHA256).Hash.ToLowerInvariant()

    # Activar precompiled=full solo despues de construir/verificar el archive.
    [System.IO.File]::WriteAllText($PropertiesPath, $archivePropertiesText, $Utf8NoBom)

    # 3) Cold compile normal con el archive final.
    Write-Host "[3/4] Cold compile usando archive final..." -ForegroundColor Cyan
    $archiveArgs = @("compile", "-b", $Fqbn, "-v", "--clean", "--build-path", $ArchiveBuild, $SketchPath)
    $archiveResult = Invoke-Captured -FilePath $ArduinoCli -Arguments $archiveArgs
    @($archiveResult.Output) | Out-File -LiteralPath $ArchiveLog -Encoding utf8
    if ($archiveResult.ExitCode -ne 0) { throw "Build archive fallo. Revisar $ArchiveLog" }

    $archiveDisplayObjects = @(
        Get-ChildItem -LiteralPath $ArchiveBuild -Recurse -File -ErrorAction SilentlyContinue |
        Where-Object {
            $_.Name -in @("JWPLC_Display.cpp.o", "JWPLC_IdleScreen.cpp.o") -and
            $_.FullName -match '[\\/]libraries[\\/]JWPLC_Display[\\/]'
        }
    )
    $archiveDisplayCompiles = $archiveDisplayObjects.Count
    if ($archiveDisplayCompiles -ne 0)
    {
        throw ("Build precompilado genero {0} objetos source de Display." -f $archiveDisplayCompiles)
    }

    $precompiledObserved = @($archiveResult.Output | Where-Object {
        $_ -match 'Using precompiled library' -and $_ -match 'JWPLC_Display'
    }).Count -gt 0
    if (-not $precompiledObserved)
    {
        throw "El build archive no reporto uso de JWPLC_Display precompilado."
    }

    # 4) Paridad estructural: mismos miembros, simbolos y RAM. Solo se admite
    # padding del linker reflejado de forma consistente en .flash.rodata y APP.
    Write-Host "[4/4] Verificando paridad estructural source/archive..." -ForegroundColor Cyan

    $sourceMap = Get-OneFile -Dir $SourceBuild -Filter "*.ino.map"
    $archiveMap = Get-OneFile -Dir $ArchiveBuild -Filter "*.ino.map"
    $sourceBin = Get-OneFile -Dir $SourceBuild -Filter "*.ino.bin"
    $archiveBin = Get-OneFile -Dir $ArchiveBuild -Filter "*.ino.bin"

    $sourceData = Get-MapData -MapPath $sourceMap.FullName
    $archiveData = Get-MapData -MapPath $archiveMap.FullName

    $onlySourceSymbols = @($sourceData.Symbols.Keys | Where-Object { -not $archiveData.Symbols.ContainsKey($_) })
    $onlyArchiveSymbols = @($archiveData.Symbols.Keys | Where-Object { -not $sourceData.Symbols.ContainsKey($_) })
    if ($onlySourceSymbols.Count -ne 0 -or $onlyArchiveSymbols.Count -ne 0)
    {
        throw ("Conjunto de simbolos distinto. source-only={0}, archive-only={1}" -f $onlySourceSymbols.Count, $onlyArchiveSymbols.Count)
    }

    $sectionNames = @($sourceData.Sections.Keys + $archiveData.Sections.Keys | Sort-Object -Unique)
    $sectionDiffs = @()
    foreach ($name in $sectionNames)
    {
        $sSize = if ($sourceData.Sections.ContainsKey($name)) { $sourceData.Sections[$name].Size } else { -1 }
        $aSize = if ($archiveData.Sections.ContainsKey($name)) { $archiveData.Sections[$name].Size } else { -1 }
        $sAddr = if ($sourceData.Sections.ContainsKey($name)) { $sourceData.Sections[$name].Address } else { -1 }
        $aAddr = if ($archiveData.Sections.ContainsKey($name)) { $archiveData.Sections[$name].Address } else { -1 }
        if ($sSize -ne $aSize -or $sAddr -ne $aAddr)
        {
            $sectionDiffs += [PSCustomObject]@{
                Section = $name
                DeltaSize = $aSize - $sSize
                DeltaAddr = if ($sAddr -ge 0 -and $aAddr -ge 0) { $aAddr - $sAddr } else { [int64]::MaxValue }
            }
        }
    }

    $sourceFillTotal = [int64](($sourceData.Fills | Measure-Object -Property Size -Sum).Sum)
    $archiveFillTotal = [int64](($archiveData.Fills | Measure-Object -Property Size -Sum).Sum)
    $fillDelta = $archiveFillTotal - $sourceFillTotal

    $runtimeDiffs = @($sectionDiffs | Where-Object { $_.Section -notmatch '^\.debug' })
    $rodataDelta = [int64]0
    if ($runtimeDiffs.Count -gt 0)
    {
        if ($runtimeDiffs.Count -ne 1 -or $runtimeDiffs[0].Section -ne ".flash.rodata")
        {
            throw ("Secciones runtime distintas fuera de padding rodata: {0}" -f (($runtimeDiffs | ForEach-Object { $_.Section }) -join ', '))
        }
        if ($runtimeDiffs[0].DeltaAddr -ne 0)
        {
            throw ".flash.rodata cambio su direccion base."
        }
        $rodataDelta = [int64]$runtimeDiffs[0].DeltaSize
    }

    if ($rodataDelta -ne $fillDelta)
    {
        throw ("Delta .flash.rodata ({0}) no coincide con delta linker fill ({1})." -f $rodataDelta, $fillDelta)
    }

    $sourceAppBytes = Get-AppUsage -Output $sourceResult.Output
    $archiveAppBytes = Get-AppUsage -Output $archiveResult.Output
    $sourceRamBytes = Get-RamUsage -Output $sourceResult.Output
    $archiveRamBytes = Get-RamUsage -Output $archiveResult.Output
    $appDelta = $archiveAppBytes - $sourceAppBytes

    if ($sourceRamBytes -ne $archiveRamBytes)
    {
        throw ("RAM source/archive distinta: {0} vs {1}." -f $sourceRamBytes, $archiveRamBytes)
    }
    if ($appDelta -ne $fillDelta)
    {
        throw ("Delta de app ({0}) no coincide con padding linker ({1})." -f $appDelta, $fillDelta)
    }

    $structuralParity = $true
    $success = $true

    Write-Host ""
    Write-Host ("Archive bytes              : {0}" -f $archiveFile.Length)
    Write-Host ("Archive SHA256             : {0}" -f $archiveSha)
    Write-Host ("Archive members exactos    : {0}" -f $archiveMembersEquivalent)
    Write-Host ("Source Display compiles    : {0}" -f $sourceDisplayCompiles)
    Write-Host ("Archive Display compiles   : {0}" -f $archiveDisplayCompiles)
    Write-Host ("Precompiled observed       : {0}" -f $precompiledObserved)
    Write-Host ("Source app bytes           : {0}" -f $sourceAppBytes)
    Write-Host ("Archive app bytes          : {0}" -f $archiveAppBytes)
    Write-Host ("App delta bytes            : {0}" -f $appDelta)
    Write-Host ("Source RAM bytes           : {0}" -f $sourceRamBytes)
    Write-Host ("Archive RAM bytes          : {0}" -f $archiveRamBytes)
    Write-Host ("Linker fill delta          : {0}" -f $fillDelta)
    Write-Host (".flash.rodata delta        : {0}" -f $rodataDelta)
    Write-Host ("Source-only symbols        : {0}" -f $onlySourceSymbols.Count)
    Write-Host ("Archive-only symbols       : {0}" -f $onlyArchiveSymbols.Count)
    Write-Host ("Raw .bin delta             : {0}" -f ($archiveBin.Length - $sourceBin.Length))
    Write-Host ("Structural parity          : {0}" -f $structuralParity)
    Write-Host ("Source log                 : {0}" -f $SourceLog)
    Write-Host ("Archive log                : {0}" -f $ArchiveLog)
    Write-Host ""
    git status --short
    Write-Host ""
    Write-Host "ALPHA6_DISPLAY_FINAL_ARCHIVE=PASS" -ForegroundColor Green
}
finally
{
    if ($success)
    {
        # En PASS se conserva la adopcion: archive final + precompiled=full.
        [System.IO.File]::WriteAllText($PropertiesPath, $archivePropertiesText, $Utf8NoBom)
    }
    else
    {
        Write-Host "Restaurando estado anterior por fallo..." -ForegroundColor Yellow
        [System.IO.File]::WriteAllBytes($PropertiesPath, $originalPropertiesBytes)
        if (Test-Path -LiteralPath $ArchivePath) { Remove-Item -LiteralPath $ArchivePath -Force }
        if ($hadOldArchive -and (Test-Path -LiteralPath $BackupArchive))
        {
            Copy-Item -LiteralPath $BackupArchive -Destination $ArchivePath -Force
        }
    }

    if (Test-Path -LiteralPath $BackupArchive) { Remove-Item -LiteralPath $BackupArchive -Force }
}
