# Integración de alto nivel — programa persistente hacia runtime

## Objetivo

Cerrar el flujo público sin exponer buffers internos:

```text
FRAM A/B
→ prepareBoot()
→ clasificación de arranque
→ copia profunda en LogicEngine
→ start() explícito
→ tick()
```

## API

```cpp
runtime.storage().begin(JWPLC_FRAM);
runtime.begin();

JWPLCLogicStorageBootState state =
    runtime.prepareStoredProgram();
```

También se conserva la compatibilidad booleana:

```cpp
bool loaded = runtime.loadStoredProgram();
```

`loadStoredProgram()` devuelve `true` para:

```text
ACTIVE_PROGRAM_LOADED
FALLBACK_PROGRAM_LOADED
```

No llama a `start()`.

## Seguridad

Cuando la evaluación devuelve:

```text
UNFORMATTED
EMPTY
NO_VALID_PROGRAM
INVALID_PROGRAM
```

el motor descarga cualquier programa previo. Así un fallo persistente no deja disponible silenciosamente una lógica anterior.

## Copia profunda

`LogicEngine` conserva ahora:

- nombre del programa;
- definiciones de bloques;
- descriptor interno;
- estados temporales de ejecución.

Esto elimina la dependencia de vida de los buffers externos. Una operación posterior de `storage().save()` puede reutilizar el buffer de almacenamiento sin invalidar el programa que el motor ya había cargado.

El costo es un aumento fijo de RAM proporcional a `JWPLC_LOGIC_COMPILED_MAX_BLOCKS`. Debe medirse en la compilación física antes de cerrar la estrategia definitiva de perfiles 100/400 bloques.

## Prueba reversible preparada

Ejemplo:

```text
JWPLC_LogicRuntime_Stored_Program_Integration
```

La prueba:

1. inicializa la fachada y el runtime;
2. confirma `UNFORMATTED` y rechazo de `start()` sin programa;
3. respalda 5184 bytes en NVS;
4. formatea y confirma `EMPTY`;
5. guarda y prepara Programa A;
6. ejecuta un scan sin bloques de salida;
7. guarda Programa B mientras A continúa cargado;
8. vuelve a ejecutar A para comprobar independencia del buffer;
9. prepara y ejecuta B;
10. corrompe B y prepara fallback A;
11. corrompe A y confirma descarga total del motor;
12. restaura exactamente la FRAM original.

Los programas de prueba no contienen bloques `DigitalOutput`. El ejemplo inicializa E/S, pero mantiene Q0 apagadas y no energiza relés.

Comando de confirmación:

```text
RUNTIME
```

Resultado esperado:

```text
Resultado: 48 PASS, 0 FAIL
INTEGRACION RUNTIME PERSISTENTE: PASS
```

## Pendiente posterior

- medir Flash y RAM después de la copia profunda;
- decidir si 8 KiB compila solo 100 bloques o conserva 400;
- pruebas de corrupción de superblocks;
- diagnóstico TFT;
- retentivos persistentes.
