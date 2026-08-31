param(
    [Parameter(Mandatory = $true)]
    [string]$OpenPLCRepo
)

$ErrorActionPreference = "Stop"

function Write-Section([string]$Text) {
    Write-Host ""
    Write-Host "============================================================" -ForegroundColor Cyan
    Write-Host " $Text" -ForegroundColor Cyan
    Write-Host "============================================================" -ForegroundColor Cyan
}

function Normalize-Lf([string]$Text) {
    return $Text.Replace("`r`n", "`n").Replace("`r", "`n")
}

function Replace-Once([string]$Text, [string]$Old, [string]$New, [string]$Label) {
    $count = ([regex]::Matches($Text, [regex]::Escape($Old))).Count
    if ($count -ne 1) {
        throw "No se encontro un ancla unica para $Label. Coincidencias: $count"
    }
    return $Text.Replace($Old, $New)
}

function Write-Utf8NoBom([string]$Path, [string]$Text) {
    $utf8NoBom = New-Object System.Text.UTF8Encoding($false)
    [System.IO.File]::WriteAllText($Path, $Text, $utf8NoBom)
}

$OpenPLCRepo = (Resolve-Path $OpenPLCRepo).Path
$expectedBranch = "develop/alpha7-openplc-remote-io-rtu"
$branch = (& git -C $OpenPLCRepo branch --show-current).Trim()
if ($branch -ne $expectedBranch) {
    throw "Branch OpenPLC inesperado. Esperado: $expectedBranch ; actual: $branch"
}

$sliceRel = "src/frontend/store/slices/device/slice.ts"
$helperRel = "src/frontend/store/slices/device/default-pin-mapping.ts"
$testRel = "src/frontend/store/slices/device/__tests__/default-pin-mapping.test.ts"

$slicePath = Join-Path $OpenPLCRepo $sliceRel
$helperPath = Join-Path $OpenPLCRepo $helperRel
$testPath = Join-Path $OpenPLCRepo $testRel

if (-not (Test-Path -LiteralPath $slicePath)) {
    throw "No existe slice esperado: $slicePath"
}

$targetStatus = & git -C $OpenPLCRepo status --porcelain -- $sliceRel $helperRel $testRel
if ($targetStatus) {
    throw "Los archivos objetivo ya tienen cambios. Revisa antes de aplicar:`n$($targetStatus -join "`n")"
}

if ((Test-Path -LiteralPath $helperPath) -or (Test-Path -LiteralPath $testPath)) {
    throw "El helper/test de default pin mapping ya existe."
}

Write-Section "ALPHA7 - APPLY VPP DEFAULT PIN MAPPING"
Write-Host "OpenPLC : $OpenPLCRepo"
Write-Host "Branch  : $branch"
Write-Host "Scope   : VPP boards only"
Write-Host "Policy  : initialize only when pinsByBoard[board] is undefined"

$helper = @'
import type { DevicePin, PinType } from '../../../../middleware/shared/ports/types'

import { createNewAddress, getHighestPinAddress } from './validation/pins'

type BoardPinDefaults = {
  defaultDin?: string[]
  defaultDout?: string[]
  defaultAin?: string[]
  defaultAout?: string[]
}

function appendDefaultPins(target: DevicePin[], names: string[] | undefined, pinType: PinType): void {
  for (const rawName of names ?? []) {
    const pin = rawName.trim()
    if (!pin) continue

    const previousAddress = getHighestPinAddress(target, pinType)
    target.push({
      pin,
      pinType,
      address: createNewAddress('INCREMENT', previousAddress),
      alias: '',
    })
  }
}

/**
 * Materialize a VPP board's declarative default pin lists into the same
 * DevicePin shape used by the Configuration -> Pin Mapping table.
 *
 * Address allocation deliberately reuses the table's canonical IEC helpers:
 *   digital inputs  -> %IX0.0, %IX0.1, ...
 *   digital outputs -> %QX0.0, %QX0.1, ...
 *   analog inputs   -> %IW0, %IW1, ...
 *   analog outputs  -> %QW0, %QW1, ...
 */
export function buildDefaultPinMapping(defaults: BoardPinDefaults | undefined): DevicePin[] {
  const pins: DevicePin[] = []

  appendDefaultPins(pins, defaults?.defaultDin, 'digitalInput')
  appendDefaultPins(pins, defaults?.defaultDout, 'digitalOutput')
  appendDefaultPins(pins, defaults?.defaultAin, 'analogInput')
  appendDefaultPins(pins, defaults?.defaultAout, 'analogOutput')

  return pins
}

export type { BoardPinDefaults }
'@

$test = @'
import { buildDefaultPinMapping } from '../default-pin-mapping'

describe('buildDefaultPinMapping', () => {
  it('materializes JWPLC-style digital defaults with canonical IEC addresses', () => {
    const pins = buildDefaultPinMapping({
      defaultDin: ['I0_0', 'I0_1', 'I0_2', 'I0_3', 'I0_4', 'I0_5', 'I0_6', 'I0_7'],
      defaultDout: ['Q0_0', 'Q0_1', 'Q0_2', 'Q0_3', 'Q0_4', 'Q0_5', 'Q0_6', 'Q0_7'],
    })

    expect(pins).toHaveLength(16)
    expect(pins[0]).toEqual({ pin: 'I0_0', pinType: 'digitalInput', address: '%IX0.0', alias: '' })
    expect(pins[7]).toEqual({ pin: 'I0_7', pinType: 'digitalInput', address: '%IX0.7', alias: '' })
    expect(pins[8]).toEqual({ pin: 'Q0_0', pinType: 'digitalOutput', address: '%QX0.0', alias: '' })
    expect(pins[15]).toEqual({ pin: 'Q0_7', pinType: 'digitalOutput', address: '%QX0.7', alias: '' })
  })

  it('rolls digital addresses to the next byte', () => {
    const pins = buildDefaultPinMapping({
      defaultDin: ['I0', 'I1', 'I2', 'I3', 'I4', 'I5', 'I6', 'I7', 'I8'],
    })

    expect(pins[8]?.address).toBe('%IX1.0')
  })

  it('materializes analog defaults with word addresses', () => {
    const pins = buildDefaultPinMapping({
      defaultAin: ['AI0', 'AI1'],
      defaultAout: ['AO0', 'AO1'],
    })

    expect(pins).toEqual([
      { pin: 'AI0', pinType: 'analogInput', address: '%IW0', alias: '' },
      { pin: 'AI1', pinType: 'analogInput', address: '%IW1', alias: '' },
      { pin: 'AO0', pinType: 'analogOutput', address: '%QW0', alias: '' },
      { pin: 'AO1', pinType: 'analogOutput', address: '%QW1', alias: '' },
    ])
  })

  it('ignores blank manifest entries without shifting valid addresses', () => {
    const pins = buildDefaultPinMapping({ defaultDin: [' I0_0 ', '', '   ', 'I0_1'] })

    expect(pins).toEqual([
      { pin: 'I0_0', pinType: 'digitalInput', address: '%IX0.0', alias: '' },
      { pin: 'I0_1', pinType: 'digitalInput', address: '%IX0.1', alias: '' },
    ])
  })
})
'@

$slice = Normalize-Lf ([System.IO.File]::ReadAllText($slicePath))

$importOld = @'
import type { DeviceConfiguration, DevicePin } from '../../../../middleware/shared/ports/types'
import { defaultDeviceConfiguration } from './data/types'
import type { DeviceSlice, DeviceSliceRoot, PinUpdateResponse } from './types'
'@
$importNew = @'
import type { DeviceConfiguration, DevicePin } from '../../../../middleware/shared/ports/types'
import { defaultDeviceConfiguration } from './data/types'
import { buildDefaultPinMapping } from './default-pin-mapping'
import type { DeviceSlice, DeviceSliceRoot, PinUpdateResponse } from './types'
'@
$slice = Replace-Once $slice (Normalize-Lf $importOld) (Normalize-Lf $importNew) "default-pin-mapping import"

$helperAnchor = @'
function getActivePinsDraft(draft: DeviceSlice): DevicePin[] {
  const board = draft.deviceDefinitions.configuration.deviceBoard
  if (!draft.deviceDefinitions.pinMapping.pinsByBoard[board]) {
    draft.deviceDefinitions.pinMapping.pinsByBoard[board] = []
  }
  return draft.deviceDefinitions.pinMapping.pinsByBoard[board]
}

const createDeviceSlice: StateCreator<DeviceSliceRoot, [], [], DeviceSlice> = (setState, getState) => ({
'@
$helperReplacement = @'
function getActivePinsDraft(draft: DeviceSlice): DevicePin[] {
  const board = draft.deviceDefinitions.configuration.deviceBoard
  if (!draft.deviceDefinitions.pinMapping.pinsByBoard[board]) {
    draft.deviceDefinitions.pinMapping.pinsByBoard[board] = []
  }
  return draft.deviceDefinitions.pinMapping.pinsByBoard[board]
}

/**
 * Seed declarative VPP defaults once, and only once, for a board in this
 * project. A missing property means "never initialized"; an existing empty
 * array is intentional user state and must not be repopulated.
 *
 * Built-in hals.json boards are deliberately excluded from this Alpha7 change
 * so their historical behavior remains untouched.
 */
function ensureDefaultPinsForVppBoard(draft: DeviceSlice, board: string): void {
  if (!board) return

  const pinsByBoard = draft.deviceDefinitions.pinMapping.pinsByBoard
  if (Object.prototype.hasOwnProperty.call(pinsByBoard, board)) return

  const boardInfo = draft.deviceAvailableOptions.availableBoards.get(board)
  if (!boardInfo?.vpp) return

  const defaults = buildDefaultPinMapping(boardInfo.pins)
  if (defaults.length === 0) return

  pinsByBoard[board] = defaults
}

const createDeviceSlice: StateCreator<DeviceSliceRoot, [], [], DeviceSlice> = (setState, getState) => ({
'@
$slice = Replace-Once $slice (Normalize-Lf $helperAnchor) (Normalize-Lf $helperReplacement) "ensureDefaultPinsForVppBoard"

$availableOld = @'
    setAvailableOptions: ({ availableBoards, availableCommunicationPorts }): void => {
      setState(
        produce(({ deviceAvailableOptions }: DeviceSlice) => {
          if (availableBoards) {
            deviceAvailableOptions.availableBoards = availableBoards
          }
          if (availableCommunicationPorts) {
            deviceAvailableOptions.availableCommunicationPorts = availableCommunicationPorts
          }
        }),
      )
'@
$availableNew = @'
    setAvailableOptions: ({ availableBoards, availableCommunicationPorts }): void => {
      setState(
        produce((draft: DeviceSlice) => {
          const { deviceAvailableOptions } = draft
          if (availableBoards) {
            deviceAvailableOptions.availableBoards = availableBoards
            ensureDefaultPinsForVppBoard(draft, draft.deviceDefinitions.configuration.deviceBoard)
          }
          if (availableCommunicationPorts) {
            deviceAvailableOptions.availableCommunicationPorts = availableCommunicationPorts
          }
        }),
      )
'@
$slice = Replace-Once $slice (Normalize-Lf $availableOld) (Normalize-Lf $availableNew) "setAvailableOptions default seed"

$setBoardOld = @'
      setState(
        produce(({ deviceDefinitions, deviceUpdated }: DeviceSlice) => {
          deviceUpdated.updated = true
'@
$setBoardNew = @'
      setState(
        produce((draft: DeviceSlice) => {
          const { deviceDefinitions, deviceUpdated } = draft
          deviceUpdated.updated = true
'@

# The same produce signature appears in several actions; target the occurrence
# immediately following setDeviceBoard rather than replacing globally.
$boardMarker = "    setDeviceBoard: (deviceBoard): void => {`n      const previousBoard = getState().deviceDefinitions.configuration.deviceBoard`n"
$markerIndex = $slice.IndexOf($boardMarker)
if ($markerIndex -lt 0) {
    throw "No se encontro ancla setDeviceBoard."
}
$prefix = $slice.Substring(0, $markerIndex)
$tail = $slice.Substring($markerIndex)
$tail = Replace-Once $tail (Normalize-Lf $setBoardOld) (Normalize-Lf $setBoardNew) "setDeviceBoard draft"

$boardAssignOld = @'
          deviceDefinitions.configuration.deviceBoard = deviceBoard
        }),
'@
$boardAssignNew = @'
          deviceDefinitions.configuration.deviceBoard = deviceBoard
          ensureDefaultPinsForVppBoard(draft, deviceBoard)
        }),
'@
$tail = Replace-Once $tail (Normalize-Lf $boardAssignOld) (Normalize-Lf $boardAssignNew) "setDeviceBoard seed"
$slice = $prefix + $tail

# Validate all transformations before any write.
if ($slice -notmatch 'function ensureDefaultPinsForVppBoard') {
    throw "Validacion interna: falta helper de seed."
}
if ($slice -notmatch 'ensureDefaultPinsForVppBoard\(draft, deviceBoard\)') {
    throw "Validacion interna: falta seed al cambiar board."
}
if ($slice -notmatch 'ensureDefaultPinsForVppBoard\(draft, draft\.deviceDefinitions\.configuration\.deviceBoard\)') {
    throw "Validacion interna: falta seed cuando llega availableBoards."
}

$helperDir = Split-Path -Parent $helperPath
$testDir = Split-Path -Parent $testPath
New-Item -ItemType Directory -Force -Path $helperDir | Out-Null
New-Item -ItemType Directory -Force -Path $testDir | Out-Null

Write-Utf8NoBom $slicePath ($slice.TrimEnd() + "`n")
Write-Utf8NoBom $helperPath ((Normalize-Lf $helper).Trim() + "`n")
Write-Utf8NoBom $testPath ((Normalize-Lf $test).Trim() + "`n")

Write-Section "VALIDATION"
& git -C $OpenPLCRepo diff --check
if ($LASTEXITCODE -ne 0) {
    throw "git diff --check fallo."
}

Write-Host ""
Write-Host "=== OPENPLC STATUS ===" -ForegroundColor Cyan
& git -C $OpenPLCRepo status --short

Write-Host ""
Write-Host "=== OPENPLC DIFF STAT (tracked) ===" -ForegroundColor Cyan
& git -C $OpenPLCRepo diff --stat

Write-Host ""
Write-Host "VPP_DEFAULT_PIN_MAPPING=APPLIED"
Write-Host "VPP_ONLY=YES"
Write-Host "PRESERVE_EXISTING_BUCKET=YES"
Write-Host "BUILTIN_BOARD_BEHAVIOR=UNCHANGED"
Write-Host "NEXT=FOCUSED_JEST_AND_TYPECHECK"
