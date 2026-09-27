# Alpha14 — H3E.1B.1 — Display archive vs source A/B

Fecha: 2026-09-27

## Objetivo

Determinar si la diferencia observada entre:

- H3E.0B: SYS_DISPLAY ~19 ms promedio;
- H3E.1: SYS_DISPLAY ~8.3 ms promedio;

se debe a que el archive precompilado de JWPLC_Display está desalineado respecto
al source actual.

No se modifica la API ni el algoritmo de render durante este gate.

## Diseño A/B

Se usa el Master H3E.0B ya validado para que la telemetría Display provenga del
core, no del profiler interno H3E.1.

Ambos legs usan:

```txt
TFT SPI = 80 MHz
RTU = 500000 / APB_FORCED
Master FIFO = 9
Slave FIFO = 8
RX = BULK
TX = QUEUED
Master framing = GAP
Slave framing = STRUCTURAL
CRC = BITWISE
TCP = 500 req/s
Display telemetry = 100 ms
Full runtime = activo
```

La única variable del Master es el linkage de JWPLC_Display.

### Leg A — ARCHIVE

```txt
libJWPLC_Display.a presente
Display source .o = 0
```

Marker requerido:

```txt
DISPLAY_LINKAGE_PROOF=ARCHIVE_NO_SOURCE_OBJECTS
```

### Leg B — SOURCE

```txt
backup libJWPLC_Display.a
hide libJWPLC_Display.a
compile source actual
restore archive exacto
```

Marker requerido:

```txt
DISPLAY_LINKAGE_PROOF=SOURCE_OBJECTS_PRESENT
```

## Duración

El runner histórico H3E.0B exige >=300 s.

Por tanto:

```txt
Leg A = 300 s
Leg B = 300 s
```

No se reduce la ventana para mantener comparabilidad con H3E.0B/H3E.1.

## Métricas principales

La comparación se centrará en:

```txt
SYS_DISPLAY AVG_US
SYS_DISPLAY MAX_US
RTU_HZ
TCP_AVG_US
TCP_P99_US
TCP_MAX_US
RTU_SERVICE_GAP_MAX_US
LOOP_GAP_MAX_US
```

## Control visual

La HMI del Master debe conservar exactamente el layout ya observado:

```txt
Rol MASTER
TCP OK <contador>
RTU OK <contador>
RTU FAIL 0
SD OK
ETH UP
```

Durante el A/B sólo TCP OK y RTU OK deben cambiar continuamente.

El usuario confirmará:

```txt
misma HMI de referencia
sin cortes
sin parpadeo anómalo
```

## Protección de artefactos

Hashes esperados:

```txt
core.a
4BFF8C8241DA2E8BD0E1BBA99835ADDF91B9085A05C4DFBD05C339B824794566

libJWPLC_ModbusRTU.a
444BE3A04079A579252B2737FE6070E00ADCA949FD176880588FE69561B2A79F

libJWPLC_Display.a
2974D42C847C1B7C7AB3A7B74DA42E2F17969FB852B47A8D434F57F70DA924AF
```

Los tres deben quedar exactamente iguales después de cada leg.

## Secuencia

Por política de un gate por vez:

1. preflight Leg A / ARCHIVE;
2. ejecutar Leg A / ARCHIVE;
3. revisar y registrar;
4. preflight Leg B / SOURCE;
5. ejecutar Leg B / SOURCE;
6. comparar resultados;
7. decidir si el archive Display debe regenerarse antes de H3E.1C.

## Interpretación

Si source es claramente más rápido con la misma HMI y carga:

```txt
archive precompilado necesita regeneración/calificación
```

Si son equivalentes:

```txt
la diferencia H3E.0B vs H3E.1 proviene de otra variable
```

Si archive es más rápido:

```txt
investigar overhead introducido en source actual antes de regenerar archive
```

No se modifica TFT_eSPI/JW_TFT durante H3E.1B.1.
