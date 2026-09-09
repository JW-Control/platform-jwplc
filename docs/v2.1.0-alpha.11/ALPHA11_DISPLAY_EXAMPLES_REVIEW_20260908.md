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

## Archive precompilado final de JWPLC_Display

Finalizer ejecutado desde el HEAD `bc21768860498ae18154d91431abfe983cb3c04a` con source vacío, source HMI, archive vacío y archive HMI.

Resultado:

```text
ARCHIVE_BYTES=849596
ARCHIVE_SHA256=2974d42c847c1b7c7ab3a7b74da42e2f17969fb852b47a8d434f57f70da924af
DISPLAY_TUS=6
ARCHIVE_MEMBERS_EXACT=PASS
PRECOMPILED_DISPLAY_SOURCE_TUS=0
SOURCE_ARCHIVE_EMPTY_PARITY=PASS
SOURCE_ARCHIVE_HMI_PARITY=PASS
ALPHA11_DISPLAY_FINAL_ARCHIVE=PASS
```

El archive quedó versionado en:

```text
JWPLC/2.1.0/libraries/JWPLC_Display/src/esp32/libJWPLC_Display.a
```

Commit que lo publica:

```text
4142f801fbacc9388bf63c3a6352696522d6b445
```

La configuración pública asociada se mantiene en `JWPLC_Display/library.properties` con `dot_a_linkage=true` y `precompiled=full`.

## Benchmark final asociado

El benchmark final de Alpha11 quedó documentado en:

```text
docs/v2.1.0-alpha.11/ALPHA11_BUILD_BENCHMARK.md
```

Resultado:

```text
TOTAL_PHASES=72
FAILED_PHASES=0
ALPHA11_BUILD_BENCHMARK_3X=PASS
ALPHA11_COMPILER_STRUCTURE_PARITY=PASS
ALPHA11_WARM_PERFORMANCE_STABLE=PASS
ALPHA11_BINARY_SIZE_REGRESSION=MATERIAL_NO
ALPHA11_DISPLAY_PRECOMPILED_SOURCE_AVOIDANCE=PASS
ALPHA11_DISPLAY_SOURCE_ARCHIVE_PARITY=PASS
ALPHA11_EXACT_SPEEDUP_CLAIM=NOT_USED
```

El archive evita recompilar las TUs de `JWPLC_Display`, pero el tiempo global warm del package se mantiene esencialmente en el mismo rango que Alpha10. No se usa una afirmación porcentual artificial de aceleración total.

## Cierre de revisión

La revisión de ejemplos, el cierre técnico del precompilado Display y el benchmark Alpha11 quedan cerrados respecto a:

- API Display moderna;
- retorno IDLE y reentrada USER;
- TFT declarativa sin parpadeo en diagnósticos Ethernet;
- eliminación de includes redundantes en ejemplos integrados;
- compilación de los 57 sketches modificados;
- archive final con 6 TUs exactas y paridad source/archive;
- cero TUs de `JWPLC_Display` recompiladas al usar el archive final;
- benchmark final de 72 fases sin fallos y rendimiento warm estable frente a Alpha10.

Pendiente del cierre general Alpha11: cerrar documentación de release/README, revisar reproducibilidad del empaquetado HMI Designer y preparar PR/PreRelease en español.