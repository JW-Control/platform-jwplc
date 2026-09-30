# Alpha14 — S2D DataLog Full Runtime — cierre 2026-09-30

## Resultado

```text
A14_S2D_DATALOG_FULL_RUNTIME=PASS
HARNESS_FAILURE=NO
PRODUCT_FAILURE=NO_EVIDENCE
HARDWARE_FAILURE=NO_EVIDENCE
PHYSICAL_STABILITY=PASS
```

S2D mantiene el mismo full runtime de S2 y cambia únicamente la política SD:

```text
ONLY_VARIABLE=SD_ACCESS_POLICY
S2_DIRECT=JWPLC_SD/JWPLCFile bajo nivel
S2D_DATALOG=JWPLCDataLog alto nivel
```

Configuración DataLog:

```text
BUFFER_BYTES=4096
COMMIT_THRESHOLD_BYTES=512
COMMIT_TIMEOUT_MS=5000
RECORD_BYTES=32
WRITE_PERIOD_MS=20
MANUAL_SERVICE_CALL=NO
```

El auto-servicio sigue la ruta de producto:

```text
jwplcSystemTask
 -> jwplcDataLogTickCallback()
 -> JWPLC_SD.serviceDataLogs()
 -> JWPLCDataLog.service()
```

## Resultado S2D

```text
DURATION_MS=600002

ETH_RX_BYTES=246061124
ETH_ECHO_BYTES=246061124
ETH_RX_MBPS=3.280804
ETH_ECHO_TX_MBPS=3.280804
ETH_CORRUPTION_ERRORS=0
ETH_TRANSPORT_ERRORS=0
ETH_SPI_LOCK_ERRORS=0
ETH_SPI_HOLD_MAX_US=3097

MODBUS_CYCLES_OK=16414
MODBUS_FAILURES=0
MODBUS_PATTERN_MISMATCHES=0
MODBUS_MAX_TRANSACTION_US=17402
MODBUS_REQUESTS_OK=49242
MODBUS_CRC_ERRORS=0
MODBUS_TIMEOUTS=0

DATALOG_ACTIVE=YES
DATALOG_ACCEPTED_WRITES=29164
DATALOG_ACCEPTED_BYTES=933248
DATALOG_PENDING_BYTES_AT_SNAPSHOT=384
DATALOG_COMMITTED_BYTES=932864
DATALOG_COMMIT_COUNT=1822
DATALOG_FAILED_COMMITS=0
DATALOG_MANUAL_SERVICE_CALL=NO

FRAM_OK=1198
FRAM_FAIL=0
RTC_OK=599
RTC_FAIL=0
IO_SAMPLES=29135
MASTER_MAX_LOOP_US=22006
MASTER_RESETS=0

SLAVE_MODBUS_CRC_ERRORS=0
SLAVE_FRAM_FAIL=0
SLAVE_RTC_FAIL=0
SLAVE_MAX_LOOP_US=4128
SLAVE_RESETS=0

TFT_PHYSICAL=PASS
BUTTONS_PHYSICAL=PASS
```

Los 384 B pendientes corresponden al snapshot previo al cleanup. El STOP posterior
ejecuta el commit/cierre ordenado del DataLog.

## Comparación contra S2 DIRECT

| Métrica | S2 DIRECT | S2D DataLog | Delta |
|---|---:|---:|---:|
| Ethernet RX Mbps | 3.146726 | 3.280804 | +4.261% |
| Ethernet TX echo Mbps | 3.146726 | 3.280804 | +4.261% |
| hold SPI Ethernet máx. | 3269 us | 3097 us | -5.262% |
| ciclos RTU OK | 16047 | 16414 | +2.287% |
| transacción RTU máx. | 52928 us | 17402 us | -67.121% |
| loop Master máx. | 69468 us | 22006 us | -68.322% |
| I/O samples | 28668 | 29135 | +1.629% |

No se interpreta el aumento de throughput como ganancia causal exclusiva del
DataLog; el dato principal es que el camino de producto mantiene integridad y
reduce materialmente el jitter extremo observado con el patrón SD directo.

## Conclusión

```text
DATALOG_PRODUCT_RUNTIME=PASS
DATALOG_AUTOSERVICE=PASS
DATALOG_FAILED_COMMITS=0
DATALOG_FULL_RUNTIME_COEXISTENCE=PASS
DIRECT_SD_JITTER_NOT_REPRESENTATIVE_OF_PRODUCT_DATALOG=YES
```

La ruta de microSD para futuros gates de runtime queda fijada en
`JWPLCDataLog`. El acceso directo queda reservado a pruebas explícitas de bajo
nivel.

## Siguiente

1. Promover FIFO_REUSE como default del package.
2. Gate corto compilando el default sin override de sketch.
3. SINGLE_STATUS: adoptar la política en schedulers que ya posean el resultado
   de conexión dentro de la misma pasada; no crear cache persistente.
4. H3E-R con 1000 req/s TCP + RTU ~50 Hz + DataLog real + periféricos.
5. P4.1 DLEN_REUSE.
