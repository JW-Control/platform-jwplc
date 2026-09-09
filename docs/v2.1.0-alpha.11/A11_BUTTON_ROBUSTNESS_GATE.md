# Alpha11 · Gate de robustez de botonera

## Estado

```text
A11_BUTTON_ROBUSTNESS=PASS_PHYSICAL
BUTTON_SCAN_TIGHT_LOOP_NO_DELAY=PASS
BUTTON_SCAN_MULTI_RESET=PASS
PAGE_CONTENT_ESC_CLEANUP=PASS
PENDING_OK_NO_REENTRY=PASS
SERIAL_REQUIRED=NO
USER_DELAY_REQUIRED=NO
```

## Contexto

Durante A11-4 se reprodujo de forma intermitente un bloqueo de interacción al entrar en `PAGE_CONTENT` cuando el `loop()` consulta `JWPLC_Buttons.pressed(...)` de forma intensiva y sin `delay()`.

Patrón observado:

- Navegación `PAGE_SELECT` operativa.
- Página 01 y Página 02 podían entrar/salir normalmente.
- Página 03, que ejecutaba lógica `pressed(BTN_UP)` / `pressed(BTN_DOWN)` en un loop cerrado, podía dejar de responder al entrar con `OK`.
- Añadir `Serial.printf(...)` o un `delay(1)` al loop hacía desaparecer el fallo durante múltiples reinicios.
- El scanner real del JWPLC Basic es `jwplcBtnScan` en `JWPLC_GlobalPeripherals`; `JWPLC_Buttons.taskRunning()` no representa ese task y no debe usarse para diagnosticarlo.

La reproducción fue importante porque el comportamiento coincide con los bloqueos intermitentes observados previamente durante ejercicios de botonera en taller: un sketch de usuario no debe necesitar `delay()` ni tráfico Serial para que `pressed()` sea robusto.

## Fix 1 · Robustez del scanner

`jwplcBtnScan` mantiene:

- periodo: `5 ms`
- core: `ARDUINO_RUNNING_CORE`
- stack: `4096 bytes`

Cambio Alpha11:

- prioridad del task: `1 -> 2`
- se comprueba el resultado de `xTaskCreatePinnedToCore()` y `begin()` falla explícitamente si el task no puede crearse.

Objetivo: el scanner debe poder adelantarse a un `loop()` agresivo. Si el scanner de mayor prioridad espera el mutex de `JW_MatrixButtons`, la herencia de prioridad del mutex permite que el poseedor termine y libere el recurso sin exigir `delay()` ni tráfico Serial en el sketch.

## Fix 2 · Limpieza CONTENT -> SELECT

Al pulsar `ESC` dentro de `PAGE_CONTENT`:

1. se vuelve a `PAGE_SELECT`;
2. se limpian PRESS/RELEASE/REPEAT/eventos pendientes de la botonera;
3. se sincroniza el snapshot físico `g_previousMask` con el estado actual.

Objetivo: un `OK` u otra tecla pendiente usada dentro de la aplicación no puede sobrevivir a la transición y provocar un reingreso fantasma al contenido.

## Evidencia física

Se validó el sketch HMI generado real, sin Serial de diagnóstico y sin `delay()` añadido por el usuario.

Se realizaron múltiples reinicios/subidas y se recorrieron repetidamente las tres páginas:

```text
01 -> OK -> ESC
02 -> OK -> ESC
03 -> OK
UP / DOWN
ESC
```

Resultado:

- entrada/salida estable en todas las páginas;
- `pressed(BTN_UP)` / `pressed(BTN_DOWN)` operativos en loop cerrado;
- BAR actualizado correctamente desde la lógica de usuario;
- no se requirió `Serial.begin()`, `Serial.printf()` ni `delay(1)`;
- `ESC` retorna a `PAGE_SELECT` de forma estable;
- múltiples `OK` dentro de `PAGE_CONTENT` ya no provocan reingreso al salir con `ESC`.

## Criterio de cierre

```text
BUTTON_SCAN_TIGHT_LOOP_NO_DELAY=PASS
BUTTON_SCAN_MULTI_RESET=PASS
PAGE_CONTENT_ESC_CLEANUP=PASS
PENDING_OK_NO_REENTRY=PASS
SERIAL_REQUIRED=NO
USER_DELAY_REQUIRED=NO
A11_BUTTON_ROBUSTNESS=PASS_PHYSICAL
```

## Nota obligatoria para README

Al documentar la botonera del JWPLC Basic debe quedar explícito:

- el package mantiene el escaneo automáticamente en segundo plano;
- el sketch normal no debe llamar `JWPLC_Buttons.update()` ni crear un segundo scanner;
- `pressed()` / `released()` pueden usarse directamente desde un `loop()` sin añadir delays artificiales;
- los eventos son latcheados/consumibles;
- al diseñar máquinas de estados o navegación, conviene limpiar eventos pendientes al cambiar de contexto cuando una pulsación vieja no debe sobrevivir a la transición.

```text
README_BUTTON_ROBUSTNESS_NOTE=REQUIRED_AT_ALPHA11_CLOSE
```
