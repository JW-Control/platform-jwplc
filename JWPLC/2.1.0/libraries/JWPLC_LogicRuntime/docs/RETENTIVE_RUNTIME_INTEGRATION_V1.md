# Integración retentiva de alto nivel v1

## Estado

```text
IMPLEMENTACIÓN PREPARADA
VALIDACIÓN FÍSICA PENDIENTE
```

## Flujo de arranque

La aplicación conserva decisiones explícitas y separadas:

```cpp
runtime.storage().begin(JWPLC_FRAM);
runtime.begin();

JWPLCLogicStorageBootState bootState =
    runtime.prepareStoredProgram();

JWPLCLogicRetentiveState retentiveState =
    runtime.restoreStoredRetentiveState();

runtime.start();
```

Ninguna de estas operaciones inicia automáticamente la lógica.

## Flujo de guardado

El estado retentivo se guarda únicamente por una orden explícita:

```cpp
runtime.saveStoredRetentiveState();
runtime.stop();
```

El guardado debe realizarse antes de `stop()`, ya que `stop()` conserva su comportamiento histórico y limpia los estados temporales del motor.

No se escribe FRAM en cada scan ni ante cada cambio de `SET/RESET`.

## Estados públicos

```text
NOT_EVALUATED
NOT_READY
NO_STORED_PROGRAM
NO_RETENTIVE_BLOCKS
NO_SNAPSHOT
RESTORED
SAVED
NO_MATCHING_SNAPSHOT
STORE_ERROR
```

Consulta:

```cpp
runtime.retentiveState();
runtime.retentiveStateName(state);
runtime.retentiveStoreError();
runtime.retentiveStoreStatus();
```

## Identidad y seguridad

La restauración requiere que el programa reconstruido por `storage()` coincida exactamente con la copia profunda cargada en el motor.

Después se exige coincidencia de:

```text
Program ID
generación
cantidad de bloques
bytes del bitmap
```

Consecuencias:

- un snapshot antiguo no se aplica a una generación nueva;
- un fallback utiliza la identidad del slot realmente cargado;
- un programa cargado directamente con `loadProgram()` no recibe por accidente el snapshot de un programa persistente anterior;
- la ausencia de snapshot deja los retentivos en falso y no impide un `start()` explícito;
- una identidad distinta deja los retentivos en falso;
- un snapshot corrupto no se aplica.

## Compatibilidad

Las API anteriores permanecen sin cambios:

```cpp
runtime.prepareStoredProgram();
runtime.loadStoredProgram();
runtime.start();
runtime.stop();
runtime.tick();
```

La integración retentiva es optativa. Un sketch que no llame a las nuevas funciones mantiene el comportamiento anterior.

## Validación física preparada

Ejemplo:

```text
JWPLC_LogicRuntime_Retentive_Runtime_Integration
```

Comando requerido:

```text
RETRUNTIME
```

La prueba modifica temporalmente:

```text
0x0000..0x1A3F
```

Incluye el program store A/B y la región retentiva. La reserva `0x1A40..0x1FFF` queda fuera.

Protecciones:

- respaldo de 6720 bytes en NVS;
- CRC32 del respaldo;
- restauración pendiente;
- recuperación automática tras reinicio;
- comparación byte por byte;
- guarda posterior dentro de la reserva;
- programas sin bloques `DigitalOutput`.

Flujo validado por el ejemplo:

1. formatear explícitamente el program store;
2. limpiar temporalmente la región retentiva;
3. guardar y preparar Programa A;
4. confirmar `NO_SNAPSHOT`;
5. activar en RAM y guardar el retentivo de A;
6. detener, preparar y restaurar A;
7. guardar y preparar Programa B;
8. confirmar `NO_MATCHING_SNAPSHOT` para el snapshot de A;
9. guardar y restaurar el snapshot de B;
10. hacer rollback a A y recuperar su snapshot;
11. regresar a B, corromper su snapshot y rechazarlo;
12. conservar la copia retentiva A válida;
13. restaurar exactamente los 6720 bytes originales.

Resultado esperado:

```text
Resultado: 59 PASS, 0 FAIL
INTEGRACION RETENTIVA DEL RUNTIME: PASS
```

La prueba inicializa E/S, pero ambos programas carecen de bloques `DigitalOutput`, por lo que Q0 permanece apagado.