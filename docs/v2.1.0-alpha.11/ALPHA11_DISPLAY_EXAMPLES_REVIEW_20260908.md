# Alpha11 - Revisión de ejemplos JWPLC_Display no-HMI

Fecha: 2026-09-08
Branch: `v2.1.0-alpha.11/feature/hmi-designer`

## Alcance

Revisión de los ejemplos de `JWPLC_Display` que no dependen del HMI declarativo/Designer. Se priorizó compatibilidad con la API actual, retorno IDLE, reentrada USER, uso correcto del scheduler y convivencia con el bus SPI compartido.

## Resultado por ejemplo

| Ejemplo | Resultado | Decisión |
|---|---|---|
| `01.Display_IDLE_Status` | PASS | Mantener. Usa sólo IDLE y redibujos explícitos. |
| `04.Display_TFT_Direct` | AJUSTADO | Migrado a `jwplcUIEnter()/jwplcUIUpdate()` y lectura de hora mediante `JWPLC_Time`; no expone callbacks históricos. |
| `Display_DotAPI_Minimal` | PASS | Mantener. Es la prueba mínima de la API liviana/autoload. |
| `Display_Efficient_Redraw` | AJUSTADO | Migrado a callbacks cortos y corregida la caché de reentrada USER. |
| `Display_Idle_Return_Modes` | AJUSTADO | Migrado a callbacks cortos; mantiene las tres políticas demostradas. |
| `Display_UserUI_Callbacks` | AJUSTADO | Migrado a `jwplcUIEnter/Update/Exit`; eliminada la doble temporización interna. |
| `Display_Tetris` | GATE FÍSICO PASS | Juego, botones y audio operativos; usa API corta y el retorno IDLE hereda el periodo por defecto del package. |
| `Display_FlappyBird` | GATE FÍSICO PASS | Juego, botones y audio operativos; usa API corta y el mismo criterio de retorno IDLE que Tetris. |

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
2. Para código nuevo de UI manual usar `jwplcUIEnter()`, `jwplcUIUpdate()` y `jwplcUIExit()`.
3. Los callbacks históricos `jwplcUserDisplay*` se conservan únicamente como compatibilidad interna para sketches existentes; no forman parte de la API recomendada y no deben aparecer en ejemplos públicos nuevos.
4. Cuando un ejemplo necesita información de periféricos debe preferir las vistas/APIs públicas actuales, por ejemplo `JWPLC_Time`, en lugar de depender de snapshots del callback histórico.
5. La cadencia USER debe expresarse con `setUserRefreshPeriodMs()` y no duplicarse con temporizadores internos salvo que la lógica del ejemplo realmente lo requiera.
6. Toda pantalla que hace `fillScreen()` al entrar debe reinicializar su caché de regiones dinámicas para soportar reentrada rápida.
7. El smoke CI debe compilar todos los ejemplos no-HMI modernizados antes de integrar a `release/v2.1.x`.

## Gate de API legacy

Validación local sobre `JWPLC/2.1.0/libraries/**/examples/*.ino`:

```text
ALPHA11_EXAMPLES_LEGACY_API=PASS
```

Esto confirma que los ejemplos públicos del package ya no invocan `jwplcUserDisplay*`.

## Cierre de revisión

La revisión de diseño/código de ejemplos no-HMI queda cerrada respecto a API y comportamiento. Pendiente del cierre general Alpha11: ejecutar la matriz final de compilación de los ejemplos modernizados, retirar cualquier instrumentación diagnóstica temporal, regenerar el archive precompilado final de `JWPLC_Display` y completar benchmark/documentación de release.
