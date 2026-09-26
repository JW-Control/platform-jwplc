param()

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"
. (Join-Path $PSScriptRoot "common.ps1")

function Replace-ExactOnce {
    param(
        [Parameter(Mandatory = $true)][string]$Text,
        [Parameter(Mandatory = $true)][string]$Old,
        [Parameter(Mandatory = $true)][string]$New,
        [Parameter(Mandatory = $true)][string]$Label
    )

    $first = $Text.IndexOf($Old, [System.StringComparison]::Ordinal)
    if ($first -lt 0) { throw "A14_NB3E_PATCH_ANCHOR_MISSING=$Label" }

    $second = $Text.IndexOf($Old, $first + $Old.Length, [System.StringComparison]::Ordinal)
    if ($second -ge 0) { throw "A14_NB3E_PATCH_ANCHOR_NOT_UNIQUE=$Label" }

    return $Text.Substring(0, $first) + $New + $Text.Substring($first + $Old.Length)
}

function Get-CppFunctionBlock {
    param(
        [Parameter(Mandatory = $true)][string]$Text,
        [Parameter(Mandatory = $true)][string]$Signature
    )

    $searchOffset = 0
    $start = -1
    $braceStart = -1

    while ($true) {
        $candidate = $Text.IndexOf($Signature, $searchOffset, [System.StringComparison]::Ordinal)
        if ($candidate -lt 0) { break }

        $cursor = $candidate + $Signature.Length
        while ($cursor -lt $Text.Length -and [char]::IsWhiteSpace($Text[$cursor])) {
            ++$cursor
        }

        if ($cursor -lt $Text.Length -and $Text[$cursor] -eq "{") {
            $start = $candidate
            $braceStart = $cursor
            break
        }

        $searchOffset = $candidate + $Signature.Length
    }

    if ($start -lt 0 -or $braceStart -lt 0) {
        throw "A14_NB3E_FUNCTION_DEFINITION_NOT_FOUND=$Signature"
    }

    $depth = 0
    for ($i = $braceStart; $i -lt $Text.Length; ++$i) {
        $ch = $Text[$i]
        if ($ch -eq "{") {
            ++$depth
        }
        elseif ($ch -eq "}") {
            --$depth
            if ($depth -eq 0) {
                return $Text.Substring($start, $i - $start + 1)
            }
        }
    }

    throw "A14_NB3E_FUNCTION_END_NOT_FOUND=$Signature"
}

function Invoke-NB3NativeToLog {
    param(
        [Parameter(Mandatory = $true)][string]$FilePath,
        [Parameter(Mandatory = $true)][string[]]$Arguments,
        [Parameter(Mandatory = $true)][string]$LogPath
    )

    $previousPreference = $ErrorActionPreference
    try {
        $ErrorActionPreference = "Continue"
        & $FilePath @Arguments *> $LogPath
        return [int]$LASTEXITCODE
    }
    finally {
        $ErrorActionPreference = $previousPreference
    }
}

function Assert-NB3RepoEthernetUsed {
    param(
        [Parameter(Mandatory = $true)][string]$LogPath,
        [Parameter(Mandatory = $true)][string]$ExpectedLibraryPath,
        [Parameter(Mandatory = $true)][string]$Label
    )

    $text = [System.IO.File]::ReadAllText($LogPath)
    $used = ($text.IndexOf($ExpectedLibraryPath, [System.StringComparison]::OrdinalIgnoreCase) -ge 0)
    Write-Host "$($Label)_REPO_ETHERNET_LIBRARY_USED=$used"
    if (-not $used) { throw "A14_NB3E_WRONG_ETHERNET_LIBRARY=$Label" }
}

Write-Host "============================================================"
Write-Host " A14 NB3-E1 - HARDEN UDP parsePacket APPLY + COMPILE"
Write-Host "============================================================"

Assert-G2Branch

Write-Host "HEAD=$(Get-G2Head)"
Write-Host "EFFECTIVE_SPI_HZ=$(Get-G2SpiHz)"
Write-Host "UPLOAD=NO"

if ((Get-G2SpiHz) -ne 26000000) { throw "A14_NB3E_SPI_FREQUENCY_MISMATCH" }

$udpCppRelative = "JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/EthernetUdp.cpp"

$expectedDirty = @(
    "JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/Dns.cpp",
    "JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/Dns.h",
    "JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/EthernetClient.cpp",
    "JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/EthernetUdp.cpp",
    "JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/jwplc_ethernet_async_tx.cpp",
    "JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/jwplc_ethernet_async_tx.h",
    "JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/JWPLC_W5x00_Ethernet.h",
    "JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/socket.cpp",
    "JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/utility/w5100.cpp",
    "JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/utility/w5100.h",
    "tools/modbus-tcp-benchmark/firmware/eth14_raw_transport_server/eth14_raw_transport_server.ino"
) | Sort-Object

$dirty = @(Get-G2TrackedDirtyPaths)
Write-Host "TRACKED_DIRTY_BEFORE=$($dirty.Count)"
$dirty | ForEach-Object { Write-Host "DIRTY_BEFORE=$_" }

if ($dirty.Count -ne $expectedDirty.Count) { throw "A14_NB3E_DIRTY_COUNT_INVALID=$($dirty.Count)" }
for ($i = 0; $i -lt $expectedDirty.Count; ++$i) {
    if ($dirty[$i] -ne $expectedDirty[$i]) { throw "A14_NB3E_DIRTY_PATH_INVALID=$($dirty[$i])" }
}

$staged = @(& git -C $script:G2RepoRoot diff --cached --name-only)
if ($LASTEXITCODE -ne 0 -or $staged.Count -ne 0) { throw "A14_NB3E_INDEX_NOT_CLEAN" }
Write-Host "STAGED_COUNT_BEFORE=0"

Assert-G2ProtectedArtifacts

$udpCppPath = Get-G2Path $udpCppRelative
$udpCpp = [System.IO.File]::ReadAllText($udpCppPath)
$preHash = Get-G2Sha256 $udpCppRelative
Write-Host "UDP_CPP_SHA256_BEFORE=$preHash"

$oldBlock = @'
	// discard any remaining bytes in the last packet
	while (_remaining) {
		// could this fail (loop endlessly) if _remaining > 0 and recv in read fails?
		// should only occur if recv fails after telling us the data is there, lets
		// hope the w5100 always behaves :)
		read((uint8_t *)NULL, _remaining);
	}

'@

$newBlock = @'
	// Discard remaining bytes from the previous packet cooperatively.
	// Perform at most one drain attempt per parsePacket() call. If recv fails
	// or returns only part of the payload, leave _remaining intact for the
	// next call instead of risking an endless loop here.
	if (_remaining > 0) {
		const int drained = read((uint8_t *)NULL, _remaining);

		if (drained <= 0 || _remaining > 0) {
			return 0;
		}
	}

'@

$hasOld = ($udpCpp.IndexOf($oldBlock, [System.StringComparison]::Ordinal) -ge 0)
$hasNew = ($udpCpp.IndexOf($newBlock, [System.StringComparison]::Ordinal) -ge 0)

Write-Host "NB3_E1_OLD_PARSE_DRAIN_PRESENT=$hasOld"
Write-Host "NB3_E1_NEW_PARSE_DRAIN_PRESENT=$hasNew"

if ($hasOld -and -not $hasNew) {
    if ($preHash -ne "90F37F3C3E7E4C0F8E0B4982E515AFCFE4A3A480084FBA3E6DCDF0D0DA58CAA1") {
        throw "A14_NB3E_PRE_HASH_MISMATCH=$preHash"
    }

    $udpCpp = Replace-ExactOnce -Text $udpCpp -Old $oldBlock -New $newBlock -Label "UDP_PARSE_REMAINING_DRAIN"
    $utf8NoBom = New-Object -TypeName System.Text.UTF8Encoding -ArgumentList $false
    [System.IO.File]::WriteAllText($udpCppPath, $udpCpp, $utf8NoBom)
    Write-Host "NB3_E1_PATCH_APPLICATION=APPLIED"
}
elseif (-not $hasOld -and $hasNew) {
    Write-Host "NB3_E1_RESUME_PATCHED_STATE=YES"
    Write-Host "NB3_E1_PATCH_APPLICATION=SKIPPED_ALREADY_APPLIED"
}
else {
    throw "A14_NB3E_PARSE_DRAIN_STATE_AMBIGUOUS old=$hasOld new=$hasNew"
}

$previousPreference = $ErrorActionPreference
try {
    $ErrorActionPreference = "Continue"
    $diffCheck = @(& git -C $script:G2RepoRoot diff --check 2>&1)
    $diffCheckExit = [int]$LASTEXITCODE
}
finally {
    $ErrorActionPreference = $previousPreference
}

Write-Host "GIT_DIFF_CHECK_EXIT=$diffCheckExit"
if ($diffCheckExit -ne 0) {
    $diffCheck | ForEach-Object { Write-Host $_ }
    throw "A14_NB3E_GIT_DIFF_CHECK_FAILED"
}
Write-Host "GIT_DIFF_CHECK=PASS"

$udpVerify = [System.IO.File]::ReadAllText($udpCppPath)
$parsePacketBlock = Get-CppFunctionBlock -Text $udpVerify -Signature "int EthernetUDP::parsePacket()"

$oldLoopCount = ([regex]::Matches($parsePacketBlock, 'while\s*\(\s*_remaining\s*\)')).Count
$newGuardCount = ([regex]::Matches($parsePacketBlock, 'if\s*\(\s*_remaining\s*>\s*0\s*\)\s*\{[\s\S]*?const int drained')).Count
$returnGuardCount = ([regex]::Matches($parsePacketBlock, 'if\s*\(\s*drained\s*<=\s*0\s*\|\|\s*_remaining\s*>\s*0\s*\)\s*\{[\s\S]*?return 0;')).Count

Write-Host "UDP_PARSE_VERIFY_SCOPE=PARSE_PACKET_FUNCTION_BODY"
Write-Host "UDP_PARSE_REMAINING_WHILE_COUNT=$oldLoopCount"
Write-Host "UDP_PARSE_SINGLE_DRAIN_GUARD_COUNT=$newGuardCount"
Write-Host "UDP_PARSE_RETRY_RETURN_GUARD_COUNT=$returnGuardCount"

if ($oldLoopCount -ne 0) { throw "A14_NB3E_UNBOUNDED_PARSE_LOOP_REMAINS" }
if ($newGuardCount -ne 1 -or $returnGuardCount -ne 1) { throw "A14_NB3E_PARSE_GUARD_CONTRACT_INVALID" }

$postHash = Get-G2Sha256 $udpCppRelative
Write-Host "UDP_CPP_SHA256_AFTER=$postHash"

$dirtyAfter = @(Get-G2TrackedDirtyPaths)
$stagedAfter = @(& git -C $script:G2RepoRoot diff --cached --name-only)

Write-Host "TRACKED_DIRTY_AFTER_PATCH=$($dirtyAfter.Count)"
Write-Host "STAGED_COUNT_AFTER_PATCH=$($stagedAfter.Count)"

if ($dirtyAfter.Count -ne $expectedDirty.Count -or $stagedAfter.Count -ne 0) {
    throw "A14_NB3E_POST_PATCH_WORKTREE_INVALID"
}

$arduinoCli = "C:\Program Files\Arduino PLC IDE Tools\arduino-cli.exe"
if (-not (Test-Path -LiteralPath $arduinoCli)) { throw "A14_NB3E_ARDUINO_CLI_NOT_FOUND=$arduinoCli" }

$fqbn = "jwplc_local:esp32:jwplcbasic"
$librariesRoot = Get-G2Path "JWPLC/2.1.0/libraries"
$expectedEthernetLibrary = Join-Path $librariesRoot "JWPLC_Ethernet"
$rawFirmwarePath = Get-G2Path $script:G2RawFirmwareRelative
$rawSketchDir = Split-Path -Parent $rawFirmwarePath

$timestamp = Get-Date -Format "yyyyMMdd_HHmmss"
$tempRoot = Join-Path $env:TEMP ("jwplc_a14_nb3e1_parse_packet_{0}" -f $timestamp)
$rawBuildPath = Join-Path $tempRoot "raw_build"
$probeSketchDir = Join-Path $tempRoot "api_probe"
$probeBuildPath = Join-Path $tempRoot "probe_build"
$rawCompileLog = Join-Path $tempRoot "raw_compile.log"
$probeCompileLog = Join-Path $tempRoot "probe_compile.log"

New-Item -ItemType Directory -Force -Path $rawBuildPath | Out-Null
New-Item -ItemType Directory -Force -Path $probeSketchDir | Out-Null
New-Item -ItemType Directory -Force -Path $probeBuildPath | Out-Null

$probeSketchName = Split-Path -Leaf $probeSketchDir
$probeSketchPath = Join-Path $probeSketchDir ($probeSketchName + ".ino")
$utf8NoBom = New-Object -TypeName System.Text.UTF8Encoding -ArgumentList $false

$probeSketch = @'
#include <JWPLC_Ethernet.h>

volatile int nb3eSink = 0;

static void compileOnlyParsePacketProbe()
{
    EthernetUDP udp;
    nb3eSink += udp.parsePacket();
    nb3eSink += udp.available();

    uint8_t buffer[8] = {};
    nb3eSink += udp.read(buffer, sizeof(buffer));
}

void setup()
{
    if (false)
    {
        compileOnlyParsePacketProbe();
    }
}

void loop()
{
}
'@

[System.IO.File]::WriteAllText($probeSketchPath, $probeSketch, $utf8NoBom)

Write-Host "API_PROBE_SKETCH_FOLDER=$probeSketchName"
Write-Host "API_PROBE_MAIN_BASENAME=$([System.IO.Path]::GetFileNameWithoutExtension($probeSketchPath))"
Write-Host "UPLOAD=NO"

Write-Host ""
Write-Host "=== COMPILE RAW CANDIDATE ==="

$rawCompileArgs = @(
    "compile",
    "--fqbn", $fqbn,
    "--build-path", $rawBuildPath,
    "--libraries", $librariesRoot,
    $rawSketchDir
)

$rawCompileExit = Invoke-NB3NativeToLog -FilePath $arduinoCli -Arguments $rawCompileArgs -LogPath $rawCompileLog
Write-Host "RAW_COMPILE_EXIT=$rawCompileExit"
Write-Host "RAW_COMPILE_LOG=$rawCompileLog"

if ($rawCompileExit -ne 0) {
    Invoke-G2CompileFinishedSound -Success $false
    Get-Content -LiteralPath $rawCompileLog -Tail 140 | ForEach-Object { Write-Host $_ }
    throw "A14_NB3E_RAW_COMPILE_FAILED"
}

Assert-NB3RepoEthernetUsed -LogPath $rawCompileLog -ExpectedLibraryPath $expectedEthernetLibrary -Label "RAW"

Write-Host ""
Write-Host "=== COMPILE UDP PUBLIC API PROBE ==="

$probeCompileArgs = @(
    "compile",
    "--fqbn", $fqbn,
    "--build-path", $probeBuildPath,
    "--libraries", $librariesRoot,
    $probeSketchDir
)

$probeCompileExit = Invoke-NB3NativeToLog -FilePath $arduinoCli -Arguments $probeCompileArgs -LogPath $probeCompileLog
Write-Host "API_PROBE_COMPILE_EXIT=$probeCompileExit"
Write-Host "API_PROBE_COMPILE_LOG=$probeCompileLog"

if ($probeCompileExit -ne 0) {
    Invoke-G2CompileFinishedSound -Success $false
    Get-Content -LiteralPath $probeCompileLog -Tail 140 | ForEach-Object { Write-Host $_ }
    throw "A14_NB3E_API_PROBE_COMPILE_FAILED"
}

Assert-NB3RepoEthernetUsed -LogPath $probeCompileLog -ExpectedLibraryPath $expectedEthernetLibrary -Label "API_PROBE"

$rawBinCount = @(Get-ChildItem -LiteralPath $rawBuildPath -Recurse -File -Filter "*.bin").Count
$probeBinCount = @(Get-ChildItem -LiteralPath $probeBuildPath -Recurse -File -Filter "*.bin").Count

Write-Host "RAW_BIN_COUNT=$rawBinCount"
Write-Host "API_PROBE_BIN_COUNT=$probeBinCount"

if ($rawBinCount -lt 1 -or $probeBinCount -lt 1) {
    Invoke-G2CompileFinishedSound -Success $false
    throw "A14_NB3E_BIN_OUTPUT_MISSING"
}

Invoke-G2CompileFinishedSound -Success $true
Write-Host "COMPILE_FINISHED_SOUND=PLAYED"

Assert-G2ProtectedArtifacts

$dirtyFinal = @(Get-G2TrackedDirtyPaths)
$stagedFinal = @(& git -C $script:G2RepoRoot diff --cached --name-only)

Write-Host "TRACKED_DIRTY_COUNT_FINAL=$($dirtyFinal.Count)"
$dirtyFinal | ForEach-Object { Write-Host "DIRTY_FINAL=$_" }
Write-Host "STAGED_COUNT_FINAL=$($stagedFinal.Count)"

if ($dirtyFinal.Count -ne $expectedDirty.Count -or $stagedFinal.Count -ne 0) {
    throw "A14_NB3E_FINAL_WORKTREE_INVALID"
}

for ($i = 0; $i -lt $expectedDirty.Count; ++$i) {
    if ($dirtyFinal[$i] -ne $expectedDirty[$i]) {
        throw "A14_NB3E_FINAL_DIRTY_PATH_INVALID=$($dirtyFinal[$i])"
    }
}

Write-Host "NB3_UDP_PARSE_UNBOUNDED_REMAINING_LOOP=REMOVED"
Write-Host "NB3_UDP_PARSE_VERIFICATION=FUNCTION_SCOPED"
Write-Host "NB3_UDP_PARSE_DRAIN_ATTEMPTS_PER_CALL=ONE"
Write-Host "NB3_UDP_PARSE_RECV_FAILURE=RETURN_ZERO_RETRY_NEXT_CALL"
Write-Host "NB3_UDP_PARSE_PARTIAL_DRAIN=RETURN_ZERO_RETRY_NEXT_CALL"
Write-Host "NB3_UDP_PARSE_PUBLIC_API=PRESERVED"
Write-Host "NB3_UDP_RX_THROUGHPUT_OPTIMIZATION=NOT_IN_THIS_GATE"
Write-Host "NB3_SPI_FREQUENCY_CHANGE=NO"
Write-Host "NB3_UPLOAD=NO"
Write-Host "A14_NB3_E1_UDP_PARSE_PACKET_HARDEN_APPLY_COMPILE=PASS"
