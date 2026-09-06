param(
    [string]$OutputPath = (Join-Path $PSScriptRoot 'JWPLC-HMI-Designer.exe'),
    [string]$IconPath = (Join-Path $PSScriptRoot 'assets\JWPLC-HMI-Designer.ico')
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
$addTypeArgs = @{
    TypeDefinition = $code
    Language = 'CSharp'
    OutputAssembly = $OutputPath
    OutputType = 'WindowsApplication'
    ErrorAction = 'Stop'
}

if (-not [string]::IsNullOrWhiteSpace($IconPath) -and
    (Test-Path -LiteralPath $IconPath -PathType Leaf)) {
    $resolvedIcon = (Resolve-Path -LiteralPath $IconPath).Path
    $addTypeArgs['CompilerOptions'] = @(('/win32icon:"{0}"' -f $resolvedIcon))
    Write-Host "Icono EXE: $resolvedIcon" -ForegroundColor DarkGray
}
else {
    Write-Host 'Icono EXE no encontrado; se generará con el icono por defecto de Windows.' -ForegroundColor Yellow
}

Add-Type @addTypeArgs

if (-not (Test-Path -LiteralPath $OutputPath -PathType Leaf)) {
    throw "No se generó el ejecutable esperado: $OutputPath"
}

Write-Host 'JWPLC HMI Designer launcher EXE generado.' -ForegroundColor Green
Write-Host "  $OutputPath"
Write-Output $OutputPath
