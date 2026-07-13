# Redundancia de superblocks v1

## Objetivo

Validar físicamente las dos copias de metadata que seleccionan el programa activo del gestor A/B.

Cada superblock ocupa 32 bytes:

```text
Copia 0: 0x0000..0x001F
Copia 1: 0x0020..0x003F
```

Cada copia contiene:

- firma y versión;
- número de secuencia;
- slot activo;
- `Program ID`;
- generación;
- CRC32 de la metadata.

## Selección

Al abrir el almacenamiento:

1. se leen ambas copias;
2. se descartan firmas, versiones, tamaños, slots o CRC inválidos;
3. si ambas son válidas, se selecciona la secuencia más reciente;
4. si solo una es válida, se usa esa copia;
5. si ninguna es válida, el gestor no declara la FRAM como formateada.

La comparación de secuencias soporta el desbordamiento de `uint32_t` mediante diferencia con signo.

## Escenario de prueba

Después de formatear y guardar dos programas, el estado previsto es:

```text
Copia 1: secuencia 2, Slot A, Programa A, generación 1
Copia 0: secuencia 3, Slot B, Programa B, generación 2
```

La Copia 0 es la más reciente.

### Corrupción de la copia más reciente

Se altera un byte del CRC de la Copia 0.

Resultado esperado:

```text
Copia 0 rechazada
Copia 1 seleccionada
Secuencia 2
Slot A activo
Programa A cargado
```

Esto representa recuperación hacia la última decisión transaccional cuya metadata sigue íntegra.

### Corrupción de la copia anterior

Después de restaurar ambas copias, se altera el CRC de la Copia 1.

Resultado esperado:

```text
Copia 1 rechazada
Copia 0 seleccionada
Secuencia 3
Slot B activo
Programa B cargado
```

### Corrupción de ambas copias

Se alteran ambos CRC.

Comportamiento conservador actual:

```text
isReady() = true
isFormatted() = false
storeError() = UNFORMATTED
prepareBoot() = UNFORMATTED
programa cargado = no
programa arrancable = no
```

No se formatea, no se escribe y no se intenta ejecutar un slot sin una decisión válida de metadata.

Después de la prueba física se decidirá explícitamente si el estado público debe mantenerse como `UNFORMATTED` o ampliarse con un diagnóstico específico como `CORRUPT_METADATA`. La prioridad es no confundir una FRAM nunca inicializada con un gestor previamente válido que perdió ambas copias.

## Prueba reversible

Ejemplo:

```text
JWPLC_LogicRuntime_Storage_API_Superblock_Redundancy
```

La prueba:

1. confirma el estado original sin formato;
2. respalda los 5184 bytes del gestor A/B en NVS con CRC32;
3. registra restauración pendiente;
4. formatea explícitamente;
5. guarda Programa A y Programa B;
6. captura las dos copias válidas de superblock;
7. corrompe la copia más reciente y valida recuperación por la anterior;
8. restaura la metadata;
9. corrompe la copia anterior y valida uso de la más reciente;
10. restaura la metadata;
11. corrompe ambas copias y valida el estado seguro;
12. restaura las copias y comprueba nuevamente Programa B;
13. restaura exactamente los 5184 bytes originales;
14. elimina el respaldo temporal de NVS.

Solo se alteran bytes de CRC de los superblocks. Los descriptores y las imágenes de Slot A/B no se modifican durante las inyecciones.

## Seguridad

La prueba:

- no llama a `runtime.begin()`;
- no inicializa E/S;
- no ejecuta bloques;
- no conmuta salidas;
- requiere escribir explícitamente `SUPERBLOCK`;
- recupera automáticamente el respaldo original si detecta una restauración pendiente después de un reinicio.

Durante la fase reversible no se debe borrar la NVS ni cargar otro sketch.

## Resultado esperado

```text
Resultado: 51 PASS, 0 FAIL
REDUNDANCIA DE SUPERBLOCKS: PASS
```

## Pendiente posterior

- decidir el diagnóstico público para pérdida de ambas copias;
- probar cortes simulados durante escritura de un superblock con backend RAM;
- evaluar si la reparación de una copia debe ser manual o explícita;
- mantener el arranque sin escrituras automáticas.
