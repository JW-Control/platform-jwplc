# Alpha13-G8-P0 — Baseline de Modbus RTU y RS-485 (2026-10-11)

**Estado:** PLAN_READ_ONLY. Ejecutar después de G7C `CLOSED_PASS`.

## Objetivo

Caracterizar, sin editar archivos productivos ni acceder al puerto serial,
las correcciones canónicas:

- **A13-010**: `JWPLC_ModbusRTU.cpp`, contador `_stats.rxBytes` en
  Slave/Master y bulk/no-bulk. En el baseline, Slave no-bulk suma
  dos `rxBytes++` consecutivos por byte aceptado (líneas 493–494),
  mientras Master no-bulk no suma `rxBytes` en la ruta de aceptación
  (alrededor de línea 905). No alterar parsing, CRC ni tiempos RTU.
- **A13-011**: `JWPLC_RS485.cpp`, `begin()` llama
  `_serial->begin(...)` y fija `_ready=true` sin verificar que
  exista el driver UART. Debe definirse contrato de fallo/recuperación
  conservando firmas públicas y status/error existente.

## Política source→objeto→archive

- `JWPLC_ModbusRTU`: `precompiled=full`; el archive productivo
  `src/esp32/libJWPLC_ModbusRTU.a` se debe regenerar si cambia fuente.
  Un parche solo a `.cpp` NO completa A13-010.
- `JWPLC_RS485`: biblioteca desde fuente, sin `precompiled=full`;
  comprobar que compile `JWPLC_RS485.cpp.o` en compilación normal.
- `HardwareSerial` forma parte del core ESP32; G8 no lo modifica.
- Baseline debe preservar `jwplc_local:esp32:jwplcbasic`, `core.a`,
  `libJWPLC_TFT.a`, FRAM G7C, autoload de todos los periféricos,
  RS485 AutoDirection MAX13487E y APIs de usuario.

## P0 — Evidencia mínima

1. Comprobar rama/HEAD/worktree, Arduino CLI oficial 1.5.1,
   package local, hashes fuentes/archive, core/TFT y FRAM de G7C.
2. Constatar los dos patrones defectuosos directamente en fuentes
   productivas; no suponer que todavía existan.
3. Compilar sketches normales Slave Holding y Master Read y un
   consumidor RS-485; registrar rutas reales de bibliotecas,
   presencia de objeto RS-485 y enlace `-L/-lJWPLC_ModbusRTU`.
4. Guardar `MANIFEST.json`, `SUMMARY.log` y logs por fase.
   No hacer upload, escribir FRAM, usar COM ni hacer commits.
5. Clasificar PASS/REVIEW/FAIL y detener ante incongruencias.
   PASS en P0 es solo **baseline reproducible**, no corrección.

## Próximas fases tras P0

- **P1:** fixtures fallos RX, Slave/Master × bulk/no-bulk,
  lecturas 0/negativas, overflow y UART begin/end/begin fallido.
- **P2:** candidatos mínimos, reconstrucción del archive RTU,
  paridad objeto/miembro, enlaces exactos, regresiones/autoload.
- **P3:** decidir prueba física RS485, con entorno de banco seguro
  y peer real si se requiere; nunca inventar un PASS.
- **P4:** adopción productiva y cierre Git únicamente tras pruebas.

```text
G7C=CLOSED_PASS
G8_P0=READY_FOR_READ_ONLY_IMPLEMENTATION
A13-010=PENDING
A13-011=PENDING
G8_PRODUCT_CHANGED=NO
G8_HARDWARE_ACCESS=NO
OPENPLC=OUT_OF_SCOPE
HMI_DESIGNER=OUT_OF_SCOPE
TFT_NEW_FEATURES=OUT_OF_SCOPE
RELEASE_MERGE_PR_PUBLICATION=NO
```

Referencia de alcance: `A13_G7_CANONICAL_RECONCILIATION_AND_G8_SCOPE_20261011.md`.
