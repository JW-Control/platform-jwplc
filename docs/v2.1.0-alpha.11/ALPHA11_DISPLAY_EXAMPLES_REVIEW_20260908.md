# Alpha11 - Revisión de ejemplos JWPLC_Display no-HMI

Fecha: 2026-09-08
Branch: `v2.1.0-alpha.11/feature/hmi-designer`

## Alcance

Revisión de los ejemplos de `JWPLC_Display` que no dependen del HMI declarativo/Designer. Se priorizó compatibilidad con la API actual, retorno IDLE, reentrada USER, uso correcto del scheduler y convivencia con el bus SPI compartido.

## Resultado por ejemplo

| Ejemplo | Resultado | Decisión |
|---|---|---|
| `01.Display_IDLE_Status` | PASS | Mantener. Usa sólo IDLE y redibujos explícitos. |
| `04.Display_TFT_Direct` | AJUSTADO | Wake central `OK -> USER`; conserva callback legacy de refresh porque consume `io/rtc`. |
| `Display_DotAPI_Minimal` | PASS | Mantener. Es la prueba mínima de la API liviana/autoload. |
| `Display_Efficient_Redraw` | AJUSTADO | Migrado a callbacks cortos y corregida la caché de reentrada USER. |
| `Display_Idle_Return_Modes` | AJUSTADO | Migrado a callbacks cortos; mantiene las tres políticas demostradas. |
| `Display_UserUI_Callbacks` | AJUSTADO | Migrado a `jwplcUIEnter/Update/Exit`; eliminada la doble temporización interna. |
| `Display_Tetris` | GATE FÍSICO PASS | Juego, botones y audio operativos; el retorno IDLE debe heredar el periodo por defecto del package. |
| `Display_FlappyBird` | GATE FÍSICO PASS | Juego, botones y audio operativos; mismo criterio de retorno IDLE que Tetris. |

## Hallazgo de retorno Tetris -> IDLE

La demora percibida no provenía del audio ni de `goIdle()`. Los juegos fijaban `setIdleRefreshPeriodMs(250)`, mientras el IDLE completo se reconstruye en cuatro fases.

Con periodo equivalente al default efectivo del package (20 ms por clamp del core), se midieron retornos Tetris -> IDLE de:

- 137 ms
- 116 ms
- 116 ms
- 116 ms

El tiempo de dibujo acumulado de las cuatro fases fue aproximadamente 94-95 ms. No se requiere un modo turbo adicional en el runtime.

## Reglas resultantes

1. Los ejemplos no deben fijar `IdleRefreshPeriodMs` salvo que ese parámetro sea parte explícita de la demostración.
2. Para código nuevo sin necesidad de snapshots `io/rtc`, usar `jwplcUIEnter()`, `jwplcUIUpdate()` y `jwplcUIExit()`.
3. El callback legacy `jwplcUserDisplayRefreshCallback(io, rtc)` se conserva y sigue siendo válido cuando el ejemplo necesita esos snapshots; `04.Display_TFT_Direct` documenta ese caso.
4. La cadencia USER debe expresarse con `setUserRefreshPeriodMs()` y no duplicarse con temporizadores internos salvo que la lógica del ejemplo realmente lo requiera.
5. Toda pantalla que hace `fillScreen()` al entrar debe reinicializar su caché de regiones dinámicas para soportar reentrada rápida.
6. El smoke CI debe compilar todos los ejemplos no-HMI antes de integrar a `release/v2.1.x`.

## Cierre de revisión

La revisión de diseño/código de ejemplos no-HMI queda cerrada con los ajustes anteriores. Pendiente del cierre general Alpha11: aplicar la limpieza final de Tetris/Flappy sobre el branch, regenerar el archive precompilado sin profiler diagnóstico y ejecutar el smoke/validación final de compilación.
