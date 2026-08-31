param(
    [Parameter(Mandatory = $true)]
    [string]$OpenPLCRepo
)

$ErrorActionPreference = "Stop"

$baseScript = Join-Path $PSScriptRoot "Apply-OpenPLC-VppDefaultPinMapping.ps1"
if (-not (Test-Path -LiteralPath $baseScript)) {
    throw "No existe aplicador base: $baseScript"
}

$text = [System.IO.File]::ReadAllText($baseScript)

$replaceOnceBlock = @'
function Replace-Once([string]$Text, [string]$Old, [string]$New, [string]$Label) {
    $count = ([regex]::Matches($Text, [regex]::Escape($Old))).Count
    if ($count -ne 1) {
        throw "No se encontro un ancla unica para $Label. Coincidencias: $count"
    }
    return $Text.Replace($Old, $New)
}
'@

$replacementBlock = @'
function Replace-Once([string]$Text, [string]$Old, [string]$New, [string]$Label) {
    $count = ([regex]::Matches($Text, [regex]::Escape($Old))).Count
    if ($count -ne 1) {
        throw "No se encontro un ancla unica para $Label. Coincidencias: $count"
    }
    return $Text.Replace($Old, $New)
}

function Replace-First([string]$Text, [string]$Old, [string]$New, [string]$Label) {
    $index = $Text.IndexOf($Old, [System.StringComparison]::Ordinal)
    if ($index -lt 0) {
        throw "No se encontro el ancla esperada para $Label."
    }
    return $Text.Substring(0, $index) + $New + $Text.Substring($index + $Old.Length)
}
'@

if (-not $text.Contains($replaceOnceBlock)) {
    throw "No se encontro el bloque Replace-Once esperado en el aplicador base."
}
$text = $text.Replace($replaceOnceBlock, $replacementBlock)

$oldCall = '$tail = Replace-Once $tail (Normalize-Lf $setBoardOld) (Normalize-Lf $setBoardNew) "setDeviceBoard draft"'
$newCall = '$tail = Replace-First $tail (Normalize-Lf $setBoardOld) (Normalize-Lf $setBoardNew) "setDeviceBoard draft"'
if (-not $text.Contains($oldCall)) {
    throw "No se encontro la llamada setDeviceBoard esperada en el aplicador base."
}
$text = $text.Replace($oldCall, $newCall)

$temp = Join-Path $env:TEMP "JWPLC_Apply-OpenPLC-VppDefaultPinMapping-v2.generated.ps1"
$utf8NoBom = New-Object System.Text.UTF8Encoding($false)
[System.IO.File]::WriteAllText($temp, $text, $utf8NoBom)

Write-Host "============================================================" -ForegroundColor Cyan
Write-Host " ALPHA7 - VPP DEFAULT PIN MAPPING V2" -ForegroundColor Cyan
Write-Host "============================================================" -ForegroundColor Cyan
Write-Host "Fix activo: setDeviceBoard usa la primera ancla dentro de su propio bloque."
Write-Host ""

try {
    & powershell -NoProfile -ExecutionPolicy Bypass -File $temp -OpenPLCRepo $OpenPLCRepo
    $exitCode = $LASTEXITCODE
}
finally {
    Remove-Item -LiteralPath $temp -Force -ErrorAction SilentlyContinue
}

if ($exitCode -ne 0) {
    throw "El aplicador V2 termino con codigo $exitCode."
}
