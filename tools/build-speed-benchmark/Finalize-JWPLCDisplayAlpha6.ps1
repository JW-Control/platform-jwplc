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
$RunRoot = Join-Path $ScriptRoot ("results\alpha6-display-final-" + $RunId)
$SourceBuild = Join-Path $RunRoot "source"
$ArchiveBuild = Join-Path $RunRoot "archive"
$SourceLog = Join-Path $RunRoot "source.log"
$ArchiveLog = Join-Path $RunRoot "archive.log"
$BackupArchive = Join-Path ([System.IO.Path]::GetTempPath()) ("jwplc-display-old-" + $RunId + ".a")

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

    throw "No se pudo localizar xtensa-esp32-elf-gcc-ar desde el build fuente."
}

function Get-AppBin
{
    param([string]$BuildPath)
    $bin = Get-ChildItem -LiteralPath $BuildPath -Filter "*.ino.bin" -File | Select-Object -First 1
    if ($null -eq $bin) { throw "No se encontro .ino.bin en $BuildPath" }
    return $bin
}

function Test-PayloadEquivalent
{
    param([string]$ReferencePath, [string]$CandidatePath)

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

Set-Location $RepoRoot

$branch = (git branch --show-current).Trim()
if ($branch -ne "v2.1.0-alpha.6/feature/ethernet-nonblocking-runtime")
{
    throw "Branch incorrecto: $branch"
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
if ($originalPropertiesText -notmatch '(?m)^precompiled=full\s*$')
{
    throw "JWPLC_Display no declara precompiled=full."
}

$hadOldArchive = Test-Path -LiteralPath $ArchivePath
if ($hadOldArchive)
{
    Copy-Item -LiteralPath $ArchivePath -Destination $BackupArchive -Force
}

New-Item -ItemType Directory -Path $SourceBuild -Force | Out-Null
New-Item -ItemType Directory -Path $ArchiveBuild -Force | Out-Null

$success = $false
try
{
    Write-Host "=== ALPHA6 - FINALIZAR JWPLC_Display PRECOMPILADO ===" -ForegroundColor Cyan
    Write-Host "Branch : $branch"
    Write-Host ("HEAD   : {0}" -f ((git rev-parse --short=8 HEAD).Trim()))

    # 1) Fuente real: retirar temporalmente precompiled=full y el archive anterior.
    $sourcePropertiesText = [regex]::Replace(
        $originalPropertiesText,
        '(?m)^precompiled=full\s*\r?\n?',
        '')
    [System.IO.File]::WriteAllText($PropertiesPath, $sourcePropertiesText, (New-Object System.Text.UTF8Encoding($false)))
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

    # La existencia de ambos .o en un --clean con precompiled retirado es la prueba
    # robusta de compilacion source. No dependemos del formato textual de -v.
    $sourceDisplayCompiles = $sourceObjects.Count
    Write-Host ("Source Display objects: {0}/2" -f $sourceDisplayCompiles) -ForegroundColor Green

    # 2) Crear archive con los objetos acabados de compilar.
    $archiver = Resolve-Archiver -Lines $sourceResult.Output
    [System.IO.File]::WriteAllBytes($PropertiesPath, $originalPropertiesBytes)
    New-Item -ItemType Directory -Path (Split-Path -Parent $ArchivePath) -Force | Out-Null

    Write-Host "[2/4] Generando libJWPLC_Display.a final..." -ForegroundColor Cyan
    $arResult = Invoke-Captured -FilePath $archiver -Arguments @("crs", $ArchivePath, $displayObj, $idleObj)
    if ($arResult.ExitCode -ne 0 -or -not (Test-Path -LiteralPath $ArchivePath))
    {
        throw "No se pudo generar libJWPLC_Display.a"
    }

    $archiveFile = Get-Item -LiteralPath $ArchivePath
    $archiveSha = (Get-FileHash -LiteralPath $ArchivePath -Algorithm SHA256).Hash.ToLowerInvariant()

    # 3) Cold compile normal usando precompiled=full.
    Write-Host "[3/4] Cold compile usando archive final..." -ForegroundColor Cyan
    $archiveArgs = @("compile", "-b", $Fqbn, "-v", "--clean", "--build-path", $ArchiveBuild, $SketchPath)
    $archiveResult = Invoke-Captured -FilePath $ArduinoCli -Arguments $archiveArgs
    @($archiveResult.Output) | Out-File -LiteralPath $ArchiveLog -Encoding utf8
    if ($archiveResult.ExitCode -ne 0) { throw "Build archive fallo. Revisar $ArchiveLog" }

    # En el build precompilado no deben existir objetos source de JWPLC_Display.
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

    # 4) Paridad source vs archive.
    Write-Host "[4/4] Verificando paridad source/archive..." -ForegroundColor Cyan
    $sourceBin = Get-AppBin -BuildPath $SourceBuild
    $archiveBin = Get-AppBin -BuildPath $ArchiveBuild
    $sameSize = $sourceBin.Length -eq $archiveBin.Length
    $payloadEquivalent = Test-PayloadEquivalent -ReferencePath $sourceBin.FullName -CandidatePath $archiveBin.FullName

    if (-not $sameSize) { throw "Source/archive cambiaron tamano de app." }
    if (-not $payloadEquivalent) { throw "Source/archive no son equivalentes fuera de hashes de build." }

    $success = $true

    Write-Host ""
    Write-Host ("Archive bytes : {0}" -f $archiveFile.Length)
    Write-Host ("Archive SHA256: {0}" -f $archiveSha)
    Write-Host ("Source Display compiles : {0}" -f $sourceDisplayCompiles)
    Write-Host ("Archive Display compiles: {0}" -f $archiveDisplayCompiles)
    Write-Host ("Precompiled observed     : {0}" -f $precompiledObserved)
    Write-Host ("App bytes iguales       : {0}" -f $sameSize)
    Write-Host ("Payload equivalente     : {0}" -f $payloadEquivalent)
    Write-Host ("Source log : {0}" -f $SourceLog)
    Write-Host ("Archive log: {0}" -f $ArchiveLog)
    Write-Host ""
    git status --short
    Write-Host ""
    Write-Host "ALPHA6_DISPLAY_FINAL_ARCHIVE=PASS" -ForegroundColor Green
}
finally
{
    [System.IO.File]::WriteAllBytes($PropertiesPath, $originalPropertiesBytes)

    if (-not $success)
    {
        Write-Host "Restaurando archive anterior por fallo..." -ForegroundColor Yellow
        if (Test-Path -LiteralPath $ArchivePath) { Remove-Item -LiteralPath $ArchivePath -Force }
        if ($hadOldArchive -and (Test-Path -LiteralPath $BackupArchive))
        {
            Copy-Item -LiteralPath $BackupArchive -Destination $ArchivePath -Force
        }
    }

    if (Test-Path -LiteralPath $BackupArchive) { Remove-Item -LiteralPath $BackupArchive -Force }
}
