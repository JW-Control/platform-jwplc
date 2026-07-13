# Política de arranque persistente v1

## Objetivo

Definir un resultado explícito y no ambiguo al evaluar la FRAM durante el arranque del JWPLC Logic Runtime.

La evaluación se realiza mediante:

```cpp
JWPLCLogicStorageBootState state =
    runtime.storage().prepareBoot();
```

`prepareBoot()` no formatea, no guarda programas, no ejecuta rollback y no modifica el superblock. Solo inspecciona el almacenamiento y, cuando existe un programa válido, lo reconstruye en RAM.

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
prepareBoot() nunca activa salidas y nunca escribe la FRAM
```

La ejecución física requiere pasos posteriores y explícitos:

```cpp
runtime.begin();
runtime.loadProgram(runtime.storage().activeProgram());
runtime.start();
```

La integración de alto nivel con `loadStoredProgram()` se ajustará después de validar esta política de forma aislada.

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

## Política propuesta por estado

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
- El runtime todavía no debe ejecutarlo hasta recibir `start()`.

### `FALLBACK_PROGRAM_LOADED`

- El programa alterno está disponible en RAM.
- Informar que hubo recuperación.
- No reparar automáticamente el superblock.
- La aplicación decide si ejecuta, bloquea o confirma rollback.

### `NO_VALID_PROGRAM`

- Mantener salidas apagadas.
- No intentar ejecutar bloques.
- Informar fallo persistente.
- Requerir reinstalación o mantenimiento.

### `INVALID_PROGRAM`

- Mantener salidas apagadas.
- Rechazar la imagen aunque CRC y codec sean correctos.
- Informar el error del validador lógico.

## Prueba física reversible

Ejemplo:

```text
JWPLC_LogicRuntime_Storage_API_Startup_Policy
```

La prueba respalda `0x0000..0x143F` en NVS y valida:

1. FRAM original sin formato → `UNFORMATTED`;
2. gestor recién formateado → `EMPTY`;
3. Programa A válido → `ACTIVE_PROGRAM_LOADED`;
4. Programa B válido → `ACTIVE_PROGRAM_LOADED`;
5. Programa B activo corrompido → fallback hacia A;
6. A y B corruptos → `NO_VALID_PROGRAM`;
7. restauración exacta de los 5184 bytes originales.

Para iniciar la fase destructiva reversible se debe escribir:

```text
BOOT
```

Resultado esperado:

```text
Resultado: 44 PASS, 0 FAIL
POLITICA DE ARRANQUE: PASS
```

La prueba no inicializa el motor de E/S ni conmuta salidas. Si ocurre un reinicio después de registrar la restauración pendiente, el siguiente arranque intenta recuperar automáticamente el contenido original.

## Pendiente después de validar

- Integrar esta clasificación en `runtime.loadStoredProgram()`.
- Definir si `FALLBACK_PROGRAM_LOADED` puede arrancar automáticamente o exige confirmación según el modo de aplicación.
- Exponer diagnóstico resumido en TFT.
- Añadir pruebas de corrupción de superblocks además de imágenes.
