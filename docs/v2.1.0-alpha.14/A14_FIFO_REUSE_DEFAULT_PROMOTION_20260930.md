# Alpha14 — promoción FIFO_REUSE como default — 2026-09-30

## Resultado

La corrida de promoción corta compiló el package sin override
`JWPLC_W5500_RX_FIFO_REUSE` y confirmó que el comportamiento promovido
`#define JWPLC_W5500_RX_FIFO_REUSE 1` funciona en full runtime.

```text
A14_FIFO_REUSE_DEFAULT_PROMOTION=PASS_TECHNICAL
FIFO_REUSE_SOURCE=PACKAGE_DEFAULT
FIFO_REUSE_BUILD_OVERRIDE=NO
PACKAGE_PROMOTION_CONTRACT=PASS
PRODUCT_FAILURE=NO_EVIDENCE
HARDWARE_FAILURE=NO_EVIDENCE
```

La ejecución terminó posteriormente en una condición del harness porque no se
realizó la interacción manual de botonera durante esta corrida de 120 s:

```text
MASTER_BUTTON_DOWN_SAMPLES=0
SLAVE_BUTTON_DOWN_SAMPLES=0
```

Esto no invalida la promoción porque:

1. `S2_GATE_DATA=PASS`;
2. S2D de 600 s ya validó botonera y TFT físicamente con FIFO_REUSE=1;
3. la única variable de esta promoción es la fuente de activación de FIFO_REUSE:
   override explícito versus default del package;
4. el macro compilado resultante sigue siendo el mismo valor `1`.

Por ello los físicos se heredan de S2D y el gate se corrige para no exigirlos
nuevamente en `--promotion-short`.

## Datos de la corrida

```text
DURATION_MS=120002
ETH_RX_BYTES=49320140
ETH_ECHO_BYTES=49320140
ETH_RX_MBPS=3.287955
ETH_ECHO_TX_MBPS=3.287955

ETH_CORRUPTION_ERRORS=0
ETH_TRANSPORT_ERRORS=0
ETH_SPI_LOCK_ERRORS=0
ETH_SPI_HOLD_MAX_US=3245

MODBUS_CYCLES_OK=3281
MODBUS_REQUESTS_OK=9843
MODBUS_FAILURES=0
MODBUS_PATTERN_MISMATCHES=0
MODBUS_CRC_ERRORS=0
MODBUS_TIMEOUTS=0
MODBUS_MAX_TRANSACTION_US=18634

DATALOG_ACTIVE=YES
DATALOG_ACCEPTED_WRITES=5830
DATALOG_ACCEPTED_BYTES=186560
DATALOG_COMMITTED_BYTES=186368
DATALOG_COMMIT_COUNT=364
DATALOG_FAILED_COMMITS=0
DATALOG_MANUAL_SERVICE_CALL=NO

FRAM_FAIL=0
RTC_FAIL=0
SD_FAIL=0
MASTER_MAX_LOOP_US=14163
SLAVE_MAX_LOOP_US=1862
MASTER_RESETS=0
SLAVE_RESETS=0
```

## Decisión

```text
FIFO_REUSE_DEFAULT=ON
FIFO_REUSE_PROMOTED=YES
DIRECT_RX_DEFAULT=OFF
W5500_SPI_HZ=26000000
DATALOG_RUNTIME_PATH=MANDATORY
NEXT=H3E_R
```
