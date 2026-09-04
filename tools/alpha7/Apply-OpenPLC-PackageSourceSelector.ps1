param(
    [Parameter(Mandatory = $true)]
    [string]$OpenPLCRepo
)

$ErrorActionPreference = 'Stop'

function Get-GitOutput {
    param(
        [Parameter(Mandatory = $true)][string]$Repo,
        [Parameter(Mandatory = $true)][string[]]$Arguments
    )

    $output = & git -C $Repo @Arguments 2>&1
    if ($LASTEXITCODE -ne 0) {
        throw "git -C `"$Repo`" $($Arguments -join ' ') fallo:`n$output"
    }
    return ($output -join "`n").Trim()
}

function Assert-CleanBranch {
    param(
        [Parameter(Mandatory = $true)][string]$Repo,
        [Parameter(Mandatory = $true)][string]$ExpectedBranch
    )

    $branch = Get-GitOutput -Repo $Repo -Arguments @('branch', '--show-current')
    if ($branch -ne $ExpectedBranch) {
        throw "Rama inesperada en $Repo. Esperada: $ExpectedBranch. Actual: $branch"
    }

    $status = Get-GitOutput -Repo $Repo -Arguments @('status', '--porcelain')
    if ($status) {
        throw "El working tree no esta limpio en $Repo. Revisa git status antes de aplicar el selector."
    }
}

function Read-Utf8Text {
    param([Parameter(Mandatory = $true)][string]$Path)
    return [System.IO.File]::ReadAllText($Path)
}

function Write-Utf8NoBom {
    param(
        [Parameter(Mandatory = $true)][string]$Path,
        [Parameter(Mandatory = $true)][string]$Content
    )

    $encoding = New-Object System.Text.UTF8Encoding($false)
    [System.IO.File]::WriteAllText($Path, $Content, $encoding)
}

function Replace-ExactlyOnce {
    param(
        [Parameter(Mandatory = $true)][string]$Content,
        [Parameter(Mandatory = $true)][string]$Old,
        [Parameter(Mandatory = $true)][string]$New,
        [Parameter(Mandatory = $true)][string]$Label
    )

    $first = $Content.IndexOf($Old, [System.StringComparison]::Ordinal)
    if ($first -lt 0) {
        throw "No se encontro el ancla esperada: $Label"
    }
    $second = $Content.IndexOf($Old, $first + $Old.Length, [System.StringComparison]::Ordinal)
    if ($second -ge 0) {
        throw "El ancla aparece mas de una vez y no es seguro reemplazarla: $Label"
    }

    return $Content.Substring(0, $first) + $New + $Content.Substring($first + $Old.Length)
}

$platformRepo = Get-GitOutput -Repo $PSScriptRoot -Arguments @('rev-parse', '--show-toplevel')
$openplcRepoResolved = (Resolve-Path $OpenPLCRepo).Path
$platformRepoResolved = (Resolve-Path $platformRepo).Path

Write-Host '============================================================' -ForegroundColor Cyan
Write-Host ' ALPHA7 - APPLY OPENPLC PACKAGE SOURCE SELECTOR' -ForegroundColor Cyan
Write-Host '============================================================' -ForegroundColor Cyan
Write-Host "OpenPLC : $openplcRepoResolved"
Write-Host "Platform: $platformRepoResolved"

Assert-CleanBranch -Repo $openplcRepoResolved -ExpectedBranch 'develop/alpha7-openplc-remote-io-rtu'
Assert-CleanBranch -Repo $platformRepoResolved -ExpectedBranch 'v2.1.0-alpha.7/feature/openplc-backplane-validation'

$typesPath = Join-Path $openplcRepoResolved 'src/middleware/shared/ports/types.ts'
$compilerPath = Join-Path $openplcRepoResolved 'src/backend/editor/compiler/compiler-module.ts'
$boardPath = Join-Path $openplcRepoResolved 'src/frontend/components/_features/[workspace]/editor/device/configuration/board.tsx'
$resolverPath = Join-Path $openplcRepoResolved 'src/backend/shared/firmware/resolve-platform-options.ts'
$resolverTestPath = Join-Path $openplcRepoResolved 'src/backend/shared/firmware/__tests__/resolve-platform-options.test.ts'
$manifestPath = Join-Path $platformRepoResolved 'openplc-editor-installers/v4.2.7/vpp/manifest.json'

foreach ($path in @($typesPath, $compilerPath, $boardPath, $manifestPath)) {
    if (-not (Test-Path $path)) {
        throw "No existe el archivo esperado: $path"
    }
}
if (Test-Path $resolverPath) {
    throw "Ya existe $resolverPath. No se aplicara dos veces."
}
if (Test-Path $resolverTestPath) {
    throw "Ya existe $resolverTestPath. No se aplicara dos veces."
}

$types = Read-Utf8Text $typesPath
$compiler = Read-Utf8Text $compilerPath
$board = Read-Utf8Text $boardPath
$manifest = Read-Utf8Text $manifestPath

$typesOld = @'
export interface PlatformOptionValue {
  id: string
  label: string
  help?: string
}
'@
$typesNew = @'
export interface PlatformOptionValue {
  id: string
  label: string
  help?: string
  /** Optional exact FQBN replacement. When present, selecting this value
   *  replaces the manifest's base platform instead of appending a boards.txt
   *  menu segment. Ordinary values keep the existing :key=id behaviour. */
  fqbn?: string
}
'@
$types = Replace-ExactlyOnce -Content $types -Old $typesOld -New $typesNew -Label 'PlatformOptionValue'

$compilerImportOld = @'
import { buildArduinoCliCompileArgs } from '@root/backend/shared/firmware/build-arduino-cli-args'
'@
$compilerImportNew = @'
import { buildArduinoCliCompileArgs } from '@root/backend/shared/firmware/build-arduino-cli-args'
import { resolvePlatformOptions } from '@root/backend/shared/firmware/resolve-platform-options'
'@
$compiler = Replace-ExactlyOnce -Content $compiler -Old $compilerImportOld -New $compilerImportNew -Label 'import resolvePlatformOptions'

$applyOld = @'
  static applyPlatformOptions(
    platform: string,
    platformOptions: PlatformOption[] | undefined,
    selected: Record<string, string> | undefined,
  ): string {
    if (!platformOptions || platformOptions.length === 0) return platform
    const segments: string[] = []
    for (const opt of platformOptions) {
      const chosen = selected?.[opt.key] ?? opt.default
      segments.push(`${opt.key}=${chosen}`)
    }
    return `${platform}:${segments.join(':')}`
  }
'@
$applyNew = @'
  static applyPlatformOptions(
    platform: string,
    platformOptions: PlatformOption[] | undefined,
    selected: Record<string, string> | undefined,
  ): string {
    return resolvePlatformOptions(platform, platformOptions, selected)
  }
'@
$compiler = Replace-ExactlyOnce -Content $compiler -Old $applyOld -New $applyNew -Label 'CompilerModule.applyPlatformOptions'

$selectionOld = @'
    const { boardEntry, boardRuntime, isSimulator, isRuntimeV3, isRuntimeV4 } = selection

    const normalizedProjectPath = projectPath.replace('project.json', '')
'@
$selectionNew = @'
    const { boardEntry: resolvedBoardEntry, boardRuntime, isSimulator, isRuntimeV3, isRuntimeV4 } = selection

    const normalizedProjectPath = projectPath.replace('project.json', '')

    // Resolve persisted platform choices before the shared pipeline starts.
    // This is earlier than the legacy compile/upload FQBN composition on
    // purpose: core-install derives its core id from boardEntry.platform, so
    // an exact FQBN override must already be visible here. Otherwise a local
    // development target could accidentally preflight/install the published
    // core even though the final compile uses a different namespace.
    const resolvedBoardInfo = resolver.resolve(boardTarget)
    const selectedPlatformOptions = await this.#readSelectedPlatformOptions(normalizedProjectPath)
    const effectivePlatform =
      typeof resolvedBoardEntry.platform === 'string'
        ? CompilerModule.applyPlatformOptions(
            resolvedBoardEntry.platform,
            resolvedBoardInfo.platformOptions,
            selectedPlatformOptions,
          )
        : undefined
    const boardEntry =
      effectivePlatform !== undefined && effectivePlatform !== resolvedBoardEntry.platform
        ? { ...resolvedBoardEntry, platform: effectivePlatform }
        : resolvedBoardEntry

    for (const option of resolvedBoardInfo.platformOptions ?? []) {
      const requestedId = selectedPlatformOptions[option.key] ?? option.default
      const selectedValue =
        option.values.find((value) => value.id === requestedId) ??
        option.values.find((value) => value.id === option.default)
      if (selectedValue) {
        _mainProcessPort.postMessage({
          logLevel: 'info',
          message: `Platform option ${option.key}=${selectedValue.id} (${selectedValue.label})`,
        })
      }
    }
    if (effectivePlatform) {
      _mainProcessPort.postMessage({ logLevel: 'info', message: `Arduino FQBN: ${effectivePlatform}` })
    }
'@
$compiler = Replace-ExactlyOnce -Content $compiler -Old $selectionOld -New $selectionNew -Label 'compileProgram boardEntry resolution'

$boardHooksOld = @'
  const setDeviceBoard = boardSelectors.useSetDeviceBoard()
  const setCommunicationPort = boardSelectors.useSetCommunicationPort()
  const setAvailableOptions = boardSelectors.useSetAvailableOptions()
'@
$boardHooksNew = @'
  const setDeviceBoard = boardSelectors.useSetDeviceBoard()
  const setCommunicationPort = boardSelectors.useSetCommunicationPort()
  const selectedPlatformOptions = boardSelectors.useSelectedPlatformOptions()
  const setSelectedPlatformOption = boardSelectors.useSetSelectedPlatformOption()
  const setAvailableOptions = boardSelectors.useSetAvailableOptions()
'@
$board = Replace-ExactlyOnce -Content $board -Old $boardHooksOld -New $boardHooksNew -Label 'board platform option hooks'

$boardUiOld = @'
          </div>
          {isSimulatorTarget(currentBoardInfo) ? (
'@
$boardUiNew = @'
          </div>
          {currentBoardInfo?.platformOptions?.map((option) => {
            const selectedValue = selectedPlatformOptions[option.key] ?? option.default
            return (
              <div
                key={option.key}
                id={`platform-option-${option.key}`}
                className='flex w-full items-center justify-start gap-1 pr-5'
              >
                <Label className='whitespace-pre text-xs text-neutral-950 dark:text-white'>{option.label}</Label>
                <Select
                  value={selectedValue}
                  onValueChange={(value) => setSelectedPlatformOption(option.key, value)}
                >
                  <SelectTrigger
                    aria-label={`${option.label} selection`}
                    placeholder={option.label}
                    withIndicator
                    className='flex h-[30px] w-full items-center justify-between gap-1 rounded-md border border-neutral-100 bg-white px-2 py-1 font-caption text-cp-sm font-medium text-neutral-850 outline-none data-[state=open]:border-brand-medium-dark dark:border-neutral-850 dark:bg-neutral-950 dark:text-neutral-300'
                  />
                  <SelectContent
                    className='h-fit max-h-[250px] w-[--radix-select-trigger-width] overflow-hidden rounded-lg border border-neutral-100 bg-white outline-none drop-shadow-lg dark:border-brand-medium-dark dark:bg-neutral-950'
                    sideOffset={5}
                    alignOffset={5}
                    position='popper'
                    align='center'
                    side='bottom'
                  >
                    {option.values.map((value) => (
                      <SelectItem
                        key={value.id}
                        value={value.id}
                        className={cn(
                          'data-[state=checked]:[&:not(:hover)]:bg-neutral-100 data-[state=checked]:dark:[&:not(:hover)]:bg-neutral-900',
                          'flex w-full cursor-pointer items-center px-2 py-[9px] outline-none hover:bg-neutral-200 dark:hover:bg-neutral-850',
                        )}
                      >
                        <span className='flex items-center gap-2 font-caption text-cp-sm font-medium text-neutral-850 dark:text-neutral-300'>
                          {value.label}
                        </span>
                      </SelectItem>
                    ))}
                  </SelectContent>
                </Select>
              </div>
            )
          })}
          {isSimulatorTarget(currentBoardInfo) ? (
'@
$board = Replace-ExactlyOnce -Content $board -Old $boardUiOld -New $boardUiNew -Label 'board platform option UI'

$manifestOld = @'
                "boardManagerUrl": "https://raw.githubusercontent.com/JW-Control/platform-jwplc/main/JWPLC/package_jwplc_index.json"
'@
$manifestNew = @'
                "boardManagerUrl": "https://raw.githubusercontent.com/JW-Control/platform-jwplc/main/JWPLC/package_jwplc_index.json",
                "platformOptions": [
                    {
                        "key": "packageSource",
                        "label": "Package Source",
                        "default": "published",
                        "help": "Selecciona si OpenPLC compila con el package publicado/instalado o con el namespace local de desarrollo.",
                        "values": [
                            {
                                "id": "published",
                                "label": "Published / Installed",
                                "fqbn": "jwplc:esp32:jwplcbasic"
                            },
                            {
                                "id": "local",
                                "label": "Local Development",
                                "fqbn": "jwplc_local:esp32:jwplcbasic"
                            }
                        ]
                    }
                ]
'@
$manifest = Replace-ExactlyOnce -Content $manifest -Old $manifestOld -New $manifestNew -Label 'JWPLC VPP platformOptions'

$resolver = @'
import type { PlatformOption } from '../../../middleware/shared/ports/types'

/**
 * Resolve VPP-declared Arduino platform options into the effective FQBN.
 *
 * Ordinary values preserve the historical boards.txt menu behaviour and append
 * `:key=id`. A value that declares `fqbn` replaces the base platform exactly;
 * any ordinary options are then appended to that replacement. Stale persisted
 * ids fall back to the manifest default instead of leaking an invalid choice
 * into arduino-cli.
 */
export function resolvePlatformOptions(
  platform: string,
  platformOptions: PlatformOption[] | undefined,
  selected: Record<string, string> | undefined,
): string {
  if (!platformOptions || platformOptions.length === 0) return platform

  let effectivePlatform = platform
  const segments: string[] = []

  for (const option of platformOptions) {
    const requestedId = selected?.[option.key] ?? option.default
    const requestedValue = option.values.find((value) => value.id === requestedId)
    const defaultValue = option.values.find((value) => value.id === option.default)
    const resolvedValue = requestedValue ?? defaultValue
    const resolvedId = resolvedValue?.id ?? option.default

    if (resolvedValue?.fqbn) {
      effectivePlatform = resolvedValue.fqbn
      continue
    }

    segments.push(`${option.key}=${resolvedId}`)
  }

  return segments.length > 0 ? `${effectivePlatform}:${segments.join(':')}` : effectivePlatform
}
'@

$resolverTest = @'
import type { PlatformOption } from '../../../../middleware/shared/ports/types'

import { resolvePlatformOptions } from '../resolve-platform-options'

const packageSource: PlatformOption = {
  key: 'packageSource',
  label: 'Package Source',
  default: 'published',
  values: [
    { id: 'published', label: 'Published / Installed', fqbn: 'jwplc:esp32:jwplcbasic' },
    { id: 'local', label: 'Local Development', fqbn: 'jwplc_local:esp32:jwplcbasic' },
  ],
}

const cpuOption: PlatformOption = {
  key: 'cpu',
  label: 'Processor',
  default: 'atmega328',
  values: [
    { id: 'atmega328', label: 'ATmega328P' },
    { id: 'atmega328old', label: 'ATmega328P (Old Bootloader)' },
  ],
}

describe('resolvePlatformOptions', () => {
  it('keeps the platform unchanged when no options exist', () => {
    expect(resolvePlatformOptions('arduino:avr:mega', undefined, undefined)).toBe('arduino:avr:mega')
  })

  it('resolves the published JWPLC FQBN exactly', () => {
    expect(resolvePlatformOptions('jwplc:esp32:jwplcbasic', [packageSource], { packageSource: 'published' })).toBe(
      'jwplc:esp32:jwplcbasic',
    )
  })

  it('resolves the local JWPLC FQBN exactly', () => {
    expect(resolvePlatformOptions('jwplc:esp32:jwplcbasic', [packageSource], { packageSource: 'local' })).toBe(
      'jwplc_local:esp32:jwplcbasic',
    )
  })

  it('preserves normal Arduino boards.txt menu options', () => {
    expect(resolvePlatformOptions('arduino:avr:nano', [cpuOption], { cpu: 'atmega328old' })).toBe(
      'arduino:avr:nano:cpu=atmega328old',
    )
  })

  it('appends normal menu options after an exact FQBN override', () => {
    expect(
      resolvePlatformOptions('jwplc:esp32:jwplcbasic', [packageSource, cpuOption], {
        packageSource: 'local',
        cpu: 'atmega328old',
      }),
    ).toBe('jwplc_local:esp32:jwplcbasic:cpu=atmega328old')
  })

  it('falls back to the manifest default when a persisted id is stale', () => {
    expect(resolvePlatformOptions('jwplc:esp32:jwplcbasic', [packageSource], { packageSource: 'missing' })).toBe(
      'jwplc:esp32:jwplcbasic',
    )
  })
})
'@

# All anchors were validated in memory above. Only now mutate the working trees.
Write-Utf8NoBom -Path $typesPath -Content $types
Write-Utf8NoBom -Path $compilerPath -Content $compiler
Write-Utf8NoBom -Path $boardPath -Content $board
Write-Utf8NoBom -Path $manifestPath -Content $manifest
Write-Utf8NoBom -Path $resolverPath -Content $resolver
Write-Utf8NoBom -Path $resolverTestPath -Content $resolverTest

Write-Host ''
Write-Host '=== OPENPLC DIFF CHECK ===' -ForegroundColor Yellow
& git -C $openplcRepoResolved diff --check
if ($LASTEXITCODE -ne 0) { throw 'git diff --check fallo en openplc-editor.' }
& git -C $openplcRepoResolved diff --stat

Write-Host ''
Write-Host '=== PLATFORM DIFF CHECK ===' -ForegroundColor Yellow
& git -C $platformRepoResolved diff --check
if ($LASTEXITCODE -ne 0) { throw 'git diff --check fallo en platform-jwplc.' }
& git -C $platformRepoResolved diff --stat

Write-Host ''
Write-Host '============================================================' -ForegroundColor Green
Write-Host ' SELECTOR APLICADO EN WORKING TREES' -ForegroundColor Green
Write-Host '============================================================' -ForegroundColor Green
Write-Host 'OPENPLC_PACKAGE_SOURCE_SELECTOR=APPLIED'
Write-Host 'VPP_SIGNATURE=STALE_REQUIRES_REBUILD'
Write-Host ''
Write-Host 'Siguiente paso obligatorio antes de importar el VPP:' -ForegroundColor Yellow
Write-Host '  openplc-editor-installers\v4.2.7\build-jwplc-vpp-v2.bat --no-clean-installed'
Write-Host ''
Write-Host 'No hagas commit todavia: primero ejecuta pruebas del editor y regenera signature.json.' -ForegroundColor Yellow
