# Estrategia de precompilación — v2.1.0-alpha.4

## Objetivo

Reducir el tiempo de compilación del JWPLC Basic sin sacrificar la experiencia de autoload:

```text
sketch vacío
-> Arduino.h
-> JWPLC autoload
-> Display + periféricos integrados
```

No se busca obtener un benchmark rápido quitando periféricos.

La meta es que las partes estables del ecosistema que no cambian entre cargas se compilen una vez y se reutilicen.

---

## Estado de Basic y Basic Core

Ambos perfiles usan:

```text
build.mcu=esp32
build.core=jwcontrol
build.variant=jwplcbasic
-DJWPLC_BASIC
-DHAVE_TCA6424A
```

Pero no tienen exactamente las mismas macros de hardware.

### Basic

```text
JWPLC_HAS_RTC=1
JWPLC_HAS_FRAM=1
JWPLC_HAS_SD=1
JWPLC_HAS_ETHERNET=1
JWPLC_FRAM_SIZE_BYTES=8192
```

### Basic Core

```text
JWPLC_HAS_RTC=1
JWPLC_HAS_FRAM=0
JWPLC_HAS_SD=0
JWPLC_HAS_ETHERNET=0
JWPLC_FRAM_SIZE_BYTES=0
```

Por tanto, que ambos boards carguen las mismas librerías no significa que cualquier binario precompilado pueda compartirse ciegamente.

---

## Regla principal

### Sí

Compartir una sola librería precompilada entre Basic y Basic Core cuando su código generado sea independiente de las macros de perfil.

### No

Compilar una librería con macros de Basic y enlazar ese mismo `.a` en Basic Core si esas macros modifican el comportamiento interno.

El ejemplo principal es actualmente:

```text
JWPLC_GlobalPeripherals.cpp
```

que contiene bloques como:

```cpp
#if JWPLC_HAS_FRAM
...
#endif

#if JWPLC_HAS_SD
...
#endif
```

Estas decisiones quedan fijadas durante compilación.

---

## Soporte Arduino

La especificación de librerías Arduino permite:

```text
precompiled=full
```

Cuando existe un binario compatible, los fuentes de esa librería no se compilan. Si el binario no existe, los fuentes actúan como fallback.

Para el MCU actual, los archivos se ubican conceptualmente como:

```text
<library>/src/esp32/libNombre.a
```

porque:

```text
build.mcu=esp32
```

También existe:

```text
precompiled=true
```

para librerías mixtas, donde el binario precompilado se enlaza pero los fuentes restantes continúan compilándose.

El `platform.txt` JWPLC actual ya incorpora `compiler.libraries.ldflags` en la receta de enlace, requisito necesario para el mecanismo estándar de librerías precompiladas.

---

## Estrategia propuesta

### Fase P0 — Baseline

Antes de añadir ningún `.a`:

1. ejecutar `01_empty` en Basic;
2. ejecutar `01_empty` en Basic Core;
3. guardar cold/warm/touch;
4. guardar upload full/app-only;
5. conservar logs verbose;
6. contar invocaciones reales del compilador.

No implementar precompilación antes de tener esta base.

---

## Fase P1 — Librerías JWPLC comunes

Primero evaluar librerías que pueden ser idénticas para Basic y Basic Core.

| Librería | Candidato inicial | Riesgo esperado |
|---|---:|---:|
| `JW_RTC` | Sí | Bajo |
| `JW_FRAM` | Sí | Bajo |
| `JW_MatrixButtons` | Sí | Bajo |
| `JW_SD` | Sí | Medio |
| `JWPLC_RS485` | Sí | Bajo |
| `JWPLC_ModbusRTU` | Sí | Medio |
| `JWPLC_Ethernet` | Sí | Medio |
| `JWPLC_Display` | Evaluar | Medio |
| `JWPLC_GlobalPeripherals` | No como un único `.a` todavía | Alto |

La tabla es una clasificación de trabajo, no una aprobación. Los logs del baseline deben confirmar cuáles tienen peso real en `01_empty`.

Las librerías LogicRuntime/LogicRuntime_UI no forman parte del objetivo inicial si no aparecen en el grafo real del sketch vacío.

---

## Fase P2 — GlobalPeripherals

`JWPLC_GlobalPeripherals` necesita tratamiento especial porque combina:

- objetos globales;
- inicialización automática;
- SPI compartido;
- FRAM;
- SD;
- RTC;
- botones;
- Ethernet;
- decisiones condicionadas por `JWPLC_HAS_*`.

Se evaluarán dos diseños.

### Opción A — Adaptador pequeño por perfil

Separar el código en:

```text
GlobalPeripherals común pesado
+ adaptador de perfil pequeño compilado normalmente
```

El código común podría precompilarse una sola vez para ESP32.

El adaptador conservaría las decisiones:

```text
Basic
Basic Core
```

Esta es la opción preferida si el coste de `JWPLC_GlobalPeripherals` resulta significativo.

### Opción B — Configuración en runtime

Compilar un único superset y decidir en runtime qué hardware está habilitado.

Ventaja:

```text
un solo binario común
```

Riesgos:

- mayor tamaño final;
- posibles referencias a hardware ausente;
- cambio de arquitectura respecto del estado probado;
- mayor superficie de regresión.

No adoptar esta opción sólo para simplificar la precompilación.

---

## Fase P3 — Display y dependencias externas

`JWPLC_Display` arrastra Adafruit GFX/ST7789.

Primero debe medirse cuánto tiempo corresponde a:

```text
JWPLC_Display
Adafruit GFX
Adafruit ST7735/ST7789
```

No modificar o redistribuir dependencias externas como binarios precompilados sin revisar antes:

- licencia;
- versión exacta;
- mecanismo de actualización;
- compatibilidad con Arduino IDE;
- impacto del package.

Si estas librerías dominan el cold/warm touch, se abre una fase específica.

---

## Reglas de ABI para un `.a`

Un archivo precompilado debe considerarse inválido y reconstruirse si cambia cualquiera de estos elementos relevantes:

- versión de toolchain;
- versión de `esp32-libs`;
- flags C/C++;
- `-Os` / optimización;
- macros que afecten al código;
- headers públicos usados por el binario;
- versión de la librería;
- core JWPLC;
- estructura de clases/structs;
- dependencias enlazadas;
- configuración de excepciones/RTTI si aplicara.

Para Alpha4, la referencia actual del package usa:

```text
esp-x32: 2601
esp32-libs: 3.3.8
MCU: esp32
CPU: 240 MHz
```

No generar binarios manualmente con un conjunto de flags aproximado.

El futuro script de precompilación debe obtener o reproducir exactamente las recetas reales de Arduino CLI/platform.txt.

---

## Política de fallback

Siempre que sea viable se prefiere:

```text
precompiled=full
+ fuentes presentes como fallback
```

Esto permite que una configuración no cubierta por el binario pueda seguir compilando desde fuente en lugar de fallar silenciosamente.

No retirar los fuentes originales durante Alpha4.

---

## Comparación requerida

Cada experimento de precompilación debe repetirse con exactamente la misma matriz:

```text
managed_cold
managed_warm_nochange
managed_warm_touch
explicit_cold
explicit_warm_nochange
explicit_warm_touch
upload_full
upload_app_only
```

Y comparar:

- tiempo;
- invocaciones del compilador;
- tamaño de binario;
- éxito de link;
- arranque físico;
- Display;
- RTC;
- FRAM;
- SD;
- Ethernet;
- botones;
- RS-485;
- Modbus RTU;
- I/O TCA6424A.

---

## Criterio de éxito práctico

La métrica prioritaria no será únicamente el cold build.

El caso de producto que debe mejorar es:

```text
abrir sketch
-> editar una línea
-> Compilar/Subir
-> probar en JWPLC Basic
```

Por ello `managed_warm_touch` y el tiempo combinado de recompilación + upload tienen prioridad para decidir si la optimización merece entrar a Alpha4.

---

## Estado

```text
Baseline: pendiente de ejecutar en PC real
Precompilación: no implementada todavía
App-only: benchmark experimental preparado
Bootloader precompilado: pendiente de medición; no definitivo
FlashFreq: sin decisión final nueva
```
