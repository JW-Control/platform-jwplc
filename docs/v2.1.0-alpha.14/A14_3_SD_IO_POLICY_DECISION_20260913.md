# Alpha14.3 — decisión de política de I/O microSD

Fecha: 2026-09-13

## Estado

```text
A14_3_SD_IO_POLICY=DECIDED
SD_DEFAULT_FREQUENCY=20000000
SD_DEFAULT_FREQUENCY_CHANGE=NO
JW_SD_LIBRARY_CHANGE=NOT_REQUIRED_FOR_POLICY
FULL_RUNTIME_REALISTIC_SD_POLICY=KEEP_FILE_OPEN_FLUSH_EVERY_5_RECORDS
```

## Objetivo

Cerrar la investigación abierta por las operaciones microSD de aproximadamente 28–29 ms observadas durante `FULL_RUNTIME_REALISTIC @ 1000 req/s` y definir una política de I/O realista antes del long-run integrado.

La investigación se realizó sin modificar `JW_SD`, `SD`, `FS` ni precompilados.

## Baseline previo

A 20 MHz, la prueba inicial mostró aproximadamente:

```text
32 B + flush + close ≈ 9.25 ms
32 B + close only    ≈ 9.11 ms
100 x 32 B batch     ≈ 0.164 ms/registro
SEQ_WRITE            ≈ 966.9 KiB/s
SEQ_READ             ≈ 1266.4 KiB/s
```

Esto indicó que el principal costo no estaba en transferir 32 bytes por SPI, sino en el patrón de filesystem y sincronización.

## Sweep de frecuencia

Se probaron 4, 10, 20 y 25 MHz con la misma tarjeta y pila.

Resultados relevantes:

```text
4 MHz:
  DURABLE32_AVG_US=23380
  SEQ_WRITE_KIB_S=328.53
  SEQ_READ_KIB_S=394.95

10 MHz:
  DURABLE32_AVG_US=12555
  SEQ_WRITE_KIB_S=684.42
  SEQ_READ_KIB_S=870.18

20 MHz:
  DURABLE32_AVG_US=9217
  SEQ_WRITE_KIB_S=962.93
  SEQ_READ_KIB_S=1439.69

25 MHz:
  DURABLE32_AVG_US=9140
  SEQ_WRITE_KIB_S=1069.88
  SEQ_READ_KIB_S=1452.10
```

Interpretación:

- de 20 a 25 MHz, `DURABLE32` mejora sólo ~0.84 %;
- escritura secuencial mejora ~11.1 %;
- lectura secuencial mejora <1 %;
- 20 MHz ya está en la zona práctica de rendimiento para el patrón pequeño/durable que importa al runtime integrado.

Decisión:

```text
SD_DEFAULT_FREQUENCY=KEEP_20_MHZ
25_MHZ=NOT_JUSTIFIED_AS_DEFAULT
```

Se conserva margen eléctrico/temporal en el bus SPI compartido en vez de aumentar el reloj por una mejora marginal en la latencia de registros pequeños.

## Descomposición de latencia a 20 MHz

Prueba física con 30 iteraciones:

```text
COMP_OPEN_AVG_US=6750
COMP_OPEN_MIN_US=6340
COMP_OPEN_MAX_US=7162

COMP_WRITE32_AVG_US=30
COMP_WRITE32_MIN_US=29
COMP_WRITE32_MAX_US=52

COMP_FLUSH_AVG_US=1946
COMP_FLUSH_MIN_US=1793
COMP_FLUSH_MAX_US=5471

COMP_CLOSE_AVG_US=149
COMP_CLOSE_MIN_US=148
COMP_CLOSE_MAX_US=159

COMP_TOTAL_AVG_US=8882
COMP_TOTAL_MIN_US=8318
COMP_TOTAL_MAX_US=12173
```

Distribución aproximada del promedio total:

- `open`: ~76 %;
- `flush`: ~22 %;
- `close`: ~1.7 %;
- `write(32)`: ~0.3 %.

Conclusión: el costo dominante es abrir el archivo en cada registro. La escritura física de 32 bytes es prácticamente irrelevante en comparación.

## Archivo persistente

### Flush cada registro

```text
PERSIST_WRITE32_AVG_US=23
PERSIST_FLUSH_AVG_US=2128
PERSIST_FLUSH_MAX_US=9048
PERSIST_AMORTIZED_US_PER_RECORD=2151
```

Mejora frente a `open/write/flush/close` por registro:

```text
~8.882 ms -> ~2.151 ms
≈4.1x más rápido
```

### Flush cada 5 registros

```text
PERSIST_WRITE32_AVG_US=23
PERSIST_FLUSH_AVG_US=2710
PERSIST_FLUSH_MAX_US=5443
PERSIST_AMORTIZED_US_PER_RECORD=565
```

Mejora:

```text
~8.882 ms -> ~0.565 ms amortizado
≈15.7x más rápido
```

### Flush cada 10 registros

```text
PERSIST_WRITE32_AVG_US=23
PERSIST_FLUSH_AVG_US=3386
PERSIST_FLUSH_MAX_US=5121
PERSIST_AMORTIZED_US_PER_RECORD=362
```

Mejora:

```text
~8.882 ms -> ~0.362 ms amortizado
≈24.5x más rápido
```

## Decisión de política

Para `FULL_RUNTIME_REALISTIC`, la política seleccionada es:

```text
SD_APPEND_PERIOD_MS=1000
SD_FILE_POLICY=PERSISTENT_OPEN
SD_FLUSH_EVERY_RECORDS=5
SD_FLUSH_PERIOD_EFFECTIVE_MS≈5000
```

Razones:

1. Representa mejor un logger industrial real que abrir/cerrar el archivo cada segundo.
2. Reduce el costo amortizado por registro a ~0.565 ms.
3. Limita la ventana normal de datos todavía no sincronizados a aproximadamente 5 s.
4. Mantiene sincronización periódica real sin convertir el benchmark `REALISTIC` en un torture-test de metadata/FAT.
5. El perfil `STRESS` podrá conservar una política más agresiva para buscar límites.

No se selecciona flush cada 10 registros para `REALISTIC` porque el ahorro adicional frente a cada 5 registros es pequeño en términos absolutos (~0.203 ms amortizados) a cambio de duplicar la ventana de datos pendientes.

## Lectura bulk de JWPLCFile

La investigación confirmó que `JWPLCFile` carece actualmente de `read(uint8_t *buffer, size_t size)` protegido, mientras que sí dispone de `write(buffer,size)`.

El baseline mostró una mejora pequeña pero real al leer 32 bytes en bulk frente a byte-a-byte. Se registra como mejora de API válida, pero no es necesaria para cerrar la política de SD ni explica el costo dominante observado.

Decisión:

```text
JWPLCFILE_BULK_READ=VALID_FOLLOWUP
JWPLCFILE_BULK_READ=NOT_BLOCKING_SD_POLICY
```

Si se implementa, deberá validarse anulando temporalmente la precompilación local de `JW_SD` y posteriormente sincronizarse correctamente con `JW-Libraries` una vez corregido el flujo de sync para 2.1.0/Alpha14.

## TFT observada durante diagnósticos

La pantalla parcialmente dibujada durante `setup()` no se atribuye a contención SD.

El IDLE se construye por fases y `initPeripherals()` sólo ejecuta el primer refresh antes de entrar a `setup()`. La tarea `jwplcSystemTask`, que continúa los siguientes refreshes, espera el semáforo liberado después de que `setup()` termina.

Se registra como comportamiento de arranque/UX separado del rendimiento microSD.

## Integridad

```text
SD_POLICY_ERRORS=0
A14_SD_POLICY=PASS_PHYSICAL
BUILD_UPLOAD_EXIT=0
COMPILE_WARNINGS=0
SERIAL_READER_EXIT=0
WORKTREE_AFTER=PASS
LIBRARY_SOURCE_CHANGES=0
PRECOMPILED_OVERRIDE_REQUIRED=NO
MBLOCK_POC_TOUCHED=NO
```

## Siguiente paso

1. Ajustar únicamente el harness `FULL_RUNTIME_REALISTIC` para usar archivo SD persistente y `flush` cada 5 registros.
2. No cambiar todavía la frecuencia SD ni `JW_SD`.
3. Mantener el long-run en espera hasta cerrar `A14_3_TCP_REACCEPT_LATENCY=REVIEW_CONFIRMED`.
4. Continuar con diagnóstico físico de sockets W5500/reaccept.
