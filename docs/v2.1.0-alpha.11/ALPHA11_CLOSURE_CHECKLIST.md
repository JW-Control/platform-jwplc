# Alpha11 — Checklist de cierre

Fecha de cierre: 2026-09-09.

## Alcance

- [x] JWPLC HMI Designer V1 cerrado.
- [x] TEXT / VALUE / BOOL / BAR validados.
- [x] Multipágina y selector `NN/TT` validados.
- [x] PixelMap RGB565 y herramientas de edición validadas.
- [x] LIVE Preview validado físicamente.
- [x] Codegen y proyectos `.jwhmi` validados.
- [x] Aplicación Windows standalone validada.
- [x] Arduino IDE 2.3.4 launcher validado.
- [x] Autocompletado contextual `JWPLC_Display` validado.

## Runtime

- [x] Botonera robusta con loop intensivo.
- [x] `digitalWrite()` repetido no requiere detección manual de flanco.
- [x] `digitalWrite()` repetido no requiere `delay(1)`.
- [x] RTC continúa recibiendo servicio.
- [x] Display continúa recibiendo servicio.
- [x] TCA6424A evita escrituras físicas redundantes mediante shadow.
- [x] `jwplcSystemTask` queda coherente con el runtime Alpha11.

```text
A11_BUTTON_ROBUSTNESS=PASS_PHYSICAL
ALPHA11_RUNTIME_CLOSED_LOOP=PASS_PHYSICAL
ALPHA11_DIGITALWRITE_REPEATED_STATE=PASS_PHYSICAL
ALPHA11_USER_DELAY_REQUIRED=NO
```

## Precompilación

- [x] `core.a` JWPLC Basic regenerado desde source Alpha11.
- [x] Build fuente del core PASS.
- [x] Target normal usa stub + archive.
- [x] `core.a` probado físicamente.
- [x] `JWPLC_Display` precompilado validado.
- [x] 0 TUs fuente de Display en el build precompilado normal.
- [x] Paridad source/archive de Display PASS.

```text
CORE_SHA256=6edf40d105936318a2fd8a84d7f0724571657910e8d92e8538640ec613f4dd68
DISPLAY_SHA256=2974d42c847c1b7c7ab3a7b74da42e2f17969fb852b47a8d434f57f70da924af
```

## Benchmark

- [x] Tres réplicas completas.
- [x] 72/72 fases PASS.
- [x] Basic cold = 15 compiladores.
- [x] Core cold = 78 compiladores.
- [x] Warm = 1 compilador.
- [x] Sin regresión material de tamaño binario.
- [x] No se reclama porcentaje exacto de speedup global por variación del host.

## CI y PR técnico

- [x] PR #95 abierto contra `release/v2.1.x`.
- [x] CI smoke inicial PASS.
- [x] Bootstrap CI actualizado de Alpha5 a Alpha10.
- [x] CI verifica paridad byte-a-byte del overlay del branch.
- [x] Segundo CI definitivo PASS.
- [x] PR #95 integrada mediante Squash and merge.

```text
PR95_MERGE_SHA=64ce22447e0a9b5852ed83cb5f3a1bd2de3aa218
```

## Publicación

- [x] Auto Release detectó Alpha11.
- [x] `Release JWPLC Arduino Package` #20 = SUCCESS.
- [x] Tag `v2.1.0-alpha.11` creado.
- [x] PreRelease publicada.
- [x] ZIP con raíz única `2.1.0/`.
- [x] SHA-256 y tamaño registrados.
- [x] PR #96 de índice dev integrada a `main`.
- [x] Índice dev publica Alpha11.
- [x] Índice estable continúa en `v2.0.0`.

```text
TAG=v2.1.0-alpha.11
PUBLISHED_PACKAGE_SOURCE_SHA=64ce22447e0a9b5852ed83cb5f3a1bd2de3aa218
ZIP=jwplc-esp32-2.1.0-alpha.11.zip
SIZE=24547524
SHA256=465440cf92491b3c9050aa44b6184afae7bfa8e52993d6bc777487d203a97474
PACKAGE_ROOT=2.1.0/
```

## Validación del package publicado

- [x] Se usó Arduino CLI con directorios aislados bajo `%TEMP%`.
- [x] Se instaló exactamente `jwplc:esp32@2.1.0-alpha.11` desde el índice dev oficial.
- [x] `core.a` publicado coincide con el archive probado.
- [x] `libJWPLC_Display.a` publicado coincide con el archive probado.
- [x] Compilación desde el package publicado = PASS.
- [x] Upload físico desde el package publicado = PASS.
- [x] TFT operativo después del upload.
- [x] RTC continúa actualizándose.
- [x] Q0_0 conmuta continuamente.
- [x] Sin congelamiento del Display.
- [x] Sin reset inesperado.
- [x] Sin `delay(1)`.

```text
ALPHA11_PUBLISHED_INSTALL=PASS
ALPHA11_PUBLISHED_ARCHIVE_PARITY=PASS
ALPHA11_PUBLISHED_COMPILE=PASS
ALPHA11_PUBLISHED_UPLOAD=PASS
ALPHA11_PUBLISHED_RUNTIME=PASS
ALPHA11_PUBLISHED_NO_DELAY_GATE=PASS
```

## Periféricos y compatibilidad

- [x] Display permanece integrado.
- [x] Ethernet permanece integrado.
- [x] microSD permanece integrada.
- [x] FRAM permanece integrada.
- [x] RTC permanece integrado.
- [x] Botonera permanece integrada.
- [x] RS-485 permanece integrado.
- [x] Modbus RTU permanece integrado.
- [x] TCA/I/O permanece integrado.
- [x] APIs previamente probadas preservadas cuando corresponde.

```text
AUTOLOAD_PERIPHERALS_REMOVED=NO
```

## Decisiones heredadas

- [x] App-only continúa como herramienta validada de desarrollo, no upload default.
- [x] Bootloader precompilado no adoptado.
- [x] No se publica `bootloader.bin` definitivo.
- [x] Perfil Flash actual validado; configuración universal final continúa pendiente.
- [x] OTA no definido.
- [x] OpenPLC fuera del autoload Arduino normal.

## Documentación

- [x] README raíz actualizado a estado Alpha11 publicado.
- [x] `ALPHA11_STATUS.md` actualizado.
- [x] `ALPHA11_CLOSURE_CHECKLIST.md` actualizado.
- [x] `PRE_RELEASE.md` actualizado con metadatos finales.
- [x] `ALPHA11_PUBLISHED_VALIDATION.md` creado.

## Sincronización final

- [x] Índice dev Alpha11 incorporado al árbol canónico de `release/v2.1.x`.
- [x] Sincronización hacia `main` diseñada desde un branch nacido en `main`.
- [x] El árbol objetivo del branch de sync es exactamente el árbol final de `release/v2.1.x`.
- [x] Paridad final se evalúa por tree SHA/contenido, no por ancestry.
- [x] `main` y `release/v2.1.x` terminan con tree SHA idéntico.
- [x] No se realizan commits unilaterales después del sync final.

```text
TREE_PARITY_CRITERION=PASS
GIT_ANCESTRY_PARITY=NOT_REQUIRED
ALPHA11_RELEASE_MAIN_TREE_PARITY=PASS
```

## Estado final

```text
ALPHA11_TECHNICAL_CLOSURE=PASS
ALPHA11_RELEASE_PUBLICATION=PASS
ALPHA11_DEV_INDEX=PASS
ALPHA11_STABLE_INDEX_UNCHANGED=PASS
ALPHA11_PUBLISHED_PACKAGE_GATE=PASS
ALPHA11_RELEASE_MAIN_TREE_PARITY=PASS
ALPHA11_STATUS=CLOSED_PUBLISHED
NEXT_ALPHA=UNBLOCKED
```
