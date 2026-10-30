param(
    [switch]$SkipDependencies,
    [switch]$NoStartMenu,
    [switch]$NoDesktop,
    [switch]$NoArduinoIDEExtension,
    [string]$InstallRoot = (Join-Path $env:LOCALAPPDATA 'JWPLC\HMI Designer')
)

$ErrorActionPreference = 'Stop'

$scriptDir = $PSScriptRoot
$root = Split-Path $scriptDir -Parent
. (Join-Path $scriptDir 'JWPLC-ElectronBuilderCache.ps1')

$electronRoot = Join-Path $root 'electron'
$packageJson = Join-Path $electronRoot 'package.json'
$packageLock = Join-Path $electronRoot 'package-lock.json'
$sourceIcon = Join-Path $root 'assets\JWPLC-HMI-Designer.ico'
$electronBuild = Join-Path $electronRoot 'build'
$electronIcon = Join-Path $electronBuild 'icon.ico'
$releaseRoot = Join-Path $electronRoot 'release'
$distRoot = Join-Path $root 'dist'
$targetExe = Join-Path $distRoot 'JWPLC-HMI-Designer-Electron.exe'
$sourcePoc = Join-Path $root 'poc'
$launcherInstaller = Join-Path $scriptDir 'Install-ArduinoIDE-Launcher.ps1'

if (-not (Test-Path -LiteralPath $packageJson -PathType Leaf)) {
    throw "No se encontró electron\package.json."
}
if (-not (Test-Path -LiteralPath $packageLock -PathType Leaf)) {
    throw "No se encontró electron\package-lock.json. El build reproducible requiere el lockfile."
}
if (-not (Test-Path -LiteralPath $sourceIcon -PathType Leaf)) {
    throw "No se encontró el icono del Designer: $sourceIcon"
}
if (-not (Test-Path -LiteralPath $sourcePoc -PathType Container)) {
    throw "No se encontró la carpeta poc del Designer: $sourcePoc"
}

$npm = Get-Command npm.cmd -ErrorAction SilentlyContinue
if (-not $npm) {
    throw 'No se encontró npm.cmd. Instala Node.js LTS para compilar la app.'
}

# --- FASE 1: Compilar la app de escritorio (Electron) ---
Write-Host '================================================' -ForegroundColor DarkCyan
Write-Host ' JWPLC HMI Designer - Generar e instalar' -ForegroundColor Cyan
Write-Host '================================================' -ForegroundColor DarkCyan
Write-Host ''
Write-Host '[1/2] Compilando la app de escritorio (Electron)...' -ForegroundColor Cyan

New-Item -ItemType Directory -Force -Path $electronBuild | Out-Null
New-Item -ItemType Directory -Force -Path $distRoot | Out-Null
Copy-Item -LiteralPath $sourceIcon -Destination $electronIcon -Force

Push-Location $electronRoot
try {
    if (-not $SkipDependencies) {
        Write-Host '  Instalando dependencias desde package-lock.json...' -ForegroundColor DarkGray
        & $npm.Source ci --no-audit --no-fund
        if ($LASTEXITCODE -ne 0) {
            throw "npm ci falló con código $LASTEXITCODE."
        }
    }

    # Caché winCodeSign para evitar fallo con enlaces simbólicos en Windows
    Initialize-JwplcWinCodeSignCache -ElectronRoot $electronRoot

    Write-Host '  Generando ejecutable portable nativo...' -ForegroundColor DarkGray
    & $npm.Source run dist:portable
    if ($LASTEXITCODE -ne 0) {
        throw "electron-builder falló con código $LASTEXITCODE."
    }
}
finally {
    Pop-Location
}

$artifact = Get-ChildItem -LiteralPath $releaseRoot -Filter 'JWPLC-HMI-Designer-Portable-*.exe' -File |
    Sort-Object LastWriteTime -Descending |
    Select-Object -First 1

if (-not $artifact) {
    throw "No se encontró el ejecutable portable generado en $releaseRoot."
}

Copy-Item -LiteralPath $artifact.FullName -Destination $targetExe -Force
Write-Host '  Compilación de Electron completada con éxito.' -ForegroundColor Green
Write-Host ''

# --- FASE 2: Instalar en el sistema ---
Write-Host '[2/2] Instalando en el sistema...' -ForegroundColor Cyan
$InstallRoot = [IO.Path]::GetFullPath($InstallRoot)
$installPoc = Join-Path $InstallRoot 'poc'
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
        Write-Host '  JWPLC HMI Designer está abierto; cerrando la instancia instalada para actualizarla...' -ForegroundColor Yellow
        $matches | Stop-Process -Force -ErrorAction SilentlyContinue
    }

    $deadline = [DateTime]::UtcNow.AddSeconds(5)
    while ([DateTime]::UtcNow -lt $deadline) {
        if ((@(Get-InstalledDesignerProcesses)).Count -eq 0 -and (Test-InstallExeUnlocked)) {
            return
        }
        Start-Sleep -Milliseconds 150
    }

    Get-Process -Name 'JWPLC-HMI-Designer' -ErrorAction SilentlyContinue |
        Stop-Process -Force -ErrorAction SilentlyContinue

    Start-Sleep -Milliseconds 300
    if (-not (Test-InstallExeUnlocked)) {
        throw 'No se pudo cerrar JWPLC HMI Designer. Cierra la aplicación manualmente y vuelve a ejecutar el instalador.'
    }
}

Stop-InstalledDesigner

New-Item -ItemType Directory -Force -Path $InstallRoot | Out-Null
if (Test-Path -LiteralPath $installPoc) {
    Remove-Item -LiteralPath $installPoc -Recurse -Force
}
Copy-Item -LiteralPath $sourcePoc -Destination $installPoc -Recurse -Force

if (Test-Path -LiteralPath $sourceIcon -PathType Leaf) {
    Copy-Item -LiteralPath $sourceIcon -Destination $installIcon -Force
    Copy-Item -LiteralPath $sourceIcon -Destination $installWebIcon -Force
}

Copy-Item -LiteralPath $targetExe -Destination $installExe -Force

[Environment]::SetEnvironmentVariable('JWPLC_HMI_DESIGNER_HOME', $InstallRoot, 'User')

# Protocolo jwplc-hmi://
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
if (-not $NoArduinoIDEExtension) {
    if (Test-Path -LiteralPath $launcherInstaller -PathType Leaf) {
        Write-Host '  Instalando extensión JWPLC HMI para Arduino IDE 2...' -ForegroundColor Cyan
        $extensionOutput = @(& $launcherInstaller -DesignerHome $InstallRoot)
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
    }
}

Write-Host ''
Write-Host '================================================' -ForegroundColor DarkCyan
Write-Host ' JWPLC HMI Designer instalado con éxito.' -ForegroundColor Green
Write-Host '================================================' -ForegroundColor DarkCyan
Write-Host "  Aplicación: $installExe"
Write-Host "  Recursos:   $InstallRoot"
Write-Host "  Protocolo:  jwplc-hmi://open"
$created | ForEach-Object { Write-Host "  Acceso:     $_" }
if ($arduinoExtensionPath) {
    Write-Host "  Extensión Arduino IDE: $arduinoExtensionPath"
}
