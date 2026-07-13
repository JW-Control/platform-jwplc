# API persistente v1 — JWPLC Logic Runtime

## Estado

Primera integración pública del backend FRAM, el codec binario y el gestor A/B.

Esta etapa conserva una regla fundamental:

```text
begin() nunca formatea automáticamente la FRAM
```

La memoria solo se modifica mediante llamadas explícitas como `format()` o `save()`.

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

## Flujo previsto

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

La generación se incrementa automáticamente. El programa se valida antes de serializarlo y se guarda en el slot inactivo.

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
- programa lógico inválido.

## Memoria RAM

La fachada no usa memoria dinámica. Conserva:

- un `LogicProgramBuffer` para el programa reconstruido;
- un buffer scratch suficiente para la imagen máxima compilada;
- estado del backend y del gestor.

El impacto exacto en RAM global debe registrarse con la compilación física del ejemplo `JWPLC_LogicRuntime_Storage_API_ReadOnly`.

## Prueba inicial no destructiva

Ejemplo:

```text
JWPLC_LogicRuntime_Storage_API_ReadOnly
```

Comprueba la nueva API sin:

- inicializar entradas o salidas;
- ejecutar el motor;
- formatear la FRAM;
- escribir un programa;
- modificar retentivos o reserva.

Resultado esperado:

```text
Resultado: 8 PASS, 0 FAIL
API PERSISTENTE LECTURA: PASS
```

## Pendiente inmediato

Después de validar compilación, RAM y lectura sobre el hardware:

1. prueba física reversible de `format()`, `save()` y `loadActive()` usando respaldo previo;
2. operación explícita de rollback hacia el slot verificado anterior;
3. política de arranque ante almacenamiento sin formato, programa ausente o ambas imágenes corruptas;
4. API de producción para activar un programa sin exponer buffers internos.
