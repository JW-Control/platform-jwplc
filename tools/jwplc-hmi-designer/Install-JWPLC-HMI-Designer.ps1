param(
    [switch]$NoStartMenu,
    [switch]$NoDesktop,
    [switch]$NoArduinoIDEExtension,
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
$sourceElectronExe = Join-Path $PSScriptRoot 'dist\JWPLC-HMI-Designer-Electron.exe'
$sourceIcon = Join-Path $PSScriptRoot 'assets\JWPLC-HMI-Designer.ico'
$arduinoInstaller = Join-Path $PSScriptRoot 'Install-ArduinoIDE-Launcher.ps1'

# Alpha11: la instalación normal incluye la extensión de Arduino IDE 2.
# -NoArduinoIDEExtension queda únicamente como opt-out técnico. El switch
# histórico -InstallArduinoIDELauncher se conserva por compatibilidad y fuerza
# explícitamente la instalación si algún script antiguo todavía lo utiliza.
$installArduinoIDEExtension = -not $NoArduinoIDEExtension
if ($InstallArduinoIDELauncher) {
    $installArduinoIDEExtension = $true
}

$requiredFiles = @(
    $sourcePoc,
    $sourceStart,
    $sourceServer,
    $sourceCmd,
    $sourceBuildExe,
    $sourceLauncherCs
)
if ($installArduinoIDEExtension) {
    $requiredFiles += $arduinoInstaller
}

foreach ($required in $requiredFiles) {
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
$installIcon = Join-Path $InstallRoot 'JWPLC-HMI-Designer.ico'
$installWebIcon = Join-Path $installPoc 'JWPLC-HMI-Designer.ico'

function Get-InstalledDesignerProcesses {
    $target = $installExe
    return @(
        Get-Process -ErrorAction SilentlyContinue | Where-Object {
            try {
                $_.Path -and [string]::Equals(
                    [IO.Path]::GetFullPath($_.Path),
                    $target,
                    [StringComparison]::OrdinalIgnoreCase)
            }
            catch {
                $false
            }
        }
    )
}

function Test-InstallExeUnlocked {
    if (-not (Test-Path -LiteralPath $installExe -PathType Leaf)) {
        return $true
    }

    try {
        $stream = [IO.File]::Open(
            $installExe,
            [IO.FileMode]::Open,
            [IO.FileAccess]::Read,
            [IO.FileShare]::None)
        $stream.Dispose()
        return $true
    }
    catch {
        return $false
    }
}

function Stop-InstalledDesigner {
    $matches = @(Get-InstalledDesignerProcesses)
    if ($matches.Count -gt 0) {
        Write-Host 'JWPLC HMI Designer esta abierto; cerrando la instancia instalada para poder actualizarla...' -ForegroundColor Yellow
        $matches | Stop-Process -Force -ErrorAction SilentlyContinue
    }

    $deadline = [DateTime]::UtcNow.AddSeconds(5)
    while ([DateTime]::UtcNow -lt $deadline) {
        if ((@(Get-InstalledDesignerProcesses)).Count -eq 0 -and (Test-InstallExeUnlocked)) {
            return
        }
        Start-Sleep -Milliseconds 150
    }

    # Fallback: Electron puede mantener varios procesos con el mismo nombre.
    # Durante una reinstalacion es preferible cerrarlos antes que dejar el EXE bloqueado.
    Get-Process -Name 'JWPLC-HMI-Designer' -ErrorAction SilentlyContinue |
        Stop-Process -Force -ErrorAction SilentlyContinue

    Start-Sleep -Milliseconds 300
    if (-not (Test-InstallExeUnlocked)) {
        throw 'No se pudo cerrar JWPLC HMI Designer. Cierra la aplicacion manualmente y vuelve a ejecutar el instalador.'
    }
}

Write-Host 'Instalando JWPLC HMI Designer...' -ForegroundColor Cyan
Write-Host "  Destino: $InstallRoot"
if ($installArduinoIDEExtension) {
    Write-Host '  Arduino IDE 2: extensión JWPLC HMI incluida' -ForegroundColor Cyan
}
else {
    Write-Host '  Arduino IDE 2: extensión omitida por -NoArduinoIDEExtension' -ForegroundColor Yellow
}

# El ejecutable Electron instalado no puede ser reemplazado mientras sigue abierto.
# El instalador lo cierra de forma controlada antes de copiar la nueva version.
Stop-InstalledDesigner

New-Item -ItemType Directory -Force -Path $InstallRoot | Out-Null
if (Test-Path -LiteralPath $installPoc) {
    Remove-Item -LiteralPath $installPoc -Recurse -Force
}
Copy-Item -LiteralPath $sourcePoc -Destination $installPoc -Recurse -Force
Copy-Item -LiteralPath $sourceStart -Destination $installStart -Force
Copy-Item -LiteralPath $sourceServer -Destination $installServer -Force
Copy-Item -LiteralPath $sourceCmd -Destination $installCmd -Force

$buildIcon = ''
if (Test-Path -LiteralPath $sourceIcon -PathType Leaf) {
    Copy-Item -LiteralPath $sourceIcon -Destination $installIcon -Force
    Copy-Item -LiteralPath $sourceIcon -Destination $installWebIcon -Force
    $buildIcon = $sourceIcon
    Write-Host "Icono: $sourceIcon" -ForegroundColor DarkGray
}

$installMode = 'CHROMIUM_FALLBACK'
if (Test-Path -LiteralPath $sourceElectronExe -PathType Leaf) {
    Write-Host 'Instalando ejecutable nativo Electron...' -ForegroundColor Cyan
    Copy-Item -LiteralPath $sourceElectronExe -Destination $installExe -Force
    $installMode = 'ELECTRON_NATIVE'
}
else {
    Write-Host 'Ejecutable Electron no encontrado; usando launcher Chromium de compatibilidad.' -ForegroundColor Yellow
    Write-Host 'Para el modo nativo ejecuta primero:' -ForegroundColor Yellow
    Write-Host '  .\tools\jwplc-hmi-designer\Build-JWPLC-HMI-Designer-Electron.ps1' -ForegroundColor Yellow
    Write-Host ''
    Write-Host 'Generando ejecutable de entrada...' -ForegroundColor Cyan
    if ([string]::IsNullOrWhiteSpace($buildIcon)) {
        & $sourceBuildExe -OutputPath $installExe | Out-Null
    }
    else {
        & $sourceBuildExe -OutputPath $installExe -IconPath $buildIcon | Out-Null
    }
}

if (-not (Test-Path -LiteralPath $installExe -PathType Leaf)) {
    throw "No se pudo instalar el ejecutable del Designer: $installExe"
}

[Environment]::SetEnvironmentVariable('JWPLC_HMI_DESIGNER_HOME', $InstallRoot, 'User')

# Protocolo local estable usado por la extensión de Arduino IDE.
$protocolRoot = 'HKCU:\Software\Classes\jwplc-hmi'
New-Item -Path $protocolRoot -Force | Out-Null
Set-Item -Path $protocolRoot -Value 'URL:JWPLC HMI Designer'
New-ItemProperty -Path $protocolRoot -Name 'URL Protocol' -Value '' -PropertyType String -Force | Out-Null
$commandKey = Join-Path $protocolRoot 'shell\open\command'
New-Item -Path $commandKey -Force | Out-Null
Set-Item -Path $commandKey -Value ('"' + $installExe + '" "%1"')

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

$arduinoExtensionPath = ''
if ($installArduinoIDEExtension) {
    Write-Host ''
    Write-Host 'Instalando extensión JWPLC HMI para Arduino IDE 2...' -ForegroundColor Cyan

    # Los errores del subinstalador son terminating errors porque ambos scripts
    # usan ErrorActionPreference=Stop. No se consulta LASTEXITCODE: una llamada
    # a otro .ps1 no tiene semántica fiable de exit code nativo.
    $extensionOutput = @(& $arduinoInstaller -DesignerHome $InstallRoot)
    $reportedVsix = @(
        $extensionOutput | Where-Object {
            $_ -is [string] -and
            $_ -like '*.vsix' -and
            (Test-Path -LiteralPath $_ -PathType Leaf)
        }
    ) | Select-Object -Last 1

    if ($reportedVsix) {
        $arduinoExtensionPath = [IO.Path]::GetFullPath([string]$reportedVsix)
    }
    else {
        $pluginsRoot = Join-Path $HOME '.arduinoIDE\plugins'
        $installedVsix = Get-ChildItem -LiteralPath $pluginsRoot -Filter 'jwplc-hmi-launcher-*.vsix' -File -ErrorAction SilentlyContinue |
            Sort-Object LastWriteTime -Descending |
            Select-Object -First 1
        if ($installedVsix) {
            $arduinoExtensionPath = $installedVsix.FullName
        }
    }

    if (-not $arduinoExtensionPath) {
        throw 'La extensión JWPLC HMI terminó sin reportar un VSIX instalado.'
    }
}

Write-Host ''
Write-Host 'JWPLC HMI Designer instalado.' -ForegroundColor Green
Write-Host "  Aplicación: $installExe"
Write-Host "  Modo: $installMode"
Write-Host "  Recursos: $InstallRoot"
if (Test-Path -LiteralPath $installIcon -PathType Leaf) {
    Write-Host "  Icono: $installIcon"
}
Write-Host '  Protocolo: jwplc-hmi://open'
$created | ForEach-Object { Write-Host "  Acceso: $_" }
if ($installArduinoIDEExtension) {
    Write-Host "  Extensión Arduino IDE: $arduinoExtensionPath"
    Write-Host '  Reinicia completamente Arduino IDE 2 para cargar la extensión.' -ForegroundColor Yellow
}
else {
    Write-Host '  Extensión Arduino IDE: no instalada (-NoArduinoIDEExtension).' -ForegroundColor Yellow
}
Write-Host 'El usuario ya no depende de la carpeta del repositorio para ejecutar el Designer.'
