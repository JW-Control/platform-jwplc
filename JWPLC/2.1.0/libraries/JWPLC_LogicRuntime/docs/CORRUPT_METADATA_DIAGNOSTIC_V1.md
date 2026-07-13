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

Se añadió:

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

## Validación preparada

Ejemplo:

```text
JWPLC_LogicRuntime_Storage_Metadata_Diagnostic
```

Flujo:

1. confirmar que la FRAM original se clasifica `UNFORMATTED`;
2. respaldar los 5184 bytes del gestor A/B en NVS;
3. formatear y guardar Programa A y Programa B;
4. comprobar `VALID` con ambas copias íntegras;
5. corromper la copia más reciente y comprobar `VALID` mediante la redundante;
6. restaurar la metadata;
7. corromper ambas copias;
8. comprobar `CORRUPT_METADATA`;
9. confirmar que la API pública actual continúa en estado seguro sin programa arrancable;
10. restaurar exactamente la metadata y los 5184 bytes originales.

La fase destructiva reversible requiere escribir:

```text
METADATA
```

Resultado esperado:

```text
Resultado: 36 PASS, 0 FAIL
DIAGNOSTICO DE METADATA: PASS
```

La prueba no inicializa E/S ni conmuta salidas.

## Integración posterior

Después del PASS, `CORRUPT_METADATA` se conectará a:

```text
JWPLCLogicStorageBootState
runtime.storage().prepareBoot()
runtime.prepareStoredProgram()
bootStateName()
```

Se conservarán estas reglas:

- `begin()` seguirá siendo no destructivo;
- `start()` seguirá siendo explícito;
- no habrá reparación automática durante el arranque;
- la incorporación del nuevo estado se hará al final del enum para no cambiar los valores numéricos existentes.