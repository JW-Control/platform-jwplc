# v2.1.0-alpha.13 — Robustez Ethernet W5500, SPI compartido y sockets Modbus TCP

**Estado del documento:** notas finales de candidata; **NO publicada todavía**. Se actualizarán exclusivamente los metadatos reales de la PreRelease al ejecutar y validar el flujo de publicación. Código de referencia integrado en la base Alpha13 por [PR #124](https://github.com/JW-Control/platform-jwplc/pull/124), merge `3467a375ccaf7c7ee27220ed0bc348056e5e8219`.

## Alcance

Alpha13 consolida mantenimiento, robustez y corrección de fallos del package Arduino JWPLC Basic v2.0.0, respetando las APIs existentes y el autoload de Display, Ethernet W5500, SD, FRAM, RTC, botones, RS-485, Modbus RTU, Modbus TCP y TCA/I/O.

- **F131/F1, chip-select W5500:** se adopta `JWPLC_ETH_CS=5` como fuente del SS de Ethernet en el código `utility/w5100.cpp` y guarda de compilación contra GPIO6–11 (flash del ESP32). Se corrige el riesgo latente de que el fallback a GPIO10 provoque watchdog en enlace temprano.
- **SPI y ownership de sockets:** dos fuentes nuevas `JWPLC_W5500_SafeAccess.h/.cpp`, mutex SPI no recursivo, propietario y generación de socket, retorno explícito de bus ocupado o token obsoleto, y cierre diferido cuando el bus no está disponible.
- **Ethernet:** mejoras internas en `EthernetClient`, `EthernetServer`, `EthernetUDP`, `JWPLC_Ethernet`, `JWPLC_W5x00_Ethernet` y `socket.cpp` para TCP/UDP cooperativo, tratamiento seguro de `BUS_BUSY`, lifecycle y preservación de sesiones.
- **Modbus TCP:** mejoras en cliente/servidor y cabeceras correspondientes para cierre, lectura de estado, reconexión y coexistencia con RTU. Se mantienen las APIs y funciones Modbus ya soportadas en Alpha12.

**Alcance productivo exacto G10:** 10 archivos modificados y 2 añadidos en `JWPLC/2.1.0/libraries/JWPLC_Ethernet` y `JWPLC/2.1.0/libraries/JWPLC_ModbusTCP`; los cambios del overlay de laboratorio no se promueven. `Dns.cpp` y `Dhcp.cpp` no fueron modificados directamente durante la promoción G10.

## Validación de ingeniería

- N0: contrato exacto y preservación F1/archives; CI Windows y Ubuntu.
- N1: contratos nativos C++ y paridad SHA 14/14 con el compositor funcional; CI Windows/Ubuntu.
- N2: dos perfiles FQBN JWPLC Basic y Basic Core, compilaciones sobre fuentes del package real, dependencia `.d`, objetos y ELF verificados.
- N3: 11 sketches de integración con Display, Ethernet, I/O, RS-485, Modbus RTU/TCP, FRAM, RTC y SD. Autoload normal intacto.
- PH0: upload y arranque reales COM4 Master/COM14 Slave, dos IP DHCP de router aislado, FC03 Modbus TCP en ambos y comunicación RS-485 RTU.
- R1: **605 s** con 589 lecturas FC03 correctas por cada servidor, 589 ecos UDP y contadores UART posteriores coherentes (RTU, SD, FRAM, RTC, TFT, I/O).
- R2: **3/3 ciclos de red** con recuperación TCP+UDP en 1,69 / 10,05 / 6,03 s; **2 transiciones PHY reales** adicionales en UART con mismo BOOT y progreso RTU/periféricos.

Fuentes y criterios de no repetición: `A13_G10_PROMOTED_N0_N3_20261019.md`, `A13_G10_H_DICTAMEN_CIERRE_ACOTADO_20261020.md` y `ALPHA13_STATUS.md`.

## Limitaciones reconocidas

- R3 externo: cierre y reapertura **secuencial** de sesiones TCP queda observado en las 589 lecturas R1 por PLC, pero **no** se completó fault injection física interna para `BUS_BUSY`, leases `Stale`, close diferido concurrente ni cancelación async DNS/DHCP/TCP.
- R4 canónico: duración de 1805 s **NO ejecutada** por decisión del operador de abreviar. Los 605 s de R1 no se presentan como equivalentes a endurance/ausencia absoluta de starvation SPI.
- No se probó resolución DNS ni renovación/rebind de DHCP interna; `PHY=1` y DHCP READY no acreditan por sí solos estos protocolos.
- E1 `PENDING_GDB_UNAVAILABLE`; E2 `PENDING_SDKCONFIG_NOT_LOCATED`. La prueba H0 extendida 20/20 estricta conserva brechas de captura aceptadas con observación, no se transforma en PASS perfecto.
- No existen afirmaciones de determinismo duro de RTU, OTA definida, OpenPLC integrado, FlashFreq universal definitivo o `bootloader.bin` publicable.
- **CLI/IDE instalando la PreRelease desde el índice publicado, subida desde package distribuido y postpublicación quedan pendientes hasta que exista el asset ZIP real.**

## Seguridad, compatibilidad y distribución

Se mantiene el perfil de hardware y periféricos de JWPLC Basic sin configuración de pines por el sketch. No tocar `boards.txt`, `platform.txt`, `GPIO4` ni los archives precompilados congelados solo por velocidad. No cambiar el canal estable: Alpha13 es **PreRelease de desarrollo**, dev index únicamente.

**Publicación planificada, no ejecutada:**

```text
TAG_PLAN=v2.1.0-alpha.13
PACKAGE_ROOT=2.1.0/
ZIP_PLAN=jwplc-esp32-2.1.0-alpha.13.zip
ZIP_SIZE=PENDING
ZIP_SHA256=PENDING
GITHUB_PRERELEASE=NOT_RUN
INDEX_DEV_PR=NOT_CREATED
PUBLISHED_INSTALL_COMPILE_UPLOAD=NOT_RUN
ALPHA13_STATUS=OPEN_RELEASE_PENDING
```

Tras publicar: incluir el SHA real del source/tag, tamaño y checksum del ZIP, URL de PreRelease, número y estado del PR de index hacia `main`, instalación/compilación/upload físico desde el package publicado, y **solo entonces** documentar `CLOSED_PUBLISHED`.
