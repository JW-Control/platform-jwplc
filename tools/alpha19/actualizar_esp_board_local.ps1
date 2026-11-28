# Arma el package «ESP Board» (jwesp) desde esta rama y lo instala en Arduino IDE
# (%LOCALAPPDATA%\Arduino15\packages\jwesp_local\hardware\esp32\<versión>).
# El JWPLC Basic no lo necesita: jwplc_local es una junction a JWPLC\2.1.0 y se actualiza solo.
#
# Uso, desde la raíz del repo y con Arduino IDE cerrado:
#   powershell -ExecutionPolicy Bypass -File tools\alpha19\actualizar_esp_board_local.ps1

$ErrorActionPreference = "Stop"
$repo = Resolve-Path (Join-Path $PSScriptRoot "..\..")

# Python: el del PATH si responde; si no, el instalado por el usuario (python.org).
$python = $null
foreach ($c in @("python", "py")) {
    $cmd = Get-Command $c -ErrorAction SilentlyContinue
    if ($cmd -and $cmd.Source -notlike "*WindowsApps*") { $python = $cmd.Source; break }
}
if (-not $python) {
    $python = Get-ChildItem "$env:LOCALAPPDATA\Programs\Python\Python3*\python.exe" -ErrorAction SilentlyContinue |
        Sort-Object FullName -Descending | Select-Object -First 1 -ExpandProperty FullName
}
if (-not $python) { throw "No se encontró Python 3. Instálalo desde python.org." }

Push-Location $repo
try {
    & $python "JWPLC\tools\jwplc_esp32_generico.py" --local
    if ($LASTEXITCODE -ne 0) { throw "El script terminó con error ($LASTEXITCODE)" }
} finally {
    Pop-Location
}
