param(
    [string]$ArduinoCli = 'C:\Program Files\Arduino PLC IDE Tools\arduino-cli.exe'
)
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$ScriptPath = Join-Path $PSScriptRoot 'a13_tft_pre6_p2a_adopt_product.py'
$cmd = Get-Command python.exe -ErrorAction SilentlyContinue
if ($null -eq $cmd) { $cmd = Get-Command python -ErrorAction SilentlyContinue }
if ($null -eq $cmd) {
    Write-Host 'A13_TFT_PRE6_P2A_PYTHON_MISSING=YES'
    exit 2
}
if (-not (Test-Path -LiteralPath $ScriptPath -PathType Leaf)) {
    Write-Host 'A13_TFT_PRE6_P2A_SCRIPT_MISSING=YES'
    exit 2
}
Write-Host "A13_TFT_PRE6_P2A_PYTHON=$($cmd.Source)"
& $cmd.Source -B $ScriptPath --arduino-cli $ArduinoCli
exit [int]$LASTEXITCODE
