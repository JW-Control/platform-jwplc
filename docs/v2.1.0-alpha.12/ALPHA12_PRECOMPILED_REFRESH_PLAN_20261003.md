# Alpha12 — plan de refresh de precompilados

Fecha: 2026-10-03

## Objetivo

Cerrar la divergencia entre sources actuales y artifacts precompilados antes del
freeze final del package Alpha12.

Regla principal:

```text
SOURCE_CHANGE
-> ARCHIVE_INVALIDATED
-> SOURCE-FIRST PASS
-> REBUILD
-> ARCHIVE PARITY/LINK
-> PHYSICAL/FINAL GATE
-> ONLY THEN RELEASE
```

Referencia de prevención:

```text
F050=PRECOMPILED_CORE_STALE_AFTER_RUNTIME_SOURCE_CHANGE
F081=BACKEND_MIGRATION_LEFT_DISTRIBUTED_CONSUMERS_STALE
```

## Matriz de artifacts

| Artifact | Estado respecto al source | Acción Alpha12 |
|---|---|---|
| `precompiled/core/JWPLCBASIC/core.a` | STALE | REBUILD obligatorio |
| `JWPLC_Display/src/esp32/libJWPLC_Display.a` | STALE | REBUILD obligatorio |
| `JWPLC_ModbusRTU/src/esp32/libJWPLC_ModbusRTU.a` | STALE | REBUILD obligatorio |
| `JW_SD/src/esp32/libJW_SD.a` | STALE | REBUILD obligatorio |
| `SPI/src/esp32/libSPI.a` | STALE | REBUILD obligatorio |
| `JWPLC_TFT/src/esp32/libJWPLC_TFT.a` | source/archive sincronizados | REQUALIFY; rebuild sólo si falla |
| `JW_FRAM/src/esp32/libJW_FRAM.a` | source sin cambios Alpha12 | RETAIN + audit |
| `JW_MatrixButtons/src/esp32/libJW_MatrixButtons.a` | source sin cambios Alpha12 | RETAIN + audit |
| `SD/src/esp32/libSD.a` | source sin cambios Alpha12 | RETAIN + audit |
| `FS/src/esp32/libFS.a` | source sin cambios Alpha12 | RETAIN + audit |
| `Wire/src/esp32/libWire.a` | source sin cambios Alpha12 | RETAIN + audit |
| Adafruit base archives | source sin cambios Alpha12 | RETAIN + audit |

## Evidencia de staleness

### core.a

Último cambio del directorio source:

```text
2026-10-01
7bc000100b2b3016e7cbcfb0e3faf0103071b646
feat(alpha14): integrar candidato INT interno Modbus TCP
```

Archive actual:

```text
2026-09-28
f1648654eac265de64aefd1ea601c28f01db9db8
build(alpha14): adoptar core y Modbus RTU precompilados cualificados
```

```text
CORE_ARCHIVE=STALE
```

Aunque INT queda OFF por default, el archive final debe corresponder al source
actual, incluyendo defaults OFF y hooks/runtime actuales.

### JWPLC_Display

Último cambio source:

```text
2026-09-29
04cff2e214f2c529decdfae4dd7c81b8742a6717
Update JWPLC_IdleScreen.cpp
```

Archive:

```text
2026-09-28
34f8ba28af4c54cdcdcfdc4e22f9d3680fc7dff4
feat(alpha14): adoptar JWPLC_Display precompilado sobre JWPLC_TFT
```

```text
DISPLAY_ARCHIVE=STALE
```

### JWPLC_ModbusRTU

Último source:

```text
2026-10-02
08902b37ee7fc707bf1a01ddc75f56c9332c4424
fix(alpha14): ampliar hold de parciales RTU FAST
```

Archive:

```text
2026-09-28
f1648654eac265de64aefd1ea601c28f01db9db8
```

```text
MODBUS_RTU_ARCHIVE=STALE
```

### JW_SD

Último source:

```text
2026-09-14
b8b6b18f179edca317054b734ac06da8b8b9715e
fix(alpha14): recuperar DataLog tras reinsercion SD
```

Archive:

```text
2026-08-10
fc29e265ae94637a07fbf0360d246a4a70636ff8
perf(build): consolidar librerías precompiladas P1-P6
```

```text
JW_SD_ARCHIVE=STALE
```

### SPI

Último source:

```text
2026-10-01
ebf0982ad0121db7ed9febdc2ca6870f1c77bf7f
perf(alpha14): promover COPY_OUT 64B al default
```

Archive:

```text
2026-08-11
82fe109432f78abfc54f098c66fc237e8a7d8c7e
perf(build): precompilar Wire y SPI para P8
```

```text
SPI_ARCHIVE=STALE
```

## JWPLC_TFT

El último commit del directorio `JWPLC_TFT/src` coincide con la adopción del
archive de shapes:

```text
2026-09-28
53b6ef0d2be2ef2391dc85ec58dc0f8de06d5984
feat(alpha14): adoptar primitivas shapes precompiladas en JWPLC_TFT
```

Los cambios Alpha12 posteriores afectaron consumers de Display/LogicRuntime_UI,
no `JWPLC_TFT.cpp`.

Por tanto:

```text
JWPLC_TFT_FUNCTIONAL_REBUILD_REQUIRED=NO
JWPLC_TFT_REQUALIFICATION_REQUIRED=YES
```

El archive histórico es autocontenido:

```text
JWPLC_TFT.cpp.o
TFT_eSPI.cpp.o
```

y el package final debe mantener:

```text
EXTERNAL_TFT_ESPI_REQUIRED_AT_USER_BUILD=NO
```

La source-first qualification sí requiere disponer de TFT_eSPI 2.5.43 en el
entorno del mantenedor. En el gate del 03-oct se resolvió desde:

```text
C:\Users\jeykc\Documentos\Programacion\Arduino\libraries\TFT_eSPI
version=2.5.43
```

Eso es una dependencia de mantenimiento/build de source, no del usuario final.

## Orden de refresh

Un artifact por vez:

```text
P1 CORE
P2 JWPLC_ModbusRTU
P3 SPI
P4 JW_SD
P5 JWPLC_Display
P6 JWPLC_TFT requalification
P7 audit global archives
P8 compile parity / examples
P9 Arduino IDE + upload físico final
```

Razón:

- core primero porque define runtime/autoservice/hooks;
- RTU/SPI antes de coexistencia final de libraries;
- SD después de core por DataLog autoservice;
- Display después de TFT source-first gate ya cerrado;
- TFT sólo requalify si su identidad/paridad sigue válida.

## Política de properties

Durante desarrollo source-first:

```text
JWPLC_Display precompiled=full absent
JWPLC_TFT precompiled=full absent
JWPLC_ModbusRTU precompiled=full absent
JW_SD precompiled=full absent
SPI precompiled=full absent
```

No restaurar `precompiled=full` de forma global antes de que el artifact
correspondiente pase:

1. generación desde source actual;
2. member/object parity;
3. compile/link usando archive;
4. source object count = 0 en modo precompiled;
5. hash/size registrados.

## Gate ya cerrado

```text
ALPHA12_TFT_BACKEND_COMPILE=PASS
PASS=4
FAIL=0
SOURCE_FIRST=PASS
```

Esto desbloquea preparar precompilados, no publicarlos automáticamente.
