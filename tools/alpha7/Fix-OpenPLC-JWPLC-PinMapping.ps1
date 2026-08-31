param(
    [string]$PlatformRepo = ""
)

$ErrorActionPreference = "Stop"

function Write-Section([string]$Text) {
    Write-Host ""
    Write-Host "============================================================" -ForegroundColor Cyan
    Write-Host " $Text" -ForegroundColor Cyan
    Write-Host "============================================================" -ForegroundColor Cyan
}

if ([string]::IsNullOrWhiteSpace($PlatformRepo)) {
    $PlatformRepo = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path
}
else {
    $PlatformRepo = (Resolve-Path $PlatformRepo).Path
}

$expectedBranch = "v2.1.0-alpha.7/feature/openplc-backplane-validation"
$branch = (& git -C $PlatformRepo branch --show-current).Trim()
if ($branch -ne $expectedBranch) {
    throw "Branch inesperado. Esperado: $expectedBranch ; actual: $branch"
}

$manifestRel = "openplc-editor-installers/v4.2.7/vpp/manifest.json"
$manifestPath = Join-Path $PlatformRepo $manifestRel
if (-not (Test-Path -LiteralPath $manifestPath)) {
    throw "No existe manifest esperado: $manifestPath"
}

Write-Section "ALPHA7 - RESTORE JWPLC PIN MAPPING"
Write-Host "Platform : $PlatformRepo"
Write-Host "Branch   : $branch"
Write-Host "Manifest : $manifestPath"

$text = [System.IO.File]::ReadAllText($manifestPath)

# Este fix se aplica sobre el manifest que ya contiene el selector Package Source.
if ($text -notmatch '"key"\s*:\s*"packageSource"') {
    throw "El manifest no contiene Package Source. Aplica primero el selector Alpha7."
}
if ($text -notmatch '"fqbn"\s*:\s*"jwplc_local:esp32:jwplcbasic"') {
    throw "El manifest no contiene el FQBN Local Development esperado."
}

$falseMatches = [regex]::Matches($text, '"pinMapping"\s*:\s*false')
$trueMatches  = [regex]::Matches($text, '"pinMapping"\s*:\s*true')

if ($falseMatches.Count -eq 1) {
    $text = [regex]::Replace(
        $text,
        '"pinMapping"\s*:\s*false',
        '"pinMapping": true',
        1
    )
}
elseif ($falseMatches.Count -eq 0 -and $trueMatches.Count -eq 1) {
    Write-Host "pinMapping ya estaba habilitado; no se vuelve a modificar." -ForegroundColor Yellow
}
else {
    throw "Cantidad inesperada de entradas pinMapping. false=$($falseMatches.Count) true=$($trueMatches.Count)"
}

# El artefacto alpha.12 ya fue generado con contenido distinto. Incrementar la
# version VPP evita dos paquetes diferentes con el mismo identificador/version.
$version12 = [regex]::Matches($text, '"version"\s*:\s*"2\.1\.0-alpha\.12"')
$version13 = [regex]::Matches($text, '"version"\s*:\s*"2\.1\.0-alpha\.13"')

if ($version12.Count -eq 1) {
    $text = [regex]::Replace(
        $text,
        '"version"\s*:\s*"2\.1\.0-alpha\.12"',
        '"version": "2.1.0-alpha.13"',
        1
    )
}
elseif ($version12.Count -eq 0 -and $version13.Count -eq 1) {
    Write-Host "Version VPP ya estaba en 2.1.0-alpha.13." -ForegroundColor Yellow
}
else {
    throw "Version VPP inesperada. alpha12=$($version12.Count) alpha13=$($version13.Count)"
}

$utf8NoBom = New-Object System.Text.UTF8Encoding($false)
[System.IO.File]::WriteAllText($manifestPath, $text, $utf8NoBom)

Write-Section "VALIDATION"
$updated = [System.IO.File]::ReadAllText($manifestPath)

if ($updated -notmatch '"pinMapping"\s*:\s*true') {
    throw "No quedo pinMapping=true en manifest.json"
}
if ($updated -notmatch '"version"\s*:\s*"2\.1\.0-alpha\.13"') {
    throw "No quedo version 2.1.0-alpha.13 en manifest.json"
}
if ($updated -notmatch '"key"\s*:\s*"packageSource"') {
    throw "Se perdio Package Source durante el ajuste."
}

& git -C $PlatformRepo diff --check
if ($LASTEXITCODE -ne 0) {
    throw "git diff --check fallo."
}

Write-Host ""
Write-Host "=== MANIFEST FOCUSED DIFF ===" -ForegroundColor Cyan
& git -C $PlatformRepo diff -- $manifestRel

Write-Host ""
Write-Host "JWPLC_PIN_MAPPING=ENABLED"
Write-Host "VPP_VERSION=2.1.0-alpha.13"
Write-Host "PACKAGE_SOURCE_SELECTOR=PRESERVED"
Write-Host "VPP_SIGNATURE=STALE_REQUIRES_RESIGN"
