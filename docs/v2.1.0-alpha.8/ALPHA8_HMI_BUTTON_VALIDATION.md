# Alpha8 — Validación física HMI, Display y botonera

Fecha: 2026-09-04

## Objetivo

Validar físicamente en JWPLC Basic que Alpha8:

- elimina el autowake inesperado observado durante el desarrollo;
- mantiene la botonera operativa e independiente del Display;
- permite entrada/salida USER explícita;
- soporta varias páginas HMI;
- actualiza valor, texto, booleano y barra;
- usa snapshots de I/O/RTC sin bloquear el runtime;
- no introduce flicker o congelamientos visibles.

## Sketch

```text
JWPLC/2.1.0/libraries/JWPLC_Display/examples/Display_Alpha8_HMI_Gate/Display_Alpha8_HMI_Gate.ino
```

FQBN de validación:

```text
jwplc_local:esp32:jwplcbasic
```

Puerto físico usado:

```text
COM4
```

## Gate de matriz de botones previo

Con wake deshabilitado se verificaron los seis botones:

```text
LEFT  -> mask 0x01
UP    -> mask 0x02
RIGHT -> mask 0x04
ESC   -> mask 0x08
OK    -> mask 0x10
DOWN  -> mask 0x20
```

La máscara regresó a cero después de cada liberación.

No se observaron eventos falsos sostenidos con la botonera sin tocar.

Marcadores:

```text
ALPHA8_GATE_A_BUTTON_MATRIX=PASS
ALPHA8_ALL_6_BUTTONS=PASS
ALPHA8_NO_SUSTAINED_RUNTIME_GHOST_BUTTONS=PASS
ALPHA8_IDLE_WITH_WAKE_DISABLED=PASS
```

## Reproducción del problema anterior

Durante el desarrollo se reprodujo una transición espontánea a USER con el wake anterior habilitado:

```text
[HB] ... mode=IDLE mask=0x0
[MODE CHANGE] USER
[HB] ... mode=USER mask=0x0
```

No hubo pulsación física deliberada.

Conclusión del gate de diagnóstico:

```text
ALPHA8_GATE_B_AUTOWAKE_REPRODUCED=PASS
ALPHA8_DISPLAY_FALSE_USER_TRANSITION=CONFIRMED
ALPHA8_ROOT_CAUSE_DISPLAY_AUTOWAKE=CONFIRMED
```

La corrección Alpha8 adopta `IDLE_WAKE_DISABLED` por defecto y desacopla la navegación interna del Display de los latches consumibles del sketch.

## Soak IDLE Alpha8

Después de cargar el gate final se dejó la placa sin tocar durante 180 s.

Extracto:

```text
[HB] t=174017 mode=IDLE inputs=0x0 rtc=23:31:21 unexpectedUser=0
[HB] t=175017 mode=IDLE inputs=0x0 rtc=23:31:22 unexpectedUser=0
[HB] t=176017 mode=IDLE inputs=0x0 rtc=23:31:23 unexpectedUser=0
[HB] t=177017 mode=IDLE inputs=0x0 rtc=23:31:24 unexpectedUser=0
[HB] t=178017 mode=IDLE inputs=0x0 rtc=23:31:25 unexpectedUser=0
[HB] t=179017 mode=IDLE inputs=0x0 rtc=23:31:26 unexpectedUser=0
[PASS] IDLE_SOAK_180S_NO_AUTOWAKE
[HB] t=180017 mode=IDLE inputs=0x0 rtc=23:31:27 unexpectedUser=0
```

Resultado:

```text
ALPHA8_IDLE_SOAK_180S=PASS
ALPHA8_NO_SPONTANEOUS_AUTOWAKE=PASS
ALPHA8_RTC_RUNTIME=PASS
ALPHA8_RUNTIME_CONTINUITY=PASS
```

## Navegación USER

### Entrada explícita con OK

```text
[BTN PRESS] OK count=1
[UI ENTER] EXPECTED_BY_SKETCH
[UI PAGE] 0
[BTN RELEASE] OK
[MODE] USER
```

Resultado:

```text
ALPHA8_USER_ENTRY_EXPLICIT=PASS
```

### Cambio de página

```text
[BTN PRESS] RIGHT count=1
[UI PAGE] 1
[BTN RELEASE] RIGHT
```

Posteriormente:

```text
[BTN PRESS] LEFT count=1
[UI PAGE] 0
[BTN RELEASE] LEFT
```

Resultado:

```text
ALPHA8_USER_PAGE_NAVIGATION=PASS
```

### Barra

```text
[BTN PRESS] UP count=1
[BAR] 10.0
[BTN PRESS] UP count=2
[BAR] 20.0
[BTN PRESS] UP count=3
[BAR] 30.0

[BTN PRESS] DOWN count=1
[BAR] 20.0
```

Resultado:

```text
ALPHA8_HMI_BAR_UPDATE=PASS
```

## ESC observado por Display y sketch

Gate:

```text
[BTN PRESS] ESC count=1
[UI EXIT] USER -> IDLE
[MODE] IDLE
[BTN RELEASE] ESC
```

El sketch recibió el `PRESS` y el Display realizó su retorno de forma independiente.

Resultado:

```text
ALPHA8_ESC_DISPLAY_AND_SKETCH=PASS
ALPHA8_DISPLAY_DOES_NOT_STEAL_APP_LATCH=PASS
```

## Botones en IDLE con wake deshabilitado

Después de regresar a IDLE se pulsaron:

```text
LEFT
UP
RIGHT
ESC
DOWN
```

El monitor registró los eventos de aplicación y `mode=IDLE` permaneció estable.

Ejemplo:

```text
[BTN PRESS] LEFT count=2
[BTN RELEASE] LEFT
[HB] ... mode=IDLE ... unexpectedUser=0

[BTN PRESS] UP count=4
[BAR] 30.0
[BTN RELEASE] UP
[HB] ... mode=IDLE ... unexpectedUser=0
```

Resultado:

```text
ALPHA8_BUTTONS_AVAILABLE_IN_IDLE=PASS
ALPHA8_IDLE_WAKE_DISABLED_DEFAULT=PASS
```

## Reentrada USER

OK volvió a abrir USER explícitamente:

```text
[BTN PRESS] OK count=2
[UI ENTER] EXPECTED_BY_SKETCH
[UI PAGE] 1
[MODE] USER
[BTN RELEASE] OK
```

Resultado:

```text
ALPHA8_USER_REENTRY=PASS
```

## Regresión de pulsación sostenida

Después de restaurar la semántica interna de `anyPressedOrRepeated()`, se recompiló/subió el gate y se mantuvo `UP` pulsado durante varios segundos.

Extracto:

```text
[BTN PRESS] UP count=1
[BAR] 10.0
[HB] t=945031 mode=IDLE inputs=0x0 rtc=00:04:58 unexpectedUser=0
[HB] t=946031 mode=IDLE inputs=0x0 rtc=00:04:59 unexpectedUser=0
[HB] t=947031 mode=IDLE inputs=0x0 rtc=00:05:00 unexpectedUser=0
[HB] t=948031 mode=IDLE inputs=0x0 rtc=00:05:01 unexpectedUser=0
[BTN RELEASE] UP
```

Se repitió una segunda pulsación sostenida y el heartbeat continuó.

Observación física:

```text
TFT operativa
sin congelamiento
sin flicker problemático observado
RTC avanzando
unexpectedUser=0
```

Resultado:

```text
ALPHA8_HELD_BUTTON_REGRESSION=PASS
ALPHA8_TFT_RUNTIME_STABILITY=PASS
```

## Funciones HMI cubiertas

El gate utiliza y valida físicamente:

```text
JWPLC_Display.setFields(...)
JWPLC_Display.setValue(...)
JWPLC_Display.setText(...)
JWPLC_Display.setBool(...)
JWPLC_Display.setBar(...)
JWPLC_Display.setUserPage(...)
JWPLC_Display.enterUserUI()
JWPLC_Display.goIdle()

JWPLC_IO.inputs()
JWPLC_Time.*

JWPLC_Buttons.pressed(...)
JWPLC_Buttons.released(...)
```

## Incidente histórico de taller

Durante un taller anterior se observaron bloqueos aparentes de IDLE/botonera en varias laptops/placas con comportamiento variable.

La reproducción exacta de todos esos casos históricos no quedó disponible de forma controlada y el entorno de cada laptop no fue preservado.

Para Alpha8 se adopta la siguiente clasificación:

```text
WORKSHOP_BUTTONS_FREEZE=HISTORICAL_INCIDENT
EXACT_ROOT_CAUSE=NOT_CONCLUSIVELY_REPRODUCED
```

No se atribuye retrospectivamente todo el incidente a una única causa demostrada.

Sin embargo, el problema se considera **resuelto operacionalmente** para continuar porque el runtime Alpha8 actual pasó:

- soak IDLE prolongado;
- navegación completa;
- independencia de eventos;
- reentrada/salida USER;
- pulsación sostenida;
- ejecución física continua sin congelamiento observado.

Para futuras reproducciones, si una laptop vuelve a presentar el problema, se recomienda preservar su entorno antes de reinstalar y registrar:

```text
Arduino IDE / CLI version
FQBN
package exacto
Used platform
Used library
SHA256 del binario
placa física
```

## Resultado final

```text
ALPHA8_HMI_HARDWARE_GATE=PASS
ALPHA8_BUTTON_RUNTIME=PASS
ALPHA8_DISPLAY_RUNTIME=PASS
ALPHA8_NO_AUTOWAKE=PASS
ALPHA8_APP_BUTTON_LATCHES=PASS
ALPHA8_HMI_FIELDS=PASS
ALPHA8_HMI_PAGES=PASS
ALPHA8_CACHED_RTC_IO=PASS
ALPHA8_PHYSICAL_FREEZE=PASS
```
