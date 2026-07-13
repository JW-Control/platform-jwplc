# Perfil compilado de memoria v1

## Objetivo

Separar dos conceptos que antes compartían el mismo límite:

```text
Capacidad física del formato persistente
≠
Capacidad reservada en RAM por el build
```

El mapa y el formato binario v1 continúan admitiendo:

```text
FRAM 8 KiB:  hasta 100 bloques
FRAM 32 KiB: hasta 400 bloques
```

Sin embargo, cada instancia de `JWPLC_LogicRuntime` reserva en RAM:

- estados temporales del motor;
- copia profunda del programa activo;
- buffer reconstruido del almacenamiento;
- scratch del codec.

Reservar siempre 400 bloques penaliza innecesariamente al JWPLC Basic actual, cuya FRAM es de 8 KiB y cuyo límite funcional es 100 bloques.

## Decisión v1

El build predeterminado usa internamente:

```cpp
JWPLC_LOGIC_COMPILED_MAX_BLOCKS_CONFIG = 100
```

Y conserva la constante pública existente:

```cpp
JWPLC_LOGIC_COMPILED_MAX_BLOCKS == 100
```

Un hardware futuro con FRAM de 32 KiB podrá compilar explícitamente con:

```text
-DJWPLC_LOGIC_COMPILED_MAX_BLOCKS_CONFIG=400
```

La configuración futura deberá incorporarse en el package o variante correspondiente. No se solicita al usuario normal que agregue flags manuales en Arduino IDE.

La constante pública `JWPLC_LOGIC_COMPILED_MAX_BLOCKS` se mantiene para no romper código existente que consulte el límite.

## Límite efectivo

Cada perfil calcula:

```text
min(capacidad física, capacidad compilada)
```

Build predeterminado actual:

| Perfil físico | Capacidad física | Límite compilado | Límite efectivo |
|---|---:|---:|---:|
| FRAM 8 KiB | 100 | 100 | 100 |
| FRAM 32 KiB | 400 | 100 | 100 |

Build futuro de 400 bloques:

| Perfil físico | Capacidad física | Límite compilado | Límite efectivo |
|---|---:|---:|---:|
| FRAM 8 KiB | 100 | 400 | 100 |
| FRAM 32 KiB | 400 | 400 | 400 |

Esto evita que un perfil anuncie más bloques de los que sus buffers compilados pueden almacenar.

## Compatibilidad

La reducción del límite predeterminado:

- no modifica el formato binario `JWLR`;
- no modifica el mapa FRAM v1;
- no modifica las direcciones de Slot A/B;
- conserva la constante pública del límite;
- no afecta los programas de hasta 100 bloques;
- rechaza de forma segura imágenes mayores al límite compilado;
- conserva la ruta futura de 400 bloques mediante configuración explícita.

## Motivación medida

Integración persistente compilada inicialmente para 400 bloques:

```text
Flash:       440717 bytes / 3145728 bytes (14 %)
RAM global:   51108 bytes / 327680 bytes (15 %)
RAM restante: 276572 bytes
```

El costo de la copia profunda frente al build previo fue:

```text
4832 bytes
```

Además de esa copia, los estados, el buffer reconstruido y el scratch también escalan con el límite compilado.

## Validación física del build de 100 bloques

Ejemplo no destructivo:

```text
JWPLC_LogicRuntime_Compiled_Profile
```

Resultado:

```text
Resultado: 10 PASS, 0 FAIL
PERFIL COMPILADO 100 BLOQUES: PASS
```

Compilación física:

```text
Flash:       420065 bytes / 3145728 bytes (13 %)
RAM global:   32612 bytes / 327680 bytes (9 %)
RAM restante: 295068 bytes
```

Tamaños medidos:

```text
sizeof(LogicBlockDefinition): 12
sizeof(LogicBlockState):       8
sizeof(LogicProgramBuffer):    1256
sizeof(LogicEngine):           2052
sizeof(JWPLCLogicStorage):     2588
sizeof(JWPLC_LogicRuntime):    4688
```

También se confirmó:

```text
Bloques compilados:                100
Límite efectivo FRAM 8 KiB:        100
Límite efectivo FRAM 32 KiB:       100
Capacidad física FRAM 8 KiB:       100
Capacidad física FRAM 32 KiB:      400
```

La prueba no inicializó E/S ni leyó o escribió la FRAM.

## Criterio de cierre

El perfil compilado de 100 bloques queda validado estructuralmente y en consumo de memoria. Antes de avanzar a corrupción de superblocks se repetirá la integración persistente completa con este mismo build para confirmar que:

1. `prepareStoredProgram()` conserva todos los estados de arranque;
2. la copia profunda sigue siendo independiente de los buffers de almacenamiento;
3. fallback y descarga segura mantienen el comportamiento validado;
4. la reducción de capacidad no introduce regresiones;
5. se obtiene una comparación directa de RAM con el mismo sketch usado para 400 bloques.
