# Política de arranque persistente v1

## Estado

```text
VALIDADA EN HARDWARE
Resultado: 44 PASS, 0 FAIL
POLITICA DE ARRANQUE: PASS
```

Compilación validada:

```text
Flash:       438453 bytes / 3145728 bytes (13 %)
RAM global:   46276 bytes / 327680 bytes (14 %)
RAM restante: 281404 bytes
```

## Objetivo

Definir un resultado explícito y no ambiguo al evaluar la FRAM durante el arranque del JWPLC Logic Runtime.

La evaluación de bajo nivel se realiza mediante:

```cpp
JWPLCLogicStorageBootState state =
    runtime.storage().prepareBoot();
```

La integración de alto nivel usa:

```cpp
JWPLCLogicStorageBootState state =
    runtime.prepareStoredProgram();
```

`prepareBoot()` no formatea, no guarda programas, no ejecuta rollback y no modifica el superblock. Solo inspecciona el almacenamiento y, cuando existe un programa válido, lo reconstruye en RAM.

`prepareStoredProgram()` aplica la misma clasificación y además carga una copia propietaria en `LogicEngine`. Nunca ejecuta `start()` automáticamente.

## Estados

| Estado | Significado | Programa en RAM |
|---|---|---:|
| `NOT_EVALUATED` | Todavía no se ejecutó la política | No |
| `NOT_READY` | Backend o fachada no inicializados | No |
| `UNFORMATTED` | No existe firma válida del gestor A/B | No |
| `EMPTY` | El gestor está formateado pero no tiene slot activo | No |
| `ACTIVE_PROGRAM_LOADED` | El slot activo fue validado y cargado | Sí |
| `FALLBACK_PROGRAM_LOADED` | El activo falló y se cargó el slot alterno | Sí |
| `NO_VALID_PROGRAM` | Ninguno de los dos slots pudo cargarse | No |
| `INVALID_PROGRAM` | La imagen se reconstruyó pero falló el validador lógico | No |

Consulta textual:

```cpp
JWPLCLogicStorage::bootStateName(state);
```

## Regla de seguridad

```text
prepareBoot() y prepareStoredProgram() nunca llaman start()
```

La ejecución física requiere una orden posterior y explícita:

```cpp
JWPLCLogicStorageBootState state =
    runtime.prepareStoredProgram();

if (state == JWPLCLogicStorageBootState::ActiveProgramLoaded)
{
  runtime.start();
}
```

En caso de fallback, la aplicación puede exigir confirmación:

```cpp
if (state == JWPLCLogicStorageBootState::FallbackProgramLoaded)
{
  // Informar al operador, bloquear o confirmar runtime.start().
}
```

## Fallback

Cuando el superblock apunta al Slot B y la imagen B está dañada:

1. se intenta cargar B;
2. B se rechaza por descriptor, CRC o codec;
3. se intenta cargar A;
4. si A es válido, se devuelve `FALLBACK_PROGRAM_LOADED`;
5. A queda reconstruido en RAM;
6. el superblock sigue apuntando a B.

No se realiza una reparación automática durante el arranque. Esto evita convertir una lectura de recuperación en una escritura persistente inesperada.

El usuario puede decidir posteriormente:

- ejecutar temporalmente el fallback cargado;
- llamar a `rollback()` para activarlo de forma persistente;
- instalar una nueva versión;
- detener el equipo y solicitar mantenimiento.

## Política por estado

### `UNFORMATTED`

- Mantener salidas apagadas.
- No formatear automáticamente.
- Permitir que un instalador o editor solicite `format()` explícito.

### `EMPTY`

- Mantener salidas apagadas.
- Informar que el almacenamiento está preparado pero no contiene programa.
- Esperar instalación explícita.

### `ACTIVE_PROGRAM_LOADED`

- El programa está disponible en RAM.
- El runtime todavía no lo ejecuta hasta recibir `start()`.

### `FALLBACK_PROGRAM_LOADED`

- El programa alterno está disponible en RAM.
- Informar que hubo recuperación.
- No reparar automáticamente el superblock.
- La aplicación decide si ejecuta, bloquea o confirma rollback.

### `NO_VALID_PROGRAM`

- Mantener salidas apagadas.
- Descargar cualquier programa previamente preparado en el motor.
- No intentar ejecutar bloques.
- Requerir reinstalación o mantenimiento.

### `INVALID_PROGRAM`

- Mantener salidas apagadas.
- Rechazar la imagen aunque CRC y codec sean correctos.
- Informar el error del validador lógico.

## Prueba física reversible validada

Ejemplo:

```text
JWPLC_LogicRuntime_Storage_API_Startup_Policy
```

La prueba validó:

1. FRAM original sin formato → `UNFORMATTED`;
2. gestor recién formateado → `EMPTY`;
3. Programa A válido → `ACTIVE_PROGRAM_LOADED`;
4. Programa B válido → `ACTIVE_PROGRAM_LOADED`;
5. Programa B activo corrompido → fallback hacia A;
6. A y B corruptos → `NO_VALID_PROGRAM`;
7. restauración exacta de los 5184 bytes originales.

Resultado físico:

```text
Resultado: 44 PASS, 0 FAIL
POLITICA DE ARRANQUE: PASS
```

Documentación detallada:

```text
docs/STARTUP_POLICY_RESULTS.md
```

## Siguiente validación

Ejemplo preparado:

```text
JWPLC_LogicRuntime_Stored_Program_Integration
```

Valida la integración `runtime.prepareStoredProgram()` y una propiedad adicional: `LogicEngine` conserva ahora una copia profunda del programa, por lo que una operación posterior de `storage().save()` no invalida el programa ya cargado en el motor.

Pendientes posteriores:

- diagnóstico resumido en TFT;
- corrupción controlada de superblocks;
- política final por modo de aplicación para aceptar o bloquear fallback;
- revisión de RAM para perfiles compilados de 100/400 bloques.
