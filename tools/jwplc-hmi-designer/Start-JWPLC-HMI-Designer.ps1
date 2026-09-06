param(
    [int]$Port = 8765
)

$ErrorActionPreference = 'Stop'
$designerRoot = Join-Path $PSScriptRoot 'poc'
$serverScript = Join-Path $PSScriptRoot 'JWPLC-HMI-Server.ps1'
$url = "http://127.0.0.1:$Port/desktop.html"
$healthUrl = "http://127.0.0.1:$Port/__health"

function Test-JwplcHmiServer {
    try {
        $response = Invoke-WebRequest -UseBasicParsing -Uri $healthUrl -TimeoutSec 1
        return $response.StatusCode -eq 200 -and $response.Content -eq 'JWPLC_HMI_OK'
    } catch {
        return $false
    }
}

function Find-ChromiumBrowser {
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

if (-not (Test-Path -LiteralPath $designerRoot -PathType Container)) {
    throw "No se encontró la carpeta del Designer: $designerRoot"
}
if (-not (Test-Path -LiteralPath $serverScript -PathType Leaf)) {
    throw "No se encontró el servidor local del Designer: $serverScript"
}

if (-not (Test-JwplcHmiServer)) {
    $serverArgs = @(
        '-NoProfile',
        '-ExecutionPolicy', 'Bypass',
        '-WindowStyle', 'Hidden',
        '-File', "`"$serverScript`"",
        '-Root', "`"$designerRoot`"",
        '-Port', $Port,
        '-IdleTimeoutMinutes', 60
    )

    Start-Process -FilePath 'powershell.exe' -ArgumentList $serverArgs -WindowStyle Hidden | Out-Null

    $ready = $false
    for ($attempt = 0; $attempt -lt 30; $attempt++) {
        Start-Sleep -Milliseconds 100
        if (Test-JwplcHmiServer) {
            $ready = $true
            break
        }
    }
    if (-not $ready) {
        throw "No se pudo iniciar el servidor local de JWPLC HMI Designer en el puerto $Port."
    }
}

$browser = Find-ChromiumBrowser
if ($browser) {
    Start-Process -FilePath $browser -ArgumentList @("--app=$url", '--start-maximized') | Out-Null
} else {
    # Fallback. El Designer abre, aunque LIVE/Web Serial requiere un navegador
    # Chromium compatible (Edge o Chrome).
    Start-Process $url | Out-Null
}
