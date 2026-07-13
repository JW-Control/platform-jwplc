# Resultado de validación — diagnóstico de metadata corrupta

## Estado

```text
VALIDADO EN HARDWARE
Resultado: 36 PASS, 0 FAIL
DIAGNOSTICO DE METADATA: PASS
```

## Compilación física

```text
Flash:       438693 bytes / 3145728 bytes (13 %)
RAM global:   38036 bytes / 327680 bytes (11 %)
RAM restante: 289644 bytes
```

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

## Propiedades confirmadas

- Una FRAM sin firmas reconocibles permanece diferenciada como `UNFORMATTED`.
- Dos copias válidas seleccionan la secuencia más reciente.
- Una copia corrupta conserva evidencia JWPLC, pero la copia íntegra mantiene el estado general `VALID`.
- Cuando ambas copias conservan evidencia JWPLC pero fallan CRC, el inspector informa `CORRUPT_METADATA`.
- Ninguna copia corrupta se considera válida.
- La fachada permaneció operativa y no declaró formato válido.
- La política pública anterior se mantuvo temporalmente en el estado seguro `UNFORMATTED`.
- No quedó programa cargado ni arrancable.
- La evaluación no inicializó E/S ni conmutó salidas.
- Los 5184 bytes originales fueron restaurados exactamente.
- El estado original `UNFORMATTED` volvió a detectarse después de la restauración.
- El respaldo temporal NVS fue eliminado.

## Conclusión

El criterio de detección queda validado físicamente:

```text
sin evidencia JWPLC       -> UNFORMATTED
con evidencia, sin copia válida -> CORRUPT_METADATA
con al menos una copia válida   -> VALID
```

Con este resultado se integra `CORRUPT_METADATA` en la fachada pública, `prepareBoot()` y `runtime.prepareStoredProgram()`. La integración pública deberá pasar una nueva prueba reversible antes de darse por cerrada.