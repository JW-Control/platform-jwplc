#requires -Version 7.4

[CmdletBinding()]
param(
    [string]$Fqbn = "jwplc_local:esp32:jwplcbasic",
    [string]$Sketch,
    [string]$ArduinoCliPath,
    [switch]$FullCold,
    [string]$ReuseCompileDb
)

Set-StrictMode -Version 2.0
$ErrorActionPreference = "Stop"

$ScriptRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$RepoRoot = [System.IO.Path]::GetFullPath((Join-Path $ScriptRoot "..\.."))

if ([string]::IsNullOrWhiteSpace($Sketch))
{
    $Sketch = Join-Path $ScriptRoot "sketches\01_empty"
}
$Sketch = [System.IO.Path]::GetFullPath($Sketch)

function Resolve-ArduinoCli
{
    param([string]$ExplicitPath)

    if (-not [string]::IsNullOrWhiteSpace($ExplicitPath))
    {
        $resolved = [System.IO.Path]::GetFullPath($ExplicitPath)
        if (-not (Test-Path -LiteralPath $resolved -PathType Leaf))
        {
            throw "No existe arduino-cli en: $resolved"
        }
        return $resolved
    }

    $cmd = Get-Command arduino-cli -ErrorAction SilentlyContinue
    if ($null -ne $cmd)
    {
        return $cmd.Source
    }

    $candidates = New-Object System.Collections.Generic.List[string]
    if ($env:LOCALAPPDATA)
    {
        [void]$candidates.Add((Join-Path $env:LOCALAPPDATA "Programs\Arduino IDE\resources\app\lib\backend\resources\arduino-cli.exe"))
    }
    if ($env:ProgramFiles)
    {
        [void]$candidates.Add((Join-Path $env:ProgramFiles "Arduino IDE\resources\app\lib\backend\resources\arduino-cli.exe"))
    }
    $pf86 = [Environment]::GetEnvironmentVariable("ProgramFiles(x86)")
    if ($pf86)
    {
        [void]$candidates.Add((Join-Path $pf86 "Arduino IDE\resources\app\lib\backend\resources\arduino-cli.exe"))
    }

    foreach ($candidate in $candidates)
    {
        if (Test-Path -LiteralPath $candidate -PathType Leaf)
        {
            return $candidate
        }
    }

    throw "No se encontro arduino-cli. Pase -ArduinoCliPath con la ruta a arduino-cli.exe."
}

function Invoke-ArduinoCliMeasured
{
    param(
        [Parameter(Mandatory = $true)][string]$Cli,
        [Parameter(Mandatory = $true)][string[]]$Arguments
    )

    $sw = [System.Diagnostics.Stopwatch]::StartNew()
    $output = @(& $Cli @Arguments 2>&1 | ForEach-Object { [string]$_ })
    $exitCode = $LASTEXITCODE
    $sw.Stop()

    return [PSCustomObject]@{
        ExitCode = $exitCode
        Elapsed = $sw.Elapsed
        Lines = $output
    }
}

function Get-EntryInvocation
{
    param([Parameter(Mandatory = $true)]$Entry)

    if (-not ($Entry.PSObject.Properties.Name -contains "arguments"))
    {
        throw "Entrada de compile_commands.json sin arguments[]."
    }

    $allArguments = @($Entry.arguments)
    if ($allArguments.Count -lt 2)
    {
        throw "Entrada de compile_commands.json con arguments[] incompleto."
    }

    return [PSCustomObject]@{
        FileName = [string]$allArguments[0]
        Arguments = [string[]]@($allArguments | Select-Object -Skip 1)
    }
}

function Get-OutputPathFromArguments
{
    param(
        [Parameter(Mandatory = $true)][string[]]$Arguments,
        [Parameter(Mandatory = $true)][string]$WorkingDirectory
    )

    for ($i = 0; $i -lt ($Arguments.Count - 1); $i++)
    {
        if ($Arguments[$i] -eq "-o")
        {
            $outputPath = [string]$Arguments[$i + 1]
            if (-not [System.IO.Path]::IsPathRooted($outputPath))
            {
                $outputPath = [System.IO.Path]::GetFullPath((Join-Path $WorkingDirectory $outputPath))
            }
            return $outputPath
        }
    }

    return $null
}

function Get-Category
{
    param([Parameter(Mandatory = $true)][string]$Source)

    if ($Source -match '[\\/]libraries[\\/](?<lib>[^\\/]+)[\\/]')
    {
        return [PSCustomObject]@{ Type = "library"; Group = $Matches["lib"] }
    }
    if ($Source -match '[\\/]cores[\\/](?<core>[^\\/]+)[\\/]')
    {
        return [PSCustomObject]@{ Type = "core"; Group = $Matches["core"] }
    }
    if ($Source -match '[\\/]variants[\\/](?<variant>[^\\/]+)[\\/]')
    {
        return [PSCustomObject]@{ Type = "variant"; Group = $Matches["variant"] }
    }
    if ($Source -match '[\\/]sketch[\\/]' -or $Source -match '\.ino\.cpp$')
    {
        return [PSCustomObject]@{ Type = "sketch"; Group = "sketch" }
    }
    return [PSCustomObject]@{ Type = "other"; Group = "other" }
}

function Get-DisplayPath
{
    param([Parameter(Mandatory = $true)][string]$Path)

    $full = [System.IO.Path]::GetFullPath($Path)
    if ($full.StartsWith($RepoRoot, [StringComparison]::OrdinalIgnoreCase))
    {
        return $full.Substring($RepoRoot.Length).TrimStart('\','/')
    }
    return $full
}

function Set-FileOptsForSource
{
    param(
        [Parameter(Mandatory = $true)][string]$FileOptsPath,
        [Parameter(Mandatory = $true)][string]$Source
    )

    if (-not (Test-Path -LiteralPath $FileOptsPath -PathType Leaf))
    {
        return
    }

    $value = ""
    if ($Source -match '[\\/]cores[\\/]')
    {
        $value = "-DARDUINO_CORE_BUILD"
    }

    [System.IO.File]::WriteAllText(
        $FileOptsPath,
        $value,
        [System.Text.Encoding]::ASCII
    )
}

function Invoke-CompilerMeasured
{
    param(
        [Parameter(Mandatory = $true)][string]$FileName,
        [Parameter(Mandatory = $true)][string[]]$Arguments,
        [Parameter(Mandatory = $true)][string]$WorkingDirectory
    )

    $psi = [System.Diagnostics.ProcessStartInfo]::new()
    $psi.FileName = $FileName
    $psi.WorkingDirectory = $WorkingDirectory
    $psi.UseShellExecute = $false
    $psi.CreateNoWindow = $true
    $psi.RedirectStandardOutput = $true
    $psi.RedirectStandardError = $true

    foreach ($argument in $Arguments)
    {
        [void]$psi.ArgumentList.Add($argument)
    }

    $process = [System.Diagnostics.Process]::new()
    $process.StartInfo = $psi

    $sw = [System.Diagnostics.Stopwatch]::StartNew()
    if (-not $process.Start())
    {
        throw "No se pudo iniciar el compilador: $FileName"
    }

    $stdout = $process.StandardOutput.ReadToEnd()
    $stderr = $process.StandardError.ReadToEnd()
    $process.WaitForExit()
    $sw.Stop()

    return [PSCustomObject]@{
        ExitCode = $process.ExitCode
        Elapsed = $sw.Elapsed
        StdOut = $stdout
        StdErr = $stderr
    }
}

$ArduinoCli = Resolve-ArduinoCli -ExplicitPath $ArduinoCliPath
if (-not (Test-Path -LiteralPath $Sketch))
{
    throw "No existe el sketch: $Sketch"
}

$timestamp = Get-Date -Format "yyyyMMdd_HHmmss"
$WorkRoot = Join-Path $ScriptRoot "compile-profile-work"
$reuseMode = -not [string]::IsNullOrWhiteSpace($ReuseCompileDb)
$discoverySeconds = $null
$discoveryLines = @()
$DiscoveryLog = $null

if ($reuseMode)
{
    $CompileDb = [System.IO.Path]::GetFullPath($ReuseCompileDb)
    if (-not (Test-Path -LiteralPath $CompileDb -PathType Leaf))
    {
        throw "No existe compile_commands.json para reutilizar: $CompileDb"
    }

    $BuildPath = Split-Path -Parent $CompileDb
    $sourceRunRoot = Split-Path -Parent $BuildPath
    $RunRoot = Join-Path $WorkRoot ("reuse_" + $timestamp)

    $sourceDiscoveryLog = Join-Path $sourceRunRoot "discovery.log"
    if (Test-Path -LiteralPath $sourceDiscoveryLog -PathType Leaf)
    {
        $discoveryLines = @(Get-Content -LiteralPath $sourceDiscoveryLog)
    }
}
else
{
    $RunRoot = Join-Path $WorkRoot $timestamp
    $BuildPath = Join-Path $RunRoot "profile-Basic"
    $CompileDb = Join-Path $BuildPath "compile_commands.json"
    $DiscoveryLog = Join-Path $RunRoot "discovery.log"
}

$TuCsv = Join-Path $RunRoot "TU_TIMINGS.csv"
$GroupCsv = Join-Path $RunRoot "GROUP_TIMINGS.csv"
$ReportPath = Join-Path $RunRoot "REPORT.md"
$FileOpts = Join-Path $BuildPath "file_opts"

New-Item -ItemType Directory -Force -Path $RunRoot | Out-Null
if (-not $reuseMode)
{
    New-Item -ItemType Directory -Force -Path $BuildPath | Out-Null
}

Write-Host "JWPLC - perfil cold por unidad de compilacion" -ForegroundColor Cyan
Write-Host "------------------------------------------------"
Write-Host ("Arduino CLI: {0}" -f $ArduinoCli)
Write-Host ("FQBN:        {0}" -f $Fqbn)
Write-Host ("Sketch:      {0}" -f $Sketch)
Write-Host ("Run:         {0}" -f $RunRoot)
Write-Host ""

if ($reuseMode)
{
    Write-Host "Fase 1/2: reutilizando compile_commands.json existente." -ForegroundColor Yellow
    Write-Host ("Compile DB:   {0}" -f $CompileDb)
}
else
{
    Write-Host "Fase 1/2: preparando build cold y compile_commands.json..." -ForegroundColor Yellow

    $discoveryArgs = @(
        "compile",
        "--fqbn", $Fqbn,
        "-j", "1",
        "-v",
        "--clean",
        "--build-path", $BuildPath,
        "--only-compilation-database",
        $Sketch
    )

    $discovery = Invoke-ArduinoCliMeasured -Cli $ArduinoCli -Arguments $discoveryArgs
    $discoveryLines = @($discovery.Lines)
    $discoveryLines | Set-Content -LiteralPath $DiscoveryLog -Encoding utf8NoBOM

    if ($discovery.ExitCode -ne 0)
    {
        Write-Host "Fallo preparando compilation database." -ForegroundColor Red
        Write-Host ("Log: {0}" -f $DiscoveryLog)
        exit $discovery.ExitCode
    }

    $discoverySeconds = $discovery.Elapsed.TotalSeconds
}

if (-not (Test-Path -LiteralPath $CompileDb -PathType Leaf))
{
    throw "No se encontro compile_commands.json: $CompileDb"
}

$entries = @(Get-Content -LiteralPath $CompileDb -Raw | ConvertFrom-Json)
if ($entries.Count -eq 0)
{
    throw "compile_commands.json no contiene unidades de compilacion."
}

$precompiledLibraries = @(
    $discoveryLines |
        ForEach-Object {
            if ($_ -match '^Skipping dependencies detection for precompiled library (?<name>.+)$')
            {
                $Matches["name"].Trim()
            }
        } |
        Where-Object { -not [string]::IsNullOrWhiteSpace($_) } |
        Sort-Object -Unique
)

if ($null -ne $discoverySeconds)
{
    Write-Host ("Preparacion/discovery: {0:N3} s | TUs detectados: {1}" -f $discoverySeconds, $entries.Count) -ForegroundColor Green
}
else
{
    Write-Host ("Compile DB reutilizado | TUs detectados: {0}" -f $entries.Count) -ForegroundColor Green
}

Write-Host ""
Write-Host "Fase 2/2: midiendo cada TU con arguments[] nativo..." -ForegroundColor Yellow
Write-Host "Nota: los TUs se miden secuencialmente para atribuir tiempo por archivo; no equivalen al cold paralelo normal." -ForegroundColor DarkGray
Write-Host ""

$rows = New-Object System.Collections.Generic.List[object]
$index = 0

foreach ($entry in $entries)
{
    $index++
    $source = [System.IO.Path]::GetFullPath([string]$entry.file)
    $directory = [System.IO.Path]::GetFullPath([string]$entry.directory)
    $invocation = Get-EntryInvocation -Entry $entry

    Set-FileOptsForSource -FileOptsPath $FileOpts -Source $source

    $outputPath = Get-OutputPathFromArguments -Arguments $invocation.Arguments -WorkingDirectory $directory
    if (-not [string]::IsNullOrWhiteSpace($outputPath))
    {
        if (Test-Path -LiteralPath $outputPath)
        {
            Remove-Item -LiteralPath $outputPath -Force
        }

        $depPath = [System.IO.Path]::ChangeExtension($outputPath, ".d")
        if (Test-Path -LiteralPath $depPath)
        {
            Remove-Item -LiteralPath $depPath -Force
        }
    }

    $cat = Get-Category -Source $source
    $display = Get-DisplayPath -Path $source
    $leaf = Split-Path -Leaf $source

    Write-Host ("[{0,2}/{1}] {2} :: {3}" -f $index, $entries.Count, $cat.Group, $leaf) -NoNewline

    $result = Invoke-CompilerMeasured -FileName $invocation.FileName -Arguments $invocation.Arguments -WorkingDirectory $directory
    Write-Host ("  {0:N3} s" -f $result.Elapsed.TotalSeconds) -ForegroundColor Green

    if ($result.ExitCode -ne 0)
    {
        $failedLog = Join-Path $RunRoot ("FAILED_{0:D2}.log" -f $index)
        @(
            "SOURCE: $source",
            "DIRECTORY: $directory",
            "EXECUTABLE: $($invocation.FileName)",
            "EXIT: $($result.ExitCode)",
            "",
            "ARGUMENTS:",
            ($invocation.Arguments | ForEach-Object { [string]$_ }),
            "",
            "STDOUT:",
            $result.StdOut,
            "",
            "STDERR:",
            $result.StdErr
        ) | Set-Content -LiteralPath $failedLog -Encoding utf8NoBOM

        throw "Fallo compilando $source. Ver: $failedLog"
    }

    [void]$rows.Add([PSCustomObject]@{
        Index = $index
        Type = $cat.Type
        Group = $cat.Group
        Source = $display
        Leaf = $leaf
        DurationSeconds = [math]::Round($result.Elapsed.TotalSeconds, 6)
        DurationMs = [math]::Round($result.Elapsed.TotalMilliseconds, 3)
    })
}

$groups = @(
    $rows |
        Group-Object Group |
        ForEach-Object {
            $items = @($_.Group)
            [PSCustomObject]@{
                Group = $_.Name
                Type = ($items | Select-Object -First 1).Type
                TUs = $items.Count
                TotalSeconds = [math]::Round((($items | Measure-Object DurationSeconds -Sum).Sum), 6)
                AverageSeconds = [math]::Round((($items | Measure-Object DurationSeconds -Average).Average), 6)
                MaxSeconds = [math]::Round((($items | Measure-Object DurationSeconds -Maximum).Maximum), 6)
            }
        } |
        Sort-Object TotalSeconds -Descending
)

$rows | Sort-Object DurationSeconds -Descending | Export-Csv -LiteralPath $TuCsv -NoTypeInformation -Encoding utf8NoBOM
$groups | Export-Csv -LiteralPath $GroupCsv -NoTypeInformation -Encoding utf8NoBOM

$compileTotal = [double](($rows | Measure-Object DurationSeconds -Sum).Sum)
$fullColdSeconds = $null
$fullColdLog = $null

if ($FullCold)
{
    Write-Host ""
    Write-Host "Fase opcional: cold completo normal con -j 0..." -ForegroundColor Yellow
    $fullBuildPath = Join-Path $RunRoot "full-cold-Basic"
    $fullColdLog = Join-Path $RunRoot "FULL_COLD.log"

    $fullArgs = @(
        "compile",
        "--fqbn", $Fqbn,
        "-j", "0",
        "-v",
        "--clean",
        "--build-path", $fullBuildPath,
        $Sketch
    )

    $full = Invoke-ArduinoCliMeasured -Cli $ArduinoCli -Arguments $fullArgs
    $full.Lines | Set-Content -LiteralPath $fullColdLog -Encoding utf8NoBOM

    if ($full.ExitCode -ne 0)
    {
        Write-Host ("Cold completo fallo; revisar {0}" -f $fullColdLog) -ForegroundColor Red
    }
    else
    {
        $fullColdSeconds = $full.Elapsed.TotalSeconds
        Write-Host ("Cold completo -j 0: {0:N3} s" -f $fullColdSeconds) -ForegroundColor Green
    }
}

$cpuName = "no disponible"
$ramGb = "no disponible"

try
{
    $cpuName = ((Get-CimInstance Win32_Processor | Select-Object -First 1 -ExpandProperty Name) -replace '\s+', ' ').Trim()
    $ramGb = [math]::Round(((Get-CimInstance Win32_ComputerSystem).TotalPhysicalMemory / 1GB), 1)
}
catch
{
    # La informacion del host es auxiliar; no debe invalidar el perfil.
}

$report = New-Object System.Collections.Generic.List[string]
[void]$report.Add("# JWPLC - perfil cold por unidad de compilacion")
[void]$report.Add("")
[void]$report.Add(("Fecha: {0}" -f (Get-Date -Format "yyyy-MM-dd HH:mm:ss")))
[void]$report.Add("")
[void]$report.Add(("- Host: {0}" -f $env:COMPUTERNAME))
[void]$report.Add(("- CPU: {0}" -f $cpuName))
[void]$report.Add(("- RAM: {0} GB" -f $ramGb))
[void]$report.Add(("- FQBN: {0}" -f $Fqbn))
[void]$report.Add(("- Sketch: {0}" -f (Get-DisplayPath -Path $Sketch)))
[void]$report.Add(("- Arduino CLI: {0}" -f $ArduinoCli))
[void]$report.Add(("- Compile DB: {0}" -f $CompileDb))
[void]$report.Add(("- Modo reutilizacion: {0}" -f $reuseMode))
[void]$report.Add("")
[void]$report.Add("## Lectura metodologica")
[void]$report.Add("")
[void]$report.Add("Cada TU se ejecuta usando directamente el array arguments[] de compile_commands.json mediante ProcessStartInfo.ArgumentList. No se reconstruye una linea de comandos textual, para preservar correctamente argumentos con comillas internas como los defines de Arduino.")
[void]$report.Add("")
[void]$report.Add("Los tiempos por TU se miden de forma secuencial y no equivalen al tiempo de una compilacion normal paralela con -j 0. El objetivo es identificar hotspots por archivo y por biblioteca.")
[void]$report.Add("")
[void]$report.Add("## Resumen")
[void]$report.Add("")

if ($null -ne $discoverySeconds)
{
    [void]$report.Add(("- Preparacion/discovery + compilation database: **{0:N3} s**" -f $discoverySeconds))
}
else
{
    [void]$report.Add("- Preparacion/discovery: no medida en este run; se reutilizo un compile_commands.json existente.")
}

[void]$report.Add(("- TUs compilados desde fuente: **{0}**" -f $rows.Count))
[void]$report.Add(("- Suma de compilacion individual de TUs: **{0:N3} s**" -f $compileTotal))

if ($null -ne $fullColdSeconds)
{
    [void]$report.Add(("- Cold completo normal -j 0: **{0:N3} s**" -f $fullColdSeconds))
}
[void]$report.Add("")

if ($precompiledLibraries.Count -gt 0)
{
    [void]$report.Add("## Librerias detectadas como precompiladas")
    [void]$report.Add("")
    foreach ($name in $precompiledLibraries)
    {
        [void]$report.Add(("- {0}" -f $name))
    }
    [void]$report.Add("")
}

[void]$report.Add("## Tiempo por grupo")
[void]$report.Add("")
[void]$report.Add("| Grupo | Tipo | TUs | Total s | Promedio s | Max s |")
[void]$report.Add("|---|---|---:|---:|---:|---:|")

foreach ($g in $groups)
{
    [void]$report.Add(("| {0} | {1} | {2} | {3:N3} | {4:N3} | {5:N3} |" -f $g.Group, $g.Type, $g.TUs, $g.TotalSeconds, $g.AverageSeconds, $g.MaxSeconds))
}

[void]$report.Add("")
[void]$report.Add("## Unidades de compilacion - de mayor a menor")
[void]$report.Add("")
[void]$report.Add("| # | Grupo | Fuente | Tiempo s |")
[void]$report.Add("|---:|---|---|---:|")

$rank = 0
foreach ($row in @($rows | Sort-Object DurationSeconds -Descending))
{
    $rank++
    $safeSource = ([string]$row.Source).Replace('|','\|')
    [void]$report.Add(("| {0} | {1} | {2} | {3:N3} |" -f $rank, $row.Group, $safeSource, $row.DurationSeconds))
}

[void]$report.Add("")
[void]$report.Add("## Archivos generados")
[void]$report.Add("")
[void]$report.Add("- TU_TIMINGS.csv: detalle por archivo.")
[void]$report.Add("- GROUP_TIMINGS.csv: agregado por libreria/core/sketch.")

if ($null -ne $DiscoveryLog)
{
    [void]$report.Add("- discovery.log: salida de Arduino CLI al preparar el build cold.")
}
if ($null -ne $fullColdLog)
{
    [void]$report.Add("- FULL_COLD.log: cold normal opcional con -j 0.")
}

$report | Set-Content -LiteralPath $ReportPath -Encoding utf8NoBOM

Write-Host ""
Write-Host "=== TOP 10 TUs ===" -ForegroundColor Cyan

$rows |
    Sort-Object DurationSeconds -Descending |
    Select-Object -First 10 Group,Leaf,DurationSeconds |
    Format-Table -AutoSize

Write-Host "=== GRUPOS ===" -ForegroundColor Cyan
$groups | Format-Table Group,Type,TUs,TotalSeconds,AverageSeconds,MaxSeconds -AutoSize

if ($null -ne $discoverySeconds)
{
    Write-Host ("Preparacion/discovery: {0:N3} s" -f $discoverySeconds) -ForegroundColor Green
}
else
{
    Write-Host "Preparacion/discovery: reutilizado / no medido en este run" -ForegroundColor Yellow
}

Write-Host ("Suma TUs:             {0:N3} s" -f $compileTotal) -ForegroundColor Green

if ($null -ne $fullColdSeconds)
{
    Write-Host ("Cold normal -j 0:      {0:N3} s" -f $fullColdSeconds) -ForegroundColor Green
}

Write-Host ("Reporte: {0}" -f $ReportPath) -ForegroundColor Yellow
Write-Host ("CSV TU:  {0}" -f $TuCsv) -ForegroundColor DarkGray
Write-Host ("CSV grp: {0}" -f $GroupCsv) -ForegroundColor DarkGray
