# Integración de alto nivel — programa persistente hacia runtime

## Estado

```text
VALIDADO EN HARDWARE
Resultado: 48 PASS, 0 FAIL
INTEGRACION RUNTIME PERSISTENTE: PASS
```

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

`LogicEngine` conserva:

- nombre del programa;
- definiciones de bloques;
- descriptor interno;
- estados temporales de ejecución.

Esto elimina la dependencia de vida de los buffers externos. Una operación posterior de `storage().save()` puede reutilizar el buffer de almacenamiento sin invalidar el programa que el motor ya había cargado.

## Validación física

Ejemplo:

```text
JWPLC_LogicRuntime_Stored_Program_Integration
```

La prueba confirmó:

1. inicialización independiente de la fachada y del runtime;
2. clasificación `UNFORMATTED` y rechazo de `start()` sin programa;
3. respaldo reversible de 5184 bytes en NVS;
4. clasificación `EMPTY` después del formato explícito;
5. carga de Programa A mediante la API de alto nivel;
6. ejecución de A sin bloques de salida;
7. guardado de B mientras A seguía cargado;
8. independencia de la copia profunda de A frente a la reutilización de buffers;
9. reemplazo correcto de A por B;
10. fallback hacia A al corromper B;
11. descarga total del motor al corromper ambos slots;
12. restauración exacta de la FRAM original.

Los programas de prueba no contienen bloques `DigitalOutput`. El ejemplo inicializa E/S, pero mantiene Q0 apagadas y no energiza relés.

Resultado:

```text
Resultado: 48 PASS, 0 FAIL
INTEGRACION RUNTIME PERSISTENTE: PASS
```

## Consumo medido

```text
Flash:       440717 bytes / 3145728 bytes (14 %)
RAM global:   51108 bytes / 327680 bytes (15 %)
RAM restante: 276572 bytes
```

Comparado con la prueba de política de arranque, que usa el mismo respaldo reversible:

```text
RAM política de arranque: 46276 bytes
RAM integración completa: 51108 bytes
Aumento por copia profunda: 4832 bytes
```

La medición confirma que el límite compilado de bloques afecta directamente el costo fijo de RAM.

## Decisión siguiente

El JWPLC Basic actual usa FRAM de 8 KiB y su límite funcional es 100 bloques. Por tanto, el build predeterminado pasará a reservar RAM para 100 bloques.

La capacidad física y el formato de 32 KiB seguirán admitiendo 400 bloques, pero requerirán una configuración de compilación explícita en el hardware futuro correspondiente.

## Pendiente posterior

- validar el perfil compilado predeterminado de 100 bloques y medir el ahorro de RAM;
- conservar una opción explícita de 400 bloques para FRAM de 32 KiB;
- pruebas de corrupción de superblocks;
- diagnóstico TFT;
- retentivos persistentes.
