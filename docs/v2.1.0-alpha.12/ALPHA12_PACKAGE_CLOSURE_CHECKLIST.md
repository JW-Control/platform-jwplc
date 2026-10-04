# Alpha12 — checklist de cierre del package

Fecha: 2026-10-03

Estado:

```text
ALPHA12_PACKAGE_CLOSURE=IN_PROGRESS
```

## 1. Identidad y alcance

- [x] Alpha11 confirmado como último release publicado previo.
- [x] trabajo histórico Modbus/Ethernet renumerado a release Alpha12.
- [x] rama canónica Alpha12 creada.
- [x] evidencia histórica Alpha14 preservada sin reescritura.
- [x] Alpha13 reservado para actualización TFT/Display.
- [x] Alpha14 reservado para OpenPLC + integración sobre TCP/RTU optimizados.

## 2. Inventario productivo

- [x] inventariar cambios de core.
- [x] inventariar JWPLC_Ethernet/W5500.
- [x] inventariar JWPLC_ModbusTCP.
- [x] inventariar JWPLC_ModbusRTU.
- [x] inventariar JWPLC_RS485.
- [x] inventariar SPI.
- [x] inventariar JW_SD/DataLog.
- [x] inventariar JWPLC_Display/JWPLC_TFT.
- [x] marcar cada cambio PRODUCT / INTERNAL / BENCHMARK_ONLY / DEFERRED.
- [x] confirmar defaults finales.
- [x] confirmar autoload final.

## 3. API pública y configuración

- [x] listar APIs nuevas de Modbus TCP Server.
- [x] listar APIs nuevas de Modbus TCP Client.
- [x] listar APIs RTU ASYNC/SYNC.
- [x] listar API RTU microsecond frame gap.
- [x] listar API RS485 queued TX y diagnóstico.
- [x] listar APIs Ethernet/W5500 aditivas expuestas.
- [x] listar APIs UDP fast que sí son soportadas.
- [x] separar APIs internas/qualification de APIs de usuario.
- [ ] confirmar backward compatibility.
- [ ] decidir/documentar compatibilidad raw TFT: `Adafruit_ST7789&` explícito -> `JWPLC_TFTClass&`.
  - Estado: consumers internos/oficiales migrados; compatibilidad de sketches externos con tipo explícito sigue siendo decisión de release.

## 4. Decisiones técnicas finales

- [x] TCP RX policy = POLLING C0.
- [x] INT/D2/D3/E1 defaults = OFF.
- [x] W5500 SPI = 26 MHz en perfil actual validado.
- [x] FIFO reuse = ON.
- [x] DLEN reuse = ON.
- [x] COPY_OUT_64 = ON.
- [x] UDP fast path no reemplaza API Arduino legacy.
- [x] single-CS UDP no productizado.
- [x] RTU ASYNC recomendado / SYNC compatibilidad.
- [x] coexistencia TCP250 + RTU800 + UDP1M LR600 confirmada.
- [x] RTU100Hz = contrato operacional, no hard real-time.
- [ ] decisión final sobre policy de precompilación por librería.
- [ ] registrar cualquier decisión aún diferida explícitamente.

## 5. README/library.properties

- [x] README raíz: pasar estado Alpha11 -> Alpha12 en cierre/publicación.
- [x] JWPLC_ModbusTCP README: Server/Client/FC/coexistencia reales.
- [x] JWPLC_ModbusTCP library.properties: eliminar texto “evolucionará a Client”.
- [x] JWPLC_ModbusRTU README: estado Alpha12 y defaults finales.
- [x] JWPLC_RS485 README: queued TX ya cualificado.
- [x] JWPLC_Ethernet README: hardening Alpha12 + L2 refresh + backend actual.
- [x] JWPLC_Display README: separar estado Alpha11 histórico de cambios Alpha12.
- [x] JWPLC_TFT README: actualizar migración H3E ya completada.
- [x] JW_SD README: separar contenido futuro Alpha31 del cierre Alpha12.
- [x] revisar encoding/acentos de README nuevos.
- [ ] revisar ejemplos mostrados en README contra headers reales.

## 6. Ejemplos

- [x] migrar ejemplos Display actuales de `ST77XX_*` a `JWPLC_TFT_*`.
- [x] auditar `JWPLC_Display/src` contra referencias Adafruit/ST77XX obsoletas.
- [x] auditar `JWPLC_LogicRuntime_UI` contra referencias Adafruit/ST77XX obsoletas.
- [x] añadir `JWPLC_LogicRuntime_UI` al smoke CI.
- [x] ejecutar `alpha12_tft_backend_compile.ps1` source-first.
- [ ] compilar ejemplos ModbusTCP Server.
- [ ] compilar ejemplos ModbusTCP Client.
- [ ] revisar ejemplos ModbusRTU.
- [ ] revisar ejemplos Ethernet.
- [ ] revisar ejemplos RS485.
- [ ] asegurar que ninguno exige APIs experimentales desactivadas.

## 7. Precompilados

- [ ] congelar source final.
- [x] bloquear regeneración de Display/TFT hasta PASS del gate source-first.
- [x] determinar qué libraries/core requieren regeneración.
- [x] P1: regenerar y verificar `core.a` actual.
- [x] P2: regenerar y verificar `libJWPLC_ModbusRTU.a` actual.
- [x] P3: regenerar y verificar `libSPI.a` actual.
- [x] P4: regenerar y verificar `libJW_SD.a` actual.
- [x] P5: regenerar y verificar `libJWPLC_Display.a` actual.
- [x] P6: requalificar `libJWPLC_TFT.a` y demostrar selección autocontenida del backend TFT.
- [x] cerrar P6 sin regenerar el archive histórico físicamente cualificado.
- [ ] P7: auditar globalmente archives retenidos/regenerados y congelar el conjunto final.
- [ ] registrar tabla consolidada SHA-256 de todos los archives finales.
- [ ] registrar tabla consolidada de tamaños de todos los archives finales.
- [ ] restaurar `precompiled=full` sólo donde corresponda.
- [ ] verificar source/archive parity.
- [ ] comprobar que build final enlaza los nuevos archives.
- [ ] evitar archive stale tras cualquier cambio posterior.



Resultado P6:

```text
P6_TFT_REQUALIFICATION=PASS_CLOSED
ARCHIVE_SHA256=5d860a131811dd9a7eb6fa55f5674b1d78b0de7dfaf8748ce18a60ceed2d3738
STRUCTURAL_EQUIVALENCE=PASS
GLOBAL_TFT_ESPI_REQUIRED_AT_USER_BUILD=NO
OFFICIAL_ARCHIVE_CHANGED=NO
```

El benchmark final permanece bloqueado hasta completar P7 y declarar el freeze
global de precompilados.

## 8. Benchmark final de tiempos de compilación

Ubicación obligatoria en la secuencia:

```text
P6_TFT_REQUALIFICATION
-> P7_GLOBAL_PRECOMPILED_ARCHIVE_AUDIT
-> PRECOMPILED_FREEZE
-> FINAL_BUILD_SPEED_BENCHMARK
-> FINAL_CLI_IDE_UPLOAD_GATES
-> RELEASE
```

No ejecutar el benchmark oficial antes de P6: cualquier cambio posterior en un
archive invalidaría la tabla de tiempos.

Metodología a reutilizar:

```text
tools/build-speed-benchmark/Run-JWPLCBuildBenchmark.ps1
```

Matriz mínima Alpha12:

- [ ] HEAD funcional/precompilados congelados antes de medir.
- [ ] working tree tracked limpio.
- [ ] sketch `01_empty` con autoload normal completo.
- [ ] target `JWPLC Basic`.
- [ ] target `JWPLC Basic Core` como control source.
- [ ] `Jobs=0`.
- [ ] `managed_cold`.
- [ ] `managed_warm_nochange`.
- [ ] `managed_warm_touch`.
- [ ] `explicit_cold`.
- [ ] `explicit_warm_nochange`.
- [ ] `explicit_warm_touch`.
- [ ] registrar `CompilerInvocations`/TUs.
- [ ] registrar tamaño de binarios.
- [ ] registrar Arduino CLI, CPU/RAM, host y commit exacto.
- [ ] generar tabla final Alpha12.
- [ ] comparar contra referencias históricas Alpha4/Alpha5 sólo cuando host/metodología sean comparables.
- [ ] documentar por separado cualquier resultado de upload; no mezclar tiempo de compilación con tiempo de carga.
- [ ] confirmar que la mejora no proviene de retirar periféricos del autoload.

Referencias históricas de metodología:

```text
tools/build-speed-benchmark/README.md
tools/build-speed-benchmark/BASELINE_ALPHA3_INSTALLED_20260809.md
docs/v2.1.0-alpha.5/BUILD_SPEED_COMPARISON_ALPHA4_ALPHA5_FINAL_20260824.md
```

Resultado requerido antes de avanzar a gates finales:

```text
ALPHA12_FINAL_BUILD_SPEED_BENCHMARK=PASS
ALPHA12_BUILD_SPEED_TABLE=RECORDED
```

---

## 9. Gates finales

- [x] ceilings TCP/UDP/RTU.
- [x] coexistencia final >=600 s.
- [x] Arduino IDE físico histórico post-H3E.
- [ ] Arduino CLI final del HEAD congelado.
- [ ] Arduino IDE final del HEAD congelado.
- [ ] upload físico final.
- [ ] normal autoload: Display/RTC/FRAM/SD/buttons/DI/DO/Ethernet/RS485.
- [ ] regresión Modbus TCP Server/Client mínima.
- [ ] regresión Modbus RTU mínima.
- [ ] working tree / diff / conflict markers clean.

## 10. Release

- [ ] conclusión técnica Alpha12.
- [ ] PRE_RELEASE.md en español.
- [ ] README release marker = 2.1.0-alpha.12.
- [ ] PR Alpha12 en español.
- [ ] CI final HEAD.
- [ ] merge a release/v2.1.x.
- [ ] artefacto ZIP.
- [ ] SHA-256 + size.
- [ ] GitHub PreRelease.
- [ ] package_jwplc_index_dev.json.
- [ ] isolated install desde índice dev.
- [ ] isolated compile.
- [ ] isolated physical upload.
- [ ] runtime/TFT post-upload.
- [ ] registrar topología release/main.
- [ ] cierre documental final.

## 11. Regla de cierre

No avanzar formalmente a Alpha13 hasta:

```text
ALPHA12_STATUS=CLOSED_PUBLISHED
```

Los experimentos no productizados deben permanecer documentados, pero no deben
bloquear el release si ya existe una decisión explícita de NO PROMOTION o
DEFERRED.
