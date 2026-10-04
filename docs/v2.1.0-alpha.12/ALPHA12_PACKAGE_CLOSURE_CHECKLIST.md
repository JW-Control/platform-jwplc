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

- [ ] README raíz: pasar estado Alpha11 -> Alpha12 en cierre/publicación.
- [x] JWPLC_ModbusTCP README: Server/Client/FC/coexistencia reales.
- [x] JWPLC_ModbusTCP library.properties: eliminar texto “evolucionará a Client”.
- [ ] JWPLC_ModbusRTU README: estado Alpha12 y defaults finales.
- [ ] JWPLC_RS485 README: queued TX ya cualificado.
- [ ] JWPLC_Ethernet README: hardening Alpha12 + L2 refresh + backend actual.
- [ ] JWPLC_Display README: separar estado Alpha11 histórico de cambios Alpha12.
- [ ] JWPLC_TFT README: actualizar migración H3E ya completada.
- [ ] JW_SD README: separar contenido futuro Alpha31 del cierre Alpha12.
- [ ] revisar encoding/acentos de README nuevos.
- [ ] revisar ejemplos mostrados en README contra headers reales.

## 6. Ejemplos

- [ ] compilar ejemplos ModbusTCP Server.
- [ ] compilar ejemplos ModbusTCP Client.
- [ ] revisar ejemplos ModbusRTU.
- [ ] revisar ejemplos Ethernet.
- [ ] revisar ejemplos RS485.
- [ ] asegurar que ninguno exige APIs experimentales desactivadas.

## 7. Precompilados

- [ ] congelar source final.
- [ ] determinar qué libraries/core requieren regeneración.
- [ ] regenerar archives.
- [ ] registrar SHA-256.
- [ ] registrar tamaños.
- [ ] restaurar `precompiled=full` sólo donde corresponda.
- [ ] verificar source/archive parity.
- [ ] comprobar que build final enlaza los nuevos archives.
- [ ] evitar archive stale tras cualquier cambio posterior.

## 8. Gates finales

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

## 9. Release

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

## 10. Regla de cierre

No avanzar formalmente a Alpha13 hasta:

```text
ALPHA12_STATUS=CLOSED_PUBLISHED
```

Los experimentos no productizados deben permanecer documentados, pero no deben
bloquear el release si ya existe una decisión explícita de NO PROMOTION o
DEFERRED.
