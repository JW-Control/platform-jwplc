<#
JWPLC Basic para mBlock 5: instala el JWPLC mBlock Bridge en esta máquina.

- Copia el bridge a %USERPROFILE%\.jwplc\mblock-bridge\app (fuera de AppData).
- Lo ejecuta con el Electron de mBlock (ELECTRON_RUN_AS_NODE=1): no hace falta
  instalar Node. Si no encuentra mBlock, usa node.exe del PATH.
- Arranque automático al iniciar sesión (acceso directo en Inicio) y a demanda
  con el protocolo jwplc-bridge:// (la extensión lo abre si el bridge no responde).
- Con -Extension también instala los bloques (sin pasar por el .mext). Cerrar
  mBlock antes: lee la lista de dispositivos al arrancar.
- Revisa Arduino CLI / Arduino IDE 2 y el package jwplc:esp32 (no los instala).

Uso:
  Install-JWPLC-mBlock.cmd                 instalar el bridge
  Install-JWPLC-mBlock.cmd -Extension      bridge y bloques
  Install-JWPLC-mBlock.cmd -NoAutostart    sin arranque al iniciar sesión
  Install-JWPLC-mBlock.cmd -Uninstall      quitar bridge, arranque y protocolo
#>
param(
    [switch]$Extension,
    [switch]$NoAutostart,
    [switch]$Uninstall
)

$ErrorActionPreference = 'Stop'

$WorkDir = Join-Path $env:USERPROFILE '.jwplc\mblock-bridge'
$AppDir = Join-Path $WorkDir 'app'
$Launcher = Join-Path $AppDir 'start-bridge.vbs'
$BridgeScript = Join-Path $AppDir 'jwplc-mblock-bridge.cjs'
$StartupLink = Join-Path ([Environment]::GetFolderPath('Startup')) 'JWPLC mBlock Bridge.lnk'
$ProtocolKey = 'HKCU:\Software\Classes\jwplc-bridge'
$HealthUrl = 'http://127.0.0.1:47820/health'

function Say([string]$Text) { Write-Host $Text }

function Stop-Bridge {
    Get-CimInstance Win32_Process | Where-Object { $_.CommandLine -and $_.CommandLine -match 'jwplc-mblock-bridge\.cjs' } |
        ForEach-Object { Stop-Process -Id $_.ProcessId -Force -ErrorAction SilentlyContinue; Say "  bridge detenido (PID $($_.ProcessId))" }
}

function Find-Runtime {
    $candidates = @()
    foreach ($root in 'HKCU:\Software\Microsoft\Windows\CurrentVersion\Uninstall', 'HKLM:\Software\Microsoft\Windows\CurrentVersion\Uninstall',
        'HKLM:\Software\WOW6432Node\Microsoft\Windows\CurrentVersion\Uninstall') {
        if (-not (Test-Path $root)) { continue }
        Get-ChildItem $root -ErrorAction SilentlyContinue | ForEach-Object {
            $p = Get-ItemProperty $_.PSPath -ErrorAction SilentlyContinue
            if ($p.DisplayName -match '^mBlock' -and $p.InstallLocation) { $candidates += (Join-Path $p.InstallLocation 'mBlock.exe') }
        }
    }
    $candidates += 'C:\Users\Public\Programs\mblock\mBlock.exe'
    $candidates += (Join-Path $env:ProgramFiles 'mblock\mBlock.exe')
    $candidates += (Join-Path $env:LOCALAPPDATA 'Programs\mblock\mBlock.exe')
    foreach ($c in $candidates) {
        if ($c -and (Test-Path $c)) { return @{ Exe = (Resolve-Path $c).Path; Electron = $true } }
    }
    $node = Get-Command node.exe -ErrorAction SilentlyContinue
    if ($node) { return @{ Exe = $node.Source; Electron = $false } }
    return $null
}

function Find-ArduinoCli {
    $candidates = @(
        $env:JWPLC_ARDUINO_CLI,
        'C:\Program Files\Arduino CLI\arduino-cli.exe',
        'C:\Program Files\Arduino IDE\resources\app\lib\backend\resources\arduino-cli.exe',
        (Join-Path $env:LOCALAPPDATA 'Programs\Arduino IDE\resources\app\lib\backend\resources\arduino-cli.exe')
    )
    foreach ($c in $candidates) { if ($c -and (Test-Path $c)) { return $c } }
    $cli = Get-Command arduino-cli.exe -ErrorAction SilentlyContinue
    if ($cli) { return $cli.Source }
    return $null
}

# Lanzador sin ventana: wscript ejecuta mBlock.exe (Electron como Node) oculto.
function Write-Launcher($Runtime) {
    $exe = $Runtime.Exe.Replace('"', '')
    $lines = @(
        "' JWPLC mBlock Bridge: arranca el bridge sin ventana (lo genera Install-JWPLC-mBlock.ps1).",
        'Set sh = CreateObject("WScript.Shell")'
    )
    if ($Runtime.Electron) { $lines += 'sh.Environment("Process")("ELECTRON_RUN_AS_NODE") = "1"' }
    $lines += ('sh.Run """' + $exe + '"" ""' + $BridgeScript + '""", 0, False')
    Set-Content -Path $Launcher -Value $lines -Encoding ASCII
}

function Test-Bridge([int]$Seconds) {
    $deadline = (Get-Date).AddSeconds($Seconds)
    while ((Get-Date) -lt $deadline) {
        try { return (Invoke-RestMethod -Uri $HealthUrl -TimeoutSec 2) } catch { Start-Sleep -Milliseconds 500 }
    }
    return $null
}

Say ''
Say 'JWPLC Basic para mBlock 5 - instalador del bridge'
Say '================================================='

if ($Uninstall) {
    Stop-Bridge
    Remove-Item $StartupLink -Force -ErrorAction SilentlyContinue
    Remove-Item $ProtocolKey -Recurse -Force -ErrorAction SilentlyContinue
    Remove-Item $AppDir -Recurse -Force -ErrorAction SilentlyContinue
    Say ('  quitados: arranque al iniciar sesión, protocolo jwplc-bridge:// y ' + $AppDir)
    Say ('  (se conservan los datos de trabajo en ' + $WorkDir + ')')
    exit 0
}

# 1. Entorno para ejecutar el bridge.
$runtime = Find-Runtime
if (-not $runtime) { throw 'No se encontró mBlock 5 ni node.exe. Instala mBlock 5 y vuelve a ejecutar el instalador.' }
Say ('  ejecutable: ' + $runtime.Exe + $(if ($runtime.Electron) { ' (Electron de mBlock como Node)' } else { '' }))

# 2. Copia del bridge (detiene uno anterior para reemplazarlo).
Stop-Bridge
New-Item -ItemType Directory -Force -Path $AppDir | Out-Null
Copy-Item (Join-Path $PSScriptRoot 'bridge\jwplc-mblock-bridge.cjs') $BridgeScript -Force
Write-Launcher $runtime
Say ('  bridge copiado en ' + $AppDir)

# 3. Configuración: package publicado y sketchbook propio vacío, para que las
#    librerías de Documentos\Arduino no tapen las del package.
$config = Join-Path $WorkDir 'bridge.json'
if (-not (Test-Path $config)) {
    $sketchbook = Join-Path $WorkDir 'sketchbook-empty'
    New-Item -ItemType Directory -Force -Path $sketchbook | Out-Null
    @{ fqbn = 'jwplc:esp32:jwplcbasic'; sketchbook = $sketchbook } | ConvertTo-Json | Set-Content -Path $config -Encoding UTF8
    Say ('  configuración: ' + $config)
} else {
    Say ('  configuración existente conservada: ' + $config)
}

# 4. Protocolo jwplc-bridge:// (arranque a demanda desde mBlock).
New-Item -Path "$ProtocolKey\shell\open\command" -Force | Out-Null
Set-Item -Path $ProtocolKey -Value 'URL:JWPLC mBlock Bridge'
New-ItemProperty -Path $ProtocolKey -Name 'URL Protocol' -Value '' -PropertyType String -Force | Out-Null
Set-Item -Path "$ProtocolKey\shell\open\command" -Value ('wscript.exe "' + $Launcher + '"')
Say '  protocolo jwplc-bridge:// registrado'

# 5. Arranque al iniciar sesión.
if ($NoAutostart) {
    Remove-Item $StartupLink -Force -ErrorAction SilentlyContinue
    Say '  sin arranque al iniciar sesión (arranca a demanda desde mBlock)'
} else {
    $shell = New-Object -ComObject WScript.Shell
    $lnk = $shell.CreateShortcut($StartupLink)
    $lnk.TargetPath = 'wscript.exe'
    $lnk.Arguments = '"' + $Launcher + '"'
    $lnk.WorkingDirectory = $AppDir
    $lnk.Description = 'JWPLC mBlock Bridge'
    $lnk.Save()
    Say ('  arranque al iniciar sesión: ' + $StartupLink)
}

# 6. Bloques de mBlock (opcional).
if ($Extension) {
    if (Get-Process mBlock -ErrorAction SilentlyContinue | Where-Object { $_.MainWindowHandle -ne 0 }) {
        Say '  AVISO: mBlock está abierto; ciérralo y vuelve a abrirlo para ver los bloques.'
    }
    $installExt = Join-Path $PSScriptRoot 'extension\install-extension.cjs'
    if ($runtime.Electron) { $env:ELECTRON_RUN_AS_NODE = '1' }
    & $runtime.Exe $installExt | ForEach-Object { Say ('  ' + $_) }
    Remove-Item Env:\ELECTRON_RUN_AS_NODE -ErrorAction SilentlyContinue
    if ($LASTEXITCODE -ne 0) { throw 'No se pudieron instalar los bloques en mBlock.' }
}

# 7. Arrancar y comprobar.
Start-Process wscript.exe -ArgumentList ('"' + $Launcher + '"')
$health = Test-Bridge 15
Say ''
if ($health) {
    Say ('  BRIDGE OK: versión ' + $health.version + ', ' + $health.fqbn)
} else {
    Say ('  ERROR: el bridge no respondió. Revisa ' + (Join-Path $WorkDir 'bridge.log'))
}

# 8. Revisión de requisitos (informativa).
Say ''
Say 'Requisitos para compilar y subir:'
$cli = Find-ArduinoCli
if ($cli) {
    Say ('  [OK] Arduino CLI: ' + $cli)
    $cores = & $cli core list 2>$null | Out-String
    if ($cores -match 'jwplc:esp32\s+(\S+)') {
        Say ('  [OK] package jwplc:esp32 ' + $Matches[1])
    } else {
        Say '  [FALTA] package jwplc:esp32: instálalo desde el Gestor de placas de Arduino IDE 2 (índice JWPLC).'
    }
} else {
    Say '  [FALTA] Arduino IDE 2 o Arduino CLI: instala Arduino IDE 2 y el package jwplc:esp32.'
}
if (Get-CimInstance Win32_PnPEntity | Where-Object { $_.Name -match 'CH340' }) {
    Say '  [OK] adaptador CH340 conectado'
} else {
    Say '  [?] no hay ninguna JWPLC (CH340) conectada ahora; mBlock puede instalar el driver.'
}
Say ''
if ($Extension) {
    Say 'Listo. Abre mBlock 5 y añade el dispositivo "JWPLC Basic".'
} else {
    Say 'Listo. En mBlock 5, arrastra el archivo .mext a la ventana para añadir los bloques.'
}
