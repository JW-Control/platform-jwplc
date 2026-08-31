param(
    [string]$PlatformRepo = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path
)

$ErrorActionPreference = "Stop"

function Normalize-Lf([string]$Text) {
    return $Text.Replace("`r`n", "`n").Replace("`r", "`n")
}

function Replace-Once([string]$Text, [string]$Old, [string]$New, [string]$Label) {
    $count = ([regex]::Matches($Text, [regex]::Escape($Old))).Count
    if ($count -ne 1) {
        throw "No se encontro un ancla unica para $Label. Coincidencias: $count"
    }
    return $Text.Replace($Old, $New)
}

function Write-Utf8NoBom([string]$Path, [string]$Text) {
    $utf8NoBom = New-Object System.Text.UTF8Encoding($false)
    [System.IO.File]::WriteAllText($Path, $Text, $utf8NoBom)
}

$PlatformRepo = (Resolve-Path $PlatformRepo).Path
$expectedBranch = "v2.1.0-alpha.7/feature/openplc-backplane-validation"
$branch = (& git -C $PlatformRepo branch --show-current).Trim()
if ($branch -ne $expectedBranch) {
    throw "Branch platform-jwplc inesperado. Esperado: $expectedBranch ; actual: $branch"
}

$manifestRel = "openplc-editor-installers/v4.2.7/vpp/manifest.json"
$configRel = "openplc-editor-installers/v4.2.7/vpp/screens/jwplc-basic-remote-io.json"
$manifestPath = Join-Path $PlatformRepo $manifestRel
$configPath = Join-Path $PlatformRepo $configRel

foreach ($path in @($manifestPath, $configPath)) {
    if (-not (Test-Path -LiteralPath $path)) {
        throw "No existe archivo esperado: $path"
    }
}

$manifest = Normalize-Lf ([System.IO.File]::ReadAllText($manifestPath))
$config = Normalize-Lf ([System.IO.File]::ReadAllText($configPath))

if ($manifest -notmatch '"version"\s*:\s*"2\.1\.0-alpha\.15"') {
    throw "Se esperaba VPP 2.1.0-alpha.15 antes de aplicar esta politica."
}

$configOld = @'
                    "default": 2,
                    "min": 1,
                    "max": 247,
'@
$configNew = @'
                    "default": 2,
                    "defaultFromSlot": true,
                    "uniqueAcrossSlots": true,
                    "min": 1,
                    "max": 247,
'@
$config = Replace-Once $config (Normalize-Lf $configOld) (Normalize-Lf $configNew) "metadata Slave ID"

$manifest = [regex]::Replace(
    $manifest,
    '"version"\s*:\s*"2\.1\.0-alpha\.15"',
    '"version": "2.1.0-alpha.16"',
    1
)

Write-Utf8NoBom $configPath ($config.TrimEnd() + "`n")
Write-Utf8NoBom $manifestPath ($manifest.TrimEnd() + "`n")

$manifestJson = Get-Content -LiteralPath $manifestPath -Raw | ConvertFrom-Json
$configJson = Get-Content -LiteralPath $configPath -Raw | ConvertFrom-Json
$field = $configJson.sections[0].fields[0]

if ($manifestJson.package.version -ne "2.1.0-alpha.16") { throw "VERSION_FAIL" }
if ($field.id -ne "slaveId") { throw "FIELD_ID_FAIL" }
if ($field.defaultFromSlot -ne $true) { throw "DEFAULT_FROM_SLOT_FAIL" }
if ($field.uniqueAcrossSlots -ne $true) { throw "UNIQUE_ACROSS_SLOTS_FAIL" }
if ($field.default -ne 2) { throw "FALLBACK_DEFAULT_FAIL" }

& git -C $PlatformRepo diff --check -- $manifestRel $configRel
if ($LASTEXITCODE -ne 0) {
    throw "git diff --check fallo para VPP Alpha16."
}

Write-Host "`n=== VPP REMOTE SLOT ID POLICY ===" -ForegroundColor Cyan
Write-Host "VERSION=$($manifestJson.package.version)"
Write-Host "FIELD_ID=$($field.id)"
Write-Host "FALLBACK_DEFAULT=$($field.default)"
Write-Host "DEFAULT_FROM_SLOT=$($field.defaultFromSlot)"
Write-Host "UNIQUE_ACROSS_SLOTS=$($field.uniqueAcrossSlots)"
Write-Host "RANGE=$($field.min)..$($field.max)"

Write-Host "`n=== DIFF STAT ===" -ForegroundColor Cyan
& git -C $PlatformRepo diff --stat -- $manifestRel $configRel

Write-Host "`nVPP_ALPHA16_SLOT_ID_POLICY=PASS" -ForegroundColor Green
Write-Host "SIGNATURE=STALE_REGENERATE_AFTER_COMMIT" -ForegroundColor Yellow
Write-Host "NEXT=COMMIT_AND_REBUILD_ALPHA16" -ForegroundColor Yellow
