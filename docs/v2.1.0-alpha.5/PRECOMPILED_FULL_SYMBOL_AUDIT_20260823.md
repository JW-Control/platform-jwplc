# Alpha5 - auditoría completa de símbolos JWPLC en archives precompilados

Fecha: 2026-08-23.

## Motivo

La auditoría inicial de precompilados sólo detectaba símbolos con forma `jwplc_...`. Durante el piloto de `JWPLC_Display` el gate de enlace genérico reveló una referencia no detectada previamente:

```txt
jwplcSystemForceDisplayRefresh
```

El auditor fue corregido para clasificar cualquier símbolo externo cuyo nombre comience por `jwplc`, manteniendo como bridge-compatible únicamente:

```txt
jwplc_pinMode
jwplc_digitalWrite
jwplc_digitalRead
```

## Resultado de la auditoría corregida

Con 12 archives presentes y `-AllowGenericGpioBridge`:

```txt
[BRIDGE] Adafruit_BusIO/libAdafruit_BusIO.a: jwplc_digitalWrite, jwplc_pinMode
[BRIDGE] Adafruit_GFX_Library/libAdafruit_GFX_Library.a: jwplc_digitalRead, jwplc_digitalWrite, jwplc_pinMode
[PASS] Adafruit_ST7735_and_ST7789_Library/libAdafruit_ST7735_and_ST7789_Library.a
[PASS] FS/libFS.a
[PASS] JW_FRAM/libJW_FRAM.a
[FAIL] JW_RTC/libJW_RTC.a: jwplcI2C_begin, jwplcI2C_beginWithPins, jwplcI2C_readReg8, jwplcI2C_readRegs, jwplcI2C_setClock, jwplcI2C_writeReg8, jwplcI2C_writeRegs
[FAIL] JWPLC_Display/libJWPLC_Display.a: jwplcSPI_acquire, jwplcSPI_begin, jwplcSPI_deselectAll, jwplcSPI_prepareForTFT, jwplcSPI_release, jwplcSystemForceDisplayRefresh, jwplcSystemMarkDisplayDirty
[BRIDGE] JWPLC_Ethernet_W5x00_Backend/libJWPLC_Ethernet_W5x00_Backend.a: jwplc_digitalWrite, jwplc_pinMode
[PASS] JWPLC_ModbusRTU/libJWPLC_ModbusRTU.a
[BRIDGE] SD/libSD.a: jwplc_digitalWrite, jwplc_pinMode
[PASS] SPI/libSPI.a
[PASS] Wire/libWire.a
EXIT CODE: 2
```

## Interpretación

### JWPLC_Display

El archive histórico no depende únicamente del bridge GPIO. También conserva acoplamiento al arbitraje SPI JWPLC y al runtime de refresco del display:

```txt
jwplcSPI_acquire
jwplcSPI_begin
jwplcSPI_deselectAll
jwplcSPI_prepareForTFT
jwplcSPI_release
jwplcSystemForceDisplayRefresh
jwplcSystemMarkDisplayDirty
```

Estos símbolos no se consideran equivalentes a primitivas Arduino genéricas y no se amplía el bridge GPIO para cubrirlos en Alpha5.

Decisión: **piloto precompilado de JWPLC_Display rechazado; volver a compilación desde fuente**.

### JW_RTC

El archive histórico conserva dependencia directa del bridge I2C JWPLC:

```txt
jwplcI2C_begin
jwplcI2C_beginWithPins
jwplcI2C_readReg8
jwplcI2C_readRegs
jwplcI2C_setClock
jwplcI2C_writeReg8
jwplcI2C_writeRegs
```

La clasificación anterior de `JW_RTC` como neutral fue un falso negativo del regex antiguo.

Decisión: **retirar el archive compartido de JW_RTC y volver a compilación desde fuente**. No se amplía en Alpha5 el bridge genérico hacia la capa I2C JWPLC.

## Checkpoint posterior a la limpieza - PASS

Después de retirar los archives de `JWPLC_Display` y `JW_RTC` y mantener ambas librerías en compilación desde fuente, se repitió la auditoría completa con el regex corregido.

Log local:

```txt
tools/build-speed-benchmark/results/manual-logs/20260823_231541_16_post_full_symbol_cleanup_audit.log
```

Resultado:

```txt
Modo bridge GPIO generico: HABILITADO
Archives encontrados: 10
[BRIDGE] Adafruit_BusIO/libAdafruit_BusIO.a: jwplc_digitalWrite, jwplc_pinMode
[BRIDGE] Adafruit_GFX_Library/libAdafruit_GFX_Library.a: jwplc_digitalRead, jwplc_digitalWrite, jwplc_pinMode
[PASS] Adafruit_ST7735_and_ST7789_Library/libAdafruit_ST7735_and_ST7789_Library.a
[PASS] FS/libFS.a
[PASS] JW_FRAM/libJW_FRAM.a
[BRIDGE] JWPLC_Ethernet_W5x00_Backend/libJWPLC_Ethernet_W5x00_Backend.a: jwplc_digitalWrite, jwplc_pinMode
[PASS] JWPLC_ModbusRTU/libJWPLC_ModbusRTU.a
[BRIDGE] SD/libSD.a: jwplc_digitalWrite, jwplc_pinMode
[PASS] SPI/libSPI.a
[PASS] Wire/libWire.a
EXIT CODE: 0
```

Conclusión del checkpoint: **PASS BRIDGE-COMPATIBLE**. No quedan dependencias externas con prefijo `jwplc` fuera de los tres símbolos GPIO permitidos entre los archives compartidos activos.

## Impacto sobre pilotos ya adoptados

No cambia la validez de:

- `JWPLC_Ethernet_W5x00_Backend`;
- `Adafruit_GFX_Library`;
- `Adafruit_BusIO`;
- `SD` nativa.

Sus dependencias externas siguen limitadas al bridge GPIO permitido o son neutrales.

## Recuento estructural corregido

El fallback conservador original tenía 23 translation units desde fuente. Al detectar que `JW_RTC` tampoco es un archive compartido seguro, el conjunto seguro real pasa a 24 translation units fuente antes de las recuperaciones.

Recuperados y adoptados hasta este punto:

```txt
Ethernet backend        8
Adafruit GFX            4
Adafruit BusIO          4
SD nativa               3
--------------------------
Total recuperado       19
```

Pendientes desde fuente:

```txt
JWPLC_Display           2
JW_RTC                  1
JW_MatrixButtons        1
JW_SD                   1
--------------------------
Total pendiente         5
```

La meta estructural de 5 translation units totales de Alpha4 P8 deja de ser un objetivo obligatorio para Alpha5. Con la política de compatibilidad corregida, cualquier comparación final debe reportar explícitamente este cambio de criterio y priorizar estabilidad sobre igualdad artificial de conteo.

## Siguiente paso

1. continuar con el piloto de `JW_SD`;
2. repetir el probe físico de arranque/coexistencia SPI solicitado antes del benchmark final;
3. cerrar `JW_MatrixButtons` si mantiene compatibilidad;
4. ejecutar benchmark final y documentar la comparación con Alpha4.
