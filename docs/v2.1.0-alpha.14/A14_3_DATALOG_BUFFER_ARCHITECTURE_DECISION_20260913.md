# Alpha14.3 - Decisión de arquitectura DataLog RAM + FRAM + microSD

Fecha: 2026-09-13

## Decisión

Se adopta como dirección de diseño para la capa de DataLog de `JW_SD` una arquitectura desacoplada entre captura lógica y escritura física a microSD.

La captura de datos desde Arduino/OpenPLC/Ladder no debe ejecutar una escritura física bloqueante a la tarjeta SD dentro del camino crítico del ciclo de usuario. Los registros se aceptarán primero en memoria rápida y la persistencia en SD se realizará de forma cooperativa y automática por bloques.

## Distribución de memoria propuesta

- Buffer principal en RAM: candidato inicial de 4096 bytes mediante ring buffer.
- Journal de seguridad en FRAM: máximo 1024 bytes en la generación actual.
- microSD: almacenamiento histórico/removible y destino final de los batches.

El límite de 1024 bytes de FRAM se fija deliberadamente porque el JWPLC Basic v2.0 dispone actualmente de 8 KB de FRAM. Una revisión futura de hardware podrá incorporar una FRAM de 32 KB, pero esta decisión no depende de ese aumento de capacidad: el journal de DataLog se mantendrá en 1 KB como máximo hasta una decisión posterior explícita.

## Semántica funcional

El usuario no deberá decidir cuándo ejecutar físicamente `File.write()` o `flush()` para cada registro.

La API de alto nivel deberá permitir que el usuario únicamente entregue registros al DataLog. La librería será responsable de:

1. Aceptar y serializar el registro.
2. Incorporarlo al ring buffer de RAM.
3. Registrar la ventana pendiente en FRAM cuando el modo seguro esté habilitado.
4. Vigilar ocupación y tiempo máximo pendiente.
5. Escribir automáticamente a microSD cuando se alcance el umbral configurado o venza el timeout de commit.
6. Confirmar/liberar en FRAM los datos ya persistidos.

## API objetivo

Se conserva la API de bajo nivel existente de `JW_SD`/`JWPLCFile` para compatibilidad Arduino.

Encima de ella se plantea una capa PLC orientada a DataLog, con conceptos equivalentes a:

- `DataLogCreate`
- `DataLogWrite`
- `REQ`
- `BUSY`
- `DONE`
- `ERROR`
- estado de registros pendientes

`DataLogWrite` debe significar "aceptar este registro para persistencia", no "forzar una escritura física inmediata a la SD".

La semántica exacta de `DONE` deberá distinguir entre dato aceptado en memoria volátil y dato ya persistido. Con journal FRAM habilitado, la intención es que `DONE` pueda representar aceptación persistente no volátil aunque la migración final a SD siga pendiente.

## Parámetros todavía no fijados

No se fijan todavía como valores finales:

- intervalo mínimo de captura;
- umbral de escritura SD;
- timeout máximo de commit;
- tamaño óptimo de batch físico;
- formato binario/CSV final de la capa DataLog.

Estos valores se decidirán con evidencia física de Alpha14.3.

## Siguiente validación

Antes de implementar la arquitectura completa se cerrará el diagnóstico de rendimiento actual:

1. confirmar el coste de la ruta física `JWPLCFile::write()` mediante dry-run;
2. comparar batches de 160, 512, 1024 y 4096 bytes;
3. elegir el tamaño/umbral que preserve el objetivo de rendimiento Modbus TCP;
4. integrar posteriormente ring buffer RAM + journal FRAM de hasta 1 KB.

## Estado

```text
DATALOG_ARCHITECTURE=DECIDED
RAM_RING_BUFFER_TARGET=4096B_CANDIDATE
FRAM_JOURNAL_MAX=1024B
FRAM_CURRENT_CAPACITY=8KB
FRAM_FUTURE_CAPACITY=32KB_PLANNED_NON_BLOCKING
SD_WRITE_POLICY=AUTO_THRESHOLD_OR_TIMEOUT
LOW_LEVEL_JW_SD_API=KEEP_COMPATIBLE
FINAL_BATCH_SIZE=PENDING_PHYSICAL_BENCHMARK
```
