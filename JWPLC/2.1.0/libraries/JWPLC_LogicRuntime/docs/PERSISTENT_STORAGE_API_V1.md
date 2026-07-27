# API persistente v1 — JWPLC Logic Runtime

## Estado

Primera integración pública del backend FRAM, el codec binario y el gestor A/B.

Esta etapa conserva una regla fundamental:

```text
begin() nunca formatea automáticamente la FRAM
```

La memoria solo se modifica mediante llamadas explícitas como `format()`, `save()` o `rollback()`.

## Acceso estilo punto

```cpp
JWPLC_LogicRuntime runtime;

runtime.storage().begin(JWPLC_FRAM);
runtime.storage().isReady();
runtime.storage().isFormatted();
runtime.storage().status();
```

La fachada se obtiene mediante:

```cpp
JWPLCLogicStorage &storage = runtime.storage();
```

## Flujo disponible

### Detectar almacenamiento sin escribir

```cpp
if (!runtime.storage().begin(JWPLC_FRAM))
{
  Serial.println(
      JWPLCLogicStorage::errorName(runtime.storage().lastError()));
}
```

`begin()`:

- consulta la capacidad física de `JWPLC_FRAM`;
- selecciona el perfil de 8 KiB o 32 KiB;
- conecta el backend al mapa completo correspondiente;
- lee las copias de superblock;
- informa si existe un formato válido;
- no ejecuta `fill()`, `write()` ni formateo automático.

### Formato explícito

```cpp
runtime.storage().format();
```

El formato actual inicializa únicamente la región del gestor de programas:

```text
Superblocks + Slot A + Slot B
```

No borra la futura región de retentivos ni la reserva.

### Guardar

```cpp
runtime.storage().save(program, 0x1001);
```

El programa se valida antes de serializarlo y se guarda en el slot inactivo. La generación se obtiene de la secuencia monotónica del superblock, evitando reutilizar generaciones antiguas después de un rollback.

### Cargar

```cpp
if (runtime.storage().loadActive())
{
  LogicProgram program = runtime.storage().activeProgram();
}
```

El programa cargado referencia un buffer propiedad de la fachada. Ese buffer permanece válido mientras exista el objeto `JWPLC_LogicRuntime` y no se ejecute otra operación que reemplace su contenido.

También existe la integración directa:

```cpp
runtime.begin();
runtime.loadStoredProgram();
runtime.start();
```

`loadStoredProgram()` exige que tanto el runtime como la fachada persistente hayan sido inicializados.

### Rollback explícito

```cpp
if (runtime.storage().rollback())
{
  LogicProgram previous = runtime.storage().activeProgram();
}
```

El rollback usa dos fases:

1. carga el slot alterno sin cambiar el superblock activo;
2. verifica descriptor, CRC, codec y validador lógico;
3. solo entonces escribe la copia alterna del superblock;
4. deja el programa reactivado cargado en RAM.

La imagen del slot no se reescribe. Con dos slots, `rollback()` significa “activar el otro slot verificado”. Una segunda llamada puede volver al programa que quedó alterno.

## Estado consultable

```cpp
const LogicProgramStoreStatus &status = runtime.storage().status();
```

Campos disponibles:

- `formatted`;
- `activeSlot`;
- `superblockCopy`;
- `lastLoadedSlot`;
- `sequence`;
- `programId`;
- `generation`.

## Errores

La fachada expone tres niveles:

```cpp
runtime.storage().lastError();
runtime.storage().storeError();
runtime.storage().validationError();
```

Esto permite distinguir:

- fallo general de la fachada;
- fallo transaccional A/B;
- programa lógico inválido;
- rollback no disponible;
- fallo al activar el candidato validado.

## Memoria RAM

La fachada no usa memoria dinámica. Conserva:

- un `LogicProgramBuffer` para el programa reconstruido;
- un buffer scratch suficiente para la imagen máxima compilada;
- estado del backend y del gestor.

Compilación física validada con `JWPLC_LogicRuntime_Storage_API_ReadOnly`:

```text
Flash:       421449 bytes / 3145728 bytes (13 %)
RAM global:   40980 bytes / 327680 bytes (12 %)
RAM restante: 286700 bytes
```

Compilación física de la prueba reversible:

```text
Flash:       436597 bytes / 3145728 bytes (13 %)
RAM global:   46276 bytes / 327680 bytes (14 %)
RAM restante: 281404 bytes
```

La diferencia incluye el respaldo global de 5184 bytes usado únicamente por el banco de pruebas reversible. No corresponde al costo permanente de la fachada.

Antes de cerrar la API se evaluará si conviene mantener buffers para 400 bloques dentro de cada instancia o permitir buffers externos/perfiles compilados para reducir RAM en equipos de 8 KiB.

## Prueba inicial no destructiva — validada

Ejemplo:

```text
JWPLC_LogicRuntime_Storage_API_ReadOnly
```

Resultado físico:

```text
Resultado: 8 PASS, 0 FAIL
API PERSISTENTE LECTURA: PASS
```

Estado detectado:

```text
FRAM formateada para runtime: NO
Slot activo: NINGUNO
Secuencia: 0
Program ID: 0
Generacion: 0
Error fachada: NONE
Estado gestor: UNFORMATTED
```

Esto confirma que `storage().begin(JWPLC_FRAM)` es no destructivo y que una FRAM sin firma válida permanece intacta.

## Prueba reversible de escritura — validada

Ejemplo:

```text
JWPLC_LogicRuntime_Storage_API_Reversible
```

La prueba realizó:

1. respaldo de `0x0000..0x143F` en NVS con CRC32;
2. registro de restauración pendiente;
3. `storage().format()` explícito;
4. `storage().save()` en Slot A;
5. `storage().loadActive()`;
6. reinicialización de la fachada;
7. segunda carga del programa;
8. restauración exacta de los 5184 bytes originales;
9. recuperación del estado sin formato;
10. eliminación del respaldo temporal.

Resultado físico:

```text
Resultado: 27 PASS, 0 FAIL
API PERSISTENTE REVERSIBLE: PASS
```

Quedaron validados el ID del programa, la generación automática, el contenido del `TON`, la salida, la persistencia del estado A/B y la restauración exacta.

## Prueba reversible de rollback — preparada

Ejemplo:

```text
JWPLC_LogicRuntime_Storage_API_Rollback
```

Flujo:

1. respaldar los 5184 bytes del gestor A/B;
2. formatear explícitamente;
3. guardar Programa A en Slot A, generación 1;
4. guardar Programa B en Slot B, generación 2;
5. cargar Programa B;
6. ejecutar `storage().rollback()`;
7. verificar que Slot A vuelve a quedar activo;
8. verificar ID, generación, contenido y secuencia;
9. reinicializar la fachada y volver a cargar Programa A;
10. restaurar exactamente el contenido original.

La prueba exige escribir:

```text
ROLLBACK
```

Resultado esperado:

```text
Resultado: 34 PASS, 0 FAIL
ROLLBACK PERSISTENTE: PASS
```

No inicializa el motor de E/S ni conmuta salidas. Si ocurre un reinicio después de registrar la restauración pendiente, el siguiente arranque recupera automáticamente el contenido original.

## Pendiente inmediato

Después de validar rollback:

1. política de arranque ante almacenamiento sin formato, programa ausente o ambas imágenes corruptas;
2. API de producción para activar un programa sin exponer buffers internos;
3. revisión del consumo RAM por instancia y estrategia para perfiles de 8/32 KiB;
4. retentivos persistentes.
