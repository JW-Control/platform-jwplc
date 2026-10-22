# Alpha13 — TFT-PRE1 — Baseline de arranque visual

Fecha: 2026-10-09

## Resultado

```text
GATE=A13-TFT-PRE1
STATUS=PASS
REASON=BASELINE_TIMING_CAPTURED
PRODUCT_FAILURE=NO
HARNESS_FAILURE=NO
HARDWARE_FAILURE=NO
ENVIRONMENT_FAILURE=NO
```

## Ruta validada

```text
POWER_SOURCE=USB_ONLY
24VDC=DISCONNECTED
HEAD=ce7831e214a88551a85acf2be83dad569131881f
SERIAL_PORT=COM4
CORE_SHA256=6f328eeb796091070c8d852a2c0e90f71047d8d2b5786fe3cd5079b2fb6ff983
USES_STUB_CORE=True
USES_SOURCE_CORE=False
CORE_A_LINKED=True
```

## Runtime observado

```text
SETUP_ENTRY_MS=664
UPTIME_MS=1797
DISPLAY_READY=YES
IO_READY=YES
TFT_RST_OUTPUT_ENABLE=YES
TFT_RST_OUTPUT_LATCH=HIGH
TFT_CS_OUTPUT_ENABLE=NO
TFT_CS_OUTPUT_LATCH=LOW
```

## Higiene

```text
TRACKED_DIRTY_FINAL=0
STAGED_FINAL=0
DIFF_CHECK_FINAL=True
```

## Interpretación

`setup()` se alcanza a 664 ms según el reloj Arduino y el Display ya está
ready en ese punto. Por tanto, PRE1 no respalda una explicación de varios
segundos de inicialización firmware antes del primer frame.

PRE1 todavía no mide el instante exacto de `initPeripherals()`, la aserción de
RST ni el retorno del primer refresh. Tampoco convierte el estado de CS
observado después del init en contrato: la pantalla funciona y ese dato se
conserva sólo como diagnóstico.

## Siguiente paso

TFT-PRE2 instrumentará temporalmente el core source para medir hitos internos
de arranque y restaurará los archivos byte-for-byte antes de subir el binario.
No se cambia todavía el comportamiento productivo.
