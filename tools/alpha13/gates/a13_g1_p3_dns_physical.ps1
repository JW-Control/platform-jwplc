param(
    [Parameter(Position = 0)][string]$SerialPort = '',
    [string]$ArduinoCli = 'C:\Program Files\Arduino PLC IDE Tools\arduino-cli.exe'
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$ExpectedBranch = 'v2.1.0-alpha.13/feature/cleanup-robustness'
$BaselineHead = '20f1b66075dac04160311b7f40c939e88534073f'
$ExpectedDnsSha = '08291b4b89274f014e1ce6073bd16303192e5bfe1b4e2fdeaa21d133fb783995'
$DnsRelative = 'JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/Dns.cpp'
$Fqbn = 'jwplc_local:esp32:jwplcbasic'

$ScriptDir = $PSScriptRoot
$RepoRoot = [IO.Path]::GetFullPath((Join-Path $ScriptDir '..\..\..'))
$CommonPath = Join-Path $ScriptDir 'common.ps1'
$ResultsRoot = Join-Path $RepoRoot 'tools\alpha13\results'
$RunId = Get-Date -Format 'yyyyMMdd_HHmmss'
$RunRoot = Join-Path $ResultsRoot $RunId
$SummaryLog = Join-Path $RunRoot 'SUMMARY.log'

New-Item -ItemType Directory -Path $RunRoot -Force | Out-Null

function Write-Summary {
    param([string[]]$Lines)
    $Lines | Set-Content -LiteralPath $SummaryLog -Encoding UTF8
}

function Finish-Gate {
    param(
        [Parameter(Mandatory = $true)][ValidateSet('PASS','REVIEW','FAIL')][string]$Status,
        [Parameter(Mandatory = $true)][string]$Reason,
        [string]$ProductFailure = 'NO',
        [string]$HarnessFailure = 'NO',
        [string]$HardwareFailure = 'NO',
        [string]$EnvironmentFailure = 'NO',
        [int]$ExitCode = 0,
        [string[]]$Extra = @()
    )

    $lines = @(
        'GATE=A13-G1-P3-R2',
        "STATUS=$Status",
        "REASON=$Reason",
        "PRODUCT_FAILURE=$ProductFailure",
        "HARNESS_FAILURE=$HarnessFailure",
        "HARDWARE_FAILURE=$HardwareFailure",
        "ENVIRONMENT_FAILURE=$EnvironmentFailure",
        "RUN_ROOT=$RunRoot",
        "SUMMARY_LOG=$SummaryLog"
    ) + $Extra

    Write-Summary -Lines $lines
    $lines | ForEach-Object { Write-Host $_ }
    exit $ExitCode
}

$parseTokens = $null
$parseErrors = $null
[System.Management.Automation.Language.Parser]::ParseFile(
    $CommonPath,
    [ref]$parseTokens,
    [ref]$parseErrors
) | Out-Null

if ($parseErrors.Count -ne 0) {
    $parseErrors | ForEach-Object { Write-Host $_.Message }
    Finish-Gate -Status 'REVIEW' -Reason 'COMMON_PS1_SYNTAX_ERROR' -HarnessFailure 'YES' -ExitCode 2
}

. $CommonPath

Write-Host '============================================================'
Write-Host ' A13-G1-P3-R2 - PHYSICAL DNS REGRESSION'
Write-Host '============================================================'

Push-Location $RepoRoot
try {
    $Branch = (git branch --show-current).Trim()
    $Head = (git rev-parse HEAD).Trim()
    $Parent = (git rev-parse 'HEAD^').Trim()

    & git merge-base --is-ancestor $BaselineHead $Head
    $BaselineAncestor = ($LASTEXITCODE -eq 0)
    $CommittedSinceBaseline = @(
        git diff --name-only "$BaselineHead..$Head" |
            Where-Object { $_ -and $_.Trim().Length -gt 0 }
    )
    $AllowedCommitted = @(
        'docs/v2.1.0-alpha.13/ALPHA13_STATUS.md',
        'tools/alpha13/gates/common.ps1',
        'tools/alpha13/gates/a13_g1_p3_dns_physical.ps1',
        'tools/alpha13/gates/run_a13_g1_p3_dns_physical.bat',
        'tools/alpha13/results/.gitkeep'
    )
    $UnexpectedCommitted = @(
        $CommittedSinceBaseline |
            Where-Object { $_.Replace('\\','/') -notin $AllowedCommitted }
    )
    $DnsCommitted = @(
        $CommittedSinceBaseline |
            Where-Object { $_.Replace('\\','/') -eq $DnsRelative }
    ).Count -ne 0

    $Dirty = @(Get-A13TrackedDirty)
    $Staged = @(Get-A13Staged)

    $DnsPath = Join-Path $RepoRoot $DnsRelative
    $LibrariesRoot = Join-Path $RepoRoot 'JWPLC\2.1.0\libraries'
    $EthernetRoot = Join-Path $LibrariesRoot 'JWPLC_Ethernet'
    $ProbePath = Join-Path $RepoRoot 'tools\modbus-tcp-benchmark\firmware\a14_nb2_dns_async_probe\a14_nb2_dns_async_probe.ino'
    $ProbeDir = Split-Path -Parent $ProbePath
    $ClientPath = Join-Path $RepoRoot 'tools\modbus-tcp-benchmark\gates\a14_nb3_udp_send_dns_physical_client.py'

    foreach ($required in @($DnsPath, $EthernetRoot, $ProbePath, $ClientPath)) {
        if (-not (Test-Path -LiteralPath $required)) {
            Finish-Gate -Status 'REVIEW' -Reason "REQUIRED_PATH_MISSING:$required" -HarnessFailure 'YES' -ExitCode 3
        }
    }

    if ($Branch -ne $ExpectedBranch) {
        Finish-Gate -Status 'REVIEW' -Reason 'UNEXPECTED_BRANCH' -HarnessFailure 'YES' -ExitCode 4 -Extra @("BRANCH=$Branch", "HEAD=$Head")
    }

    if (-not $BaselineAncestor -or $UnexpectedCommitted.Count -ne 0 -or $DnsCommitted) {
        Finish-Gate -Status 'REVIEW' -Reason 'UNEXPECTED_HEAD_TOPOLOGY' -HarnessFailure 'YES' -ExitCode 5 -Extra @(
            "HEAD=$Head",
            "HEAD_PARENT=$Parent",
            "BASELINE_HEAD=$BaselineHead",
            "BASELINE_IS_ANCESTOR=$BaselineAncestor",
            "COMMITTED_SINCE_BASELINE=$($CommittedSinceBaseline -join ';')",
            "UNEXPECTED_COMMITTED=$($UnexpectedCommitted -join ';')",
            "DNS_COMMITTED=$DnsCommitted"
        )
    }

    $DnsSha = Get-A13Sha256 -Path $DnsPath
    if ($DnsSha -ne $ExpectedDnsSha) {
        Finish-Gate -Status 'REVIEW' -Reason 'DNS_CANDIDATE_HASH_MISMATCH' -HarnessFailure 'YES' -ExitCode 6 -Extra @("DNS_SHA256=$DnsSha", "EXPECTED_DNS_SHA256=$ExpectedDnsSha")
    }

    if ($Dirty.Count -ne 1 -or $Dirty[0].Replace('\','/') -ne $DnsRelative -or $Staged.Count -ne 0) {
        Finish-Gate -Status 'REVIEW' -Reason 'WORKTREE_SCOPE_MISMATCH' -HarnessFailure 'YES' -ExitCode 7 -Extra @(
            "TRACKED_DIRTY_COUNT=$($Dirty.Count)",
            "STAGED_COUNT=$($Staged.Count)",
            "DIRTY=$($Dirty -join ';')",
            "STAGED=$($Staged -join ';')"
        )
    }

    & git diff --check
    if ($LASTEXITCODE -ne 0) {
        Finish-Gate -Status 'REVIEW' -Reason 'GIT_DIFF_CHECK_FAILED' -HarnessFailure 'YES' -ExitCode 8
    }

    Write-Host "BRANCH=$Branch"
    Write-Host "HEAD=$Head"
    Write-Host "HEAD_PARENT=$Parent"
    Write-Host "BASELINE_IS_ANCESTOR=$BaselineAncestor"
    Write-Host "COMMITTED_SINCE_BASELINE=$($CommittedSinceBaseline -join ';')"
    Write-Host "DNS_COMMITTED=$DnsCommitted"
    Write-Host "DNS_SHA256=$DnsSha"
    Write-Host 'TRACKED_DIRTY_COUNT=1'
    Write-Host 'STAGED_COUNT=0'

    $PortResolution = Resolve-A13SerialPort -RequestedPort $SerialPort
    Write-Host "PORTS_VISIBLE=$($PortResolution.Ports -join ',')"
    Write-Host "PORT_CANDIDATES=$($PortResolution.Candidates -join ',')"
    Write-Host "PORT_RESOLUTION=$($PortResolution.Reason)"

    if ($PortResolution.Status -ne 'PASS') {
        Finish-Gate -Status 'REVIEW' -Reason $PortResolution.Reason -EnvironmentFailure 'YES' -ExitCode 10 -Extra @(
            "PORTS_VISIBLE=$($PortResolution.Ports -join ',')",
            "PORT_CANDIDATES=$($PortResolution.Candidates -join ',')",
            'UPLOAD_EXECUTED=NO',
            'CLIENT_EXECUTED=NO'
        )
    }

    $ResolvedPort = $PortResolution.Port
    Write-Host "SERIAL_PORT=$ResolvedPort"

    if (-not (Test-Path -LiteralPath $ArduinoCli)) {
        $cliCommand = Get-Command arduino-cli -ErrorAction SilentlyContinue
        if ($null -eq $cliCommand) {
            Finish-Gate -Status 'REVIEW' -Reason 'ARDUINO_CLI_NOT_FOUND' -EnvironmentFailure 'YES' -ExitCode 11
        }
        $ArduinoCli = $cliCommand.Source
    }

    $CliVersion = @( & $ArduinoCli version 2>&1 | ForEach-Object { $_.ToString() } ) -join ' '
    Write-Host "ARDUINO_CLI=$CliVersion"
    if ($CliVersion -notmatch 'Version:\s*1\.0\.2') {
        Finish-Gate -Status 'REVIEW' -Reason 'ARDUINO_CLI_VERSION_MISMATCH' -EnvironmentFailure 'YES' -ExitCode 12 -Extra @("ARDUINO_CLI=$CliVersion")
    }

    $PythonCommand = Get-Command python.exe -ErrorAction SilentlyContinue
    if ($null -eq $PythonCommand) {
        $PythonCommand = Get-Command python -ErrorAction SilentlyContinue
    }
    if ($null -eq $PythonCommand) {
        Finish-Gate -Status 'REVIEW' -Reason 'PYTHON_NOT_FOUND' -EnvironmentFailure 'YES' -ExitCode 13
    }
    $PythonExe = $PythonCommand.Source

    $PySerial = Invoke-A13NativeCaptured -FilePath $PythonExe -Arguments @('-c', 'import serial; print(serial.__version__)')
    if ($PySerial.ExitCode -ne 0) {
        Finish-Gate -Status 'REVIEW' -Reason 'PYSERIAL_NOT_AVAILABLE' -EnvironmentFailure 'YES' -ExitCode 14
    }

    $ClientSyntax = Invoke-A13NativeCaptured -FilePath $PythonExe -Arguments @('-m', 'py_compile', $ClientPath)
    if ($ClientSyntax.ExitCode -ne 0) {
        Finish-Gate -Status 'REVIEW' -Reason 'DNS_CLIENT_SYNTAX_ERROR' -HarnessFailure 'YES' -ExitCode 15
    }

    $DnsText = [IO.File]::ReadAllText($DnsPath)
    $GetHostMatch = [regex]::Match($DnsText, '(?ms)^int DNSClient::getHostByName\(.*?^\}')
    $PollMatch = [regex]::Match($DnsText, '(?ms)^int DNSClient::pollResolveAsync\(\)\s*\{.*?^\}')

    if (-not $GetHostMatch.Success -or -not $PollMatch.Success) {
        Finish-Gate -Status 'REVIEW' -Reason 'DNS_NORMAL_PATH_FUNCTION_NOT_FOUND' -HarnessFailure 'YES' -ExitCode 16
    }

    $LegacyBeginCount = ([regex]::Matches($GetHostMatch.Value, 'beginResolveAsync\s*\(')).Count
    $LegacyPollCount = ([regex]::Matches($GetHostMatch.Value, 'pollResolveAsync\s*\(')).Count
    $PacketParserCallCount = ([regex]::Matches($PollMatch.Value, 'ProcessResponsePacket\s*\(')).Count
    $PathContract = ($LegacyBeginCount -eq 1 -and $LegacyPollCount -eq 1 -and $PacketParserCallCount -eq 1)

    Write-Host "DNS_NORMAL_PATH_CONTRACT=$PathContract"
    if (-not $PathContract) {
        Finish-Gate -Status 'REVIEW' -Reason 'DNS_NORMAL_PATH_CONTRACT_CHANGED' -HarnessFailure 'YES' -ExitCode 17
    }

    $BuildPath = Join-Path $RunRoot 'build'
    $CompileLog = Join-Path $RunRoot 'compile.log'
    $UploadLog = Join-Path $RunRoot 'upload.log'
    $ClientLog = Join-Path $RunRoot 'client.log'
    New-Item -ItemType Directory -Path $BuildPath -Force | Out-Null

    $Compile = Invoke-A13NativeCaptured -FilePath $ArduinoCli -Arguments @(
        'compile', '--fqbn', $Fqbn, '-j', '0', '-v', '--clean',
        '--build-path', $BuildPath, '--libraries', $LibrariesRoot, $ProbeDir
    )
    $Compile.Output | Set-Content -LiteralPath $CompileLog -Encoding UTF8
    Write-Host "COMPILE_EXIT=$($Compile.ExitCode)"
    Write-Host "COMPILE_LOG=$CompileLog"

    if ($Compile.ExitCode -ne 0) {
        Finish-Gate -Status 'REVIEW' -Reason 'COMPILE_FAILED' -HarnessFailure 'YES' -ProductFailure 'UNDETERMINED' -ExitCode 20 -Extra @("COMPILE_LOG=$CompileLog")
    }

    $EthernetSelections = @($Compile.Output | Where-Object { $_ -match '^Using library JWPLC_Ethernet at version .+ in folder: ' })
    $ExpectedEthernetPath = [IO.Path]::GetFullPath($EthernetRoot).TrimEnd('\','/')
    $RepoEthernetSelected = $false
    foreach ($line in $EthernetSelections) {
        if ($line -match ' in folder: (?<path>.+)$') {
            $selected = [IO.Path]::GetFullPath($Matches['path'].Trim()).TrimEnd('\','/')
            if ($selected -ieq $ExpectedEthernetPath) {
                $RepoEthernetSelected = $true
            }
        }
    }

    $DnsObjects = @(
        Get-ChildItem -LiteralPath $BuildPath -Recurse -File |
            Where-Object { $_.Name -eq 'Dns.cpp.o' }
    )

    Write-Host "REPO_ETHERNET_SELECTED=$RepoEthernetSelected"
    Write-Host "DNS_SOURCE_OBJECT_COUNT=$($DnsObjects.Count)"

    if (-not $RepoEthernetSelected -or $DnsObjects.Count -ne 1) {
        Finish-Gate -Status 'REVIEW' -Reason 'SOURCE_SELECTION_NOT_PROVEN' -HarnessFailure 'YES' -ExitCode 21 -Extra @(
            "REPO_ETHERNET_SELECTED=$RepoEthernetSelected",
            "DNS_SOURCE_OBJECT_COUNT=$($DnsObjects.Count)",
            "COMPILE_LOG=$CompileLog"
        )
    }

    $Upload = Invoke-A13NativeCaptured -FilePath $ArduinoCli -Arguments @(
        'upload', '--fqbn', $Fqbn, '--port', $ResolvedPort,
        '--input-dir', $BuildPath, $ProbeDir
    )
    $Upload.Output | Set-Content -LiteralPath $UploadLog -Encoding UTF8
    Write-Host "UPLOAD_EXIT=$($Upload.ExitCode)"
    Write-Host "UPLOAD_LOG=$UploadLog"

    if ($Upload.ExitCode -ne 0) {
        Finish-Gate -Status 'REVIEW' -Reason 'UPLOAD_FAILED' -HardwareFailure 'YES' -EnvironmentFailure 'YES' -ExitCode 22 -Extra @(
            "SERIAL_PORT=$ResolvedPort",
            "UPLOAD_LOG=$UploadLog",
            'CLIENT_EXECUTED=NO'
        )
    }

    Start-Sleep -Milliseconds 800

    $Client = Invoke-A13NativeCaptured -FilePath $PythonExe -Arguments @(
        $ClientPath, '--serial', $ResolvedPort, '--baud', '115200', '--timeout-s', '10'
    )
    $Client.Output | Set-Content -LiteralPath $ClientLog -Encoding UTF8
    Write-Host "CLIENT_EXIT=$($Client.ExitCode)"
    Write-Host "CLIENT_LOG=$ClientLog"
    $Client.Output | ForEach-Object { Write-Host $_ }

    if ($Client.ExitCode -ne 0) {
        Finish-Gate -Status 'REVIEW' -Reason 'DNS_CLIENT_OR_RESPONDER_FAILED' -HarnessFailure 'YES' -EnvironmentFailure 'YES' -ExitCode 23 -Extra @(
            "CLIENT_EXIT=$($Client.ExitCode)",
            "CLIENT_LOG=$ClientLog"
        )
    }

    $LogText = $Client.Output -join [Environment]::NewLine
    $ResultCode = Get-A13LogInt -Text $LogText -Key 'RESULT_CODE'
    $ProbeFailed = Get-A13LogValue -Text $LogText -Key 'PROBE_FAILED'
    $DutIp = Get-A13LogValue -Text $LogText -Key 'DUT_IP_EFFECTIVE'
    $PcDnsIp = Get-A13LogValue -Text $LogText -Key 'PC_DNS_SERVER_IP'
    $SuccessIp = Get-A13LogValue -Text $LogText -Key 'SUCCESS_RESULT_IP'
    $SuccessDuration = Get-A13LogInt -Text $LogText -Key 'SUCCESS_DURATION_MS'
    $SuccessPolls = Get-A13LogInt -Text $LogText -Key 'SUCCESS_POLL_COUNT'
    $TimeoutDuration = Get-A13LogInt -Text $LogText -Key 'TIMEOUT_DURATION_MS'
    $TimeoutPolls = Get-A13LogInt -Text $LogText -Key 'TIMEOUT_POLL_COUNT'
    $SpiLockErrors = Get-A13LogInt -Text $LogText -Key 'SPI_LOCK_ERRORS'
    $ValidQueries = Get-A13LogInt -Text $LogText -Key 'DNS_VALID_QUERY_COUNT'
    $TimeoutQueries = Get-A13LogInt -Text $LogText -Key 'DNS_TIMEOUT_QUERY_COUNT'
    $OtherQueries = Get-A13LogInt -Text $LogText -Key 'DNS_OTHER_QUERY_COUNT'
    $ClientPass = Get-A13LogValue -Text $LogText -Key 'NB3_DNS_CLIENT_PASS'

    $requiredMarkersPresent = @(
        $ResultCode, $ProbeFailed, $DutIp, $PcDnsIp, $SuccessIp,
        $SuccessDuration, $SuccessPolls, $TimeoutDuration, $TimeoutPolls,
        $SpiLockErrors, $ValidQueries, $TimeoutQueries, $OtherQueries, $ClientPass
    ) -notcontains $null

    if (-not $requiredMarkersPresent) {
        Finish-Gate -Status 'REVIEW' -Reason 'DNS_RESULT_MARKERS_INCOMPLETE' -HarnessFailure 'YES' -ExitCode 24 -Extra @("CLIENT_LOG=$ClientLog")
    }

    $PhysicalPass = (
        $ResultCode -eq 1 -and
        $ProbeFailed -eq 'NO' -and
        $SuccessIp -eq '10.20.30.40' -and
        $SuccessPolls -ge 1 -and
        $TimeoutPolls -ge 1 -and
        $SpiLockErrors -eq 0 -and
        $ValidQueries -ge 1 -and
        $TimeoutQueries -ge 1 -and
        $OtherQueries -eq 0 -and
        $ClientPass -eq 'YES'
    )

    $DnsShaFinal = Get-A13Sha256 -Path $DnsPath
    $DirtyFinal = @(Get-A13TrackedDirty)
    $StagedFinal = @(Get-A13Staged)
    $ScopePass = (
        $DnsShaFinal -eq $ExpectedDnsSha -and
        $DirtyFinal.Count -eq 1 -and
        $DirtyFinal[0].Replace('\','/') -eq $DnsRelative -and
        $StagedFinal.Count -eq 0
    )

    & git diff --check
    $DiffCheckPass = ($LASTEXITCODE -eq 0)

    $resultLines = @(
        "BRANCH=$Branch",
        "HEAD=$Head",
        "SERIAL_PORT=$ResolvedPort",
        "DNS_SHA256=$DnsShaFinal",
        'DNS_NORMAL_PATH_CONTRACT=True',
        'COMPILE_EXIT=0',
        'REPO_ETHERNET_SELECTED=True',
        'DNS_SOURCE_OBJECT_COUNT=1',
        'UPLOAD_EXIT=0',
        'CLIENT_EXIT=0',
        "DUT_IP_EFFECTIVE=$DutIp",
        "PC_DNS_SERVER_IP=$PcDnsIp",
        "RESULT_CODE=$ResultCode",
        "PROBE_FAILED=$ProbeFailed",
        "SUCCESS_RESULT_IP=$SuccessIp",
        "SUCCESS_DURATION_MS=$SuccessDuration",
        "SUCCESS_POLL_COUNT=$SuccessPolls",
        "TIMEOUT_DURATION_MS=$TimeoutDuration",
        "TIMEOUT_POLL_COUNT=$TimeoutPolls",
        "SPI_LOCK_ERRORS=$SpiLockErrors",
        "DNS_VALID_QUERY_COUNT=$ValidQueries",
        "DNS_TIMEOUT_QUERY_COUNT=$TimeoutQueries",
        "DNS_OTHER_QUERY_COUNT=$OtherQueries",
        "DNS_CLIENT_PASS=$ClientPass",
        "DIRTY_SCOPE_VALID=$ScopePass",
        "DIFF_CHECK_PASS=$DiffCheckPass",
        "COMPILE_LOG=$CompileLog",
        "UPLOAD_LOG=$UploadLog",
        "CLIENT_LOG=$ClientLog"
    )

    if (-not $ScopePass -or -not $DiffCheckPass) {
        Finish-Gate -Status 'REVIEW' -Reason 'FINAL_CANDIDATE_AUDIT_FAILED' -HarnessFailure 'YES' -ExitCode 25 -Extra $resultLines
    }

    if (-not $PhysicalPass) {
        Finish-Gate -Status 'FAIL' -Reason 'PHYSICAL_DNS_CONTRACT_FAILED' -ProductFailure 'YES' -ExitCode 30 -Extra $resultLines
    }

    Finish-Gate -Status 'PASS' -Reason 'PHYSICAL_DNS_REGRESSION_PASS' -ExitCode 0 -Extra $resultLines
}
catch {
    Write-Host $_.Exception.Message
    Finish-Gate -Status 'REVIEW' -Reason 'UNEXPECTED_GATE_EXCEPTION' -HarnessFailure 'YES' -ProductFailure 'UNDETERMINED' -ExitCode 90 -Extra @("EXCEPTION=$($_.Exception.Message)")
}
finally {
    Pop-Location
}
