param(
    [string]$ArduinoCli = 'C:\Program Files\Arduino PLC IDE Tools\arduino-cli.exe'
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$ExpectedBranch = 'v2.1.0-alpha.13/feature/cleanup-robustness'
$ProductHead = '6a585693c20c4f44bd789e00a7ae65b9828bcdec'
$ExpectedVersion = '2.5.43'
$Fqbn = 'jwplc_local:esp32:jwplcbasic'

$ScriptDir = $PSScriptRoot
$RepoRoot = [IO.Path]::GetFullPath((Join-Path $ScriptDir '..\..\..'))
$CommonPath = Join-Path $ScriptDir 'common.ps1'
$ProbeDir = Join-Path $RepoRoot 'tools\modbus-tcp-benchmark\firmware\a14_h3e1d1_tft_espi_compile_probe'

$RunId = Get-Date -Format 'yyyyMMdd_HHmmss'
$RunRoot = Join-Path $RepoRoot ("tools\alpha13\results\tft_pre3_{0}" -f $RunId)
$BuildRoot = Join-Path $env:TEMP ("jwplc_a13_tft_pre3_{0}" -f $RunId)
$CompileLog = Join-Path $RunRoot 'compile.log'
$SummaryLog = Join-Path $RunRoot 'SUMMARY.log'

New-Item -ItemType Directory -Path $RunRoot -Force | Out-Null
New-Item -ItemType Directory -Path $BuildRoot -Force | Out-Null

. $CommonPath

function Finish-TFTPre3
{
    param(
        [Parameter(Mandatory = $true)][ValidateSet('PASS','REVIEW','FAIL')][string]$Status,
        [Parameter(Mandatory = $true)][string]$Reason,
        [string]$ProductFailure = 'NO',
        [string]$HarnessFailure = 'NO',
        [string]$EnvironmentFailure = 'NO',
        [int]$ExitCode = 0,
        [string[]]$Extra = @()
    )

    $lines = @(
        'GATE=A13-TFT-PRE3',
        "STATUS=$Status",
        "REASON=$Reason",
        "PRODUCT_FAILURE=$ProductFailure",
        "HARNESS_FAILURE=$HarnessFailure",
        "ENVIRONMENT_FAILURE=$EnvironmentFailure",
        "RUN_ROOT=$RunRoot",
        "SUMMARY_LOG=$SummaryLog"
    ) + $Extra

    $lines | Set-Content -LiteralPath $SummaryLog -Encoding utf8
    $lines | ForEach-Object { Write-Host $_ }
    exit $ExitCode
}

function Get-DelayValues
{
    param([Parameter(Mandatory = $true)][string]$Text)

    $values = New-Object System.Collections.Generic.List[string]
    foreach ($match in [regex]::Matches($Text, 'delay\s*\(\s*(\d+)\s*\)'))
    {
        $values.Add($match.Groups[1].Value)
    }
    return @($values)
}

function Get-WriteCommandMatches
{
    param([Parameter(Mandatory = $true)][string]$Text)

    return @(
        [regex]::Matches(
            $Text,
            'writecommand\s*\(\s*(?<expr>[^\)]+?)\s*\)'
        )
    )
}

function Test-CommandSemantic
{
    param(
        [Parameter(Mandatory = $true)][string]$Expression,
        [Parameter(Mandatory = $true)][ValidateSet('SLPOUT','NORON','DISPON')][string]$Semantic
    )

    $expr = $Expression.Trim()
    $exprLower = $expr.ToLowerInvariant()

    switch ($Semantic)
    {
        'SLPOUT'
        {
            return (
                $expr -eq 'TFT_SLPOUT' -or
                $expr -eq 'ST7789_SLPOUT' -or
                $exprLower -eq '0x11' -or
                $expr -eq '17'
            )
        }
        'NORON'
        {
            return (
                $expr -eq 'TFT_NORON' -or
                $expr -eq 'ST7789_NORON' -or
                $exprLower -eq '0x13' -or
                $expr -eq '19'
            )
        }
        'DISPON'
        {
            return (
                $expr -eq 'TFT_DISPON' -or
                $expr -eq 'ST7789_DISPON' -or
                $exprLower -eq '0x29' -or
                $expr -eq '41'
            )
        }
    }

    return $false
}

function Get-CommandSemanticMatches
{
    param(
        [Parameter(Mandatory = $true)][object[]]$Matches,
        [Parameter(Mandatory = $true)][ValidateSet('SLPOUT','NORON','DISPON')][string]$Semantic
    )

    return @(
        $Matches |
            Where-Object {
                Test-CommandSemantic -Expression $_.Groups['expr'].Value -Semantic $Semantic
            }
    )
}

function Get-FirstDelayAfterMatch
{
    param(
        [Parameter(Mandatory = $true)][string]$Text,
        [Parameter(Mandatory = $true)][object]$Match
    )

    $index = $Match.Index + $Match.Length
    if ($index -ge $Text.Length)
    {
        return $null
    }

    $tailLength = [Math]::Min(500, $Text.Length - $index)
    $tail = $Text.Substring($index, $tailLength)
    $delayMatch = [regex]::Match($tail, 'delay\s*\(\s*(\d+)\s*\)')
    if (-not $delayMatch.Success)
    {
        return $null
    }

    return [int]$delayMatch.Groups[1].Value
}

Write-Host '============================================================'
Write-Host ' A13-TFT-PRE3 - TFT_eSPI 2.5.43 SOURCE ATTRIBUTION'
Write-Host '============================================================'

Push-Location $RepoRoot
try
{
    foreach ($required in @($CommonPath, $ProbeDir))
    {
        if (-not (Test-Path -LiteralPath $required))
        {
            Finish-TFTPre3 -Status 'REVIEW' -Reason "REQUIRED_PATH_MISSING:$required" -HarnessFailure 'YES' -ExitCode 3
        }
    }

    $branch = (git branch --show-current).Trim()
    $head = (git rev-parse HEAD).Trim()

    if ($branch -ne $ExpectedBranch)
    {
        Finish-TFTPre3 -Status 'REVIEW' -Reason 'UNEXPECTED_BRANCH' -HarnessFailure 'YES' -ExitCode 4
    }

    & git merge-base --is-ancestor $ProductHead $head
    if ($LASTEXITCODE -ne 0)
    {
        Finish-TFTPre3 -Status 'REVIEW' -Reason 'G2_PRODUCT_HEAD_NOT_ANCESTOR' -HarnessFailure 'YES' -ExitCode 5
    }

    $porcelain = @(& git status --porcelain=v1 --untracked-files=normal)
    if ($porcelain.Count -ne 0)
    {
        Finish-TFTPre3 -Status 'REVIEW' -Reason 'WORKTREE_NOT_CLEAN' -HarnessFailure 'YES' -ExitCode 6 -Extra @(
            "STATUS=$($porcelain -join ';')"
        )
    }

    if (-not (Test-Path -LiteralPath $ArduinoCli))
    {
        $cli = Get-Command arduino-cli -ErrorAction SilentlyContinue
        if ($null -eq $cli)
        {
            Finish-TFTPre3 -Status 'REVIEW' -Reason 'ARDUINO_CLI_NOT_FOUND' -EnvironmentFailure 'YES' -ExitCode 7
        }
        $ArduinoCli = $cli.Source
    }

    $cliVersion = @(& $ArduinoCli version 2>&1 | ForEach-Object { $_.ToString() }) -join ' '
    Write-Host "ARDUINO_CLI=$cliVersion"
    if ($cliVersion -notmatch 'Version:\s*1\.0\.2')
    {
        Finish-TFTPre3 -Status 'REVIEW' -Reason 'ARDUINO_CLI_VERSION_MISMATCH' -EnvironmentFailure 'YES' -ExitCode 8
    }

    $libList = Invoke-A13NativeCaptured -FilePath $ArduinoCli -Arguments @('lib','list')
    if ($libList.ExitCode -ne 0)
    {
        Finish-TFTPre3 -Status 'REVIEW' -Reason 'ARDUINO_LIB_LIST_FAILED' -EnvironmentFailure 'YES' -ExitCode 9
    }

    $libLine = @(
        $libList.Output |
            Where-Object { $_ -match '(^|\s)TFT_eSPI(\s|$)' }
    ) | Select-Object -First 1

    if ($null -eq $libLine)
    {
        Finish-TFTPre3 -Status 'REVIEW' -Reason 'TFT_ESPI_NOT_INSTALLED' -EnvironmentFailure 'YES' -ExitCode 10
    }

    $versionMatch = [regex]::Match($libLine, '(?<!\d)(\d+\.\d+\.\d+)(?!\d)')
    if (-not $versionMatch.Success)
    {
        Finish-TFTPre3 -Status 'REVIEW' -Reason 'TFT_ESPI_VERSION_PARSE_FAILED' -HarnessFailure 'YES' -ExitCode 11
    }

    $installedVersion = $versionMatch.Groups[1].Value
    Write-Host "TFT_ESPI_LIB_LIST_VERSION=$installedVersion"
    if ($installedVersion -ne $ExpectedVersion)
    {
        Finish-TFTPre3 -Status 'REVIEW' -Reason 'TFT_ESPI_VERSION_PIN_MISMATCH' -EnvironmentFailure 'YES' -ExitCode 12
    }

    $compile = Invoke-A13NativeCaptured -FilePath $ArduinoCli -Arguments @(
        'compile',
        '--fqbn', $Fqbn,
        '-j', '0',
        '-v',
        '--clean',
        '--build-path', $BuildRoot,
        $ProbeDir
    )

    $compile.Output | Set-Content -LiteralPath $CompileLog -Encoding utf8
    Write-Host "COMPILE_EXIT=$($compile.ExitCode)"

    if ($compile.ExitCode -ne 0)
    {
        Finish-TFTPre3 -Status 'REVIEW' -Reason 'TFT_ESPI_SOURCE_PROBE_COMPILE_FAILED' -EnvironmentFailure 'YES' -ExitCode 13 -Extra @(
            "COMPILE_LOG=$CompileLog"
        )
    }

    $selectionLines = @(
        $compile.Output |
            Where-Object { $_ -match '^Using library TFT_eSPI at version .+ in folder: .+$' }
    )

    if ($selectionLines.Count -ne 1)
    {
        Finish-TFTPre3 -Status 'REVIEW' -Reason 'TFT_ESPI_SELECTION_AMBIGUOUS' -HarnessFailure 'YES' -ExitCode 14 -Extra @(
            "SELECTION_COUNT=$($selectionLines.Count)"
        )
    }

    $selection = $selectionLines[0]
    $selectionMatch = [regex]::Match(
        $selection,
        '^Using library TFT_eSPI at version (?<version>\S+) in folder: (?<folder>.+)$'
    )

    if (-not $selectionMatch.Success)
    {
        Finish-TFTPre3 -Status 'REVIEW' -Reason 'TFT_ESPI_SELECTION_PARSE_FAILED' -HarnessFailure 'YES' -ExitCode 15
    }

    $selectedVersion = $selectionMatch.Groups['version'].Value.Trim()
    $selectedRoot = [IO.Path]::GetFullPath($selectionMatch.Groups['folder'].Value.Trim())

    Write-Host "TFT_ESPI_SELECTED_VERSION=$selectedVersion"
    Write-Host "TFT_ESPI_SELECTED_ROOT=$selectedRoot"

    if ($selectedVersion -ne $ExpectedVersion)
    {
        Finish-TFTPre3 -Status 'REVIEW' -Reason 'TFT_ESPI_SELECTED_VERSION_MISMATCH' -EnvironmentFailure 'YES' -ExitCode 16
    }

    $cppPath = Join-Path $selectedRoot 'TFT_eSPI.cpp'
    $initPath = Join-Path $selectedRoot 'TFT_Drivers\ST7789_Init.h'
    $headerPath = Join-Path $selectedRoot 'TFT_eSPI.h'

    foreach ($required in @($cppPath, $initPath, $headerPath))
    {
        if (-not (Test-Path -LiteralPath $required))
        {
            Finish-TFTPre3 -Status 'REVIEW' -Reason "TFT_ESPI_SOURCE_FILE_MISSING:$required" -EnvironmentFailure 'YES' -ExitCode 17
        }
    }

    $cppText = [IO.File]::ReadAllText($cppPath)
    $initText = [IO.File]::ReadAllText($initPath)
    $headerText = [IO.File]::ReadAllText($headerPath)

    $cppSha = Get-A13Sha256 -Path $cppPath
    $initSha = Get-A13Sha256 -Path $initPath
    $headerSha = Get-A13Sha256 -Path $headerPath

    $reset150 = [regex]::IsMatch($cppText, 'delay\s*\(\s*150\s*\)')
    $resetHighLowHigh =
        ([regex]::Matches($cppText, 'digitalWrite\s*\(\s*TFT_RST\s*,\s*HIGH\s*\)').Count -ge 2) -and
        ([regex]::Matches($cppText, 'digitalWrite\s*\(\s*TFT_RST\s*,\s*LOW\s*\)').Count -ge 1)

    $writeCommands = @(Get-WriteCommandMatches -Text $initText)
    $slpOutMatches = @(Get-CommandSemanticMatches -Matches $writeCommands -Semantic 'SLPOUT')
    $norOnMatches = @(Get-CommandSemanticMatches -Matches $writeCommands -Semantic 'NORON')
    $dispOnMatches = @(Get-CommandSemanticMatches -Matches $writeCommands -Semantic 'DISPON')

    $slpOutCount = $slpOutMatches.Count
    $norOnCount = $norOnMatches.Count
    $dispOnCount = $dispOnMatches.Count

    $slpOutExpr = if ($slpOutCount -gt 0) {
        $slpOutMatches[0].Groups['expr'].Value.Trim()
    }
    else {
        ''
    }

    $norOnExpr = if ($norOnCount -gt 0) {
        $norOnMatches[0].Groups['expr'].Value.Trim()
    }
    else {
        ''
    }

    $dispOnExpr = if ($dispOnCount -gt 0) {
        $dispOnMatches[0].Groups['expr'].Value.Trim()
    }
    else {
        ''
    }

    $delayValues = @(Get-DelayValues -Text $initText)
    $postDispOnDelay = if ($dispOnCount -gt 0) {
        Get-FirstDelayAfterMatch -Text $initText -Match $dispOnMatches[0]
    }
    else {
        $null
    }

    $ordered = (
        $slpOutCount -gt 0 -and
        $dispOnCount -gt 0 -and
        $dispOnMatches[0].Index -gt $slpOutMatches[0].Index
    )

    $versionLiteral = [regex]::Match($headerText, '#define\s+TFT_ESPI_VERSION\s+"([^"]+)"')
    $headerVersion = if ($versionLiteral.Success) { $versionLiteral.Groups[1].Value } else { '' }

    Write-Host "TFT_ESPI_HEADER_VERSION=$headerVersion"
    Write-Host "TFT_ESPI_CPP_SHA256=$cppSha"
    Write-Host "ST7789_INIT_SHA256=$initSha"
    Write-Host "TFT_ESPI_HEADER_SHA256=$headerSha"
    Write-Host "RESET_HIGH_LOW_HIGH_PRESENT=$resetHighLowHigh"
    Write-Host "RESET_DELAY_150_PRESENT=$reset150"
    Write-Host "ST7789_SLPOUT_COUNT=$slpOutCount"
    Write-Host "ST7789_NORON_COUNT=$norOnCount"
    Write-Host "ST7789_DISPON_COUNT=$dispOnCount"
    Write-Host "ST7789_SLPOUT_EXPR=$slpOutExpr"
    Write-Host "ST7789_NORON_EXPR=$norOnExpr"
    Write-Host "ST7789_DISPON_EXPR=$dispOnExpr"
    Write-Host "ST7789_DELAY_VALUES_MS=$($delayValues -join ',')"
    Write-Host "ST7789_SLPOUT_BEFORE_DISPON=$ordered"
    Write-Host "ST7789_FIRST_DELAY_AFTER_DISPON_MS=$postDispOnDelay"

    if ($headerVersion -ne $ExpectedVersion)
    {
        Finish-TFTPre3 -Status 'REVIEW' -Reason 'TFT_ESPI_HEADER_VERSION_MISMATCH' -EnvironmentFailure 'YES' -ExitCode 18
    }

    if (-not $resetHighLowHigh -or -not $reset150)
    {
        Finish-TFTPre3 -Status 'REVIEW' -Reason 'TFT_ESPI_RESET_SEQUENCE_UNEXPECTED' -HarnessFailure 'YES' -ExitCode 19
    }

    if ($slpOutCount -lt 1 -or $dispOnCount -lt 1 -or -not $ordered)
    {
        Finish-TFTPre3 -Status 'REVIEW' -Reason 'ST7789_INIT_SEQUENCE_UNEXPECTED' -HarnessFailure 'YES' -ExitCode 20
    }

    $porcelainFinal = @(& git status --porcelain=v1 --untracked-files=normal)
    & git diff --check
    $diffCheck = ($LASTEXITCODE -eq 0)

    if ($porcelainFinal.Count -ne 0 -or -not $diffCheck)
    {
        Finish-TFTPre3 -Status 'REVIEW' -Reason 'FINAL_REPO_AUDIT_FAILED' -HarnessFailure 'YES' -ExitCode 21 -Extra @(
            "STATUS=$($porcelainFinal -join ';')",
            "DIFF_CHECK=$diffCheck"
        )
    }

    Finish-TFTPre3 -Status 'PASS' -Reason 'TFT_ESPI_2543_SOURCE_ATTRIBUTED' -ExitCode 0 -Extra @(
        "HEAD=$head",
        "TFT_ESPI_SELECTED_VERSION=$selectedVersion",
        "TFT_ESPI_SELECTED_ROOT=$selectedRoot",
        "TFT_ESPI_CPP_SHA256=$cppSha",
        "ST7789_INIT_SHA256=$initSha",
        "TFT_ESPI_HEADER_SHA256=$headerSha",
        "RESET_HIGH_LOW_HIGH_PRESENT=$resetHighLowHigh",
        "RESET_DELAY_150_PRESENT=$reset150",
        "ST7789_SLPOUT_COUNT=$slpOutCount",
        "ST7789_NORON_COUNT=$norOnCount",
        "ST7789_DISPON_COUNT=$dispOnCount",
        "ST7789_SLPOUT_EXPR=$slpOutExpr",
        "ST7789_NORON_EXPR=$norOnExpr",
        "ST7789_DISPON_EXPR=$dispOnExpr",
        "ST7789_DELAY_VALUES_MS=$($delayValues -join ',')",
        "ST7789_SLPOUT_BEFORE_DISPON=$ordered",
        "ST7789_FIRST_DELAY_AFTER_DISPON_MS=$postDispOnDelay",
        "WORKTREE_FINAL=CLEAN",
        "DIFF_CHECK_FINAL=$diffCheck",
        "COMPILE_LOG=$CompileLog"
    )
}
catch
{
    Finish-TFTPre3 -Status 'REVIEW' -Reason 'UNEXPECTED_GATE_EXCEPTION' -HarnessFailure 'YES' -ProductFailure 'UNDETERMINED' -ExitCode 90 -Extra @(
        "EXCEPTION=$($_.Exception.Message)"
    )
}
finally
{
    Pop-Location
}
