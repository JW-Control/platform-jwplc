[CmdletBinding()]
param(
    [string]$ArduinoCli = "arduino-cli"
)

Set-StrictMode -Version 2.0
$ErrorActionPreference = "Stop"

$ScriptRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$Replacement = Join-Path $ScriptRoot "Verify-JWPLCUnifiedEthernetSelection.ps1"

Write-Warning "Verify-JWPLCBundledEthernetSelection.ps1 es un alias legacy. Alpha6 consolidó el backend W5x00 dentro de JWPLC_Ethernet."

if (-not (Test-Path -LiteralPath $Replacement))
{
    throw "No existe el verificador unificado: $Replacement"
}

& $Replacement -ArduinoCli $ArduinoCli
