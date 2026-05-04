param(
    [string]$Sketch = "C:\Users\jeykc\Documentos\GitHub\platform-jwplc\JWPLC\JWPLC-2.0.0\libraries\JWPLC_GlobalPeripherals\examples\JWPLC_IO_BlockMirror",
    [string]$Fqbn = "jwplc:esp32:jwplcbasic",
    [string]$BuildPath = "C:\JWPLC_Build\jwplcbasic",
    [string]$CachePath = "C:\JWPLC_Build\cache",
    [string]$Port = "COM14",
    [string]$Libraries = "C:\Users\jeykc\Documentos\Programacion\Arduino\libraries",
    [switch]$Upload,
    [switch]$Clean
)

Write-Host ""
Write-Host "Alpha30 JWPLC build measurement"
Write-Host "--------------------------------"
Write-Host "Sketch:     $Sketch"
Write-Host "FQBN:       $Fqbn"
Write-Host "BuildPath:  $BuildPath"
Write-Host "CachePath:  $CachePath"
Write-Host "Upload:     $Upload"
Write-Host "Clean:      $Clean"
Write-Host "Libraries:  $Libraries"
Write-Host "--------------------------------"
Write-Host ""

if ($Clean) {
    if (Test-Path $BuildPath) {
        Write-Host "Removing build path: $BuildPath"
        Remove-Item -Recurse -Force $BuildPath
    }

    if (Test-Path $CachePath) {
        Write-Host "Removing cache path: $CachePath"
        Remove-Item -Recurse -Force $CachePath
    }
}

New-Item -ItemType Directory -Force -Path $BuildPath | Out-Null
New-Item -ItemType Directory -Force -Path $CachePath | Out-Null

$argsList = @(
    "compile",
    "--fqbn", $Fqbn,
    "--build-path", $BuildPath,
    "--build-cache-path", $CachePath,
    "--libraries", $Libraries
)

if ($Upload) {
    $argsList += @("--upload", "-p", $Port)
}

$argsList += @($Sketch)

$start = Get-Date

Write-Host "Running:"
Write-Host "arduino-cli $($argsList -join ' ')"
Write-Host ""

& arduino-cli @argsList

$exitCode = $LASTEXITCODE
$end = Get-Date
$elapsed = $end - $start

Write-Host ""
Write-Host "--------------------------------"
Write-Host "Exit code:  $exitCode"
Write-Host ("Elapsed:    {0:mm\:ss\.fff}" -f $elapsed)
Write-Host "Started:    $start"
Write-Host "Finished:   $end"
Write-Host "--------------------------------"

exit $exitCode
