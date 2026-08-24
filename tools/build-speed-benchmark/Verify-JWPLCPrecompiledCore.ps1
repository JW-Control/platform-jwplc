[CmdletBinding()]
param(
    [ValidateSet("Basic", "Core")]
    [string]$Target = "Basic",
    [string]$ArduinoCli = "arduino-cli",
    [int]$Jobs = 0,
    [string]$ReferenceRunPath = ""
)

Set-StrictMode -Version 2.0
$ErrorActionPreference = "Stop"

$ScriptRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$RepoRoot = [System.IO.Path]::GetFullPath((Join-Path $ScriptRoot "..\.."))
$PlatformRoot = Join-Path $RepoRoot "JWPLC\2.1.0"
$SketchPath = Join-Path $ScriptRoot "sketches\01_empty"
$VerifyRoot = Join-Path $ScriptRoot "core-precompiled-verify-work"
$BoardsLocalPath = Join-Path $PlatformRoot "boards.local.txt"
$ArchivePath = Join-Path $PlatformRoot "precompiled\core\JWPLCBASIC\core.a"
$SourceCoreRoot = Join-Path $PlatformRoot "cores\jwcontrol"
$StubCorePath = Join-Path $PlatformRoot "cores\jwcontrol_precompiled_stub\precompiled_core_stub.c"

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

function Get-BoardsLocalState
{
    if (-not (Test-Path $BoardsLocalPath))
    {
        return [PSCustomObject]@{
            Exists = $false
            Length = [int64]0
            SHA256 = ""
        }
    }

    $item = Get-Item -LiteralPath $BoardsLocalPath
    return [PSCustomObject]@{
        Exists = $true
        Length = [int64]$item.Length
        SHA256 = (Get-FileHash -LiteralPath $BoardsLocalPath -Algorithm SHA256).Hash.ToLowerInvariant()
    }
}

function Assert-BoardsLocalUnchanged
{
    param(
        [Parameter(Mandatory = $true)][object]$Before,
        [Parameter(Mandatory = $true)][object]$After
    )

    if ($Before.Exists -ne $After.Exists)
    {
        throw "boards.local.txt cambio de existencia durante el gate."
    }

    if ($Before.Exists)
    {
        if ($Before.Length -ne $After.Length -or $Before.SHA256 -ne $After.SHA256)
        {
            throw "boards.local.txt fue modificado durante el gate."
        }
    }
}

function Test-NativeOutputContains
{
    param(
        [AllowNull()][AllowEmptyCollection()][object[]]$Output,
        [Parameter(Mandatory = $true)][string]$Pattern
    )

    foreach ($line in $Output)
    {
        if (([string]$line) -match $Pattern)
        {
            return $true
        }
    }
    return $false
}

function Invoke-VerificationBuild
{
    param(
        [Parameter(Mandatory = $true)][string]$Fqbn,
        [Parameter(Mandatory = $true)][string]$BuildPath,
        [Parameter(Mandatory = $true)][string]$LogPath
    )

    if (Test-Path $BuildPath)
    {
        Remove-Item -LiteralPath $BuildPath -Recurse -Force
    }
    New-Item -ItemType Directory -Path $BuildPath -Force | Out-Null

    $args = @(
        "compile",
        "-b", $Fqbn,
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

    @($native.Output) | Out-File -LiteralPath $LogPath -Encoding utf8

    if ($native.ExitCode -ne 0)
    {
        @($native.Output | Select-Object -Last 20) | ForEach-Object { Write-Host $_ -ForegroundColor DarkYellow }
        throw "Arduino CLI fallo. Revisar: $LogPath"
    }

    $appBin = Get-ChildItem -LiteralPath $BuildPath -Filter "*.ino.bin" -File | Select-Object -First 1
    if ($null -eq $appBin)
    {
        throw "No se genero app .ino.bin en $BuildPath"
    }

    return [PSCustomObject]@{
        DurationMs = [Math]::Round($sw.Elapsed.TotalMilliseconds, 3)
        Output = @($native.Output)
        CompileDb = Get-CompileDatabaseInfo -BuildPath $BuildPath
        AppBin = $appBin
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

foreach ($requiredPath in @($SketchPath, $ArchivePath, $SourceCoreRoot, $StubCorePath))
{
    if (-not (Test-Path $requiredPath))
    {
        throw "Falta ruta requerida: $requiredPath"
    }
}

if (-not [string]::IsNullOrWhiteSpace($ReferenceRunPath))
{
    Write-Warning "-ReferenceRunPath se conserva por compatibilidad, pero ya no participa del gate Alpha5."
    Write-Warning "La equivalencia binaria fuente/precompilado no es un criterio valido porque chip-debug-report.cpp y firmware_msc_fat.c usan __DATE__/__TIME__."
}

$runId = (Get-Date).ToString("yyyyMMdd_HHmmss")
$runRoot = Join-Path $VerifyRoot $runId
$buildPath = Join-Path $runRoot ("verify-{0}" -f $Target)
$logPath = Join-Path $runRoot ("verify-{0}.log" -f $Target)
$summaryPath = Join-Path $runRoot "CORE_PRECOMPILED_VERIFY_SUMMARY.md"
New-Item -ItemType Directory -Path $runRoot -Force | Out-Null

$boardsBefore = Get-BoardsLocalState

if ($Target -eq "Basic")
{
    $fqbn = "jwplc_local:esp32:jwplcbasic"
    Write-Host "Core JWPLC precompilado - gate normalizado" -ForegroundColor Cyan
    Write-Host "Target: Basic normal"
    Write-Host "Esperado: jwcontrol_precompiled_stub + precompiled/core/JWPLCBASIC/core.a"

    $result = Invoke-VerificationBuild -Fqbn $fqbn -BuildPath $buildPath -LogPath $logPath
    $boardsAfter = Get-BoardsLocalState
    Assert-BoardsLocalUnchanged -Before $boardsBefore -After $boardsAfter

    $usesStub = Test-NativeOutputContains -Output $result.Output -Pattern "Using core 'jwcontrol_precompiled_stub'"
    $usesSource = Test-NativeOutputContains -Output $result.Output -Pattern "Using core 'jwcontrol'"
    $archiveLinked = Test-NativeOutputContains -Output $result.Output -Pattern '[\\/]precompiled[\\/]core[\\/]JWPLCBASIC[\\/]core\.a'
    $stubNamedCount = @(
        $result.CompileDb.StubFiles | Where-Object {
            [System.IO.Path]::GetFileName($_) -ieq "precompiled_core_stub.c"
        }
    ).Count

    Write-Host ""
    Write-Host ("Tiempo: {0:N3} s" -f ($result.DurationMs / 1000.0)) -ForegroundColor Green
    Write-Host ("compile_commands: total={0}, jwcontrol={1}, stub={2}" -f $result.CompileDb.Entries, $result.CompileDb.SourceCount, $result.CompileDb.StubCount)
    Write-Host ("Using stub normalizado: {0}" -f $usesStub)
    Write-Host ("Using core fuente: {0}" -f $usesSource)
    Write-Host ("precompiled_core_stub.c: {0}" -f $stubNamedCount)
    Write-Host ("core.a JWPLCBASIC enlazado: {0}" -f $archiveLinked)
    Write-Host ("App: {0} bytes" -f $result.AppBin.Length)

    if (-not $usesStub) { throw "Basic normal no reporto Using core 'jwcontrol_precompiled_stub'." }
    if ($usesSource) { throw "Basic normal uso inesperadamente jwcontrol fuente." }
    if ($result.CompileDb.SourceCount -ne 0) { throw "Basic normal compilo fuentes de cores/jwcontrol." }
    if ($result.CompileDb.StubCount -ne 1) { throw ("Basic normal esperaba 1 TU de stub; obtuvo {0}." -f $result.CompileDb.StubCount) }
    if ($stubNamedCount -ne 1) { throw "Basic normal no compilo exactamente precompiled_core_stub.c." }
    if (-not $archiveLinked) { throw "Basic normal no mostro enlace del core.a oficial JWPLCBASIC." }

    $summary = @(
        "# Core JWPLC precompilado - verificacion normalizada",
        "",
        ("Run: {0}" -f $runId),
        "Target: Basic normal",
        ("FQBN: {0}" -f $fqbn),
        "",
        ("Tiempo: {0:N3} s" -f ($result.DurationMs / 1000.0)),
        ("Compile DB total: {0}" -f $result.CompileDb.Entries),
        ("jwcontrol source TUs: {0}" -f $result.CompileDb.SourceCount),
        ("stub TUs: {0}" -f $result.CompileDb.StubCount),
        ("precompiled_core_stub.c: {0}" -f $stubNamedCount),
        ("Using jwcontrol_precompiled_stub: {0}" -f $usesStub),
        ("core.a JWPLCBASIC enlazado: {0}" -f $archiveLinked),
        "",
        "Nota de reproducibilidad:",
        "- chip-debug-report.cpp.o depende de __DATE__/__TIME__.",
        "- firmware_msc_fat.c.o depende de __DATE__/__TIME__.",
        "- No se exige SHA bit-a-bit entre builds fuente ejecutados en instantes distintos.",
        "",
        "CORE_PRECOMPILED_VERIFY_BASIC=PASS"
    )
    $summary | Out-File -LiteralPath $summaryPath -Encoding utf8

    Write-Host ""
    Write-Host "CORE_PRECOMPILED_VERIFY_BASIC=PASS" -ForegroundColor Green
}
else
{
    $fqbn = "jwplc_local:esp32:jwplcbasiccore"
    Write-Host "Core JWPLC fuente - control inverso normalizado" -ForegroundColor Cyan
    Write-Host "Target: Basic Core"
    Write-Host "Esperado: cores/jwcontrol fuente; sin stub y sin core.a JWPLCBASIC"

    $result = Invoke-VerificationBuild -Fqbn $fqbn -BuildPath $buildPath -LogPath $logPath
    $boardsAfter = Get-BoardsLocalState
    Assert-BoardsLocalUnchanged -Before $boardsBefore -After $boardsAfter

    $usesSource = Test-NativeOutputContains -Output $result.Output -Pattern "Using core 'jwcontrol'"
    $usesStub = Test-NativeOutputContains -Output $result.Output -Pattern "Using core 'jwcontrol_precompiled_stub'"
    $archiveLinked = Test-NativeOutputContains -Output $result.Output -Pattern '[\\/]precompiled[\\/]core[\\/]JWPLCBASIC[\\/]core\.a'
    $peripheralsCount = @(
        $result.CompileDb.SourceFiles | Where-Object {
            [System.IO.Path]::GetFileName($_) -ieq "peripherals_init.cpp"
        }
    ).Count

    Write-Host ""
    Write-Host ("Tiempo: {0:N3} s" -f ($result.DurationMs / 1000.0)) -ForegroundColor Green
    Write-Host ("compile_commands: total={0}, jwcontrol={1}, stub={2}" -f $result.CompileDb.Entries, $result.CompileDb.SourceCount, $result.CompileDb.StubCount)
    Write-Host ("Using core fuente: {0}" -f $usesSource)
    Write-Host ("Using stub normalizado: {0}" -f $usesStub)
    Write-Host ("peripherals_init.cpp: {0}" -f $peripheralsCount)
    Write-Host ("core.a JWPLCBASIC enlazado: {0}" -f $archiveLinked)
    Write-Host ("App: {0} bytes" -f $result.AppBin.Length)

    if (-not $usesSource) { throw "Basic Core no reporto Using core 'jwcontrol'." }
    if ($usesStub) { throw "Basic Core uso inesperadamente el stub precompilado." }
    if ($result.CompileDb.SourceCount -lt 1) { throw "Basic Core no compilo fuentes de cores/jwcontrol." }
    if ($result.CompileDb.StubCount -ne 0) { throw "Basic Core compilo inesperadamente el stub precompilado." }
    if ($peripheralsCount -ne 1) { throw ("Basic Core esperaba 1 peripherals_init.cpp; obtuvo {0}." -f $peripheralsCount) }
    if ($archiveLinked) { throw "Basic Core enlazo inesperadamente core.a JWPLCBASIC." }

    $summary = @(
        "# Core JWPLC fuente - control inverso normalizado",
        "",
        ("Run: {0}" -f $runId),
        "Target: Basic Core",
        ("FQBN: {0}" -f $fqbn),
        "",
        ("Tiempo: {0:N3} s" -f ($result.DurationMs / 1000.0)),
        ("Compile DB total: {0}" -f $result.CompileDb.Entries),
        ("jwcontrol source TUs: {0}" -f $result.CompileDb.SourceCount),
        ("stub TUs: {0}" -f $result.CompileDb.StubCount),
        ("peripherals_init.cpp: {0}" -f $peripheralsCount),
        ("Using jwcontrol: {0}" -f $usesSource),
        ("core.a JWPLCBASIC enlazado: {0}" -f $archiveLinked),
        "",
        "CORE_PRECOMPILED_VERIFY_CORE=PASS"
    )
    $summary | Out-File -LiteralPath $summaryPath -Encoding utf8

    Write-Host ""
    Write-Host "CORE_PRECOMPILED_VERIFY_CORE=PASS" -ForegroundColor Green
}

Write-Host ("Resumen: {0}" -f $summaryPath)
Write-Host ("Log: {0}" -f $logPath)
