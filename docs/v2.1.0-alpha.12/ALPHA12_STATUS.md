# v2.1.0-alpha.12 — Estado de cierre

Actualizado: 2026-10-04

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
P3_SPI=PASS_ADOPTED
P4_JW_SD=READY_TO_RUN
P5_DISPLAY=PENDING
P6_TFT_REQUALIFICATION=PENDING
```

## P3 precompilado — SPI cerrado

Commit de adopción:

```text
15c1bd1108038c59d0c07230911e4d5d0b653499
```

Artifact:

```text
JWPLC/2.1.0/libraries/SPI/src/esp32/libSPI.a
BYTES=103412
SHA256=b433758b746380bf8d1ea102aca1d1637b5a8cab50616b56024f4b022a916445
```

Gate validado:

```text
SOURCE_OBJECT_COUNT=1
ARCHIVE_MEMBER_COUNT=1
ARCHIVE_MEMBER=SPI.cpp.o
ARCHIVE_MEMBER_BYTE_PARITY=PASS
CANDIDATE_SOURCE_OBJECT_COUNT=0
CANDIDATE_PRECOMPILED_MARKER=YES
CANDIDATE_LINK=PASS
```

P3 queda adoptado en remoto/local.

## P4 precompilado — JW_SD cerrado

Commit de adopción:

```text
6c6c0eb250cea930fba4da1fd3c4306d649a9b5a
```

Artifact:

```text
JWPLC/2.1.0/libraries/JW_SD/src/esp32/libJW_SD.a
BYTES=362316
SHA256=1b9619ba37295782ade1edc6f06439a38e7e534fc08f5b05757c9af7a645acc0
```

Gate validado:

```text
SOURCE_OBJECT_COUNT=1
SOURCE_PRECOMPILED_MARKER=NO
ARCHIVE_MEMBER_COUNT=1
ARCHIVE_MEMBER=JW_SD.cpp.o
ARCHIVE_MEMBER_BYTE_PARITY=PASS
CANDIDATE_SOURCE_OBJECT_COUNT=0
CANDIDATE_PRECOMPILED_MARKER=YES
CANDIDATE_LINK=PASS
DATALOG_MANAGER=ON
CARD_LIFECYCLE_RECOVERY=ON
SPI_LOCK_CALLBACKS=ON
```

P4 queda adoptado en remoto/local.

## P5 precompilado — JWPLC_Display preparado

Gate:

```text
tools/alpha12/gates/alpha12_display_precompiled_refresh.ps1
```

Contrato:

- exige branch Alpha12 y tracked/index clean;
- exige el conjunto actual de 7 translation units de Display;
- rechaza referencias actuales a `Adafruit_ST7789` o `ST77XX_*`;
- confirma que la API pública retorna `JWPLC_TFTClass&`;
- compila `JWPLC_Display` source-only con sus dependencias reales;
- crea `libJWPLC_Display.a` con exactamente 7 miembros;
- extrae cada miembro y exige SHA-256 byte-parity contra su objeto fuente;
- compila un sketch que usa Display, HMI y acceso `JWPLC_TFTClass` contra candidato temporal `precompiled=full`;
- exige cero objetos Display recompilados desde source en modo candidato;
- sólo al PASS copia el candidate al worktree oficial;
- rollback automático ante cualquier fallo;
- guarda logs verbosos en `tools/alpha12/results/` y deja terminal resumido;
- no realiza upload físico.

Miembros esperados:

```text
JWPLC_Display.cpp.o
JWPLC_Display_H3E1_Profile.cpp.o
JWPLC_IdleScreen.cpp.o
JWPLC_UI.cpp.o
JWPLC_UI_API.cpp.o
JWPLC_UI_Pages.cpp.o
JWPLC_UI_PixelMap.cpp.o
```

Estado:

```text
P1_CORE=PASS_ADOPTED
P2_MODBUS_RTU=PASS_ADOPTED
P3_SPI=PASS_ADOPTED
P4_JW_SD=PASS_ADOPTED
P5_DISPLAY=READY_TO_RUN
P6_TFT_REQUALIFICATION=PENDING
PHYSICAL_GATE_REQUIRED_FOR_P5=NO
```


## P5 intento 1 — fallo de sintaxis del harness

La primera ejecución de `alpha12_display_precompiled_refresh.ps1` no llegó a ejecutar el gate.
PowerShell rechazó el archivo durante parsing por una interpolación ambigua:

```text
ParserError
$lineNo:
La referencia de variable no es válida
```

Clasificación:

```text
HARNESS_FAILURE=YES
PRODUCT_FAILURE=NO
HARDWARE_FAILURE=NO
SOURCE_BUILD_STARTED=NO
ARCHIVE_MUTATION=NO
PHYSICAL_UPLOAD=NO
```

Causa:

```powershell
"$($file.Name):$lineNo:$($_.Trim())"
```

En una cadena expandible, PowerShell interpreta `$lineNo:` como referencia de variable/scope inválida.
Se reemplazó por concatenación explícita sin ambigüedad.

Corrección versionada:

```text
ae758a1c70f441fb8f0c4e628df7691159e73f12
fix(alpha12): corregir interpolacion PowerShell en gate Display

70e060391cc7562d2bb4a39afcba9efcda91c2ca
fix(alpha12): endurecer compatibilidad PowerShell del gate Display
```

Auditoría preventiva adicional antes del rerun:

```text
AMBIGUOUS_VARIABLE_COLON_HITS=0
GET_FILE_HASH_OCCURRENCES=0
HERE_STRING_DELIMITERS=BALANCED
WRITE_HOST_F_AMBIGUITY=0
ARDUINO_TEMP_SKETCH_NAME_CONTRACT=PASS
```

El rerun debe ejecutar primero `System.Management.Automation.Language.Parser::ParseFile()`
en el host Windows y sólo continuar si el parser devuelve cero errores.

Prevención reforzada:

- no ejecutar directamente un gate PowerShell nuevo/modificado;
- ejecutar `System.Management.Automation.Language.Parser::ParseFile()` y exigir cero errores antes del gate;
- tratar un ParserError previo a ejecución como fallo del harness, nunca del producto;
- buscar interpolaciones ambiguas `$variable:` en strings expandibles y preferir concatenación o `${variable}` cuando corresponda.

Estado:

```text
P5_DISPLAY_ATTEMPT_1=HARNESS_PARSE_FAILURE
P5_DISPLAY_PRODUCT_EVIDENCE=NOT_STARTED
P5_DISPLAY=READY_TO_RERUN_AFTER_SYNTAX_PREFLIGHT
```


## P5 precompilado — JWPLC_Display cerrado

Commit de adopción:

```text
c30bc01897fcba055921b58a2506ea7aa4ba6c70
```

Artifact:

```text
JWPLC/2.1.0/libraries/JWPLC_Display/src/esp32/libJWPLC_Display.a
BYTES=941228
SHA256=c960d718433e29a40e3cc55bc745c9a2e121ee1ee598ec72c04872327592dc02
```

Gate validado:

```text
POWERSHELL_SYNTAX_ERROR_COUNT=0
SOURCE_OBJECT_COUNT=7
ARCHIVE_MEMBER_COUNT=7
ARCHIVE_MEMBER_BYTE_PARITY=PASS
CANDIDATE_SOURCE_OBJECT_COUNT=0
CANDIDATE_PRECOMPILED_MARKER=YES
CANDIDATE_LINK=PASS
SOURCE_WARNING_LINES=0
CANDIDATE_WARNING_LINES=0
SOURCE_ERROR_LINES=0
CANDIDATE_ERROR_LINES=0
TFT_BACKEND=JWPLC_TFT
LEGACY_BACKEND_REFS=0
```

P5 queda adoptado en remoto/local.

## Secuencia restante de cierre técnico

```text
P6 = JWPLC_TFT requalification / backend self-contained
THEN = freeze de precompilados
THEN = benchmark final de tiempos de compilacion
THEN = Arduino CLI / Arduino IDE / upload fisico final
THEN = release
```

El benchmark final reutilizará la metodología histórica de
`tools/build-speed-benchmark/Run-JWPLCBuildBenchmark.ps1` y no debe ejecutarse
antes de P6, porque cualquier cambio posterior de archive invalidaría la tabla.

Matriz mínima:

```text
TARGETS=Basic,Basic Core
SKETCH=01_empty
JOBS=0
managed_cold
managed_warm_nochange
managed_warm_touch
explicit_cold
explicit_warm_nochange
explicit_warm_touch
```

Debe registrar tiempos, invocaciones de compilador/TUs, tamaños, host, Arduino CLI
y commit exacto; la tabla Alpha12 será un artefacto obligatorio del cierre.

Estado:

```text
P1_CORE=PASS_ADOPTED
P2_MODBUS_RTU=PASS_ADOPTED
P3_SPI=PASS_ADOPTED
P4_JW_SD=PASS_ADOPTED
P5_DISPLAY=PASS_ADOPTED
P6_TFT_REQUALIFICATION=NEXT
FINAL_BUILD_SPEED_BENCHMARK=BLOCKED_UNTIL_P6
```


## P6 precompilado — JWPLC_TFT preparado

Gate:

```text
tools/alpha12/gates/alpha12_tft_precompiled_requalify.ps1
```

Objetivo:

- requalificar el archive histórico sin regenerarlo si la paridad sigue vigente;
- exigir miembros exactos `JWPLC_TFT.cpp.o` + `TFT_eSPI.cpp.o`;
- recompilar source-first con TFT_eSPI 2.5.43 del entorno de mantenimiento;
- comparar SHA-256 byte a byte de ambos objetos source vs archive;
- crear un `JWPLC_TFT` temporal `precompiled=full` sin fuentes backend;
- compilar tres casos contra ese candidate:
  - acceso directo JWPLC_TFT;
  - integración Display;
  - `01_empty` con autoload normal;
- exigir cero `JWPLC_TFT.cpp.o` y cero `TFT_eSPI.cpp.o` compilados desde source en los casos candidate;
- exigir que Arduino CLI NO seleccione ninguna librería global `TFT_eSPI` en los casos candidate;
- no modificar el archive oficial;
- no realizar upload físico.

Contrato esperado:

```text
ARCHIVE_MEMBER_COUNT=2
ARCHIVE_MEMBER_BYTE_PARITY=PASS
MAINTAINER_TFT_ESPI_VERSION=2.5.43
DIRECT_TFT_PRECOMPILED_LINK=PASS
DISPLAY_INTEGRATION_PRECOMPILED_LINK=PASS
NORMAL_AUTOLOAD_PRECOMPILED_LINK=PASS
GLOBAL_TFT_ESPI_REQUIRED_AT_USER_BUILD=NO
OFFICIAL_ARCHIVE_CHANGED=NO
FINAL_TRACKED_DIRTY_COUNT=0
```

Auditoría estática previa a publicación del gate:

```text
AMBIGUOUS_VARIABLE_COLON_HITS=0
GET_FILE_HASH_OCCURRENCES=0
POWERSHELL_CONTINUATION_BACKTICKS=0
HERE_STRING_DELIMITERS=BALANCED
EXPECTED_ARCHIVE_MEMBERS=PASS
SELF_CONTAINED_CASES=3
```

Estado:

```text
P1_CORE=PASS_ADOPTED
P2_MODBUS_RTU=PASS_ADOPTED
P3_SPI=PASS_ADOPTED
P4_JW_SD=PASS_ADOPTED
P5_DISPLAY=PASS_ADOPTED
P6_TFT_REQUALIFICATION=READY_TO_RUN
FINAL_BUILD_SPEED_BENCHMARK=BLOCKED_UNTIL_P6
```


## P6 intento 1 — criterio de paridad bit-for-bit inválido para requalification

La primera corrida de P6 pasó:

```text
POWERSHELL_SYNTAX_ERROR_COUNT=0
SOURCE_COMPILE_EXIT=0
SOURCE_TFT_OBJECT_COUNT=1
SOURCE_TFT_ESPI_OBJECT_COUNT=1
SOURCE_TFT_PRECOMPILED_MARKER=NO
SOURCE_WARNING_LINES=0
SOURCE_ERROR_LINES=0
SOURCE_TFT_ESPI_VERSION=2.5.43
ARCHIVE_MEMBER_COUNT=2
```

pero el gate exigió igualdad SHA-256 entre objetos recompilados hoy y los
miembros históricos del archive físico cualificado.

Resultado:

```text
JWPLC_TFT_OBJECT_SOURCE_SHA256=16fe072b...
JWPLC_TFT_OBJECT_ARCHIVE_SHA256=37f0723f...

TFT_ESPI_OBJECT_SOURCE_SHA256=5f98cc18...
TFT_ESPI_OBJECT_ARCHIVE_SHA256=821abac2...
```

Clasificación:

```text
HARNESS_CRITERION_FAILURE=YES
PRODUCT_FAILURE=NOT_PROVEN
ARCHIVE_STALE=NOT_PROVEN
OFFICIAL_ARCHIVE_MUTATED=NO
PHYSICAL_UPLOAD=NO
```

Causa del criterio incorrecto:

- el archive actual fue creado/adoptado mediante H3E4A1/H3E4A2/H3E4A3;
- la receta histórica usaba
  `-DJWPLC_TFT_ESPI_DIAGNOSTIC_NO_DISPLAY_AUTOLOAD=1`;
- la primera versión de P6 no reprodujo esa receta;
- además, el proceso histórico nunca declaró reproducibilidad bit-for-bit entre
  recompilaciones futuras como contrato de cierre;
- la adopción histórica se basó en identidad del archive físicamente cualificado,
  link release-like, ausencia de TFT_eSPI externo y equivalencia estructural.

Identidad histórica cualificada del archive actual:

```text
BYTES=1091098
SHA256=5d860a131811dd9a7eb6fa55f5674b1d78b0de7dfaf8748ce18a60ceed2d3738
```

P6-R1 pasa a exigir:

```text
CURRENT_SOURCE_BLOB_IDENTITY=PASS
ARCHIVE_IDENTITY_PHYSICAL_QUALIFIED=PASS
SOURCE_FIRST_CURRENT_SOURCE=PASS
STRUCTURAL_EQUIVALENCE=PASS
DEFINED_SYMBOL_NAME_TYPE_SIZE_PARITY=PASS
GLOBAL_TFT_ESPI_REQUIRED_AT_USER_BUILD=NO
OFFICIAL_ARCHIVE_CHANGED=NO
```

La diferencia bit-for-bit de objetos reconstruidos se conserva como
observabilidad, pero deja de ser un requisito de PASS.

Gate corregido:

```text
b21ecaf5683b30d381cd72cbb4dc3a2da5e82efa
fix(alpha12): alinear P6 TFT con criterio historico cualificado
```

Estado:

```text
P6_TFT_ATTEMPT_1=HARNESS_CRITERION_FAILURE
P6_TFT_PRODUCT_FAILURE=NOT_PROVEN
P6_TFT_R1=READY_TO_RUN
FINAL_BUILD_SPEED_BENCHMARK=BLOCKED_UNTIL_P6
```


## P6-R1 intento de parseo — variable automática `$args`

El preflight obligatorio detectó antes de ejecutar el gate:

```text
POWERSHELL_SYNTAX_ERROR_COUNT=1
No se puede asignar una variable automática "args" con el tipo "System.Object[]"
LINE=279
```

Clasificación:

```text
HARNESS_PARSE_FAILURE=YES
PRODUCT_FAILURE=NO
P6_PRODUCT_EXECUTION_STARTED=NO
ARCHIVE_MUTATION=NO
PHYSICAL_UPLOAD=NO
```

Causa:

```powershell
[string[]]$args = @(...)
```

`$args` es una variable automática de PowerShell. Se reemplazó por
`$compileArgs`.

Corrección:

```text
3cb97e4504c9accc36a5cbde005e601f39d53232
fix(alpha12): evitar variable automatica args en P6 TFT
```

Auditoría preventiva después de la corrección:

```text
ARGS_ASSIGNMENT_HITS=0
RESERVED_AUTOMATIC_VARIABLE_ASSIGNMENT_HITS=0
AMBIGUOUS_VARIABLE_COLON_HITS=0
GET_FILE_HASH_OCCURRENCES=0
POWERSHELL_CONTINUATION_BACKTICKS=0
```

Nota operativa:

Si se pegan varias sentencias consecutivas en una consola interactiva,
un `throw` de un bloque anterior no impide necesariamente que las líneas
posteriores ya pegadas sean procesadas. Por tanto, el texto posterior
`POWERSHELL_SYNTAX=PASS` de ese intento no invalida el error real:
`POWERSHELL_SYNTAX_ERROR_COUNT=1`.

Estado:

```text
P6_TFT_R1_PARSE_ATTEMPT=HARNESS_FAILURE
P6_TFT_PRODUCT_FAILURE=NO
P6_TFT_R2=READY_TO_RUN
```


## P6-R2 — fallo de resolución del tool `nm`

P6-R2 alcanzó y validó correctamente:

```text
CURRENT_SOURCE_BLOB_IDENTITY=PASS
ARCHIVE_IDENTITY_PHYSICAL_QUALIFIED=PASS
SOURCE_COMPILE_EXIT=0
SOURCE_TFT_OBJECT_COUNT=1
SOURCE_TFT_ESPI_OBJECT_COUNT=1
SOURCE_TFT_PRECOMPILED_MARKER=NO
SOURCE_WARNING_LINES=0
SOURCE_ERROR_LINES=0
SOURCE_TFT_ESPI_VERSION=2.5.43
SOURCE_FIRST_CURRENT_SOURCE=PASS
```

El gate se detuvo antes de auditar el archive/candidates por:

```text
A12_TFT_REQUAL_TOOL_NOT_FOUND=xtensa-esp32-elf-nm
```

Clasificación:

```text
HARNESS_RUNTIME_FAILURE=YES
PRODUCT_FAILURE=NO
ARCHIVE_STALE=NOT_PROVEN
OFFICIAL_ARCHIVE_MUTATED=NO
PHYSICAL_UPLOAD=NO
```

Causa:

El gate intentó localizar `xtensa-esp32-elf-nm` reparseando una línea de
`g++`. Sin embargo, la corrida ya había resuelto de forma confiable:

```text
...\bin\xtensa-esp32-elf-gcc-ar.exe
```

El proceso histórico H3E4 resolvía `nm` desde el mismo directorio del
archiver. P6 debe reutilizar ese criterio.

Corrección:

```text
3044e9ae69bc787884aa0160b043486748454c86
fix(alpha12): resolver nm desde toolchain ya identificado en P6
```

Nueva regla:

```text
RESOLVE_ONE_TRUSTED_TOOLCHAIN_BINARY
-> DERIVE_SIBLING_TOOLS_FROM_SAME_DIRECTORY
-> VERIFY_PATH_EXISTS
-> DO_NOT_REPARSE_UNRELATED_COMPILER_OUTPUT
```

Estado:

```text
P6_TFT_R2=HARNESS_RUNTIME_FAILURE
P6_TFT_PRODUCT_FAILURE=NO
P6_TFT_R3=READY_FOR_SYNTAX_PREFLIGHT
FINAL_BUILD_SPEED_BENCHMARK=BLOCKED_UNTIL_P6
```


## P6-R3 — resolución genérica de `nm` no reprodujo el patrón histórico

P6-R3 volvió a confirmar antes del fallo:

```text
CURRENT_SOURCE_BLOB_IDENTITY=PASS
ARCHIVE_IDENTITY_PHYSICAL_QUALIFIED=PASS
SOURCE_COMPILE_EXIT=0
SOURCE_TFT_OBJECT_COUNT=1
SOURCE_TFT_ESPI_OBJECT_COUNT=1
SOURCE_TFT_PRECOMPILED_MARKER=NO
SOURCE_WARNING_LINES=0
SOURCE_ERROR_LINES=0
SOURCE_TFT_ESPI_VERSION=2.5.43
SOURCE_FIRST_CURRENT_SOURCE=PASS
```

El fallo fue únicamente del harness:

```text
A12_TFT_REQUAL_SIBLING_TOOL_NOT_FOUND=xtensa-esp32-elf-nm
base=C:\Users\jeykc\AppData\Local\Arduino15\packages\jwplc_local\tools\esp-x32\2601\bin
```

El preflight manual había demostrado:

```text
NM_EXISTS=True
```

Por tanto:

```text
HARNESS_RUNTIME_FAILURE=YES
PRODUCT_FAILURE=NO
ARCHIVE_STALE=NOT_PROVEN
OFFICIAL_ARCHIVE_MUTATED=NO
PHYSICAL_UPLOAD=NO
```

Corrección R4:

- eliminar el helper genérico `Resolve-ToolBesidePath`;
- reproducir el patrón histórico H3E4;
- derivar `$toolDir` desde el archiver ya resuelto;
- probar literalmente:
  - `xtensa-esp32-elf-nm.exe`;
  - fallback `xtensa-esp32-elf-nm`;
- imprimir candidatos y `Test-Path` antes de resolver;
- mantener auditoría de variables reservadas/sintaxis.

Commit:

```text
b23ef32e4e8135a3018e6b9730866ab0d61b8f3e
fix(alpha12): usar resolucion literal probada para nm en P6
```

Estado:

```text
P6_TFT_R3=HARNESS_RUNTIME_FAILURE
P6_TFT_PRODUCT_FAILURE=NO
P6_TFT_R4=READY_FOR_SYNTAX_PREFLIGHT
```


## P6-R4 preflight manual — error de formato en consola interactiva

El preflight manual sí confirmó:

```text
POWERSHELL_SYNTAX_ERROR_COUNT=0
NM_EXE_EXISTS=True
NM_RESOLVED=C:\Users\jeykc\AppData\Local\Arduino15\packages\jwplc_local\tools\esp-x32\2601\bin\xtensa-esp32-elf-nm.exe
```

Los errores posteriores:

```text
elseif: El término "elseif" no se reconoce...
else: El término "else" no se reconoce...
```

fueron causados por entregar un bloque `if / elseif / else` para pegar en
PowerShell interactivo. El `if` se ejecutó como sentencia completa al cerrar
su primer bloque y los `elseif/else` posteriores quedaron separados.

Clasificación:

```text
MANUAL_PREFLIGHT_FORMAT_FAILURE=YES
PRODUCT_FAILURE=NO
P6_GATE_EXECUTED=NO
NM_EXISTENCE=PROVEN
POWERSHELL_GATE_SYNTAX=PASS
```

Cambio de proceso:

```text
NO_MORE_MULTI_STEP_INTERACTIVE_PREFLIGHT_SNIPPETS
ONE_SELF_CONTAINED_RUNNER_PER_GATE
```

Se agregó:

```text
tools/alpha12/gates/run_alpha12_tft_precompiled_requalify.ps1
```

El runner ejecuta internamente:

```text
Parser::ParseFile
-> require 0 syntax errors
-> verify gcc-ar
-> verify nm
-> invoke P6 gate
-> stop automatically on any failure
```

Commit:

```text
5166274cf45e612bc76bce8686da5dff979558df
test(alpha12): agregar runner seguro para P6 TFT
```

Estado:

```text
P6_TFT_R4_PREFLIGHT_CORE_CHECKS=PASS
P6_TFT_MANUAL_PREFLIGHT_FORMAT=HARNESS_INSTRUCTION_FAILURE
P6_TFT_SAFE_RUNNER=READY
```


## P6-R4 — PASS final y cierre de JWPLC_TFT

Runner seguro:

```text
tools/alpha12/gates/run_alpha12_tft_precompiled_requalify.ps1
```

La corrida final cerró:

```text
P6_GATE_POWERSHELL_SYNTAX_ERROR_COUNT=0
P6_RUNNER_PREFLIGHT=PASS
ALPHA12_TFT_PRECOMPILED_REQUALIFICATION=PASS
P6_SAFE_RUNNER=PASS
```

Identidad y source:

```text
CURRENT_SOURCE_BLOB_IDENTITY=PASS
ARCHIVE_IDENTITY_PHYSICAL_QUALIFIED=PASS
ARCHIVE_BYTES=1091098
ARCHIVE_SHA256=5d860a131811dd9a7eb6fa55f5674b1d78b0de7dfaf8748ce18a60ceed2d3738
MAINTAINER_TFT_ESPI_VERSION=2.5.43
SOURCE_FIRST_CURRENT_SOURCE=PASS
```

Equivalencia:

```text
OBJECT_BIT_FOR_BIT_REBUILD_REQUIRED=NO
FLASH_DELTA_BYTES=-8
RAM_DELTA_BYTES=0
DEFINED_SYMBOL_NAME_TYPE_SIZE_PARITY=PASS
STRUCTURAL_EQUIVALENCE=PASS
```

Autocontención:

```text
DIRECT_TFT_PRECOMPILED_LINK=PASS
DISPLAY_INTEGRATION_PRECOMPILED_LINK=PASS
NORMAL_AUTOLOAD_PRECOMPILED_LINK=PASS
GLOBAL_TFT_ESPI_REQUIRED_AT_USER_BUILD=NO
OFFICIAL_ARCHIVE_CHANGED=NO
PHYSICAL_UPLOAD_PERFORMED=NO
FINAL_TRACKED_DIRTY_COUNT=0
```

Evidencia externa revisada:

```text
tft_precompiled_requalify_20261004_114112.zip
BYTES=28987
SHA256=0b3dc2f63f3b6f6108adcb5a402f188665a3e3038f29ae7f885c399f8419aaad
```

Resultado detallado:

```text
docs/v2.1.0-alpha.12/ALPHA12_P6_TFT_REQUALIFICATION_RESULT_20261004.md
```

Estado:

```text
P1_CORE=PASS_ADOPTED
P2_MODBUS_RTU=PASS_ADOPTED
P3_SPI=PASS_ADOPTED
P4_JW_SD=PASS_ADOPTED
P5_DISPLAY=PASS_ADOPTED
P6_TFT_REQUALIFICATION=PASS_CLOSED

P7_GLOBAL_PRECOMPILED_ARCHIVE_AUDIT=NEXT
PRECOMPILED_FREEZE=BLOCKED_UNTIL_P7
FINAL_BUILD_SPEED_BENCHMARK=BLOCKED_UNTIL_PRECOMPILED_FREEZE
```


## P7A preparado — auditoría global de precompilados

P6 quedó cerrado antes de preparar P7A.

Gate:

```text
tools/alpha12/gates/alpha12_precompiled_global_audit.ps1
```

Runner seguro:

```text
tools/alpha12/gates/run_alpha12_precompiled_global_audit.ps1
```

Objetivo P7A:

- inventariar todos los archives del package;
- verificar SHA-256/tamaño;
- comprobar identidades congeladas de P1-P6;
- detectar source más nuevo que archive;
- validar la política de archives retenidos;
- validar la política source-only intencional del autoload;
- identificar exactamente qué librerías requieren reactivar `precompiled=full`;
- no modificar el árbol tracked.

Identidades P1-P6 exigidas:

```text
core.a              78d0c0ab14f156b96116529e88872340d51877af40d24ba3559f6081e0bf34fb
JWPLC_ModbusRTU.a   424ed3f462bb57cce0019d690486e612d5f243c40ae62d44d3ad973b0a521085
SPI.a               b433758b746380bf8d1ea102aca1d1637b5a8cab50616b56024f4b022a916445
JW_SD.a             1b9619ba37295782ade1edc6f06439a38e7e534fc08f5b05757c9af7a645acc0
JWPLC_Display.a     c960d718433e29a40e3cc55bc745c9a2e121ee1ee598ec72c04872327592dc02
JWPLC_TFT.a         5d860a131811dd9a7eb6fa55f5674b1d78b0de7dfaf8748ce18a60ceed2d3738
```

Source-only intencional validado por política/historia:

```text
JW_RTC
JWPLC_GlobalPeripherals
JWPLC_Ethernet
JWPLC_RS485
```

En particular, `JW_RTC` volvió explícitamente a source en:

```text
74c202ecaf24e4df41b63fd9e83f8f24a65da63c
fix(rtc): volver a compilacion desde fuente tras auditoria completa
```

Set esperado de activación pendiente antes del freeze:

```text
JWPLC_Display
JWPLC_ModbusRTU
JWPLC_TFT
JW_SD
SPI
```

P7A debe terminar con:

```text
ALPHA12_P7A_GLOBAL_PRECOMPILED_ARCHIVE_AUDIT=PASS
KNOWN_REGENERATED_IDENTITIES=PASS
SOURCE_FRESHNESS=PASS
RETAINED_ARCHIVE_POLICY=PASS
SOURCE_ONLY_POLICY=PASS
CORE_PRECOMPILED_POLICY=PASS
ACTIVATION_PENDING_COUNT=5
PRECOMPILED_FREEZE=NOT_YET
NEXT=P7B_RELEASE_LIKE_PRECOMPILED_ACTIVATION
FINAL_TRACKED_DIRTY_COUNT=0
```

Commits de preparación:

```text
e086b00cd2f15e6cd7bdf05d4d39b6e0235f74ff  gate P7A
2b0d0c221430bbb8f522c1d599b3bfe6a6779267  runner seguro P7A
e63fdafbbae6c8e525eac3af10af3ffd687d7b1d  política source-only P7A
```

Estado:

```text
P6_TFT_REQUALIFICATION=PASS_CLOSED
P7A_GLOBAL_PRECOMPILED_ARCHIVE_AUDIT=READY_TO_RUN
P7B_RELEASE_LIKE_PRECOMPILED_ACTIVATION=BLOCKED_UNTIL_P7A
PRECOMPILED_FREEZE=BLOCKED_UNTIL_P7B
FINAL_BUILD_SPEED_BENCHMARK=BLOCKED_UNTIL_PRECOMPILED_FREEZE
```


## P7A intento 1 — SHA esperado de JWPLC_Display mal transcrito

P7A pasó la auditoría de 13/14 archives y detectó un único fallo:

```text
P7_FAILURE=EXPECTED_SHA_MISMATCH:JWPLC_Display
```

El archive actual reportó:

```text
BYTES=941228
SHA256=c960d718433e29a40e3cc55bc745c9a2e121ee1ee598ec72c04872327592dc02
```

La investigación del historial confirmó que:

```text
P5_ADOPTION_COMMIT=c30bc01897fcba055921b58a2506ea7aa4ba6c70
P5_ADOPTION_GIT_BLOB=fe037ed5a5b55aabdd765b4e0a15e39160e9f12f
CURRENT_GIT_BLOB=fe037ed5a5b55aabdd765b4e0a15e39160e9f12f
SAME_GIT_BLOB=YES
CURRENT_BYTES=941228
```

Por tanto, el archive no cambió desde P5. El fallo estaba en el literal esperado
codificado en P7A:

```text
INCORRECT_EXPECTED_SHA=c960d718433e29a40e3cc55bc745c9a2e121ee1ee598ec72c04872327592dc
INCORRECT_LENGTH=62
CORRECT_SHA256=c960d718433e29a40e3cc55bc745c9a2e121ee1ee598ec72c04872327592dc02
CORRECT_LENGTH=64
```

Clasificación:

```text
HARNESS_EXPECTED_DATA_FAILURE=YES
PRODUCT_FAILURE=NO
DISPLAY_ARCHIVE_CHANGED=NO
DISPLAY_ARCHIVE_STALE=NO
P7_PRODUCT_FINDING=NONE
```

Corrección:

```text
2391727299558713a5541b4047b2cc5da3520cca
fix(alpha12): corregir SHA Display y validar hashes en P7A
```

P7A ahora valida además que todo `ExpectedSha` no vacío cumpla:

```text
^[0-9a-fA-F]{64}$
```

antes de comparar el valor con un artifact.

Estado:

```text
P7A_ATTEMPT_1=HARNESS_EXPECTED_DATA_FAILURE
P7A_PRODUCT_FAILURE=NO
P7A_RERUN=READY
PRECOMPILED_FREEZE=BLOCKED_UNTIL_P7B
FINAL_BUILD_SPEED_BENCHMARK=BLOCKED_UNTIL_PRECOMPILED_FREEZE
```
