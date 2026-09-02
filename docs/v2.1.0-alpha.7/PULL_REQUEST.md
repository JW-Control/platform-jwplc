# v2.1.0-alpha.7 — Pull Request

## Objetivo

Cerrar Alpha7 consolidando la robustez de Modbus RTU / Remote I/O, los fixes de runtime detectados durante los gates distribuidos y la integración externa OpenPLC/VPP trabajada en este ciclo, preservando el autoload completo del JWPLC Basic y las decisiones de compatibilidad tomadas en Alpha6.

## Resumen

Alpha7 introduce cuatro grupos principales de cambios:

1. **Modbus RTU / Remote I/O**
   - parser multidrop corregido por estructura de ADU;
   - Master cooperativo/no bloqueante como API recomendada;
   - API Sync explícita preservada;
   - validación física FC01/FC02/FC05/FC15;
   - fail-safe, pérdida de bus, reset de Slave y recuperación sin resetear el Master;
   - `libJWPLC_ModbusRTU.a` regenerado y restaurado a `precompiled=full`.

2. **TCA / I2C**
   - corrección del tratamiento de lectura `0xFF`;
   - actualización del core precompilado JWPLC Basic;
   - diagnósticos I2C seguros utilizados durante los soak tests.

3. **Ethernet / W5500**
   - corrección de falso `LINK_OFF` causado por contención temporal del mutex SPI;
   - diferenciación entre `BUS_LOCK_TIMEOUT`, `LinkOFF` físico y estado de link desconocido;
   - snapshot único de IP + LINK durante cierre de configuración;
   - recuperación Router -> laptop sin DHCP -> Router validada sin RESET;
   - 12 contenciones SPI forzadas sin pérdida de `READY` ni falsos `LINK_OFF`.

4. **OpenPLC / VPP**
   - preservación reproducible del VPP Alpha18;
   - payload firmado declarado reproducible 9/9 desde checkout fresco;
   - política EOL histórica preservada mediante `.gitattributes`;
   - feedback FC01 integrado en la HAL/VPP;
   - OpenPLC permanece fuera del autoload obligatorio del package Arduino.

## Modbus RTU multidrop

La causa raíz de los falsos CRC en topología M2 + S1 + S2 fue la concatenación de request y response válidos de otro nodo dentro del mismo buffer UART.

El parser ahora obtiene la longitud esperada desde la estructura Modbus y usa CRC para validar esa ADU concreta.

Resultado físico:

```text
S1 CRC=0
S1 EXCEPTIONS=0
S2 CRC=0
S2 EXCEPTIONS=0
M2 S1 W/R/V=0/0/0
M2 S2 W/R/V=0/0/0
S1 ONLINE=YES
S2 ONLINE=YES
```

## Master cooperativo y Remote I/O

La ruta recomendada para código nuevo es:

```cpp
JWPLC_ModbusRTU.requestReadHoldingRegisters(...);
JWPLC_ModbusRTU.requestWriteSingleRegister(...);
JWPLC_ModbusRTU.task();
```

Las variantes Sync permanecen disponibles de forma explícita para commissioning o sketches donde bloquear sea aceptable.

Gate A7.1:

```text
ALPHA7_A7_1_ARDUINO_ARDUINO=PASS
FC01_READ_COILS=PASS
FC02_READ_DISCRETE_INPUTS=PASS
FC05_WRITE_SINGLE_COIL=PASS
FC15_WRITE_MULTIPLE_COILS=PASS
MASTER_COOPERATIVE=PASS
DO_REFRESH_40MS=PASS
BUS_DISCONNECT_DETECTED=PASS
FAILSAFE_ON_BUS_LOSS=PASS
MASTER_TIMEOUT_NO_FREEZE=PASS
RECONNECT_WITHOUT_MASTER_RESET=PASS
SLAVE_RESET_DETECTED=PASS
MASTER_SURVIVES_SLAVE_RESET=PASS
RECOVERY_WITHOUT_MASTER_RESET=PASS
```

## Ethernet: falso LINK_OFF por contención SPI

El comportamiento anterior podía transformar un timeout al adquirir el mutex SPI en `LINK_OFF`, invalidando el estado `READY` aunque el cable siguiera conectado.

Alpha7 corrige esa clasificación:

```text
SPI ocupado temporalmente -> BUS_LOCK_TIMEOUT / READY se conserva
Cable desconectado        -> LINK_OFF real
Cable reconectado         -> READY recuperado
```

Gate dirigido:

```text
FORCED_BUS_LOCK_TIMEOUTS=12
FALSE_LINK_OFFS=0
READY_DROPS=0
WRONG_STATE_ON_TIMEOUT=0
FINAL_READY=YES
FINAL_STATE=5
FINAL_ERROR=0
FINAL_LINK=ON
ALPHA7_ETH_SPI_CONTENTION=PASS
```

Recovery real:

```text
ETH_RECOVERY_ROUTER_INITIAL=PASS
ETH_RECOVERY_ROUTER_TO_OFF=PASS
ETH_RECOVERY_LAPTOP_NO_DHCP=PASS
ETH_RECOVERY_LAPTOP_TO_OFF=PASS
ETH_RECOVERY_ROUTER_FINAL=PASS
ETH_RECOVERY_NO_RESET=PASS
ETH_ROUTER_LAPTOP_ROUTER_RECOVERY=PASS
```

## Precompilación final

`JWPLC_ModbusRTU` vuelve a `precompiled=full` sólo después de congelar y validar el source Alpha7.

Archive final:

```text
JWPLC/2.1.0/libraries/JWPLC_ModbusRTU/src/esp32/libJWPLC_ModbusRTU.a
Bytes  : 231062
SHA256 : 444BE3A04079A579252B2737FE6070E00ADCA949FD176880588FE69561B2A79F
```

Validación:

```text
ALPHA7_MODBUS_PRECOMPILED=PASS
MODBUS_SOURCE_COMPILES=0
MODBUS_ARCHIVE_MENTIONS=1
ALPHA7_MODBUS_BASICCORE_PRECOMPILED=PASS
ALPHA7_MODBUS_PRECOMPILED_FREEZE=PASS
```

El core JWPLC Basic precompilado también fue actualizado durante Alpha7 después de la corrección TCA.

## OpenPLC / VPP

La preservación Alpha18 cerró con:

```text
ALPHA18_VPP_STATUS=CLOSED
SIGNED_PAYLOAD_PHYSICAL=9/9
FRESH_CHECKOUT_SIGNED_PAYLOAD=9/9
ALPHA18_SIGNED_BYTES_PRESERVED=PASS
ALPHA18_CHECKOUT_REPRODUCIBILITY=PASS
```

Posteriormente se integró feedback FC01 en `hal/jwplcbasic.cpp` para comparar el snapshot enviado mediante FC15 con coils reales leídas desde el Slave.

Este PR no convierte OpenPLC en una dependencia obligatoria del package Arduino.

## Autoload preservado

Se mantienen en el flujo normal:

- Display;
- Ethernet / W5500;
- microSD;
- FRAM;
- RTC;
- botonera;
- RS-485;
- Modbus RTU;
- TCA / I/O;
- mutex SPI global.

No se retiró ningún periférico para ganar velocidad de compilación.

## Build

- Arduino CLI 1.5.1 utilizado en los gates finales.
- JWPLC Basic compila con `JWPLC_Ethernet.cpp` desde source.
- JWPLC Basic Core compila utilizando el archive Modbus RTU final.
- Ethernet continúa source-build, decisión ya aceptada desde Alpha6.
- Alpha7 no reclama una nueva mejora global de cold build sin un benchmark dedicado adicional.

## Decisiones que NO toma Alpha7

Este PR no:

- integra OpenPLC como runtime obligatorio del package;
- define OTA;
- fija FlashFreq/configuración universal definitiva;
- publica un `bootloader.bin` definitivo;
- migra el producto a ESP32-S3;
- elimina periféricos del autoload;
- redefine el proceso canónico futuro de firma VPP.

App-only continúa como herramienta de desarrollo y no como ruta de upload por defecto.

## Sincronización con release/v2.1.x

Antes del cierre se incorporó el fix de release `#75` que siembra los índices del Boards Manager desde `main`.

El cherry-pick genera ancestry distinta y `rev-list` conserva una diferencia histórica de un commit, pero el workflow en Alpha7 y `release/v2.1.x` tiene exactamente el mismo blob:

```text
898c5e4e2c08a7fb725a1c58d6c30b08f2dc3982
```

Por ello no se realizó un rebase tardío de toda la rama ni force-push.

## Documentación

- `docs/v2.1.0-alpha.7/MODBUS_RTU_MASTER_COOPERATIVO.md`
- `docs/v2.1.0-alpha.7/REMOTE_IO_A7_1_VALIDATION.md`
- `docs/v2.1.0-alpha.7/OPENPLC_BACKPLANE_ALPHA18_VPP.md`
- `docs/v2.1.0-alpha.7/ALPHA7_CLOSURE_CHECKLIST.md`
- `docs/v2.1.0-alpha.7/PRE_RELEASE.md`

## Checklist para merge

- [x] Modbus multidrop corregido y validado.
- [x] Master cooperativo validado físicamente.
- [x] Remote I/O A7.1 cerrado.
- [x] TCA/I2C corregido.
- [x] Core precompilado actualizado.
- [x] Ethernet falso LINK_OFF corregido y validado físicamente.
- [x] Modbus RTU precompilado final regenerado y auditado.
- [x] JWPLC Basic Core validado con el archive final.
- [x] VPP Alpha18 preservado/reproducible.
- [x] Autoload normal preservado.
- [x] README y documentación de cierre preparados.
- [ ] CI del PR aprobado.

## Publicación después del merge

El README contiene:

```text
<!-- JWPLC_RELEASE_VERSION: 2.1.0-alpha.7 -->
```

y existe:

```text
docs/v2.1.0-alpha.7/PRE_RELEASE.md
```

Tras el merge a `release/v2.1.x`, el flujo de release debe preparar el ZIP, crear la PreRelease y actualizar el índice dev mediante el proceso automatizado existente.

No avanzar a otro alpha hasta cerrar y verificar esa publicación.
