# Alpha11 - regresión de callbacks USER manuales

Fecha: 2026-09-08

Estado: **IMPLEMENTADO / PENDIENTE DE GATE FÍSICO**

## Observación física

Durante la revisión final de ejemplos, `Display_Tetris` permitió entrar a la pantalla del juego y la música continuó ejecutándose, pero:

- la pieza activa no descendía;
- el juego no avanzaba;
- los botones no producían acciones dentro del juego.

La música de Tetris se atiende desde `loop()`, mientras que caída, controles y redibujado del juego se atienden desde `jwplcUserDisplayRefreshCallback(...)`. Por ello la observación es consistente con un callback USER que deja de recibir frames mientras el `loop()` principal sigue vivo.

## Causa de compatibilidad localizada

El runtime base `JWPLC_Display.cpp` consulta `jwplcUserDisplayRefreshNeededCallback(...)` antes de adquirir el bus TFT y ejecutar el callback USER manual.

El default legacy delega en `jwplcUIRuntimeRefreshNeeded()`.

Cuando sólo están activos los hooks weak del Display base, ese hook devuelve `true` y preserva la cadencia manual. Sin embargo, si el motor HMI declarativo queda enlazado, `JWPLC_UI_API.cpp` aporta la implementación strong y delega en `JWPLCUI::refreshNeeded()`.

El motor HMI parte de `USER_REFRESH_ON_DEMAND`; sin fields/PixelMaps dirty, `refreshNeeded()` puede ser falso. En ese estado un callback USER manual puede quedar filtrado aunque el sketch haya configurado `setUserRefreshPeriodMs()`.

Esto es incompatible con ejemplos de dibujo directo como Tetris y FlappyBird, donde el callback USER es también el tick lógico del juego.

## Corrección Alpha11

`jwplcUIRuntimeRefreshNeeded()` ahora distingue entre:

1. **sin contenido HMI declarativo registrado** (`fieldCount()==0 && pixelMapCount()==0`): devuelve `true` y conserva el comportamiento periódico/manual del Display;
2. **con fields o PixelMaps declarativos**: conserva `JWPLCUI::refreshNeeded()` y por tanto ON_DEMAND/PERIODIC continúa funcionando como antes.

No se obliga a Tetris/FlappyBird a llamar `setUserRefreshMode(USER_REFRESH_PERIODIC)`, porque esa API pertenece al motor HMI y forzarla en ejemplos de TFT directa dañaría la intención de lazy-link.

Si una aplicación mezcla deliberadamente callbacks manuales con HMI declarativa y necesita tick periódico del callback, debe seleccionar `USER_REFRESH_PERIODIC` explícitamente.

## Paridad source / archive del gate de precompilación

Durante la regeneración del archive se detectaron dos asimetrías de enlace que se corrigieron antes de aceptar el precompilado:

1. El modo source sin `dot_a_linkage=true` podía enlazar directamente todos los objetos de `JWPLC_Display`, arrastrando TUs HMI no requeridos.
2. Una vez activado `dot_a_linkage=true`, el precompilado seguía usando `ldflags=-Wl,--undefined=JWPLC_Display`, mientras que source usaba el anchor real de `JWPLC_Display_Auto.h`, dejando una diferencia artificial de un símbolo en el `.map`.

La configuración final queda:

```text
dot_a_linkage=true
precompiled=full
```

sin `ldflags=-Wl,--undefined=JWPLC_Display`.

`JWPLC_Display_Auto.h` mantiene una referencia mínima al objeto global `JWPLC_Display` fuera de la fase de discovery. Así source y precompiled comparten el mismo mecanismo de autoload, conservan lazy-link por miembro y no requieren `--whole-archive`.

### Resultado final de paridad

Gate ejecutado el 2026-09-08 sobre:

```text
HEAD=d2296d73840318afe23388862dd051f06a061739
```

Resultado:

```text
DISPLAY_TUS=6
ARCHIVE_MEMBERS_EXACT=PASS
PRECOMPILED_DISPLAY_SOURCE_TUS=0
SOURCE_ARCHIVE_EMPTY_PARITY=PASS
SOURCE_ARCHIVE_HMI_PARITY=PASS
ALPHA11_DISPLAY_FINAL_ARCHIVE=PASS
```

Candidato generado:

```text
ARCHIVE_BYTES=849596
ARCHIVE_SHA256=2974d42c847c1b7c7ab3a7b74da42e2f17969fb852b47a8d434f57f70da924af
```

El archive contiene exactamente las seis TUs actuales de `JWPLC_Display` y los miembros extraídos coinciden byte a byte con los objetos source usados para generarlo.

La protección añadida en `jwplcUIRuntimeRefreshNeeded()` se mantiene además como defensa de compatibilidad para los casos en que el motor HMI sí quede enlazado legítimamente.

Este ajuste no cambia la API pública.

## Auditoría de ejemplos

Se revisaron los ejemplos actuales de `JWPLC_Display`.

### Correctos en entrada USER

- `02.Display_HMI_Fields`: entrada explícita con `enterUserUI()`.
- `03.Display_HMI_Pages`: entrada explícita con `enterUserUI()`.
- `04.Display_TFT_Direct`: entrada explícita con `enterUserUI()`.
- `Display_Alpha8_HMI_Gate`: entrada explícita y gate de botonera.
- `Display_Tetris`: wake explícito `IDLE_WAKE_BUTTON_ONLY` con `BTN_OK`.
- `Display_FlappyBird`: wake explícito `IDLE_WAKE_BUTTON_ONLY` con `BTN_OK`.

### Corregidos por asumir wake histórico

- `Display_DotAPI_Minimal`.
- `Display_Efficient_Redraw`.
- `Display_Idle_Return_Modes`.
- `Display_UserUI_Callbacks`.

Estos ejemplos ahora solicitan explícitamente `BTN_OK` + `IDLE_WAKE_BUTTON_ONLY`, coherente con el default seguro Alpha8+.

## CI

El smoke general del package añade compilación de:

- `Display_UserUI_Callbacks`;
- `Display_Tetris`;
- `Display_FlappyBird`.

La compilación CI no sustituye el gate físico de avance del callback.

## Gate físico requerido

### Tetris

Esperado tras cargar `Display_Tetris`:

- `OK` entra al juego;
- la música continúa;
- la pieza desciende automáticamente (~650 ms con velocidad 100 %);
- `LEFT/RIGHT` mueven;
- `DOWN` acelera descenso;
- `UP` hace hard drop;
- `OK` rota;
- `ESC` retorna a IDLE;
- `Measured FPS` deja de permanecer en 0 y se aproxima al objetivo configurado;
- sin congelamiento ni reset.

### FlappyBird

Esperado:

- `OK` entra/inicia;
- escenario y física avanzan;
- `OK/UP` producen salto;
- audio continúa;
- `ESC` retorna a IDLE;
- sin congelamiento ni reset.

## Criterio de cierre

El candidato `libJWPLC_Display.a` ya pasó paridad source/precompiled, pero **no se congela ni publica todavía** hasta completar el gate físico de Tetris y FlappyBird. El archive final debe corresponder al mismo source corregido y conservar el SHA-256 registrado o ser regenerado y documentado nuevamente si cambia cualquier TU.

```text
A11_DISPLAY_MANUAL_CALLBACK_COMPAT=IMPLEMENTED_PENDING_PHYSICAL_GATE
A11_DISPLAY_SOURCE_ARCHIVE_LINKAGE=PASS
A11_DISPLAY_PRECOMPILED_PARITY=PASS
A11_TETRIS_RUNTIME=PHYSICAL_GATE_PENDING
A11_FLAPPY_RUNTIME=PHYSICAL_GATE_PENDING
```
