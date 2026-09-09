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

# Windows PowerShell 5.1 no expone -CompilerOptions en Add-Type.
# Usamos CodeDOM directamente para conservar compatibilidad con el entorno
# típico de Arduino IDE en Windows y poder embeber /win32icon en el EXE.
$compilerParameters = New-Object System.CodeDom.Compiler.CompilerParameters
$compilerParameters.GenerateExecutable = $true
$compilerParameters.GenerateInMemory = $false
$compilerParameters.IncludeDebugInformation = $false
$compilerParameters.OutputAssembly = $OutputPath
$compilerParameters.CompilerOptions = '/target:winexe'
[void]$compilerParameters.ReferencedAssemblies.Add('System.dll')

if (-not [string]::IsNullOrWhiteSpace($IconPath) -and
    (Test-Path -LiteralPath $IconPath -PathType Leaf)) {
    $resolvedIcon = (Resolve-Path -LiteralPath $IconPath).Path
    $compilerParameters.CompilerOptions += (' /win32icon:"{0}"' -f $resolvedIcon)
    Write-Host "Icono EXE: $resolvedIcon" -ForegroundColor DarkGray
}
else {
    Write-Host 'Icono EXE no encontrado; se generará con el icono por defecto de Windows.' -ForegroundColor Yellow
}

Write-Host 'Compilador: Microsoft.CSharp.CSharpCodeProvider (compatible con Windows PowerShell 5.1)' -ForegroundColor DarkGray
$provider = New-Object Microsoft.CSharp.CSharpCodeProvider
try {
    $result = $provider.CompileAssemblyFromSource($compilerParameters, $code)
}
finally {
    $provider.Dispose()
}

$compileErrors = @($result.Errors | Where-Object { -not $_.IsWarning })
if ($compileErrors.Count -gt 0) {
    foreach ($errorItem in $compileErrors) {
        Write-Host ("C# {0}({1},{2}): {3}" -f $errorItem.ErrorNumber, $errorItem.Line, $errorItem.Column, $errorItem.ErrorText) -ForegroundColor Red
    }
    throw "No se pudo compilar JWPLC-HMI-Designer.exe."
}

if (-not (Test-Path -LiteralPath $OutputPath -PathType Leaf)) {
    throw "No se generó el ejecutable esperado: $OutputPath"
}

Write-Host 'JWPLC HMI Designer launcher EXE generado.' -ForegroundColor Green
Write-Host "  $OutputPath"
Write-Output $OutputPath
