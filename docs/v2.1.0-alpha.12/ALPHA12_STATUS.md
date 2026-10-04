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


## Blocker TFT resuelto estáticamente

Se detectó y corrigió una incompatibilidad interna posterior a la migración
`JWPLC_Display -> JWPLC_TFT`:

```text
JWPLC_LogicRuntime_UI consumers Adafruit_ST7789 -> JWPLC_TFTClass
Display examples ST77XX_* -> JWPLC_TFT_*
```

Auditoría actual:

```text
JWPLC_DISPLAY_SRC_ADAFRUIT_ST7789_REFS=0
JWPLC_DISPLAY_SRC_ST77XX_REFS=0
JWPLC_LOGICRUNTIME_UI_CURRENT_CONSUMER_ADAFRUIT_REFS=0
JWPLC_LOGICRUNTIME_UI_CURRENT_CONSUMER_ST77XX_REFS=0
DISPLAY_CURRENT_EXAMPLES_ADAFRUIT_REFS=0
DISPLAY_CURRENT_EXAMPLES_ST77XX_REFS=0
STATIC_TFT_BACKEND_MIGRATION=PASS
```

Prevención:

```text
F081=BACKEND_MIGRATION_LEFT_DISTRIBUTED_CONSUMERS_STALE
CI_LOGICRUNTIME_UI_COVERAGE=ADDED
```

Gate preparado:

```text
tools/alpha12/gates/alpha12_tft_backend_compile.ps1
```

Estado:

```text
TFT_SOURCE_FIRST_COMPILE=PENDING
PRECOMPILED_DISPLAY_TFT_REGEN=BLOCKED_UNTIL_GATE_PASS
PHYSICAL_UPLOAD_REQUIRED_FOR_THIS_GATE=NO
```

Documento:

```text
docs/v2.1.0-alpha.12/ALPHA12_TFT_BACKEND_MIGRATION_20261003.md
```


## Gate TFT source-first cerrado y refresh precompilado abierto

Resultado validado desde ZIP:

```text
ALPHA12_TFT_BACKEND_COMPILE=PASS
PASS=4
FAIL=0
SOURCE_FIRST=PASS
COMPILER_WARNINGS=0
COMPILER_ERRORS=0
```

Documento:

```text
ALPHA12_TFT_BACKEND_SOURCE_FIRST_RESULT_20261003.md
```

Matriz de precompilados:

```text
STALE_REQUIRED_REBUILD:
- core.a
- libJWPLC_Display.a
- libJWPLC_ModbusRTU.a
- libJW_SD.a
- libSPI.a

REQUALIFY_NO_FUNCTIONAL_REBUILD:
- libJWPLC_TFT.a

RETAIN_AND_AUDIT:
- libJW_FRAM.a
- libJW_MatrixButtons.a
- libSD.a
- libFS.a
- libWire.a
- Adafruit base archives
```

Siguiente único gate:

```text
tools/alpha12/gates/alpha12_core_precompiled_refresh.ps1
```

Contrato del gate:

- genera `core.a` desde `cores/jwcontrol` actual;
- verifica Basic normal con stub + nuevo archive;
- verifica Basic Core como control source;
- rollback automático si falla;
- al PASS deja sólo `precompiled/core/JWPLCBASIC/core.a` como tracked dirty;
- no hace upload físico.


## P1 precompilado — core.a cerrado

Commit de adopción:

```text
bedc551a00e784c2561d23df58c14c2390267384
```

Artifact:

```text
JWPLC/2.1.0/precompiled/core/JWPLCBASIC/core.a
BYTES=3042444
SHA256=78d0c0ab14f156b96116529e88872340d51877af40d24ba3559f6081e0bf34fb
```

Gate:

```text
CORE_PRECOMPILED_BUILD=PASS
CORE_PRECOMPILED_VERIFY_BASIC=PASS
CORE_PRECOMPILED_VERIFY_CORE=PASS
FINAL_TRACKED_DIRTY_SCOPE=core.a_ONLY
```

El archive anterior quedó invalidado por F050 y ya no es el artifact de cierre.

Siguiente artifact:

```text
P2=JWPLC_ModbusRTU/src/esp32/libJWPLC_ModbusRTU.a
```


## P2 precompilado — Modbus RTU preparado

Gate versionado:

```text
tools/alpha12/gates/alpha12_modbus_rtu_precompiled_refresh.ps1
```

Contrato:

1. exige branch Alpha12 y tracked/index clean;
2. verifica que las APIs RTU actuales existan en header y source;
3. compila un Slave RTU desde una librería temporal source-only;
4. exige exactamente un `JWPLC_ModbusRTU.cpp.o`;
5. crea `libJWPLC_ModbusRTU.a` con un único miembro;
6. extrae el miembro y exige SHA-256 idéntico al objeto fuente;
7. compila Slave y Master contra una librería temporal `precompiled=full`;
8. exige marker precompiled y cero objetos RTU compilados desde source;
9. copia el mismo candidato al worktree oficial;
10. al PASS deja únicamente `libJWPLC_ModbusRTU.a` como tracked dirty;
11. rollback automático al archive previo si cualquier paso falla;
12. no realiza upload físico.

Sketches de link:

```text
01.ModbusRTU_Slave_Holding
02.ModbusRTU_Master_Read
```

Estado:

```text
P1_CORE=PASS_ADOPTED
P2_MODBUS_RTU=READY_TO_RUN
PHYSICAL_GATE_REQUIRED_FOR_P2=NO
```


## P2 precompilado — Modbus RTU cerrado

Commit de adopción:

```text
b1cd40901de77e00bb486fc19288ebb6ad35f7ff
```

Artifact:

```text
JWPLC/2.1.0/libraries/JWPLC_ModbusRTU/src/esp32/libJWPLC_ModbusRTU.a
BYTES=292390
SHA256=424ed3f462bb57cce0019d690486e612d5f243c40ae62d44d3ad973b0a521085
```

Gate previo:

```text
SOURCE_OBJECT_COUNT=1
SOURCE_PRECOMPILED_MARKER=NO
ARCHIVE_MEMBER_COUNT=1
ARCHIVE_MEMBER=JWPLC_ModbusRTU.cpp.o
ARCHIVE_MEMBER_BYTE_PARITY=PASS
CANDIDATE_SLAVE_LINK=PASS
CANDIDATE_MASTER_LINK=PASS
FINAL_TRACKED_DIRTY_SCOPE=libJWPLC_ModbusRTU.a_ONLY
```

P2 queda adoptado en remoto/local.

## P3 precompilado — SPI preparado

Gate:

```text
tools/alpha12/gates/alpha12_spi_precompiled_refresh.ps1
```

Contrato:

- exige branch Alpha12 y tracked/index clean;
- confirma `JWPLC_SPI_FIFO_REUSE_DLEN_CACHE=1`;
- confirma `JWPLC_SPI_FIFO_REUSE_COPY_OUT_64=1`;
- compila SPI source-only mediante sketch temporal aislado;
- exige exactamente un `SPI.cpp.o`;
- crea `libSPI.a` de un único miembro;
- extrae el miembro y exige SHA-256 byte-parity con el objeto fuente;
- compila el mismo sketch contra candidato `precompiled=full`;
- exige cero `SPI.cpp.o` recompilados desde source en modo candidato;
- sólo al PASS copia el candidate al worktree oficial;
- rollback automático ante cualquier fallo;
- no realiza upload físico.

Estado:

```text
P1_CORE=PASS_ADOPTED
P2_MODBUS_RTU=PASS_ADOPTED
P3_SPI=READY_TO_RUN
P4_JW_SD=PENDING
P5_DISPLAY=PENDING
P6_TFT_REQUALIFICATION=PENDING
```
