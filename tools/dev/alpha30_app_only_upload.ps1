param(
    [string]$BuildPath = "C:\JWPLC_Build\jwplcbasic",
    [string]$ProjectName = "JWPLC_IO_BlockMirror.ino",
    [string]$Port = "COM14",
    [string]$Baud = "921600",
    [string]$Esptool = "$env:LOCALAPPDATA\Arduino15\packages\jwplc\tools\esptool_py\5.2.0\esptool.exe"
)

Write-Host ""
Write-Host "Alpha30 JWPLC app-only upload"
Write-Host "-----------------------------"
Write-Host "BuildPath: $BuildPath"
Write-Host "Project:   $ProjectName"
Write-Host "Port:      $Port"
Write-Host "Baud:      $Baud"
Write-Host "Esptool:   $Esptool"
Write-Host "-----------------------------"
Write-Host ""

if (!(Test-Path $Esptool)) {
    Write-Host "ERROR: esptool.exe no encontrado en:"
    Write-Host $Esptool
    exit 1
}

$bin = Join-Path $BuildPath "$ProjectName.bin"

if (!(Test-Path $bin)) {
    Write-Host "No se encontró binario esperado:"
    Write-Host $bin
    Write-Host ""
    Write-Host "Buscando binario de aplicación en BuildPath..."

    $candidates = Get-ChildItem -Path $BuildPath -Filter "*.bin" -File |
        Where-Object {
            $_.Name -notlike "*.bootloader.bin" -and
            $_.Name -notlike "*.partitions.bin" -and
            $_.Name -notlike "*.merged.bin"
        } |
        Sort-Object LastWriteTime -Descending

    if ($candidates.Count -eq 0) {
        Write-Host "ERROR: no se encontró ningún .bin de aplicación."
        exit 1
    }

    $bin = $candidates[0].FullName
}

Write-Host "Application bin:"
Write-Host $bin
Write-Host ""

$argsList = @(
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

$start = Get-Date

Write-Host "Running:"
Write-Host "$Esptool $($argsList -join ' ')"
Write-Host ""

& $Esptool @argsList

$exitCode = $LASTEXITCODE
$end = Get-Date
$elapsed = $end - $start

Write-Host ""
Write-Host "-----------------------------"
Write-Host "Exit code: $exitCode"
Write-Host ("Elapsed:   {0:mm\:ss\.fff}" -f $elapsed)
Write-Host "Started:   $start"
Write-Host "Finished:  $end"
Write-Host "-----------------------------"

exit $exitCode
