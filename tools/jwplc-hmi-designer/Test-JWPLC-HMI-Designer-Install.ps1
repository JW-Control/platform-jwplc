param(
    [string]$InstallRoot = (Join-Path $env:LOCALAPPDATA 'JWPLC\HMI Designer'),
    [switch]$SkipShortcuts
)

$ErrorActionPreference = 'Stop'

$root = $PSScriptRoot
$sourceExe = Join-Path $root 'dist\JWPLC-HMI-Designer-Electron.exe'
$installedExe = Join-Path $InstallRoot 'JWPLC-HMI-Designer.exe'
$extensionPackage = Join-Path $root 'arduino-ide-launcher\package.json'
$pluginsRoot = Join-Path $HOME '.arduinoIDE\plugins'

function Write-Gate([string]$Name, [bool]$Pass, [string]$Detail = '') {
    $state = if ($Pass) { 'PASS' } else { 'FAIL' }
    $color = if ($Pass) { 'Green' } else { 'Red' }
    if ([string]::IsNullOrWhiteSpace($Detail)) {
        Write-Host ("{0}={1}" -f $Name, $state) -ForegroundColor $color
    }
    else {
        Write-Host ("{0}={1} | {2}" -f $Name, $state, $Detail) -ForegroundColor $color
    }
    return $Pass
}

$results = New-Object System.Collections.Generic.List[bool]

Write-Host '================================================' -ForegroundColor DarkCyan
Write-Host ' ALPHA11 - GATE INSTALADOR COMBINADO HMI' -ForegroundColor Cyan
Write-Host '================================================' -ForegroundColor DarkCyan
Write-Host ''
Write-Host "InstallRoot: $InstallRoot"
Write-Host "Plugins:     $pluginsRoot"
Write-Host ''

$appExists = Test-Path -LiteralPath $installedExe -PathType Leaf
$results.Add((Write-Gate 'HMI_APP_INSTALLED' $appExists $installedExe))

if ($appExists -and (Test-Path -LiteralPath $sourceExe -PathType Leaf)) {
    $sourceHash = (Get-FileHash -LiteralPath $sourceExe -Algorithm SHA256).Hash.ToLowerInvariant()
    $installedHash = (Get-FileHash -LiteralPath $installedExe -Algorithm SHA256).Hash.ToLowerInvariant()
    $results.Add((Write-Gate 'HMI_APP_SOURCE_PARITY' ($sourceHash -eq $installedHash) "source=$sourceHash installed=$installedHash"))
}
else {
    $results.Add((Write-Gate 'HMI_APP_SOURCE_PARITY' $false 'source o instalación no disponible'))
}

$expectedHome = [IO.Path]::GetFullPath($InstallRoot).TrimEnd('\')
$userHome = [Environment]::GetEnvironmentVariable('JWPLC_HMI_DESIGNER_HOME', 'User')
$homePass = -not [string]::IsNullOrWhiteSpace($userHome)
if ($homePass) {
    try {
        $homePass = [string]::Equals(
            [IO.Path]::GetFullPath($userHome).TrimEnd('\'),
            $expectedHome,
            [StringComparison]::OrdinalIgnoreCase)
    }
    catch {
        $homePass = $false
    }
}
$results.Add((Write-Gate 'HMI_ENV_HOME' $homePass "actual=$userHome"))

$protocolCommand = ''
try {
    $protocolCommand = (Get-ItemPropertyValue -Path 'HKCU:\Software\Classes\jwplc-hmi\shell\open\command' -Name '(default)' -ErrorAction Stop)
}
catch {
    try {
        $protocolCommand = (Get-Item -Path 'HKCU:\Software\Classes\jwplc-hmi\shell\open\command' -ErrorAction Stop).GetValue('')
    }
    catch {
        $protocolCommand = ''
    }
}
$protocolPass = -not [string]::IsNullOrWhiteSpace($protocolCommand) -and
    $protocolCommand.IndexOf($installedExe, [StringComparison]::OrdinalIgnoreCase) -ge 0
$results.Add((Write-Gate 'HMI_PROTOCOL_REGISTERED' $protocolPass "command=$protocolCommand"))

if (-not (Test-Path -LiteralPath $extensionPackage -PathType Leaf)) {
    throw "No se encontró package.json de la extensión: $extensionPackage"
}
$pkg = Get-Content -LiteralPath $extensionPackage -Raw | ConvertFrom-Json
$expectedVsix = ('{0}-{1}.vsix' -f [string]$pkg.name, [string]$pkg.version)
$installedVsix = Join-Path $pluginsRoot $expectedVsix
$vsixPass = Test-Path -LiteralPath $installedVsix -PathType Leaf
$results.Add((Write-Gate 'HMI_EXTENSION_INSTALLED' $vsixPass $installedVsix))

$allVsix = @(
    Get-ChildItem -LiteralPath $pluginsRoot -Filter 'jwplc-hmi-launcher-*.vsix' -File -ErrorAction SilentlyContinue
)
$singleVsixPass = $allVsix.Count -eq 1 -and $vsixPass
$results.Add((Write-Gate 'HMI_EXTENSION_SINGLE_VERSION' $singleVsixPass ("count={0}" -f $allVsix.Count)))

if (-not $SkipShortcuts) {
    $desktop = [Environment]::GetFolderPath('Desktop')
    $desktopShortcut = if ($desktop) { Join-Path $desktop 'JWPLC HMI Designer.lnk' } else { '' }
    $desktopPass = -not [string]::IsNullOrWhiteSpace($desktopShortcut) -and (Test-Path -LiteralPath $desktopShortcut -PathType Leaf)
    $results.Add((Write-Gate 'HMI_DESKTOP_SHORTCUT' $desktopPass $desktopShortcut))

    $programs = [Environment]::GetFolderPath('Programs')
    $startShortcut = if ($programs) { Join-Path $programs 'JWPLC\JWPLC HMI Designer.lnk' } else { '' }
    $startPass = -not [string]::IsNullOrWhiteSpace($startShortcut) -and (Test-Path -LiteralPath $startShortcut -PathType Leaf)
    $results.Add((Write-Gate 'HMI_STARTMENU_SHORTCUT' $startPass $startShortcut))
}

$failed = @($results | Where-Object { -not $_ }).Count
Write-Host ''
Write-Host '=== RESULTADO ===' -ForegroundColor Cyan
Write-Host "TOTAL_GATES=$($results.Count)"
Write-Host "FAILED_GATES=$failed" -ForegroundColor $(if ($failed -eq 0) { 'Green' } else { 'Red' })

if ($failed -eq 0) {
    Write-Host 'ALPHA11_COMBINED_INSTALLER=PASS' -ForegroundColor Green
    exit 0
}

Write-Host 'ALPHA11_COMBINED_INSTALLER=FAIL' -ForegroundColor Red
exit 1
