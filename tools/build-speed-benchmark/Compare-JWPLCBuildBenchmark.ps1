[CmdletBinding()]
param(
    [string]$ArduinoCli = "arduino-cli",
    # Versión de referencia (p. ej. alpha.13 instalada desde Boards Manager).
    [string]$BaseNamespace = "jwplc",
    [string]$BaseLabel = "2.1.0-alpha.13",
    # Versión candidata (p. ej. jwplc_local apuntando a la rama de la alpha).
    [string]$HeadNamespace = "jwplc_local",
    [string]$HeadLabel = "2.1.0-alpha.14",
    [ValidateSet("Basic", "Core")]
    [string[]]$Targets = @("Basic"),
    [string[]]$Sketches = @("01_empty", "02_io_basic"),
    [ValidateRange(1, 20)]
    [int]$Repetitions = 3,
    [int]$Jobs = 0
)

# =============================================================================
# Compara tiempos de compilación entre dos versiones del package JWPLC.
#
# Llama a Run-JWPLCBuildBenchmark.ps1 (sólo fases managed_*: caché aislada,
# sin subida) alternando base y candidata en cada repetición, y resume la
# mediana en COMPARE.md:
#
#   Frío     = managed_cold        (caché Arduino aislada y vacía)
#   Caliente = managed_warm_touch  (misma caché; sólo cambia la fecha del .ino)
#
# Ejemplo (alpha.13 instalada como jwplc, rama alpha.14 enlazada como jwplc_local):
#
#   .\Compare-JWPLCBuildBenchmark.ps1 -Targets Basic,Core -Repetitions 3
# =============================================================================

Set-StrictMode -Version 2.0
$ErrorActionPreference = "Stop"

$ScriptRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$Runner = Join-Path $ScriptRoot "Run-JWPLCBuildBenchmark.ps1"
$OutputRoot = Join-Path $ScriptRoot ("results\compare_{0}" -f (Get-Date).ToString("yyyyMMdd_HHmmss"))
New-Item -ItemType Directory -Path $OutputRoot -Force | Out-Null

$versions = @(
    [PSCustomObject]@{ Label = $BaseLabel; Namespace = $BaseNamespace },
    [PSCustomObject]@{ Label = $HeadLabel; Namespace = $HeadNamespace }
)

$rows = @()
for ($rep = 1; $rep -le $Repetitions; $rep++)
{
    foreach ($version in $versions)
    {
        Write-Host ("== Repetición {0}/{1}: {2} ({3})" -f $rep, $Repetitions, $version.Label, $version.Namespace) -ForegroundColor Cyan
        $runOutput = Join-Path $OutputRoot ("{0}_rep{1}" -f ($version.Label -replace '[^A-Za-z0-9_.-]', '_'), $rep)

        & $Runner -ArduinoCli $ArduinoCli -PackageNamespace $version.Namespace -Targets $Targets `
            -Sketches $Sketches -SkipExplicitBuild -SkipUploads -Jobs $Jobs `
            -RunLabel ("{0}-rep{1}" -f $version.Label, $rep) -OutputRoot $runOutput

        $csv = Get-ChildItem -Path $runOutput -Filter "results.csv" -Recurse | Select-Object -First 1
        if ($null -eq $csv)
        {
            throw "No se encontró results.csv en $runOutput"
        }

        foreach ($row in (Import-Csv -Path $csv.FullName))
        {
            $rows += [PSCustomObject]@{
                Version     = $version.Label
                Repetition  = $rep
                Target      = $row.Target
                Sketch      = $row.Sketch
                Phase       = $row.Phase
                Success     = $row.Success
                DurationMs  = [double]$row.DurationMs
                BinaryBytes = $row.BinaryBytes
            }
        }
    }
}

$rows | Export-Csv -Path (Join-Path $OutputRoot "all-results.csv") -NoTypeInformation -Encoding utf8

function Get-Median
{
    param([double[]]$Values)

    $sorted = @($Values | Sort-Object)
    if ($sorted.Count -eq 0)
    {
        return [double]::NaN
    }
    $mid = [int][Math]::Floor($sorted.Count / 2)
    if ($sorted.Count % 2 -eq 1)
    {
        return $sorted[$mid]
    }
    return ($sorted[$mid - 1] + $sorted[$mid]) / 2.0
}

function Format-Seconds
{
    param([double]$Ms)

    if ([double]::IsNaN($Ms))
    {
        return "FALLO"
    }
    return ("{0:N1} s" -f ($Ms / 1000.0))
}

$failed = @($rows | Where-Object { $_.Success -ne "True" })

$cliVersion = (& $ArduinoCli version 2>$null | Out-String).Trim()
$lines = New-Object System.Collections.Generic.List[string]
$lines.Add("# Benchmark de compilación: $BaseLabel frente a $HeadLabel")
$lines.Add("")
$lines.Add(('- Fecha: {0}' -f (Get-Date).ToString("yyyy-MM-dd HH:mm")))
$lines.Add(('- Equipo: {0}, {1} núcleos lógicos' -f $env:COMPUTERNAME, [Environment]::ProcessorCount))
$lines.Add(('- arduino-cli: {0}' -f $cliVersion))
$lines.Add(('- Base: `{0}` ({1}); candidata: `{2}` ({3})' -f $BaseNamespace, $BaseLabel, $HeadNamespace, $HeadLabel))
$lines.Add(('- Repeticiones: {0} (mediana); fallos: {1}' -f $Repetitions, $failed.Count))
$lines.Add("")
$lines.Add("| Placa | Sketch | Frío $BaseLabel | Frío $HeadLabel | Caliente $BaseLabel | Caliente $HeadLabel |")
$lines.Add("|---|---|---|---|---|---|")

foreach ($target in $Targets)
{
    foreach ($sketch in $Sketches)
    {
        $cells = @()
        foreach ($phase in @("managed_cold", "managed_warm_touch"))
        {
            foreach ($version in $versions)
            {
                $values = @($rows | Where-Object {
                        $_.Version -eq $version.Label -and $_.Target -eq $target -and
                        $_.Sketch -eq $sketch -and $_.Phase -eq $phase -and $_.Success -eq "True"
                    } | ForEach-Object { $_.DurationMs })
                $cells += Format-Seconds -Ms (Get-Median -Values $values)
            }
        }
        $lines.Add(("| {0} | {1} | {2} |" -f $target, $sketch, ($cells -join " | ")))
    }
}

$lines.Add("")
$lines.Add("Frío = ``managed_cold`` (caché Arduino aislada y vacía). Caliente = ``managed_warm_touch``")
$lines.Add("(misma caché; sólo cambia la fecha del ``.ino``, como al pulsar Compilar otra vez en el IDE).")

$summary = Join-Path $OutputRoot "COMPARE.md"
$lines | Out-File -FilePath $summary -Encoding utf8

Write-Host ""
Get-Content -Encoding UTF8 $summary | Write-Host
Write-Host ""
Write-Host ("Resumen: {0}" -f $summary) -ForegroundColor Green
if ($failed.Count -gt 0)
{
    Write-Host ("Hubo {0} compilaciones fallidas: revisar all-results.csv y los logs." -f $failed.Count) -ForegroundColor Yellow
    exit 1
}
