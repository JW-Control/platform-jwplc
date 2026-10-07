# Alpha13 — G2-P2 — Diseño candidato A13-002

Fecha: 2026-10-07

## Objetivo

Corregir el startup TCA/EN_IO sin cambiar APIs públicas y sin regenerar aún el
core precompilado.

## Candidato

Archivos productivos:

```text
JWPLC/2.1.0/cores/jwcontrol/peripherals_init.cpp
JWPLC/2.1.0/cores/jwcontrol/jwplc_peripherals.cpp
JWPLC/2.1.0/cores/jwcontrol/jwplc_peripherals.h
```

Semántica:

1. `jwplcSystemInitState()` crea snapshots/cache pero deja
   `initialized=false`.
2. Las cinco operaciones post-probe del TCA deben devolver éxito.
3. El primer fallo retorna inmediatamente.
4. Como `EN_IO` ya estaba LOW, permanece LOW en toda ruta de fallo.
5. Sólo la ruta normal habilita `EN_IO=HIGH`.
6. Tras el settle de 2 ms, se marca el init de periféricos y luego
   `JWPLC_IO.ready()==true`.
7. El path no-Basic conserva compatibilidad marcando ready explícitamente.

## API

No se modifica la API Arduino de usuario.

Se añade un helper interno del core:

```cpp
void jwplcSystemSetIOReady(bool ready);
```

Su único propósito en este cambio es separar:

```text
estado/cache inicializado
!=
E/S físicas listas
```

## Criterio G2-P2

Control normal:

```text
OP_ATTEMPT_MASK=31
OP_OK_MASK=31
EN_IO_HIGH_REQUESTED=YES
EN_IO_OUTPUT_LATCH=HIGH
PERIPHERALS_INITIALIZED=YES
IO_VIEW_READY=YES
```

Para fallo inyectado N=1..5:

```text
OP_ATTEMPT_MASK=(1<<N)-1
OP_OK_MASK=(1<<(N-1))-1
EN_IO_HIGH_REQUESTED=NO
EN_IO_OUTPUT_LATCH=LOW
PERIPHERALS_INITIALIZED=NO
IO_STATE_INITIALIZED=NO
IO_VIEW_READY=NO
```

## Packaging

El `core.a` actual queda congelado durante G2-P2:

```text
SHA256=78d0c0ab14f156b96116529e88872340d51877af40d24ba3559f6081e0bf34fb
```

Secuencia obligatoria:

```text
candidate local uncommitted
-> source-first PASS
-> refresh core.a
-> normal jwplcbasic archive verification
-> physical gate
-> final diff audit
-> product commit
```
