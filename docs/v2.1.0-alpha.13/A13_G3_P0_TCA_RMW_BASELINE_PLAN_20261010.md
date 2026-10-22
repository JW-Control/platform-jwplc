# Alpha13 — A13-G3-P0 — línea base del riesgo TCA RMW/shadow

Fecha: 2026-10-10. Estado: **TOOLING_VERSIONED_AWAITING_USER_RUN**.

## Objeto y frontera de seguridad

Analizar el riesgo de intercalación entre escrituras concurrentes de GPIO virtuales TCA6424A (A13-004) sin modificar firmware, archivos productivos, hardware ni periféricos. Mantener G1, G2 y TFT-CLOSURE cerrados.

Fuentes del **baseline exacto** de la rama Alpha13 antes de G3, commit `bb538fa98a26b6486bae9f2f037a6a5f1b4e5dbe`:

- `JWPLC/2.1.0/cores/jwcontrol/peripherals/src/jwplc_i2c_bridge.cpp`
- `JWPLC/2.1.0/cores/jwcontrol/peripherals/src/peripheral-tca6424a.c`

## Mecanismo identificado por inspección

`jwplcI2C_updateBit()` llama a `jwplcI2C_readReg8()` y después a `jwplcI2C_writeReg8()`. Esas dos primitivas bloquean I²C individualmente; no existe un bloqueo exterior de toda la transacción read-modify-write. `TCA6424A_writePin()` mantiene un shadow compartido sin sincronización propia; la decisión de omitir una escritura puede depender de un valor que ya no representa el hardware.

## Contrajemplo determinista (modelo anclado a las fuentes)

Registro inicial = `0x00`. Tarea A pone el bit 0 y tarea B pone el bit 1.

| Secuencia | Registro hardware final | Shadow modelado | Resultado |
|---|---:|---:|---|
| A lee, A escribe, B lee, B escribe | 0x03 | 0x03 | Control secuencial correcto |
| A lee, B lee, A escribe, B escribe | 0x02 | 0x03 | Actualización del bit 0 perdida |
| A lee, B lee, B escribe, A escribe | 0x01 | 0x03 | Actualización del bit 1 perdida |

En la segunda intercalación, una llamada posterior para volver a poner el bit 0 puede ser omitida como no-op si se confía en el shadow erróneo.

**Límite epistemológico:** P0 demuestra que el código admite esa intercalación y que el modelo la reproduce. No ejecuta dos tareas reales ni prueba todavía la frecuencia del defecto en el ESP32. No clasificar como fallo físico observado.

## Ejecución prevista

Desde la raíz de `platform-jwplc`:

```powershell
.\tools\alpha13\gates\run_a13_g3_p0_tca_rmw_baseline.bat
```


El runner exige la rama Alpha13, que el commit base sea ancestro, y byte-identidad de ambos archivos fuente frente al baseline. Valida la estructura del contrato y ejecuta los tres escenarios; escribe `SUMMARY.log` y `MANIFEST.json` en `tools/alpha13/results/g3_p0_*/`. No hace compile, upload, reset ni commit.

## Resultado esperado y siguiente frontera

```text
STATUS=PASS_BASELINE_RACE_REPRODUCED
SOURCE_CONTRACT=PASS
SEQUENTIAL_CONTROL=0x03
INTERLEAVED_AB_HARDWARE=0x02
INTERLEAVED_BA_HARDWARE=0x01
INTERLEAVED_SHADOW=0x03
PRODUCT_MUTATED=NO
PHYSICAL_EXECUTED=NO
```


Tras revisar la salida: P1 debe decidir implementación mínima que mantenga atomicidad de RMW, coherencia del shadow, no-op seguro y arbitraje entre `writePin/writeBank`. No basta con bloquear solo lectura/escritura por separado; evitar deadlocks y conservar el contrato de autoload/API. Un cambio en core requerirá reconstruir y cualificar `core.a` y luego prueba física de E/S. No cambiar OPENPLC, HMI_DESIGNER ni TFT_NEW_FEATURES.
