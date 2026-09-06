# Alpha11 · Gate de robustez de botonera

## Contexto

Durante A11-4 se reprodujo de forma intermitente un bloqueo de interacción al entrar en `PAGE_CONTENT` cuando el `loop()` consulta `JWPLC_Buttons.pressed(...)` de forma intensiva y sin `delay()`.

Patrón observado:

- Navegación `PAGE_SELECT` operativa.
- Página 01 y Página 02 podían entrar/salir normalmente.
- Página 03, que ejecutaba lógica `pressed(BTN_UP)` / `pressed(BTN_DOWN)` en un loop cerrado, podía dejar de responder al entrar con `OK`.
- Añadir `Serial.printf(...)` o un `delay(1)` al loop hacía desaparecer el fallo durante múltiples reinicios.
- El scanner real del JWPLC Basic es `jwplcBtnScan` en `JWPLC_GlobalPeripherals`; `JWPLC_Buttons.taskRunning()` no representa ese task y no debe usarse para diagnosticarlo.

Esto es consistente con contención/scheduling entre el loop del usuario y el task de scan, pero la causa se considera **pendiente de cierre físico** hasta superar el gate sin Serial ni delays de usuario.

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

## Gate físico obligatorio

Usar el sketch HMI generado real, **sin Serial de diagnóstico y sin `delay()` añadido por el usuario**.

Repetir al menos 10 ciclos de reset/subida con:

```text
01 -> OK -> ESC
02 -> OK -> ESC
03 -> OK
UP UP DOWN
ESC
```

En Página 03 mantener la lógica intensiva:

```cpp
if (!JWPLC_Display.isUserPageSelection() &&
    JWPLC_Display.userPage() == PAGE_PAGINA_3)
{
    if (JWPLC_Buttons.pressed(BTN_UP))
        nivel11 += 10.0f;

    if (JWPLC_Buttons.pressed(BTN_DOWN))
        nivel11 -= 10.0f;
}
```

Validar adicionalmente el caso de latches pendientes:

```text
entrar en una página
OK varias veces
ESC
```

Esperado: vuelve una sola vez a `PAGE_SELECT` y permanece allí; no debe reentrar por un `OK` pendiente.

## Criterio de cierre

```text
BUTTON_SCAN_TIGHT_LOOP_NO_DELAY=PASS
BUTTON_SCAN_MULTI_RESET=PASS
PAGE_CONTENT_ESC_CLEANUP=PASS
PENDING_OK_NO_REENTRY=PASS
SERIAL_REQUIRED=NO
USER_DELAY_REQUIRED=NO
```

Hasta completar este gate:

```text
A11_BUTTON_ROBUSTNESS=IMPLEMENTED_PENDING_PHYSICAL_GATE
```
