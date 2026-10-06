param(
    [Parameter(Mandatory = $true)]
    [string]$Path
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$resolved = (Resolve-Path -LiteralPath $Path).Path
$tokens = $null
$errors = $null

[System.Management.Automation.Language.Parser]::ParseFile(
    $resolved,
    [ref]$tokens,
    [ref]$errors
) | Out-Null

Write-Host "POWERSHELL_SYNTAX_TARGET=$resolved"
Write-Host "POWERSHELL_SYNTAX_ERROR_COUNT=$($errors.Count)"

if ($errors.Count -gt 0) {
    foreach ($errorRecord in $errors) {
        $extent = $errorRecord.Extent
        Write-Host (
            "POWERSHELL_SYNTAX_ERROR Line={0} Column={1} Message={2}" -f
            $extent.StartLineNumber,
            $extent.StartColumnNumber,
            $errorRecord.Message
        )
    }

    exit 90
}

Write-Host "POWERSHELL_SYNTAX=PASS"
exit 0
