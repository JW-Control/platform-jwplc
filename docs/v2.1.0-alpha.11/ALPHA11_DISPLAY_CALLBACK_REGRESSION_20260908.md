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

No regenerar ni congelar el `libJWPLC_Display.a` final de Alpha11 hasta que esta corrección pase el gate físico. El archive debe construirse desde el source ya corregido.

```text
A11_DISPLAY_MANUAL_CALLBACK_COMPAT=IMPLEMENTED_PENDING_PHYSICAL_GATE
A11_TETRIS_RUNTIME=PHYSICAL_GATE_PENDING
A11_FLAPPY_RUNTIME=PHYSICAL_GATE_PENDING
```
