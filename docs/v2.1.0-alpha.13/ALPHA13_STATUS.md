## G8-P0 — Ejecutable read-only preparado y CI PASS (2026-10-11)

```text
ALPHA13_STATUS=IN_PROGRESS
G7C=CLOSED_PASS
A13-009=CLOSED_PASS
A13-013=CLOSED_PASS
G8_SCOPE=A13-010_MODBUS_RTU_RX_BYTES+A13-011_RS485_UART_READY
G8_P0_RUNNER=tools/alpha13/gates/a13_g8_p0_baseline.py
G8_P0_WRAPPER=tools/alpha13/gates/run_a13_g8_p0_baseline.bat
G8_P0_CI_SOURCE_STATIC=PASS_RUN_38185801023
G8_P0_WINDOWS_COMPILE_RUN=NOT_EXECUTED
G8_P0_PRODUCT_CHANGE=NO
G8_P0_USB_SERIAL_FRAM_ACCESS=NO
G8_P0_GIT_COMMIT_PUSH=NO
NEXT_GATE=A13-G8_P0_MODBUS_RTU_RS485_BASELINE_RUN
F120_ROOT_CAUSE=UNKNOWN_PENDING
RELEASE_MERGE_PR_PUBLICATION=NO
```

Baseline P0 preparado bajo `tools/alpha13/gates`: preflight
rama/HEAD, CLI 1.5.1, fuentes/bibliotecas/archives, contrato exacto
de contadores RTU y readiness RS485, compilaciones normales Slave,
Master y RS485 Status; registrar SHA, linker `-L/-lJWPLC_ModbusRTU`
y objeto `JWPLC_RS485.cpp.o`. Sin acceso físico, sin Git de producto.
La [CI G8 P0](https://github.com/JW-Control/platform-jwplc/actions/runs/38185801023)
ha validado sintaxis y detección estática sobre el código fuente real.
Aún falta ejecutar las compilaciones normales en el Windows del operador.
Plan: [A13_G8_P0_MODBUS_RTU_RS485_BASELINE_PLAN_20261011.md](A13_G8_P0_MODBUS_RTU_RS485_BASELINE_PLAN_20261011.md).

---

## G7C — Cierre canónico FRAM productivo verificado (2026-10-11)

```text
ALPHA13_STATUS=IN_PROGRESS
G1=CLOSED_PASS
G2=CLOSED_PASS
TFT_CLOSURE=CLOSED_PASS
G3=CLOSED_PASS
G4=CLOSED_PASS
G5=CLOSED_PASS
G6=CLOSED_PASS
G7_FRAM=CLOSED_PASS
G7C_FRAM_CANONICAL=CLOSED_PASS
A13-009=CLOSED_PASS
A13-013=CLOSED_PASS
G7C_PRODUCT_COMMIT=f2a93a8e5cbb736bbaa98ded9c7d2845582b7b32
G7C_PRODUCT_PUSH=PASS_VERIFIED_REMOTE
G7C_GIT_RUN_ID=g7c_git_20261011_184501_0e9245c3
G7C_ADOPTION_RUN_ID=g7c_adopt_20261011_182119_efba4050
G7C_OFFLINE_RUN_ID=g7c_20261011_175217_3b6759cd
G7C_FRAM_ARCHIVE_SHA256=df390f3379da13c6d4503a9fb83bc098f2944590fe3ad083348e95bfdcaf7299
G7C_REGRESSIONS=PASS_2_OF_2
G7C_NATIVE_TESTS=PASS_11_OF_11
G7C_CI_POST_PRODUCT_COMMIT=PASS_RUN_38185454578
G7C_RUNTIME_PHYSICAL_RETEST=NOT_EXECUTED_NOT_REQUIRED_BY_CURRENT_DECISION
G7C_FRAM_WRITE=NONE
G7C_RELEASE_MERGE_PR_PUBLICATION=NO
F120_ROOT_CAUSE=UNKNOWN_PENDING
G8_A13-010=PENDING
G8_A13-011=PENDING
G8_P0=NOT_EXECUTED
NEXT_GATE=A13-G8_P0_MODBUS_RTU_RS485_BASELINE
OPENPLC=OUT_OF_SCOPE
HMI_DESIGNER=OUT_OF_SCOPE
TFT_NEW_FEATURES=OUT_OF_SCOPE
```

GitHub confirmó el commit productivo `f2a93a8` con SOLO
`JW_FRAM.cpp`, `JW_FRAM.h` y `libJW_FRAM.a`; CI posterior PASS.
No confundir este cierre técnico con ZIP/PreRelease publicado.
Se preservan el cierre original G7 y la causa desconocida de F120.
Documento detallado: [A13_G7C_FRAM_CLOSURE_20261011.md](A13_G7C_FRAM_CLOSURE_20261011.md).

**Precedencia:** este estado al inicio del documento reemplaza los
`G7C=NOT_ADOPTED` y `NEXT_GATE=G7C_OFFLINE_INTEGRATED` históricos
que permanecen debajo para trazabilidad. Abrir G8 únicamente por P0
read-only; no modificar todavía código productivo de Modbus RTU/RS485.

---

## G7C — Adopción local controlada preparada, NO ejecutada (2026-10-11)

```text
G7C_CANDIDATE_OFFLINE=PASS
G7C_NATIVE_CI_11_OF_11=PASS
G7C_ADOPTION_RUNNER=tools/alpha13/gates/a13_g7c_adopt_local.py
G7C_ADOPTION_WRAPPER=tools/alpha13/gates/run_a13_g7c_adopt.bat
G7C_ADOPTION_RUNNER_STATUS=VERSIONED_NOT_EXECUTED_WINDOWS
G7C_ADOPTION_SOURCE_RUN=g7c_20261011_175217_3b6759cd
G7C_ADOPTION_ARCHIVE_SHA256=df390f3379da13c6d4503a9fb83bc098f2944590fe3ad083348e95bfdcaf7299
G7C_ADOPTION_MODE=BACKUP_THEN_3_PRODUCT_FILES_THEN_2_REGRESSIONS
G7C_ON_FAILURE=ROLLBACK_AND_REVIEW_STOP
G7C_ON_PASS=PRODUCT_ADOPTED_REGRESSIONS_PASS_PENDING_COMMIT
G7C_UPLOAD_OR_FRAM_WRITE=NO
G7C_GIT_COMMIT_OR_PUSH=NO
G7C_NEXT_GATE=RUN_LOCAL_PRODUCT_ADOPTION
A13-009=CANDIDATE_VALIDATED_NOT_ADOPTED
A13-013=CANDIDATE_VALIDATED_NOT_ADOPTED
G8=NOT_STARTED
```

El ejecutor usa exclusivamente las fuentes y el archive de la corrida
offline identificada y valida SHA de sus artefactos, CI nativo, baseline
productivo, core/TFT y rama. Respalda los tres archivos en TEMP; ante
fallo posterior al respaldo restaura bytes y hashes. Tras adoptar exige
dos compilaciones productivas con enlace del archive nuevo, sin modificar
el firmware grabado, sin tocar FRAM y sin `git add/commit/push`.

**No marcar G7C cerrado** antes de recibir el resultado Windows y
decidir el commit productivo exacto. No iniciar G8.

---

## G7C — Candidato validado offline y nativo, NO adoptado (2026-10-11)

```text
G1_G7=CLOSED_PASS
G7C_WINDOWS_RUN_ID=g7c_20261011_175217_3b6759cd
G7C_WINDOWS_HEAD=20858de6d25bb23428488800f0f4e03d2fb846d5
G7C_SOURCE_CONTRACT=PASS
G7C_REBUILD_ARCHIVE_LINK=PASS
G7C_REGRESSIONS=PASS_2_OF_2
G7C_FINAL_AUDIT=PASS_PRODUCT_UNCHANGED
G7C_NATIVE_WINDOWS=NOT_AVAILABLE_PENDING_IN_THAT_RUN
G7C_NATIVE_GITHUB_ACTIONS=PASS_11_OF_11
G7C_NATIVE_CI_RUN=38183543776
G7C_NATIVE_CI_SHA=cd5172d99fb9d0ade76e3b36f106436cc4c08434
G7C_SOURCE_OBJECT_SHA256=2d635936aeb0096a7e3dd8051e2c4a23e56dc863922f0a114cc3b2b56323366c
G7C_CANDIDATE_ARCHIVE_SHA256=df390f3379da13c6d4503a9fb83bc098f2944590fe3ad083348e95bfdcaf7299
G7C_CANDIDATE_VALIDATION=PASS
A13-009=CANDIDATE_VALIDATED_NOT_ADOPTED
A13-013=CANDIDATE_VALIDATED_NOT_ADOPTED
G7C_PRODUCT_FILES=UNCHANGED
G7C_PRODUCT_COMMIT=NO
G7C_PHYSICAL_UPLOAD_OR_FRAM_ACCESS=NONE
G7C_NEXT_GATE=ADOPTION_WITH_ROLLBACK_AND_PRODUCT_REGRESSIONS
G8=NOT_STARTED
F120_POST_UPLOAD_FREEZE_CAUSE=UNKNOWN_PENDING
RELEASE_MERGE_PR_PUBLICATION=NO
```

Evidencia Windows: ejecución enviada por el operador; reconstrucción fuente→objeto→archive,
enlace `-L/-lJW_FRAM`, regresiones `GLOBAL_FRAM` y `CONFIG_BLOCK`,
auditoría final sin escrituras de producto. La prueba C++ nativa no estuvo
disponible en Windows y **no** se contabiliza como PASS de ese entorno.

Evidencia nativa independiente: [GitHub Actions G7C #1](https://github.com/JW-Control/platform-jwplc/actions/runs/38183543776),
compilación Linux g++ del cuerpo productivo extraído con 11/11 escenarios PASS,
`G7C_NATIVE_CPP=PASS_ACTUAL_EXTRACTED_METHOD`. El mock necesitó conservar el
valor predeterminado de `maxLen`, corregido solo en tooling (`a97e908`).
Workflow de pruebas en `cd5172d`.

El candidato tiene evidencia suficiente para **preparar** adopción controlada:
copiar fuentes + archive exacto con backup, compilar regresiones contra el archive
productivo y revisar hashes, Git y rollback. No actualizar aún A13-009/013 a
`CLOSED_PASS`, ni iniciar G8, hasta validar esa adopción y cerrar Git.

---

# v2.1.0-alpha.13 — Estado operativo y continuidad

## G7C — Candidato canónico preparado, pendiente de ejecución offline (2026-10-11)

```text
G1_G7=CLOSED_PASS
G7C_SCOPE=A13-009_readString+A13-013_private_cleanup
G7C_CANDIDATES=VERSIONED_NOT_ADOPTED
G7C_SRC_CPP=tools/alpha13/candidates/g7c/JW_FRAM.cpp
G7C_SRC_H=tools/alpha13/candidates/g7c/JW_FRAM.h
G7C_SOURCE_CONTRACT=READY_NOT_EXECUTED
G7C_NATIVE_RUNTIME=READY_NOT_EXECUTED
G7C_OBJECT_ARCHIVE_REBUILD=READY_NOT_EXECUTED
G7C_CANDIDATE_LINK_REGRESSIONS=READY_NOT_EXECUTED
G7C_NORMAL_PRODUCT_FILES=UNCHANGED
G7C_FRAM_PHYSICAL=NOT_REQUESTED
G7C_GIT_PRODUCT_COMMIT=NO
G7C_GIT_PUSH_PRODUCT=NO
A13-009=IN_PROGRESS_NOT_CLOSED
A13-013=IN_PROGRESS_NOT_CLOSED
NEXT_GATE=G7C_OFFLINE_INTEGRATED
RELEASE_MERGE_PR_PUBLICATION=NO
```

El cierre G7 ya confirmado no reemplaza A13-009/013.
Se preparó nuevo candidato mínimo y runner offline de
fuente→objeto→archive→enlace→2 regresiones; la
prueba unitaria nativa extrae el cuerpo C++ real y
tiene 11 escenarios. El resultado debe interpretarse
como candidato técnico, NO cierre productivo G7C.
Plan: `A13_G7C_FRAM_CANONICAL_PLAN_20261011.md`.

---


## Conciliación canónica G7 y preparación G8 (2026-10-11)

```text
G7_TECHNICAL_GATE=CLOSED_PASS
G7_COMMIT_PRODUCT=a166d08e4eb1467c6237ba6196b687f381554b44
G7_COMMIT_DOCS=cd3fe89d2dbf882bbdb6b8a04f2f7d1d916df16a
A13-009=PENDING_CANONICAL_FIX_readString_length_SPI_failure
A13-013=PENDING_CANONICAL_CLEANUP_FRAM_ctors_debugPrint
G7_CANONICAL_MAPPING=RECONCILED_WITH_PENDING_SCOPE
G8_SCOPE=MODBUS_RTU_RX_COUNTER_AND_RS485_UART_READY
G8_A13-010=PENDING
G8_A13-011=PENDING
G8_P0=READ_ONLY_PLAN_PREPARED_NOT_EXECUTED
NEXT_ACTION=DECIDE_G7_CANONICAL_DELTA_PRIORITY_VS_G8_P0
F120_POST_UPLOAD_FREEZE_CAUSE=UNKNOWN_PENDING
RELEASE_MERGE_PR_PUBLICATION=NO
```

El hito G7 cerrado es válido para lo que implementó y probó,
pero la auditoría canónica exige además dos correcciones
FRAM aún ausentes: `readString()` no distingue error
leyendo longitud del texto vacío; y las dos sobrecargas
privadas + guards de constructores siguen en el producto.
No confundir `G7=CLOSED_PASS` con
`A13-009/013=RESOLVED`. Ver
`A13_G7_CANONICAL_RECONCILIATION_AND_G8_SCOPE_20261011.md`.

G8 debe limitarse a estadísticas RX de Modbus RTU y
readiness de RS-485, no a Serial USB genérico.
No se cambia firmware/producto en esta revisión.

---


## Estado vigente — G7 cerrado, G8 siguiente (2026-10-11)

ALPHA13_STATUS=IN_PROGRESS
G1=CLOSED_PASS
G2=CLOSED_PASS
G3=CLOSED_PASS
G4=CLOSED_PASS
G5=CLOSED_PASS
G6=CLOSED_PASS
G7_FRAM=CLOSED_PASS
G7_RUN_ID=g7_close_20261011_173351_a4c16676
G7_PHYSICAL_RUN_ID=g7_20261011_164228_e5e86ddc
G7_FRAM_RESTORED=PASS
G7_F120_POST_UPLOAD_CAUSE=UNKNOWN_PENDING
G7_ARCHIVE_SHA256=477ba0ccc3b20e638eccd7dda7d22b209d1ce73003df34917dc9ea7d476d83dd
G7_SOURCE_ARCHIVE_PARITY=PASS
G7_A13_009_013_CANONICAL_MAPPING=REVIEW_PENDING
NEXT_GATE=A13-G8_SERIAL_A13_010_A13_011
NEXT_FAILURE_ID=F122
RELEASE_MERGE_PR_PUBLICATION=NO

Cierre: A13_G7_FRAM_CLOSURE_20261011.md

---


## G7 — F121 corregido en harness, regeneración documental pendiente (2026-10-11)

```text
ALPHA13_STATUS=IN_PROGRESS
G1_G6=CLOSED_PASS
G7_FRAM_PHYSICAL=PASS
G7_FRAM_RESTORED=PASS
G7_PRODUCT_REGRESSIONS=PASS_2_OF_2
G7_FINAL_AUDIT=PASS
G7_CLOSE_PREVIOUS=REVIEW_STOP_KEYERROR_product_regressions
G7_CLOSE_ROLLBACK=PASS
G7_CLOSE_PRODUCT_COMMIT=NO
G7_CLOSE_DOCS_COMMIT=NO
G7_CLOSE_PUSH=NO
G7_F121_FIX=VERSIONED_NOT_EXECUTED_WINDOWS
G7_DOCUMENT_SELFTEST=PREPARED_NOT_EXECUTED
G7_NEW_UPLOAD_REQUIRED=NO
G7_PHYSICAL_RETEST_REQUIRED=NO
G7_ARCHIVE_SHA256=477ba0ccc3b20e638eccd7dda7d22b209d1ce73003df34917dc9ea7d476d83dd
G7_F120_POST_UPLOAD_FREEZE_CAUSE=UNKNOWN
NEXT_FAILURE_ID=F122
NEXT_GATE=A13_G7_CLOSE_FROM_PHYSICAL_PASS_RETRY
RELEASE_MERGE_PR_PUBLICATION=NO
```

El cierre R4 falló sólo al leer una clave de manifiesto
mal nombrada mientras generaba los documentos, después de
regresiones y auditoría PASS. Se restauró todo el producto,
no hubo cambios Git ni nuevas operaciones físicas.
El runner y un test real de generación documental ya se
versionaron para impedir este error al final del hito.

---


## G7 — PHYSICAL_PASS con NVS restaurado, falta cierre productivo (2026-10-11)

```text
BRANCH=v2.1.0-alpha.13/feature/cleanup-robustness
ALPHA13_STATUS=IN_PROGRESS
G1_G6=CLOSED_PASS
G7_PHYSICAL_RUN_ID=g7_20261011_164228_e5e86ddc
G7_UPLOAD=PASS_PREVIOUS_ATTEMPT
G7_APP_SHA256=30cb8c596d212b6209e3fb179c1b9f216588c2fc83225834dfb2bce7088c663a
G7_PHYSICAL=PASS_BACKUP_RESTORE_CHECKSUM_NULL_BOUNDS
G7_FRAM_STALE_RECOVERY=1
G7_FRAM_BACKUP_PASS=1
G7_FRAM_NULL_AND_BOUNDS_ERRORS=0
G7_FRAM_VALID_BLOCK_ERRORS=0
G7_FRAM_CORRUPT_BLOCK_ERRORS=0
G7_FRAM_VERSION_ERRORS=0
G7_FRAM_RESTORE_PASS=1
G7_FRAM_TOTAL_ERRORS=0
G7_FRAM_DONE=PASS_EXACT_TOKEN
G7_FRAM_RECOVERY_PROOF=PHYSICAL_PASS_JSON_CREATED
G7_NEW_UPLOAD_REQUIRED=NO
G7_ADOPTION_AND_PRODUCT_REGRESSIONS=PENDING
G7_GIT_CLOSURE=PENDING
G7_CLOSE_FROM_PHYSICAL_PASS=PREPARED_NOT_EXECUTED
F120_CAUSE_POST_UPLOAD_FREEZE=UNKNOWN
G7_CANONICAL_A13_009_013_MAPPING=REVIEW_PENDING
NEXT_FAILURE_ID=F121
RELEASE_MERGE_PR_PUBLICATION=NO
```

El operador ejecutó una única orden `G7_FRAM_RUN` sobre
firmware existente, recibida tras `READY` por COM4 con
DTR/RTS desactivados. La FRAM restauró los 128 bytes originales,
sin diferencias y con 0 errores. La prueba no repetirá
el acceso a FRAM. Se preparó cierre desde evidencia en
`a13_g7_close_from_physical_pass.py`, que comprueba
manifiesto, hashes y logs; sólo después adopta dos fuentes y
`libJW_FRAM.a` y ejecuta regresiones Arduino IDE.
**G7 no está CLOSED_PASS hasta terminar regresiones y Git.**
F120 sigue con causa de freeze post-upload no demostrada.

---


## G7 — F120 power-cycle recuperado y UART READY probado; físico pendiente (2026-10-11)

```text
ALPHA13_STATUS=IN_PROGRESS
G1_G6=CLOSED_PASS
G7_R3_SOURCE_AND_ARCHIVE=PASS
G7_R3_CANDIDATE_COMPILE_AND_LINK=PASS
G7_R3_PHYSICAL_UPLOAD=PASS
G7_R3_READY_TIMEOUT=REVIEW_STOP
G7_POWER_CYCLE_AFTER_FAILURE=OPERATOR_DISCONNECTED_AND_RECONNECTED
G7_TFT_RTC_AFTER_POWER_CYCLE=NORMAL_OPERATOR_OBSERVATION
G7_UART_PASSIVE=29_READY_EXACT_IN_20_SECONDS
G7_UART_PANIC_REBOOT_MARKERS=0
G7_FIRMWARE_TOKEN=g7_20261011_164228_e5e86ddc
G7_APP_SHA256=30cb8c596d212b6209e3fb179c1b9f216588c2fc83225834dfb2bce7088c663a
G7_FRAM_RUN_COMMAND=NOT_YET_SENT
G7_FRAM_BACKUP=NOT_YET_STARTED
G7_FRAM_RESTORE=NOT_YET_TESTED
G7_PHYSICAL_ONLY_RECOVERY=PREPARED_NOT_EXECUTED
G7_REFLASH=NOT_REQUIRED
G7_PRODUCT_ADOPTION=NO
F120_SYMPTOM=RECOVERED_AFTER_POWER_CYCLE_CAUSE_UNKNOWN
NEXT_FAILURE_ID=F121
NEXT_GATE=A13_G7_PHYSICAL_ONLY_SAME_FIRMWARE_TOKEN
RELEASE_MERGE_PR_PUBLICATION=NO
```

Tras el primer timeout del ensayo físico G7, el operador
desconectó/reconectó el JWPLC. La TFT volvió a actualizar RTC.
El puerto COM4, abierto en modo pasivo DTR=RTS=False,
recibió 29 veces `G7_FRAM_READY` con el token exacto,
sin señales de panic ni reboot. Esto confirma
`setup()`/`loop()` tras el arranque frío, pero **no explica**
el bloqueo transitorio posterior a la subida.

Se modificó el host serial para abrir COM4 con DTR/RTS desactivados,
igual que la captura pasiva exitosa, y se versionó una
reanudación sólo física del sketch ya cargado, sin reconstrucción
ni flash. El ensayo que escribe FRAM aún NO se ha ejecutado;
no hay evidencia de bytes alterados por G7.
El resultado `fram_restored=False` del intento anterior significa
**sin validación porque no se envió G7_FRAM_RUN**, no un
fallo demostrado de restauración.

---


## G7 — F120: upload PASS, READY_TIMEOUT y TFT detenida; UART pasiva pendiente (2026-10-11)

```text
ALPHA13_STATUS=IN_PROGRESS
G1_G6=CLOSED_PASS
G7_SOURCE_ARCHIVE_BUILD=PASS
G7_CANDIDATE_LINK_AND_REGRESSIONS=PASS
G7_PHYSICAL_BUILD=PASS
G7_UPLOAD=PASS
G7_FIRMWARE_SHA256=30cb8c596d212b6209e3fb179c1b9f216588c2fc83225834dfb2bce7088c663a
G7_FRAM_SERIAL=REVIEW_READY_TIMEOUT
G7_TFT_CLOCK=STOPPED_AT_16_46_54_OPERATOR_OBSERVATION
G7_FRAM_TEST_COMMAND=NOT_SENT
G7_FRAM_RESTORE_VERIFIED=NO_NOT_RUN
G7_PRODUCT_ADOPTION=NO
G7_GIT_CLOSE=NO
G7_PHYSICAL_CAUSE=UNKNOWN
F120=POST_UPLOAD_READY_TIMEOUT
F120_PASSIVE_SERIAL_DIAG=VERSIONED_NOT_EXECUTED
NEXT_FAILURE_ID=F121
NEXT_GATE=A13_G7_UART_READONLY_DIAGNOSIS
DO_NOT_REFLASH_OR_RESET_BEFORE_SERIAL_EVIDENCE=YES
RELEASE_MERGE_PR_PUBLICATION=NO
```

No repetir G7 ni declarar CLOSED_PASS. Primero captar pasivamente
UART de COM4 a 115200, sin transmitir `G7_FRAM_RUN`, sin reset
del ESP32 y sin sobrescribir firmware/NVS. El core sí incluye un
`jwplcSystemTask` para pantalla y RTC independiente del sketch.
La coincidencia reloj detenido + READY_TIMEOUT merece diagnóstico de
arranque, serial o tareas, sin atribución causal prematura.
Documento detallado: `A13_G7_POST_UPLOAD_READY_TIMEOUT_20261011.md`.

---


## G7 — F119 enlace FRAM verificado; reanudación preparada (2026-10-11)

```text
ALPHA13_STATUS=IN_PROGRESS
G1_G6=CLOSED_PASS
G7_R2_SOURCE=PASS
G7_R2_ARCHIVE=PASS
G7_R2_ARCHIVE_SHA256=477ba0ccc3b20e638eccd7dda7d22b209d1ce73003df34917dc9ea7d476d83dd
G7_R2_CANDIDATE_COMPILE=PASS
G7_R2_LINK_FLAGS=-L_CANDIDATE_AND_-lJW_FRAM
G7_R2_REVIEW_REASON=HARNESS_ARCHIVE_LITERAL_MISSING
G7_R2_PRODUCT_MUTATION=NO
G7_R2_FIRMWARE_UPLOAD=NO
G7_R2_FRAM_WRITE=NO
G7_R3_REUSE_FROM_RUN=g7_20261011_162621_6ca7668e
G7_R3_RUNNER=VERSIONED_NOT_EXECUTED
G7_R3_START_PHASE=CANDIDATE_LINK
G7_R3_GIT_AUTO_CLOSE=ONLY_AFTER_ALL_PASS
NEXT_FAILURE_ID=F120
NEXT_GATE=A13_G7_RESUME_CANDIDATE_ARCHIVE
RELEASE_MERGE_PR_PUBLICATION=NO
```

El linker recibió `-lJW_FRAM` y el directorio
`-L` de `TEMP/candidate/JW_FRAM/src/esp32`; el problema fue
exigir que apareciera el literal `libJW_FRAM.a`.
El harness ahora valida ambas opciones, existencia y SHA del
archive exacto. La nueva autoprueba ejecuta casos negativos reales.
La reanudación reutiliza la fuente y archive generados, con
manifiesto/hashes del intento detenido: no prueba aún G7 en hardware.
El respaldo NVS permanece sin usar porque la FRAM no fue escrita.

---


## G7 — F118 harness corregido; prueba física aún pendiente (2026-10-11)

```text
ALPHA13_STATUS=IN_PROGRESS
G1_G6=CLOSED_PASS
G7_R1_PREFLIGHT=PASS
G7_R1_F112=PASS
G7_R1_CONTRACT_SELFTEST=REVIEW_HARNESS_WRONG_LABEL
G7_R1_SOURCE_BUILD=NOT_EXECUTED
G7_R1_FRAM_UPLOAD=NOT_EXECUTED
G7_R1_FRAM_WRITE=NONE
G7_R1_PRODUCT_ADOPTION=NO
G7_R1_GIT_COMMITS=NO
G7_F118_FIX=VERSIONED_NOT_RETESTED
NEXT_FAILURE_ID=F119
NEXT_GATE=A13_G7_FRAM_INTEGRATED_RETRY_AFTER_HARNESS_FIX
RELEASE_MERGE_PR_PUBLICATION=NO
```

F118 se debió a buscar una cadena ficticia
`regression(cli,FRAM,"PRODUCT")` mientras el runner contiene
`regression(cli,FRAM,"PRODUCT_REGRESSIONS")`.
Se corrigió el autocontrato y se hicieron idempotentes los
directorios de trabajo reutilizados; la clasificación de fallos
de la autoprueba ahora es HARNESS. No declarar G7 PASS por
la revisión: requiere nueva ejecución en el banco.

---


## G7 — candidato e integración listos; pendiente de ensayo en PC (2026-10-11)

```text
ALPHA13_STATUS=IN_PROGRESS
G1_G6=CLOSED_PASS
G7=FRAM_PROBE_SOURCE_ARCHIVE_INTEGRATED_READY
G7_OPERATOR_FRAM_WRITE_CONSENT=YES
G7_SAFE_WINDOW=0x1F00..0x1F7F
G7_WINDOW_BACKUP=NVS_RESTORE_WITH_BYTE_COMPARE
G7_CANDIDATE_NULL_GUARDS=VERSIONED_NOT_APPLIED
G7_CANDIDATE_BLOCK_CHECKSUM_PRESERVE_OUTPUT=VERSIONED_NOT_APPLIED
G7_SOURCE_TO_OBJECT_ARCHIVE_LINK=RUNNER_PREPARED_NOT_EXECUTED
G7_APP_PHYSICAL=NOT_EXECUTED
G7_PRODUCT_ADOPTION=NO
G7_AUTO_GIT_CLOSE=ONLY_AFTER_PASS
G7_NEXT_FAILURE_ID=F118
G7_CANONICAL_009_013_MAPPING=REVIEW_PENDING
NEXT_GATE=A13_G7_FRAM_INTEGRATED
RELEASE_MERGE_PR_PUBLICATION=NO
```

El operador confirmó que la FRAM física puede usarse para ensayos.
Se prepararon candidato, prueba física con respaldo persistente
en NVS, archivo `.a` generado desde objeto y ejecutor integrado.
La ventana 0x1F00..0x1F7F se lee, respalda en NVS antes de modificar y
se restaura/verifica tras la prueba. Una marca `pending` permite
recuperación si ocurre reinicio; si la recuperación falla, se detiene
G7 y queda el respaldo NVS para análisis.

Sin cambios productivos hasta superar reconstrucción, enlace, carga,
serial y restauración; no se solicita `AUTORIZO_COMMIT_G7`.
Requiere `BANCO_FRAM` únicamente por seguridad de actuadores
antes de reemplazar el sketch. G7 no equivale a cierre semántico
de A13-009/013 sin disponer del hallazgo canónico original.

---


## Estado vigente — G7 FRAM preflight preparado, producto intacto (2026-10-11)

```text
BRANCH=v2.1.0-alpha.13/feature/cleanup-robustness
ALPHA13_STATUS=IN_PROGRESS
G1_DNS=CLOSED_PASS
G2_TCA_STARTUP=CLOSED_PASS
TFT_CLOSURE=CLOSED_PASS
G3_TCA_RMW=CLOSED_PASS
G4_TFT_BATCH=CLOSED_PASS
G5_TCP=CLOSED_PASS
G6_UDP=CLOSED_PASS
G6_PRODUCT_COMMIT=575144f49f1a392615ee3e0c1b9e137202ff532d
G6_DOCS_COMMIT=17c37355e851bcc4306b8dd737eedfa45eef05b2
G7=A13-009_PLUS_A13-013_FRAM
G7_PREP_COMMIT=d90401c79dd69b9d54a0844ac43038bcf3a123a9
G7_PREFLIGHT=VERSIONED_NOT_EXECUTED
G7_ARCHIVE_MODE=PRECOMPILED_FULL
G7_PRODUCT_CHANGED=NO
G7_PHYSICAL=NOT_EXECUTED
G7_FRAM_DATA_POLICY=PRESERVE_UNTIL_SAFE_TEST_WINDOW_IS_DEFINED
G7_SOURCE_ARCHIVE_REBUILD=PENDING
NEXT_GATE=A13_G7_FRAM_PREFLIGHT_AND_CANDIDATE_QUALIFICATION
NEXT_FAILURE_ID=F118
OPENPLC=OUT_OF_SCOPE
HMI_DESIGNER=OUT_OF_SCOPE
TFT_NEW_FEATURES=OUT_OF_SCOPE
RELEASE_MERGE_PR_PUBLICATION=NO
```

G6 ha cerrado con prueba física, regresiones y push sin fuerza;
no hay que repetirlo. G7 requiere especial tratamiento por
`JW_FRAM` `precompiled=full` y `libJW_FRAM.a`: el siguiente
cambio productivo debe incluir build fuente→objeto→archive→link
y verificar un espacio seguro antes de cualquier escritura FRAM.
Se versionó el plan
`A13_G7_FRAM_PREPARATION_PLAN_20261011.md` y un preflight que
solo compila, sin upload ni escritura FRAM. A13-009/A13-013 no
se declaran corregidos sin contrastar alcance con auditoría.

---


## Estado vigente — G6 cerrado, G7 siguiente (2026-10-11)

ALPHA13_STATUS=IN_PROGRESS
G1=CLOSED_PASS
G2=CLOSED_PASS
TFT_CLOSURE=CLOSED_PASS
G3=CLOSED_PASS
G4=CLOSED_PASS
G5=CLOSED_PASS
G6=CLOSED_PASS
G6_RUN_ID=20261011_150515_c7ab9c7e
G6_PHYSICAL=PASS_UDP_2048_TX_256_PARTIAL_16_RECOVERED_REOPEN
G6_REGRESSIONS=3_OVERLAY_PASS_3_PRODUCT_PASS
G6_PRODUCT_FILES=2
G6_A13_008=REVIEW_MAPPING_PENDING
NEXT_GATE=A13-G7_FRAM_A13_009_A13_013
NEXT_FAILURE_ID=F115
OPENPLC=OUT_OF_SCOPE
HMI_DESIGNER=OUT_OF_SCOPE
TFT_NEW_FEATURES=OUT_OF_SCOPE
RELEASE_MERGE_PR_PUBLICATION=NO

Cierre: A13_G6_CLOSURE_20261011.md
Checklist: A13_G6_CLOSURE_CHECKLIST_20261011.md

---


## G6 — MTU confirmado y reanudación sin upload preparada (2026-10-11)

```text
ALPHA13_STATUS=IN_PROGRESS
G1=CLOSED_PASS
G2=CLOSED_PASS
G3=CLOSED_PASS
G4=CLOSED_PASS
G5=CLOSED_PASS
G6_NETWORK_R2=REVIEW_EXPECTED_ONE_DATAGRAM
G6_UPLOAD_R2=PASS_ARDUINO_CLI_1_5_1
G6_TX_DIAG=PASS_SPLIT_1472_576_CONCAT_EXACT
G6_MTUSTREAM_SEMANTICS=W5500_2_UDP_MESSAGES
G6_PRODUCT_MUTATION=NO
G6_GIT_CLOSURE=NO
G6_RESUME_RUNNER=VERSIONED_NOT_EXECUTED
G6_RESUME_UPLOAD=NOT_REQUIRED
F117=CONFIRMED_HARNESS_FIX_AWAITING_FULL_RETEST
NEXT_FAILURE_ID=F118
NEXT_GATE=A13_G6_RESUME_UPLOADED
RELEASE_MERGE_PR_PUBLICATION=NO
```

Los 2048 bytes se recibieron en dos datagramas UDP distintos,
1472+576, ambos desde la IP del JWPLC y con bytes concatenados
exactos. WIZnet documenta la división automática por MTU.
La prueba PC admitía erróneamente sólo un paquete de 2048 bytes.
Reanudación preparada reutilizando el mismo firmware/token,
sin nueva compilación de overlay ni upload.

---


## G6 — F117 diagnóstico UDP TX tras upload PASS (2026-10-11)

```text
ALPHA13_STATUS=IN_PROGRESS
G1=CLOSED_PASS
G2=CLOSED_PASS
G3=CLOSED_PASS
G4=CLOSED_PASS
G5=CLOSED_PASS
G6_R2_CLI=ARDUINO_CLI_OFFICIAL_1_5_1_PASS
G6_R2_COM4=PASS
G6_R2_BUILD=PASS
G6_R2_UPLOAD=PASS
G6_R2_PC_UDP=REVIEW_TX_PAYLOAD_MISMATCH
G6_DIAGNOSTIC=PREPARED_NO_REFLASH
G6_PRODUCT_MUTATION=NO
G6_COMMITS=NO
F115=PENDING_PHYSICAL_CLASSIFICATION
F116=RESOLVED_LAUNCHER_FIX
F117=OPEN_DIAGNOSTIC_PENDING
NEXT_FAILURE_ID=F118
NEXT_GATE=A13_G6_UDP_TX_DIAGNOSE
RELEASE_MERGE_PR_PUBLICATION=NO
```

El script de recepción G6 R2 no discrimina diferencia de origen,
longitud y contenido del datagrama. El firmware de prueba quedó
cargado correctamente y puede reutilizarse para recopilar evidencia
antes de cualquier cambio de producto o compilación nueva.

---


## G6 — F116: falsa ausencia del core en preflight PowerShell (2026-10-11)

```text
ALPHA13_STATUS=IN_PROGRESS
G1=CLOSED_PASS
G2=CLOSED_PASS
G3=CLOSED_PASS
G4=CLOSED_PASS
G5=CLOSED_PASS
G6_R1=REVIEW_COM4_OPEN_FAILED
G6_R2=NOT_STARTED_POWERSHELL_CORE_LIST_FALSE_NEGATIVE
G6_PRODUCT_MUTATION=NO
G6_GIT_COMMIT=NO
G6_PHYSICAL=NOT_EXECUTED
ARDUINO_CLI_OFFICIAL_1_5_1=USER_VERIFIED
JWPLC_LOCAL_CORE_2_1_0_DEV=USER_VERIFIED
COM4_RECHECK=NOT_EXECUTED
F115=OPEN_AWAITING_REAL_ENVIRONMENT_VALIDATION
F116=HARNESS_FALSE_NEGATIVE_FIX_PREPARED
NEXT_FAILURE_ID=F117
NEXT_GATE=A13_G6_COM4_DIAGNOSIS_AND_RETEST
RELEASE_MERGE_PR_PUBLICATION=NO
```

PowerShell aplicó `-notmatch` sobre las múltiples líneas de
`arduino-cli core list`, interpretando erróneamente que el core no
existía. No hubo compilación, upload, mutación de producto ni commit
en este intento. El CLI oficial y el core están reconocidos según el
diagnóstico anterior del operador; se retira el chequeo PowerShell
redundante. Pendiente confirmar COM4 antes de reintentar G6.

---


## G6 — CLI y COM4 en revisión (2026-10-11)

```text
ALPHA13_STATUS=IN_PROGRESS
G1=CLOSED_PASS
G2=CLOSED_PASS
G3=CLOSED_PASS
G4=CLOSED_PASS
G5=CLOSED_PASS
G6_R1_PREFLIGHT=PASS
G6_R1_SELFTEST=PASS
G6_R1_CANDIDATE_BUILD=PASS_3_OF_3
G6_R1_PHYSICAL_BUILD=PASS
G6_R1_UPLOAD=REVIEW_COM4_OPEN_PERMISSION_ERROR_13
G6_R1_CLI=ARDUINO_PLC_IDE_TOOLS_UNEXPECTED
G6_R1_PRODUCT_MUTATION=NO
G6_R1_GIT_COMMIT=NO
G6_R1_TCP_UDP_PHYSICAL=NOT_RUN
G6_CLI_POLICY=EXPLICIT_ARDUINO_IDE_CLI_REQUIRED
G6_RETEST=NOT_EXECUTED
F115=OPEN_CODE_FIX_VERSIONED
NEXT_FAILURE_ID=F116
NEXT_GATE=A13_G6_ENVIRONMENT_DIAGNOSE_THEN_RESUME
RELEASE_MERGE_PR_PUBLICATION=NO
```

El fallback que seleccionó `Arduino PLC IDE Tools` fue un error del
harness; la causa de `PermissionError(13)` de COM4 no está todavía
establecida. El runner ahora exige CLI por ruta absoluta, no acepta
aquel fallback y registra versión y SHA. No reejecutar G6 antes de
seleccionar el CLI correcto y diagnosticar el puerto. Mantener fuentes
productivas y G1–G5 intactos.

---


## Estado vigente — G6 listo para ejecutar gate integrado (2026-10-11)

```text
ALPHA13_STATUS=IN_PROGRESS
BRANCH=v2.1.0-alpha.13/feature/cleanup-robustness
G1_DNS=CLOSED_PASS
G2_TCA_STARTUP=CLOSED_PASS
TFT_CLOSURE=CLOSED_PASS
G3_TCA_RMW_SHADOW=CLOSED_PASS
G4_TFT_BATCH_OWNERSHIP=CLOSED_PASS
G5_TCP_CORRECTNESS=CLOSED_PASS
G6=A13-007_UDP_DATAGRAM_BOUNDS_AND_A13-008_REVIEW
G6_SOURCE_CANDIDATES=VERSIONED_OUTSIDE_PRODUCT
G6_RUNNER=VERSIONED_NOT_EXECUTED
G6_CONTRACT_SELFTEST=VERSIONED_NOT_EXECUTED
G6_HARDWARE=NOT_EXECUTED
G6_AUTO_CLOSE_GIT=ENABLED_AFTER_ALL_GATES_PASS
G6_BENCH_APPROVAL=REQUIRED_BEFORE_UPLOAD
G6_PRODUCT_FILES=NOT_MODIFIED
NEXT_GATE=A13_G6_INTEGRATED
NEXT_FAILURE_ID=F115
RELEASE_MERGE_PR_PUBLICATION=NO
OPENPLC=OUT_OF_SCOPE
HMI_DESIGNER=OUT_OF_SCOPE
TFT_NEW_FEATURES=OUT_OF_SCOPE
```

El ejecutor G6 incluye selftest de Python/Git (F112) y contrato
generador↔prueba↔cierre, compilación source-first, validación física
UDP PC↔JWPLC, rollback previo al primer commit, y cierre Git automático
sin la cadena `AUTORIZO_COMMIT_G6`. Solo solicita `BANCO_UDP` antes del
upload físico. Los paths productivos permanecen intactos hasta PASS.

**Esta preparación todavía no demuestra PASS de Arduino CLI ni de la
prueba física.** El operador debe ejecutar el hito en su entorno;
cualquier fallo queda REVIEW y no se reintenta ciegamente.

---


## Estado vigente — G6 preparación y cierre automatizado (2026-10-11)

```text
ALPHA13_STATUS=IN_PROGRESS
G1_DNS=CLOSED_PASS
G2_TCA_STARTUP=CLOSED_PASS
TFT_CLOSURE=CLOSED_PASS
G3=CLOSED_PASS
G4=CLOSED_PASS
G5=CLOSED_PASS
G5_CLOSURE_COMMIT=cc965e89311c7a54465818bf8fdda7cca1b59ff7
G6=A13-007_DATAGRAM_ROBUSTNESS_PLUS_A13-008_REVIEW
G6_PREPARATION=IN_PROGRESS
G6_PRODUCT_MUTATION=NOT_EXECUTED
G6_PHYSICAL_GATE=NOT_EXECUTED
G6_AUTO_GIT_CLOSURE=ENABLED_ONLY_AFTER_ALL_GATES_PASS
G6_FLASH_PERMISSION=BENCH_SAFETY_ONLY
G6_RELEASE_PUBLICATION=NO
NEXT_GATE=A13_G6_PREPARATION_AND_INTEGRATED_TEST
NEXT_FAILURE_ID=F115
OPENPLC=OUT_OF_SCOPE
HMI_DESIGNER=OUT_OF_SCOPE
TFT_NEW_FEATURES=OUT_OF_SCOPE
```

G5 ya está cerrado. Por petición expresa del operador se retira la
solicitud textual `AUTORIZO_COMMIT_GN`: el cierre Git en la rama
Alpha13 pasa a ser automático **solo con preflight, evidencia, hashes,
contratos y scope íntegramente PASS**. Conserva confirmación de banco
para cualquier upload físico. La política canónica está en
`docs/JWPLC_COLLABORATION_WORKFLOW.md`, sección 12.

---


## Estado vigente — G5 cerrado y G6 siguiente (2026-10-11)

El bloque es el estado vigente; los antiguos bloques G4/G5 preparado
permanecen como histórico.

ALPHA13_STATUS=IN_PROGRESS
G1_DNS=CLOSED_PASS
G2_TCA_STARTUP=CLOSED_PASS
TFT_CLOSURE=CLOSED_PASS
G3=CLOSED_PASS
G4=CLOSED_PASS
G5=CLOSED_PASS
G5_RUN_ID=20261010_175509_dbd543a2
G5_PHYSICAL=PASS_TCP_5000_TX_128_RX_64_SERVER
G5_REGRESSIONS=3_OF_3_OVERLAY_AND_3_OF_3_PRODUCT
G5_PRODUCT_FILES=2
G5_PRODUCT_COMMIT=c45b77f512fd5829da33c491fd512c0a810846b9
G5_TOOLING_COMMIT=dc2c9c59d68fe3b79e901a9103267951d065dd76
G5_SYNC_MERGE_COMMIT=699d8dc8a4d3281cc2fb144cb3fa9c337d9d712b
F112=CLOSED_PREVENTION_REGISTERED
F113=CLOSED_HARNESS_REMEDIATED
F114=CLOSED_HARNESS_REMEDIATED
NEXT_FAILURE_ID=F115
NEXT_GATE=A13-G6_DATAGRAM_ROBUSTNESS_A13_007_A13_008_REVIEW
OPENPLC=OUT_OF_SCOPE
HMI_DESIGNER=OUT_OF_SCOPE
TFT_NEW_FEATURES=OUT_OF_SCOPE

Cierre: A13_G5_CLOSURE_20261011.md.
Checklist: A13_G5_CLOSURE_CHECKLIST_20261011.md.
Alpha13 todavía no tiene merge release, PR ni PreRelease.

---


## G5 — F114 reparado, cierre Git todavía pendiente (2026-10-11)

```text
G1=CLOSED_PASS
G2=CLOSED_PASS
G3=CLOSED_PASS
G4=CLOSED_PASS
G5_PHYSICAL=PASS
G5_REGRESSIONS=PASS_3_OVERLAY_3_PRODUCT
G5_GIT_CLOSURE=PENDING
F113=PENDING_CLOSURE_RETEST
F114=FIX_VERSIONED_PENDING_RETEST
NEXT_FAILURE_ID=F115
CMAKE_BUILD_IGNORED=/build/
NEXT_GATE=A13_G5_GIT_CLOSURE
RELEASE_MERGE_PR_PUBLICATION=NO
```

El manifiesto G5 usa claves de producto; el finalizador ahora refleja
exactamente ese contrato. No repetir gate ni reflashear el hardware.

---

## Seguimiento G5 — prueba PASS, cierre Git F113 pendiente (2026-10-11)

```text
ALPHA13_STATUS=IN_PROGRESS
G1_DNS=CLOSED_PASS
G2_TCA_STARTUP=CLOSED_PASS
G3=CLOSED_PASS
G4=CLOSED_PASS
G5_PHYSICAL=PASS
G5_REGRESSIONS=3_OVERLAY_PASS_3_PRODUCT_PASS
G5_PRODUCT_FILES=2_LOCAL_MODIFIED_UNCOMMITTED
G5_FINALIZER_R1=REVIEW_STOP_UNTRACKED_FILES
G5_FINALIZER_R2=VERSIONED_NOT_EXECUTED
G5_CLOSURE=PENDING
F113=OPEN_FIX_VERSIONED_PENDING_RETEST
NEXT_FAILURE_ID=F114
NEXT_GATE=A13_G5_GIT_CLOSURE
RELEASE_MERGE_PR_PUBLICATION=NO
```

Las rutas untracked de la PC aún no fueron identificadas en la consola.
El finalizador corregido las conservará sin tocar; únicamente incorporará
los dos archivos Ethernet validados y la documentación autorizada.
No repetir las pruebas físicas de G5.

---


## Estado vigente — G5 preparado para validación integrada (2026-10-10)

**Prevalece sobre el bloque histórico previo en relación con G5.**

```text
BRANCH=v2.1.0-alpha.13/feature/cleanup-robustness
ALPHA13_STATUS=IN_PROGRESS
G1_DNS=CLOSED_PASS
G2_TCA_STARTUP=CLOSED_PASS
TFT_CLOSURE=CLOSED_PASS
G3_TCA_RMW_SHADOW=CLOSED_PASS
G4_TFT_BATCH_TASK_OWNERSHIP=CLOSED_PASS
F112=CLOSED_PREVENTION_REGISTERED
G5=A13-005_PLUS_A13-006_TCP_CORRECTNESS
G5_CANDIDATE=VERSIONED_OUTSIDE_PRODUCT
G5_PRODUCT_SOURCE_MUTATION=NOT_EXECUTED
G5_TCP_PHYSICAL=NOT_EXECUTED
G5_NORMAL_SOURCE_REGRESSIONS=NOT_EXECUTED
G5_INTEGRATED_TOOLING=VERSIONED_READY_TO_RUN
G5_PRODUCT_COMMIT=NO
NEXT_GATE=A13-G5-INTEGRATED-TCP-CORRECTNESS
NEXT_FAILURE_ID=F113
OPENPLC=OUT_OF_SCOPE
HMI_DESIGNER=OUT_OF_SCOPE
TFT_NEW_FEATURES=OUT_OF_SCOPE
```

Las fuentes de `JWPLC_Ethernet` permiten observar tres contratos TCP en riesgo:
`EthernetClient::write(size>SSIZE)`, `EthernetClient::read(size_t)`
y `EthernetServer::write` cuando no hay clientes o el envío falla.
**No se presenta como equivalencia verificada de cada ID A13-005/A13-006
con el informe canónico no disponible en la rama;** los cambios quedan
acotados al código comprobado y la homologación final de IDs debe
contrastarse antes del cierre G5.

El <u>único</u> gate integrado G5 usa overlay temporal de fuentes,
compilación Arduino CLI desde fuente, prueba de red con token serial y
LAN real, regresiones de producto y rollback; **sin commits automáticos**.
La fase física requiere PC y JWPLC en la misma red y equipo desconectado
de actuadores. Ver [A13_G5_INTEGRATED_PLAN_20261010.md](A13_G5_INTEGRATED_PLAN_20261010.md).

---


## Prevención F112 — cierre G4 y portabilidad Python/Git (2026-10-10)

```text
ALPHA13_STATUS=IN_PROGRESS
G1_DNS=CLOSED_PASS
G2_TCA_STARTUP=CLOSED_PASS
TFT_CLOSURE=CLOSED_PASS
G3=CLOSED_PASS
G4=CLOSED_PASS
G4_CLOSURE_COMMIT=bd1581035af09eff47a08cae959312f8eae1d06f
F110=CLOSED
F111=CLOSED
F112=CLOSED_PREVENTION_REGISTERED
F112_SCOPE=POWERSHELL_PYTHON_GIT_FINALIZER_PORTABILITY
F112_PORTABILITY_SELFTEST=VERSIONED
NEXT_GATE=A13-G5_TCP_CORRECTNESS_A13_005_A13_006
NEXT_FAILURE_ID=F113
OPENPLC=OUT_OF_SCOPE
HMI_DESIGNER=OUT_OF_SCOPE
TFT_NEW_FEATURES=OUT_OF_SCOPE
```

F112 documenta bloqueos de cierre debidos a invocación de Python,
estado HEAD local asumido y comparación byte a byte de textos Git
en Windows. La compilación, prueba física y cierre G4 permanecen PASS;
no hubo cambio productivo adicional por registrar esta prevención.
Consultar `A13_TOOLING_FAILURES_AND_PREVENTION_20261006.md` y
la sección 9 de `docs/JWPLC_COLLABORATION_WORKFLOW.md` antes de
preparar finalizadores G5+.

---


## Estado vigente — G4 cerrado (2026-10-10)

Este bloque es canonico y prevalece sobre las entradas anteriores.

ALPHA13_STATUS=IN_PROGRESS
G1_DNS=CLOSED_PASS
G2_TCA_STARTUP=CLOSED_PASS
TFT_CLOSURE=CLOSED_PASS
G3=CLOSED_PASS
G4=CLOSED_PASS
G4_RUN_ID=20261010_141745_907ce6dc
G4_PHYSICAL=50_OF_50_PASS
G4_ERRORS=0
G4_VISUAL=PASS_OPERATOR_REPORTED
G4_REGRESSIONS=4_OF_4_PASS
G4_TFT_ARCHIVE_SHA256=4c606ba66e29c1337d42450a29fd09fe76c6988f72aa51d3882b81db59f82ecc
G4_PRODUCT_COMMIT=46a695756ecae205f585e59bf8e129493fa51d91
G4_SYNC_MERGE_COMMIT=dae9e674b3bc1212210fe9ca780b3000c72cbab2
F111=CLOSED_HARNESS_REMEDIATED
NEXT_GATE=A13-G5_TCP_CORRECTNESS_A13_005_A13_006
NEXT_FAILURE_ID=F112
OPENPLC=OUT_OF_SCOPE
HMI_DESIGNER=OUT_OF_SCOPE
TFT_NEW_FEATURES=OUT_OF_SCOPE

Cierre: A13_G4_CLOSURE_20261010.md. Checklist: A13_G4_CLOSURE_CHECKLIST_20261010.md.
Sin publicacion ni merge a release.

---


## Estado vigente — G4 R2: F111 identificado/corregido, validación pendiente (2026-10-10)

```text
BRANCH=v2.1.0-alpha.13/feature/cleanup-robustness
ALPHA13_STATUS=IN_PROGRESS
G1_DNS=CLOSED_PASS
G2_TCA_STARTUP=CLOSED_PASS
TFT_CLOSURE=CLOSED_PASS
G3=CLOSED_PASS
G4_R1=REVIEW_HARNESS_PIN
G4_R2=REVIEW_HARNESS_PIN
G4_R2_LOCAL_BACKEND_MATCHES_OFFICIAL=YES
G4_R2_UPSTREAM_BACKEND_MATCHES_OFFICIAL=YES
G4_F111_ROOT_CAUSE=TRUNCATED_PIN_LITERAL
G4_PIN_FIX_VERSIONED=YES
G4_HISTORICAL_SHA_GUARD_ADDED=YES
G4_PRODUCT_CHANGED=NO
G4_PHYSICAL_UPLOAD=NOT_EXECUTED
G4_RETEST=NOT_EXECUTED
F111=OPEN_PENDING_G4_RETEST
NEXT_GATE=A13_G4_INTEGRATED_RETEST
NEXT_FAILURE_ID=F112
OPENPLC=OUT_OF_SCOPE
HMI_DESIGNER=OUT_OF_SCOPE
TFT_NEW_FEATURES=OUT_OF_SCOPE
```

Se comprobó que la cabecera de init instalada coincide por bytes SHA-256 con la fuente oficial del tag TFT_eSPI `V2.5.43` y con el pin del gate TFT-CLOSURE. El fallo era de nuestra constante G4, corregida en tooling, no del usuario. No reinstalar ni modificar `TFT_eSPI`. Al completar el gate integrado, cerrar G4 y F111 según evidencia.

---


## Estado vigente — G4 R1 bloqueado por diferencia de backend local (2026-10-10)

```text
ALPHA13_STATUS=IN_PROGRESS
G1_DNS=CLOSED_PASS
G2_TCA_STARTUP=CLOSED_PASS
TFT_CLOSURE=CLOSED_PASS
G3=CLOSED_PASS
G4_R1_PREFLIGHT=PASS
G4_R1_BACKEND_RECIPE=REVIEW_ENVIRONMENT_SHA_MISMATCH
G4_R1_LOCAL_BACKEND_INIT_SHA256=NOT_YET_OBSERVED
G4_R1_SOURCE_REBUILD=NOT_EXECUTED
G4_R1_PHYSICAL_UPLOAD=NOT_EXECUTED
G4_R1_PRODUCT_ADOPTION=NOT_EXECUTED
G4_BACKEND_RECOVERY=VERSIONED_READY_TO_RETEST
G4_PRODUCT_COMMIT=NO
NEXT_GATE=A13_G4_INTEGRATED_RETEST
NEXT_FAILURE_ID=F111
OPENPLC=OUT_OF_SCOPE
HMI_DESIGNER=OUT_OF_SCOPE
TFT_NEW_FEATURES=OUT_OF_SCOPE
```

La versión actual del gate compara variantes SHA fijadas y puede recuperar
la fuente original desde TFT_eSPI V2.5.43 oficial **solo en TEMP**,
sin sobrescribir archivos de usuario. No considerar G4 cerrado hasta
obtener compilaciones, serial físico, confirmación visual y auditoría de producto.

---

## Estado vigente — G4 A13-003 preparado (2026-10-10)

**Hito integrado en preparación:** esta entrada prevalece sobre el `NEXT_GATE` histórico de G3. El candidato está versionado **solo bajo tooling**, no se han cambiado fuentes ni binarios de producto.

```text
BRANCH=v2.1.0-alpha.13/feature/cleanup-robustness
ALPHA13_STATUS=IN_PROGRESS
G1_DNS=CLOSED_PASS
G2_TCA_STARTUP=CLOSED_PASS
TFT_CLOSURE=CLOSED_PASS
G3_TCA_RMW_SHADOW=CLOSED_PASS
G4=A13-003_TFT_BATCH_TASK_OWNERSHIP
G4_PRODUCT_CHANGE=NOT_EXECUTED
G4_ARCHIVE_REBUILD=NOT_EXECUTED
G4_PHYSICAL_TEST=NOT_EXECUTED
G4_TOOLING=VERSIONED_READY_TO_RUN
NEXT_GATE=A13_G4_INTEGRATED
NEXT_FAILURE_ID=F111
OPENPLC=OUT_OF_SCOPE
HMI_DESIGNER=OUT_OF_SCOPE
TFT_NEW_FEATURES=OUT_OF_SCOPE
```

Criterios y ejecutor: [A13_G4_INTEGRATED_PLAN_20261010.md](A13_G4_INTEGRATED_PLAN_20261010.md). Mantener G3 core.a y TFT startup sin cambios. El gate G4 debe reconstruir el archive TFT desde las fuentes candidatas y backend 2.5.43 aislado, comprobar enlace normal, ejecutar test con tareas FreeRTOS y requerir observación humana. No marcar G4 PASS hasta obtener evidencia del operador.

---


## Estado vigente — G3 A13-004 cerrado (2026-10-10)

**Prevalece sobre los bloques anteriores de G3.**

G1_DNS=CLOSED_PASS
G2_TCA_STARTUP=CLOSED_PASS
TFT_CLOSURE=CLOSED_PASS
G3_P0=PASS_BASELINE_RACE_REPRODUCED
G3_SOURCE_CORE_REBUILD=PASS
G3_NORMAL_PRECOMPILED_LINK=PASS
G3_REGRESSIONS=3_OF_3_PASS
G3_PHYSICAL=150_OF_150_PASS
G3_SERIAL_ERRORS=0
G3_FINAL_OUTPUT=0x00
G3=CLOSED_PASS
G3_RUN_ID=20261010_091949_a04d2223
G3_CORE_SHA256=a1a985f64c22838a1987d4280b8c1dc431a42d5789c6692cbfa36dfb6252246d
G3_PRODUCT_COMMIT=8b8b9a7b491feb92d8fc91f06ae75234771cde70
G3_SYNC_NOT_REQUIRED=NOT_REQUIRED
F110=CLOSED_HARNESS_REMEDIATED
NEXT_GATE=A13-G4_TFT_BATCH_TASK_OWNERSHIP
NEXT_FAILURE_ID=F111
OPENPLC=OUT_OF_SCOPE
HMI_DESIGNER=OUT_OF_SCOPE
TFT_NEW_FEATURES=OUT_OF_SCOPE

Documentos: [cierre G3](A13_G3_CLOSURE_20261010.md) y
[checklist G3](A13_G3_CLOSURE_CHECKLIST_20261010.md).
Sin publicación ni merge a release.

---

## Estado vigente — G3 integrado: candidata corregida, prueba pendiente (2026-10-10)

**Prevalece sobre los bloques anteriores de G3.** El intento inicial falló por siete firmas duplicadas generadas en las fuentes candidatas; no por TCA ni por el toolchain. Restauración local validada.

```text
ALPHA13_STATUS=IN_PROGRESS
BRANCH=v2.1.0-alpha.13/feature/cleanup-robustness
G1_DNS=CLOSED_PASS
G2_TCA_STARTUP=CLOSED_PASS
TFT_CLOSURE=CLOSED_PASS
G3_P0=PASS_BASELINE_RACE_REPRODUCED
G3_ATTEMPT_1=REVIEW_CANDIDATE_SYNTAX
G3_ATTEMPT_1_BUILD_OFFICIAL_CORE=FAIL
G3_ATTEMPT_1_ROOT_CAUSE=DUPLICATED_RETURN_TYPES_IN_GENERATED_CANDIDATE
G3_ATTEMPT_1_PRODUCT_FAILURE=NO_VALID_PRODUCT_CANDIDATE
G3_ATTEMPT_1_PHYSICAL=NOT_EXECUTED
G3_ATTEMPT_1_ROLLBACK=PASS
G3_ATTEMPT_1_ORIGINAL_CORE_SHA256=6f328eeb796091070c8d852a2c0e90f71047d8d2b5786fe3cd5079b2fb6ff983
G3_CANDIDATE_SIGNATURES_FIXED=YES
G3_HARNESS_STATIC_SIGNATURE_GUARD=ADDED
G3_RETEST=NOT_EXECUTED
G3_PRODUCT_COMMIT=NO
NEXT_GATE=A13-G3_INTEGRATED_RETEST
NEXT_FAILURE_ID=F111
OPENPLC=OUT_OF_SCOPE
HMI_DESIGNER=OUT_OF_SCOPE
TFT_NEW_FEATURES=OUT_OF_SCOPE
```

Causa confirmada por `source-Basic.log` compartido por el operador. No repetir P0. Los cambios de candidato y harness se documentan en [A13_G3_INTEGRATED_PLAN_20261010.md](A13_G3_INTEGRATED_PLAN_20261010.md). El siguiente intento solo debe ejecutarse con worktree limpio y con los actuadores desconectados. No dar por aprobada la prueba física hasta obtener evidencia serial y revisión humana.

---

## Estado vigente — G3 integrado preparado (2026-10-10)

**Nuevo estado operativo:** prevalece sobre el bloque anterior "G3-P0 preparado". G1/G2/TFT permanecen cerrados.

```text
BRANCH=v2.1.0-alpha.13/feature/cleanup-robustness
ALPHA13_STATUS=IN_PROGRESS
G1_DNS=CLOSED_PASS
G2_TCA_STARTUP=CLOSED_PASS
TFT_CLOSURE=CLOSED_PASS
G3_P0=PASS_BASELINE_RACE_REPRODUCED
G3_P0_SOURCE_CONTRACT=PASS
G3_P0_PHYSICAL=NOT_EXECUTED
G3_P0_HEAD=c33aff2bad3c3a94e988ae030ceb99ad241c9eef
G3_P0_HARDWARE_INTERLEAVING=NOT_MEASURED
G3_INTEGRATED_TOOLING=VERSIONED_READY_TO_RUN
G3_CANDIDATE_PRODUCT_COMMIT=NOT_EXECUTED
G3_CANDIDATE_SOURCE_ARCHIVE_VALIDATION=NOT_EXECUTED
G3_PHYSICAL=NOT_EXECUTED
NEXT_GATE=A13-G3_INTEGRATED_SOURCE_ARCHIVE_PHYSICAL
NEXT_FAILURE_ID=F111
OPENPLC=OUT_OF_SCOPE
HMI_DESIGNER=OUT_OF_SCOPE
TFT_NEW_FEATURES=OUT_OF_SCOPE
```

El usuario ejecutó `run_a13_g3_p0_tca_rmw_baseline.bat` con `STATUS=PASS_BASELINE_RACE_REPRODUCED`, `SEQUENTIAL_CONTROL=0x03`, `INTERLEAVED_AB_HARDWARE=0x02`, `INTERLEAVED_BA_HARDWARE=0x01`, `SHADOW_FALSE_NOOP_POSSIBLE=YES`, sin modificar producto. La prueba fue un **modelo determinista**, no una medición de hardware. La continuación no requiere más rondas manuales para P1: el gate integrado con candidata reversible, core.a, compilación normal y ensayo físico supervisado está en [A13_G3_INTEGRATED_PLAN_20261010.md](A13_G3_INTEGRATED_PLAN_20261010.md).

**Precaución física:** dos salidas de relé se conmutan; solo equipo con cargas/actuadores desconectados, y confirmación explícita. No usar físicamente si el entorno no está aislado. Producto permanece sin commit hasta pasar el gate.

---


## Estado vigente — G3-P0 preparado (2026-10-10)

**Prevalece para la próxima acción sobre los bloques cronológicos anteriores.** El cierre TFT permanece retenido.

```text
BRANCH=v2.1.0-alpha.13/feature/cleanup-robustness
ALPHA13_STATUS=IN_PROGRESS
G1_DNS=CLOSED_PASS
G2_TCA_STARTUP=CLOSED_PASS
TFT_CLOSURE=CLOSED_PASS
G3=A13-004_TCA_RMW_SHADOW_ATOMICITY
G3_P0_TOOLING=VERSIONED_READY_TO_RUN
G3_P0_RESULT=NOT_EXECUTED
G3_PRODUCT_CHANGE=NO
NEXT_GATE=A13-G3-P0_TCA_RMW_BASELINE
NO_UPLOAD=YES
NO_RECOMPILE_CORE=YES
OPENPLC=OUT_OF_SCOPE
HMI_DESIGNER=OUT_OF_SCOPE
TFT_NEW_FEATURES=OUT_OF_SCOPE
```

Ejecutor: `tools/alpha13/gates/run_a13_g3_p0_tca_rmw_baseline.bat`. Diseño y criterio de evidencia: [A13_G3_P0_TCA_RMW_BASELINE_PLAN_20261010.md](A13_G3_P0_TCA_RMW_BASELINE_PLAN_20261010.md). El resultado no se clasificará como PASS hasta recibir la salida ejecutada; en P0 se modela una intercalación, no se mide concurrencia física. No repetir TFT.

---


## Estado vigente — TFT-CLOSURE cerrado (2026-10-09)

**TFT_CLOSURE_CLOSED_PASS_CURRENT_20261009**. Este bloque prevalece sobre las entradas históricas P2A/P2B y NEXT_GATE=TFT-CLOSURE más abajo, preservadas para trazabilidad.

```text
BRANCH=v2.1.0-alpha.13/feature/cleanup-robustness
ALPHA13_STATUS=IN_PROGRESS
G1_DNS=CLOSED_PASS
G2_TCA_STARTUP=CLOSED_PASS
TFT_PRE6_P2A=CLOSED_PASS
TFT_PRE6_P2B_PHYSICAL=PASS
TFT_CLOSURE=CLOSED_PASS
TFT_VISUAL=PASS_OPERATOR_REPORTED
TFT_VIDEO_REVIEW=NOT_PERFORMED
TFT_REBUILD_FUNCTIONAL=PASS
TFT_REBUILD_BIT_IDENTICAL=NO
TFT_REGRESSION_NORMAL_BUILDS=4_OF_4_PASS
TFT_RUN_ID=20261009_220034_167fa2ad
TFT_ARCHIVE_SHA256=ab73b244c44ebd75d29a4eeb3cd97f5d18c08470f535eb16d55c2fdbf2310ff8
TFT_TOOLING_COMMIT=b85cb9c37cde39c33b56c56fd7ef325ca6670355
TFT_PRODUCT_COMMIT=e211ff6acc85b57f97bf7a9d28fbb91025cd0bae
TFT_SYNC_MERGE_COMMIT=dbcc68019590cc5a37c2042b09db5ef6ac131b77
NEXT_GATE=A13-G3-TCA_RMW_SHADOW_ATOMICITY
PRODUCT_WORKTREE_EXPECTED=CLEAN_AFTER_COMMIT
RELEASE_MERGE=NOT_EXECUTED
ALPHA13_PUBLICATION=NOT_EXECUTED
NEXT_FAILURE_ID=F111
OPENPLC=OUT_OF_SCOPE
HMI_DESIGNER=OUT_OF_SCOPE
TFT_NEW_FEATURES=OUT_OF_SCOPE
```

Fuentes: [cierre TFT](A13_TFT_CLOSURE_20261009.md), [evidencia física](A13_TFT_CLOSURE_PHYSICAL_EVIDENCE_20261009.md) y [checklist](A13_TFT_CLOSURE_CHECKLIST_20261009.md). No repetir gates de TFT ya cerrados. El próximo avance es G3.

---

Actualizado: 2026-10-09 — G2 / A13-002 CLOSED_PASS

> Fuente viva de continuidad del alpha. Un chat nuevo debe verificar el estado real
> del repositorio y continuar desde `NEXT_GATE`.

## Identidad

```text
RELEASE_VERSION=v2.1.0-alpha.13
BRANCH=v2.1.0-alpha.13/feature/cleanup-robustness
BASELINE_HEAD=20f1b66075dac04160311b7f40c939e88534073f
ALPHA12_STATUS=CLOSED_PUBLISHED
ALPHA13_STATUS=IN_PROGRESS
```

## Alcance

```text
PURPOSE=cleanup + technical debt + robustness + safe optimization
OPENPLC=OUT_OF_SCOPE
HMI_DESIGNER=OUT_OF_SCOPE
TFT_NEW_FEATURES=OUT_OF_SCOPE
```

## P0

```text
A13_P0=PASS_WITH_KNOWN_BASELINE_DEFECT
P0A=PASS
P0B=PASS
P0C=PASS
P0D1=PASS_WITH_KNOWN_BASELINE_DEFECT
P0D2=PASS
```

Known baseline defect:

```text
Adafruit_BusIO source fallback -> Wire.h unresolved
release/precompiled path affected=NO
```

## G1 — A13-001 DNS truncation

```text
G1_PRE1=PASS
G1_P1=PASS
G1_P2=PASS
G1_P3_ATTEMPT_1=REVIEW_ENVIRONMENT
G1_P3_R1=REVIEW_HARNESS
G1_P3_R2=REVIEW_HARNESS
G1_P3_R3=REVIEW_PRECONDITION
G1_P3_R4=REVIEW_HARNESS
G1_P3_R5=PASS
G1_STATUS=CLOSED_PASS
```

Candidato cerrado:

```text
FILE=JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/Dns.cpp
SHA256=08291b4b89274f014e1ce6073bd16303192e5bfe1b4e2fdeaa21d133fb783995
PRODUCT_COMMIT=8bc48b73074859d367b2d949e333f0b16c83cbd7
COMMIT_MESSAGE=fix(ethernet): evitar bloqueo DNS con respuestas truncadas
REMOTE_PUSH=PASS
WORKTREE_AFTER_COMMIT=CLEAN
```

Evidencia cerrada:

```text
BASELINE_TRUNCATED_LOOP_REPRODUCED=True
PATCH_COMPILE=PASS
VALID_DNS_PACKETS=3/3
TRUNCATED_PREFIXES=136/136
```

P3 intento 1:

```text
DNS_NORMAL_PATH_CONTRACT=True
COMPILE_EXIT=0
REPO_ETHERNET_SELECTED=True
DNS_SOURCE_OBJECT_COUNT=1
PORTS_VISIBLE=COM1
REQUESTED_PORT=COM4
UPLOAD_EXIT=1
PHYSICAL_REGRESSION=NOT_EXECUTED
PRODUCT_FAILURE=NO
ENVIRONMENT_FAILURE=YES
```

P3 R1 — wrapper de sintaxis:

```text
BAT_SYNTAX_PREFLIGHT=FAIL
ERROR=PowerShell recibió el token literal ^ antes de |
PHYSICAL_REGRESSION=NOT_EXECUTED
PRODUCT_FAILURE=NO
HARNESS_FAILURE=YES
```

Clasificación conforme al registro de fallos del proyecto:

```text
F091_APPLIES=YES
CHAT_AS_LONG_GATE_EDITOR=DO_NOT_REPEAT
VERSIONED_PS1_BAT_LOGS_STATUS=REQUIRED
```

P3 R2 — carrera READY serial:

```text
A13_GATE_SYNTAX=PASS
COMPILE_EXIT=0
REPO_ETHERNET_SELECTED=True
DNS_SOURCE_OBJECT_COUNT=1
UPLOAD_EXIT=0
CLIENT_EXIT=4
NB3_DNS_READY_TIMEOUT=YES
PHYSICAL_DNS_SEQUENCE=NOT_STARTED
PRODUCT_FAILURE=NO
HARNESS_FAILURE=YES
ENVIRONMENT_FAILURE=YES
```

Hipótesis planteada durante R2, posteriormente NO confirmada como causa raíz:

```text
probe READY = one-shot
gate delay after upload = 800 ms
client wait starts after serial open
posible pérdida del marcador READY
```

La evidencia posterior mostró que el JWPLC todavía no tenía la precondición
Ethernet completa (RJ45 + red con DHCP) durante esos intentos. Por tanto:

```text
R2_READY_RACE_AS_ROOT_CAUSE=NOT_PROVEN
PRODUCT_FAILURE=NO
LESSON=no promover una hipótesis temporal a causa raíz sin instrumentación
```

Corrección R3:

```text
probe Alpha13 propio
READY reemitido cada 500 ms mientras WAIT_COMMAND
sin cambios en Dns.cpp
sin cambios en API/producto
```

P3 R3 — READY no alcanzado:

```text
A13_GATE_SYNTAX=PASS
COMPILE_EXIT=0
REPO_ETHERNET_SELECTED=True
DNS_SOURCE_OBJECT_COUNT=1
UPLOAD_EXIT=0
CLIENT_EXIT=4
NB3_DNS_READY_TIMEOUT=YES
DNS_SEQUENCE_STARTED=NO
PRODUCT_FAILURE=NO
ROOT_CAUSE=PRECONDITION_NOT_YET_CLASSIFIED
```

La reemisión periódica elimina la hipótesis de marcador READY perdido.
R4 añade telemetría serial no invasiva de runtime Ethernet mientras READY=false:

```text
BEGIN_ATTEMPTED
READY
BUSY
runtimeState
lastError
diagnosticCode
```

Esto permite separar LINK_OFF / DHCP / SPI / HW / otro estado antes de
atribuir cualquier regresión a Dns.cpp.

Higiene de evidencia R4:

```text
BUILD_PATH=%TEMP%
RESULTS=logs locales ignorados por Git
GITHUB_DESKTOP=no debe listar objetos .o/.d/.bin/.elf/.map del build
```

Se recupera así el patrón de los gates físicos Alpha14, que compilaban fuera
del working tree.

P3 R4 — fallo del reader serial:

```text
A13_GATE_SYNTAX=PASS
COMPILE_EXIT=0
REPO_ETHERNET_SELECTED=True
DNS_SOURCE_OBJECT_COUNT=1
UPLOAD_EXIT=0
CLIENT_EXIT=4
SERIAL_READER_EXCEPTION=UnicodeEncodeError
CONSOLE_ENCODING=cp1252
INVALID_SERIAL_BYTE_DECODED_AS=U+FFFD
NB2_ETH_DIAG_CAPTURED=NO
PRODUCT_FAILURE=NO
HARNESS_FAILURE=YES
```

R5 usa un cliente Alpha13 propio. Los bytes seriales inválidos se representan
con escapes ASCII y la escritura a stdout aplica `backslashreplace`; un byte
de arranque corrupto ya no puede matar el thread de captura.

P3 R5 — cierre físico:

```text
A13_GATE_SYNTAX=PASS
DNS_SHA256=08291b4b89274f014e1ce6073bd16303192e5bfe1b4e2fdeaa21d133fb783995
DNS_NORMAL_PATH_CONTRACT=True
COMPILE_EXIT=0
REPO_ETHERNET_SELECTED=True
DNS_SOURCE_OBJECT_COUNT=1
UPLOAD_EXIT=0
CLIENT_EXIT=0
DUT_IP_EFFECTIVE=192.168.0.31
PC_DNS_SERVER_IP=192.168.0.4
RESULT_CODE=1
PROBE_FAILED=NO
SUCCESS_RESULT_IP=10.20.30.40
SUCCESS_DURATION_MS=1
SUCCESS_POLL_COUNT=5
TIMEOUT_DURATION_MS=453
TIMEOUT_POLL_COUNT=5663
DNS_BEGIN_HOLD_MAX_US=1460
DNS_POLL_HOLD_MAX_US=748
LOOP_GAP_MAX_US=644
SPI_LOCK_ERRORS=0
DNS_VALID_QUERY_COUNT=1
DNS_TIMEOUT_QUERY_COUNT=1
DNS_OTHER_QUERY_COUNT=0
NB3_DNS_CLIENT_PASS=YES
DIRTY_SCOPE_VALID=True
DIFF_CHECK_PASS=True
STATUS=PASS
PRODUCT_FAILURE=NO
HARNESS_FAILURE=NO
HARDWARE_FAILURE=NO
ENVIRONMENT_FAILURE=NO
```

Precondición física confirmada para este gate:

```text
USB/serial -> COM4
W5500/RJ45 -> LAN con DHCP
PC -> misma LAN
```

Cierre Git:

```text
FINAL_DIFF_AUDIT=PASS
STAGED_FILE_COUNT=1
STAGED_FILE=JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/Dns.cpp
GIT_DIFF_CACHED_CHECK=PASS
PRODUCT_COMMIT=8bc48b73074859d367b2d949e333f0b16c83cbd7
REMOTE_PUSH=PASS
G1=CLOSED_PASS
```

## Infraestructura G1-P3-R5

Versionada en commits de tooling/docs descendientes de `BASELINE_HEAD`. El gate
acepta esa cadena sólo si los archivos commiteados desde el baseline pertenecen
al allowlist de tooling/docs y confirma que `Dns.cpp` sigue sin commit:

```text
tools/alpha13/gates/common.ps1
tools/alpha13/gates/a13_g1_p3_dns_physical.ps1
tools/alpha13/gates/a13_g1_p3_dns_physical_client.py
tools/alpha13/gates/run_a13_g1_p3_dns_physical.bat
tools/alpha13/firmware/a13_g1_p3_dns_physical_probe/a13_g1_p3_dns_physical_probe.ino
tools/alpha13/results/.gitignore
tools/alpha13/results/.gitkeep
```

Contrato del gate:

```text
SerialPort explícito = prioridad 1
autodetección = sólo un único puerto distinto de COM1
sin candidato inequívoco = REVIEW_ENVIRONMENT
preflight de puerto fallido = no compilar / no subir / no abrir cliente
compile/upload/client/SUMMARY = logs persistentes por run
Dns.cpp = nunca stageado ni commiteado por el gate
wrapper BAT = sin pipes escapados entre cmd.exe y powershell -Command
topología = baseline ancestro + allowlist de commits tooling/docs
```

## NEXT_GATE

```text
NEXT_GATE=TFT-CLOSURE
NEXT_PHYSICAL_GATE=TFT-PRE6-P2B
OBJECTIVE=confirm boot-clean on physical JWPLC with normal product precompiled archive, then regress, finalize rebuild recipe and close
STATE=PRODUCT_ADOPTED_LOCALLY_UNCOMMITTED_AWAITING_PHYSICAL_GATE
PREREQUISITE=TFT_PRE6_P2A_CLOSED_PASS
P1A_R1=REVIEW_HARNESS_F109
P1A_R2=REVIEW_HARNESS_F109
P1A_R3=CLOSED_PASS
P1B=CLOSED_PASS
P2A=CLOSED_PASS
P2A_PRODUCT_FILES_CHANGED=3
PRODUCT_FIX_COMMITTED=NO
PRODUCTION_ARCHIVE_REFRESH=APPLIED_LOCAL_ONLY
NEXT_FAILURE_ID=F111
```

No modificar todavía código productivo de G2 hasta completar su preflight
dirigido y fijar el contrato del gate.

## G2 — A13-002 TCA startup / EN_IO

```text
G2_PRE1=PASS
G2_CLASSIFICATION=ROBUSTNESS_FIX
G2_PRIORITY=P0
G2_CONFIDENCE=HIGH
G2_STATUS=CLOSED_PASS
G2_P1_R2=PASS
G2_P2_R5=PASS
G2_P3=PASS
G2_P4=PASS
G2_P5=PASS
PRODUCT_CHANGE=COMMITTED_PUSHED
PRODUCT_COMMIT=6a585693
REMOTE_PUSH=PASS
WORKTREE_AFTER_COMMIT=CLEAN
```

Preflight read-only confirmado sobre HEAD:

```text
HEAD=690ca995bbced130f389b2432814fb85f7feb59b
SOURCE=JWPLC/2.1.0/cores/jwcontrol/peripherals_init.cpp
```

Hallazgos:

```text
EN_IO starts LOW=YES
TCA connection failure keeps EN_IO LOW=YES
five post-probe TCA configuration results ignored=YES
EN_IO goes HIGH after those calls regardless of result=YES
g_ioState.initialized becomes true before I2C/TCA completion=YES
JWPLC_IO.ready() consumes that initialized flag=YES
```

Operaciones cuyo resultado se ignora actualmente:

```text
1 TCA6424A_writeBank(bank1, 0x00)
2 TCA6424A_writeBank(bank2, 0x00)
3 TCA6424A_setBankDirection(bank0, 0xFF)
4 TCA6424A_setBankDirection(bank1, 0x00)
5 TCA6424A_setBankDirection(bank2, 0xFF)
```

Dependencia de build:

```text
normal jwplcbasic -> jwcontrol_precompiled_stub + precompiled/core/JWPLCBASIC/core.a
jwplcbasic source core direct at normal build=NO
core.a changed since Alpha13 baseline=NO
inherited Alpha12 core.a bytes=3042444
inherited Alpha12 core.a SHA256=78d0c0ab14f156b96116529e88872340d51877af40d24ba3559f6081e0bf34fb
```

Política obligatoria si A13-002 modifica `cores/jwcontrol`:

```text
SOURCE_CHANGE
-> ARCHIVE_INVALIDATED
-> SOURCE-FIRST PASS
-> REBUILD core.a
-> VERIFY normal jwplcbasic stub + archive
-> physical gate
-> product commit
```

G2-P1 debe reproducir baseline mediante instrumentación temporal y segura:

```text
5 failure legs + 1 normal/control leg
compile real peripherals_init.cpp from source using full jwplcbasic profile
do not mutate versioned core.a
temporary build under %TEMP%
restore any temporary source/boards.local mutation byte-for-byte
during failure legs intercept EN_IO HIGH request and keep physical EN_IO LOW
record requested EN_IO state, actual EN_IO state, operation result,
TCA register snapshot and JWPLC_IO.ready()
```

Criterio de reproducción baseline:

```text
for each injected failed operation:
OP_RESULT=FAIL
EN_IO_HIGH_REQUESTED=YES
EN_IO_ACTUAL=LOW   # safety interlock of harness
IO_READY=TRUE      # demonstrates current false-ready behavior
PRODUCT_FAILURE=REPRODUCED_BASELINE_DEFECT
```

La lectura de registros se conserva como evidencia, pero no se exige energizar
salidas para demostrar el defecto.

No implementar todavía el fix A13-002 hasta cerrar G2-P1.

Infraestructura G2-P1 versionada:

```text
tools/alpha13/gates/a13_g2_p1_tca_startup_baseline.ps1
tools/alpha13/gates/a13_g2_tca_startup_client.py
tools/alpha13/gates/run_a13_g2_p1_tca_startup_baseline.bat
tools/alpha13/firmware/a13_g2_tca_startup_probe/a13_g2_tca_startup_probe.ino
```

Salvaguardas:

```text
product source instrumentation=TEMPORARY_ONLY
source restored before physical uploads=REQUIRED
boards.local restored byte-for-byte=REQUIRED
versioned core.a mutation=FORBIDDEN
builds=%TEMP%
failure legs physical EN_IO=FORCED_LOW
control leg physical EN_IO=normal
```

G2-P1 R1 — evidencia:

```text
6/6 source-core compiles=PASS
source profile full jwplcbasic=PASS
peripherals_init.cpp count=1
stub core=0
versioned core.a linked=NO
source restore=PASS
boards.local restore=PASS
core.a SHA preserved=PASS
failure legs 1..5 contract=PASS
control leg contract=REVIEW
```

Las cinco piernas inyectadas reprodujeron de forma consistente:

```text
EN_IO_HIGH_REQUESTED=YES
IO_VIEW_READY=YES
OP_OK_MASK=30/29/27/23/15
```

El único fallo de contrato fue la pierna control porque `digitalRead(EN_IO)`
devolvió LOW pese a que el baseline había solicitado HIGH y toda la
configuración TCA era correcta. Esto se clasifica como F097: el harness usó
readback de pad para un GPIO configurado `GPIO_MODE_OUTPUT` sin input-enable.

R2 corrige sólo la observabilidad:

```text
EN_IO_OUTPUT_ENABLE <- GPIO_ENABLE_REG
EN_IO_OUTPUT_LATCH  <- GPIO_OUT_REG
EN_IO_PAD_READBACK  <- diagnóstico no contractual
PRODUCT_CHANGE=NO
```

G2-P1 R2 — cierre baseline:

```text
HEAD=0dd5e236604391c497839ae08452e507a131db78
A13_GATE_SYNTAX=PASS
COMPILE_LEGS=6/6 PASS
SOURCE_CORE=YES
STUB_CORE=NO
VERSIONED_CORE_A_LINKED=NO
FULL_JWPLCBASIC_PROFILE=YES
SOURCE_RESTORED=True
BOARDS_LOCAL_RESTORED=True
CORE_SHA256_PRESERVED=True
CONTROL_STEP_CONTRACT=PASS
FAULT_STEP_1_CONTRACT=PASS
FAULT_STEP_2_CONTRACT=PASS
FAULT_STEP_3_CONTRACT=PASS
FAULT_STEP_4_CONTRACT=PASS
FAULT_STEP_5_CONTRACT=PASS
STATUS=PASS
REASON=BASELINE_DEFECT_REPRODUCED_SAFELY
PRODUCT_FAILURE=REPRODUCED_BASELINE_DEFECT
HARNESS_FAILURE=NO
HARDWARE_FAILURE=NO
ENVIRONMENT_FAILURE=NO
```

Control normal:

```text
OP_OK_MASK=31
EN_IO_HIGH_REQUESTED=YES
EN_IO_OUTPUT_ENABLE=YES
EN_IO_OUTPUT_LATCH=HIGH
IO_VIEW_READY=YES
```

En las cinco fallas inyectadas el baseline conserva el defecto:

```text
EN_IO_HIGH_REQUESTED=YES
EN_IO_OUTPUT_LATCH=LOW   # interlock de seguridad del harness
IO_VIEW_READY=YES
PERIPHERALS_INITIALIZED=YES
```

Por tanto A13-002 deja de ser sólo riesgo estático: queda reproducido en
hardware con fault injection controlado.

Siguiente paso: diseñar el cambio mínimo productivo. Debe mantener `EN_IO`
en LOW y `JWPLC_IO.ready()==false` ante cualquiera de las cinco fallas, y
sólo declarar ready después de completar correctamente la configuración TCA.

No regenerar todavía `core.a`: primero debe pasar source-first el candidato.

Diseño G2-P2 fijado:

```text
PRODUCT_FILES=3
1 cores/jwcontrol/peripherals_init.cpp
2 cores/jwcontrol/jwplc_peripherals.cpp
3 cores/jwcontrol/jwplc_peripherals.h
PUBLIC_API_BREAK=NO
CORE_A_REFRESH=NOT_YET
```

Cambios mínimos:

```text
jwplcSystemInitState() -> IO ready=false
post-probe TCA ops -> every return checked
any failed op -> return with EN_IO still LOW
successful startup -> EN_IO HIGH -> settle -> peripheral init true -> IO ready=true
non-Basic path -> explicit IO ready=true after system state init
```

Identidad esperada del candidato local:

```text
peripherals_init.cpp blob=23efb3935a34e6b5649875b57c804e60538827cd
jwplc_peripherals.cpp blob=c3d53566d274e95b7dda111327140b5393f7db35
jwplc_peripherals.h blob=288667f1caa08142e2a155b8c85f24b2aa5beb44
staged=0
commit=NO
core.a SHA256=78d0c0ab14f156b96116529e88872340d51877af40d24ba3559f6081e0bf34fb
```

Infraestructura G2-P2:

```text
tools/alpha13/gates/apply_a13_g2_p2_candidate.ps1
tools/alpha13/gates/run_a13_g2_p2_apply_candidate.bat
tools/alpha13/gates/run_a13_g2_p2_apply_and_test.bat
tools/alpha13/gates/a13_g2_p2_tca_startup_candidate.ps1
tools/alpha13/gates/a13_g2_tca_startup_candidate_client.py
tools/alpha13/gates/run_a13_g2_p2_tca_startup_candidate.bat
tools/alpha13/firmware/a13_g2_tca_startup_candidate_probe/a13_g2_tca_startup_candidate_probe.ino
```

Contrato source-first:

```text
control:
attempt=31
ok=31
EN_IO latch=HIGH
peripherals initialized=YES
IO ready=YES

fault step N:
attempt=(1<<N)-1
ok=(1<<(N-1))-1
EN_IO high requested=NO
EN_IO latch=LOW
peripherals initialized=NO
IO ready=NO
```

El gate vuelve a compilar las seis piernas desde `cores/jwcontrol` con el
perfil completo `jwplcbasic`, preserva el `core.a` versionado y restaura la
instrumentación temporal al candidato exacto antes de cualquier upload.

G2-P2 R1 — REVIEW_HARNESS antes de ejecutar producto:

```text
APPLY_SYNTAX=PASS
APPLY_RESULT=FAIL
APPLY_REASON=Get-FileHash unavailable in local PowerShell environment
PATCH_APPLIED=NO
PRODUCT_CHANGE_EXECUTED=NO

CANDIDATE_GATE_SYNTAX=FAIL
PHYSICAL_GATE_EXECUTED=NO
PRODUCT_FAILURE=NO
HARNESS_FAILURE=YES
```

Corrección R2:

```text
SHA helper -> Get-A13Sha256 from versioned common.ps1
Get-FileHash dependency -> removed
duplicate malformed contract block in candidate gate -> removed
BAT parser preflight -> retained
candidate still must start from clean baseline
```

G2-P2 R2 — REVIEW_HARNESS:

```text
APPLY_SYNTAX=PASS
BASELINE_BLOBS=PASS
CORE_ARCHIVE_SHA=PASS
GIT_APPLY_CHECK=FAIL
PATCH_APPLIED=NO
PRODUCT_CHANGE_EXECUTED=NO

CANDIDATE_GATE_SYNTAX=PASS
CANDIDATE_GATE_STATUS=REVIEW
REASON=UNEXPECTED_HEAD_TOPOLOGY
UNEXPECTED_COMMITTED=
  docs/v2.1.0-alpha.13/A13_TOOLING_FAILURES_AND_PREVENTION_20261006.md
  tools/alpha13/gates/run_a13_g2_p2_apply_candidate.bat
PRODUCT_FAILURE=NO
HARNESS_FAILURE=YES
```

Corrección R3:

```text
git apply candidate mechanism -> removed
candidate application -> deterministic exact-once transforms
baseline blob guard -> retained
candidate blob guard -> retained
core.a SHA guard -> retained
allowlist -> completed with all G2-P2 tooling/docs paths
combined apply+test runner -> added
test cannot start if apply fails
```
G2-P2 R3 — REVIEW_HARNESS:

```text
A13_APPLY_SYNTAX=PASS
CANDIDATE_BLOB_MISMATCH=YES
FILE=jwplc_peripherals.cpp
ACTUAL=2e54c7950e2d23db2c19548e56f5b81fd483f92f
EXPECTED=875a50fd64552e8c4a4300e07b9494d2c32605d7
CATCH_RESTORE=EXECUTED
PHYSICAL_GATE_EXECUTED=NO
PRODUCT_FAILURE=NO
HARNESS_FAILURE=YES
```

Diagnóstico byte-a-byte:

```text
wrapper forced powershell.exe (PowerShell legacy) + UTF-8 without BOM
todavía -> todavÃ­a
están   -> estÃ¡n
MOJIBAKE_VARIANT_BLOB=2e54c7950e2d23db2c19548e56f5b81fd483f92f
MATCHES_OBSERVED_ACTUAL=YES
```

Corrección R4:

```text
product-generating PS1 literals -> ASCII-only
new expected jwplc_peripherals.cpp blob=c3d53566d274e95b7dda111327140b5393f7db35
candidate bytes are generated and hashed under %TEMP% first
repo product files are not touched unless all 3 temp blobs match
combined BAT parses common + apply + physical gate before apply
```

Entorno PowerShell confirmado por el desarrollador:

```text
PSEdition=Core
PSVersion=7.6.6
EXECUTABLE=pwsh
```

Regla aplicada desde G2-P2-R4:

```text
material BAT wrappers -> require pwsh / PowerShell Core 7+
no silent fallback to powershell.exe
wrapper prints resolved pwsh version before parser/apply/gate
```

G2-P2 R4 — REVIEW_HARNESS por parser de evidencia:

```text
PWSH_VERSION=7.6.6
CANDIDATE_APPLY=PASS
DIRTY_COUNT=3
STAGED_COUNT=0
6/6 COMPILE=PASS
SOURCE_CORE=True
STUB_CORE=False
ARCHIVE_LINKED=False
FULL_PROFILE=True
6/6 UPLOAD=PASS
6/6 CLIENT_EXIT=0
FAULT_STEPS_1_3_4_5 CONTRACT=PASS
STEP0/STEP2 FALSE_NEGATIVE=YES
PRODUCT_FAILURE=NO
HARNESS_FAILURE=YES
```

Evidencia visible del producto en R4:

```text
CONTROL:
attempt=31
ok=31
EN_IO high requested=YES
EN_IO latch=HIGH
peripherals initialized=YES
IO ready=YES

FAULT 1:
attempt=1
ok=0
EN_IO high requested=NO
EN_IO latch=LOW
peripherals initialized=NO
IO ready=NO

FAULT 2:
attempt=3
ok=1
EN_IO high requested=NO
EN_IO latch=LOW
peripherals initialized=NO
IO ready=NO

FAULT 3:
attempt=7
ok=3
EN_IO high requested=NO
EN_IO latch=LOW
peripherals initialized=NO
IO ready=NO

FAULT 4:
attempt=15
ok=7
EN_IO high requested=NO
EN_IO latch=LOW
peripherals initialized=NO
IO ready=NO

FAULT 5:
attempt=31
ok=15
EN_IO high requested=NO
EN_IO latch=LOW
peripherals initialized=NO
IO ready=NO
```

R5 cambia sólo el harness: el cliente emite claves canónicas
`A13_G2_CLIENT_*` del único bloque que ya validó y el gate consume sólo esas
claves. El candidato productivo local no cambia.

G2-P2 R5 — cierre source-first:

```text
STATUS=PASS
REASON=CANDIDATE_SOURCE_FIRST_PASS
6/6 COMPILE=PASS
6/6 UPLOAD=PASS
6/6 CLIENT=PASS
6/6 CONTRACT=PASS
SOURCE_CORE=True
STUB_CORE=False
ARCHIVE_LINKED=False
FULL_PROFILE=True
TRACKED_DIRTY_FINAL=3
STAGED_FINAL=0
DIFF_CHECK_FINAL=True
PRODUCT_FAILURE=NO
HARNESS_FAILURE=NO
HARDWARE_FAILURE=NO
ENVIRONMENT_FAILURE=NO
```

G2-P3 queda desbloqueado. Debe regenerar `core.a` desde los tres blobs
candidatos exactos, usar `%TEMP%` para outputs de build/verify, y demostrar
que el FQBN normal `jwplcbasic` compila con
`jwcontrol_precompiled_stub + core.a`.

Ante cualquier fallo de build/verify, el gate debe restaurar el archive
anterior y dejar sólo los tres sources candidatos dirty.


G2-P3 — refresh de core y enlace normal:

```text
HEAD=0cfec7d38719bea643baab97a5c4d7dac9833c48
PWSH_VERSION=7.6.6
ARDUINO_CLI=1.0.2

SOURCE_BUILD=PASS
SOURCE_BUILD_TIME_S=110.125
SOURCE_COMPILE_DB_ENTRIES=79
SOURCE_JWCONTROL_TUS=64
SOURCE_STUB_TUS=0
SOURCE_PERIPHERALS_INIT_COUNT=1

CORE_BEFORE_BYTES=3042444
CORE_BEFORE_SHA256=78d0c0ab14f156b96116529e88872340d51877af40d24ba3559f6081e0bf34fb
CORE_AFTER_BYTES=3043670
CORE_AFTER_SHA256=6f328eeb796091070c8d852a2c0e90f71047d8d2b5786fe3cd5079b2fb6ff983

NORMAL_VERIFY=PASS
NORMAL_VERIFY_TIME_S=73.107
NORMAL_COMPILE_DB_ENTRIES=16
NORMAL_SOURCE_TUS=0
NORMAL_STUB_TUS=1
NORMAL_PRECOMPILED_STUB_COUNT=1
NORMAL_JWPLCBASIC_STUB=True
NORMAL_JWPLCBASIC_SOURCE_CORE=False
NORMAL_JWPLCBASIC_CORE_A_LINKED=True
APP_BYTES=411376

BOARDS_LOCAL_UNCHANGED=True
TRACKED_DIRTY_FINAL=4
STAGED_FINAL=0
CANDIDATE_BLOBS_FINAL=True
DIFF_CHECK_FINAL=True
STATUS=PASS
REASON=CORE_REFRESH_AND_NORMAL_LINK_PASS
```

G2-P4 queda desbloqueado. Debe compilar un probe con el FQBN normal
`jwplcbasic`, demostrar nuevamente stub + archive sin source core, subirlo al
JWPLC y verificar físicamente:

```text
JWPLC_IO.ready()=YES
EN_IO output-enable=YES
EN_IO latch=HIGH
lastScanMs>0
```

Los fault steps no se repiten en P4: ya quedaron validados source-first en
G2-P2 y el archive de P3 fue generado desde esos mismos blobs cualificados.

G2-P4 — validación física con package normal precompilado:

```text
HEAD=5cf8dba1495de150bc9c7161a0233732417bb4b7
SERIAL_PORT=COM4
ARDUINO_CLI=1.0.2
COMPILE_EXIT=0
USES_STUB_CORE=True
USES_SOURCE_CORE=False
CORE_A_LINKED=True
SOURCE_TU_COUNT=0
STUB_TU_COUNT=1
PRECOMPILED_STUB_COUNT=1
FULL_PROFILE=True
UPLOAD_EXIT=0
CLIENT_EXIT=0
IO_READY=YES
EN_IO_OUTPUT_ENABLE=YES
EN_IO_OUTPUT_LATCH=HIGH
LAST_SCAN_MS=2280
CORE_SHA256=6f328eeb796091070c8d852a2c0e90f71047d8d2b5786fe3cd5079b2fb6ff983
SOURCE_BLOBS_FINAL=True
TRACKED_DIRTY_FINAL=4
STAGED_FINAL=0
DIFF_CHECK_FINAL=True
STATUS=PASS
REASON=NORMAL_PRECOMPILED_PHYSICAL_PASS
PRODUCT_FAILURE=NO
HARNESS_FAILURE=NO
HARDWARE_FAILURE=NO
ENVIRONMENT_FAILURE=NO
```

Cadena A13-002 completada:

```text
baseline defect reproduced -> PASS
source-first candidate physical -> PASS
core.a refresh -> PASS
normal stub + archive link -> PASS
normal precompiled physical -> PASS
```

Pendiente antes del commit:

```text
final unstaged diff audit
git diff --check
explicit staging of exactly 4 product files
staged diff audit
single product commit
push
status closure
```

## Observación mapeada — delay() y temporización no bloqueante

No forma parte del fix A13-002 ni abre un gate nuevo en Alpha13.

Estado actual del core:

```text
delay(ms) -> vTaskDelay(ms / portTICK_PERIOD_MS)
BLOCKS_ENTIRE_ESP32=NO
BLOCKS_USER_LOOP_TASK=YES
JWPLC_SYSTEM_TASK_CONTINUES=YES
```

Mientras el sketch está dentro de `delay(ms)`, la tarea independiente
`jwplcSystemTask` puede seguir atendiendo:

```text
I/O scan
RTC
Ethernet service
DataLog service
Display
```

La botonera también tiene su propia tarea de escaneo.

Sí quedan pausados hasta que retorna el `loop()` del usuario:

```text
código secuencial restante del loop
serialEventRun
JWPLC_ModbusTCP.task() autoservice pre/post-loop
JWPLC_ModbusRTU.task() si el usuario lo atiende desde loop
cualquier máquina de estados del sketch
```

Decisión de diseño:

```text
DO_NOT_OVERRIDE_ARDUINO_DELAY=YES
ASYNC_DELAY_API=CANDIDATE_BACKLOG
CURRENT_ALPHA_PRODUCT_SCOPE=NO_CHANGE
```

Dirección propuesta futura: una API de temporizadores no bloqueantes, por
ejemplo `JWPLC_Timer`, basada en `millis()` y segura ante rollover, con
`start()`, `done()`, `every()`, `restart()`, `cancel()` y
`remaining()`. Evitar callbacks/tareas por defecto para no introducir
concurrencia innecesaria en sketches de PLC.

`delayMicroseconds()` merece una revisión separada porque su implementación
espera activamente y no tiene la misma semántica cooperativa de `delay(ms)`.


## Observación mapeada — TFT sucia al energizar por USB

No se clasifica todavía como regresión productiva y no se mezcla con A13-002.

Estado actual del source:

```text
JWPLC_TFT_RST=GPIO14
initPeripherals():
  TFT_CS -> OUTPUT/HIGH
  TFT_RST -> OUTPUT/LOW
JWPLC_TFT.begin():
  TFT_eSPI backend init()
Alpha11 contract:
  TFT_RST_HELD_LOW_DURING_AUTOLOAD=YES
  FIRST_IDLE_FRAME_IMMEDIATE_AFTER_DISPLAY_BEGIN=YES
```

La retención de reset existe y no fue eliminada por A13-002. Sin embargo,
`initPeripherals()` se ejecuta dentro de `loopTask`, después de
`initArduino()`. El backlight del Basic v2 no tiene control por software.
Por ello existe una ventana estrictamente anterior a `initPeripherals()` en
la que el firmware de aplicación todavía no gobierna GPIO14 y el GRAM del
ST7789 puede hacerse visible al energizar.

Interpretación provisional:

```text
dirty pattern only before first IDLE frame -> pre-firmware/power-on window
dirty pattern persists after IDLE/display begin -> DISPLAY REGRESSION
```

Acción:

```text
TFT_STARTUP_DIRTY=OPEN_ROBUSTNESS_OBSERVATION
TFT_PRE1=BASELINE_TIMING_WITH_NORMAL_PACKAGE
TFT_NEW_FEATURES=OUT_OF_SCOPE
PRODUCT_CHANGE_BEFORE_MEASUREMENT=NO
G3_BLOCKED_UNTIL_TFT_PRE1_CLASSIFIED=YES
```

TFT-PRE1 — baseline con package normal:

```text
HEAD=ce7831e214a88551a85acf2be83dad569131881f
POWER_SOURCE=USB_ONLY
24VDC=DISCONNECTED
SERIAL_PORT=COM4
USES_STUB_CORE=True
USES_SOURCE_CORE=False
CORE_A_LINKED=True
SETUP_ENTRY_MS=664
DISPLAY_READY=YES
IO_READY=YES
TFT_RST_OUTPUT_ENABLE=YES
TFT_RST_OUTPUT_LATCH=HIGH
TFT_CS_OUTPUT_ENABLE=NO
TFT_CS_OUTPUT_LATCH=LOW
TRACKED_DIRTY_FINAL=0
STAGED_FINAL=0
DIFF_CHECK_FINAL=True
STATUS=PASS
REASON=BASELINE_TIMING_CAPTURED
```

Interpretación:

```text
first valid display frame occurs before setup()
software autoload before setup <= 664 ms from Arduino millis epoch
multi-second firmware-delay hypothesis=NOT_SUPPORTED_BY_PRE1
exact RST-low and first-frame timestamps=NOT_YET_MEASURED
TFT_CS post-init state=DIAGNOSTIC_ONLY
```

PRE1 no mide el instante exacto de power-on, `initPeripherals()` ni del primer
frame; sólo fija un límite superior antes de `setup()`. TFT-PRE2 debe usar
instrumentación temporal source-first y restaurarla byte-for-byte antes del
upload para medir `app_main`, `initArduino`, entrada a `initPeripherals`, RST,
I2C, RTC, FRAM, SD, botones, Display begin, primer refresh y TCA.

Comparación de backend relevante para el arranque:

```text
ALPHA11_BACKEND=Adafruit_ST7789
ALPHA12_CURRENT_BACKEND=JWPLC_TFT -> TFT_eSPI
CURRENT_JWPLC_TFT_BEGIN=g_backend.init() -> setRotation() -> release
CURRENT_EXPLICIT_BLACK_CLEAR_INSIDE_TFT_BEGIN=NO
FIRST_BLACK_CLEAR=Idle phase 0 after jwplcDisplayBeginCallback() returns
BACKLIGHT_SOFTWARE_CONTROL=NO
RST_HOLD_BEFORE_DISPLAY_BEGIN=YES
```

El código Alpha11 de JWPLC tampoco hacía un `fillScreen(BLACK)` dentro de
`tft.init()`: la corrección histórica fue mantener RST bajo durante el autoload
y hacer la inicialización antes de `setup()`. Por tanto la diferencia actual a
investigar es la secuencia interna del backend TFT_eSPI, no la desaparición de
la protección RST.

TFT-PRE2 cerró PASS con instrumentación source-first:

```text
DISPLAY_BEGIN_DURATION_US=558180
FIRST_REFRESH_DURATION_US=14067
RST_TO_DISPLAY_START_US=13145
BOOT_TO_FIRST_REFRESH_US=659341
SETUP_ENTRY_US=664542
SOURCE_RESTORED=True
CORE_SHA_PRESERVED=True
WORKTREE_FINAL=CLEAN
```

Clasificación:

```text
RTC_FRAM_SD_BUTTONS_AS_MAIN_CAUSE=NO
IDLE_FIRST_REFRESH_AS_MAIN_CAUSE=NO
DISPLAY_BEGIN_BACKEND_WINDOW=CONFIRMED
PRODUCT_CHANGE=NO
```

TFT-PRE3 reutiliza el probe de cualificación TFT_eSPI ya existente para
identificar la instalación 2.5.43 usada por el entorno de mantenimiento,
verificar hashes del source y caracterizar la secuencia ST7789 antes de
construir un candidato.

TFT-PRE3 R1 — atribución válida, parser semántico incompleto:

```text
TFT_ESPI_LIB_LIST_VERSION=2.5.43
TFT_ESPI_SELECTED_VERSION=2.5.43
TFT_ESPI_SELECTED_ROOT=C:\Users\jeykc\Documentos\Programacion\Arduino\libraries\TFT_eSPI
TFT_ESPI_HEADER_VERSION=2.5.43
TFT_ESPI_CPP_SHA256=01ed6edb0530d38b94ddeac079ba81633aa21d77d049b12da21a37f4bec69ee1
ST7789_INIT_SHA256=e21cae2ac84285dc0e77648eca67ca753f41f7da3c271594ede750b725136c10
TFT_ESPI_HEADER_SHA256=b1b2789ace7ac8fd4a4c414054757d91e6e62649a23d74226ea28ceb8d6f4462
RESET_HIGH_LOW_HIGH_PRESENT=True
RESET_DELAY_150_PRESENT=True
ST7789_DELAY_VALUES_MS=120,10,120,120,120,10,120,120
STATUS=REVIEW
REASON=ST7789_INIT_SEQUENCE_UNEXPECTED
PRODUCT_FAILURE=NO
HARNESS_FAILURE=YES
ENVIRONMENT_FAILURE=NO
```

F105: el parser asumió únicamente macros `TFT_*`; R2 acepta también
`ST7789_*` y valores literales 0x11/0x13/0x29, y reporta la expresión real
capturada para SLPOUT/NORON/DISPON.

TFT-PRE3 R2 — abortado por syntax preflight:

```text
A13_PWSH_VERSION=7.6.6
A13_TFT_PRE3_SYNTAX=FAIL
COMPILE_EXECUTED=NO
UPLOAD_EXECUTED=NO
PRODUCT_FAILURE=NO
HARNESS_FAILURE=YES
ENVIRONMENT_FAILURE=NO
FAILURE=F106
```

La edición remota R2 se corrompió por semántica de replacement de JavaScript
al insertar texto PowerShell que contenía `$'`. El BAT detectó la sintaxis
inválida antes de ejecutar el gate. R3 fue reconstruido desde el script R1
que sí había ejecutado, conservando el parser semántico corregido con un
mecanismo de replacement seguro.

TFT-PRE3 R3 — cierre:

```text
STATUS=PASS
REASON=TFT_ESPI_2543_SOURCE_ATTRIBUTED
TFT_ESPI_SELECTED_VERSION=2.5.43
TFT_ESPI_CPP_SHA256=01ed6edb0530d38b94ddeac079ba81633aa21d77d049b12da21a37f4bec69ee1
ST7789_INIT_SHA256=e21cae2ac84285dc0e77648eca67ca753f41f7da3c271594ede750b725136c10
TFT_ESPI_HEADER_SHA256=b1b2789ace7ac8fd4a4c414054757d91e6e62649a23d74226ea28ceb8d6f4462
RESET_HIGH_LOW_HIGH_PRESENT=True
RESET_DELAY_150_PRESENT=True
ST7789_SLPOUT_EXPR=ST7789_SLPOUT
ST7789_NORON_EXPR=ST7789_NORON
ST7789_DISPON_EXPR=ST7789_DISPON
ST7789_SLPOUT_BEFORE_DISPON=True
ST7789_FIRST_DELAY_AFTER_DISPON_MS=120
WORKTREE_FINAL=CLEAN
```

Conclusión:

```text
DISPLAY_BEGIN_DURATION_US=558180
backend delay blocks account for nearly all measured duration
visible dirty window begins after backend DISPON
candidate direction=defer DISPON, clear GRAM black, then DISPON
do not mutate installed TFT_eSPI in place
```

TFT-PRE4 sólo genera el candidato bajo `%TEMP%`: no modifica producto ni la
instalación TFT_eSPI. El candidato conserva el delay post-DISPON de 120 ms,
pero lo mueve después de limpiar GRAM a negro.

TFT-PRE4 cerró PASS:

```text
STATUS=PASS
REASON=TEMP_DEFERRED_DISPON_CANDIDATE_READY
CANDIDATE_JWPLC_TFT_CPP_SHA256=494440b00e74e74a7b23975420574e035ef2138fdf5a0cea2fed51c986c29d25
CANDIDATE_TFT_SETUP_SHA256=8fa079444ca130772d3a642eaf814035100bc098032b403c922c458d351ae5e1
CANDIDATE_ST7789_INIT_SHA256=44873be82fe836084934a328df77f098e9ab88d212dd1d570da5e8aac74671bc
BACKEND_ORIGINAL_MUTATED=NO
REPO_PRODUCT_MUTATED=NO
WORKTREE_FINAL=CLEAN
```

TFT-PRE5 R2 — REVIEW_HARNESS (F108):

```text
A13_TFT_PRE5_SYNTAX=PASS
SERIAL_PORT=COM4
PRE4_SHA_GUARDS=PASS
STATUS=REVIEW
REASON=UNEXPECTED_GATE_EXCEPTION
EXCEPTION=No se encuentra la propiedad "Count" en este objeto
COMPILE_CANDIDATE=NOT_REACHED
UPLOAD=NOT_EXECUTED
VISUAL_VERIFICATION=NOT_PERFORMED
```

R3 conserva PRE4 sin alteraciones, fuerza `@(...)` para los resultados de
`Get-ChildItem`, adelanta el control de cardinalidad y añade comprobaciones
sobre la unidad de traducción real `TFT_eSPI.cpp`: si no recibe
`ST7789_DRIVER` y `JWPLC_TFT_DEFER_DISPON`, el build aborta por `#error`.
La guardia se inyecta sólo bajo `%TEMP%` y se registran los hashes de
archivos originales y temporales. Cualquier anomalía evita la subida.

Para evitar un PASS falso por firmware anterior, la copia temporal del
probe imprime en cada bloque canónico el identificador
`A13_TFT_PRE5_CANDIDATE_SHA256` de PRE4. El cliente lo recopila y el gate
exige el valor esperado además de `DISPLAY_READY=YES`, `IO_READY=YES` y
estado válido de TFT_RST. La salida no declara corrección visual hasta que
se revise el video de tres encendidos USB completos.

Importante: un eventual PASS de PRE5-R3 significa compilación, subida y
runtime serial correctos, pero NO demuestra por sí solo la eliminación
visual del blanco/GRAM. Esa conclusión necesita el video de energizado USB
sin 24 V, comparado con el baseline PRE1/PRE2.

TFT-PRE5 R1 — REVIEW_HARNESS:

```text
A13_TFT_PRE5_SYNTAX=PASS
PRE4 candidate SHA guards=PASS
SERIAL_PORT=COM4
COMPILE_EXIT=1
TEMP_JWPLC_TFT_SELECTED=YES
TEMP_TFT_ESPI_SELECTED=YES
ERROR_CLASS=driver setup symbols missing
UPLOAD_EXECUTED=NO
STATUS=REVIEW
REASON=SOURCE_FIRST_CANDIDATE_COMPILE_FAILED
PRODUCT_FAILURE=NO
HARNESS_FAILURE=YES
FAILURE=F107
```

R2 elimina la sustitución de `User_Setup.h` / `User_Setup_Select.h` y usa
el mecanismo nativo de TFT_eSPI 2.5.43: una copia temporal del probe con el
`tft_setup.h` candidato en la misma carpeta del `.ino`. El backend global y
el repositorio permanecen intactos.

TFT-PRE5:

```text
- localiza el último candidato PRE4 PASS y verifica sus tres SHA;
- copia TFT_eSPI 2.5.43 a `%TEMP%` y aplica allí ST7789_Init.h candidato;
- sustituye User_Setup/User_Setup_Select únicamente en la copia temporal para
  fijar de forma reproducible el perfil privado JWPLC;
- crea JWPLC_TFT source-only temporal;
- exige selección de ambas librerías temporales y exactamente un
  JWPLC_TFT.cpp.o + un TFT_eSPI.cpp.o;
- mantiene JWPLC_Display y core en sus rutas precompiladas normales;
- sube el probe PRE1 y exige DISPLAY_READY=YES + IO_READY=YES;
- no modifica el repo ni la instalación TFT_eSPI del mantenedor;
- deja el firmware candidato cargado para la prueba visual de power-cycle.
```

Referencia externa de drivers:

```text
Adafruit ST7789 generic:
  SWRESET delay 150 ms
  SLPOUT delay 10 ms
  NORON delay 10 ms
  DISPON delay 10 ms

TFT_eSPI ST7789 upstream:
  hardware reset high/low/high + reset wait
  SLPOUT delay 120 ms
  delay 120 ms before DISPON
  DISPON
  delay 120 ms after DISPON
```

Esto es consistente con el video nuevo: la pantalla se hace blanca cerca de
+750 ms y el primer frame limpio empieza ~180 ms después. TFT-PRE2 medirá el
tiempo real de `jwplcDisplayBeginCallback()` ejecutando el archive actual, sin
cambiar aún el producto.

Si el fenómeno es sólo transitorio, la mejora software posible es adelantar
CS=HIGH/RST=LOW al punto seguro más temprano de `app_main()`; eso reduce la
ventana pero no puede eliminar el intervalo anterior al firmware. La solución
eléctrica absoluta requeriría mantener RST definido durante power-on o controlar
backlight, lo cual pertenece a hardware/revisión de placa y no se asumirá sin
revisar el esquemático.

## TFT-PRE5 R3 — cierre del candidato físico y visual (2026-10-09)

```text
TFT_PRE5_R3=PASS
REASON=SOURCE_FIRST_CANDIDATE_FLASHED
PROBE_INO_COUNT=1
COMPILE_EXIT=0
JWPLC_TFT_TEMP_SELECTED=True
TFT_ESPI_TEMP_SELECTED=True
JWPLC_TFT_SOURCE_OBJECT_COUNT=1
TFT_ESPI_SOURCE_OBJECT_COUNT=1
JWPLC_TFT_PRECOMPILED=False
JWPLC_DISPLAY_PRECOMPILED=True
USES_STUB_CORE=True
CORE_A_LINKED=True
UPLOAD_EXIT=0
CLIENT_EXIT=0
RUNTIME_CANDIDATE_SHA256=494440b00e74e74a7b23975420574e035ef2138fdf5a0cea2fed51c986c29d25
DISPLAY_READY=YES
IO_READY=YES
TFT_RST_OUTPUT_ENABLE=YES
TFT_RST_OUTPUT_LATCH=HIGH
TFT_CS_OUTPUT_ENABLE=YES
TFT_CS_OUTPUT_LATCH=HIGH
SETUP_ENTRY_MS=676
BASELINE_SETUP_ENTRY_MS=664
USER_VISUAL_WHITE_GRAM=RESOLVED_IN_TEMP_CANDIDATE
IDLE_PROGRESSIVE_RENDERING=OBSERVED_REMAINING
PACKAGE_LIBRARY_ARCHIVE_NOT_REFRESHED=YES
PRODUCT_COMMIT=NO
```

El usuario confirmó por nueva grabación y mensaje que el fondo blanco
desapareció y el arranque permanece limpio. El IDLE sigue renderizándose por
fases: no es parte del defecto de GRAM y no autoriza features nuevas.

PRE5 R3 conservó íntegros el repositorio, la librería externa TFT_eSPI y
los archives precompilados versionados. Por eso **la solución aún NO está
disponible para un sketch normal del package**: el candidato se compiló desde
dos librerías temporales seleccionadas por Arduino CLI, mientras
`JWPLC_TFT` distribuida sigue enlazando el archive anterior.

Documento de cierre: `A13_TFT_PRE5_PHYSICAL_VISUAL_20261009.md`.

```text
TFT-PRE6=NEXT
BUILD_RECIPE=TEMPORARY_COPY_OF_VERIFIED_TFT_ESPI_2_5_43
PRECOMPILED_ARCHIVE_REBUILD=REQUIRED
NORMAL_PACKAGE_NO_USER_TFT_ESPI_DEPENDENCY=REQUIRED
DISPLAY_UI_CONSUMER_REGRESSION=REQUIRED
NORMAL_PACKAGE_PHYSICAL_USB_STARTUP_VISUAL=REQUIRED
COMMIT_ONLY_AFTER_PASS=YES
```

## TFT-PRE6 — plan y primer gate sin mutación productiva

Documento: `A13_TFT_PRE6_INTEGRATION_PLAN_20261009.md`.

```text
TFT_PRE6_P0=PREPARED_NOT_EXECUTED
TFT_PRE6_P0_GATE=tools/alpha13/gates/a13_tft_pre6_p0_archive_candidate.ps1
TFT_PRE6_P0_RUNNER=tools/alpha13/gates/run_a13_tft_pre6_p0_archive_candidate.bat
P0_ARCHIVE_INPUT=PRE5_R3_COMPILED_SOURCE_OBJECTS
P0_OUTPUT=%TEMP%/candidate-libraries/JWPLC_TFT/src/esp32/libJWPLC_TFT.a
P0_NORMAL_CASES=DIRECT_TFT,DISPLAY_INTEGRATION,NORMAL_AUTOLOAD
P0_USES_NORMAL_PRECOMPILED_TFT=YES
P0_USER_EXTERNAL_TFT_ESPI_REQUIRED=NO
P0_OFFICIAL_ARCHIVE_MUTATION=NO
P0_USB_FLASH=NO
P1_CANONICAL_SOURCE_REBUILD=PENDING_P0_PASS
PRODUCT_COMMIT=NO
```

Prevención heredada: F105 macros semánticas; F106 reemplazos seguros;
F107 setup por sketch-local `tft_setup.h`; F108 arrays explícitos
`@(...)`, no reportar PASS físico en una compilación meramente estática.
El run P0 sólo garantiza archivo temporal autocontenido y compilación
normal (no demuestra aún fuente canónica ni un archive productivo instalado).

## TFT-PRE6-P0 — archive autónomo temporal (2026-10-09)

```text
TFT_PRE6_P0_STATUS=CLOSED_PASS
REASON=TEMP_PRECOMPILED_ARCHIVE_QUALIFIED
HEAD=89e461123474310e8528e4daf50c97262a86390b
CANDIDATE_ARCHIVE_SHA256=ff9dd89cb267bc270d2ca6fa2d0f1362b76595b8dc6b49f764d05a8e28bb6705
CANDIDATE_ARCHIVE_BYTES=1091942
TFT_OBJECT_SHA256=0a84fc685255b7b2e42a8050d0c643ce12afd6fcb628273993822b77755b74bb
BACKEND_OBJECT_SHA256=f4b771ba6847cb2b8dca3a7d9c29879725e3e160047df2e9b68b5944fae244cf
ARCHIVE_MEMBER_COUNT=2
ARCHIVE_MEMBER_PARITY=PASS
DIRECT_TFT_COMPILE_EXIT=0
DISPLAY_INTEGRATION_COMPILE_EXIT=0
NORMAL_AUTOLOAD_COMPILE_EXIT=0
COMPILE_CASES_PASS=3
GLOBAL_TFT_ESPI_REQUIRED=NO
OFFICIAL_ARCHIVES_PRESERVED=YES
WORKTREE_FINAL=CLEAN
UPLOAD_EXECUTED=NO
PRODUCT_REPO_MODIFIED=NO
```

Registro detallado: `A13_TFT_PRE6_P0_CLOSURE_20261009.md`.

PRE6-P0 prueba que los objetos físicamente validados en PRE5 R3 se pueden
empaquetar en una librería autónoma enlazable en tres casos Arduino normales.
Todavía no demuestra reconstrucción desde las fuentes canónicas.

## TFT-PRE6-P1A — reproducción exacta desde fuente (preparado)

```text
TFT_PRE6_P1A=PREPARED_NOT_EXECUTED
SOURCE_GEN=tools/alpha13/gates/a13_tft_pre6_p1_source_regen.py
GATE=tools/alpha13/gates/a13_tft_pre6_p1a_source_regen.ps1
RUNNER=tools/alpha13/gates/run_a13_tft_pre6_p1a_source_regen.bat
EXPECTED_SOURCE_CPP_SHA256=494440b00e74e74a7b23975420574e035ef2138fdf5a0cea2fed51c986c29d25
EXPECTED_SETUP_SHA256=8fa079444ca130772d3a642eaf814035100bc098032b403c922c458d351ae5e1
EXPECTED_ST7789_INIT_SHA256=44873be82fe836084934a328df77f098e9ab88d212dd1d570da5e8aac74671bc
PRODUCT_MUTATION=NO
COMPILE=NO
UPLOAD=NO
P1B_SOURCE_BUILD=BLOCKED_UNTIL_P1A_PASS
```

P1A usa únicamente sources actuales con identidad de Git comprobada y
TFT_eSPI 2.5.43 con hashes SHA-256 exactos. Transforma sólo archivos
temporales, sin modificar el código producto ni la instalación global
de TFT_eSPI. La coincidencia de los tres SHA físicos PRE4 es requisito
obligatorio para pasar a recompilación P1B. En Windows sólo se permite
normalizar CRLF/LF durante la comparación de identidad del blob Git.

## TFT-PRE6-P1A R1 — REVIEW_HARNESS / F109 (2026-10-09)

```text
P0_PREVIOUS=CLOSED_PASS
P1A_R1=REVIEW_HARNESS
REASON=P0_PASS_PROOF_NOT_FOUND
PHASE=P0_PROVENANCE
P0_ACTUAL_SUMMARY=tools/alpha13/results/tft_pre6_p0_20261009_123147/SUMMARY.log
P0_ACTUAL_STATUS=PASS
P0_ACTUAL_REASON=TEMP_PRECOMPILED_ARCHIVE_QUALIFIED
P0_ACTUAL_ARCHIVE_SHA256=ff9dd89cb267bc270d2ca6fa2d0f1362b76595b8dc6b49f764d05a8e28bb6705
P1A_COMPILE_EXECUTED=NO
P1A_UPLOAD_EXECUTED=NO
PRODUCT_REPO_MODIFIED=NO
FAILURE=F109
```

P1A R1 no encontró la prueba previa a pesar de que P0 pasó y dejó
evidencia canónica. R2 prueba el parser con un fixture sintético, hace la
llamada a `Get-A13LogValue` explícita y muestra los valores
`STATUS/REASON/SHA` para cada `SUMMARY.log` candidato. La causa exacta
queda condicionada al diagnóstico R2; P0 no se repite.

Se reforzó también el generador de `ST7789_Init.h` para comprobar,
contra el SHA físico PRE4, las variantes exactas LF/CRLF de los here-strings
PowerShell. No flexibilizar hashes ni modificar el producto.
`NEXT_FAILURE_ID=F111`; sin nuevas hipótesis tratadas como hechos.

## TFT-PRE6-P1A R2 — evidencia faltante (2026-10-09)

```text
P1A_R2=REVIEW_HARNESS
PROOF_PARSER_SELF_TEST=PASS
PROOF_SEARCH_DIRECTORY_COUNT=1
PROOF_SUMMARY_MISSING=tools/alpha13/results/tft_pre6_p0_20261009_123147/SUMMARY.log
COMPILE_EXECUTED=NO
UPLOAD_EXECUTED=NO
PRODUCT_REPO_MODIFIED=NO
F109_CAUSE_CLASS=LOCAL_PROOF_UNAVAILABLE
P0_PREVIOUS=CLOSED_PASS
NEXT=READ_ONLY_CHECK_OF_P0_DIR_AND_TEMP_ARCHIVE
```

La prueba del parser ha descartado la hipótesis de R1. El script
encuentra el directorio P0, pero no el `SUMMARY.log` que P0 anunció.
Se desconoce por qué el archivo ya no es accesible; no atribuirlo a
limpieza de archivos sin evidencia. P0 conserva un cierre Git durable,
`A13_TFT_PRE6_P0_CLOSURE_20261009.md`, pero el gate aún debe exigir
prueba suficientemente fuerte antes de aceptar un handoff recuperado.

P1A no se relanza hasta verificar si el archive de P0 en `%TEMP%`
sigue disponible. No regenerar el `SUMMARY.log` histórico ni repetir
P0 innecesariamente. Seguir `F109`; no crear `F110` sin otra clase
independiente de fallo.

## TFT-PRE6-P1A — recuperación de evidencia y R3 (2026-10-09)

```text
P0_DIRECTORY_EXISTS=True
P0_SUMMARY_EXISTS=False
P0_ARCHIVE_EXISTS=True
P0_ARCHIVE_BYTES=1091942
P0_ARCHIVE_SHA256=ff9dd89cb267bc270d2ca6fa2d0f1362b76595b8dc6b49f764d05a8e28bb6705
P0_RESULT=CLOSED_PASS
P1A_R3=PREPARED_NOT_EXECUTED
REQUIRES_HISTORICAL_P0_SUMMARY=NO
SOURCE_OF_TRUTH=P0_PRE4_PRE3_VERSIONED_DOCS_PLUS_P0_ARCHIVE_SHA
PRE4_CPP_SETUP_ST7789_EXPECTED_SHA=UNCHANGED
PRODUCT_MUTATION=NO
COMPILE_EXECUTED=NO
UPLOAD_EXECUTED=NO
NEXT_FAILURE_ID=F111
```

P1A R2 identificó que el parser pasa pero el archivo
`tools/alpha13/results/tft_pre6_p0_20261009_123147/SUMMARY.log`
ya no está disponible. El usuario confirmó que su archive P0 en
`%TEMP%` sigue presente, mide 1 091 942 bytes y coincide con el SHA
cerrado. La causa de desaparición del `SUMMARY.log` no se conoce.

R3 usa como evidencia durable los documentos versionados:
`A13_TFT_PRE6_P0_CLOSURE_20261009.md`,
`A13_TFT_PRE4_CANDIDATE_20261009.md` y
`A13_TFT_PRE3_SOURCE_ATTRIBUTION_20261009.md`.
Comprueba blobs Git desde HEAD, campos, compilaciones P0 documentadas,
archive SHA/tamaño, y backend TFT_eSPI 2.5.43 con hashes originales.
No se reconstruye ni falsifica el `SUMMARY.log` previo. El generador
de fuentes de P1A permanece sin modificaciones en R3. Sólo se progresa
a P1B después de `STATUS=PASS` real en P1A.

## TFT-PRE6-P1A R3 — CLOSED_PASS; P1B listo

```text
P1A_R3=PASS
REASON=CANONICAL_SOURCE_TRANSFORM_REPRODUCED
P0_HANDOFF_RECOVERED=YES
P0_PROOF_SOURCE=VERSIONED_CLOSURE_AND_ARCHIVE_SHA256
P0_HISTORICAL_SUMMARY=NOT_REQUIRED_MISSING
P0_ARCHIVE_SHA256=ff9dd89cb267bc270d2ca6fa2d0f1362b76595b8dc6b49f764d05a8e28bb6705
P1A_CPP_SHA256=494440b00e74e74a7b23975420574e035ef2138fdf5a0cea2fed51c986c29d25
P1A_SETUP_SHA256=8fa079444ca130772d3a642eaf814035100bc098032b403c922c458d351ae5e1
P1A_INIT_SHA256=44873be82fe836084934a328df77f098e9ab88d212dd1d570da5e8aac74671bc
P1A_BACKEND_CPP_INSTRUMENTED_SHA256=3fe3c601830aad09e579e8fec1223c6a0e9b6a31a82f491743fbe166a5b16350
P1A_MANIFEST=%TEMP%/jwplc_a13_tft_pre6_p1a_20261009_165802/MANIFEST.json
P1A_COMPILE_EXECUTED=NO
P1A_UPLOAD_EXECUTED=NO
P1A_PRODUCT_MUTATED=NO
P1B_GATE=tools/alpha13/gates/a13_tft_pre6_p1b_rebuild_archive.ps1
P1B_WRAPPER=tools/alpha13/gates/run_a13_tft_pre6_p1b_rebuild_archive.bat
P1B_STATE=READY_NOT_EXECUTED
P1B_SOURCE_COMPILE=REQUIRED
P1B_ARCHIVE_OUTPUT=TEMP_ONLY
P1B_NORMAL_COMPILE_CASES=3
P1B_UPLOAD=NO
P1B_PRODUCT_MUTATION=NO
NEXT_FAILURE_ID=F111
```

Documento de cierre: `A13_TFT_PRE6_P1A_CLOSURE_20261009.md`.

P1B selecciona el manifiesto P1A que declare las mismas fuentes y
verifica los hashes físicos PRE4 antes de compilar. El gate se basa en el
modelo de archive y tres consumidores de P0, que ya pasaron.
Exige compilación source-first de `JWPLC_TFT.cpp.o` y
`TFT_eSPI.cpp.o`, archive de dos miembros con paridad byte a byte,
y builds Arduino normales que NO seleccionen la TFT_eSPI global.
No se permite publicar el archive ni dar por corregido el package antes
de P2 y su prueba física.

## TFT-PRE6-P1B — archive reconstruido desde fuentes (2026-10-09)

```text
P1B_STATUS=CLOSED_PASS
REASON=CANONICAL_SOURCE_ARCHIVE_REBUILT_AND_LINKED
HEAD=8598a0dbee6006b27c28f1232ca2877d882423fe
SOURCE_COMPILE_EXIT=0
SOURCE_JWPLC_TFT_TEMP_SELECTED=True
SOURCE_TFT_eSPI_TEMP_SELECTED=True
SOURCE_CORE_STUB=True
SOURCE_CORE_ARCHIVE_LINKED=True
SOURCE_DISPLAY_PRECOMPILED=True
REGENERATED_TFT_OBJECT_SHA256=03063a848b5d5e20d8010b7d0bc19189b7cc5be412489588c1b14ded66815adf
REGENERATED_BACKEND_OBJECT_SHA256=59b4050dd44b5f6db7a99b2050aee7ff076008eb28032d18d222f4cf57b06732
P1B_ARCHIVE_SHA256=ab73b244c44ebd75d29a4eeb3cd97f5d18c08470f535eb16d55c2fdbf2310ff8
P1B_ARCHIVE_BYTES=1091990
ARCHIVE_MEMBER_COUNT=2
ARCHIVE_MEMBER_PARITY=PASS
NORMAL_COMPILE_CASES_PASS=3
GLOBAL_TFT_ESPI_REQUIRED_FOR_NORMAL_SKETCH=NO
OFFICIAL_ARCHIVES_PRESERVED=YES
WORKTREE_FINAL=CLEAN
PRODUCT_MUTATION=NO
UPLOAD_EXECUTED=NO
```

Cierre versionado: A13_TFT_PRE6_P1B_CLOSURE_20261009.md.
Diferencia entre P0 y P1B archive SHA aceptada: objetos compilados
independientemente con fuentes equivalentes y gates normales PASS.

## TFT-PRE6-P2A — adopción local reversible, gate preparado

```text
P2A_STATE=PREPARED_NOT_EXECUTED
P2A_GATE_PY=tools/alpha13/gates/a13_tft_pre6_p2a_adopt_product.py
P2A_GATE_PS1=tools/alpha13/gates/a13_tft_pre6_p2a_adopt_product.ps1
P2A_GATE_BAT=tools/alpha13/gates/run_a13_tft_pre6_p2a_adopt_product.bat
P2A_CHANGES=JWPLC_TFT.cpp,tft_setup.h,libJWPLC_TFT.a
P2A_GIT_COMMIT=NO
P2A_UPLOAD=NO
P2A_COMPILE_CASES=DIRECT_TFT,DISPLAY_INTEGRATION,NORMAL_AUTOLOAD,STARTUP_PROBE
P2A_FAIL_ROLLBACK=BYTE_IDENTICAL_BACKUPS_REQUIRED
P2A_SUCCESS_WORKTREE=3_EXPECTED_TRACKED_FILES_MODIFIED
P2A_SUCCESS_NEEDS_PHYSICAL_P2B=YES
PRODUCTIVE_ARCHIVE_REFRESH=NOT_YET_APPLIED
NEXT_FAILURE_ID=F111
```

P2A valida previamente hashes originales del core, Display, fuentes TFT
y libJWPLC_TFT.a. El candidato se selecciona por manifiesto de P1B
bajo %TEMP%, hashes de objeto, SHA del archive, tamaño y fuentes P1A.
Sólo después copia los tres archivos productivos, guarda originales
en %TEMP%, compila cuatro casos mediante la JWPLC_TFT del repositorio
y comprueba autoload, core precompilado, ausencia de fuente backend y
de TFT_eSPI externa seleccionada.

Si falla una condición, restaura los tres archivos originales y exige
worktree CLEAN. Si pasa, **deja tres modificaciones locales SIN COMMIT**
para realizar P2B físico. No hacer git pull, reset ni cambios de
rama hasta completar P2B o restaurar explícitamente la copia de
seguridad. Mantener F110 disponible sólo para fallos nuevos.

## TFT-PRE6-P2A — CLOSED_PASS; hito TFT-CLOSURE pendiente (2026-10-09)

**Estado canónico para nuevo chat.** El usuario ejecutó P2A con estos resultados:

```text
GATE=A13-TFT-PRE6-P2A
STATUS=PASS
REASON=PRODUCT_WORKTREE_ADOPTED_AND_FOUR_NORMAL_BUILDS_PASS
BRANCH=v2.1.0-alpha.13/feature/cleanup-robustness
HEAD_LOCAL_DURING_P2A=ac69e41922f512c280ad116bcf79b24596befa02
HEAD_REMOTO_AFTER_DOCS=CHECK_ON_ENTRY
P2A_CPP_SHA256=494440b00e74e74a7b23975420574e035ef2138fdf5a0cea2fed51c986c29d25
P2A_TFT_SETUP_SHA256=8fa079444ca130772d3a642eaf814035100bc098032b403c922c458d351ae5e1
P2A_TFT_ARCHIVE_SHA256=ab73b244c44ebd75d29a4eeb3cd97f5d18c08470f535eb16d55c2fdbf2310ff8
P2A_TFT_ARCHIVE_BYTES=1091990
P2A_CORE_SHA256=6f328eeb796091070c8d852a2c0e90f71047d8d2b5786fe3cd5079b2fb6ff983
P2A_DISPLAY_SHA256=c960d718433e29a40e3cc55bc745c9a2e121ee1ee598ec72c04872327592dc02
P2A_NORMAL_COMPILE_CASES_PASS=4
P2A_DIRECT_TFT=PASS
P2A_DISPLAY_INTEGRATION=PASS
P2A_NORMAL_AUTOLOAD=PASS
P2A_STARTUP_PROBE=PASS
P2A_GLOBAL_TFT_ESPI_REQUIRED=NO
P2A_PRODUCT_CHANGED_FILE_COUNT=3
P2A_PRODUCT_COMMIT_EXECUTED=NO
P2A_UPLOAD_EXECUTED=NO
P2A_BACKUP_ROOT=%TEMP%/jwplc_a13_tft_pre6_p2a_20261009_172339/originals
LOCAL_WORKTREE=3_TRACKED_PRODUCT_FILES_MODIFIED_UNCOMMITTED
NEXT_FAILURE_ID=F111
```

Los tres archivos locales adoptados en el equipo Windows **y sólo esos**:

```text
JWPLC/2.1.0/libraries/JWPLC_TFT/src/JWPLC_TFT.cpp
JWPLC/2.1.0/libraries/JWPLC_TFT/src/tft_setup.h
JWPLC/2.1.0/libraries/JWPLC_TFT/src/esp32/libJWPLC_TFT.a
```

**No se han versionado aún esos cambios productivos** ni se ha efectuado
upload del archive productivo. La validación visual positiva anterior de PRE5
aplica al **candidato temporal**, no sustituye la prueba normal P2B.
La rama remota sí puede contener nuevos commits **documentales** desde
`ac69e419`, sin significar que los cambios productivos estén en GitHub.

### Siguiente hito: TFT-CLOSURE (flujo integrado)

Contrato operativo: `docs/JWPLC_COLLABORATION_WORKFLOW.md`.

1. **No repetir gates cerrados.** Empezar por verificación de hashes de
   los tres archivos modificados y `git status`, FQBN/COM disponible y
   backup existente. No forzar `git pull` sobre worktree intencionalmente
   modificado. Para traer únicamente un ejecutor nuevo, usar método
   explícito que no actualice los tres archivos productivos.
2. Ejecutar un **gate integrador**, automatizando compilación normal,
   upload por COM verificado, identificación del firmware por serial y
   condición `DISPLAY_READY=YES` / `IO_READY=YES`. Asegurar prueba
   real del archive productivo `ab73b244...` sin TFT_eSPI externa;
   evitar falso PASS por firmware serial anterior (provenance).
3. Pedir al usuario observación de arranque **USB-only** y video/capturas:
   fondo blanco ausente, pantalla limpia, IDLE coherente. No inferir
   resultado visual solo de serial. El dibujado progresivo de IDLE es
   observación distinta, no bug de GRAM.
4. Tras PASS visual, validar regresiones de Display y consumers de
   Alpha12/Alpha13, autoload, APIs y reconstrucción del archive con
   receta capaz de partir de los **nuevos fuentes ya parcheados**.
   La receta P1A original usa como entrada el baseline viejo:
   **requiere adaptación idempotente o nueva receta versionada**.
5. Preparar commit exacto de producto y documentación, revisión de
   `git diff --check` y checklist de Alpha13; commit/publicación
   sujetos a confirmación. No tocar OpenPLC ni HMI Designer.

```text
NEXT_GATE=TFT-CLOSURE
P2B_PHYSICAL=NOT_EXECUTED
P3_REGRESSION_AND_REBUILD_RECIPE=NOT_EXECUTED
P3_COMMIT=NOT_EXECUTED
PRODUCT_REPO_REMOTE_CHANGED=NO_FOR_TFT_THREE_FILES
WORKFLOW=INTEGRATED_MILESTONES
MIGRATION_HANDOFF_SEPARATE_FILE=NOT_REQUIRED
```

**Nota de precedencia:** este bloque y el `NEXT_GATE` vigente
reemplazan las menciones históricas `P2A_PREPARED_NOT_EXECUTED` en
secciones anteriores, conservadas como registro cronológico.

## Gates restantes

```text
G2  A13-002 TCA startup / EN_IO                  CLOSED_PASS
G3  A13-004 TCA RMW/shadow atomicity             PENDING
G4  A13-003 TFT batch task ownership             PENDING
G5  A13-005 + A13-006 TCP correctness            PENDING
G6  A13-007 + review A13-008 datagram robustness PENDING
G7  A13-009 + A13-013 FRAM                       PENDING
G8  A13-010 + A13-011 serial                     PENDING
G9  A13-012 + A13-014 + A13-017 cleanup          PENDING
G10 final regression/freeze                       PENDING
```

## KNOWN_RISKS

```text
Adafruit_BusIO source fallback baseline defect
stale precompiled archives can mask source changes
serial COM can change between sessions
```

## DO NOT DO

```text
DO_NOT_REPEAT_P0
DO_NOT_REPEAT_G1_PRE1_P1_P2_WITHOUT_NEW_EVIDENCE
DO_NOT_REMOVE_NORMAL_AUTOLOAD_PERIPHERALS
DO_NOT_ASSUME_OPENPLC_INTEGRATED
DO_NOT_ASSUME_OTA_DEFINED
DO_NOT_FIX_FINAL_FLASH_FREQ_WITHOUT_DECISION
DO_NOT_PUBLISH_BOOTLOADER_BIN_AS_FINAL
DO_NOT_IMPLEMENT_JW_BUSIO_YET
G1_CLOSED_G2_MAY_START_WITH_READ_ONLY_PREFLIGHT
```
