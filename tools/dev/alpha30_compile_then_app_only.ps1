param(
    [string]$Sketch = "C:\Users\jeykc\Documentos\GitHub\platform-jwplc\JWPLC\JWPLC-2.0.0\libraries\JWPLC_GlobalPeripherals\examples\JWPLC_IO_BlockMirror",
    [string]$Fqbn = "jwplc:esp32:jwplcbasic",
    [string]$BuildPath = "C:\JWPLC_Build\jwplcbasic",
    [string]$CachePath = "C:\JWPLC_Build\cache",
    [string]$Libraries = "C:\Users\jeykc\Documentos\Programacion\Arduino\libraries",
    [string]$Port = "COM14",
    [string]$Baud = "921600",
    [string]$Esptool = "$env:LOCALAPPDATA\Arduino15\packages\jwplc\tools\esptool_py\5.2.0\esptool.exe",
    [switch]$Clean
)

Write-Host ""
Write-Host "Alpha30 compile + app-only upload"
Write-Host "---------------------------------"
Write-Host "Sketch:    $Sketch"
Write-Host "FQBN:      $Fqbn"
Write-Host "BuildPath: $BuildPath"
Write-Host "CachePath: $CachePath"
Write-Host "Libraries: $Libraries"
Write-Host "Port:      $Port"
Write-Host "Baud:      $Baud"
Write-Host "Clean:     $Clean"
Write-Host "---------------------------------"
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

$compileArgs = @(
    "compile",
    "--fqbn", $Fqbn,
    "--build-path", $BuildPath,
    "--build-cache-path", $CachePath,
    "--libraries", $Libraries,
    $Sketch
)

$globalStart = Get-Date

Write-Host "Compile:"
Write-Host "arduino-cli $($compileArgs -join ' ')"
Write-Host ""

$compileStart = Get-Date
& arduino-cli @compileArgs
$compileExit = $LASTEXITCODE
$compileEnd = Get-Date
$compileElapsed = $compileEnd - $compileStart

if ($compileExit -ne 0) {
    Write-Host ""
    Write-Host "ERROR: compile failed."
    Write-Host ("Compile elapsed: {0:mm\:ss\.fff}" -f $compileElapsed)
    exit $compileExit
}

$projectName = (Split-Path $Sketch -Leaf) + ".ino"
$bin = Join-Path $BuildPath "$projectName.bin"

if (!(Test-Path $bin)) {
    $candidates = Get-ChildItem -Path $BuildPath -Filter "*.bin" -File |
        Where-Object {
            $_.Name -notlike "*.bootloader.bin" -and
            $_.Name -notlike "*.partitions.bin" -and
            $_.Name -notlike "*.merged.bin"
        } |
        Sort-Object LastWriteTime -Descending

    if ($candidates.Count -eq 0) {
        Write-Host "ERROR: no se encontró binario de aplicación para subir."
        exit 1
    }

    $bin = $candidates[0].FullName
}

if (!(Test-Path $Esptool)) {
    Write-Host "ERROR: esptool.exe no encontrado en:"
    Write-Host $Esptool
    exit 1
}

$uploadArgs = @(
    "--chip", "esp32",
    "--port", $Port,
    "--baud", $Baud,
    "--before", "default-reset",
    "--after", "hard-reset",
    "write-flash",
    "-z",
    "--flash-mode", "keep",
    "--flash-freq", "keep",
    "--flash-size", "keep",
    "0x10000", $bin
)

Write-Host ""
Write-Host "App-only upload:"
Write-Host "$Esptool $($uploadArgs -join ' ')"
Write-Host ""

$uploadStart = Get-Date
& $Esptool @uploadArgs
$uploadExit = $LASTEXITCODE
$uploadEnd = Get-Date
$uploadElapsed = $uploadEnd - $uploadStart

$globalEnd = Get-Date
$globalElapsed = $globalEnd - $globalStart

Write-Host ""
Write-Host "---------------------------------"
Write-Host "Compile exit: $compileExit"
Write-Host "Upload exit:  $uploadExit"
Write-Host ("Compile:      {0:mm\:ss\.fff}" -f $compileElapsed)
Write-Host ("App upload:   {0:mm\:ss\.fff}" -f $uploadElapsed)
Write-Host ("Total:        {0:mm\:ss\.fff}" -f $globalElapsed)
Write-Host "Started:      $globalStart"
Write-Host "Finished:     $globalEnd"
Write-Host "---------------------------------"

exit $uploadExit
