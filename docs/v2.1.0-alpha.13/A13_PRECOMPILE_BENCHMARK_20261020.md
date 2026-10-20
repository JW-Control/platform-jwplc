# Alpha13 — Gate obligatorio de precompilación y benchmark Ethernet/Modbus TCP

**Estado:** `CANDIDATE_NOT_ADOPTED`. No hay todavía archives reales verificados, tabla de tiempos medida ni promoción a `JWPLC/`. **La publicación de Alpha13 se pospone** hasta cerrar este gate y revisar sus efectos.

## Fuente y reglas

- Base: `v2.1.0-alpha.13/feature/cleanup-robustness`; candidata: `codex/alpha13-precompile-benchmark-docs-20261020`.
- El código funcional G10 ya está integrado (PR #124). No introducir cambios funcionales ni alterar APIs, CS W5500, `platform.txt`, `boards.txt`, `core.a`, demás `.a` o autoload.
- Bibliotecas a empaquetar: `JWPLC_Ethernet` y `JWPLC_ModbusTCP`. **No precompilar `Dns.cpp`/ `Dhcp.cpp` por separado**: forman parte de todos los objetos del archive Ethernet.
- Build source-first, archiver del **mismo toolchain XTENSA usado por Arduino CLI**, paridad objeto→miembro `.a`; `precompiled=full` sólo cuando exista `src/esp32/lib<name>.a` válido. Prohibir compilar el `.cpp` de esas librerías otra vez en el consumer.
- Pruebas: ambos FQBN `jwplcbasic` y `jwplcbasiccore` sobre package aislado, mismo `compiler.cpp.flags`, toolchain y fuente en candidato y baseline. Comprobar símbolo y ELF, procedencia de librerías y no fallback externo.
- Validación cruzada de Windows y Linux y ejecución real desde el package `JWPLC/` **después** de promoción. No convertir un `.a` de `%TEMP%` en archive productivo sin reconstrucción y checksums reproducibles.

## Herramienta y CI

`tools/alpha13/precompile/a13_precompile_benchmark.py` hace una prueba sin USB ni MAC ni flash:

1. `--check`: N0 sin compilar.
2. Sin `--check`: construye en directorio temporal un baseline source-first de dos librerías y registra ELF, `.o`, `.d` y `--show-properties`.
3. Genera dos `.a` desde esos mismos `.o` y prepara `library.properties` con `precompiled=full` sólo en el stage temporal.
4. Compila perfiles Basic y Basic Core como consumidores reales; exige archive seleccionado y **cero** objetos `.cpp.o` de esas dos librerías.
5. Registra tiempos `source_cold`, `source_warm`, `archive_cold`, `archive_warm` para el mismo sketch y hardware de CI.
6. Guarda `MANIFEST.json`, logs y los dos `.a` **solo como artefactos de candidatura**. No hace `git add/commit` ni copia archivos al package oficial.

Workflow: `.github/workflows/alpha13-precompile-ethernet-modbus-tcp.yml`, Windows + Ubuntu, mismo Arduino CLI 1.5.1 y toolchain existente registrado para los ensayos Alpha13. Su éxito es `CANDIDATE_VALIDATED`, **no `PRODUCT_PRECOMPILED=PASS`**. La adopción deberá copiar dos binaries junto a dos `library.properties` por commit acotado (y comprobar de nuevo en el package final); no adoptar archives desde dos SO indistintamente sin verificar el formato/toolchain.

## Benchmark obligatorio (mediciones aún no ejecutadas)

La mejora absoluta y porcentual solo se informa al recibir `MANIFEST.json`:

| Configuración | Fría (s) | Caliente (s) | Perfil |
|---|---:|---:|---|
| Ethernet + Modbus TCP, fuentes | PENDING | PENDING | JWPLC Basic |
| Ethernet + Modbus TCP, ambos `.a` | PENDING | PENDING | JWPLC Basic |
| Ethernet + Modbus TCP, fuentes | PENDING | N/A | JWPLC Basic Core |
| Ethernet + Modbus TCP, ambos `.a` | PENDING | N/A | JWPLC Basic Core |

`AHORRO_FRIO_S=FUENTE_FRIO_S-ARCHIVE_FRIO_S`,
`AHORRO_FRIO_%=100*(FUENTE_FRIO_S-ARCHIVE_FRIO_S)/FUENTE_FRIO_S`.

**Metodología:** mismo equipo/runner, CLI, chip, FQBN, caches y sketch;
`--clean` para cada corrida fría y repetir sin `--clean` para caliente.
Una muestra en CI constituye una *medición preliminar*, no comparación
estadística definitiva. Para afirmar ventaja reproducible, realizar mínimo
3 pares fríos por entorno o documentar la variabilidad y la limitación.

La mejora de tiempo de **upload** no se presupone: precompilar bibliotecas
acelera potencialmente **compilación**, pero la transferencia USB depende del
firmware, puerto, baudrate y herramienta; medir upload aparte sólo cuando
haya banco y autorización. No atribuir una mejora inexistente.

## Documentación de usuario y gate final

Actualizar los README sin reordenar sus ejemplos Básico → Intermedio →
Aplicación real → Avanzado, conservar tablas de APIs y compatibilidad.
El README del package mantiene el resumen G1–G10 y el estado real
`PRECOMPILED_FREEZE`.

```text
ALPHA13=OPEN
ARCHIVES_SOURCE=2_CPP_LIBRARIES
ARCHIVE_CANDIDATES=NOT_BUILT
SOURCE_TO_OBJECT_TO_ARCHIVE_TO_ELF=PENDING
BOTH_FQBN=NOT_RUN
BENCHMARK_SOURCE_VS_ARCHIVE=PENDING
BENCHMARK_3_TRIALS=PENDING
READMES_CANDIDATE=IN_PROGRESS
PRODUCT_ARCHIVES=NOT_ADOPTED
PRODUCT_RELEASE=NOT_PUBLISHED
BASE_SYNC=PENDING_PRECOMPILE_PR
KNOWLEDGE_SYNC=OK_PREVIOUS_G10_LESSON
NEXT_FAILURE_ID=F136
NEXT_GATE=PRECOMPILE_N0_THEN_CI_CANDIDATES_AND_BENCHMARK
```
