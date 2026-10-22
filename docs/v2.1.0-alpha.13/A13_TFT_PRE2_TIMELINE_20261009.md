# Alpha13 — TFT-PRE2 — Timeline interno de arranque

Fecha: 2026-10-09

## Resultado

```text
GATE=A13-TFT-PRE2
STATUS=PASS
REASON=INTERNAL_TIMELINE_CAPTURED
PRODUCT_FAILURE=NO
HARNESS_FAILURE=NO
HARDWARE_FAILURE=NO
ENVIRONMENT_FAILURE=NO
```

## Ruta validada

```text
HEAD=b2312188ecb38bafe4292db83a13d9065454c80b
SERIAL_PORT=COM4
CORE_SHA256=6f328eeb796091070c8d852a2c0e90f71047d8d2b5786fe3cd5079b2fb6ff983
USES_SOURCE_CORE=True
USES_STUB_CORE=False
CORE_A_LINKED=False
SOURCE_TU_COUNT=64
PERIPHERALS_INIT_COUNT=1
FULL_PROFILE=True
```

## Timeline

```text
INIT_ENTRY_US=73873
RST_LOW_US=73949
I2C_END_US=76778
RTC_END_US=79801
FRAM_END_US=80425
SD_END_US=86274
BUTTONS_END_US=87091
DISPLAY_BEGIN_START_US=87094
DISPLAY_BEGIN_END_US=645274
DISPLAY_REFRESH_END_US=659341
TCA_INIT_END_US=660126
INIT_END_US=663997
SETUP_ENTRY_US=664542
```

Derivados:

```text
BOOT_TO_INIT_ENTRY_US=73873
INIT_TO_RST_US=76
RST_TO_DISPLAY_START_US=13145
DISPLAY_BEGIN_DURATION_US=558180
FIRST_REFRESH_DURATION_US=14067
BOOT_TO_FIRST_REFRESH_US=659341
FIRST_REFRESH_TO_SETUP_US=5201
```

## Conclusión técnica

El cuello de botella visual está concentrado dentro de `jwplcDisplayBeginCallback()` / `JWPLC_TFT.begin()`: 558.180 ms.

Todos los periféricos previos al Display consumen aproximadamente 13.2 ms desde RST LOW hasta el inicio del Display. El primer refresh del IDLE consume ~14.1 ms. Por tanto RTC, FRAM, SD, botonera y el redraw por fases no explican la ventana principal observada.

Esto confirma que el siguiente diagnóstico debe atribuir el tiempo a la secuencia interna del backend TFT_eSPI 2.5.43 y no reordenar periféricos.

## Higiene

```text
SOURCE_RESTORED=True
CORE_SHA_PRESERVED=True
WORKTREE_FINAL=CLEAN
DIFF_CHECK_FINAL=True
```

## Siguiente paso

TFT-PRE3: identificar de forma reproducible la instalación TFT_eSPI 2.5.43 usada para mantenimiento, verificar su source path y caracterizar estáticamente la secuencia ST7789 antes de construir un candidato productivo.
