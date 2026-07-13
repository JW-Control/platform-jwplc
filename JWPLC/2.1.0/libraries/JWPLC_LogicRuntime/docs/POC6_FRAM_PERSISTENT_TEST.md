# PoC 6 — persistencia FRAM entre reinicios

## Objetivo

Validar que un programa lógico guardado mediante `LogicProgramStore` sobre la FRAM física:

- sobrevive a un reinicio real del ESP32;
- puede ser cargado y validado después del reinicio;
- permite escribir una nueva generación en el slot inactivo;
- mantiene la activación A/B a través de un segundo reinicio;
- restaura al final el contenido original de la ventana usada para la prueba.

## Región utilizada

```text
FRAM física:       8192 bytes
Ventana de prueba: 1088 bytes
Direcciones:       0x1BC0..0x1FFF
Slot A:            512 bytes
Slot B:            512 bytes
Superblocks:       64 bytes
```

La prueba usa el mismo layout reducido de la PoC 5.

## Protección del contenido previo

Antes de modificar la FRAM, el ejemplo:

1. lee los 1088 bytes originales;
2. calcula su CRC32;
3. guarda los bytes y el CRC en NVS mediante `Preferences`;
4. registra una etapa persistente.

La FRAM original se restaura en la tercera etapa y se compara byte por byte y por CRC.

## Etapas

### Etapa 1

- Requiere confirmación manual escribiendo `START`.
- Respalda la ventana en NVS.
- Formatea el almacenamiento A/B reducido.
- Guarda Programa A.
- Verifica que Programa A puede reconstruirse.
- Solicita RESET o corte/restablecimiento de alimentación.

### Etapa 2

- Arranca después del primer reinicio.
- Carga Programa A desde la FRAM.
- Verifica ID, generación, nombre y validador.
- Guarda Programa B en el slot inactivo.
- Solicita un segundo RESET o corte/restablecimiento.

### Etapa 3

- Arranca después del segundo reinicio.
- Carga Programa B desde la FRAM.
- Verifica ID, generación, nombre y validador.
- Restaura la ventana original desde NVS.
- Compara byte por byte y por CRC.
- Elimina el estado temporal de NVS.

## Recuperación ante interrupciones

La etapa se actualiza únicamente después de completar cada operación crítica.

```text
Etapa 1 incompleta -> se vuelve a intentar guardar A.
Etapa 2 incompleta -> se vuelve a intentar cargar A y guardar B.
Etapa 3 incompleta -> se vuelve a intentar cargar B y restaurar la ventana.
```

El respaldo original permanece en NVS hasta que la restauración final ha sido verificada.

## Ejemplo

```text
Archivo
→ Ejemplos
→ JWPLC_LogicRuntime
→ JWPLC_LogicRuntime_FRAM_Persistent
```

Monitor serial:

```text
115200 baudios
Final de línea: nueva línea
```

Comando inicial:

```text
START
```

## Resultado esperado

Primer arranque:

```text
[PASS] Respaldo persistente guardado en NVS.
[PASS] Programa A persistido y validado.
ETAPA 1 COMPLETA.
```

Segundo arranque:

```text
[PASS] Programa A cargado despues del reinicio.
[PASS] Programa B persistido en el slot inactivo.
ETAPA 2 COMPLETA.
```

Tercer arranque:

```text
[PASS] Programa B cargado despues del reinicio.
[PASS] Contenido original de FRAM restaurado exactamente.
[PASS] Estado temporal NVS eliminado.

PERSISTENCIA ENTRE REINICIOS: PASS
PoC 6 completada.
```

## Restricciones

- No inicializa E/S lógicas.
- No conmuta relés.
- No ejecuta el programa almacenado.
- No define todavía el mapa permanente de producción.
- No declara hard real-time.
- No debe sustituirse el sketch durante las tres etapas.
