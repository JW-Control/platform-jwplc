# Alpha14 — G3B-D3-B-R1 harness fix — 2026-10-02

## Resultado del intento

El primer intento R1 abortó en preflight estático:

```text
D3B_SOURCE_FRAME_ACTIVITY=FAIL
RuntimeError: D3B_SOURCE_CONTRACT_FAIL
```

No se alcanzó compilación, upload ni benchmark físico.

## Causa

R1 cambió deliberadamente la llamada:

```cpp
noteRxIntFrameActivity();
```

a:

```cpp
noteRxIntFrameActivity(now);
```

para reutilizar el `nowMs` ya disponible y sacar `micros()` del hot path de
`shouldServiceRxInt()`.

El gate todavía buscaba textualmente la firma anterior:

```text
_stats.rxFrames++;
    noteRxIntFrameActivity();
```

El source real sí conserva la propiedad requerida: la actividad D3 se registra
una vez inmediatamente después de completar/incrementar una trama Modbus.

## Clasificación

```text
HARNESS_FAILURE=YES
PRODUCT_FAILURE=NO
HARDWARE_FAILURE=NO
COMPILE_REACHED=NO
UPLOAD_REACHED=NO
PHYSICAL_BENCHMARK_REACHED=NO
SOURCE_PRODUCT_CHANGE_REQUIRED=NO
```

## Corrección

Actualizar únicamente el contrato del harness a:

```text
_stats.rxFrames++;
    noteRxIntFrameActivity(now);
```

No se modifican scheduler, thresholds, defaults ni firmware productivo.

## Regla

Cuando una refactorización intencional cambia la firma de una llamada que un
gate valida textualmente, actualizar el consumidor estático junto con el
productor y comprobar que no queden patrones contractuales stale antes de
entregar el gate físico.
