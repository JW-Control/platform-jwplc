[CmdletBinding()]
param(
    [string]$ArduinoCli = "arduino-cli",
    [string]$Fqbn = "jwplc_local:esp32:jwplcbasic",
    [string]$HmiSketchPath = "",
    [int]$Jobs = 0
)

Set-StrictMode -Version 2.0
$ErrorActionPreference = "Stop"

$ScriptRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$RepoRoot = [System.IO.Path]::GetFullPath((Join-Path $ScriptRoot "..\.."))
$LibraryRoot = Join-Path $RepoRoot "JWPLC\2.1.0\libraries\JWPLC_Display"
$SourceRoot = Join-Path $LibraryRoot "src"
$PropertiesPath = Join-Path $LibraryRoot "library.properties"
$ArchivePath = Join-Path $LibraryRoot "src\esp32\libJWPLC_Display.a"
$SketchPath = Join-Path $ScriptRoot "sketches\01_empty"
$RunId = (Get-Date).ToString("yyyyMMdd_HHmmss")
$RunRoot = Join-Path $ScriptRoot ("results\alpha11-display-final-" + $RunId)
$SourceBuild = Join-Path $RunRoot "source-empty"
$ArchiveBuild = Join-Path $RunRoot "archive-empty"
$SourceHmiBuild = Join-Path $RunRoot "source-hmi"
$ArchiveHmiBuild = Join-Path $RunRoot "archive-hmi"
$ExtractDir = Join-Path $RunRoot "archive-members"
$SourceLog = Join-Path $RunRoot "source-empty.log"
$ArchiveLog = Join-Path $RunRoot "archive-empty.log"
$SourceHmiLog = Join-Path $RunRoot "source-hmi.log"
$ArchiveHmiLog = Join-Path $RunRoot "archive-hmi.log"
$SummaryPath = Join-Path $RunRoot "SUMMARY.md"
$BackupArchive = Join-Path ([System.IO.Path]::GetTempPath()) ("jwplc-display-alpha11-old-" + $RunId + ".a")
$Utf8NoBom = New-Object System.Text.UTF8Encoding($false)
$ExpectedBranch = "v2.1.0-alpha.11/feature/hmi-designer"

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

function Resolve-NativeToolPath
{
    param([string]$Candidate)

    if ([string]::IsNullOrWhiteSpace($Candidate)) { return $null }
    $normalized = $Candidate.Trim().Trim('"')
    while ($normalized.Contains("\\")) { $normalized = $normalized.Replace("\\", "\") }

    foreach ($path in @($normalized, ($normalized + ".exe"), ($normalized + ".cmd"), ($normalized + ".bat")))
    {
        if (Test-Path -LiteralPath $path) { return (Resolve-Path -LiteralPath $path).Path }
    }
    return $null
}

function Resolve-ArduinoCli
{
    param([string]$Candidate)

    $direct = Resolve-NativeToolPath -Candidate $Candidate
    if ($direct) { return $direct }

    $command = Get-Command $Candidate -ErrorAction SilentlyContinue
    if ($null -ne $command) { return $command.Source }

    $ideCli = Join-Path $env:LOCALAPPDATA "Programs\arduino-ide\resources\app\lib\backend\resources\arduino-cli.exe"
    if (Test-Path -LiteralPath $ideCli) { return (Resolve-Path -LiteralPath $ideCli).Path }

    throw "No se encontro Arduino CLI. Usa -ArduinoCli con la ruta completa."
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
        elseif ($line -match '(?<exe>\S*xtensa-esp32-elf-gcc-ar(?:\.exe)?)\s+(?:cr|crs)')
        {
            $candidate = $Matches["exe"]
        }

        if ($candidate)
        {
            $resolved = Resolve-NativeToolPath -Candidate $candidate
            if ($resolved) { return $resolved }
        }
    }

    foreach ($line in $Lines)
    {
        if ($line -match '"(?<exe>[^"]*xtensa-esp32-elf-g\+\+(?:\.exe)?)"')
        {
            $dir = Split-Path -Parent $Matches["exe"]
            foreach ($name in @("xtensa-esp32-elf-gcc-ar.exe", "xtensa-esp32-elf-gcc-ar"))
            {
                $candidate = Join-Path $dir $name
                if (Test-Path -LiteralPath $candidate) { return (Resolve-Path -LiteralPath $candidate).Path }
            }
        }
    }

    throw "No se pudo localizar xtensa-esp32-elf-gcc-ar desde el build source."
}

function Get-OneFile
{
    param([string]$Dir, [string]$Filter)
    $file = Get-ChildItem -LiteralPath $Dir -File -Filter $Filter -ErrorAction SilentlyContinue | Select-Object -First 1
    if ($null -eq $file) { throw "No se encontro $Filter en $Dir" }
    return $file
}

function Get-AppUsage
{
    param([object[]]$Output)
    foreach ($line in $Output)
    {
        if ([string]$line -match '(?:Sketch uses|El Sketch usa)\s+(?<n>\d+)\s+bytes') { return [int64]$Matches["n"] }
    }
    throw "No se pudo leer uso de flash desde Arduino CLI."
}

function Get-RamUsage
{
    param([object[]]$Output)
    foreach ($line in $Output)
    {
        if ([string]$line -match '(?:Global variables use|Las variables Globales usan)\s+(?<n>\d+)\s+bytes') { return [int64]$Matches["n"] }
    }
    throw "No se pudo leer uso de RAM desde Arduino CLI."
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
            if ($name -notmatch '^(?:0x|\.|\*|LOAD$|PROVIDE|ASSERT)' -and -not $symbols.ContainsKey($name))
            {
                $symbols[$name] = [Convert]::ToInt64($Matches["addr"], 16)
            }
        }
    }

    return [PSCustomObject]@{ Sections = $sections; Symbols = $symbols; Fills = $fills }
}

function Test-SourceArchiveParity
{
    param(
        [string]$SourceBuildPath,
        [object[]]$SourceOutput,
        [string]$ArchiveBuildPath,
        [object[]]$ArchiveOutput,
        [string]$Label
    )

    $sourceMap = Get-OneFile -Dir $SourceBuildPath -Filter "*.ino.map"
    $archiveMap = Get-OneFile -Dir $ArchiveBuildPath -Filter "*.ino.map"
    $sourceBin = Get-OneFile -Dir $SourceBuildPath -Filter "*.ino.bin"
    $archiveBin = Get-OneFile -Dir $ArchiveBuildPath -Filter "*.ino.bin"

    $sourceData = Get-MapData -MapPath $sourceMap.FullName
    $archiveData = Get-MapData -MapPath $archiveMap.FullName

    $onlySourceSymbols = @($sourceData.Symbols.Keys | Where-Object { -not $archiveData.Symbols.ContainsKey($_) })
    $onlyArchiveSymbols = @($archiveData.Symbols.Keys | Where-Object { -not $sourceData.Symbols.ContainsKey($_) })
    if ($onlySourceSymbols.Count -ne 0 -or $onlyArchiveSymbols.Count -ne 0)
    {
        throw ("$Label: conjunto de simbolos distinto. source-only={0}, archive-only={1}" -f $onlySourceSymbols.Count, $onlyArchiveSymbols.Count)
    }

    $sectionNames = @($sourceData.Sections.Keys + $archiveData.Sections.Keys | Sort-Object -Unique)
    $runtimeDiffs = @()
    foreach ($name in $sectionNames)
    {
        if ($name -match '^\.debug') { continue }
        $sSize = if ($sourceData.Sections.ContainsKey($name)) { $sourceData.Sections[$name].Size } else { -1 }
        $aSize = if ($archiveData.Sections.ContainsKey($name)) { $archiveData.Sections[$name].Size } else { -1 }
        $sAddr = if ($sourceData.Sections.ContainsKey($name)) { $sourceData.Sections[$name].Address } else { -1 }
        $aAddr = if ($archiveData.Sections.ContainsKey($name)) { $archiveData.Sections[$name].Address } else { -1 }
        if ($sSize -ne $aSize -or $sAddr -ne $aAddr)
        {
            $runtimeDiffs += [PSCustomObject]@{ Section = $name; DeltaSize = $aSize - $sSize; DeltaAddr = $aAddr - $sAddr }
        }
    }

    $sourceFill = [int64](($sourceData.Fills | Measure-Object -Property Size -Sum).Sum)
    $archiveFill = [int64](($archiveData.Fills | Measure-Object -Property Size -Sum).Sum)
    $fillDelta = $archiveFill - $sourceFill
    $rodataDelta = [int64]0

    if ($runtimeDiffs.Count -gt 0)
    {
        if ($runtimeDiffs.Count -ne 1 -or $runtimeDiffs[0].Section -ne ".flash.rodata" -or $runtimeDiffs[0].DeltaAddr -ne 0)
        {
            throw ("$Label: diferencias runtime fuera del padding permitido: {0}" -f (($runtimeDiffs | ForEach-Object { $_.Section }) -join ', '))
        }
        $rodataDelta = [int64]$runtimeDiffs[0].DeltaSize
    }

    if ($rodataDelta -ne $fillDelta)
    {
        throw "$Label: delta .flash.rodata ($rodataDelta) no coincide con linker fill ($fillDelta)."
    }

    $sourceApp = Get-AppUsage -Output $SourceOutput
    $archiveApp = Get-AppUsage -Output $ArchiveOutput
    $sourceRam = Get-RamUsage -Output $SourceOutput
    $archiveRam = Get-RamUsage -Output $ArchiveOutput
    $appDelta = $archiveApp - $sourceApp

    if ($sourceRam -ne $archiveRam) { throw "$Label: RAM distinta source=$sourceRam archive=$archiveRam." }
    if ($appDelta -ne $fillDelta) { throw "$Label: delta APP ($appDelta) no coincide con linker fill ($fillDelta)." }

    return [PSCustomObject]@{
        Label = $Label
        SourceApp = $sourceApp
        ArchiveApp = $archiveApp
        AppDelta = $appDelta
        SourceRam = $sourceRam
        ArchiveRam = $archiveRam
        FillDelta = $fillDelta
        SourceBinBytes = $sourceBin.Length
        ArchiveBinBytes = $archiveBin.Length
        SymbolParity = $true
        StructuralParity = $true
    }
}

function Invoke-Compile
{
    param(
        [string]$Cli,
        [string]$Sketch,
        [string]$BuildPath,
        [string]$LogPath,
        [string]$Label
    )

    New-Item -ItemType Directory -Path $BuildPath -Force | Out-Null
    $args = @("compile", "-b", $Fqbn, "-j", $Jobs.ToString(), "-v", "--clean", "--build-path", $BuildPath, $Sketch)
    Write-Host ("[{0}] arduino-cli {1}" -f $Label, ($args -join " ")) -ForegroundColor Cyan
    $sw = [System.Diagnostics.Stopwatch]::StartNew()
    $result = Invoke-Captured -FilePath $Cli -Arguments $args
    $sw.Stop()
    @($result.Output) | Out-File -LiteralPath $LogPath -Encoding utf8
    if ($result.ExitCode -ne 0) { throw "$Label fallo. Revisar $LogPath" }
    return [PSCustomObject]@{ Output = @($result.Output); Seconds = $sw.Elapsed.TotalSeconds }
}

if ($Jobs -lt 0) { throw "Jobs debe ser 0 o mayor." }
Set-Location $RepoRoot

$branch = (git branch --show-current).Trim()
if ($branch -ne $ExpectedBranch) { throw "Branch incorrecto: $branch. Esperado: $ExpectedBranch" }
if (@(git status --porcelain).Count -ne 0)
{
    git status --short
    throw "Repo no limpio antes de regenerar JWPLC_Display."
}

$Cli = Resolve-ArduinoCli -Candidate $ArduinoCli
if (-not (Test-Path -LiteralPath $PropertiesPath)) { throw "Falta $PropertiesPath" }
if (-not (Test-Path -LiteralPath $SketchPath)) { throw "Falta sketch de cierre: $SketchPath" }
if (-not [string]::IsNullOrWhiteSpace($HmiSketchPath))
{
    if (-not (Test-Path -LiteralPath $HmiSketchPath)) { throw "No existe HMI sketch: $HmiSketchPath" }
    $HmiSketchPath = (Resolve-Path -LiteralPath $HmiSketchPath).Path
}

$sourceCpp = @(Get-ChildItem -LiteralPath $SourceRoot -File -Filter "*.cpp" | Sort-Object Name)
if ($sourceCpp.Count -lt 1) { throw "JWPLC_Display no tiene TUs .cpp para archivar." }
$sourceNames = @($sourceCpp | ForEach-Object { $_.Name })

$originalPropertiesBytes = [System.IO.File]::ReadAllBytes($PropertiesPath)
$originalPropertiesText = [System.Text.Encoding]::UTF8.GetString($originalPropertiesBytes)
$lineEnding = if ($originalPropertiesText.Contains("`r`n")) { "`r`n" } else { "`n" }
$sourcePropertiesText = [regex]::Replace($originalPropertiesText, '(?m)^precompiled=full\s*\r?\n?', '')
$archivePropertiesText = $sourcePropertiesText
if ($archivePropertiesText -match '(?m)^ldflags=')
{
    $archivePropertiesText = [regex]::Replace($archivePropertiesText, '(?m)^ldflags=', ("precompiled=full" + $lineEnding + "ldflags="), 1)
}
else
{
    if (-not $archivePropertiesText.EndsWith($lineEnding)) { $archivePropertiesText += $lineEnding }
    $archivePropertiesText += "precompiled=full" + $lineEnding
}

$hadOldArchive = Test-Path -LiteralPath $ArchivePath
if ($hadOldArchive) { Copy-Item -LiteralPath $ArchivePath -Destination $BackupArchive -Force }

New-Item -ItemType Directory -Path $RunRoot -Force | Out-Null
New-Item -ItemType Directory -Path $ExtractDir -Force | Out-Null

$success = $false
try
{
    Write-Host "================================================" -ForegroundColor Cyan
    Write-Host " ALPHA11 - FINALIZAR JWPLC_Display PRECOMPILADO" -ForegroundColor Cyan
    Write-Host "================================================" -ForegroundColor Cyan
    Write-Host ("Branch : {0}" -f $branch)
    Write-Host ("HEAD   : {0}" -f ((git rev-parse HEAD).Trim()))
    Write-Host ("FQBN   : {0}" -f $Fqbn)
    Write-Host ("CLI    : {0}" -f $Cli)
    Write-Host ("TUs    : {0}" -f $sourceCpp.Count)
    $sourceNames | ForEach-Object { Write-Host ("  - {0}" -f $_) }
    Write-Host ""

    # Fuente real: sin archive y sin precompiled=full.
    [System.IO.File]::WriteAllText($PropertiesPath, $sourcePropertiesText, $Utf8NoBom)
    if (Test-Path -LiteralPath $ArchivePath) { Remove-Item -LiteralPath $ArchivePath -Force }

    $sourceRun = Invoke-Compile -Cli $Cli -Sketch $SketchPath -BuildPath $SourceBuild -LogPath $SourceLog -Label "1/5 SOURCE EMPTY"

    $compiledObjects = New-Object System.Collections.Generic.List[string]
    foreach ($cpp in $sourceCpp)
    {
        $obj = Join-Path $SourceBuild ("libraries\JWPLC_Display\" + $cpp.Name + ".o")
        if (-not (Test-Path -LiteralPath $obj)) { throw "Falta objeto source: $obj" }
        $compiledObjects.Add($obj)
    }

    $actualDisplayObjects = @(Get-ChildItem -LiteralPath (Join-Path $SourceBuild "libraries\JWPLC_Display") -File -Filter "*.cpp.o" -ErrorAction SilentlyContinue)
    if ($actualDisplayObjects.Count -ne $sourceCpp.Count)
    {
        throw ("Objetos Display inesperados: fuentes={0}, objetos={1}" -f $sourceCpp.Count, $actualDisplayObjects.Count)
    }

    $sourceHmiRun = $null
    if ($HmiSketchPath)
    {
        $sourceHmiRun = Invoke-Compile -Cli $Cli -Sketch $HmiSketchPath -BuildPath $SourceHmiBuild -LogPath $SourceHmiLog -Label "2/5 SOURCE HMI"
    }

    $archiver = Resolve-Archiver -Lines $sourceRun.Output
    New-Item -ItemType Directory -Path (Split-Path -Parent $ArchivePath) -Force | Out-Null
    Write-Host "[3/5 ARCHIVE] Generando libJWPLC_Display.a..." -ForegroundColor Cyan
    $arArgs = @("crs", $ArchivePath) + @($compiledObjects)
    $arResult = Invoke-Captured -FilePath $archiver -Arguments $arArgs
    if ($arResult.ExitCode -ne 0 -or -not (Test-Path -LiteralPath $ArchivePath)) { throw "No se pudo generar $ArchivePath" }

    $listResult = Invoke-Captured -FilePath $archiver -Arguments @("t", $ArchivePath)
    if ($listResult.ExitCode -ne 0) { throw "No se pudo listar el archive." }
    $members = @($listResult.Output | ForEach-Object { ([string]$_).Trim() } | Where-Object { $_ })
    $expectedMembers = @($sourceCpp | ForEach-Object { $_.Name + ".o" })
    if ((Compare-Object $expectedMembers $members).Count -ne 0)
    {
        throw ("Miembros del archive no coinciden. Esperados={0}; reales={1}" -f ($expectedMembers -join ','), ($members -join ','))
    }

    $oldLocation = Get-Location
    try
    {
        Set-Location $ExtractDir
        $extractResult = Invoke-Captured -FilePath $archiver -Arguments @("x", $ArchivePath)
    }
    finally { Set-Location $oldLocation }
    if ($extractResult.ExitCode -ne 0) { throw "No se pudieron extraer miembros del archive." }

    foreach ($member in $expectedMembers)
    {
        $src = Join-Path (Join-Path $SourceBuild "libraries\JWPLC_Display") $member
        $ext = Join-Path $ExtractDir $member
        if (-not (Test-Path -LiteralPath $ext)) { throw "Falta miembro extraido: $member" }
        if ((Get-FileHash $src -Algorithm SHA256).Hash -ne (Get-FileHash $ext -Algorithm SHA256).Hash)
        {
            throw "Miembro $member no es byte-a-byte igual al objeto source."
        }
    }

    $archiveFile = Get-Item -LiteralPath $ArchivePath
    $archiveSha = (Get-FileHash -LiteralPath $ArchivePath -Algorithm SHA256).Hash.ToLowerInvariant()

    # Adoptar precompiled=full y verificar build limpio.
    [System.IO.File]::WriteAllText($PropertiesPath, $archivePropertiesText, $Utf8NoBom)
    $archiveRun = Invoke-Compile -Cli $Cli -Sketch $SketchPath -BuildPath $ArchiveBuild -LogPath $ArchiveLog -Label "4/5 ARCHIVE EMPTY"

    $displaySourceCompileLines = @($archiveRun.Output | Where-Object {
        ([string]$_) -match '-MMD\s+-c\s' -and ([string]$_) -match '[\\/]libraries[\\/]JWPLC_Display[\\/].*\.cpp'
    })
    if ($displaySourceCompileLines.Count -ne 0) { throw "El build precompilado todavia compilo fuentes de JWPLC_Display." }

    $precompiledObserved = @($archiveRun.Output | Where-Object {
        ([string]$_) -match 'Using precompiled library|Usando libreria precompilada|Usando biblioteca precompilada' -and ([string]$_) -match 'JWPLC_Display'
    }).Count -gt 0
    if (-not $precompiledObserved) { throw "Arduino CLI no reporto JWPLC_Display como libreria precompilada." }

    $emptyParity = Test-SourceArchiveParity -SourceBuildPath $SourceBuild -SourceOutput $sourceRun.Output -ArchiveBuildPath $ArchiveBuild -ArchiveOutput $archiveRun.Output -Label "EMPTY"

    $hmiParity = $null
    if ($HmiSketchPath)
    {
        $archiveHmiRun = Invoke-Compile -Cli $Cli -Sketch $HmiSketchPath -BuildPath $ArchiveHmiBuild -LogPath $ArchiveHmiLog -Label "5/5 ARCHIVE HMI"
        $hmiSourceLines = @($archiveHmiRun.Output | Where-Object {
            ([string]$_) -match '-MMD\s+-c\s' -and ([string]$_) -match '[\\/]libraries[\\/]JWPLC_Display[\\/].*\.cpp'
        })
        if ($hmiSourceLines.Count -ne 0) { throw "El build HMI precompilado compilo fuentes de JWPLC_Display." }
        $hmiParity = Test-SourceArchiveParity -SourceBuildPath $SourceHmiBuild -SourceOutput $sourceHmiRun.Output -ArchiveBuildPath $ArchiveHmiBuild -ArchiveOutput $archiveHmiRun.Output -Label "HMI"
    }

    $lines = New-Object System.Collections.Generic.List[string]
    $lines.Add("# Alpha11 - cierre JWPLC_Display precompilado")
    $lines.Add("")
    $lines.Add(("- Branch: ``{0}``" -f $branch))
    $lines.Add(("- HEAD: ``{0}``" -f ((git rev-parse HEAD).Trim())))
    $lines.Add(("- FQBN: ``{0}``" -f $Fqbn))
    $lines.Add(("- TUs source: **{0}**" -f $sourceCpp.Count))
    $lines.Add(("- Miembros exactos: **PASS**"))
    $lines.Add(("- Archive bytes: **{0}**" -f $archiveFile.Length))
    $lines.Add(("- Archive SHA-256: ``{0}``" -f $archiveSha))
    $lines.Add(("- Display source compilado en build precompiled: **0**"))
    $lines.Add(("- Arduino reporta precompiled: **PASS**"))
    $lines.Add(("- Paridad EMPTY simbolos/estructura/RAM: **PASS**"))
    if ($hmiParity) { $lines.Add(("- Paridad HMI simbolos/estructura/RAM: **PASS**")) }
    $lines.Add("")
    $lines.Add("## Translation units")
    $lines.Add("")
    foreach ($name in $sourceNames) { $lines.Add(("- ``{0}``" -f $name)) }
    $lines.Add("")
    $lines.Add("## Paridad EMPTY")
    $lines.Add("")
    $lines.Add(("- APP source/archive: {0} / {1} bytes (delta {2})" -f $emptyParity.SourceApp, $emptyParity.ArchiveApp, $emptyParity.AppDelta))
    $lines.Add(("- RAM source/archive: {0} / {1} bytes" -f $emptyParity.SourceRam, $emptyParity.ArchiveRam))
    $lines.Add(("- Linker fill delta: {0}" -f $emptyParity.FillDelta))
    if ($hmiParity)
    {
        $lines.Add("")
        $lines.Add("## Paridad HMI")
        $lines.Add("")
        $lines.Add(("- Sketch: ``{0}``" -f $HmiSketchPath))
        $lines.Add(("- APP source/archive: {0} / {1} bytes (delta {2})" -f $hmiParity.SourceApp, $hmiParity.ArchiveApp, $hmiParity.AppDelta))
        $lines.Add(("- RAM source/archive: {0} / {1} bytes" -f $hmiParity.SourceRam, $hmiParity.ArchiveRam))
        $lines.Add(("- Linker fill delta: {0}" -f $hmiParity.FillDelta))
    }
    $lines | Out-File -LiteralPath $SummaryPath -Encoding utf8

    $success = $true
    Write-Host ""
    Write-Host ("ARCHIVE_BYTES={0}" -f $archiveFile.Length) -ForegroundColor Green
    Write-Host ("ARCHIVE_SHA256={0}" -f $archiveSha) -ForegroundColor Green
    Write-Host ("DISPLAY_TUS={0}" -f $sourceCpp.Count) -ForegroundColor Green
    Write-Host "ARCHIVE_MEMBERS_EXACT=PASS" -ForegroundColor Green
    Write-Host "PRECOMPILED_DISPLAY_SOURCE_TUS=0" -ForegroundColor Green
    Write-Host "SOURCE_ARCHIVE_EMPTY_PARITY=PASS" -ForegroundColor Green
    if ($hmiParity) { Write-Host "SOURCE_ARCHIVE_HMI_PARITY=PASS" -ForegroundColor Green }
    Write-Host "ALPHA11_DISPLAY_FINAL_ARCHIVE=PASS" -ForegroundColor Green
    Write-Host ("SUMMARY={0}" -f $SummaryPath)
    Write-Host ""
    git status --short
}
finally
{
    if ($success)
    {
        [System.IO.File]::WriteAllText($PropertiesPath, $archivePropertiesText, $Utf8NoBom)
    }
    else
    {
        Write-Host "Restaurando estado anterior por fallo..." -ForegroundColor Yellow
        [System.IO.File]::WriteAllBytes($PropertiesPath, $originalPropertiesBytes)
        if (Test-Path -LiteralPath $ArchivePath) { Remove-Item -LiteralPath $ArchivePath -Force }
        if ($hadOldArchive -and (Test-Path -LiteralPath $BackupArchive))
        {
            New-Item -ItemType Directory -Path (Split-Path -Parent $ArchivePath) -Force | Out-Null
            Copy-Item -LiteralPath $BackupArchive -Destination $ArchivePath -Force
        }
    }

    if (Test-Path -LiteralPath $BackupArchive) { Remove-Item -LiteralPath $BackupArchive -Force }
}
