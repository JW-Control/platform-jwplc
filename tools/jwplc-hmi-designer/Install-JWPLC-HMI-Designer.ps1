param(
    [switch]$NoStartMenu,
    [switch]$NoDesktop
)

$ErrorActionPreference = 'Stop'
$launcher = Join-Path $PSScriptRoot 'Start-JWPLC-HMI-Designer.ps1'
if (-not (Test-Path -LiteralPath $launcher -PathType Leaf)) {
    throw "No se encontró el launcher: $launcher"
}

function Find-BrowserIcon {
    $candidates = @(
        "${env:ProgramFiles(x86)}\Microsoft\Edge\Application\msedge.exe",
        "$env:ProgramFiles\Microsoft\Edge\Application\msedge.exe",
        "$env:LOCALAPPDATA\Microsoft\Edge\Application\msedge.exe",
        "$env:ProgramFiles\Google\Chrome\Application\chrome.exe",
        "${env:ProgramFiles(x86)}\Google\Chrome\Application\chrome.exe",
        "$env:LOCALAPPDATA\Google\Chrome\Application\chrome.exe"
    ) | Where-Object { $_ -and (Test-Path -LiteralPath $_ -PathType Leaf) }
    return $candidates | Select-Object -First 1
}

function New-JwplcShortcut([string]$Path) {
    $shell = New-Object -ComObject WScript.Shell
    $shortcut = $shell.CreateShortcut($Path)
    $shortcut.TargetPath = 'powershell.exe'
    $shortcut.Arguments = "-NoProfile -ExecutionPolicy Bypass -WindowStyle Hidden -File `"$launcher`""
    $shortcut.WorkingDirectory = $PSScriptRoot
    $shortcut.Description = 'JWPLC HMI Designer'
    $browserIcon = Find-BrowserIcon
    if ($browserIcon) { $shortcut.IconLocation = "$browserIcon,0" }
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

Write-Host 'JWPLC HMI Designer instalado.' -ForegroundColor Green
$created | ForEach-Object { Write-Host "  $_" }
Write-Host 'El launcher inicia su servidor localhost automáticamente y abre Edge/Chrome en modo aplicación.'
