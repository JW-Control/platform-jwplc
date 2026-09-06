param(
    [switch]$NoStartMenu,
    [switch]$NoDesktop,
    [switch]$InstallArduinoIDELauncher,
    [string]$InstallRoot = (Join-Path $env:LOCALAPPDATA 'JWPLC\HMI Designer')
)

$ErrorActionPreference = 'Stop'

$sourcePoc = Join-Path $PSScriptRoot 'poc'
$sourceStart = Join-Path $PSScriptRoot 'Start-JWPLC-HMI-Designer.ps1'
$sourceServer = Join-Path $PSScriptRoot 'JWPLC-HMI-Server.ps1'
$sourceCmd = Join-Path $PSScriptRoot 'JWPLC-HMI-Designer.cmd'
$sourceBuildExe = Join-Path $PSScriptRoot 'Build-JWPLC-HMI-Designer-Exe.ps1'
$sourceLauncherCs = Join-Path $PSScriptRoot 'JWPLC-HMI-Designer-Launcher.cs'
$arduinoInstaller = Join-Path $PSScriptRoot 'Install-ArduinoIDE-Launcher.ps1'

foreach ($required in @($sourcePoc, $sourceStart, $sourceServer, $sourceCmd, $sourceBuildExe, $sourceLauncherCs)) {
    if (-not (Test-Path -LiteralPath $required)) {
        throw "Falta un archivo requerido del Designer: $required"
    }
}

$InstallRoot = [IO.Path]::GetFullPath($InstallRoot)
$installPoc = Join-Path $InstallRoot 'poc'
$installStart = Join-Path $InstallRoot 'Start-JWPLC-HMI-Designer.ps1'
$installServer = Join-Path $InstallRoot 'JWPLC-HMI-Server.ps1'
$installCmd = Join-Path $InstallRoot 'JWPLC-HMI-Designer.cmd'
$installExe = Join-Path $InstallRoot 'JWPLC-HMI-Designer.exe'

Write-Host 'Instalando JWPLC HMI Designer...' -ForegroundColor Cyan
Write-Host "  Destino: $InstallRoot"

New-Item -ItemType Directory -Force -Path $InstallRoot | Out-Null
if (Test-Path -LiteralPath $installPoc) {
    Remove-Item -LiteralPath $installPoc -Recurse -Force
}
Copy-Item -LiteralPath $sourcePoc -Destination $installPoc -Recurse -Force
Copy-Item -LiteralPath $sourceStart -Destination $installStart -Force
Copy-Item -LiteralPath $sourceServer -Destination $installServer -Force
Copy-Item -LiteralPath $sourceCmd -Destination $installCmd -Force

Write-Host 'Generando ejecutable de entrada...' -ForegroundColor Cyan
& $sourceBuildExe -OutputPath $installExe | Out-Null
if (-not (Test-Path -LiteralPath $installExe -PathType Leaf)) {
    throw "No se pudo instalar el ejecutable del Designer: $installExe"
}

# El launcher experimental del Arduino IDE usa esta variable cuando está
# disponible. También existe fallback fijo a %LOCALAPPDATA%\JWPLC\HMI Designer.
[Environment]::SetEnvironmentVariable('JWPLC_HMI_DESIGNER_HOME', $InstallRoot, 'User')

function New-JwplcShortcut([string]$Path) {
    $shell = New-Object -ComObject WScript.Shell
    $shortcut = $shell.CreateShortcut($Path)
    $shortcut.TargetPath = $installExe
    $shortcut.WorkingDirectory = $InstallRoot
    $shortcut.Description = 'JWPLC HMI Designer'
    $shortcut.IconLocation = "$installExe,0"
    $shortcut.Save()
}

$created = @()
if (-not $NoDesktop) {
    $desktop = [Environment]::GetFolderPath('Desktop')
    if ($desktop) {
        $path = Join-Path $desktop 'JWPLC HMI Designer.lnk'
        New-JwplcShortcut $path
        $created += $path
    }
}

if (-not $NoStartMenu) {
    $startMenu = [Environment]::GetFolderPath('Programs')
    if ($startMenu) {
        $folder = Join-Path $startMenu 'JWPLC'
        New-Item -ItemType Directory -Force -Path $folder | Out-Null
        $path = Join-Path $folder 'JWPLC HMI Designer.lnk'
        New-JwplcShortcut $path
        $created += $path
    }
}

if ($InstallArduinoIDELauncher) {
    if (-not (Test-Path -LiteralPath $arduinoInstaller -PathType Leaf)) {
        throw "No se encontró el instalador del launcher Arduino IDE: $arduinoInstaller"
    }
    & $arduinoInstaller -DesignerHome $InstallRoot
}

Write-Host ''
Write-Host 'JWPLC HMI Designer instalado.' -ForegroundColor Green
Write-Host "  Aplicación: $installExe"
Write-Host "  Recursos: $InstallRoot"
$created | ForEach-Object { Write-Host "  Acceso: $_" }
Write-Host 'El usuario ya no depende de la carpeta del repositorio para ejecutar el Designer.'
if (-not $InstallArduinoIDELauncher) {
    Write-Host 'Launcher Arduino IDE no instalado (opcional: -InstallArduinoIDELauncher).'
}
