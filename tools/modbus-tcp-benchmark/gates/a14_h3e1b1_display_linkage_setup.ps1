param(
    [string]$MasterPort = "COM14",
    [string]$SlavePort = "COM4",
    [ValidateSet("ARCHIVE", "SOURCE")]
    [string]$DisplayLinkage = "ARCHIVE",
    [switch]$SetupOnly,
    [switch]$PreflightOnly,
    [switch]$AllowDirtyCoreCandidate
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"
. (Join-Path $PSScriptRoot "common.ps1")

function Invoke-NativeToLog {
    param([string]$FilePath, [string[]]$Arguments, [string]$LogPath)

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

Write-Host "============================================================"
Write-Host " A14 H3E.1B.1 - DISPLAY LINKAGE SETUP"
Write-Host "============================================================"

Assert-G2Branch

$expectedCoreHash = "4BFF8C8241DA2E8BD0E1BBA99835ADDF91B9085A05C4DFBD05C339B824794566"
$expectedArchiveHash = "444BE3A04079A579252B2737FE6070E00ADCA949FD176880588FE69561B2A79F"
$expectedDisplayArchiveHash = "2974D42C847C1B7C7AB3A7B74DA42E2F17969FB852B47A8D434F57F70DA924AF"

$archiveRelative = "JWPLC/2.1.0/libraries/JWPLC_ModbusRTU/src/esp32/libJWPLC_ModbusRTU.a"
$archivePath = Get-G2Path $archiveRelative

$displayArchiveRelative = "JWPLC/2.1.0/libraries/JWPLC_Display/src/esp32/libJWPLC_Display.a"
$displayArchivePath = Get-G2Path $displayArchiveRelative

$coreRelative = $script:G2CoreRelative

[string[]]$dirty = @(Get-G2TrackedDirtyPaths)
[string[]]$staged = @(& git -C $script:G2RepoRoot diff --cached --name-only)

[string[]]$expectedDirty = @(
    $coreRelative.Replace("\", "/")
)
[string[]]$normalizedDirty = @(
    $dirty |
        ForEach-Object { $_.Replace("\", "/") } |
        Sort-Object
)

if ($AllowDirtyCoreCandidate) {
    if ($normalizedDirty.Count -ne $expectedDirty.Count) {
        $normalizedDirty | ForEach-Object { Write-Host "DIRTY=$_" }
        throw "H3E1B1_SETUP_DIRTY_COUNT_INVALID"
    }

    for ($i = 0; $i -lt $expectedDirty.Count; ++$i) {
        if ($normalizedDirty[$i] -ne $expectedDirty[$i]) {
            throw "H3E1B1_SETUP_DIRTY_SCOPE_INVALID"
        }
    }
}
elseif ($dirty.Count -ne 0) {
    throw "H3E1B1_SETUP_TREE_NOT_CLEAN"
}

if ($staged.Count -ne 0) {
    throw "H3E1B1_SETUP_INDEX_NOT_CLEAN"
}

$coreHashBefore = Get-G2Sha256 $coreRelative
if ($coreHashBefore -ne $expectedCoreHash) {
    throw "H3E1B1_SETUP_CORE_HASH_INVALID"
}

if (-not (Test-Path -LiteralPath $archivePath)) {
    throw "H3E1B1_SETUP_MODBUS_RTU_ARCHIVE_MISSING_AT_ENTRY"
}

$archiveHashBefore = Get-G2Sha256 $archiveRelative
if ($archiveHashBefore -ne $expectedArchiveHash) {
    throw "H3E1B1_SETUP_MODBUS_RTU_ARCHIVE_HASH_INVALID"
}

if (-not (Test-Path -LiteralPath $displayArchivePath)) {
    throw "H3E1B1_SETUP_DISPLAY_ARCHIVE_MISSING_AT_ENTRY"
}

$displayArchiveHashBefore = Get-G2Sha256 $displayArchiveRelative
if ($displayArchiveHashBefore -ne $expectedDisplayArchiveHash) {
    throw "H3E1B1_SETUP_DISPLAY_ARCHIVE_HASH_INVALID"
}

$ports = @([System.IO.Ports.SerialPort]::GetPortNames() | Sort-Object)
Write-Host "COM_PORTS=$($ports -join ',')"

if ($MasterPort -notin $ports) {
    throw "H3E1B1_SETUP_MASTER_PORT_MISSING"
}
if ($SlavePort -notin $ports) {
    throw "H3E1B1_SETUP_SLAVE_PORT_MISSING"
}

$arduinoCli = "C:\Program Files\Arduino PLC IDE Tools\arduino-cli.exe"
if (-not (Test-Path -LiteralPath $arduinoCli)) {
    throw "H3E1B1_SETUP_ARDUINO_CLI_MISSING"
}

$pythonCommand = Get-Command python.exe -ErrorAction SilentlyContinue
if ($null -eq $pythonCommand) {
    $pythonCommand = Get-Command python -ErrorAction SilentlyContinue
}
if ($null -eq $pythonCommand) {
    throw "H3E1B1_SETUP_PYTHON_MISSING"
}
$pythonExe = $pythonCommand.Source

$platformRoot = Get-G2Path "JWPLC/2.1.0"
$repoLibrariesRoot = Get-G2Path "JWPLC/2.1.0/libraries"

$installedPlatformRoot = Join-Path $env:LOCALAPPDATA "Arduino15\packages\jwplc_local\hardware\esp32\2.1.0-dev"
if (-not (Test-Path -LiteralPath $installedPlatformRoot)) {
    throw "H3E1B1_SETUP_INSTALLED_PLATFORM_MISSING=$installedPlatformRoot"
}

$boardsLocalPath = Join-Path $installedPlatformRoot "boards.local.txt"

$repoCoreRoot = Join-Path $platformRoot "cores\jwcontrol"
$installedCoreRoot = Join-Path $installedPlatformRoot "cores\jwcontrol"

$repoCoreMain = Join-Path $repoCoreRoot "main.cpp"
$repoCoreHeader = Join-Path $repoCoreRoot "jwplc_h3e0b_profile.h"

foreach ($requiredCoreSource in @(
    $repoCoreRoot,
    $repoCoreMain,
    $repoCoreHeader,
    $installedCoreRoot
)) {
    if (-not (Test-Path -LiteralPath $requiredCoreSource)) {
        throw "H3E1B1_SETUP_CORE_SOURCE_MISSING=$requiredCoreSource"
    }
}

function Get-ReparseAncestorInfo {
    param(
        [Parameter(Mandatory = $true)][string]$StartPath,
        [Parameter(Mandatory = $true)][string]$StopPath
    )

    $current = Get-Item -LiteralPath $StartPath -Force
    $stopItem = Get-Item -LiteralPath $StopPath -Force
    $stopFull = $stopItem.FullName.TrimEnd('\')

    while ($null -ne $current) {
        $currentFull = $current.FullName.TrimEnd('\')
        $isReparse = (($current.Attributes -band [IO.FileAttributes]::ReparsePoint) -ne 0)

        if ($isReparse) {
            $targetText = ""
            $linkTypeText = ""

            if ($current.PSObject.Properties.Name -contains "Target") {
                $targetValue = $current.Target
                if ($null -ne $targetValue) {
                    $targetText = (@($targetValue) -join ";")
                }
            }

            if ($current.PSObject.Properties.Name -contains "LinkType") {
                $linkTypeText = [string]$current.LinkType
            }

            [PSCustomObject]@{
                Path = [string]$current.FullName
                LinkType = [string]$linkTypeText
                Target = [string]$targetText
            }
        }

        if ($currentFull.Equals($stopFull, [StringComparison]::OrdinalIgnoreCase)) {
            break
        }

        $current = $current.Parent
    }
}
$packageStopRoot = Join-Path $env:LOCALAPPDATA "Arduino15\packages\jwplc_local"
[object[]]$reparseAncestors = @(Get-ReparseAncestorInfo -StartPath $installedPlatformRoot -StopPath $packageStopRoot)

$installedCoreMain = Join-Path $installedCoreRoot "main.cpp"
$installedCoreHeader = Join-Path $installedCoreRoot "jwplc_h3e0b_profile.h"
$coreSourceIdentityPass = $false

if ((Test-Path -LiteralPath $installedCoreMain) -and (Test-Path -LiteralPath $installedCoreHeader)) {
    $repoCoreMainHash = (Get-FileHash -LiteralPath $repoCoreMain -Algorithm SHA256).Hash
    $installedCoreMainHash = (Get-FileHash -LiteralPath $installedCoreMain -Algorithm SHA256).Hash
    $repoCoreHeaderHash = (Get-FileHash -LiteralPath $repoCoreHeader -Algorithm SHA256).Hash
    $installedCoreHeaderHash = (Get-FileHash -LiteralPath $installedCoreHeader -Algorithm SHA256).Hash
    $coreSourceIdentityPass = $repoCoreMainHash -eq $installedCoreMainHash -and $repoCoreHeaderHash -eq $installedCoreHeaderHash
}

$hasReparseAncestor = $reparseAncestors.Count -gt 0

if ($hasReparseAncestor -and -not $coreSourceIdentityPass) {
    Write-Host "CORE_SOURCE_IDENTITY_PASS=False"
    foreach ($link in $reparseAncestors) {
        Write-Host "REPARSE_PATH=$($link.Path)"
        Write-Host "REPARSE_LINKTYPE=$($link.LinkType)"
        Write-Host "REPARSE_TARGET=$($link.Target)"
    }
    throw "H3E1B1_SETUP_LINKED_PLATFORM_SOURCE_MISMATCH"
}

if ($hasReparseAncestor -and $coreSourceIdentityPass) {
    $coreSourceStrategy = "SHARED_LINK_NO_OVERLAY"
} else {
    $coreSourceStrategy = "TEMP_INSTALL_OVERLAY_BACKUP_RESTORE"
}

Write-Host "INSTALLED_PLATFORM_ROOT=$installedPlatformRoot"
Write-Host "REPARSE_ANCESTOR_COUNT=$($reparseAncestors.Count)"
foreach ($link in $reparseAncestors) {
    Write-Host "REPARSE_PATH=$($link.Path)"
    Write-Host "REPARSE_LINKTYPE=$($link.LinkType)"
    Write-Host "REPARSE_TARGET=$($link.Target)"
}
Write-Host "CORE_SOURCE_IDENTITY_PASS=$coreSourceIdentityPass"
Write-Host "CORE_SOURCE_STRATEGY=$coreSourceStrategy"
$displaySourceRoot = Get-G2Path "JWPLC/2.1.0/libraries/JWPLC_Display/src"
$displayMainSource = Join-Path $displaySourceRoot "JWPLC_Display.cpp"
$displayUiSource = Join-Path $displaySourceRoot "JWPLC_UI.cpp"
$displayUiApiSource = Join-Path $displaySourceRoot "JWPLC_UI_API.cpp"
$displayPagesSource = Join-Path $displaySourceRoot "JWPLC_UI_Pages.cpp"
$displayProfileSource = Join-Path $displaySourceRoot "JWPLC_Display_H3E1_Profile.cpp"

foreach ($requiredDisplaySource in @(
    $displayMainSource,
    $displayUiSource,
    $displayUiApiSource,
    $displayPagesSource,
    $displayProfileSource
)) {
    if (-not (Test-Path -LiteralPath $requiredDisplaySource)) {
        throw "H3E1B1_SETUP_DISPLAY_SOURCE_MISSING=$requiredDisplaySource"
    }
}

Write-Host "DISPLAY_LINKAGE=$DisplayLinkage"
Write-Host "DISPLAY_ARCHIVE_SHA256=$displayArchiveHashBefore"
Write-Host "DISPLAY_TFT_SPI_HZ=80000000"

$masterDir = Get-G2Path "tools/modbus-tcp-benchmark/firmware/a14_h3e0b_core_profiler_master"
$masterSketch = Join-Path $masterDir "a14_h3e0b_core_profiler_master.ino"
$slaveDir = Get-G2Path "tools/modbus-tcp-benchmark/firmware/a14_p5_rtu_slave"
$slaveSketch = Join-Path $slaveDir "a14_p5_rtu_slave.ino"
$resolver = Join-Path $PSScriptRoot "a14_p5_resolve_full_runtime_ip.py"

foreach ($required in @($masterSketch,$slaveSketch,$resolver)) {
    if (-not (Test-Path -LiteralPath $required)) {
        throw "H3E1B1_SETUP_REQUIRED_FILE_MISSING=$required"
    }
}

$masterText = [IO.File]::ReadAllText($masterSketch)
if (-not $masterText.Contains("H3E0B_PROFILER=ENABLED")) {
    throw "H3E1B1_SETUP_MASTER_PROFILER_CONTRACT_MISSING"
}
if (-not $masterText.Contains("jwplcH3E0BProfilerEnabled")) {
    throw "H3E1B1_SETUP_MASTER_ENABLE_HOOK_MISSING"
}

$boardsHadOriginal = Test-Path -LiteralPath $boardsLocalPath
$boardsBytes = $null
$boardsText = ""
if ($boardsHadOriginal) {
    $boardsBytes = [IO.File]::ReadAllBytes($boardsLocalPath)
    $boardsText = [IO.File]::ReadAllText($boardsLocalPath)
}

function Restore-BoardsLocal {
    if ($boardsHadOriginal) {
        [IO.File]::WriteAllBytes($boardsLocalPath, $boardsBytes)
    }
    elseif (Test-Path -LiteralPath $boardsLocalPath) {
        Remove-Item -LiteralPath $boardsLocalPath -Force
    }
}

function Enable-SourceCoreOverride {
    $base = $boardsText

    $base = [regex]::Replace(
        $base,
        '(?ms)^# BEGIN JWPLC_SOURCE_CORE_BUILD\r?\n.*?^# END JWPLC_SOURCE_CORE_BUILD\r?\n?',
        ''
    ).TrimEnd()

    $base = [regex]::Replace(
        $base,
        '(?ms)^# BEGIN JWPLC_P2_PRECOMPILED_CORE\r?\n.*?^# END JWPLC_P2_PRECOMPILED_CORE\r?\n?',
        ''
    ).TrimEnd()

    $lines = New-Object System.Collections.Generic.List[string]
    if (-not [string]::IsNullOrWhiteSpace($base)) {
        $lines.Add($base)
        $lines.Add("")
    }

    $lines.Add("# BEGIN JWPLC_SOURCE_CORE_BUILD")
    $lines.Add("jwplcbasic.build.core=jwcontrol")
    $lines.Add("jwplcbasic.build.extra_libs=")
    $lines.Add("# END JWPLC_SOURCE_CORE_BUILD")

    $utf8 = New-Object System.Text.UTF8Encoding($false)
    [IO.File]::WriteAllText(
        $boardsLocalPath,
        (($lines -join [Environment]::NewLine) + [Environment]::NewLine),
        $utf8
    )
}

function Backup-And-Overlay-InstalledCore {
    param(
        [Parameter(Mandatory = $true)][string]$BackupRoot
    )

    if ($coreSourceStrategy -eq "SHARED_LINK_NO_OVERLAY") {
        Write-Host "INSTALLED_CORE_OVERLAY=NOT_REQUIRED_SHARED_LINK"
        return
    }

    if (Test-Path -LiteralPath $BackupRoot) {
        Remove-Item -LiteralPath $BackupRoot -Recurse -Force
    }

    New-Item -ItemType Directory -Force -Path $BackupRoot | Out-Null

    $backupCoreRoot = Join-Path $BackupRoot "jwcontrol_original"
    Copy-Item -LiteralPath $installedCoreRoot -Destination $backupCoreRoot -Recurse -Force

    Remove-Item -LiteralPath $installedCoreRoot -Recurse -Force
    Copy-Item -LiteralPath $repoCoreRoot -Destination $installedCoreRoot -Recurse -Force

    $overlayMain = Join-Path $installedCoreRoot "main.cpp"
    $overlayHeader = Join-Path $installedCoreRoot "jwplc_h3e0b_profile.h"

    if (
        -not (Test-Path -LiteralPath $overlayMain) -or
        -not (Test-Path -LiteralPath $overlayHeader)
    ) {
        throw "H3E1B1_SETUP_INSTALLED_CORE_OVERLAY_INCOMPLETE"
    }

    $repoMainHash =
        (Get-FileHash -LiteralPath $repoCoreMain -Algorithm SHA256).Hash
    $overlayMainHash =
        (Get-FileHash -LiteralPath $overlayMain -Algorithm SHA256).Hash
    $repoHeaderHash =
        (Get-FileHash -LiteralPath $repoCoreHeader -Algorithm SHA256).Hash
    $overlayHeaderHash =
        (Get-FileHash -LiteralPath $overlayHeader -Algorithm SHA256).Hash

    if (
        $repoMainHash -ne $overlayMainHash -or
        $repoHeaderHash -ne $overlayHeaderHash
    ) {
        throw "H3E1B1_SETUP_INSTALLED_CORE_OVERLAY_HASH_MISMATCH"
    }

    Write-Host "INSTALLED_CORE_OVERLAY=APPLIED"
    Write-Host "INSTALLED_CORE_OVERLAY_IDENTITY=PASS"
}

function Restore-InstalledCore {
    param(
        [Parameter(Mandatory = $true)][string]$BackupRoot
    )

    $backupCoreRoot = Join-Path $BackupRoot "jwcontrol_original"

    if (-not (Test-Path -LiteralPath $backupCoreRoot)) {
        throw "H3E1B1_SETUP_INSTALLED_CORE_BACKUP_MISSING"
    }

    if (Test-Path -LiteralPath $installedCoreRoot) {
        Remove-Item -LiteralPath $installedCoreRoot -Recurse -Force
    }

    Move-Item -LiteralPath $backupCoreRoot -Destination $installedCoreRoot

    $restoredMain = Join-Path $installedCoreRoot "main.cpp"
    if (-not (Test-Path -LiteralPath $restoredMain)) {
        throw "H3E1B1_SETUP_INSTALLED_CORE_RESTORE_FAILED"
    }

    Write-Host "INSTALLED_CORE_RESTORE=PASS"
}

Write-Host "MODBUS_RTU_ARCHIVE_SHA256=$archiveHashBefore"
Write-Host "MODBUS_RTU_SOURCE_POLICY=HIDE_COMPILE_RESTORE"
Write-Host "DISPLAY_LINKAGE_POLICY=$DisplayLinkage"
Write-Host "DISPLAY_ARCHIVE_POLICY=$(
    if ($DisplayLinkage -eq 'ARCHIVE') {
        'KEEP_ARCHIVE_REQUIRE_NO_SOURCE_OBJECTS'
    }
    else {
        'HIDE_COMPILE_SOURCE_RESTORE'
    }
)"
Write-Host "CORE_SOURCE_POLICY=$coreSourceStrategy"
Write-Host "CORE_SOURCE_SHARED_LINK=$($coreSourceStrategy -eq 'SHARED_LINK_NO_OVERLAY')"
Write-Host "PREFLIGHT_MUTATES_INSTALLED_PACKAGE=NO"
Write-Host "PREFLIGHT_COMPILES=NO"
Write-Host "PREFLIGHT_UPLOADS=NO"
Write-Host "H3E1B1_STATIC_PREFLIGHT=PASS"

if ($PreflightOnly) {
    Write-Output "A14_H3E1B1_PREFLIGHT_ONLY=PASS"
    return
}

$timestamp = Get-Date -Format "yyyyMMdd_HHmmss"
$tempRoot = Join-Path $env:TEMP ("jwplc_a14_h3e1b1_setup_{0}" -f $timestamp)
$masterBuild = Join-Path $tempRoot "build_master_source_core"
$slaveBuild = Join-Path $tempRoot "build_slave"
$masterCompileLog = Join-Path $tempRoot "compile_master_source_core.log"
$slaveCompileLog = Join-Path $tempRoot "compile_slave.log"
$masterUploadLog = Join-Path $tempRoot "upload_master.log"
$slaveUploadLog = Join-Path $tempRoot "upload_slave.log"
$resolverLog = Join-Path $tempRoot "resolver.log"
$archiveBackup = Join-Path $tempRoot "libJWPLC_ModbusRTU.before.a"
$displayArchiveBackup = Join-Path $tempRoot "libJWPLC_Display.before.a"
$installedCoreBackupRoot = Join-Path $tempRoot "installed_core_backup"

New-Item -ItemType Directory -Force -Path $masterBuild | Out-Null
New-Item -ItemType Directory -Force -Path $slaveBuild | Out-Null

$fqbn = "jwplc_local:esp32:jwplcbasic"

Write-Host "HEAD=$(Get-G2Head)"
Write-Host "CORE_A_SHA256_BEFORE=$coreHashBefore"
Write-Host "MASTER_PORT=$MasterPort"
Write-Host "SLAVE_PORT=$SlavePort"
Write-Host "TEMP_ROOT=$tempRoot"
Write-Host "H3E1B1_CORE_MODE=SOURCE_TEMPORARY"
Write-Host "H3E1B1_PRECOMPILED_CORE_MUTATION=NO"

Write-Host ""
Write-Host "=== FORCE MODBUS RTU SOURCE + DISPLAY $DisplayLinkage ==="

Copy-Item -LiteralPath $archivePath -Destination $archiveBackup -Force
Copy-Item -LiteralPath $displayArchivePath -Destination $displayArchiveBackup -Force

$archiveHidden = $false
$displayArchiveHidden = $false
$installedCoreOverlayStarted = $false
$masterExit = -1
$slaveExit = -1

try {
    Remove-Item -LiteralPath $archivePath -Force
    $archiveHidden = $true

    if ($DisplayLinkage -eq "SOURCE") {
        Remove-Item -LiteralPath $displayArchivePath -Force
        $displayArchiveHidden = $true
        Write-Host "DISPLAY_ARCHIVE_HIDDEN=YES"
    }
    else {
        Write-Host "DISPLAY_ARCHIVE_HIDDEN=NO"
    }

    Write-Host "MODBUS_RTU_ARCHIVE_HIDDEN=YES"

    Write-Host ""
    Write-Host "=== COMPILE SLAVE FROM MODBUS RTU SOURCE ==="

    $slaveArgs = @(
        "compile",
        "--fqbn", $fqbn,
        "--build-path", $slaveBuild,
        "--libraries", $repoLibrariesRoot,
        $slaveDir
    )

    $slaveExit = Invoke-NativeToLog -FilePath $arduinoCli -Arguments $slaveArgs -LogPath $slaveCompileLog
    Write-Host "SLAVE_COMPILE_EXIT=$slaveExit"

    if ($slaveExit -ne 0) {
        Get-Content -LiteralPath $slaveCompileLog -Tail 60 | ForEach-Object { Write-Host $_ }
        throw "H3E1B1_SETUP_SLAVE_COMPILE_FAILED"
    }

    Write-Host ""
    Write-Host "=== COMPILE MASTER CORE SOURCE + MODBUS RTU SOURCE + DISPLAY $DisplayLinkage ==="

    if ($coreSourceStrategy -eq "TEMP_INSTALL_OVERLAY_BACKUP_RESTORE") {
        $installedCoreOverlayStarted = $true
    }

    Backup-And-Overlay-InstalledCore -BackupRoot $installedCoreBackupRoot
    Enable-SourceCoreOverride

    $masterArgs = @(
        "compile",
        "--fqbn", $fqbn,
        "--build-path", $masterBuild,
        "--libraries", $repoLibrariesRoot,
        $masterDir
    )

    $masterExit = Invoke-NativeToLog -FilePath $arduinoCli -Arguments $masterArgs -LogPath $masterCompileLog
    Write-Host "MASTER_COMPILE_EXIT=$masterExit"

    if ($masterExit -ne 0) {
        Get-Content -LiteralPath $masterCompileLog -Tail 60 | ForEach-Object { Write-Host $_ }
        throw "H3E1B1_SETUP_MASTER_COMPILE_FAILED"
    }
}
finally {
    $restoreErrors = New-Object System.Collections.Generic.List[string]

    try {
        Restore-BoardsLocal
    }
    catch {
        $restoreErrors.Add("boards.local.txt: $($_.Exception.Message)")
    }

    try {
        if ($installedCoreOverlayStarted) {
            Restore-InstalledCore -BackupRoot $installedCoreBackupRoot
        }
    }
    catch {
        $restoreErrors.Add("installed core: $($_.Exception.Message)")
    }

    try {
        if ($archiveHidden) {
            Copy-Item -LiteralPath $archiveBackup -Destination $archivePath -Force
        }
    }
    catch {
        $restoreErrors.Add("ModbusRTU archive: $($_.Exception.Message)")
    }

    try {
        if ($displayArchiveHidden) {
            Copy-Item -LiteralPath $displayArchiveBackup -Destination $displayArchivePath -Force
        }
    }
    catch {
        $restoreErrors.Add("Display archive: $($_.Exception.Message)")
    }

    if ($restoreErrors.Count -gt 0) {
        $restoreErrors | ForEach-Object { Write-Host "RESTORE_ERROR=$_" }
        throw "H3E1B1_SETUP_RESTORE_FAILURE"
    }
}

if (-not (Test-Path -LiteralPath $archivePath)) {
    throw "H3E1B1_SETUP_MODBUS_RTU_ARCHIVE_RESTORE_MISSING"
}

$archiveHashAfterCompile = Get-G2Sha256 $archiveRelative
Write-Host "MODBUS_RTU_ARCHIVE_SHA256_AFTER_COMPILE=$archiveHashAfterCompile"

if ($archiveHashAfterCompile -ne $archiveHashBefore) {
    throw "H3E1B1_SETUP_MODBUS_RTU_ARCHIVE_RESTORE_HASH_MISMATCH"
}

if (-not (Test-Path -LiteralPath $displayArchivePath)) {
    throw "H3E1B1_SETUP_DISPLAY_ARCHIVE_RESTORE_MISSING"
}

$displayArchiveHashAfterCompile = Get-G2Sha256 $displayArchiveRelative
Write-Host "DISPLAY_ARCHIVE_SHA256_AFTER_COMPILE=$displayArchiveHashAfterCompile"

if ($displayArchiveHashAfterCompile -ne $displayArchiveHashBefore) {
    throw "H3E1B1_SETUP_DISPLAY_ARCHIVE_RESTORE_HASH_MISMATCH"
}

$slaveRtuObjects = @(
    Get-ChildItem -LiteralPath $slaveBuild -Recurse -File -ErrorAction SilentlyContinue |
        Where-Object { $_.Name -eq "JWPLC_ModbusRTU.cpp.o" }
)
$masterRtuObjects = @(
    Get-ChildItem -LiteralPath $masterBuild -Recurse -File -ErrorAction SilentlyContinue |
        Where-Object { $_.Name -eq "JWPLC_ModbusRTU.cpp.o" }
)

Write-Host "SLAVE_MODBUS_RTU_SOURCE_OBJECT_COUNT=$($slaveRtuObjects.Count)"
Write-Host "MASTER_MODBUS_RTU_SOURCE_OBJECT_COUNT=$($masterRtuObjects.Count)"

if ($slaveRtuObjects.Count -lt 1) {
    throw "H3E1B1_SETUP_SLAVE_MODBUS_RTU_SOURCE_OBJECT_MISSING"
}
if ($masterRtuObjects.Count -lt 1) {
    throw "H3E1B1_SETUP_MASTER_MODBUS_RTU_SOURCE_OBJECT_MISSING"
}

[string[]]$displayObjectNames = @(
    "JWPLC_Display.cpp.o",
    "JWPLC_UI.cpp.o",
    "JWPLC_UI_API.cpp.o",
    "JWPLC_UI_Pages.cpp.o"
)

$displayObjectCount = 0
foreach ($displayObjectName in $displayObjectNames) {
    [object[]]$matches = @(
        Get-ChildItem -LiteralPath $masterBuild -Recurse -File -ErrorAction SilentlyContinue |
            Where-Object { $_.Name -eq $displayObjectName }
    )
    $displayObjectCount += $matches.Count
    Write-Host "MASTER_DISPLAY_OBJECT $displayObjectName=$($matches.Count)"
}

Write-Host "MASTER_DISPLAY_SOURCE_OBJECT_COUNT=$displayObjectCount"

if ($DisplayLinkage -eq "ARCHIVE") {
    if ($displayObjectCount -ne 0) {
        throw "H3E1B1_SETUP_ARCHIVE_LEG_HAS_DISPLAY_SOURCE_OBJECTS"
    }
    Write-Host "DISPLAY_LINKAGE_PROOF=ARCHIVE_NO_SOURCE_OBJECTS"
}
else {
    if ($displayObjectCount -lt 4) {
        throw "H3E1B1_SETUP_SOURCE_LEG_DISPLAY_OBJECTS_MISSING"
    }
    Write-Host "DISPLAY_LINKAGE_PROOF=SOURCE_OBJECTS_PRESENT"
}

$compileDbPath = Join-Path $masterBuild "compile_commands.json"
if (-not (Test-Path -LiteralPath $compileDbPath)) {
    throw "H3E1B1_SETUP_COMPILE_DB_MISSING"
}

$compileDbText = [IO.File]::ReadAllText($compileDbPath)
$sourceCorePass =
    $compileDbText.Contains("cores/jwcontrol/main.cpp") -or
    $compileDbText.Contains("cores\\jwcontrol\\main.cpp")

Write-Host "MASTER_SOURCE_CORE_MAIN_COMPILED=$sourceCorePass"
if (-not $sourceCorePass) {
    throw "H3E1B1_SETUP_SOURCE_CORE_NOT_CONFIRMED"
}

$coreHashAfterCompile = Get-G2Sha256 $coreRelative
Write-Host "CORE_A_SHA256_AFTER_COMPILE=$coreHashAfterCompile"

if ($coreHashAfterCompile -ne $coreHashBefore) {
    throw "H3E1B1_SETUP_PRECOMPILED_CORE_CHANGED_DURING_SOURCE_BUILD"
}

Write-Host ""
Write-Host "=== UPLOAD SLAVE $SlavePort ==="

$slaveUploadArgs = @(
    "upload",
    "--fqbn", $fqbn,
    "--port", $SlavePort,
    "--input-dir", $slaveBuild,
    $slaveDir
)
$slaveUploadExit = Invoke-NativeToLog -FilePath $arduinoCli -Arguments $slaveUploadArgs -LogPath $slaveUploadLog
Write-Host "SLAVE_UPLOAD_EXIT=$slaveUploadExit"
if ($slaveUploadExit -ne 0) {
    Get-Content -LiteralPath $slaveUploadLog -Tail 80 | ForEach-Object { Write-Host $_ }
    throw "H3E1B1_SETUP_SLAVE_UPLOAD_FAILED"
}

Start-Sleep -Seconds 2

Write-Host ""
Write-Host "=== UPLOAD MASTER SOURCE CORE $MasterPort ==="

$masterUploadArgs = @(
    "upload",
    "--fqbn", $fqbn,
    "--port", $MasterPort,
    "--input-dir", $masterBuild,
    $masterDir
)
$masterUploadExit = Invoke-NativeToLog -FilePath $arduinoCli -Arguments $masterUploadArgs -LogPath $masterUploadLog
Write-Host "MASTER_UPLOAD_EXIT=$masterUploadExit"
if ($masterUploadExit -ne 0) {
    Get-Content -LiteralPath $masterUploadLog -Tail 80 | ForEach-Object { Write-Host $_ }
    throw "H3E1B1_SETUP_MASTER_UPLOAD_FAILED"
}

Start-Sleep -Seconds 2

Write-Host ""
Write-Host "=== PHYSICAL READY PREFLIGHT ==="

$resolverArgs = @($resolver, "--serial", $MasterPort, "--timeout", "45")
$resolverExit = Invoke-NativeToLog -FilePath $pythonExe -Arguments $resolverArgs -LogPath $resolverLog
Write-Host "H3E1B1_SETUP_RESOLVER_EXIT=$resolverExit"

if ($resolverExit -ne 0) {
    Get-Content -LiteralPath $resolverLog -Tail 80 | ForEach-Object { Write-Host $_ }
    throw "H3E1B1_SETUP_PREFLIGHT_FAILED"
}

$resolverText = [IO.File]::ReadAllText($resolverLog)
$ipMatch = [regex]::Match(
    $resolverText,
    "(?m)^P5_DUT_IP_EFFECTIVE=(.+?)\r?$"
)
if (-not $ipMatch.Success) {
    throw "H3E1B1_SETUP_IP_MISSING"
}
$dutIp = $ipMatch.Groups[1].Value.Trim()

Get-Content -LiteralPath $resolverLog | ForEach-Object { Write-Host $_ }

$coreHashFinal = Get-G2Sha256 $coreRelative
$archiveHashFinal = Get-G2Sha256 $archiveRelative
$displayArchiveHashFinal = Get-G2Sha256 $displayArchiveRelative

Write-Host "CORE_A_SHA256_FINAL=$coreHashFinal"
Write-Host "MODBUS_RTU_ARCHIVE_SHA256_FINAL=$archiveHashFinal"
Write-Host "DISPLAY_ARCHIVE_SHA256_FINAL=$displayArchiveHashFinal"

if ($coreHashFinal -ne $coreHashBefore) {
    throw "H3E1B1_SETUP_PRECOMPILED_CORE_FINAL_CHANGED"
}
if ($archiveHashFinal -ne $archiveHashBefore) {
    throw "H3E1B1_SETUP_MODBUS_RTU_ARCHIVE_FINAL_CHANGED"
}
if ($displayArchiveHashFinal -ne $displayArchiveHashBefore) {
    throw "H3E1B1_SETUP_DISPLAY_ARCHIVE_FINAL_CHANGED"
}

Restore-BoardsLocal

Write-Host "A14_H3E1B1_SETUP_ONLY=PASS"
Write-Host "H3E1B1_SETUP_ONLY_MASTER_IP=$dutIp"
Write-Host "H3E1B1_PRECOMPILED_CORE_PRESERVED=YES"
Write-Host "H3E1B1_MODBUS_RTU_ARCHIVE_RESTORED=YES"
Write-Host "H3E1B1_DISPLAY_ARCHIVE_RESTORED=YES"
Write-Host "H3E1B1_DISPLAY_LINKAGE_FINAL=$DisplayLinkage"
if ($coreSourceStrategy -eq "SHARED_LINK_NO_OVERLAY") {
    Write-Host "H3E1B1_INSTALLED_CORE_RESTORED=NOT_REQUIRED_SHARED_LINK"
}
else {
    Write-Host "H3E1B1_INSTALLED_CORE_RESTORED=YES"
}
Write-Host "H3E1B1_INSTALLED_CORE_PRESERVED=YES"
Write-Host "H3E1B1_CORE_SOURCE_STRATEGY_FINAL=$coreSourceStrategy"

if (-not $SetupOnly) {
    throw "H3E1B1_SETUP_ONLY_REQUIRED"
}
