[CmdletBinding()]
param(
    [string]$ArduinoCli = "arduino-cli",
    [string]$PackageNamespace = "jwplc_local",
    [ValidateSet("Basic", "Core")]
    [string[]]$Targets = @("Basic", "Core"),
    [string[]]$Sketches = @("01_empty"),
    [string]$Port = "",
    [switch]$UploadCore,
    [switch]$SkipManagedCache,
    [switch]$SkipExplicitBuild,
    [switch]$SkipUploads,
    [int]$Jobs = 0,
    [string]$RunLabel = "",
    [string]$OutputRoot = ""
)

Set-StrictMode -Version 2.0
$ErrorActionPreference = "Stop"

$ScriptRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$RepoRoot = [System.IO.Path]::GetFullPath((Join-Path $ScriptRoot "..\.."))
$SketchRoot = Join-Path $ScriptRoot "sketches"

if ([string]::IsNullOrWhiteSpace($OutputRoot))
{
    $OutputRoot = Join-Path $ScriptRoot "results"
}

function Get-JWPLCFqbn
{
    param([Parameter(Mandatory = $true)][string]$Target)

    switch ($Target)
    {
        "Basic" { return "${PackageNamespace}:esp32:jwplcbasic" }
        "Core"  { return "${PackageNamespace}:esp32:jwplcbasiccore" }
        default { throw "Target no soportado: $Target" }
    }
}

function Get-SafeName
{
    param([string]$Text)
    return ($Text -replace '[^A-Za-z0-9_.-]', '_')
}

function Get-BinaryBytes
{
    param([string]$BuildPath)

    if ([string]::IsNullOrWhiteSpace($BuildPath) -or -not (Test-Path $BuildPath))
    {
        return 0
    }

    $bins = @(Get-ChildItem -Path $BuildPath -Filter "*.bin" -File -ErrorAction SilentlyContinue)
    if ($bins.Count -eq 0)
    {
        return 0
    }

    return [int64](($bins | Measure-Object -Property Length -Sum).Sum)
}

function Invoke-NativeCaptured
{
    param(
        [Parameter(Mandatory = $true)][string]$FilePath,
        [Parameter(Mandatory = $true)][string[]]$Arguments
    )

    # Windows PowerShell 5.1 convierte stderr de ejecutables nativos en
    # NativeCommandError cuando ErrorActionPreference=Stop. Arduino CLI usa
    # stderr también para salida informativa/verbose, así que durante la
    # invocación nativa se permite esa salida y se decide éxito únicamente
    # mediante LASTEXITCODE.
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
        Output   = $nativeOutput
    }
}

function Invoke-TimedCli
{
    param(
        [Parameter(Mandatory = $true)][string[]]$Arguments,
        [Parameter(Mandatory = $true)][string]$Phase,
        [Parameter(Mandatory = $true)][string]$Target,
        [Parameter(Mandatory = $true)][string]$Fqbn,
        [Parameter(Mandatory = $true)][string]$Sketch,
        [Parameter(Mandatory = $true)][string]$CacheMode,
        [Parameter(Mandatory = $true)][string]$LogPath,
        [string]$BuildPath = "",
        [string]$Notes = ""
    )

    $logDirectory = Split-Path -Parent $LogPath
    if (-not (Test-Path $logDirectory))
    {
        New-Item -ItemType Directory -Path $logDirectory -Force | Out-Null
    }

    Write-Host ""
    Write-Host ("[{0}] {1} / {2}" -f $Phase, $Target, $Sketch) -ForegroundColor Cyan
    Write-Host ("  {0} {1}" -f $ArduinoCli, ($Arguments -join " ")) -ForegroundColor DarkGray

    $stopwatch = [System.Diagnostics.Stopwatch]::StartNew()
    $native = Invoke-NativeCaptured -FilePath $ArduinoCli -Arguments $Arguments
    $stopwatch.Stop()

    $output = @($native.Output)
    $exitCode = [int]$native.ExitCode

    $output | Out-File -FilePath $LogPath -Encoding utf8

    $compilerInvocations = @(
        $output | Where-Object { ([string]$_) -match '-MMD\s+-c\s' }
    ).Count

    $linkInvocations = @(
        $output | Where-Object { ([string]$_) -match '-Wl,--Map=' }
    ).Count

    $binaryBytes = Get-BinaryBytes -BuildPath $BuildPath

    $result = [PSCustomObject]@{
        TimestampUtc        = [DateTime]::UtcNow.ToString("o")
        RunLabel            = $RunLabel
        Target              = $Target
        FQBN                = $Fqbn
        Sketch              = $Sketch
        Phase               = $Phase
        CacheMode           = $CacheMode
        Success             = ($exitCode -eq 0)
        ExitCode            = $exitCode
        DurationMs          = [Math]::Round($stopwatch.Elapsed.TotalMilliseconds, 3)
        CompilerInvocations = $compilerInvocations
        LinkInvocations     = $linkInvocations
        BinaryBytes         = $binaryBytes
        BuildPath           = $BuildPath
        LogPath             = $LogPath
        Notes               = $Notes
    }

    if ($result.Success)
    {
        Write-Host ("  OK  {0:N3} s | compiladores: {1}" -f ($result.DurationMs / 1000.0), $compilerInvocations) -ForegroundColor Green
    }
    else
    {
        Write-Host ("  FAIL exit={0} | {1:N3} s | revisar {2}" -f $exitCode, ($result.DurationMs / 1000.0), $LogPath) -ForegroundColor Red
        if ($output.Count -gt 0)
        {
            Write-Host "  Ultimas lineas:" -ForegroundColor Yellow
            @($output | Select-Object -Last 8) | ForEach-Object { Write-Host ("    {0}" -f $_) -ForegroundColor DarkYellow }
        }
    }

    return $result
}

function Touch-Sketch
{
    param([string]$SketchPath)

    $ino = Get-ChildItem -Path $SketchPath -Filter "*.ino" -File | Select-Object -First 1
    if ($null -eq $ino)
    {
        throw "No se encontró .ino en $SketchPath"
    }

    # Simula una edición mínima sin modificar el contenido del benchmark.
    $ino.LastWriteTime = Get-Date
}

function Invoke-TextCommand
{
    param(
        [Parameter(Mandatory = $true)][string[]]$Arguments,
        [Parameter(Mandatory = $true)][string]$Destination
    )

    $native = Invoke-NativeCaptured -FilePath $ArduinoCli -Arguments $Arguments
    @($native.Output) | Out-File -FilePath $Destination -Encoding utf8
    return $native.ExitCode
}

function Save-EnvironmentInfo
{
    param([string]$Destination)

    $cpuName = "unknown"
    $logicalCores = [Environment]::ProcessorCount
    $ramBytes = 0
    $osCaption = [Environment]::OSVersion.VersionString

    try
    {
        $cpu = Get-CimInstance Win32_Processor | Select-Object -First 1
        if ($null -ne $cpu)
        {
            $cpuName = $cpu.Name
            $logicalCores = $cpu.NumberOfLogicalProcessors
        }

        $computer = Get-CimInstance Win32_ComputerSystem
        if ($null -ne $computer)
        {
            $ramBytes = [int64]$computer.TotalPhysicalMemory
        }

        $os = Get-CimInstance Win32_OperatingSystem
        if ($null -ne $os)
        {
            $osCaption = ("{0} {1}" -f $os.Caption, $os.Version)
        }
    }
    catch
    {
        # El benchmark sigue siendo util aunque CIM no esté disponible.
    }

    $cliVersion = "unknown"
    $cliVersionResult = Invoke-NativeCaptured -FilePath $ArduinoCli -Arguments @("version")
    if ($cliVersionResult.ExitCode -eq 0)
    {
        $cliVersion = (@($cliVersionResult.Output) -join " ").Trim()
    }

    $gitCommit = "unknown"
    $gitBranch = "unknown"
    try
    {
        $gitCommit = ((& git -C $RepoRoot rev-parse HEAD 2>$null) | Out-String).Trim()
        $gitBranch = ((& git -C $RepoRoot branch --show-current 2>$null) | Out-String).Trim()
    }
    catch
    {
    }

    $info = [ordered]@{
        timestampUtc       = [DateTime]::UtcNow.ToString("o")
        runLabel           = $RunLabel
        computerName       = $env:COMPUTERNAME
        cpu                = $cpuName
        logicalCores       = $logicalCores
        ramBytes           = $ramBytes
        os                 = $osCaption
        powershell         = $PSVersionTable.PSVersion.ToString()
        arduinoCli         = $cliVersion
        packageNamespace   = $PackageNamespace
        targets            = $Targets
        sketches           = $Sketches
        jobs               = $Jobs
        port               = $Port
        repositoryRoot     = $RepoRoot
        gitBranch          = $gitBranch
        gitCommit          = $gitCommit
    }

    $info | ConvertTo-Json -Depth 6 | Out-File -FilePath $Destination -Encoding utf8
}

function Write-MarkdownSummary
{
    param(
        [object[]]$Rows,
        [string]$Destination,
        [string]$RunId
    )

    $lines = New-Object System.Collections.Generic.List[string]
    $lines.Add("# JWPLC Build Speed Benchmark")
    $lines.Add("")
    $lines.Add("Run: ``$RunId``")
    $lines.Add("")

    if (-not [string]::IsNullOrWhiteSpace($RunLabel))
    {
        $lines.Add("Label: ``$RunLabel``")
        $lines.Add("")
    }

    $lines.Add("| Target | Sketch | Fase | Cache | Tiempo (s) | Compiladores | Link | Binarios (bytes) | Resultado |")
    $lines.Add("|---|---|---|---|---:|---:|---:|---:|---|")

    foreach ($row in $Rows)
    {
        $seconds = [Math]::Round(([double]$row.DurationMs / 1000.0), 3)
        $state = if ($row.Success) { "OK" } else { "FAIL" }
        $lines.Add(("| {0} | {1} | {2} | {3} | {4:N3} | {5} | {6} | {7} | {8} |" -f `
            $row.Target, $row.Sketch, $row.Phase, $row.CacheMode, $seconds,
            $row.CompilerInvocations, $row.LinkInvocations, $row.BinaryBytes, $state))
    }

    $lines.Add("")
    $lines.Add("## Interpretación")
    $lines.Add("")
    $lines.Add("- ``managed_*`` usa una cache Arduino aislada dentro de este run.")
    $lines.Add("- ``explicit_*`` reutiliza un ``--build-path`` fijo para observar compilación incremental.")
    $lines.Add("- ``*_touch`` cambia únicamente el timestamp del .ino.")
    $lines.Add("- ``upload_full`` usa el uploader actual del board.")
    $lines.Add("- ``upload_app_only`` escribe experimentalmente sólo la aplicación en 0x10000.")
    $lines.Add("")
    $lines.Add("No usar app-only como decisión final hasta completar la validación física correspondiente.")

    $lines | Out-File -FilePath $Destination -Encoding utf8
}

if ($Jobs -lt 0)
{
    throw "Jobs debe ser 0 o mayor."
}

if ($null -eq (Get-Command $ArduinoCli -ErrorAction SilentlyContinue))
{
    throw "No se encontró '$ArduinoCli' en PATH. Instala Arduino CLI o usa -ArduinoCli con la ruta completa."
}

$runId = (Get-Date).ToString("yyyyMMdd_HHmmss")
$runRoot = Join-Path $OutputRoot $runId
$logRoot = Join-Path $runRoot "logs"
$buildRoot = Join-Path $runRoot "build"
$cacheRoot = Join-Path $runRoot "managed-cache"

New-Item -ItemType Directory -Path $runRoot -Force | Out-Null
New-Item -ItemType Directory -Path $logRoot -Force | Out-Null
New-Item -ItemType Directory -Path $buildRoot -Force | Out-Null
New-Item -ItemType Directory -Path $cacheRoot -Force | Out-Null

Save-EnvironmentInfo -Destination (Join-Path $runRoot "environment.json")
[void](Invoke-TextCommand -Arguments @("config", "dump") -Destination (Join-Path $runRoot "arduino-cli-config.txt"))
[void](Invoke-TextCommand -Arguments @("core", "list") -Destination (Join-Path $runRoot "arduino-cli-core-list.txt"))

$results = @()
$previousBuildCache = $env:ARDUINO_BUILD_CACHE_PATH

try
{
    foreach ($target in $Targets)
    {
        $fqbn = Get-JWPLCFqbn -Target $target
        $targetSafe = Get-SafeName -Text $target

        foreach ($sketch in $Sketches)
        {
            $sketchPath = Join-Path $SketchRoot $sketch
            if (-not (Test-Path $sketchPath))
            {
                throw "Sketch no encontrado: $sketchPath"
            }

            $sketchSafe = Get-SafeName -Text $sketch
            $baseCompileArgs = @("compile", "-b", $fqbn, "-j", $Jobs.ToString(), "-v")

            if (-not $SkipManagedCache)
            {
                $isolatedCache = Join-Path $cacheRoot ("{0}_{1}" -f $targetSafe, $sketchSafe)
                if (Test-Path $isolatedCache)
                {
                    Remove-Item -Path $isolatedCache -Recurse -Force
                }
                New-Item -ItemType Directory -Path $isolatedCache -Force | Out-Null
                $env:ARDUINO_BUILD_CACHE_PATH = $isolatedCache

                $phaseArgs = $baseCompileArgs + @($sketchPath)

                $results += Invoke-TimedCli -Arguments $phaseArgs -Phase "managed_cold" -Target $target -Fqbn $fqbn -Sketch $sketch -CacheMode "isolated_arduino_cache" -LogPath (Join-Path $logRoot ("{0}_{1}_managed_cold.log" -f $targetSafe, $sketchSafe)) -Notes "Cache aislada vacía; sin --build-path."

                $results += Invoke-TimedCli -Arguments $phaseArgs -Phase "managed_warm_nochange" -Target $target -Fqbn $fqbn -Sketch $sketch -CacheMode "isolated_arduino_cache" -LogPath (Join-Path $logRoot ("{0}_{1}_managed_warm.log" -f $targetSafe, $sketchSafe)) -Notes "Mismo sketch y misma cache."

                Touch-Sketch -SketchPath $sketchPath
                $results += Invoke-TimedCli -Arguments $phaseArgs -Phase "managed_warm_touch" -Target $target -Fqbn $fqbn -Sketch $sketch -CacheMode "isolated_arduino_cache" -LogPath (Join-Path $logRoot ("{0}_{1}_managed_touch.log" -f $targetSafe, $sketchSafe)) -Notes "Timestamp del .ino actualizado; contenido idéntico."
            }

            $explicitBuildPath = Join-Path $buildRoot ("{0}_{1}" -f $targetSafe, $sketchSafe)

            if (-not $SkipExplicitBuild)
            {
                if (Test-Path $explicitBuildPath)
                {
                    Remove-Item -Path $explicitBuildPath -Recurse -Force
                }
                New-Item -ItemType Directory -Path $explicitBuildPath -Force | Out-Null

                $env:ARDUINO_BUILD_CACHE_PATH = $previousBuildCache

                $coldArgs = $baseCompileArgs + @("--build-path", $explicitBuildPath, "--clean", $sketchPath)
                $warmArgs = $baseCompileArgs + @("--build-path", $explicitBuildPath, $sketchPath)

                $results += Invoke-TimedCli -Arguments $coldArgs -Phase "explicit_cold" -Target $target -Fqbn $fqbn -Sketch $sketch -CacheMode "fixed_build_path" -LogPath (Join-Path $logRoot ("{0}_{1}_explicit_cold.log" -f $targetSafe, $sketchSafe)) -BuildPath $explicitBuildPath -Notes "--clean y --build-path fijo."

                $results += Invoke-TimedCli -Arguments $warmArgs -Phase "explicit_warm_nochange" -Target $target -Fqbn $fqbn -Sketch $sketch -CacheMode "fixed_build_path" -LogPath (Join-Path $logRoot ("{0}_{1}_explicit_warm.log" -f $targetSafe, $sketchSafe)) -BuildPath $explicitBuildPath -Notes "Mismo build-path y sin cambios."

                Touch-Sketch -SketchPath $sketchPath
                $results += Invoke-TimedCli -Arguments $warmArgs -Phase "explicit_warm_touch" -Target $target -Fqbn $fqbn -Sketch $sketch -CacheMode "fixed_build_path" -LogPath (Join-Path $logRoot ("{0}_{1}_explicit_touch.log" -f $targetSafe, $sketchSafe)) -BuildPath $explicitBuildPath -Notes "Timestamp del .ino actualizado; contenido idéntico."
            }

            $shouldUpload = (-not $SkipUploads) -and (-not [string]::IsNullOrWhiteSpace($Port))
            if (($target -eq "Core") -and (-not $UploadCore))
            {
                $shouldUpload = $false
            }

            if ($shouldUpload)
            {
                if (-not (Test-Path $explicitBuildPath))
                {
                    New-Item -ItemType Directory -Path $explicitBuildPath -Force | Out-Null
                    $prepareArgs = $baseCompileArgs + @("--build-path", $explicitBuildPath, $sketchPath)
                    $results += Invoke-TimedCli -Arguments $prepareArgs -Phase "upload_prepare" -Target $target -Fqbn $fqbn -Sketch $sketch -CacheMode "fixed_build_path" -LogPath (Join-Path $logRoot ("{0}_{1}_upload_prepare.log" -f $targetSafe, $sketchSafe)) -BuildPath $explicitBuildPath -Notes "Compilación necesaria para preparar binarios."
                }

                $fullUploadArgs = @("upload", "-b", $fqbn, "-p", $Port, "--build-path", $explicitBuildPath, "-v", $sketchPath)
                $results += Invoke-TimedCli -Arguments $fullUploadArgs -Phase "upload_full" -Target $target -Fqbn $fqbn -Sketch $sketch -CacheMode "n/a" -LogPath (Join-Path $logRoot ("{0}_{1}_upload_full.log" -f $targetSafe, $sketchSafe)) -BuildPath $explicitBuildPath -Notes "Flujo actual del board."

                $appOnlyPattern = '--chip {build.mcu} --port "{serial.port}" --baud {upload.speed} {upload.flags} --before default-reset --after hard-reset write-flash -z --flash-mode keep --flash-freq keep --flash-size keep 0x10000 "{build.path}/{build.project_name}.bin" {upload.extra_flags}'
                $appOnlyProperty = "tools.esptool_py.upload.pattern_args=$appOnlyPattern"
                $appOnlyUploadArgs = @("upload", "-b", $fqbn, "-p", $Port, "--build-path", $explicitBuildPath, "-v", "--upload-property", $appOnlyProperty, $sketchPath)

                $results += Invoke-TimedCli -Arguments $appOnlyUploadArgs -Phase "upload_app_only" -Target $target -Fqbn $fqbn -Sketch $sketch -CacheMode "n/a" -LogPath (Join-Path $logRoot ("{0}_{1}_upload_app_only.log" -f $targetSafe, $sketchSafe)) -BuildPath $explicitBuildPath -Notes "EXPERIMENTAL: sólo aplicación a 0x10000."
            }
        }
    }
}
finally
{
    $env:ARDUINO_BUILD_CACHE_PATH = $previousBuildCache
}

$csvPath = Join-Path $runRoot "results.csv"
$jsonPath = Join-Path $runRoot "results.json"
$mdPath = Join-Path $runRoot "SUMMARY.md"

$results | Export-Csv -Path $csvPath -NoTypeInformation -Encoding utf8
$results | ConvertTo-Json -Depth 6 | Out-File -FilePath $jsonPath -Encoding utf8
Write-MarkdownSummary -Rows $results -Destination $mdPath -RunId $runId

Write-Host ""
Write-Host "Benchmark terminado." -ForegroundColor Green
Write-Host ("Resultados: {0}" -f $runRoot)
Write-Host ("CSV:        {0}" -f $csvPath)
Write-Host ("Resumen:    {0}" -f $mdPath)