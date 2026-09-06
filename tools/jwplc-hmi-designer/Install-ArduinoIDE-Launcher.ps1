param(
    [string]$DesignerHome = (Join-Path $env:LOCALAPPDATA 'JWPLC\HMI Designer'),
    [switch]$SkipBuild
)

$ErrorActionPreference = 'Stop'
$buildScript = Join-Path $PSScriptRoot 'Build-ArduinoIDE-Launcher.ps1'
$dist = Join-Path $PSScriptRoot 'dist'
$plugins = Join-Path $HOME '.arduinoIDE\plugins'

if (-not (Test-Path -LiteralPath (Join-Path $DesignerHome 'Start-JWPLC-HMI-Designer.ps1') -PathType Leaf)) {
    throw "JWPLC HMI Designer no está instalado en: $DesignerHome"
}

if (-not $SkipBuild) {
    if (-not (Test-Path -LiteralPath $buildScript -PathType Leaf)) {
        throw "No se encontró el script de build VSIX: $buildScript"
    }
    $vsix = (& $buildScript -OutputDirectory $dist | Select-Object -Last 1)
} else {
    $vsix = Get-ChildItem -LiteralPath $dist -Filter 'jwplc-hmi-launcher-*.vsix' -File |
        Sort-Object LastWriteTime -Descending |
        Select-Object -First 1 -ExpandProperty FullName
}

if (-not $vsix -or -not (Test-Path -LiteralPath $vsix -PathType Leaf)) {
    throw 'No se pudo localizar el VSIX del launcher JWPLC.'
}

New-Item -ItemType Directory -Force -Path $plugins | Out-Null
$currentName = Split-Path -Leaf $vsix
Get-ChildItem -LiteralPath $plugins -Filter 'jwplc-hmi-launcher-*.vsix' -File -ErrorAction SilentlyContinue |
    Where-Object { $_.Name -ne $currentName } |
    Remove-Item -Force

$destination = Join-Path $plugins $currentName
Copy-Item -LiteralPath $vsix -Destination $destination -Force
[Environment]::SetEnvironmentVariable('JWPLC_HMI_DESIGNER_HOME', $DesignerHome, 'User')

Write-Host ''
Write-Host 'Launcher experimental de Arduino IDE instalado.' -ForegroundColor Green
Write-Host "  VSIX: $destination"
Write-Host "  Designer: $DesignerHome"
Write-Host ''
Write-Host 'Cierra todas las ventanas de Arduino IDE y vuelve a abrirlo.' -ForegroundColor Yellow
Write-Host 'Gate esperado: comando "JWPLC: Abrir HMI Designer" + botón "JW HMI" en barra de estado.'
Write-Host 'El botón adicional en el título del editor es best-effort y puede variar por versión de Arduino IDE.'
