param(
    [string]$ArduinoCli = 'C:\Program Files\Arduino PLC IDE Tools\arduino-cli.exe'
)
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$ExpectedBranch = 'v2.1.0-alpha.13/feature/cleanup-robustness'
$Pre5Commit = '2a2d2943db22c69d2da6cc8377f0341a5e6f8ddb'
$CoreSha = '6f328eeb796091070c8d852a2c0e90f71047d8d2b5786fe3cd5079b2fb6ff983'
$TftOldSha = '5d860a131811dd9a7eb6fa55f5674b1d78b0de7dfaf8748ce18a60ceed2d3738'
$DisplaySha = 'c960d718433e29a40e3cc55bc745c9a2e121ee1ee598ec72c04872327592dc02'
$CandidateCppSha = '494440b00e74e74a7b23975420574e035ef2138fdf5a0cea2fed51c986c29d25'
$CandidateSetupSha = '8fa079444ca130772d3a642eaf814035100bc098032b403c922c458d351ae5e1'
$Fqbn = 'jwplc_local:esp32:jwplcbasic'
$RepoRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..\..'))
$CommonPath = Join-Path $PSScriptRoot 'common.ps1'
$Libs = Join-Path $RepoRoot 'JWPLC\2.1.0\libraries'
$TftRoot = Join-Path $Libs 'JWPLC_TFT'
$TftArchive = Join-Path $TftRoot 'src\esp32\libJWPLC_TFT.a'
$CoreArchive = Join-Path $RepoRoot 'JWPLC\2.1.0\precompiled\core\JWPLCBASIC\core.a'
$DisplayArchive = Join-Path $Libs 'JWPLC_Display\src\esp32\libJWPLC_Display.a'
$ResultsRoot = Join-Path $RepoRoot 'tools\alpha13\results'
$RunId = Get-Date -Format 'yyyyMMdd_HHmmss'
$RunRoot = Join-Path $ResultsRoot ('tft_pre6_p0_' + $RunId)
$TempRoot = Join-Path $env:TEMP ('jwplc_a13_tft_pre6_p0_' + $RunId)
$TempLibrary = Join-Path $TempRoot 'libraries\JWPLC_TFT'
$TempSrc = Join-Path $TempLibrary 'src'
$TempArchiveDir = Join-Path $TempSrc 'esp32'
$TempArchive = Join-Path $TempArchiveDir 'libJWPLC_TFT.a'
$ExtractDir = Join-Path $TempRoot 'extract'
$Summary = Join-Path $RunRoot 'SUMMARY.log'
New-Item -ItemType Directory -Force -Path $RunRoot,$TempArchiveDir,$ExtractDir | Out-Null
. $CommonPath

$script:Phase = 'PRECHECK'
function Finish-Gate {
    param(
        [ValidateSet('PASS','REVIEW')][string]$Status,
        [string]$Reason,
        [string]$HarnessFailure = 'NO',
        [string]$EnvironmentFailure = 'NO',
        [int]$Code = 0,
        [string[]]$Extra = @()
    )
    $lines = @(
        'GATE=A13-TFT-PRE6-P0',
        "STATUS=$Status",
        "REASON=$Reason",
        'PRODUCT_FAILURE=NO',
        "HARNESS_FAILURE=$HarnessFailure",
        "ENVIRONMENT_FAILURE=$EnvironmentFailure",
        'UPLOAD_EXECUTED=NO',
        'PRODUCT_REPO_MODIFIED=NO',
        "PHASE=$script:Phase",
        "RUN_ROOT=$RunRoot",
        "SUMMARY_LOG=$Summary"
    ) + $Extra
    $lines | Set-Content -LiteralPath $Summary -Encoding utf8
    $lines | ForEach-Object { Write-Host $_ }
    exit $Code
}
function Stop-Gate {
    param([string]$Reason,[string]$Category = 'HARNESS')
    Finish-Gate -Status 'REVIEW' -Reason $Reason -HarnessFailure $(if ($Category -eq 'HARNESS') {'YES'} else {'NO'}) -EnvironmentFailure $(if ($Category -eq 'ENVIRONMENT') {'YES'} else {'NO'}) -Code 3
}
function Key {
    param([string]$Text,[string]$Name)
    $m = @([regex]::Matches($Text,'(?m)^'+[regex]::Escape($Name)+'=(.*)\r?$'))
    if ($m.Count -ne 1) { return $null }
    return $m[0].Groups[1].Value.Trim()
}
function Clean {
    $dirty = @(& git -C $RepoRoot status --porcelain=v1 --untracked-files=normal)
    if ($dirty.Count -ne 0) { Stop-Gate ('WORKTREE_NOT_CLEAN:' + ($dirty -join ';')) }
    & git -C $RepoRoot diff --check
    if ($LASTEXITCODE -ne 0) { Stop-Gate 'GIT_DIFF_CHECK_FAILED' }
}
function OneObject {
    param([string]$Root,[string]$Name)
    $files = @(Get-ChildItem -LiteralPath $Root -Recurse -File -ErrorAction SilentlyContinue | Where-Object { $_.Name -eq $Name })
    if ($files.Count -ne 1) { Stop-Gate ('OBJECT_COUNT_INVALID:'+$Name+':'+$files.Count) }
    return $files[0].FullName
}
function Select-Archiver {
    $path = Join-Path $env:LOCALAPPDATA 'Arduino15\packages\jwplc_local\tools\esp-x32\2601\bin\xtensa-esp32-elf-gcc-ar.exe'
    if (Test-Path -LiteralPath $path) { return $path }
    $root = Join-Path $env:LOCALAPPDATA 'Arduino15\packages\jwplc_local\tools\esp-x32'
    if (Test-Path -LiteralPath $root) {
        $found = @(Get-ChildItem -LiteralPath $root -Recurse -File -Filter 'xtensa-esp32-elf-gcc-ar.exe' -ErrorAction SilentlyContinue)
        if ($found.Count -eq 1) { return $found[0].FullName }
    }
    Stop-Gate 'ARCHIVER_NOT_FOUND_OR_AMBIGUOUS' 'ENVIRONMENT'
}
function Compile-Case {
    param([string]$Label,[string]$Sketch)
    $script:Phase = 'COMPILE_' + $Label
    if (-not (Test-Path -LiteralPath $Sketch)) { Stop-Gate ('SKETCH_MISSING:'+$Label) }
    $build = Join-Path $TempRoot ('build_'+$Label)
    $run = Invoke-A13NativeCaptured -FilePath $ArduinoCli -Arguments @('compile','--fqbn',$Fqbn,'-j','0','-v','--clean','--build-path',$build,'--library',$TempLibrary,'--libraries',$Libs,$Sketch)
    $log = Join-Path $RunRoot ('compile_'+$Label+'.log')
    $run.Output | Set-Content -LiteralPath $log -Encoding utf8
    Write-Host ($Label+'_COMPILE_EXIT='+$run.ExitCode)
    if ($run.ExitCode -ne 0) {
        $run.Output | Select-Object -Last 40 | ForEach-Object { Write-Host $_ }
        Stop-Gate ('COMPILE_FAILED:'+$Label)
    }
    $sel = @($run.Output | Where-Object { $_ -match '^Using library JWPLC_TFT at version .+ in folder: .+$' })
    if ($sel.Count -ne 1) { Stop-Gate ('TFT_LIBRARY_SELECTION_COUNT:'+$Label+':'+$sel.Count) }
    $match = [regex]::Match($sel[0],'^Using library JWPLC_TFT at version .+ in folder: (.+)$')
    if (-not $match.Success) { Stop-Gate ('TFT_LIBRARY_SELECTION_PARSE:'+$Label) }
    $actual = [IO.Path]::GetFullPath($match.Groups[1].Value.Trim()).TrimEnd('\','/')
    $expected = [IO.Path]::GetFullPath($TempLibrary).TrimEnd('\','/')
    $esp = @($run.Output | Where-Object { $_ -match '^Using library TFT_eSPI at version ' })
    $tftObjects = @(Get-ChildItem -LiteralPath $build -Recurse -File -ErrorAction SilentlyContinue | Where-Object { $_.Name -eq 'JWPLC_TFT.cpp.o' })
    $espObjects = @(Get-ChildItem -LiteralPath $build -Recurse -File -ErrorAction SilentlyContinue | Where-Object { $_.Name -eq 'TFT_eSPI.cpp.o' })
    $text = $run.Output -join [Environment]::NewLine
    $precompiled = [regex]::IsMatch($text,'(?i)using precompiled library .*JWPLC_TFT|usando .*precompilad.*JWPLC_TFT')
    $stub = $text.Contains("Using core 'jwcontrol_precompiled_stub'")
    $core = $text -match '[\\/]precompiled[\\/]core[\\/]JWPLCBASIC[\\/]core\.a'
    Write-Host ($Label+'_TEMP_ARCHIVE_SELECTED='+($actual -ieq $expected))
    Write-Host ($Label+'_PRECOMPILED='+$precompiled)
    Write-Host ($Label+'_TFT_SOURCE_OBJECTS='+$tftObjects.Count)
    Write-Host ($Label+'_BACKEND_SOURCE_OBJECTS='+$espObjects.Count)
    Write-Host ($Label+'_GLOBAL_TFT_ESPI='+$esp.Count)
    Write-Host ($Label+'_STUB_CORE='+$stub)
    Write-Host ($Label+'_CORE_ARCHIVE_LINKED='+$core)
    if ($actual -ine $expected -or -not $precompiled -or $esp.Count -ne 0 -or
        $tftObjects.Count -ne 0 -or $espObjects.Count -ne 0 -or -not $stub -or -not $core) {
        Stop-Gate ('NORMAL_PACKAGE_CONTRACT_FAILED:'+$Label)
    }
}
Write-Host '============================================================'
Write-Host ' A13-TFT-PRE6-P0 - SELF-CONTAINED ARCHIVE CANDIDATE'
Write-Host '============================================================'
Push-Location $RepoRoot
try {
    $branch = (& git branch --show-current).Trim()
    $head = (& git rev-parse HEAD).Trim()
    if ($branch -ne $ExpectedBranch) { Stop-Gate 'BRANCH_MISMATCH' }
    & git merge-base --is-ancestor $Pre5Commit $head
    if ($LASTEXITCODE -ne 0) { Stop-Gate 'PRE5_COMMIT_NOT_ANCESTOR' }
    Clean
    foreach ($p in @($TftArchive,$CoreArchive,$DisplayArchive,$CommonPath,(Join-Path $TftRoot 'library.properties'),(Join-Path $TftRoot 'src\JWPLC_TFT.h'))) {
        if (-not (Test-Path -LiteralPath $p)) { Stop-Gate ('PATH_MISSING:'+$p) }
    }
    if ((Get-A13Sha256 -Path $TftArchive) -ne $TftOldSha -or
        (Get-A13Sha256 -Path $CoreArchive) -ne $CoreSha -or
        (Get-A13Sha256 -Path $DisplayArchive) -ne $DisplaySha) {
        Stop-Gate 'OFFICIAL_ARCHIVE_IDENTITY_MISMATCH'
    }
    if (-not (Test-Path -LiteralPath $ArduinoCli)) {
        $found = Get-Command arduino-cli -ErrorAction SilentlyContinue
        if ($null -eq $found) { Stop-Gate 'ARDUINO_CLI_NOT_FOUND' 'ENVIRONMENT' }
        $ArduinoCli = $found.Source
    }
    $cli = @(& $ArduinoCli version 2>&1 | ForEach-Object { $_.ToString() }) -join ' '
    Write-Host ('ARDUINO_CLI='+$cli)
    if ($cli -notmatch 'Version:\s*1\.0\.2') { Stop-Gate 'CLI_VERSION_MISMATCH' 'ENVIRONMENT' }

    $script:Phase = 'PRE5_EVIDENCE'
    $files = @(Get-ChildItem -LiteralPath $ResultsRoot -Directory -Filter 'tft_pre5_*' | Sort-Object Name -Descending | ForEach-Object { Join-Path $_.FullName 'SUMMARY.log' } | Where-Object { Test-Path -LiteralPath $_ })
    $proof = $null
    $proofText = ''
    foreach ($file in $files) {
        $txt = [IO.File]::ReadAllText($file)
        if ((Key $txt 'STATUS') -eq 'PASS' -and
            (Key $txt 'REASON') -eq 'SOURCE_FIRST_CANDIDATE_FLASHED' -and
            (Key $txt 'RUNTIME_CANDIDATE_SHA256') -eq $CandidateCppSha) {
            $proof = $file; $proofText = $txt; break
        }
    }
    if ($null -eq $proof) { Stop-Gate 'PRE5_VALIDATED_SUMMARY_NOT_FOUND' }
    foreach ($key in @('JWPLC_TFT_TEMP_SELECTED','TFT_ESPI_TEMP_SELECTED','USES_STUB_CORE','CORE_A_LINKED')) {
        if ((Key $proofText $key) -ne 'True') { Stop-Gate ('PRE5_BOOLEAN_INVALID:'+$key) }
    }
    foreach ($key in @('JWPLC_TFT_SOURCE_OBJECT_COUNT','TFT_ESPI_SOURCE_OBJECT_COUNT')) {
        if ((Key $proofText $key) -ne '1') { Stop-Gate ('PRE5_OBJECT_COUNT_INVALID:'+$key) }
    }
    if ((Key $proofText 'DISPLAY_READY') -ne 'YES' -or (Key $proofText 'IO_READY') -ne 'YES') { Stop-Gate 'PRE5_RUNTIME_INVALID' }
    $pre5Tft = Key $proofText 'TEMP_JWPLC_TFT_ROOT'
    if ([string]::IsNullOrWhiteSpace($pre5Tft)) { Stop-Gate 'PRE5_SOURCE_ROOT_MISSING' }
    $pre5Build = [IO.Path]::GetFullPath((Join-Path $pre5Tft '..\..\build'))
    $cpp = Join-Path $pre5Tft 'src\JWPLC_TFT.cpp'
    $setup = Join-Path $pre5Tft 'src\tft_setup.h'
    foreach ($p in @($cpp,$setup,$pre5Build)) {
        if (-not (Test-Path -LiteralPath $p)) { Stop-Gate ('PRE5_TEMP_PATH_MISSING:'+$p) }
    }
    if ((Get-A13Sha256 -Path $cpp) -ne $CandidateCppSha -or
        (Get-A13Sha256 -Path $setup) -ne $CandidateSetupSha) {
        Stop-Gate 'PRE5_SOURCE_HASH_MISMATCH'
    }
    $tftObj = OneObject $pre5Build 'JWPLC_TFT.cpp.o'
    $espObj = OneObject $pre5Build 'TFT_eSPI.cpp.o'
    $tftObjSha = Get-A13Sha256 -Path $tftObj
    $espObjSha = Get-A13Sha256 -Path $espObj
    Write-Host ('PRE5_SUMMARY='+$proof)
    Write-Host ('TFT_OBJECT_SHA256='+$tftObjSha)
    Write-Host ('BACKEND_OBJECT_SHA256='+$espObjSha)

    $script:Phase = 'ARCHIVE_BUILD'
    Copy-Item -LiteralPath (Join-Path $TftRoot 'src\JWPLC_TFT.h') -Destination (Join-Path $TempSrc 'JWPLC_TFT.h') -Force
    Copy-Item -LiteralPath (Join-Path $TftRoot 'library.properties') -Destination (Join-Path $TempLibrary 'library.properties') -Force
    $archiver = Select-Archiver
    Write-Host ('ARCHIVER='+$archiver)
    $make = Invoke-A13NativeCaptured -FilePath $archiver -Arguments @('crs',$TempArchive,$tftObj,$espObj)
    if ($make.ExitCode -ne 0 -or -not (Test-Path -LiteralPath $TempArchive)) { Stop-Gate 'ARCHIVER_CREATE_FAILED' }
    $list = Invoke-A13NativeCaptured -FilePath $archiver -Arguments @('t',$TempArchive)
    if ($list.ExitCode -ne 0) { Stop-Gate 'ARCHIVER_LIST_FAILED' }
    $members = @($list.Output | Where-Object { -not [string]::IsNullOrWhiteSpace($_) } | ForEach-Object { $_.Trim() })
    $expected = @('JWPLC_TFT.cpp.o','TFT_eSPI.cpp.o')
    $delta = @(Compare-Object -ReferenceObject $expected -DifferenceObject $members)
    if ($members.Count -ne 2 -or $delta.Count -ne 0) { Stop-Gate ('ARCHIVE_MEMBER_SET_INVALID:'+($members -join ',')) }
    Push-Location $ExtractDir
    try { $extract = Invoke-A13NativeCaptured -FilePath $archiver -Arguments @('x',$TempArchive) } finally { Pop-Location }
    if ($extract.ExitCode -ne 0) { Stop-Gate 'ARCHIVER_EXTRACT_FAILED' }
    if ((Get-A13Sha256 -Path (Join-Path $ExtractDir 'JWPLC_TFT.cpp.o')) -ne $tftObjSha -or
        (Get-A13Sha256 -Path (Join-Path $ExtractDir 'TFT_eSPI.cpp.o')) -ne $espObjSha) {
        Stop-Gate 'ARCHIVE_MEMBERS_NOT_BIT_IDENTICAL'
    }
    $archiveSha = Get-A13Sha256 -Path $TempArchive
    $archiveBytes = (Get-Item -LiteralPath $TempArchive).Length
    Write-Host ('CANDIDATE_ARCHIVE_SHA256='+$archiveSha)
    Write-Host ('ARCHIVE_MEMBER_PARITY=PASS')

    Compile-Case 'DIRECT_TFT' (Join-Path $Libs 'JWPLC_Display\examples\04.Display_TFT_Direct')
    Compile-Case 'DISPLAY_INTEGRATION' (Join-Path $Libs 'JWPLC_Display\examples\Display_UserUI_Callbacks')
    Compile-Case 'NORMAL_AUTOLOAD' (Join-Path $RepoRoot 'tools\build-speed-benchmark\sketches\01_empty')
    $script:Phase = 'FINAL_AUDIT'
    Clean
    if ((Get-A13Sha256 -Path $TftArchive) -ne $TftOldSha -or
        (Get-A13Sha256 -Path $CoreArchive) -ne $CoreSha -or
        (Get-A13Sha256 -Path $DisplayArchive) -ne $DisplaySha) {
        Stop-Gate 'OFFICIAL_ARCHIVES_CHANGED'
    }
    Finish-Gate -Status 'PASS' -Reason 'TEMP_PRECOMPILED_ARCHIVE_QUALIFIED' -Extra @(
        "HEAD=$head",
        "PRE5_SUMMARY=$proof",
        "CANDIDATE_ARCHIVE=$TempArchive",
        "CANDIDATE_ARCHIVE_SHA256=$archiveSha",
        "CANDIDATE_ARCHIVE_BYTES=$archiveBytes",
        "TFT_OBJECT_SHA256=$tftObjSha",
        "BACKEND_OBJECT_SHA256=$espObjSha",
        'ARCHIVE_MEMBER_COUNT=2',
        'ARCHIVE_MEMBER_PARITY=PASS',
        'COMPILE_CASES_PASS=3',
        'GLOBAL_TFT_ESPI_REQUIRED=NO',
        'OFFICIAL_ARCHIVES_PRESERVED=YES',
        'WORKTREE_FINAL=CLEAN',
        'NEXT=TFT_PRE6_P1_INTEGRATE_SOURCES_AND_REFRESH_ARCHIVE'
    )
}
catch {
    Finish-Gate -Status 'REVIEW' -Reason 'UNEXPECTED_GATE_EXCEPTION' -HarnessFailure 'YES' -Code 90 -Extra @(
        "EXCEPTION_TYPE=$($_.Exception.GetType().FullName)",
        "EXCEPTION_LINE=$($_.InvocationInfo.ScriptLineNumber)",
        "EXCEPTION=$($_.Exception.Message)"
    )
}
finally { Pop-Location }
