# Resultado de validación — diagnóstico de metadata corrupta

## Estado

```text
CRITERIO DE DETECCIÓN VALIDADO EN HARDWARE
Resultado: 36 PASS, 0 FAIL
DIAGNOSTICO DE METADATA: PASS

INTEGRACIÓN PÚBLICA VALIDADA EN HARDWARE
Resultado: 34 PASS, 0 FAIL
DIAGNOSTICO PUBLICO DE METADATA: PASS
```

## Compilación física del criterio de detección

```text
Flash:       438693 bytes / 3145728 bytes (13 %)
RAM global:   38036 bytes / 327680 bytes (11 %)
RAM restante: 289644 bytes
```

El consumo de Flash/RAM de la segunda compilación, correspondiente a la integración pública, quedó pendiente de registrar porque no fue incluido junto con el log serial.

## Hardware

```text
JWPLC Basic
FRAM física: 8192 bytes
Región reversible: 0x0000..0x143F
```

## Clasificaciones confirmadas

```text
FRAM original sin metadata JWPLC:
UNFORMATTED / copia seleccionada: NINGUNA

Dos superblocks íntegros:
VALID / copia seleccionada: 0

Copia 0 con CRC corrupto y copia 1 íntegra:
VALID / copia seleccionada: 1

Ambas copias con CRC corrupto:
CORRUPT_METADATA / copia seleccionada: NINGUNA
```

## Validación del criterio de detección

- Una FRAM sin firmas reconocibles permanece diferenciada como `UNFORMATTED`.
- Dos copias válidas seleccionan la secuencia más reciente.
- Una copia corrupta conserva evidencia JWPLC, pero la copia íntegra mantiene el estado general `VALID`.
- Cuando ambas copias conservan evidencia JWPLC pero fallan CRC, el inspector informa `CORRUPT_METADATA`.
- Ninguna copia corrupta se considera válida.
- La evaluación no inicializó E/S ni conmutó salidas.
- Los 5184 bytes originales fueron restaurados exactamente.

## Validación de la API pública

La segunda prueba confirmó:

1. `storage().begin(JWPLC_FRAM)` reabre la fachada con ambas copias corruptas.
2. La fachada permanece lista, pero no declara el almacenamiento formateado.
3. `metadataHealth()` informa `CORRUPT_METADATA`.
4. `prepareBoot()` informa `CORRUPT_METADATA`.
5. `bootState()` conserva el diagnóstico.
6. `bootStateName()` expone `CORRUPT_METADATA`.
7. `lastError()` informa `CorruptMetadata`.
8. `errorName()` expone `CORRUPT_METADATA`.
9. No queda programa cargado ni arrancable.
10. `runtime.prepareStoredProgram()` propaga `CORRUPT_METADATA`.
11. El runtime descarga cualquier programa anterior.
12. El runtime informa `StoredProgramLoadFailed`.
13. Evaluar la metadata corrupta no modifica los superblocks.
14. Los 5184 bytes originales se restauran exactamente.
15. La FRAM vuelve a clasificarse como `UNFORMATTED`.
16. El respaldo temporal NVS se elimina.

## Decisión v1

`CORRUPT_METADATA` queda adoptado como diagnóstico público estable del formato persistente v1.

```text
sin evidencia JWPLC             -> UNFORMATTED
con evidencia, sin copia válida -> CORRUPT_METADATA
con al menos una copia válida   -> VALID
```

Reglas vigentes:

- `begin()` continúa siendo no destructivo;
- no existe formateo automático;
- no existe reparación automática durante el arranque;
- una copia válida permite recuperación normal;
- la pérdida de ambas copias exige mantenimiento, reinstalación o formateo explícito;
- `start()` sigue siendo una operación separada y explícita.

## Pendiente administrativo

Registrar el consumo exacto de Flash, RAM global y RAM restante de la prueba pública de 34 PASS para completar la tabla histórica.