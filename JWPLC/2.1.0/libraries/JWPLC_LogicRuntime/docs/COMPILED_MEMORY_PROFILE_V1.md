# Perfil compilado de memoria v1

## Estado

```text
VALIDADO EN HARDWARE
Perfil estructural: 10 PASS, 0 FAIL
Regresión completa: 48 PASS, 0 FAIL
```

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

Cada instancia de `JWPLC_LogicRuntime` reserva en RAM:

- estados temporales del motor;
- copia profunda del programa activo;
- buffer reconstruido del almacenamiento;
- scratch del codec.

Reservar siempre 400 bloques penalizaba innecesariamente al JWPLC Basic actual, cuya FRAM es de 8 KiB y cuyo límite funcional es 100 bloques.

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
- no afecta programas de hasta 100 bloques;
- rechaza de forma segura imágenes mayores al límite compilado;
- conserva la ruta futura de 400 bloques mediante configuración explícita.

## Validación estructural

Ejemplo:

```text
JWPLC_LogicRuntime_Compiled_Profile
```

Resultado físico:

```text
Resultado: 10 PASS, 0 FAIL
PERFIL COMPILADO 100 BLOQUES: PASS
```

Compilación:

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

Límites confirmados:

```text
Bloques compilados:                100
Límite efectivo FRAM 8 KiB:        100
Límite efectivo FRAM 32 KiB:       100
Capacidad física FRAM 8 KiB:       100
Capacidad física FRAM 32 KiB:      400
```

La prueba no inicializó E/S ni leyó o escribió la FRAM.

## Regresión funcional directa

Se repitió el ejemplo completo:

```text
JWPLC_LogicRuntime_Stored_Program_Integration
```

Resultado:

```text
Resultado: 48 PASS, 0 FAIL
INTEGRACION RUNTIME PERSISTENTE: PASS
```

La prueba volvió a validar:

- clasificación `UNFORMATTED` y `EMPTY`;
- carga activa;
- copia profunda independiente de los buffers del almacenamiento;
- ejecución y parada explícitas;
- fallback al slot alterno;
- descarga segura cuando ambos programas son inválidos;
- restauración exacta de los 5184 bytes originales.

## Comparación directa 400 vs 100 bloques

Mismo sketch de integración:

| Build | Flash | RAM global | RAM restante |
|---|---:|---:|---:|
| 400 bloques | 440717 B | 51108 B | 276572 B |
| 100 bloques | 440689 B | 37908 B | 289772 B |
| Diferencia | -28 B | **-13200 B** | **+13200 B** |

El ahorro se explica exactamente por las 300 posiciones eliminadas en cuatro reservas:

```text
Definiciones internas del motor: 3600 B
Estados temporales del motor:    2400 B
Programa reconstruido:           3600 B
Scratch del codec:               3600 B
                                 ------
Total:                           13200 B
```

## Conclusión

El perfil compilado predeterminado de 100 bloques queda **cerrado y validado** para el JWPLC Basic actual.

La optimización recupera 13200 bytes de RAM en el flujo completo sin introducir regresiones, sin romper la API pública y sin cambiar el formato binario ni el mapa persistente v1.
