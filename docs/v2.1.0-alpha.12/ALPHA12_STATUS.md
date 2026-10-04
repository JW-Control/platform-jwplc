# v2.1.0-alpha.12 — Estado de cierre

Actualizado: 2026-10-03

## Identidad

```text
RELEASE_VERSION=v2.1.0-alpha.12
BRANCH=v2.1.0-alpha.12/feature/modbus-tcp
HISTORICAL_DEV_BRANCH=v2.1.0-alpha.14/feature/modbus-tcp
ALPHA12_STATUS=PACKAGE_CLOSURE_IN_PROGRESS
ALPHA11_STATUS=CLOSED_PUBLISHED
```

La evidencia técnica histórica permanece bajo
`docs/v2.1.0-alpha.14/`. Ver:

- `ALPHA12_RENUMBERING_MAP_20261003.md`.

## Alcance consolidado de Alpha12

Alpha12 reúne el trabajo de comunicaciones/runtime desarrollado después de
Alpha11:

- nueva librería `JWPLC_ModbusTCP`;
- Server y Client cooperativos;
- FC01/02/03/04/05/06/15/16;
- lifecycle TCP cooperativo;
- TX TCP asíncrono interno;
- hardening Ethernet/W5500;
- mejoras UDP internas/aditivas;
- W5500 a 26 MHz validado en el perfil actual;
- hardening y optimización Modbus RTU;
- selector de motor RTU ASYNC/SYNC;
- TX queued sobre AutoDirection;
- timing RTU en microsegundos;
- full-runtime Display/SD/FRAM/RTC/I/O/buttons;
- validación Arduino IDE;
- ceilings RAW;
- coexistencia TCP + RTU + UDP.

No se retiran periféricos del autoload normal.

## Perfil final de coexistencia confirmado

Confirmación física 600 s:

```text
TCP Modbus = 250.001 req/s
RTU        = 796.953 req/s
RTU scan   = 99.619 scans/s
RTU ops    = 8 por scan
UDP FAST   = 0.998 Mbps
UDP delivery = 100.000 %
FULL_RUNTIME_CLEAN=YES
```

Integridad:

```text
TCP_ERRORS=0
RTU_FAILED=0
RTU_REJECTED=0
RTU_VERIFY_FAILS=0
RTU_CRC_ERRORS=0
UDP_RANGE_MISSING=0
UDP_DUPLICATES=0
UDP_REORDERS=0
UDP_TRANSPORT_ERRORS=0
PERIPHERAL_FAILURE_COUNT=0
SD_DATALOG_FAILED_COMMITS=0
SPI_PROBE_FAILS=0
```

Contrato:

```text
RTU100HZ_OPERATIONAL=PASS
RTU100HZ_ZERO_SKIP_DETERMINISTIC=NO
RTU_PERIODS_SKIPPED_600S=229
```

No publicar como hard real-time/cero-jitter.

## Ceilings/characterization cerrados

RAW final de referencia:

```text
TCP RX RAW        ~= 14.11 Mbps
TCP TX RAW        ~= 4.65 Mbps
UDP RX legacy RAW ~= 11.60 Mbps
UDP TX RAW        ~= 5.18 Mbps
UDP RX FAST RAW   ~= 13.18 Mbps
```

Coexistencia:

```text
UDP1M_CONFIRM_600S=PASS
UDP2M_300S=PASS
UDP2M_CONFIRM_600S=NOT_OPERATIONAL_RTU_THRESHOLD
```

## Decisiones de producto ya cerradas

```text
TCP_RX_POLICY=POLLING_C0
TCP_INT_DEFAULT=OFF
D2_DEFAULT=OFF
D3_DEFAULT=OFF
E1_DEFAULT=OFF

W5500_SPI_HZ=26000000
FIFO_REUSE_DEFAULT=ON
DLEN_REUSE_DEFAULT=ON
COPY_OUT_64_DEFAULT=ON

LEGACY_UDP_API_BREAK=NO
UDP_FAST_PATH=ADDITIVE_INTERNAL_API
UDP_SINGLE_CS_PRODUCTIZE=NO

AUTOLOAD_PERIPHERALS_REMOVED=NO
OPENPLC_RUNTIME_AUTOLOAD=NO
OTA=NOT_DEFINED
FINAL_UNIVERSAL_FLASH_CONFIGURATION=PENDING
BOOTLOADER_BIN_FINAL=NO
```

## Estado del cierre

Técnicamente ya están cerrados:

- Modbus TCP Server;
- Modbus TCP Client;
- matriz FC;
- reconnect/timeout;
- Ethernet hardening;
- RTU hardening;
- ceilings RAW;
- coexistencia LR600;
- normal autoload físico;
- Arduino IDE físico;
- runtime integrado.

Pendiente antes de publicación Alpha12:

1. auditoría final de cambios productivos vs benchmark-only;
2. actualizar README y `library.properties`;
3. actualizar ejemplos/documentación de APIs;
4. depurar textos históricos desactualizados;
5. congelar cambios funcionales;
6. regenerar archives precompilados afectados;
7. registrar SHA-256/tamaños;
8. validar que los builds usan los archives finales;
9. CLI/IDE/upload físico final;
10. crear PRE_RELEASE Alpha12 en español;
11. actualizar README raíz/marker de release;
12. PR Alpha12 hacia `release/v2.1.x`;
13. CI final;
14. publicación + índice dev;
15. isolated install/compile/upload desde package publicado;
16. cierre documental/post-publication.

## Próximos releases

```text
ALPHA13=TFT_DISPLAY_UPDATE
ALPHA14=OPENPLC_PLUS_OPTIMIZED_TCP_RTU_INTEGRATION
```


## Avance de consolidación documental

Completado en la rama canónica Alpha12:

```text
RENUMBERING_MAP=PASS
PACKAGE_INVENTORY=PASS
ROADMAP_RENUMBERED=PASS
ROOT_README_ALPHA12_IN_CLOSURE=PASS

README_MODBUS_TCP=UPDATED
README_MODBUS_RTU=UPDATED
README_RS485=UPDATED
README_ETHERNET=UPDATED
README_JW_SD=UPDATED
README_DISPLAY=UPDATED
README_TFT=UPDATED

LIBRARY_PROPERTIES_MODBUS_TCP=UPDATED
LIBRARY_PROPERTIES_RS485=UPDATED
LIBRARY_PROPERTIES_JW_SD=UPDATED
```

Hallazgo de compatibilidad todavía abierto:

```text
DISPLAY_RAW_RETURN_TYPE_ALPHA11=Adafruit_ST7789&
DISPLAY_RAW_RETURN_TYPE_ALPHA12=JWPLC_TFTClass&
RECOMMENDED_AUTO_REFERENCE_PATTERN=COMPATIBLE_CANDIDATE
EXPLICIT_ADAFRUIT_REFERENCE=BREAKS
DECISION=PENDING_BEFORE_FREEZE
```

El marker automático del README raíz permanece deliberadamente en Alpha11
mientras Alpha12 no esté listo para disparar release.

Siguiente bloque de cierre:

1. auditar ejemplos contra firmas reales;
2. decidir compatibilidad raw TFT;
3. freeze funcional;
4. regenerar/recalificar precompilados;
5. ejecutar gates finales CLI/IDE/hardware.
