# Alpha13 G10-P1C — Compilación Arduino CLI de candidato A13-006

**Fecha:** 2026-10-12. **Estado:** `PREPARED_REAL_BUILD_PENDING`.
No se ha modificado firmware productivo, compilado el overlay
en la PC operadora ni realizado upload.

## Estado de entrada

```text
G9_FINAL=CLOSED_PASS_WITH_EXPLICIT_COVERAGE_LIMITS
G10_P0_RUN_ID=g10_p0_20261012_114921_34102c6b
G10_P1A_RUN_ID=g10_p1a_20261012_120529_a174356e
G10_P1B_RETEST_RUN_ID=g10_p1b_20261012_124640_8458554e
G10_P1B_LOCAL=SOURCE_CONTRACT_PASS_FOUR_PRIVATE_CANDIDATES
G10_P1B_CI_NATIVE=PASS_WINDOWS_LINUX_RUN_38284253191
G10_P1C_ARDUINO_BUILD=NOT_YET_RUN
A13-006=ADOPT_CONFIRMED_REPRO_FIX_PENDING
ALPHA13=IN_PROGRESS
```

Los candidatos P1B están en
`tools/alpha13/results/g10_p1b_20261012_124640_8458554e/candidate_sources/`.
El runner P1C exige manifests de P0, P1A y P1B
y compara SHA-256 de las cuatro fuentes con la rama
antes de copiar al overlay.

## Diseño del gate

Archivos versionados:

```text
tools/alpha13/gates/a13_g10_p1c_a13_006_build.py
tools/alpha13/gates/run_a13_g10_p1c_a13_006_build.bat
.github/workflows/alpha13-g10-p1c-a13-006-build-contract.yml
```

Se crea una copia TEMPORAL de la librería original
`JWPLC_ModbusTCP`, sustituyendo únicamente estos archivos
por los candidatos P1B:

```text
JWPLC_ModbusTCP_Client.cpp
JWPLC_ModbusTCP_Client.h
JWPLC_ModbusTCP.cpp
JWPLC_ModbusTCP.h
```

**No se cambia** `library.properties`,
`JWPLC_Ethernet`, `JW_SD`, core,
periféricos integrados o autoload.

### Matriz de compilación

| Librería seleccionada | Ejemplo | Artefactos esperados |
|---|---|---|
| Productiva original | `01.ModbusTCP_Server` | `JWPLC_ModbusTCP.cpp.o` y `JWPLC_ModbusTCP_Client.cpp.o` |
| Productiva original | `02.ModbusTCP_Client` | Ambos objetos |
| Overlay candidato | `01.ModbusTCP_Server` | Ambos objetos desde el overlay |
| Overlay candidato | `02.ModbusTCP_Client` | Ambos objetos desde el overlay |

Para cada uno: `arduino-cli` 1.5.1 fijo,
FQBN `jwplc_local:esp32:jwplcbasic`,
selección de ruta real de librería en log
`Using library JWPLC_ModbusTCP...`,
`JWPLC_Ethernet` declarado, compilación
de las dos fuentes C++ y enlace del core
precompilado Basic. Se usan builds limpios
y directorios independientes.

Comprobar archive SHA-256 congelados
`JW_SD.a`, `core.a` Basic, `JWPLC_TFT.a`,
`JW_FRAM.a`, `JWPLC_ModbusRTU.a` antes
y después, junto con Git y las cuatro
fuentes de producto inmutadas.

Los cuatro builds son una **prueba de compatibilidad
de compilación y enlace**, no de cierre eventual
de sockets ni de protección ante reset W5500.
Se requieren nuevos tests negativos de
`begin()/end()/task()/fail()/reset Ethernet`
antes de autorizar adopción.

## Comando previsto del operador

```powershell
git pull --ff-only origin v2.1.0-alpha.13/feature/cleanup-robustness
.\tools\alpha13\gates\run_a13_g10_p1c_a13_006_build.bat
```

Resultado esperado solo si todas las compilaciones
y auditorías pasan:

```text
G10_P1C_PHASE=PREFLIGHT_PASS
G10_P1C_PHASE=OVERLAY_PASS
G10_P1C_BASELINE_SERVER=PASS_CORRECT_LIBRARY_AND_SOURCE_OBJECTS
G10_P1C_BASELINE_CLIENT=PASS_CORRECT_LIBRARY_AND_SOURCE_OBJECTS
G10_P1C_CANDIDATE_SERVER=PASS_CORRECT_LIBRARY_AND_SOURCE_OBJECTS
G10_P1C_CANDIDATE_CLIENT=PASS_CORRECT_LIBRARY_AND_SOURCE_OBJECTS
G10_P1C_PHASE=ARDUINO_BUILDS_PASS
G10_P1C_PHASE=FINAL_AUDIT_PASS
G10_P1C_STATUS=ARDUINO_BASELINE_AND_CANDIDATE_BUILDS_PASS_PRODUCT_UNCHANGED
```

Ante `REVIEW_STOP`, conservar los logs de compilación
en `tools/alpha13/results/g10_p1c_*/`.
No editar las fuentes del producto a mano ni
hacer Git reset/rebase para corregir un error
de overlay. No upload a COM4/COM14.

## Exclusiones

```text
G10_P1C_PRODUCT_EDIT=NO
G10_P1C_UPLOAD=NO
G10_P1C_COM4_COM14=NOT_TOUCHED
G10_P1D_ADOPTION=NOT_AUTHORIZED
G10_P1E_PHYSICAL_TCP=NOT_EXECUTED
A13-007=REVIEW_OPEN
A13-008=REVIEW_OPEN
A13-015=ADOPT_PENDING
A13-016=ADOPT_PENDING
A13-018=ADOPT_PENDING
F120_ROOT_CAUSE=UNKNOWN_PENDING
ALPHA13=IN_PROGRESS
PR_MERGE_RELEASE=NO
```
