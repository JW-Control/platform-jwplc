# Alpha13-G8 — Cierre Git de producto RTU/RS-485 (2026-10-11)

**Resultado:** `G8_GIT_PRODUCT=CLOSED_PASS`. **Gate G8 funcional:** `PENDING_PHYSICAL_DECISION`. **Alpha13:** `IN_PROGRESS`.

## Evidencia Git del operador

```text
G8_GIT_RUN_ID=g8_git_20261011_194840_716ae090
G8_GIT_UNSTAGED_PATHS=EXACT_THREE_PRODUCT_FILES
G8_GIT_INDEX_FLAGS=H_H_H
G8_GIT_PREFLIGHT=PASS
G8_GIT_STAGED_AUDIT=PASS
G8_GIT_PRODUCT_COMMIT=fb71d65eb2c842b3f46e6949760e693f3b73d95e
G8_GIT_STATUS=PRODUCT_COMMIT_PUSH_PASS_PHYSICAL_DECISION_PENDING
G8_GIT_COMMIT_PUSH=PASS
```

## Verificación independiente de GitHub

Commit remoto confirmado:
[fb71d65eb2c842b3f46e6949760e693f3b73d95e](https://github.com/JW-Control/platform-jwplc/commit/fb71d65eb2c842b3f46e6949760e693f3b73d95e).

El commit contiene **solo estos tres archivos**, comprobados en la
respuesta de GitHub a la lectura del commit:

```text
JWPLC/2.1.0/libraries/JWPLC_ModbusRTU/src/JWPLC_ModbusRTU.cpp
JWPLC/2.1.0/libraries/JWPLC_ModbusRTU/src/esp32/libJWPLC_ModbusRTU.a
JWPLC/2.1.0/libraries/JWPLC_RS485/src/JWPLC_RS485.cpp
```

El HEAD remoto de `v2.1.0-alpha.13/feature/cleanup-robustness`
coincidió con el commit anterior al verificarlo.

## Qué quedó resuelto

**A13-010 — Contadores `rxBytes` de Modbus RTU.**

- Slave no-bulk: eliminado doble incremento; se cuenta una
  vez cada byte leído correctamente del transporte.
- Master no-bulk: añadido incremento faltante.
- El byte consumido que provoca overflow se contabiliza exactamente una vez.
- Se conservan las rutas bulk, lógica del protocolo, motor, API y periféricos.

**A13-011 — Comprobación real de inicio UART en RS-485.**

- `begin()` comprueba `HardwareSerial::operator bool()` después
  del `begin()` que retorna `void`.
- No marca `_ready=true` si no hay driver UART instalado.
- Conserva la API, error existente, transceptor de autodirección y
  control de serial CH340 independiente.

## Evidencias técnicas anteriores al commit

```text
G8_P0=PASS_READ_ONLY
G8_P1P2=CANDIDATE_PASS
G8_P1P2_RUN_ID=g8_p1p2_20261011_190500_d59b4022
G8_NATIVE_CI_20_CASES=PASS
G8_PRODUCT_ADOPTION_RUN_ID=g8_adopt_20261011_193839_57f9bb49
G8_PRODUCT_ADOPTION=PASS
G8_PRODUCT_REGRESSIONS=SLAVE_PASS_MASTER_PASS_RS485_STATUS_PASS
G8_LINKER_RTU_PRECOMPILED=PASS_EXACT_L_PATH_L_FLAG_SHA
G8_ARCHIVE_PRODUCT_SHA256=f239100fbb6770a3a6f336179fbfa3a4afd27cc16e12c6f239cf05f28f7a1416
G8_GIT_COMMIT_PUSH=PASS
```

El archivo `libJWPLC_ModbusRTU.a` está incluido en el mismo commit
que su fuente modificada, necesario por `precompiled=full`.
Los manifiestos locales de Windows respaldan la procedencia; el
contenido de esos manifiestos no se presupone disponible en el remoto.

## Lo que NO quedó demostrado

No hubo interacción física con transceptor RS-485, peer,
tramas reales ni upload durante la adopción/cierre Git. Los
20 casos C++ nativos se ejecutaron con mocks, y las tres
compilaciones Arduino IDE/CLI prueban integración/enlace,
no comunicación real bajo carga.

Por tanto:

```text
A13-010=PRODUCT_COMMITTED_TESTS_PASS_PHYSICAL_PENDING
A13-011=PRODUCT_COMMITTED_TESTS_PASS_PHYSICAL_PENDING
G8_GIT_PRODUCT=CLOSED_PASS
G8_PHYSICAL_RS485=NOT_EXECUTED
G8_GATE=CURRENT_NOT_CLOSED
ALPHA13=IN_PROGRESS
G8_NEXT=PHYSICAL_PRECONDITIONS_AND_DECISION
RELEASE_MERGE_PR_PUBLICATION=NO
```

No publicar nueva pre-release por este commit ni marcar G8 funcional
como `CLOSED_PASS` hasta registrar prueba física o decisión
explícita de diferimiento/aceptación de riesgo.

Referencia para el banco:
[A13_G8_RS485_MODBUS_PHYSICAL_VALIDATION_PLAN_20261011.md](A13_G8_RS485_MODBUS_PHYSICAL_VALIDATION_PLAN_20261011.md).
