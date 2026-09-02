# v2.1.0-alpha.7 — PreRelease

Alpha7 consolida la robustez de comunicaciones y Remote I/O del JWPLC Basic, cierra correcciones detectadas durante los gates distribuidos y deja preparado el package para publicación sin retirar periféricos del autoload normal.

## Cambios principales

### Modbus RTU / Remote I/O

- parser multidrop corregido para separar ADU por estructura Modbus antes de validar CRC;
- Master cooperativo/no bloqueante como API recomendada;
- variantes Sync preservadas para commissioning y compatibilidad;
- validación física FC01, FC02, FC05 y FC15;
- fail-safe de salidas ante pérdida de bus;
- recuperación tras reconexión o reset del Slave sin reiniciar el Master;
- `libJWPLC_ModbusRTU.a` regenerado y restaurado a `precompiled=full`.

Gate principal:

```text
ALPHA7_A7_1_ARDUINO_ARDUINO=PASS
MASTER_COOPERATIVE=PASS
BUS_DISCONNECT_DETECTED=PASS
FAILSAFE_ON_BUS_LOSS=PASS
MASTER_TIMEOUT_NO_FREEZE=PASS
RECONNECT_WITHOUT_MASTER_RESET=PASS
RECOVERY_WITHOUT_MASTER_RESET=PASS
```

### Ethernet / W5500

Alpha7 corrige un falso `LINK_OFF` que podía producirse cuando `JWPLC_Ethernet.service()` no conseguía adquirir temporalmente el mutex SPI.

Ahora:

```text
SPI ocupado temporalmente -> READY se conserva
Cable desconectado        -> LINK_OFF real
Cable reconectado         -> READY recuperado sin RESET
```

Gate dirigido:

```text
FORCED_BUS_LOCK_TIMEOUTS=12
FALSE_LINK_OFFS=0
READY_DROPS=0
WRONG_STATE_ON_TIMEOUT=0
FINAL_READY=YES
FINAL_LINK=ON
ALPHA7_ETH_SPI_CONTENTION=PASS
```

También se repitió el recovery Router -> laptop sin DHCP -> Router sin reset del JWPLC:

```text
ETH_ROUTER_LAPTOP_ROUTER_RECOVERY=PASS
ETH_RECOVERY_NO_RESET=PASS
```

### TCA / I2C

- corregido el tratamiento de lectura `0xFF` del TCA para no confundir un dato válido con un error;
- core precompilado JWPLC Basic actualizado;
- diagnósticos I2C utilizados en los soak tests sin convertirlos en políticas permanentes del runtime.

### OpenPLC / VPP

Dentro del trabajo externo OpenPLC de Alpha7:

- VPP Alpha18 preservado con payload firmado reproducible 9/9;
- política EOL histórica preservada mediante `.gitattributes`;
- feedback FC01 integrado en la HAL/VPP;
- se mantiene la separación entre el package Arduino y la integración OpenPLC.

```text
ALPHA18_VPP_STATUS=CLOSED
SIGNED_PAYLOAD_PHYSICAL=9/9
FRESH_CHECKOUT_SIGNED_PAYLOAD=9/9
ALPHA18_CHECKOUT_REPRODUCIBILITY=PASS
```

## Precompilación final Modbus RTU

```text
Archivo : JWPLC/2.1.0/libraries/JWPLC_ModbusRTU/src/esp32/libJWPLC_ModbusRTU.a
Bytes   : 231062
SHA256  : 444BE3A04079A579252B2737FE6070E00ADCA949FD176880588FE69561B2A79F
```

Verificación:

```text
ALPHA7_MODBUS_PRECOMPILED=PASS
ALPHA7_MODBUS_BASICCORE_PRECOMPILED=PASS
ALPHA7_MODBUS_PRECOMPILED_FREEZE=PASS
```

## Compatibilidad y autoload

Alpha7 mantiene integrados en el flujo normal:

- Display;
- Ethernet / W5500;
- microSD;
- FRAM;
- RTC;
- botonera;
- RS-485;
- Modbus RTU;
- TCA / I/O;
- mutex SPI compartido.

No se retira ningún periférico por velocidad de compilación.

Ethernet continúa compilándose desde source; Modbus RTU vuelve al archive precompilado final después de cerrar el desarrollo funcional.

## Límites de esta PreRelease

Alpha7 no:

- convierte OpenPLC en runtime obligatorio del package Arduino;
- define OTA;
- fija una FlashFreq/configuración universal definitiva;
- publica un `bootloader.bin` definitivo;
- migra el producto a ESP32-S3;
- redefine el proceso canónico futuro de firma VPP.

App-only continúa siendo una herramienta auxiliar de desarrollo, no la ruta de upload por defecto.

## Documentación

- `docs/v2.1.0-alpha.7/MODBUS_RTU_MASTER_COOPERATIVO.md`
- `docs/v2.1.0-alpha.7/REMOTE_IO_A7_1_VALIDATION.md`
- `docs/v2.1.0-alpha.7/OPENPLC_BACKPLANE_ALPHA18_VPP.md`
- `docs/v2.1.0-alpha.7/ALPHA7_CLOSURE_CHECKLIST.md`
- `docs/v2.1.0-alpha.7/PULL_REQUEST.md`

## Estado técnico

```text
ALPHA7_TECHNICAL_CLOSURE=PASS
ALPHA7_PUBLICATION=PENDING_PR_CI_RELEASE
```
