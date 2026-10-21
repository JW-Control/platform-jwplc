# F137 — resultados verificados (20/10/2026, Lima)

**Estado final (21/10/2026):** `F137_C=PASS_MERGED_BASE`. El [PR #125](https://github.com/JW-Control/platform-jwplc/pull/125) fue fusionado por squash (`23fae3a3febf8e6a9c19e7e8cb63b7b6a62cdb11`); el package base conserva árbol `cc17716f173509b1e0f3c0df5eb99549014c1d61`, igual al verificado en N3 [#39538155047](https://github.com/JW-Control/platform-jwplc/actions/runs/39538155047), con 13/13 compilaciones en **Windows y Ubuntu**. Se integraron **dos CPP de política Basic/Core, dos .a, dos library.properties** (`precompiled=full`) y ocho README; se preservan los demás archives y autoload. C de reconciliación [#39631264428](https://github.com/JW-Control/platform-jwplc/actions/runs/39631264428) y fuente base [#39632213031](https://github.com/JW-Control/platform-jwplc/actions/runs/39632213031) aprobaron sus dos jobs respectivos. Los 12+2 objetos normalizados y la política ELF `weak Basic / strong Basic Core` fueron validados entre ambos FQBN y SO.

**H física:** `PASS_FUNCIONAL_ACOTADO` en COM4 Master/COM14 Slave de desarrollo (uploads 2/2, FC03 remoto 2/2, eco UDP 3/3, RTU y periféricos). **No** representa prueba de Basic Core físico, R3 interno, R4 prolongado, DHCP/DNS específico o E1/E2. **Release:** aún no autorizado ni publicado. La auditoría global y ZIP release-like se verificaron en el PR #126; la instalación desde el índice dev y la validación de assets publicados quedan para postpublicación. Las entradas con `Draft` o `H pendiente` más abajo son **históricas** (cortes anteriores al merge), no estados vigentes.


**Actualización final PC del operador, 21/10/2026:** campaña Windows LOCAL de 3 repeticiones por FQBN y modo, fuente CPP contra archivos `.a`, 12/12 runs PASS y 24 tiempos. Mediana fría **Basic 52,120 → 40,190 s (−22,89 %)** y **Basic Core 54,964 → 47,958 s (−12,75 %)**; caliente 9,251 → 9,055 s (Basic) y 9,175 → 9,146 s (Core). Objetos Ethernet/Modbus TCP en fuentes 12/2, en archives 0/0. HEAD `158d12a2`, hashes canónicos inalterados, USB y hardware no tocados. Evidencia original [MANIFEST.json](evidence/A13_LOCAL_PC_BENCHMARK_MANIFEST_20261021.json), tabla completa y análisis de límites [A13_LOCAL_PC_BENCHMARK_RESULTADOS_20261021.md](A13_LOCAL_PC_BENCHMARK_RESULTADOS_20261021.md). **Esto es PC real, no CI**; las tablas siguientes describen exclusivamente CI.

Los benchmarks de [CI #39531758494](https://github.com/JW-Control/platform-jwplc/actions/runs/39531758494) contienen **tres parejas frías/calientes por perfil y SO**. Medianas medidas, segundos (fuente → archives):

| Sistema | FQBN | Frío fuentes → archives | Ahorro frío | Caliente fuentes → archives |
|---|---|---|---|---|
| Ubuntu | Basic | 27,245 → 18,976 | 8,269 s / **30,4 %** | 3,829 → 3,762 |
| Ubuntu | Basic Core | 35,065 → 26,141 | 8,924 s / **25,4 %** | 3,798 → 3,759 |
| Windows | Basic | 85,163 → 51,945 | 33,218 s / **39,0 %** | 10,413 → 10,246 |
| Windows | Basic Core | 95,054 → 79,908 | 15,146 s / **15,9 %** | 10,344 → 10,139 |

**Rango de muestras frías, segundos (fuente → archive):** Ubuntu Basic 27,217–27,943 → 18,601–19,171; Ubuntu Basic Core 34,731–35,335 → 26,123–26,631; Windows Basic 78,691–87,078 → 51,144–52,227; Windows Basic Core 94,620–96,247 → 74,266–81,065. El cambio **no mide ni promete** acelerar subida USB.

**Archives canónicos, idénticos Windows/Ubuntu:** Ethernet (12 miembros; 344774 bytes; SHA-256 `f190bbbbd69861941807a290be653de422e11a898d1a9583d289cc573d38a0f4`); Modbus TCP (2 miembros; 97740 bytes; SHA-256 `d5456307c5e8a4d2c4fcd3252464b96859faaa4df3ece3ca23e3c720f3b25a1d`). Manifest y `PASS_CROSS` disponibles como artifacts del run.

**Nota CI:** el run global #39531758494 concluyó FAILURE **después de pasar** ambos benchmarks, enlace cruzado y adopción: N3 Ubuntu pasó 13/13, pero N3 Windows rechazó erróneamente rutas `-L` escapadas/alias 8.3 aunque su primer ELF se enlazó. Se corrigió el gate de logs y el N3 aislado **#39538155047 terminó SUCCESS** en ambos sistemas: 13/13 sketches por runner. El FAILURE global anterior permanece como evidencia histórica del falso negativo del inspector.

---

## Histórico de decisiones y pruebas anteriores (no sustituye al estado actualizado anterior)

# Alpha13 — Gate obligatorio de precompilación y benchmark Ethernet/Modbus TCP

**Estado:** `F137_CROSS_PROFILE_GATE_IN_PROGRESS`. Los archives iniciales fueron generados y revertidos del PR por incompatibilidad de perfil; faltan aprobación binaria, medición definitiva y promoción a `JWPLC/`. **La publicación de Alpha13 se pospone** hasta cerrar este gate y revisar sus efectos.

## F137 — compatibilidad binaria FQBN (gate previo al freeze)

**Bloqueo activo:** la primera adopción automática de los dos `.a` a PR #125 fue revertida solo en la rama candidata, y el job de adopción quedó deshabilitado. Los archivos anteriores siguen en historial Git; ninguno llegó a la rama base ni al ZIP publicado.

**Prueba negativa antes del arreglo:** [CI #39516689615](https://github.com/JW-Control/platform-jwplc/actions/runs/39516689615) verificó diferencia de `.o` bajo `JWPLC_HAS_ETHERNET=1` / `0`. En Ubuntu, 1/12 objetos de Ethernet difirió (`JWPLC_Ethernet.cpp`), ModbusTCP 0/2. Windows reportó 11/12 y 2/2 con rutas de build diferentes; esa observación requiere un control de reproducibilidad y NO autoriza atribución por perfil. El gate actual recompila con la **misma ruta de build**, verifica además dos compilaciones repetidas del mismo FQBN, contrasta cada objeto, y exige en el ELF `jwplcEthernetProfileAvailable` weak en Basic y strong en Basic Core. Si falla cualquiera, `PRODUCT_ARCHIVES=BLOCKED`.

**Corrección candidata:** `JWPLC_Ethernet.cpp` no depende del `JWPLC_HAS_ETHERNET` del consumer para su código compilado. El core de `jwplcbasiccore` aporta una implementación fuerte de la política que devuelve `false`; Basic conserva la política weak habilitada. Las entradas a SPI/W5500 y el hook se detienen por la política en Core. Se conserva `core.a` sin cambio y no se modifica el autoload ni se exige editar el sketch o variant. Código funcional cambiado: exigir validación H acotada COM4 después de N3 y aprobación del operador.

**Medición actual:** las cifras de una sola pareja bajo el primer archive (Ubuntu 28,985→19,798 s en frío; Windows 59,039→43,540 s en frío para Basic) son **preliminares y corresponden al candidato revocado**. No sustituyen la tabla final. El runner actualizado mide mínimo tres parejas frío/caliente para **Basic y Basic Core**, en ambos sistemas, informa todas las muestras y mediana; no atribuye mejora de upload.

**Criterios de salida:** N0 limpio; N1 negativo reproducible; N2 paridad de cada objeto realmente archivado tras `objcopy --strip-debug` y política ELF; N3 fuentes→objetos→members `.a`→ELF sobre el package real sin compilar dos veces; benchmark 3 pares con variabilidad explícita; READMEs y checklist revisados; H mínimo aprobado/ejecutado si el cambio funcional lo exige. Solo entonces reactivar la adopción en PR #125, revisar el HEAD post-adopción y solicitar merge. No disparar release antes del merge.


### Diagnóstico Windows y normalización segura de objetos (F137)

- [CI #39518443069](https://github.com/JW-Control/platform-jwplc/actions/runs/39518443069): mismo FQBN Basic recompilado 2 veces en el mismo directorio genera 0/12 discrepancias Ethernet y 0/2 ModbusTCP. Cambiar Basic a Basic Core sigue generando diferencias crudas de 11/12 y 2/2, respectivamente.
- [CI #39519348684](https://github.com/JW-Control/platform-jwplc/actions/runs/39519348684): los archivos .o de ambos perfiles normalizados con `objcopy --strip-debug` difieren **0/12 Ethernet y 0/2 ModbusTCP**, sin cambiar fuente funcional ni toolchain. Se mantiene evidencia de los SHA originales distintos. Solo objetos normalizados e idénticos podrán entrar en el archive; el generador comprueba cada miembro extraído con su SHA y falla cerrado si existe alguna diferencia.
- [CI #39520417396](https://github.com/JW-Control/platform-jwplc/actions/runs/39520417396): Ubuntu N2 y benchmark 3 pares por perfil completados PASS, pero **run global CANCELLED** antes de finalizar Windows y enlace cruzado. Medianas Ubuntu preliminares (fría/caliente): Basic fuentes 29,175/3,902 s, archive 19,826/3,812 s; Basic Core fuentes 36,910/3,836 s, archive 28,546/3,833 s. No constituyen PASS final combinado.
- El nuevo gate de publicación exige `PASS_TWO_ARCHIVES_CANONICAL_PARITY_THREE_PAIRS`, fuentes→objetos→miembros reales del .a→ELF, N3 post-adopción de 11 sketches y 2 FQBN y verificación física mínima autorizada. El cambio se limita al PR Draft y jamás publica automáticamente.

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
NEXT_FAILURE_ID=F138
NEXT_GATE=A13_F137_FINAL_SAME_PATH_PARITY_ELF_POLICY_THREE_PAIRS_AND_N3
```
