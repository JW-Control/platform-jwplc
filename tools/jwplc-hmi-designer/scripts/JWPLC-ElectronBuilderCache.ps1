# JWPLC HMI Designer - caché de electron-builder en Windows (dot-source).
#
# electron-builder descarga winCodeSign (rcedit: icono y versión del .exe) y lo
# extrae con 7-Zip. El .7z trae dos enlaces simbólicos de macOS
# (darwin\10.12\lib\libcrypto.dylib y libssl.dylib). Sin modo desarrollador ni
# administrador, Windows no deja crear enlaces simbólicos ("El cliente no
# dispone de un privilegio requerido"): 7-Zip termina con código 2 y
# electron-builder aborta tras cuatro intentos.
#
# Initialize-JwplcWinCodeSignCache lo extrae una vez en la caché de
# electron-builder omitiendo darwin\ (en Windows no se usa). Si la carpeta
# winCodeSign-<versión> existe, electron-builder (app-builder download-artifact)
# la reutiliza y ya no descarga ni extrae nada.

function Initialize-JwplcWinCodeSignCache {
    param(
        [Parameter(Mandatory = $true)][string]$ElectronRoot,
        [string]$Version = '2.6.0'
    )

    $name = "winCodeSign-$Version"
    $cacheRoot = if ($env:ELECTRON_BUILDER_CACHE) { $env:ELECTRON_BUILDER_CACHE } else { Join-Path $env:LOCALAPPDATA 'electron-builder\Cache' }
    $parent = Join-Path $cacheRoot 'winCodeSign'
    $target = Join-Path $parent $name

    if (Test-Path -LiteralPath (Join-Path $target 'rcedit-x64.exe') -PathType Leaf) {
        Write-Host "Caché $name lista: $target"
        return
    }

    $sevenZip = Join-Path $ElectronRoot 'node_modules\7zip-bin\win\x64\7za.exe'
    if (-not (Test-Path -LiteralPath $sevenZip -PathType Leaf)) {
        Write-Warning "No se encontró $sevenZip; electron-builder extraerá $name por su cuenta."
        return
    }

    New-Item -ItemType Directory -Force -Path $parent | Out-Null
    $archive = Join-Path $parent "$name.7z"
    $staging = Join-Path $parent "$name.tmp"
    $url = "https://github.com/electron-userland/electron-builder-binaries/releases/download/$name/$name.7z"

    Write-Host "Preparando $name sin los enlaces simbólicos de macOS..." -ForegroundColor Cyan
    # Windows PowerShell 5.1 usa TLS 1.0 por defecto y GitHub exige TLS 1.2.
    [Net.ServicePointManager]::SecurityProtocol = [Net.ServicePointManager]::SecurityProtocol -bor [Net.SecurityProtocolType]::Tls12
    $previousProgress = $ProgressPreference
    $ProgressPreference = 'SilentlyContinue'
    try {
        Invoke-WebRequest -UseBasicParsing -Uri $url -OutFile $archive
    }
    finally {
        $ProgressPreference = $previousProgress
    }

    if (Test-Path -LiteralPath $staging) {
        Remove-Item -LiteralPath $staging -Recurse -Force
    }
    & $sevenZip x -bd -y "-o$staging" $archive '-xr!darwin' | Out-Null
    if ($LASTEXITCODE -ne 0) {
        throw "No se pudo extraer $name (7-Zip terminó con código $LASTEXITCODE)."
    }

    if (Test-Path -LiteralPath $target) {
        Remove-Item -LiteralPath $target -Recurse -Force
    }
    Move-Item -LiteralPath $staging -Destination $target
    Remove-Item -LiteralPath $archive -Force
    Write-Host "Caché $name lista: $target"
}
