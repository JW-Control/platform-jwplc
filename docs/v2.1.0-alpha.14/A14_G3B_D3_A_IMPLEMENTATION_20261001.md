# Alpha14 — G3B-D3-A load-adaptive implementation — 2026-10-01

## Estado

```text
BASE_HEAD=0faf57b2156c330002a10564c243b0e3b7f1e414
IMPLEMENTATION_COMMIT=1f9f52290b35f174f2d457f9cff978cfc272c9be
PRODUCT_DEFAULT_INT=0
PRODUCT_DEFAULT_HOT_POLL=0
PRODUCT_DEFAULT_LOAD_ADAPTIVE=0
NEXT=G3B_D3_B_LOAD_ADAPTIVE_AB
```

D3-A introduce un candidato interno y no cambia todavía el comportamiento
productivo por defecto.

## Política implementada

```text
IDLE_INT
  -> WARM
  -> ACTIVE_POLL
  -> COOLDOWN
  -> IDLE_INT
```

La actividad se observa una sola vez cuando se completa una ADU Modbus TCP.
Los reads/chunks siguen alimentando únicamente el D2 fijo cuando éste se
compila explícitamente con `JWPLC_MODBUS_TCP_INT_HOT_POLL_US>0`.

Umbrales iniciales:

```text
FAST_GAP_US=1600
FAST_STREAK=3
SLOW_GAP_US=1800
SLOW_STREAK=2
IDLE_EXIT_US=5000
```

## Propiedades conservadas

- ISR = flag only; no SPI en ISR.
- RECV no se enmascara durante ACTIVE_POLL en D3-A.
- DISCON/TIMEOUT siguen incluidos en la máscara.
- ACTIVE_POLL hace como máximo una pasada cooperativa por `task()`; no hay
  busy-loop interno.
- D2 fixed hot-poll y D3 load-adaptive son mutuamente excluyentes por build.
- El fallback INT de 10 ms se conserva fuera de ACTIVE_POLL.
- Los defaults productivos continúan en polling tradicional.

## Observabilidad de gate

`JWPLC_MODBUS_TCP_ENABLE_PROFILE_HOOKS=1` habilita únicamente para benchmark:

- estado final;
- tramas completas observadas;
- transiciones a WARM/ACTIVE_POLL/COOLDOWN/IDLE_INT;
- pasadas en ACTIVE_POLL;
- último gap entre tramas completas.

El hook permanece OFF por defecto.
