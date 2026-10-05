param(
    [string]$ArduinoCli = "C:\Program Files\Arduino PLC IDE Tools\arduino-cli.exe",
    [string]$Fqbn = "jwplc_local:esp32:jwplcbasic"
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$repo = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot "..\..\.."))
$expectedBranch = "v2.1.0-alpha.12/feature/modbus-tcp"
$libraries = Join-Path $repo "JWPLC\2.1.0\libraries"
$stamp = Get-Date -Format "yyyyMMdd_HHmmss"
$resultRoot = Join-Path $env:TEMP ("jwplc_alpha12_final_cli_" + $stamp)

function Invoke-Captured {
    param(
        [string]$FilePath,
        [string[]]$Arguments
    )

    $old = $ErrorActionPreference
    try {
        $ErrorActionPreference = "Continue"
        [object[]]$out = @(& $FilePath @Arguments 2>&1)
        $code = [int]$LASTEXITCODE
    }
    finally {
        $ErrorActionPreference = $old
    }

    return [pscustomobject]@{
        ExitCode = $code
        Output = @($out | ForEach-Object { $_.ToString() })
    }
}

function Test-LibrarySelected {
    param(
        [string[]]$Lines,
        [string]$LibraryName
    )

    foreach ($line in $Lines) {
        if ($line -match ("^Using library " + [regex]::Escape($LibraryName) + " at version .+ in folder: (.+)$")) {
            return $true
        }
    }

    return $false
}

if (-not (Test-Path -LiteralPath $ArduinoCli)) {
    throw "A12_FINAL_CLI_ARDUINO_CLI_NOT_FOUND"
}

$branch = (& git -C $repo branch --show-current).Trim()
$head = (& git -C $repo rev-parse HEAD).Trim()

Write-Host "=============================================================================="
Write-Host " ALPHA12 - FINAL ARDUINO CLI + REPO HYGIENE GATE"
Write-Host "=============================================================================="
Write-Host ("BRANCH=" + $branch)
Write-Host ("HEAD=" + $head)
Write-Host ("FQBN=" + $Fqbn)

if ($branch -ne $expectedBranch) {
    throw ("A12_FINAL_CLI_BRANCH_MISMATCH expected=" + $expectedBranch + " actual=" + $branch)
}

[string[]]$dirty = @(& git -C $repo status --short)
Write-Host ("ENTRY_DIRTY_COUNT=" + [string]$dirty.Count)

if ($dirty.Count -ne 0) {
    $dirty | ForEach-Object { Write-Host ("ENTRY_DIRTY=" + $_) }
    throw "A12_FINAL_CLI_TREE_NOT_CLEAN"
}

& git -C $repo diff --check
if ($LASTEXITCODE -ne 0) {
    throw "A12_FINAL_CLI_GIT_DIFF_CHECK_FAILED"
}

# Fuente autoritativa para conflictos Git no resueltos.
[string[]]$unmergedIndex = @(& git -C $repo ls-files -u)

if ($LASTEXITCODE -ne 0) {
    throw "A12_FINAL_CLI_UNMERGED_INDEX_SCAN_FAILED"
}

Write-Host ("UNMERGED_INDEX_COUNT=" + [string]$unmergedIndex.Count)

if ($unmergedIndex.Count -ne 0) {
    $unmergedIndex | ForEach-Object { Write-Host ("UNMERGED_INDEX=" + $_) }
    throw "A12_FINAL_CLI_UNMERGED_INDEX_FOUND"
}

# Scan textual defensivo SOLO del package activo. Los docs/harness pueden
# citar marcadores como texto y no forman parte del source productivo a liberar.
# Los marcadores se construyen dinamicamente para que el propio harness no los
# contenga de forma literal.
$leftMarker = ("<" * 7) + " "
$rightMarker = (">" * 7) + " "
[string[]]$conflictScope = @(
    "JWPLC/2.1.0"
)

[object[]]$leftHits = @(& git -C $repo grep -n -F -- $leftMarker @conflictScope)
$leftExit = $LASTEXITCODE
[object[]]$rightHits = @(& git -C $repo grep -n -F -- $rightMarker @conflictScope)
$rightExit = $LASTEXITCODE

if ($leftExit -notin @(0,1) -or $rightExit -notin @(0,1)) {
    throw "A12_FINAL_CLI_CONFLICT_MARKER_SCAN_FAILED"
}

[object[]]$conflictMarkers = @($leftHits) + @($rightHits)
Write-Host ("CONFLICT_MARKER_COUNT=" + [string]$conflictMarkers.Count)

if ($conflictMarkers.Count -ne 0) {
    $conflictMarkers | ForEach-Object { Write-Host ("CONFLICT_MARKER=" + $_) }
    throw "A12_FINAL_CLI_CONFLICT_MARKERS_FOUND"
}

New-Item -ItemType Directory -Path $resultRoot -Force | Out-Null

[object[]]$tests = @(
    [pscustomobject]@{
        Id = "EMPTY_AUTOLOAD"
        Sketch = Join-Path $repo "tools\build-speed-benchmark\sketches\01_empty"
        VerifyAutoload = $true
    },
    [pscustomobject]@{
        Id = "MODBUS_TCP_SERVER"
        Sketch = Join-Path $libraries "JWPLC_ModbusTCP\examples\01.ModbusTCP_Server"
        VerifyAutoload = $false
    },
    [pscustomobject]@{
        Id = "MODBUS_TCP_CLIENT"
        Sketch = Join-Path $libraries "JWPLC_ModbusTCP\examples\02.ModbusTCP_Client"
        VerifyAutoload = $false
    },
    [pscustomobject]@{
        Id = "MODBUS_RTU_SLAVE"
        Sketch = Join-Path $libraries "JWPLC_ModbusRTU\examples\01.ModbusRTU_Slave_Holding"
        VerifyAutoload = $false
    },
    [pscustomobject]@{
        Id = "MODBUS_RTU_MASTER_READ"
        Sketch = Join-Path $libraries "JWPLC_ModbusRTU\examples\02.ModbusRTU_Master_Read"
        VerifyAutoload = $false
    },
    [pscustomobject]@{
        Id = "MODBUS_RTU_MASTER_WRITE"
        Sketch = Join-Path $libraries "JWPLC_ModbusRTU\examples\03.ModbusRTU_Master_Write"
        VerifyAutoload = $false
    }
)

[string[]]$requiredAutoload = @(
    "JWPLC_Display",
    "JWPLC_TFT",
    "JW_MatrixButtons",
    "JWPLC_GlobalPeripherals",
    "JW_RTC",
    "JW_FRAM",
    "JW_SD",
    "JWPLC_Ethernet",
    "JWPLC_RS485",
    "JWPLC_ModbusRTU",
    "SPI",
    "SD"
)

[object[]]$results = @()

foreach ($test in $tests) {
    if (-not (Test-Path -LiteralPath $test.Sketch)) {
        throw ("A12_FINAL_CLI_SKETCH_MISSING:" + $test.Id)
    }

    $buildPath = Join-Path $resultRoot ("build_" + $test.Id)
    $logPath = Join-Path $resultRoot ($test.Id + ".log")

    $args = @(
        "compile",
        "--fqbn", $Fqbn,
        "-j", "0",
        "-v",
        "--clean",
        "--build-path", $buildPath,
        "--libraries", $libraries,
        $test.Sketch
    )

    $run = Invoke-Captured -FilePath $ArduinoCli -Arguments $args
    $run.Output | Set-Content -LiteralPath $logPath -Encoding UTF8

    $warnings = @($run.Output | Where-Object { $_ -match '(?i)\bwarning:' }).Count
    $errors = @($run.Output | Where-Object { $_ -match '(?i)\berror:' }).Count

    $status = "PASS"
    if ($run.ExitCode -ne 0 -or $warnings -ne 0 -or $errors -ne 0) {
        $status = "FAIL"
    }

    if ($test.VerifyAutoload -and $status -eq "PASS") {
        foreach ($libraryName in $requiredAutoload) {
            if (-not (Test-LibrarySelected -Lines $run.Output -LibraryName $libraryName)) {
                Write-Host ("AUTOLOAD_MISSING=" + $libraryName)
                $status = "FAIL"
            }
        }
    }

    $results += [pscustomobject]@{
        Id = $test.Id
        Exit = $run.ExitCode
        Warnings = $warnings
        Errors = $errors
        Result = $status
        Log = $logPath
    }

    Write-Host (
        $test.Id +
        "=" + $status +
        " EXIT=" + $run.ExitCode +
        " WARNINGS=" + $warnings +
        " ERRORS=" + $errors
    )
}

$failCount = @($results | Where-Object { $_.Result -ne "PASS" }).Count

[string[]]$finalDirty = @(& git -C $repo status --short)

Write-Host ""
Write-Host "=============================================================================="
Write-Host " SUMMARY"
Write-Host "=============================================================================="

foreach ($result in $results) {
    Write-Host (
        $result.Id +
        "=" + $result.Result +
        " EXIT=" + $result.Exit +
        " WARNINGS=" + $result.Warnings +
        " ERRORS=" + $result.Errors
    )
}

Write-Host ("PASS_COUNT=" + [string](@($results | Where-Object { $_.Result -eq "PASS" }).Count))
Write-Host ("FAIL_COUNT=" + [string]$failCount)
Write-Host ("FINAL_DIRTY_COUNT=" + [string]$finalDirty.Count)
Write-Host ("RESULT_ROOT=" + $resultRoot)

if ($failCount -ne 0) {
    throw "A12_FINAL_CLI_COMPILE_FAILURE"
}

if ($finalDirty.Count -ne 0) {
    $finalDirty | ForEach-Object { Write-Host ("FINAL_DIRTY=" + $_) }
    throw "A12_FINAL_CLI_REPOSITORY_MUTATED"
}

Write-Host "FINAL_REPO_HYGIENE=PASS"
Write-Host "FINAL_ARDUINO_CLI_GATE=PASS"
