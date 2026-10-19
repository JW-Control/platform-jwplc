# Alpha13 H0-F1 — complemento físico con RJ45 conectado (19/10/2026)

**Fuente:** registro UART aportado por el operador, COM4; firmware del package F1 HEAD `2b5802720df552e01a0b4f9d4fe1a7bd2de16daf`. Maniobras realizadas manualmente por el operador, sin modificación de firmware ni `git pull`.

**Objetivo:** confirmar retorno temprano y recuperación PHY con cable RJ45 conectado tras maniobras EN y desconexión/reconexión USB. No constituye un test de sockets, DHCP, DNS ni Modbus TCP. No sustituye automáticamente la matriz estricta 20/20 de cinco contadores.

| Fase identificada por la secuencia del log | BOOT nuevo | EARLY_LINK_RETURN | READY observados | LINK observado | Gate |
| --- | --- | --- | --- | --- | --- |
| EN-A (boot con `rst:0x1`) | `7771D843` | `1` (LinkON inmediato) | COUNT 1–6, `STAGE=2` | `1` desde COUNT 1 | PASS |
| EN-B (boot con `rst:0x1`) | `1C9EB6D5` | `1` (LinkON inmediato) | COUNT 1–7, `STAGE=2` | `1` desde COUNT 1 | PASS |
| EN-C (boot con `rst:0x1`) | `195AA6BF` | `1` (LinkON inmediato) | COUNT 1–7, `STAGE=2` | `1` desde COUNT 1 | PASS |
| USB-A (COM4 se cerró/reabrió) | `BE13DC4E` | `2` (LinkOFF transitorio) | COUNT 1–11, `STAGE=2` | COUNT 1=`2`, COUNT 2–11=`1` | PASS |
| USB-B (COM4 se cerró/reabrió) | `F72EA5FC` | **no capturado** | COUNT 1–9, `STAGE=2` | COUNT 1=`2`, COUNT 2–9=`1` | PASS por progreso de sketch, retorno explícito no registrado |
| USB-C (COM4 se cerró/reabrió) | `FD127C6F` | `2` (LinkOFF transitorio) | COUNT 1–11, `STAGE=2` | COUNT 1=`2`, COUNT 2–11=`1` | PASS |

**Lectura técnica:** son seis arranques nuevos con cinco o más mensajes READY, ninguno con `TG1WDT_SYS_RESET` ni panic visible. Tres arranques alcanzan `LINK=1` ya en `setup()`; en dos se observa `EARLY_LINK_RETURN=2` y luego `LINK=1` en el segundo heartbeat. Esta variación es compatible con el tiempo de establecimiento físico de enlace después de energizar el equipo y **no es un error Ethernet**. Para `F72EA5FC` la captura se retoma en `COUNT=1`, por lo que el retorno temprano se acredita **indirectamente** mediante `STAGE=2`, no por línea UART inventada.

La división EN/USB se basa en la descripción del operador y en el cierre/reapertura del COM4; el monitor de puerto por sí solo no mide que el corte de alimentación haya sido total. La estabilidad de 20 ciclos con RJ45 desconectado ya fue reportada, pero algunas ventanas de aquel ensayo no guardaron los cinco READY requeridos: **`H0_F1_STRICT_20_OF_20=REVIEW_EVIDENCE_GAP` sigue siendo el estado formal**, hasta reconciliación o aceptación expresa de las muestras parciales.

**Decisión de ingeniería:** no repetir 20 ciclos con RJ45 conectado: el complemento realizado supera la propuesta inicial de 2 EN + 2 USB; añade cobertura de arranque PHY con enlace presente y reconexión. Mantener R1–R4, robustez de sockets/DHCP/DNS/Modbus y promoción productiva G10 como gates separados; no declarar final de Alpha13.

```text
H0_F1_ETH_CONNECTED_EN=3_OF_3_PASS
H0_F1_ETH_CONNECTED_USB=3_OF_3_PASS
H0_F1_ETH_CONNECTED_EARLY_DIRECT=5_OF_6
H0_F1_ETH_CONNECTED_EARLY_INDIRECT=1_OF_6_BY_STAGE2
H0_F1_ETH_CONNECTED_LINK_RECOVERY=6_OF_6_TO_LINK1
H0_F1_ETH_CONNECTED_WDT=NONE_OBSERVED
H0_F1_EXTENDED_20_STRICT=REVIEW_EVIDENCE_GAP
H0_F1_PRODUCT_HEAD=2b5802720df552e01a0b4f9d4fe1a7bd2de16daf
COM14=NOT_TOUCHED
```
