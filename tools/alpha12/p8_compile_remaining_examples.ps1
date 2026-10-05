param()

$ErrorActionPreference = "Stop"

$RepoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path
$Cli = "C:\Program Files\Arduino PLC IDE Tools\arduino-cli.exe"
$Fqbn = "jwplc_local:esp32:jwplcbasic"
$Libraries = Join-Path $RepoRoot "JWPLC\2.1.0\libraries"
$Timestamp = Get-Date -Format "yyyyMMdd_HHmmss"
$ResultRoot = Join-Path $env:TEMP ("jwplc_alpha12_p8_examples_" + $Timestamp)

if (-not (Test-Path $Cli)) {
    Write-Host "ERROR=ARDUINO_CLI_NOT_FOUND"
    Write-Host "PATH=$Cli"
    exit 2
}

New-Item -ItemType Directory -Path $ResultRoot -Force | Out-Null

Set-Location $RepoRoot

$Branch = (git branch --show-current).Trim()
$Head = (git rev-parse HEAD).Trim()
$Dirty = @(git status --short)

Write-Host "============================================================"
Write-Host " ALPHA12 - P8 REMAINING EXAMPLES COMPILE RUNNER"
Write-Host "============================================================"
Write-Host "BRANCH=$Branch"
Write-Host "HEAD=$Head"
Write-Host "FQBN=$Fqbn"
Write-Host "LIBRARIES=$Libraries"
Write-Host "RESULT_ROOT=$ResultRoot"
Write-Host ("DIRTY_COUNT=" + $Dirty.Count)
Write-Host ""

$Tests = @(
    [pscustomobject]@{
        Id = "ETH_STATIC"
        Sketch = Join-Path $Libraries "JWPLC_Ethernet\examples\02.Ethernet_StaticIP_Basic"
    },
    [pscustomobject]@{
        Id = "ETH_DIAGNOSTICS"
        Sketch = Join-Path $Libraries "JWPLC_Ethernet\examples\03.Ethernet_Diagnostics"
    },
    [pscustomobject]@{
        Id = "RS485_SEND"
        Sketch = Join-Path $Libraries "JWPLC_RS485\examples\01.RS485_Send"
    },
    [pscustomobject]@{
        Id = "RS485_ECHO"
        Sketch = Join-Path $Libraries "JWPLC_RS485\examples\02.RS485_Echo"
    },
    [pscustomobject]@{
        Id = "RS485_STATUS"
        Sketch = Join-Path $Libraries "JWPLC_RS485\examples\03.RS485_Status"
    }
)

$Results = @()

foreach ($Test in $Tests) {
    $LogFile = Join-Path $ResultRoot ($Test.Id + ".log")

    if (-not (Test-Path $Test.Sketch)) {
        $Results += [pscustomobject]@{
            Id = $Test.Id
            Exit = 3
            Result = "FAIL"
            Log = $LogFile
        }
        Write-Host ($Test.Id + "=FAIL SKETCH_NOT_FOUND")
        continue
    }

    & $Cli compile --verbose --fqbn $Fqbn --libraries $Libraries $Test.Sketch *> $LogFile
    $ExitCode = $LASTEXITCODE

    if ($ExitCode -eq 0) {
        $Status = "PASS"
    }
    else {
        $Status = "FAIL"
    }

    $Results += [pscustomobject]@{
        Id = $Test.Id
        Exit = $ExitCode
        Result = $Status
        Log = $LogFile
    }

    Write-Host ($Test.Id + "=" + $Status + " EXIT=" + $ExitCode)
}

$FailCount = @($Results | Where-Object { $_.Result -ne "PASS" }).Count

Write-Host ""
Write-Host "============================================================"
Write-Host " SUMMARY"
Write-Host "============================================================"

foreach ($Result in $Results) {
    Write-Host ($Result.Id + "=" + $Result.Result + " EXIT=" + $Result.Exit)
}

Write-Host ("PASS_COUNT=" + (@($Results | Where-Object { $_.Result -eq "PASS" }).Count))
Write-Host ("FAIL_COUNT=" + $FailCount)
Write-Host "RESULT_ROOT=$ResultRoot"

if ($FailCount -eq 0) {
    Write-Host "P8_REMAINING_EXAMPLES_COMPILE=PASS"
    exit 0
}

Write-Host "P8_REMAINING_EXAMPLES_COMPILE=FAIL"
exit 1
