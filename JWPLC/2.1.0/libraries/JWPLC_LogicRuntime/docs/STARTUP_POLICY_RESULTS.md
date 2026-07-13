# Resultado de validación — política de arranque persistente v1

## Estado

```text
VALIDADO EN HARDWARE
Resultado: 44 PASS, 0 FAIL
POLITICA DE ARRANQUE: PASS
```

Compilación física:

```text
Flash:       438453 bytes / 3145728 bytes (13 %)
RAM global:   46276 bytes / 327680 bytes (14 %)
RAM restante: 281404 bytes
```

Hardware:

```text
JWPLC Basic
FRAM física: 8192 bytes
Región reversible: 0x0000..0x143F
```

## Estados validados

### `UNFORMATTED`

La FRAM restaurada sin firma válida fue detectada sin formateo automático y sin programa arrancable.

### `EMPTY`

Después de `storage().format()`, el gestor fue reconocido como válido pero sin slot activo. No quedó programa cargado.

### `ACTIVE_PROGRAM_LOADED`

Programa A y Programa B se guardaron, reconstruyeron y clasificaron correctamente como programas activos.

### `FALLBACK_PROGRAM_LOADED`

Se corrompió de forma controlada la imagen activa del Slot B. La política:

- rechazó B por integridad;
- cargó Programa A desde el Slot A;
- mantuvo B como slot indicado por el superblock;
- informó A como `lastLoadedSlot`;
- no escribió ni reparó automáticamente la FRAM.

### `NO_VALID_PROGRAM`

Después de corromper también la imagen A, la política:

- rechazó ambos slots;
- descargó el programa reconstruido;
- informó que no existía un programa arrancable;
- mantuvo las salidas sin ejecución.

## Restauración

Los 5184 bytes originales del gestor A/B se respaldaron en NVS con CRC32 y fueron restaurados exactamente al finalizar. La fachada volvió a detectar el estado original `UNFORMATTED` y se eliminó el respaldo temporal.

## Conclusión

La política de arranque persistente v1 queda validada físicamente para:

```text
UNFORMATTED
EMPTY
ACTIVE_PROGRAM_LOADED
FALLBACK_PROGRAM_LOADED
NO_VALID_PROGRAM
```

La siguiente etapa integra esta clasificación directamente con `JWPLC_LogicRuntime`, manteniendo `start()` como operación explícita y evitando que un fallo de carga deje ejecutable un programa anterior.
