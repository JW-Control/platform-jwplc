# Arma el package «ESP Board» (jwesp) desde esta rama y lo instala en Arduino IDE
# (%LOCALAPPDATA%\Arduino15\packages\jwesp_local\hardware\esp32\<versión>).
# El JWPLC Basic no lo necesita: jwplc_local es una junction a JWPLC\2.1.0 y se actualiza solo.
#
# Uso, desde la raíz del repo y con Arduino IDE cerrado:
#   powershell -ExecutionPolicy Bypass -File tools\alpha19\actualizar_esp_board_local.ps1

$ErrorActionPreference = "Stop"
$repo = Resolve-Path (Join-Path $PSScriptRoot "..\..")

# Python 3: primero el instalado por el usuario (python.org) y luego el del PATH. Cada candidato se prueba
# ejecutándolo, porque en Windows «python» y «py» pueden ser accesos directos que no ejecutan nada.
function Probar-Python([string]$exe) {
    if (-not $exe) { return $false }
    try {
        $v = & $exe -c "import sys; print(sys.version_info[0])" 2>$null
        return ($LASTEXITCODE -eq 0 -and "$v".Trim() -eq "3")
    } catch { return $false }
}
$candidatos = @()
$candidatos += Get-ChildItem "$env:LOCALAPPDATA\Programs\Python\Python3*\python.exe" -ErrorAction SilentlyContinue |
    Sort-Object FullName -Descending | Select-Object -ExpandProperty FullName
foreach ($c in @("python", "py")) {
    Get-Command $c -CommandType Application -ErrorAction SilentlyContinue |
        Where-Object { $_.Source -like "*.exe" -and $_.Source -notlike "*WindowsApps*" } |
        ForEach-Object { $candidatos += $_.Source }
}
$python = $candidatos | Where-Object { Probar-Python $_ } | Select-Object -First 1
if (-not $python) { throw "No se encontro Python 3. Instalalo desde python.org." }
Write-Host "Python: $python"

$env:PYTHONIOENCODING = "utf-8"
Push-Location $repo
try {
    & $python "JWPLC\tools\jwplc_esp32_generico.py" --local
    if ($LASTEXITCODE -ne 0) { throw "El script termino con error ($LASTEXITCODE)" }
} finally {
    Pop-Location
}
