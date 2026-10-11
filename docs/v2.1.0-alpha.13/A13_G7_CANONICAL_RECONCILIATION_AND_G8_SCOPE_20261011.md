# Alpha13 — Conciliación canónica G7 y alcance G8 (2026-10-11)

**Estado:** DIAGNOSTIC_READ_ONLY, NO_PRODUCT_MODIFICATION.
**Rama:** `v2.1.0-alpha.13/feature/cleanup-robustness`.
**HEAD de referencia:** `cd3fe89d2dbf882bbdb6b8a04f2f7d1d916df16a`.

Fuente canónica contrastada: informe de auditoría
`Markdown pegado(1).md` adjunto al proyecto.
Este documento NO repite la auditoría completa.

## G7 — dos cosas diferentes

G7 está cerrado de forma **técnica y reproducible**:
`CLOSED_PASS`, pruebas FRAM físicas y restauración,
compilación desde fuente, paridad objeto/archive, regresiones 2/2,
auditoría de hashes, commits de producto y documentación, push no-force.
No invalidar ni repetir ese resultado.

**Pero el cierre no equivale a resolver A13-009 y A13-013.**
El cierre publicado ya decía `CANONICAL_MAPPING=REVIEW_PENDING`.
La comparación puntual con el informe original y los tres archivos
de producto `JW_FRAM.cpp`, `JW_FRAM.h`, `libJW_FRAM.a` demuestra:

- **A13-009 — PENDING_CANONICAL_FIX:** `JW_FRAM::readString()`
  continúa obteniendo longitud con `read8(addr)` (que devuelve
  `0` tanto para vacío real como ante fallo SPI), y llama
  `value.reserve(len)` sin comprobar su retorno. La prueba G7
  sobre `readBlock()` no cubrió el escenario canónico.
  Se requiere un candidato distinto, lectura de longitud con
  retorno booleano y tests de fallo/empty válido/límites/payload,
  sin cambiar formato ni firma.
- **A13-013 — PENDING_CANONICAL_CLEANUP:** `JW_FRAM.cpp`
  aún tiene los guards del miembro `_spi_dev` en ambos
  constructores y `debugPrint(const String&)` /
  `debugPrint(const char*)` privados; `JW_FRAM.h`
  sigue declarándolos. Los cambios G7 de null guards y
  checksum son robustecimientos adicionales, no equivalentes
  a retirar esas rutas sin uso. Deben revisarse referencias
  exactas y ABI pública, reconstruyendo el archive.

**Decisión de alcance recomendada:** abrir subhito
`A13-G7C-FRAM-CANONICAL-COMPLETION` antes de declarar cerrados
**los hallazgos** o congelar Alpha13. No revertir producto G7 ni
restar valor a los tests cerrados. Reutilizar su proceso de
compilación y archivado, pero NO repetir escrituras FRAM si
son innecesarias; preferir fixtures de fallo SPI y regresiones,
y subir firmware físico sólo cuando el plan lo justifique.

## G8 — alcance real del informe canónico

**No es un gate de Serial USB/CH340**: apunta al transporte
industrial RS-485 y al Modbus RTU.

- **A13-010 (ADOPT):** `JWPLC_ModbusRTU.cpp` suma dos
  `_stats.rxBytes++` consecutivos en slave no-bulk (líneas
  actuales 493–494) y omite ese incremento en master no-bulk
  (línea actual 905). Los modos bulk sí suman `readBytes`.
  Cambiar exclusivamente la contabilidad para que cuente cada
  byte **realmente recibido**, una sola vez, en las cuatro
  combinaciones master/slave × bulk/no-bulk. No modificar
  parsing, framing, CRC, temporización o APIs.
- **A13-011 (ADOPT):** `JWPLC_RS485.cpp` llama
  `_serial->begin()` (void) y establece `_ready=true`
  sin inspeccionar si existe el driver. El core
  `HardwareSerial::begin` puede volver con `_uart=NULL`;
  su `operator bool()` consulta `uartIsDriverInstalled`.
  Mantener las firmas públicas y semántica de status; sólo
  anunciar ready ante inicio real. Verificar begin/end/begin,
  fallo de instalación, baud/config y error existente.

### Compilación real y política de evidencia

- `JWPLC_ModbusRTU/library.properties`: `precompiled=full`;
  archive productivo `src/esp32/libJWPLC_ModbusRTU.a`.
  Source patch SIN actualizar el .a no garantiza corrección.
  Exigir objeto recompilado = miembro del archive extraído,
  linker con `-L` exacto y `-lJWPLC_ModbusRTU`, y SHA.
- `JWPLC_RS485/library.properties`: compila desde fuente
  (sin directiva `precompiled=full`). Acreditar su
  `JWPLC_RS485.cpp.o` seleccionado en compilación normal.
- El `HardwareSerial` relevante al JWPLC Basic forma parte
  del **core precompilado**; evitar cambiarlo para estas dos
  correcciones que pueden resolverse en los wrappers.
- Conservar periféricos en autoload, APIs de usuario,
  Arduino CLI oficial, package local, RS485 AutoDirection
  del MAX13487E y Modbus RTU cooperativo.

### Gates G8 propuestos

1. **G8-P0** baseline read-only: rama/HEAD/limpieza,
   hashes fuentes + archive + core, CLI 1.5.1 oficial,
   ejemplo slave/master, selección real de librerías,
   compilación normal y casos fuente exactos.
2. **G8-P1** fixtures reproducibles, sin hardware:
   RX master/slave, bulk/non-bulk, cero bytes, errores
   negativos, overflow y estado UART `begin/end/begin`
   con error simulado. Rechazar false PASS por fuentes
   enlazadas desde archive obsoleto.
3. **G8-P2** cambios candidatos mínimos + reconstrucción
   del archive Modbus RTU + verificación objeto/SHA/link;
   regresión normal de sketches y consumidores del package.
4. **G8-P3** considerar prueba física de integración RS-485
   solo tras P1/P2 PASS y disponer de un dispositivo/puerto
   RS-485 seguro; no inventar un peer ni un resultado.
   Si no es requerida según riesgo, documentar justificación.
5. **G8-P4** adoptar únicamente tras todo PASS, auditoría
   hash/scope, cierre documental, commits/push no-force
   automáticos. Sin PR/release de Alpha13.

### Pendientes explícitos

```text
G7_TECHNICAL_GATE=CLOSED_PASS
A13-009=PENDING_CANONICAL_FIX
A13-013=PENDING_CANONICAL_CLEANUP
G8_A13-010=ADOPT_PREPARATION_ONLY
G8_A13-011=ADOPT_PREPARATION_ONLY
F120_POST_UPLOAD_FREEZE_ROOT_CAUSE=UNKNOWN_PENDING
F121_TOOLING=FIXED_VALIDATED_IN_CLOSED_G7
G8_PRODUCT_CHANGED=NO
G8_PHYSICAL=NOT_EXECUTED
RELEASE_MERGE_PR_PUBLICATION=NO
OPENPLC=OUT_OF_SCOPE
HMI_DESIGNER=OUT_OF_SCOPE
TFT_NEW_FEATURES=OUT_OF_SCOPE
```
