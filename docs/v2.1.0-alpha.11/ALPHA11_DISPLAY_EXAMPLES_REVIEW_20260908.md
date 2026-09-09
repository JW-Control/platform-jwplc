# Alpha11 - Revisión de ejemplos JWPLC_Display no-HMI

Fecha: 2026-09-08
Branch: `v2.1.0-alpha.11/feature/hmi-designer`

## Alcance

Revisión de los ejemplos de `JWPLC_Display` que no dependen del HMI declarativo/Designer. Se priorizó compatibilidad con la API actual, retorno IDLE, reentrada USER, uso correcto del scheduler y convivencia con el bus SPI compartido.

Como cierre adicional se auditó el conjunto de ejemplos de las librerías `JWPLC_*` para retirar includes redundantes ya cubiertos por el autoload del package y verificar que no se enseñe API Display legacy.

## Resultado por ejemplo Display no-HMI

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
7. Los ejemplos integrados del JWPLC Basic no deben repetir headers que ya forman parte del autoload del package. Los drivers reutilizables (`JW_FRAM`, `JW_RTC`, `JW_SD`, `JW_MatrixButtons`) conservan sus headers propios en sus ejemplos para mantener autonomía fuera del flujo integrado.
8. Un include interno redundante puede retirarse sin eliminar la operación funcional asociada. En particular, los diagnósticos Ethernet siguen usando `jwplcSPI_acquire()/jwplcSPI_release()` para ownership del bus aunque ya no incluyan directamente `jwplc_spi_bus.h`.
9. El smoke final debe compilar los ejemplos modificados antes de integrar a `release/v2.1.x`.

## Gate de API legacy

Validación sobre `JWPLC/2.1.0/libraries/**/examples`:

```text
LEGACY_DISPLAY_FILES=0
ALPHA11_EXAMPLES_LEGACY_API=PASS
```

Esto confirma que los ejemplos públicos del package ya no invocan `jwplcUserDisplay*` ni la API histórica `JWPLCDisplay::`.

## Limpieza de includes/autoload

La auditoría global detectó includes explícitos redundantes en ejemplos integrados. Se retiraron 61 includes en 57 archivos sin alterar las operaciones de los ejemplos.

Entre los headers retirados cuando ya eran cubiertos por autoload están:

- `JWPLC_GlobalPeripherals.h`
- `JWPLC_Ethernet.h`
- `JWPLC_RS485.h`
- `JWPLC_ModbusRTU.h`
- `jwplc_spi_bus.h`
- `JWPLC_Display.h` en los tres ejemplos declarativos básicos que sólo necesitan la API liviana expuesta por el package.

Se conservaron headers específicos cuando sí aportan una API no incluida en el autoload general, por ejemplo `JWPLC_Ethernet_DNS.h`, y `JWPLC_Display.h` en juegos/dibujo directo que necesitan el tipo completo de la TFT.

## Gate global de compilación

Matriz local ejecutada sobre los 57 sketches modificados por la limpieza de includes:

```text
TOTAL=57
PASS=57
FAIL=0
ALPHA11_JWPLC_EXAMPLES_COMPILE=PASS
```

Resultado: la limpieza de includes no rompió discovery, compilación ni link del package Arduino.

## Cierre de revisión

La revisión de ejemplos Alpha11 queda cerrada respecto a:

- API Display moderna;
- retorno IDLE y reentrada USER;
- TFT declarativa sin parpadeo en diagnósticos Ethernet;
- eliminación de includes redundantes en ejemplos integrados;
- compilación de los 57 sketches modificados.

Pendiente del cierre general Alpha11: regenerar el archive precompilado final de `JWPLC_Display` desde el HEAD definitivo, verificar paridad source/archive y completar benchmark/documentación de release.
