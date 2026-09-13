# Alpha14 — Limpieza de warnings previa a FULL_RUNTIME_REALISTIC

Fecha: `2026-09-13`

## Contexto

Durante la preparación del firmware `FULL_RUNTIME_REALISTIC` para `A14.3`, la compilación inicial terminó correctamente (`COMPILE_EXIT=0`) pero con warnings bajo `--warnings all`.

Se decidió no avanzar al smoke físico hasta limpiar los warnings propios del nuevo harness y revisar el warning de inicialización reportado por el backend Ethernet.

## Warnings observados

### Harness `FULL_RUNTIME_REALISTIC`

Se observaron dos warnings por incremento `++` sobre variables `volatile`:

```text
displayFramesTotal
displayPhase
```

También se reportó un helper sin uso:

```text
boolText(bool)
```

### `EthernetClient`

El compilador reportó warnings `-Wuninitialized` / `-Wmaybe-uninitialized` asociados a `_startMillis` al ejecutar asignaciones desde temporales `EthernetClient()` en el Server y Client Modbus TCP.

La causa encontrada fue que `Stream()` inicializaba `_timeout`, pero no `_startMillis`, mientras `EthernetClient()` no asignaba explícitamente ese miembro heredado.

## Corrección aplicada y validada localmente

En `JWPLC_W5x00_Ethernet.h` ambos constructores `EthernetClient` inicializan explícitamente:

```cpp
_startMillis = 0;
```

En el harness:

- se eliminó `boolText(bool)`;
- `++displayFramesTotal` se reemplazó por asignación explícita;
- `++displayPhase` se reemplazó por asignación explícita.

No se introdujo ningún cambio funcional intencional en la semántica de conexión TCP ni en el perfil de benchmark.

## Evidencia de compilación

La recompilación con:

```text
--warnings all
```

terminó con:

```text
COMPILE_EXIT=0
WARNING_COUNT=0
A14_3_FULL_RUNTIME_REALISTIC_WARNING_CLEANUP=PASS
COMPILE_WARNINGS=0
ETHERNETCLIENT_STARTMILLIS=INITIALIZED
VOLATILE_INCREMENT_WARNINGS=RESOLVED
UNUSED_FUNCTION_WARNING=RESOLVED
```

Uso de recursos del sketch:

```text
Program storage: 427421 bytes (10%)
Dynamic memory: 29796 bytes (9%)
```

## Clasificación

```text
A14_3_FULL_RUNTIME_REALISTIC_WARNING_CLEANUP=PASS
PHYSICAL_SMOKE=NOT_EXECUTED
```

## Decisión

Se permite consolidar primero el fix de `EthernetClient` y luego el harness `FULL_RUNTIME_REALISTIC` como cambios de código separados.

Después de consolidarlos, el siguiente gate será:

```text
A14.3_FULL_RUNTIME_REALISTIC_PHYSICAL_SMOKE
```
