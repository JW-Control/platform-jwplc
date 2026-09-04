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
$typesRel = "src/frontend/store/slices/device/types.ts"
$selectorsRel = "src/frontend/hooks/use-store-selectors.ts"
$boardRel = "src/frontend/components/_features/[workspace]/editor/device/configuration/board.tsx"
$helperRel = "src/frontend/store/slices/device/default-pin-mapping.ts"

$slicePath = Join-Path $OpenPLCRepo $sliceRel
$typesPath = Join-Path $OpenPLCRepo $typesRel
$selectorsPath = Join-Path $OpenPLCRepo $selectorsRel
$boardPath = Join-Path $OpenPLCRepo $boardRel
$helperPath = Join-Path $OpenPLCRepo $helperRel

foreach ($path in @($slicePath, $typesPath, $selectorsPath, $boardPath, $helperPath)) {
    if (-not (Test-Path -LiteralPath $path)) {
        throw "No existe archivo esperado: $path"
    }
}

Write-Section "ALPHA7 - PIN MAPPING MANUAL DEFAULTS"
Write-Host "OpenPLC : $OpenPLCRepo"
Write-Host "Branch  : $branch"
Write-Host "Policy  : VPP defaults stay available, but are NOT seeded automatically"
Write-Host "Backplane normal flow can own byte 0 when Pin Mapping is empty"

$slice = Normalize-Lf ([System.IO.File]::ReadAllText($slicePath))
$types = Normalize-Lf ([System.IO.File]::ReadAllText($typesPath))
$selectors = Normalize-Lf ([System.IO.File]::ReadAllText($selectorsPath))
$board = Normalize-Lf ([System.IO.File]::ReadAllText($boardPath))

if ($slice -notmatch "buildDefaultPinMapping") {
    throw "El helper de default pin mapping no esta importado en slice.ts. Ejecuta primero el aplicador V2."
}

$autoSeedHelper = @'
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

'@
$slice = Replace-Once $slice (Normalize-Lf $autoSeedHelper) "" "auto-seed helper"

$slice = Replace-Once $slice "            ensureDefaultPinsForVppBoard(draft, draft.deviceDefinitions.configuration.deviceBoard)`n" "" "auto-seed setAvailableOptions"
$slice = Replace-Once $slice "          ensureDefaultPinsForVppBoard(draft, deviceBoard)`n" "" "auto-seed setDeviceBoard"

$actionAnchor = @'
    selectPinTableRow: (selectedRow) => {
      setState(
        produce(({ deviceDefinitions }: DeviceSlice) => {
          deviceDefinitions.pinMapping.currentSelectedPinTableRow = selectedRow
        }),
      )
    },

    createNewPin: (): void => {
'@
$actionReplacement = @'
    selectPinTableRow: (selectedRow) => {
      setState(
        produce(({ deviceDefinitions }: DeviceSlice) => {
          deviceDefinitions.pinMapping.currentSelectedPinTableRow = selectedRow
        }),
      )
    },

    loadDefaultPinMapping: (): void => {
      const activeBoard = getState().deviceDefinitions.configuration.deviceBoard
      const boardInfo = getState().deviceAvailableOptions.availableBoards.get(activeBoard)
      if (!activeBoard || !boardInfo?.vpp) return

      const defaults = buildDefaultPinMapping(boardInfo.pins)
      if (defaults.length === 0) return

      setState(
        produce((draft: DeviceSlice) => {
          draft.deviceUpdated.updated = true
          draft.deviceDefinitions.pinMapping.pinsByBoard[activeBoard] = defaults
          draft.deviceDefinitions.pinMapping.currentSelectedPinTableRow = -1
        }),
      )
      getState().projectActions.recalculateIecAddresses()
    },

    clearPinMapping: (): void => {
      const activeBoard = getState().deviceDefinitions.configuration.deviceBoard
      if (!activeBoard) return

      setState(
        produce((draft: DeviceSlice) => {
          draft.deviceUpdated.updated = true
          draft.deviceDefinitions.pinMapping.pinsByBoard[activeBoard] = []
          draft.deviceDefinitions.pinMapping.currentSelectedPinTableRow = -1
        }),
      )
      getState().projectActions.recalculateIecAddresses()
    },

    createNewPin: (): void => {
'@
$slice = Replace-Once $slice (Normalize-Lf $actionAnchor) (Normalize-Lf $actionReplacement) "manual pin mapping actions"

$typesAnchor = @'
  selectPinTableRow: (row: number) => void
  createNewPin: () => void
'@
$typesReplacement = @'
  selectPinTableRow: (row: number) => void
  /** Replace the active VPP board's pin table with its declarative defaults. */
  loadDefaultPinMapping: () => void
  /** Keep an explicit empty bucket for the active board and free its IEC claims. */
  clearPinMapping: () => void
  createNewPin: () => void
'@
$types = Replace-Once $types (Normalize-Lf $typesAnchor) (Normalize-Lf $typesReplacement) "DeviceActions manual defaults"

$selectorsAnchor = @'
  useCreateNewPin: () => useOpenPLCStore((state) => state.deviceActions.createNewPin),
  useRemovePin: () => useOpenPLCStore((state) => state.deviceActions.removePin),
'@
$selectorsReplacement = @'
  useLoadDefaultPinMapping: () => useOpenPLCStore((state) => state.deviceActions.loadDefaultPinMapping),
  useClearPinMapping: () => useOpenPLCStore((state) => state.deviceActions.clearPinMapping),
  useCreateNewPin: () => useOpenPLCStore((state) => state.deviceActions.createNewPin),
  useRemovePin: () => useOpenPLCStore((state) => state.deviceActions.removePin),
'@
$selectors = Replace-Once $selectors (Normalize-Lf $selectorsAnchor) (Normalize-Lf $selectorsReplacement) "pin selectors manual defaults"

$boardHooksAnchor = @'
  const pins = pinSelectors.usePins()
  const createNewPin = pinSelectors.useCreateNewPin()
  const removePin = pinSelectors.useRemovePin()
'@
$boardHooksReplacement = @'
  const pins = pinSelectors.usePins()
  const loadDefaultPinMapping = pinSelectors.useLoadDefaultPinMapping()
  const clearPinMapping = pinSelectors.useClearPinMapping()
  const createNewPin = pinSelectors.useCreateNewPin()
  const removePin = pinSelectors.useRemovePin()
'@
$board = Replace-Once $board (Normalize-Lf $boardHooksAnchor) (Normalize-Lf $boardHooksReplacement) "board manual default hooks"

$boardUiAnchor = @'
            <TableActions
              className='w-fit *:rounded-md *:p-1'
              actions={[
                {
                  ariaLabel: 'Add table row button',
                  onClick: createNewPin,
                  icon: <PlusIcon className='!stroke-brand' />,
                  id: 'add-pin-button',
                },
                {
                  ariaLabel: 'Remove table row button',
                  onClick: removePin,
                  disabled: currentSelectedPinTableRow === -1,
                  icon: <MinusIcon className='!stroke-brand' />,
                  id: 'remove-pin-button',
                },
              ]}
            />
'@
$boardUiReplacement = @'
            <div className='flex items-center gap-2'>
              {currentBoardInfo?.vpp && (
                <button
                  type='button'
                  onClick={loadDefaultPinMapping}
                  className='rounded-md border border-brand px-2 py-1 font-caption text-xs font-medium text-brand hover:bg-brand/10'
                  title='Replace this board pin mapping with the defaults declared by its package'
                >
                  Load defaults
                </button>
              )}
              <button
                type='button'
                onClick={clearPinMapping}
                disabled={pins.length === 0}
                className='rounded-md border border-neutral-300 px-2 py-1 font-caption text-xs font-medium text-neutral-700 hover:bg-neutral-100 disabled:cursor-not-allowed disabled:opacity-40 dark:border-neutral-700 dark:text-neutral-300 dark:hover:bg-neutral-800'
                title='Clear this board pin mapping and free its IEC addresses'
              >
                Clear
              </button>
              <TableActions
                className='w-fit *:rounded-md *:p-1'
                actions={[
                  {
                    ariaLabel: 'Add table row button',
                    onClick: createNewPin,
                    icon: <PlusIcon className='!stroke-brand' />,
                    id: 'add-pin-button',
                  },
                  {
                    ariaLabel: 'Remove table row button',
                    onClick: removePin,
                    disabled: currentSelectedPinTableRow === -1,
                    icon: <MinusIcon className='!stroke-brand' />,
                    id: 'remove-pin-button',
                  },
                ]}
              />
            </div>
'@
$board = Replace-Once $board (Normalize-Lf $boardUiAnchor) (Normalize-Lf $boardUiReplacement) "Pin Mapping manual buttons"

if ($slice -match "ensureDefaultPinsForVppBoard") {
    throw "Validacion interna: quedaron referencias al auto-seed."
}
if ($slice -notmatch "loadDefaultPinMapping") {
    throw "Validacion interna: falta accion loadDefaultPinMapping."
}
if ($slice -notmatch "clearPinMapping") {
    throw "Validacion interna: falta accion clearPinMapping."
}
if ($board -notmatch ">\s*Load defaults\s*<") {
    throw "Validacion interna: falta boton Load defaults."
}
if ($board -notmatch ">\s*Clear\s*<") {
    throw "Validacion interna: falta boton Clear."
}

Write-Utf8NoBom $slicePath ($slice.TrimEnd() + "`n")
Write-Utf8NoBom $typesPath ($types.TrimEnd() + "`n")
Write-Utf8NoBom $selectorsPath ($selectors.TrimEnd() + "`n")
Write-Utf8NoBom $boardPath ($board.TrimEnd() + "`n")

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
Write-Host "PIN_MAPPING_AUTO_SEED=REMOVED"
Write-Host "PIN_MAPPING_DEFAULTS=MANUAL"
Write-Host "PIN_MAPPING_CLEAR=AVAILABLE"
Write-Host "IEC_RECALCULATE_ON_MANUAL_CHANGE=YES"
Write-Host "BACKPLANE_BYTE0_CAN_BE_FREE=YES"
Write-Host "NEXT=FOCUSED_JEST_AND_TYPECHECK"
