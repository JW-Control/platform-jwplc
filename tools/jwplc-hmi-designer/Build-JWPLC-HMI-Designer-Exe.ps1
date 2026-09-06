param(
    [string]$OutputPath = (Join-Path $PSScriptRoot 'JWPLC-HMI-Designer.exe')
)

$ErrorActionPreference = 'Stop'
$sourcePath = Join-Path $PSScriptRoot 'JWPLC-HMI-Designer-Launcher.cs'

if (-not (Test-Path -LiteralPath $sourcePath -PathType Leaf)) {
    throw "No se encontró el código fuente del launcher: $sourcePath"
}

$OutputPath = [IO.Path]::GetFullPath($OutputPath)
$outputDir = Split-Path -Parent $OutputPath
if ($outputDir) {
    New-Item -ItemType Directory -Force -Path $outputDir | Out-Null
}
Remove-Item -LiteralPath $OutputPath -Force -ErrorAction SilentlyContinue

$code = Get-Content -LiteralPath $sourcePath -Raw
Add-Type `
    -TypeDefinition $code `
    -Language CSharp `
    -OutputAssembly $OutputPath `
    -OutputType WindowsApplication `
    -ErrorAction Stop

if (-not (Test-Path -LiteralPath $OutputPath -PathType Leaf)) {
    throw "No se generó el ejecutable esperado: $OutputPath"
}

Write-Host 'JWPLC HMI Designer launcher EXE generado.' -ForegroundColor Green
Write-Host "  $OutputPath"
Write-Output $OutputPath
