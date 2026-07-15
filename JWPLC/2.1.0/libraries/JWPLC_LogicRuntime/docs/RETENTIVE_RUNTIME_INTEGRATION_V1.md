# Integración retentiva de alto nivel v1

## Estado

```text
VALIDADO EN HARDWARE
Resultado: 59 PASS, 0 FAIL
INTEGRACION RETENTIVA DEL RUNTIME: PASS
```

Compilación validada:

```text
Flash:       445757 bytes / 3145728 bytes (14 %)
RAM global:   39500 bytes / 327680 bytes (12 %)
RAM restante: 288180 bytes
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

## Validación física

Ejemplo:

```text
JWPLC_LogicRuntime_Retentive_Runtime_Integration
```

Comando utilizado:

```text
RETRUNTIME
```

La prueba modificó temporalmente:

```text
0x0000..0x1A3F
```

Incluyó el program store A/B y la región retentiva. La reserva `0x1A40..0x1FFF` quedó fuera.

Protecciones utilizadas:

- respaldo de 6720 bytes en NVS;
- CRC32 del respaldo;
- restauración pendiente;
- recuperación automática tras reinicio;
- comparación byte por byte;
- guarda posterior dentro de la reserva;
- programas sin bloques `DigitalOutput`.

Flujo validado:

1. formateo explícito del program store;
2. limpieza temporal de la región retentiva;
3. guardado y preparación de Programa A;
4. confirmación de `NO_SNAPSHOT`;
5. activación en RAM y guardado del retentivo de A;
6. detención, preparación y restauración de A;
7. guardado y preparación de Programa B;
8. confirmación de `NO_MATCHING_SNAPSHOT` para el snapshot de A;
9. guardado y restauración del snapshot de B;
10. rollback a A y recuperación de su snapshot;
11. regreso a B, corrupción de su snapshot y rechazo seguro;
12. conservación de la copia retentiva A válida;
13. restauración exacta de los 6720 bytes originales.

Resultado:

```text
Resultado: 59 PASS, 0 FAIL
INTEGRACION RETENTIVA DEL RUNTIME: PASS
```

La prueba inicializó E/S, pero ambos programas carecían de bloques `DigitalOutput`, por lo que Q0 permaneció apagado.

## Cierre

La integración retentiva v1 queda aprobada para continuar con la etapa de cierre previa a la interfaz funcional.

Antes de conectar acciones reales de la interfaz se mantienen tres pruebas:

1. persistencia del snapshot a través de reinicio o corte real de alimentación;
2. regresión completa del flujo persistente no retentivo con `flags = 0`;
3. convivencia runtime + TFT/botonera y medición de latencia durante refresco gráfico.

El diseño visual y la navegación de la interfaz pueden comenzar desde este punto sin esperar esas tres pruebas, manteniendo desacopladas las acciones reales de guardado, restauración y RUN.