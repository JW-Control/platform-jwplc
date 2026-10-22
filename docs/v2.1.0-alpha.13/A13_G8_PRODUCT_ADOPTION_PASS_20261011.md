# Alpha13 G8 — Adopción productiva local PASS (2026-10-11)

**Estado:** `PRODUCT_ADOPTED_REGRESSIONS_PASS_PENDING_PHYSICAL_DECISION_AND_COMMIT`. La adopción está aplicada solo en el árbol productivo local Windows; **todavía sin commit remoto**.

## Evidencia reportada por operador

```text
RUN_ID=g8_adopt_20261011_193839_57f9bb49
PREFLIGHT=PASS
PREFLIGHT_INDEX_FLAGS=H_H_H
BACKUP=PASS
PRODUCT_ADOPTION=PASS
SOURCE_CONTRACT_POST=PASS
PRODUCT_REGRESSIONS=PASS
PRODUCT_COMPILE_SLAVE_EXIT=0
PRODUCT_COMPILE_MASTER_EXIT=0
PRODUCT_COMPILE_RS485_STATUS_EXIT=0
RTU_ARCHIVE_LINK_SLAVE=PASS_EXACT_L_PATH_L_FLAG_SHA
RTU_ARCHIVE_LINK_MASTER=PASS_EXACT_L_PATH_L_FLAG_SHA
FINAL_AUDIT=PASS
GIT_UNSTAGED_CHANGES=EXACT_THREE_EXPECTED_PATHS
GIT_INDEX_STAGED_CHANGES=NONE
SOURCE_AND_ARCHIVE_HASH_PARITY=PASS
RTU_ARCHIVE_SHA256=f239100fbb6770a3a6f336179fbfa3a4afd27cc16e12c6f239cf05f28f7a1416
ADOPTION_STATUS=PRODUCT_ADOPTED_REGRESSIONS_PASS_PENDING_PHYSICAL_DECISION_AND_COMMIT
GIT_COMMIT_OR_PUSH=NONE
UPLOAD_OR_RS485_ACCESS=NONE
```

Se conservaron mediante respaldo y auditoría los archivos originales en
`%TEMP%/jwplc_a13_g8_adopt_20261011_193839_57f9bb49/originals`.
El manifiesto de la ejecución en Windows se encuentra en
`tools/alpha13/results/g8_adopt_20261011_193839_57f9bb49/MANIFEST.json`.
El manifiesto no está en GitHub y **se verificará** en el gate de commit; esta
sección recoge únicamente los marcadores reportados en consola.

## Archivos productivos exactos adoptados

```text
JWPLC/2.1.0/libraries/JWPLC_ModbusRTU/src/JWPLC_ModbusRTU.cpp
JWPLC/2.1.0/libraries/JWPLC_RS485/src/JWPLC_RS485.cpp
JWPLC/2.1.0/libraries/JWPLC_ModbusRTU/src/esp32/libJWPLC_ModbusRTU.a
```

A13-010: contador `rxBytes` corregido por byte consumido en Slave y
Master no-bulk (incluido byte consumido que desborda buffer);
bulk se conserva. A13-011: `begin()` de RS-485 comprueba que el driver
UART esté instalado mediante `HardwareSerial::operator bool()`;
mantiene contrato/API existente y señalización del error.

## Decisión de integración y pruebas

- **Aceptar el PASS de adopción local:** reconstrucción de archive RTU
  desde objeto real, paridad miembro–objeto, selección de librerías del
  package normal, `precompiled=full`, `JWPLC_RS485.cpp.o` desde fuente,
  tres compilaciones en Windows y 20 fixtures C++ nativos en CI.
- **Autorizar gate de cierre Git técnico** de solo estas tres rutas,
  después de verificar procedencia y hashes de los manifests y backup.
  No convertir el PASS del candidato en una publicación sin incorporar
  el archive binario al mismo commit.
- **Prueba física RS-485 / Modbus RTU:** decisión pendiente antes del
  cierre funcional de G8. Esta adopción no ejecutó COM, tráfico,
  transceptor, peer industrial ni upload. Si se prueba, definir montaje
  seguro y evidencias separadas; no inventar resultados.
- **Alpha13 sigue abierta.** No crear PR de publicación, merge a
  `release/v2.1.x`, tag ni PreRelease hasta sus gates finales.

```text
G8_ADOPTION_LOCAL=CLOSED_PASS
A13-010=PRODUCT_ADOPTED_LOCAL_PENDING_COMMIT
A13-011=PRODUCT_ADOPTED_LOCAL_PENDING_COMMIT
G8_GIT_PRODUCT_COMMIT=NOT_EXECUTED
G8_PHYSICAL_RS485=DECISION_PENDING
G8=CURRENT_NOT_CLOSED
NEXT=G8_GIT_PRODUCT_CLOSE_THREE_FILES
```
