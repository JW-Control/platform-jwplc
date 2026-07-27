# Almacenamiento A/B de programas — formato preliminar v1

## Estado

```text
PoC 4
Backend RAM preparado
Backend FRAM pendiente
```

Este documento describe la organización usada para probar actualizaciones transaccionales antes de escribir en la FRAM física.

## Principio

El programa activo nunca se sobrescribe durante una actualización.

```text
Programa activo: Slot A
Nuevo candidato: Slot B
```

Secuencia:

1. Marcar el slot inactivo como `WRITING`.
2. Escribir la imagen binaria.
3. Leerla nuevamente y comprobar CRC32.
4. Marcar el slot como `VERIFIED`.
5. Escribir una nueva copia de superblock que lo declare activo.
6. Conservar el slot anterior como fallback.

Si se interrumpe la alimentación antes del paso 5, el superblock anterior continúa apuntando al programa anterior.

## Layout para el perfil de 8 KiB

```text
0x0000–0x001F   Superblock 0        32 B
0x0020–0x003F   Superblock 1        32 B
0x0040–0x0A3F   Slot A            2560 B
0x0A40–0x143F   Slot B            2560 B
```

Capacidad usada por esta PoC:

```text
64 + 2560 + 2560 = 5184 bytes
```

Quedan 3008 bytes fuera de esta región para retentivos, journal y reservas futuras. El mapa completo de la FRAM todavía no está congelado.

## Superblock

Cada copia ocupa 32 bytes.

| Offset | Tamaño | Campo |
|---:|---:|---|
| 0 | 4 | Magic `JWSB` |
| 4 | 2 | Versión |
| 6 | 2 | Tamaño de registro |
| 8 | 4 | Secuencia |
| 12 | 1 | Slot activo: 0, 1 o `0xFF` |
| 13 | 3 | Reserva |
| 16 | 4 | Program ID |
| 20 | 4 | Generación |
| 24 | 4 | Reserva |
| 28 | 4 | CRC32 de bytes 0–27 |

Durante el arranque:

1. Validar ambas copias.
2. Elegir la secuencia válida más reciente.
3. Si la más reciente está corrupta, usar la anterior.

## Descriptor de slot

Cada slot comienza con un descriptor de 32 bytes.

| Offset | Tamaño | Campo |
|---:|---:|---|
| 0 | 4 | Magic `JWSL` |
| 4 | 2 | Versión |
| 6 | 2 | Tamaño de descriptor |
| 8 | 1 | Estado |
| 9 | 1 | Índice de slot |
| 10 | 2 | Reserva |
| 12 | 4 | Generación |
| 16 | 4 | Longitud de imagen |
| 20 | 4 | CRC32 de imagen completa |
| 24 | 4 | Program ID |
| 28 | 4 | CRC32 de bytes 0–27 |

Estados iniciales:

```text
WRITING  = 1
VERIFIED = 2
```

Solo un descriptor `VERIFIED`, íntegro y coherente con la imagen puede cargarse.

## Capacidad útil por slot

```text
2560 - 32 = 2528 bytes
```

Con el formato de imagen v1:

```text
Imagen de 100 bloques = 1264 bytes
```

Por tanto, el perfil de 8 KiB mantiene margen suficiente para el límite inicial de 100 bloques.

## Recuperación

Orden de arranque:

1. Leer superblock redundante.
2. Intentar el slot activo.
3. Verificar descriptor, CRC externo y CRC interno de la imagen.
4. Si falla, intentar el otro slot verificado.
5. Si ambos fallan, no ejecutar programa y mantener salidas seguras.

## Abstracción del medio

El gestor usa `LogicByteStorage`.

Backends previstos:

```text
LogicMemoryStorage  -> pruebas e inyección de fallos
LogicFRAMStorage    -> JW_FRAM física
```

El algoritmo A/B no debe depender de comandos SPI ni del modelo de FRAM.

## Inyección de pérdida de alimentación

`LogicMemoryStorage` permite limitar cuántos bytes pueden escribirse antes de fallar.

La prueba recorre todos los puntos posibles de interrupción durante una actualización A -> B.

Resultado requerido:

```text
Escritura incompleta -> arranca A
Escritura completa   -> arranca B
Nunca                -> imagen parcial o híbrida
```

## Pendientes antes de congelar v1

- Compilar y ejecutar PoC 4.
- Implementar backend `JW_FRAM`.
- Probar cortes físicos de alimentación.
- Definir ubicación final de retentivos y journal.
- Evaluar confirmación de arranque y rollback posterior al primer scan.
- Revisar migración de formato.
