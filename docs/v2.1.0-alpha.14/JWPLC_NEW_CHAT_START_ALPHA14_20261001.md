# Inicio de chat — JWPLC Alpha14 — D3 load-adaptive

Continuar Alpha14 desde:

```text
docs/v2.1.0-alpha.14/JWPLC_ALPHA14_HANDOFF_20261001_G3B_D3_M0A.md
```

Estado:

```text
BRANCH=v2.1.0-alpha.14/feature/modbus-tcp
HEAD=bdc16ac76122ac7346fa3ccd84149fb45ce1791b
CURRENT_GATE=G3B_D3_M0A_LOAD_CHARACTERIZATION
M0A=RUNNING_PHYSICAL_AT_HANDOFF
PRODUCT_DEFAULT_CHANGED=NO
```

Primer trabajo del nuevo chat:

1. recibir el output completo de M0A;
2. interpretar IDLE/100/500/750/1000 req/s;
3. documentar el resultado;
4. no implementar D3-A antes de revisar la curva;
5. preparar M0B RAW TCP.

Reglas críticas:

```text
W5500=26MHz
FIFO_REUSE=ON
DLEN_REUSE=ON
COPY_OUT_64=ON
DIRECT_RX=OFF
TCP_BATCH=8
RX_COMMIT=IMMEDIATE
INT_PURE=NOT_PROMOTABLE
D2_FIXED_1500US=NOT_PROMOTED
ISR=FLAG_ONLY
SAME_PASS_STATUS_REUSE=ADOPT
PERSISTENT_STATUS_CACHE=FORBIDDEN
```

Después de M0B:
thresholds/hysteresis -> D3-A -> D3-B -> D3-C H3E-R -> D3-D promotion ->
post-promotion regression -> formal RAW ~14.49 Mbps reconciliation.
