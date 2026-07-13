# Mapa de memoria FRAM v1 — JWPLC Logic Runtime

## Estado

Este documento congela el **mapa v1** usado por el modo persistente del JWPLC Logic Runtime.

La asignación solo aplica cuando el usuario habilita explícitamente el runtime persistente. El package Arduino no debe formatear ni apropiarse automáticamente de la FRAM durante el uso normal de sketches.

## Principios

- El programa activo se ejecuta desde RAM.
- La FRAM conserva dos imágenes A/B del programa.
- El slot activo nunca se sobrescribe directamente.
- La zona de retentivos queda separada del programa.
- Existe una reserva explícita para evolución del formato.
- La selección del perfil depende de la capacidad detectada.
- Una capacidad menor a 8 KiB se considera no soportada.

## Constantes compartidas

```text
Área de superblocks:       64 bytes
Descriptor por slot:       32 bytes
Formato de programa:       64 + 12 × bloques
```

Los tamaños del gestor A/B y del mapa público usan las mismas constantes para evitar divergencias.

## Perfil FRAM 8 KiB

Capacidad total:

```text
8192 bytes = 0x2000
```

| Región | Inicio | Fin | Tamaño |
|---|---:|---:|---:|
| Superblocks redundantes | `0x0000` | `0x003F` | 64 B |
| Slot A | `0x0040` | `0x0A3F` | 2560 B |
| Slot B | `0x0A40` | `0x143F` | 2560 B |
| Retentivos | `0x1440` | `0x1A3F` | 1536 B |
| Reserva | `0x1A40` | `0x1FFF` | 1472 B |

Resumen:

```text
Gestor A/B:               5184 bytes
Payload útil por slot:    2528 bytes
Imagen de 100 bloques:    1264 bytes
Margen por slot:          1264 bytes
Retentivos:               1536 bytes
Reserva:                  1472 bytes
```

El límite comercial inicial se mantiene en **100 bloques**. La capacidad física del slot permitiría más, pero el límite se conserva para mantener margen de evolución, metadatos y validaciones futuras.

## Perfil FRAM 32 KiB

Capacidad total:

```text
32768 bytes = 0x8000
```

| Región | Inicio | Fin | Tamaño |
|---|---:|---:|---:|
| Superblocks redundantes | `0x0000` | `0x003F` | 64 B |
| Slot A | `0x0040` | `0x303F` | 12288 B |
| Slot B | `0x3040` | `0x603F` | 12288 B |
| Retentivos | `0x6040` | `0x703F` | 4096 B |
| Reserva | `0x7040` | `0x7FFF` | 4032 B |

Resumen:

```text
Gestor A/B:               24640 bytes
Payload útil por slot:    12256 bytes
Imagen de 400 bloques:    4864 bytes
Retentivos:               4096 bytes
Reserva:                  4032 bytes
```

El perfil de 32 KiB amplía los límites sin cambiar el motor ni el formato binario v1.

## Propiedad de la FRAM

Al habilitar el modo persistente, el runtime considera que las regiones de este mapa son de su propiedad. Por tanto:

- no debe coexistir con datos de usuario ubicados en esas mismas direcciones;
- el primer formateo debe ser explícito;
- un almacenamiento sin firma válida no debe formatearse automáticamente;
- el modo Arduino normal sigue pudiendo usar `JWPLC_FRAM` libremente mientras no se active el almacenamiento persistente del runtime.

## Comportamiento de arranque previsto

1. Detectar capacidad física.
2. Seleccionar perfil y mapa.
3. Abrir el gestor A/B sin escribir.
4. Si no existe formato válido, mantener salidas apagadas y reportar `UNFORMATTED`.
5. Si existe programa válido, cargarlo y validarlo en RAM.
6. Si el slot activo falla, intentar fallback al slot anterior.
7. Solo iniciar el scan después de completar todas las validaciones.

## API pública prevista

La integración se realizará sin romper la API ya validada. El flujo objetivo será de estilo punto:

```cpp
JWPLC_LogicRuntime runtime;

runtime.begin();
runtime.storage().begin(JWPLC_FRAM);
runtime.storage().loadActive();
runtime.start();
```

Operaciones previstas:

```text
runtime.storage().begin(...)
runtime.storage().isFormatted()
runtime.storage().format()
runtime.storage().save(...)
runtime.storage().loadActive()
runtime.storage().status()
runtime.storage().lastError()
runtime.storage().rollback()
```

La implementación de esta fachada corresponde a la siguiente etapa. El mapa queda separado para poder validarlo antes de conectar operaciones destructivas.

## Validación

Ejemplo no destructivo:

```text
JWPLC_LogicRuntime_Storage_Layout
```

El ejemplo no inicializa E/S y no accede a la FRAM. Verifica direcciones, tamaños, capacidad útil, límites de imagen y rechazo de capacidades menores a 8 KiB.
