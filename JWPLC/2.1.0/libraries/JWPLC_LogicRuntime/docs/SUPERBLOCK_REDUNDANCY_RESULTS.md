# Resultado de validación — redundancia de superblocks

## Estado

```text
VALIDADO EN HARDWARE
Resultado: 51 PASS, 0 FAIL
REDUNDANCIA DE SUPERBLOCKS: PASS
```

## Compilación física

```text
Flash:       439501 bytes / 3145728 bytes (13 %)
RAM global:   38036 bytes / 327680 bytes (11 %)
RAM restante: 289644 bytes
```

## Hardware

```text
JWPLC Basic
FRAM física: 8192 bytes
Región reversible: 0x0000..0x143F
Superblock 0: 0x0000..0x001F
Superblock 1: 0x0020..0x003F
```

## Flujo validado

1. Se respaldaron los 5184 bytes del gestor A/B en NVS con CRC32.
2. Se formateó explícitamente el almacenamiento.
3. Se guardó Programa A en Slot A mediante la copia 1, secuencia 2.
4. Se guardó Programa B en Slot B mediante la copia 0, secuencia 3.
5. Programa B se cargó correctamente antes de las inyecciones.
6. Se corrompió el CRC de la copia más reciente, superblock 0.
7. La copia 1 recuperó la metadata anterior y cargó Programa A.
8. Se restauraron exactamente ambos superblocks.
9. Se corrompió el CRC de la copia anterior, superblock 1.
10. La copia 0 conservó la metadata más reciente y cargó Programa B.
11. Se restauraron nuevamente ambos superblocks.
12. Se corrompieron los CRC de ambas copias.
13. El backend permaneció operativo, pero el gestor no declaró formato válido.
14. No quedó programa cargado ni arrancable.
15. Se restauró la metadata y Programa B volvió a quedar activo.
16. Se restauraron exactamente los 5184 bytes originales.
17. Se recuperó el estado original sin formato y se eliminó el respaldo NVS.

## Propiedades confirmadas

- Una copia válida basta para recuperar una decisión transaccional.
- Cuando ambas copias son válidas se selecciona la secuencia más reciente.
- Corromper la copia nueva recupera la última decisión anterior íntegra.
- Corromper la copia antigua no afecta la decisión nueva.
- Perder ambas copias no provoca formateo automático ni ejecución de slots.
- Los descriptores y las imágenes A/B permanecieron intactos durante las inyecciones.
- La prueba no inicializó E/S ni conmutó salidas.
- La restauración recuperó byte por byte el contenido original.

## Decisión posterior a la prueba

El comportamiento seguro queda confirmado, pero el diagnóstico público `UNFORMATTED` resulta insuficiente para dos situaciones diferentes:

```text
FRAM sin metadata reconocible
vs.
metadata JWPLC reconocible con ambas copias corruptas
```

Se adopta la decisión de introducir un diagnóstico explícito:

```text
CORRUPT_METADATA
```

La clasificación solo se utilizará cuando exista evidencia reconocible del formato JWPLC en al menos una copia, pero ninguna supere la validación completa. Una FRAM sin firmas reconocibles continuará reportándose como `UNFORMATTED`.

La siguiente validación comprobará esta distinción antes de conectarla a la política pública de arranque.