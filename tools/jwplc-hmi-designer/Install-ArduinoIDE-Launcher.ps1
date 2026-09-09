param(
    [string]$DesignerHome = (Join-Path $env:LOCALAPPDATA 'JWPLC\HMI Designer'),
    [switch]$SkipBuild
)

$ErrorActionPreference = 'Stop'
$buildScript = Join-Path $PSScriptRoot 'Build-ArduinoIDE-Launcher.ps1'
$dist = Join-Path $PSScriptRoot 'dist'
$plugins = Join-Path $HOME '.arduinoIDE\plugins'

# El VSIX aporta dos capacidades independientes:
#   1) autocompletado contextual de JWPLC_Display;
#   2) launcher opcional del JWPLC HMI Designer.
#
# La primera no debe depender de que la aplicación Designer ya esté instalada.
# Se intenta resolver primero la ruta solicitada y luego una ruta previamente
# registrada en la variable de usuario JWPLC_HMI_DESIGNER_HOME. Si ninguna
# contiene el EXE, el VSIX se instala igualmente y sólo se advierte al usuario.
$resolvedDesignerHome = [IO.Path]::GetFullPath($DesignerHome)
$designerExe = Join-Path $resolvedDesignerHome 'JWPLC-HMI-Designer.exe'
$designerAvailable = Test-Path -LiteralPath $designerExe -PathType Leaf

if (-not $designerAvailable) {
    $configuredDesignerHome = [Environment]::GetEnvironmentVariable(
        'JWPLC_HMI_DESIGNER_HOME',
        'User'
    )

    if (-not [string]::IsNullOrWhiteSpace($configuredDesignerHome)) {
        try {
            $configuredDesignerHome = [IO.Path]::GetFullPath($configuredDesignerHome)
            $configuredDesignerExe = Join-Path $configuredDesignerHome 'JWPLC-HMI-Designer.exe'

            if (Test-Path -LiteralPath $configuredDesignerExe -PathType Leaf) {
                $resolvedDesignerHome = $configuredDesignerHome
                $designerExe = $configuredDesignerExe
                $designerAvailable = $true
            }
        }
        catch {
            # Una variable antigua o inválida no debe bloquear la instalación
            # del autocompletado de Arduino IDE.
        }
    }
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
    throw 'No se pudo localizar el VSIX de la extensión JWPLC HMI.'
}

New-Item -ItemType Directory -Force -Path $plugins | Out-Null
$currentName = Split-Path -Leaf $vsix
Get-ChildItem -LiteralPath $plugins -Filter 'jwplc-hmi-launcher-*.vsix' -File -ErrorAction SilentlyContinue |
    Where-Object { $_.Name -ne $currentName } |
    Remove-Item -Force

$destination = Join-Path $plugins $currentName
Copy-Item -LiteralPath $vsix -Destination $destination -Force

if ($designerAvailable) {
    [Environment]::SetEnvironmentVariable(
        'JWPLC_HMI_DESIGNER_HOME',
        $resolvedDesignerHome,
        'User'
    )
}

Write-Host ''
Write-Host 'Extensión JWPLC HMI para Arduino IDE 2 instalada.' -ForegroundColor Green
Write-Host "  VSIX: $destination"

if ($designerAvailable) {
    Write-Host "  Designer: $resolvedDesignerHome"
}
else {
    Write-Host '  Designer: no detectado; el autocompletado queda disponible igualmente.' -ForegroundColor Yellow
    Write-Host '  El botón JW HMI requerirá instalar JWPLC HMI Designer para poder abrir la aplicación.' -ForegroundColor Yellow
}

Write-Host ''
Write-Host 'Cierra todas las ventanas de Arduino IDE y vuelve a abrirlo.' -ForegroundColor Yellow
Write-Host 'Gate esperado: autocompletado de JWPLC_Display. + comando "JWPLC: Abrir HMI Designer" + botón "JW HMI".'
Write-Host 'El botón adicional en el título del editor es best-effort y puede variar por versión de Arduino IDE.'

# Permite al instalador combinado recuperar de forma estable la ruta instalada
# sin tener que parsear los mensajes de consola.
Write-Output $destination
