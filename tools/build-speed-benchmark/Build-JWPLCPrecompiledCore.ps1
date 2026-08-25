[CmdletBinding()]
param(
    [string]$ArduinoCli = "arduino-cli",
    [ValidateSet("Basic", "Core")]
    [string[]]$Targets = @("Basic"),
    [string]$Sketch = "01_empty",
    [int]$Jobs = 0,
    [string]$OutputRoot = ""
)

Set-StrictMode -Version 2.0
$ErrorActionPreference = "Stop"

$ScriptRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$RepoRoot = [System.IO.Path]::GetFullPath((Join-Path $ScriptRoot "..\.."))
$PlatformRoot = Join-Path $RepoRoot "JWPLC\2.1.0"
$SketchRoot = Join-Path $ScriptRoot "sketches"
$BoardsLocalPath = Join-Path $PlatformRoot "boards.local.txt"
$SourceCoreRoot = Join-Path $PlatformRoot "cores\jwcontrol"
$StubCorePath = Join-Path $PlatformRoot "cores\jwcontrol_precompiled_stub\precompiled_core_stub.c"
$ArchivePath = Join-Path $PlatformRoot "precompiled\core\JWPLCBASIC\core.a"

if ([string]::IsNullOrWhiteSpace($OutputRoot))
{
    $OutputRoot = Join-Path $ScriptRoot "core-precompiled-work"
}

function Invoke-NativeCaptured
{
    param(
        [Parameter(Mandatory = $true)][string]$FilePath,
        [Parameter(Mandatory = $true)][string[]]$Arguments
    )

    $previousPreference = $ErrorActionPreference
    $nativeOutput = @()
    $exitCode = -1

    try
    {
        $ErrorActionPreference = "Continue"
        $nativeOutput = @(& $FilePath @Arguments 2>&1 | ForEach-Object { $_.ToString() })
        $exitCode = $LASTEXITCODE
    }
    finally
    {
        $ErrorActionPreference = $previousPreference
    }

    return [PSCustomObject]@{
        ExitCode = [int]$exitCode
        Output = $nativeOutput
    }
}

function Get-CompileDatabaseInfo
{
    param([Parameter(Mandatory = $true)][string]$BuildPath)

    $compileDbPath = Join-Path $BuildPath "compile_commands.json"
    if (-not (Test-Path $compileDbPath))
    {
        throw "No se genero compile_commands.json: $compileDbPath"
    }

    $entries = @(Get-Content -LiteralPath $compileDbPath -Raw | ConvertFrom-Json)
    $sourceFiles = New-Object System.Collections.Generic.List[string]
    $stubFiles = New-Object System.Collections.Generic.List[string]

    foreach ($entry in $entries)
    {
        $file = [string]$entry.file
        if ([string]::IsNullOrWhiteSpace($file))
        {
            continue
        }

        if (-not [System.IO.Path]::IsPathRooted($file))
        {
            $directory = [string]$entry.directory
            $file = Join-Path $directory $file
        }

        $full = [System.IO.Path]::GetFullPath($file)
        $normalized = $full.Replace('\', '/')

        # Se inspecciona el operando fuente real de compile_commands.json.
        # No se infieren TUs a partir de rutas -I del log verbose.
        if ($normalized -match '/cores/jwcontrol_precompiled_stub/')
        {
            [void]$stubFiles.Add($full)
        }
        elseif ($normalized -match '/cores/jwcontrol/')
        {
            [void]$sourceFiles.Add($full)
        }
    }

    return [PSCustomObject]@{
        Entries = $entries.Count
        SourceFiles = @($sourceFiles)
        StubFiles = @($stubFiles)
        SourceCount = $sourceFiles.Count
        StubCount = $stubFiles.Count
    }
}

function Invoke-ArduinoCompile
{
    param(
        [Parameter(Mandatory = $true)][string]$BuildPath,
        [Parameter(Mandatory = $true)][string]$LogPath,
        [Parameter(Mandatory = $true)][string]$SketchPath
    )

    if (Test-Path $BuildPath)
    {
        Remove-Item -Path $BuildPath -Recurse -Force
    }
    New-Item -ItemType Directory -Path $BuildPath -Force | Out-Null

    $args = @(
        "compile",
        "-b", "jwplc_local:esp32:jwplcbasic",
        "-j", $Jobs.ToString(),
        "-v",
        "--build-path", $BuildPath,
        "--clean",
        $SketchPath
    )

    Write-Host ""
    Write-Host ("arduino-cli {0}" -f ($args -join " ")) -ForegroundColor DarkGray

    $sw = [System.Diagnostics.Stopwatch]::StartNew()
    $native = Invoke-NativeCaptured -FilePath $ArduinoCli -Arguments $args
    $sw.Stop()

    @($native.Output) | Out-File -FilePath $LogPath -Encoding utf8

    if ($native.ExitCode -ne 0)
    {
        Write-Host ("Compilacion fallo (exit={0})." -f $native.ExitCode) -ForegroundColor Red
        @($native.Output | Select-Object -Last 20) | ForEach-Object { Write-Host $_ -ForegroundColor DarkYellow }
        throw "Arduino CLI fallo. Revisar: $LogPath"
    }

    return [PSCustomObject]@{
        DurationMs = [Math]::Round($sw.Elapsed.TotalMilliseconds, 3)
        Output = @($native.Output)
        CompileDb = Get-CompileDatabaseInfo -BuildPath $BuildPath
    }
}

if ($Jobs -lt 0)
{
    throw "Jobs debe ser 0 o mayor."
}

if ($null -eq (Get-Command $ArduinoCli -ErrorAction SilentlyContinue))
{
    throw "No se encontro '$ArduinoCli' en PATH."
}

# Alpha5 normaliza un solo archive de produccion: JWPLCBASIC/core.a.
# El target Core permanece deliberadamente como build fuente de validacion.
if ($Targets.Count -ne 1 -or $Targets[0] -ne "Basic")
{
    throw "Solo Target=Basic genera core precompilado. 'Core' permanece como target fuente de validacion."
}

foreach ($requiredPath in @($SourceCoreRoot, $StubCorePath, $ArchivePath))
{
    if (-not (Test-Path $requiredPath))
    {
        throw "Falta ruta requerida: $requiredPath"
    }
}

$sketchPath = Join-Path $SketchRoot $Sketch
if (-not (Test-Path $sketchPath))
{
    throw "Sketch no encontrado: $sketchPath"
}

# boards.local.txt es estado local del usuario. Se preserva byte por byte y se
# restaura incluso si la compilacion falla.
$boardsLocalHadOriginal = Test-Path $BoardsLocalPath
$boardsLocalOriginalBytes = $null
$boardsLocalOriginalText = ""

if ($boardsLocalHadOriginal)
{
    $boardsLocalOriginalBytes = [System.IO.File]::ReadAllBytes(
        [System.IO.Path]::GetFullPath($BoardsLocalPath)
    )
    $boardsLocalOriginalText = [System.IO.File]::ReadAllText(
        [System.IO.Path]::GetFullPath($BoardsLocalPath)
    )
}

function Restore-BoardsLocal
{
    if ($boardsLocalHadOriginal)
    {
        [System.IO.File]::WriteAllBytes(
            [System.IO.Path]::GetFullPath($BoardsLocalPath),
            $boardsLocalOriginalBytes
        )
    }
    elseif (Test-Path $BoardsLocalPath)
    {
        Remove-Item -LiteralPath $BoardsLocalPath -Force
    }
}

function Enable-SourceCoreOverride
{
    $baseText = $boardsLocalOriginalText

    # Limpiar solo bloques temporales conocidos en la copia usada para build.
    $baseText = [regex]::Replace(
        $baseText,
        '(?ms)^# BEGIN JWPLC_SOURCE_CORE_BUILD\r?\n.*?^# END JWPLC_SOURCE_CORE_BUILD\r?\n?',
        ''
    ).TrimEnd()

    $baseText = [regex]::Replace(
        $baseText,
        '(?ms)^# BEGIN JWPLC_P2_PRECOMPILED_CORE\r?\n.*?^# END JWPLC_P2_PRECOMPILED_CORE\r?\n?',
        ''
    ).TrimEnd()

    $lines = New-Object System.Collections.Generic.List[string]
    if (-not [string]::IsNullOrWhiteSpace($baseText))
    {
        $lines.Add($baseText)
        $lines.Add("")
    }

    $lines.Add("# BEGIN JWPLC_SOURCE_CORE_BUILD")
    $lines.Add("# Temporal: generar core.a desde cores/jwcontrol con el perfil completo jwplcbasic.")
    $lines.Add("jwplcbasic.build.core=jwcontrol")
    $lines.Add("jwplcbasic.build.extra_libs=")
    $lines.Add("# END JWPLC_SOURCE_CORE_BUILD")

    $content = ($lines -join [Environment]::NewLine) + [Environment]::NewLine
    $utf8NoBom = New-Object System.Text.UTF8Encoding($false)
    [System.IO.File]::WriteAllText(
        [System.IO.Path]::GetFullPath($BoardsLocalPath),
        $content,
        $utf8NoBom
    )
}

$runId = (Get-Date).ToString("yyyyMMdd_HHmmss")
$runRoot = Join-Path $OutputRoot $runId
$buildPath = Join-Path $runRoot "source-Basic"
$logPath = Join-Path $runRoot "source-Basic.log"
$candidatePath = Join-Path $runRoot "JWPLCBASIC-core-candidate.a"
$archiveBackupPath = Join-Path $runRoot "JWPLCBASIC-core-before.a"
$summaryPath = Join-Path $runRoot "CORE_PRECOMPILED_BUILD_SUMMARY.md"
New-Item -ItemType Directory -Path $runRoot -Force | Out-Null

$archiveOriginalSha = (Get-FileHash -Path $ArchivePath -Algorithm SHA256).Hash.ToLowerInvariant()
$archiveOriginalBytes = (Get-Item $ArchivePath).Length
Copy-Item -LiteralPath $ArchivePath -Destination $archiveBackupPath -Force

$archiveReplaced = $false

Write-Host "Core JWPLC precompilado - generacion normalizada" -ForegroundColor Cyan
Write-Host "Fuente canonica: cores/jwcontrol"
Write-Host "Target: jwplc_local:esp32:jwplcbasic"
Write-Host ("Sketch: {0}" -f $Sketch)
Write-Host ("Archive actual: {0}" -f $ArchivePath)
Write-Host ("SHA actual: {0}" -f $archiveOriginalSha)
Write-Host ""
Write-Host "El override temporal selecciona jwcontrol y limpia build.extra_libs." -ForegroundColor DarkGray
Write-Host "El core.a versionado no se toca hasta que el build fuente haya pasado." -ForegroundColor DarkGray

try
{
    Enable-SourceCoreOverride

    try
    {
        $result = Invoke-ArduinoCompile `
            -BuildPath $buildPath `
            -LogPath $logPath `
            -SketchPath $sketchPath
    }
    finally
    {
        Restore-BoardsLocal
    }

    $outputText = ($result.Output -join [Environment]::NewLine)

    if ($outputText -notmatch "Using core 'jwcontrol'")
    {
        throw "El build fuente no reporto Using core 'jwcontrol'."
    }
    if ($outputText -match "Using core 'jwcontrol_precompiled_stub'")
    {
        throw "El build fuente uso inesperadamente jwcontrol_precompiled_stub."
    }
    if ($result.CompileDb.SourceCount -lt 1)
    {
        throw "compile_commands.json no contiene fuentes reales de cores/jwcontrol."
    }
    if ($result.CompileDb.StubCount -ne 0)
    {
        throw "El build fuente compilo inesperadamente el stub precompilado."
    }

    $peripheralsCount = @(
        $result.CompileDb.SourceFiles | Where-Object {
            [System.IO.Path]::GetFileName($_) -ieq "peripherals_init.cpp"
        }
    ).Count

    if ($peripheralsCount -ne 1)
    {
        throw "Se esperaba exactamente 1 peripherals_init.cpp compilado; obtenido: $peripheralsCount"
    }

    $builtCore = Join-Path $buildPath "core\core.a"
    if (-not (Test-Path $builtCore))
    {
        throw "No se genero core.a fuente: $builtCore"
    }

    Copy-Item -LiteralPath $builtCore -Destination $candidatePath -Force

    $candidateSha = (Get-FileHash -Path $candidatePath -Algorithm SHA256).Hash.ToLowerInvariant()
    $candidateBytes = (Get-Item $candidatePath).Length
    $sameArchiveSha = ($candidateSha -eq $archiveOriginalSha)

    Write-Host ""
    Write-Host ("Build fuente PASS en {0:N3} s" -f ($result.DurationMs / 1000.0)) -ForegroundColor Green
    Write-Host ("compile_commands: total={0}, jwcontrol={1}, stub={2}" -f $result.CompileDb.Entries, $result.CompileDb.SourceCount, $result.CompileDb.StubCount) -ForegroundColor Green
    Write-Host ("Candidato: {0} bytes" -f $candidateBytes) -ForegroundColor Green
    Write-Host ("SHA256: {0}" -f $candidateSha) -ForegroundColor Green
    Write-Host ("SHA reproducido respecto al archive previo: {0}" -f $sameArchiveSha) -ForegroundColor DarkGray

    # Solo ahora se adopta el candidato. Verify-JWPLCPrecompiledCore.ps1 se
    # encarga del gate separado con el target normal stub + core.a.
    Copy-Item -LiteralPath $candidatePath -Destination $ArchivePath -Force
    $archiveReplaced = $true

    $installedSha = (Get-FileHash -Path $ArchivePath -Algorithm SHA256).Hash.ToLowerInvariant()
    if ($installedSha -ne $candidateSha)
    {
        throw "El core.a instalado no coincide con el candidato generado."
    }

    $lines = New-Object System.Collections.Generic.List[string]
    $lines.Add("# Core JWPLC precompilado - build normalizado")
    $lines.Add("")
    $lines.Add(("Run: {0}" -f $runId))
    $lines.Add("")
    $lines.Add("Fuente canonica: cores/jwcontrol")
    $lines.Add("Target de generacion: jwplc_local:esp32:jwplcbasic")
    $lines.Add("Override temporal: build.core=jwcontrol; build.extra_libs vacio")
    $lines.Add("")
    $lines.Add(("Tiempo fuente: {0:N3} s" -f ($result.DurationMs / 1000.0)))
    $lines.Add(("Compile DB total: {0}" -f $result.CompileDb.Entries))
    $lines.Add(("jwcontrol TUs: {0}" -f $result.CompileDb.SourceCount))
    $lines.Add(("stub TUs: {0}" -f $result.CompileDb.StubCount))
    $lines.Add(("peripherals_init.cpp: {0}" -f $peripheralsCount))
    $lines.Add("")
    $lines.Add(("Archive anterior bytes: {0}" -f $archiveOriginalBytes))
    $lines.Add(("Archive anterior SHA256: {0}" -f $archiveOriginalSha))
    $lines.Add(("Archive candidato bytes: {0}" -f $candidateBytes))
    $lines.Add(("Archive candidato SHA256: {0}" -f $candidateSha))
    $lines.Add(("Archive SHA reproducido: {0}" -f $sameArchiveSha))
    $lines.Add("")
    $lines.Add("CORE_PRECOMPILED_BUILD=PASS")
    $lines | Out-File -FilePath $summaryPath -Encoding utf8

    Write-Host ""
    Write-Host "CORE_PRECOMPILED_BUILD=PASS" -ForegroundColor Green
    Write-Host ("Resumen: {0}" -f $summaryPath)
    Write-Host ""
    Write-Host "Siguiente gate: Verify-JWPLCPrecompiledCore.ps1" -ForegroundColor Cyan
}
catch
{
    Restore-BoardsLocal

    if ($archiveReplaced)
    {
        Write-Host "Restaurando core.a previo por fallo posterior a la copia..." -ForegroundColor Yellow
        Copy-Item -LiteralPath $archiveBackupPath -Destination $ArchivePath -Force
    }

    throw
}
finally
{
    Restore-BoardsLocal
}
