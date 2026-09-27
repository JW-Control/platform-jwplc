# Alpha14 — RTU-H3E.1 — Display Deep Attribution

Fecha: 2026-09-27

## Objetivo

Descomponer el coste de ~19–24 ms atribuido a Display por H3E.0B sin cambiar
todavía el algoritmo de render ni la API pública de `JWPLC_Display`.

H3E.0B cerró con la cadena causal:

```txt
loopTask
  -> taskYIELD
     -> jwplcSystemTask
        -> jwplcSystemDisplayHook
           -> ~19 ms promedio / ~24 ms máximo
```

El siguiente paso no es optimizar a ciegas, sino identificar exactamente qué
parte del callback Display consume ese tiempo.

## Base ESP32 real

La plataforma JWPLC 2.1.0 actual deriva de Arduino-ESP32:

```txt
3.3.8
```

Fuentes:

```txt
JWPLC/2.1.0/package.json
JWPLC/2.1.0/cores/esp32/esp_arduino_version.h
JWPLC/2.1.0/cores/jwcontrol/esp_arduino_version.h
```

Por tanto cualquier backend futuro, incluido un posible `JW_TFT` basado en
TFT_eSPI, deberá ser compatible como mínimo con Arduino-ESP32 3.3.8.

## Estado actual de la TFT

La TFT ya se configura con:

```cpp
#define JWPLC_SPI_TFT_HZ 80000000UL
```

y:

```cpp
tft.setSPISpeed(JWPLC_SPI_TFT_HZ);
```

H3E.1A/B mantiene 80 MHz.

Una comparación 40/80 MHz queda reservada para H3E.1C. Su objetivo será
distinguir coste dominado por transferencia SPI de coste dominado por software,
fragmentación de transacciones o primitivas gráficas.

## Cache / dirty actual

La API declarativa ya evita redibujar un field cuando su valor visible no
cambia.

`setValueString()` compara el string almacenado previamente. Numéricos se
formatean antes de comparar, por lo que dos valores internos distintos que
producen el mismo texto visible no disparan redraw.

`setBar()` conserva una comparación directa de valor.

H3E.1 registra por field:

```txt
SETTER_CALLS
SETTER_CHANGED
SETTER_UNCHANGED
DRAW_CALLS
```

Esto permitirá verificar físicamente la efectividad del cache.

## Pipeline instrumentado

H3E.1 mide:

```txt
REFRESH_TOTAL
PRECHECK
REFRESH_NEEDED

SPI_WAIT
  SPI_MUTEX
  SPI_PREPARE

PAGE_CLEAR
PAGE_ENTER
DRAW_STATIC
USER_CALLBACK
UI_UPDATE
DRAW_DIRTY
UI_DIRTY_CORE
INDICATOR
RELEASE_BUS
```

`SPI_MUTEX` mide específicamente `jwplcSPI_acquire(timeout)`.
El timeout del Display continúa siendo 20 ms; H3E.1 no modifica su valor.

`SPI_PREPARE` mide `jwplcSPI_prepareForTFT()`.

`SPI_WAIT` conserva el agregado para correlación con H3E.0B.

## Instrumentación por field

Para cada field realmente redibujado:

```txt
ID
TYPE
VALUE_W
VALUE_H
TEXT_LEN

DRAW_CALLS
DRAW_TOTAL_US
DRAW_MAX_US

CLEAR_TOTAL_US
CLEAR_MAX_US

ALIGN_TOTAL_US
ALIGN_MAX_US

PRINT_TOTAL_US
PRINT_MAX_US
```

Esto separa:

```txt
fillRect de la región VALUE
alineación / getTextBounds
tft.print()
otros costes
```

## Hipótesis a validar

### H1 — Mutex SPI

Si:

```txt
SPI_MUTEX ~= DISPLAY_TOTAL
```

el cuello principal es contención del bus compartido.

### H2 — Preparación SPI

Si:

```txt
SPI_PREPARE >> SPI_MUTEX
```

hay coste relevante en reconfigurar/preparar el bus para TFT.

### H3 — Render de texto fragmentado

Si:

```txt
DRAW_DIRTY ~= DISPLAY_TOTAL
PRINT >> CLEAR, ALIGN
```

el cuello principal está en el render de caracteres.

La implementación actual usa Adafruit GFX classic font. Su `drawChar()`
recorre bitmap/columnas y puede generar numerosas primitivas pequeñas. En
ESP32, `writeFillRect()` termina configurando address windows y transfiriendo
bloques pequeños. H3E.1 medirá si esa fragmentación domina en hardware real.

### H4 — Clear de región

Si:

```txt
CLEAR >> PRINT
```

la optimización debe centrarse en reducir/combinar limpieza de regiones.

## Perfil físico conservado

H3E.1 deriva directamente del Master H3E.0B validado:

```txt
RTU 500000 baud
APB_FORCED
Master FIFO 9
Slave FIFO 8
BULK RX
QUEUED TX
Master GAP
Slave STRUCTURAL
FC03 Q2
CRC BITWISE
TCP 500 req/s
full runtime
Display USER_REFRESH_ON_DEMAND
Display telemetry 100 ms
TFT SPI 80 MHz
```

H3E.0B permanece activo para correlacionar el nuevo desglose Display con
`SYS_DISPLAY`, `TASK_YIELD` y el peor gap del mismo firmware.

## Protección de archives

H3E.1 necesita compilar fuente de `JWPLC_Display`, porque la librería normal
usa `precompiled=full`.

Archive Display esperado:

```txt
JWPLC/2.1.0/libraries/JWPLC_Display/src/esp32/libJWPLC_Display.a
SHA256=2974D42C847C1B7C7AB3A7B74DA42E2F17969FB852B47A8D434F57F70DA924AF
```

El setup realiza:

```txt
backup Display archive
hide Display archive
compile Display source instrumentado
verificar objetos .o
restore archive exacto en finally
verificar SHA final
```

La misma política se conserva para el archive histórico de ModbusRTU.

El `core.a` candidato no se reemplaza.

## API pública

H3E.1 no modifica:

```txt
JWPLC_Display.setValue()
JWPLC_Display.setText()
JWPLC_Display.setBool()
JWPLC_Display.setBar()
setFields()
refresh modes
navegación
```

Toda la telemetría es interna y queda deshabilitada por default mediante hook
weak. Sólo el Master qualification aporta el hook strong que la activa.

## Alcance de la medición

La instrumentación H3E.1 añade varias llamadas a `micros()` alrededor de las
operaciones Display.

Por ello sus tiempos sirven para atribución relativa y diagnóstico. El
throughput final del producto debe volver a calificarse después de retirar o
desactivar el profiler.

## H3E.1C — pendiente posterior

Después de H3E.1A/B:

```txt
40 MHz
vs
80 MHz
```

misma carga y mismo firmware, cambiando únicamente la frecuencia TFT.

Interpretación:

```txt
tiempo casi /2 -> transferencia SPI domina
reducción parcial -> mezcla bus + software
cambio mínimo -> CPU/driver/fragmentación domina
```

## Línea futura: TFT_eSPI / JW_TFT

No forma parte de H3E.1A/B.

Después de identificar el cuello exacto se evaluará:

1. compatibilidad real de TFT_eSPI con Arduino-ESP32 3.3.8;
2. estado upstream y problemas abiertos con cores ESP32 modernos;
3. rendimiento ST7789 en ESP32 clásico y ESP32-S3;
4. SPI transactions, DMA, sprites y buffers;
5. convivencia con el bus SPI compartido JWPLC;
6. viabilidad de un backend interno `JW_TFT` mantenido por JW Control;
7. migración de `JWPLC_Display` sin romper su API pública.

La decisión de migrar backend se tomará con benchmark Adafruit actual vs
alternativa, no por percepción.


## Preflight integral

El gate exterior H3E.1 expone `-PreflightOnly`.

En ese modo:

```txt
- valida dirty scope e invariantes de hashes;
- valida contratos H3E.0B/H3E.1;
- valida sintaxis Python mediante ast.parse sin crear __pycache__;
- invoca el preflight estático del setup;
- detecta el junction Arduino15 -> repo;
- verifica políticas de source compile de ModbusRTU y Display;
- NO compila firmware;
- NO hace upload;
- NO oculta archives;
- NO modifica el package instalado.
```

El cierre requerido antes de cualquier compile físico es:

```txt
RTUH3E1_PYTHON_SYNTAX=PASS
H3E1_DISPLAY_SOURCE_CONTRACT=PASS
A14_H3E1_PREFLIGHT_ONLY=PASS
RTUH3E1_PREFLIGHT=PASS
PREFLIGHT_COMPILES=NO
PREFLIGHT_UPLOADS=NO
A14_RTU_H3E1_PREFLIGHT_ONLY=PASS
```
