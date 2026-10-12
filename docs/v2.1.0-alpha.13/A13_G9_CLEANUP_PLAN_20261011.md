# Alpha13 G9 — Plan operativo definitivo de cleanup canónico

**Fecha:** 2026-10-11. **Estado:** `PREPARED_G9_P0_PENDING_LOCAL_CAPTURE`.
**Rama:** `v2.1.0-alpha.13/feature/cleanup-robustness`.
**HEAD de entrada verificado antes de versionar plan:** `8bf81eb0b8bed628091c6c5bf3691bb140590301`.

## Alcance y decisiones

La auditoría canónica `Markdown pegado(1).md` asigna exactamente
los siguientes hallazgos al grupo G9 previsto en el estado Alpha13:

| Hallazgo | Decisión | Superficie | Invariante |
|---|---|---|---|
| A13-017 | ADOPT | `README.md`, comentarios `JWPLC_ModbusTCP.h` y `JW_SD.h` | Reconciliar solo texto activo con Alpha12 publicada y backend presente; preservar historia y API |
| A13-012 | ADOPT | `JW_SD.cpp` | Sustituir el carácter NUL real dentro del literal de `_path[0]` por el escape textual `'\0'` sin alterar la comparación lógica |
| A13-014 | ADOPT con equivalencia | `JWPLC_GlobalPeripherals.cpp` | `g_rtcEnabled` es siempre true y `g_buttonRefreshThrottleMs` permanece en 25; preservar RTC, callbacks de botonera y ritmo de refresco |

**Fuera de G9:** A13-013 ya se cerró en G7C. A13-015
(smoke CI incompleto), A13-016 (freshness de archives),
A13-018 (métrica `BinaryBytes`) se abordan como deuda de
verificación en G10; no darlos por resueltos por el PASS
de los gates físicos anteriores. A13-019 y A13-021
diferidos, A13-020 y A13-022 rechazados según auditoría;
no modificar perfiles ni recortar autoload/headers públicos.

## Plan ejecutable por fronteras

### G9-P0 — Baseline binario + estado actual (solo lectura)

Runner:
`tools/alpha13/gates/a13_g9_p0_readonly.py`
y BAT:
`tools/alpha13/gates/run_a13_g9_p0_readonly.bat`.

Comprobar rama, HEAD = tracking, Git limpio, ancestro de
commit producto G8, rutas rastreadas y SHA-256. Leer
`JW_SD.cpp` **como bytes**, registrar número/offset/contexto
de NUL, hash de `libJW_SD.a` y
`precompiled=full`. Enumerar referencias a
`g_rtcEnabled` y `g_buttonRefreshThrottleMs`.
Detectar afirmación contradictoria sobre Alpha11/Alpha12
en README y comentarios activos A14.2/G2b.

Un PASS de P0 es exclusivamente `BASELINE_CAPTURED_READ_ONLY`,
no corrección productiva ni compilación de nuevos fuentes.

### G9-P1 — A13-017 documentación activa

Corregir solo referencias cuyo estado real esté demostrado
por repositorio y cierres canónicos. Ejemplos de baseline:

- README declara Alpha12 PreRelease publicada y simultáneamente
  Alpha11 como última versión completamente cerrada.
- Comentario `JWPLC_ModbusTCP.h` habla de A14.2 como futuro:
  verificar contrato actual del servidor antes de redactar;
  no prometer `stopServer()` u otra API no presente.
- Comentario `JW_SD.h` usa un futuro de autoservicio G2b;
  comprobar implementación y runtime antes de actualizarlo.

Validación: diff textual mínimo, sin cambios en declaraciones
C++, sin reescritura de documentación histórica ni funcionalidad
TFT, OpenPLC o HMI. Commit documental separado.

### G9-P2 — A13-012 SD source NUL

Un único cambio lógico: sustituir byte NUL real en el literal
por `\0` textual. Sin limpieza adicional de `JW_SD`
ni cambio de funcionalidades DataLog.

**Obligatorio por `precompiled=full`:**
recompilar `JW_SD.cpp` como fuente en entorno controlado,
acreditar objeto `JW_SD.cpp.o`, reconstruir archive
`libJW_SD.a`, extraer/verificar miembro, recompilar sketches
SD/DataLog en modo package normal y acreditar que el linker
usa el archive nuevo. Si el archive reconstruido es
byte-a-byte igual al anterior, registrar esa paridad de forma
explícita; si difiere, debe acompañar la nueva fuente en
el mismo commit. Ninguna aceptación basada exclusivamente
en timestamp Git.

Mantener intactos RTC, FRAM, Ethernet, TFT, botonera,
RS485, Modbus y autoload. Fuente→objeto→archive no
implica prueba física SD cuando no se ejecuta hardware;
decidir cobertura física en función de semántica conservada.

### G9-P3 — A13-014 GlobalPeripherals

Cambios mínimos en variables internas inmutables y
ramas realmente imposibles, según referencias actuales.
Mantener `RTC enabled = true`,
`button refresh throttle = 25 ms`, callback semantics,
configuración de variantes Basic/Core, y API pública.
`JWPLC_GlobalPeripherals` es librería compilada desde
fuente; validar selección normal de su `.cpp.o` y
regresión de peripherals/RTC/botonera; no inventar un archive
GlobalPeripherals que `library.properties` no declara.

### G9-P4 — Regresión, Git y cierre

Ejecutor integrado con STOP por fases, backups/rollback,
hashes antes/después y allowlist exacta. Compilación normal
Arduino CLI/IDE (evidencia CLI validada; IDE según caso),
API/autoload y dependencia `precompiled=full` de SD.
Separar commits documentales y productivos. Dejar
`A13-012`, `A13-014`, `A13-017` con estado
verificable y checklist específico; no considerar G9
cerrado con solo P0 o sintaxis C++.

## Entrada a G10: conciliación de auditoría y freeze

G10 no es solo `PASS` de ejemplos. Requiere:

1. Reconciliar **por hallazgo** la auditoría canónica completa,
   incluidos `A13-005/A13-006` en G5 y la decisión
   experimental `A13-007/A13-008` en G6, cuyos cierres
   anteriores registran límites de correspondencia.
2. `A13-015`: CI smoke del package y cobertura real
   TCP, RTU Master, Core y fuentes precompiladas cambiadas.
3. `A13-016`: gate de fingerprint de fuentes,
   headers transitivos, flags/config, toolchain y archive;
   no usar fechas como evidencia suficiente.
4. `A13-018`: corregir/explicitar `BinaryBytes` con métricas
   de artefactos comparables, `UNKNOWN` si faltan.
5. Tabla final de builds/tiempos cuando aplique,
   tamaños Flash/RAM, compatibilidad Arduino IDE,
   matriz de regresiones físicas/no físicas y decisiones
   de riesgo explícitas. No inventar ahorros de tiempo.
6. Mantener `F120_POST_UPLOAD_FREEZE_ROOT_CAUSE=UNKNOWN_PENDING`
   si no existe evidencia nueva; G8 probado no implica
   resolución del freeze intermitente.
7. PR, PreRelease y checklist **en español** solo después
   de validar G10 y contar con autorización de publicación.

```text
ALPHA13=IN_PROGRESS
G1_G2_TFT_G3_G4_G5_G6_G7_G7C_G8=PREVIOUSLY_CLOSED_WITH_RECORDED_LIMITS
G9=PENDING_P0_BASELINE
A13-012=PENDING
A13-014=PENDING
A13-017=PENDING
G10=PENDING
A13-015=PENDING
A13-016=PENDING
A13-018=PENDING
A13-019=DEFER
A13-020=REJECT
A13-021=DEFER
A13-022=REJECT
F120=UNKNOWN_PENDING
RELEASE_MERGE_PR_PUBLICATION=NO
```
