# Diagnóstico de metadata corrupta v1

## Decisión

Se distinguen explícitamente dos situaciones:

```text
UNFORMATTED
CORRUPT_METADATA
```

### `UNFORMATTED`

No existe evidencia reconocible del formato JWPLC en ninguno de los dos superblocks.

Este estado cubre:

- FRAM nunca inicializada para el runtime;
- datos ajenos al runtime;
- región sin firmas reconocibles.

### `CORRUPT_METADATA`

Existe evidencia reconocible del formato JWPLC en al menos una copia, pero ninguna copia supera la validación completa.

La evidencia v1 se considera presente cuando una copia contiene:

- la firma exacta `JWSB`; o
- simultáneamente la versión de formato y el tamaño de superblock esperados.

La validación completa exige además:

- slot activo válido;
- CRC32 correcto;
- cabecera coherente.

## Motivo

Reportar ambos casos como `UNFORMATTED` puede inducir a un instalador o editor a tratar una pérdida de metadata como si la FRAM nunca hubiese sido inicializada.

Con `CORRUPT_METADATA` se conserva el comportamiento seguro y se mejora el diagnóstico:

```text
no formatear automáticamente
no reparar automáticamente
no ejecutar slots sin metadata válida
mantener salidas apagadas
solicitar mantenimiento, reinstalación o recuperación explícita
```

## Inspector no destructivo

API de bajo nivel:

```cpp
LogicSuperblockInspection inspection =
    LogicSuperblockInspector::inspect(storage);
```

Estados del inspector:

```text
UNKNOWN
UNFORMATTED
VALID
CORRUPT_METADATA
READ_FAILED
```

El inspector:

- solo lee los 64 bytes de superblocks;
- valida individualmente ambas copias;
- selecciona la secuencia válida más reciente;
- soporta desbordamiento de secuencia mediante diferencia con signo;
- no escribe ni repara la FRAM;
- no activa programas.

## Validación del inspector

Ejemplo:

```text
JWPLC_LogicRuntime_Storage_Metadata_Diagnostic
```

Primera validación física:

```text
Resultado: 36 PASS, 0 FAIL
DIAGNOSTICO DE METADATA: PASS
```

Quedó confirmado:

```text
sin evidencia JWPLC            -> UNFORMATTED
una o dos copias válidas        -> VALID
ambas copias reconocibles malas -> CORRUPT_METADATA
```

## Integración pública

`CORRUPT_METADATA` se añadió al final de los enums para conservar los valores numéricos existentes:

```cpp
JWPLCLogicStorageError::CorruptMetadata
JWPLCLogicStorageBootState::CorruptMetadata
```

La fachada expone además:

```cpp
runtime.storage().metadataHealth();
```

Después de `storage().begin(JWPLC_FRAM)`, la política queda así:

```text
metadataHealth() = UNFORMATTED
prepareBoot()    = UNFORMATTED

metadataHealth() = CORRUPT_METADATA
prepareBoot()    = CORRUPT_METADATA
lastError()      = CORRUPT_METADATA
```

`runtime.prepareStoredProgram()` propaga el mismo estado y descarga cualquier programa previo.

Las operaciones `save()`, `loadActive()` y `rollback()` también rechazan metadata corrupta con diagnóstico explícito. `format()` continúa disponible únicamente como acción deliberada del usuario.

## Validación pública preparada

El mismo ejemplo fue actualizado para validar la integración completa.

Flujo:

1. confirmar `UNFORMATTED` en la FRAM original;
2. respaldar los 5184 bytes del gestor A/B en NVS;
3. formatear explícitamente y confirmar `VALID` + `EMPTY`;
4. corromper ambos CRC de superblock;
5. reabrir la fachada;
6. comprobar `metadataHealth() == CORRUPT_METADATA`;
7. comprobar `prepareBoot() == CORRUPT_METADATA`;
8. comprobar nombres públicos de estado y error;
9. comprobar propagación mediante `runtime.prepareStoredProgram()`;
10. comprobar que las evaluaciones no escriben los superblocks;
11. restaurar exactamente los 5184 bytes originales.

La fase reversible requiere escribir:

```text
METADATA
```

Resultado esperado:

```text
Resultado: 34 PASS, 0 FAIL
DIAGNOSTICO PUBLICO DE METADATA: PASS
```

La prueba no llama a `runtime.begin()`, no inicializa E/S y no conmuta salidas.

## Reglas vigentes

- `begin()` continúa siendo no destructivo;
- `start()` continúa siendo explícito;
- no existe reparación automática durante el arranque;
- no se intenta interpretar o ejecutar slots sin metadata válida;
- el formateo después de `CORRUPT_METADATA` requiere una orden explícita.