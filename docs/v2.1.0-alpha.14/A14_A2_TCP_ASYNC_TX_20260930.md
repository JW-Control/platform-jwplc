# Alpha14 — A2 — TX TCP asíncrono cooperativo

Fecha: `2026-09-30`

## Resultado

```text
A14_A2_TCP_ASYNC_TX=PASS_DATA_ONLY
FLUSH_PENDING_PATH=PASS
RX_CHANGED=NO
CORRUPTION_ERRORS=0
SPI_LOCK_ERRORS=0
DEVICE_RESETS=0
LINK_FINAL=UP
PHYSICAL_STABILITY=PENDING_USER
```

Se añadió a `EthernetClient` un motor TX cooperativo y aditivo:

```text
beginWriteAsync(...)
pollWriteAsync()
writeAsyncInProgress()
cancelWriteAsync()
```

`write()` conserva su firma y ruta bloqueante previa. El cambio no modifica
ninguna ruta RX. Cada llamada cooperativa realiza como máximo una consulta o
una transición: espera espacio TX, copia el payload, emite `SEND` y consulta
`SEND_OK/TIMEOUT` en polls posteriores.

La cancelación antes de copiar solo descarta el estado software. Después de
emitir `SEND`, cierra el socket para impedir que un `SEND_OK` antiguo sea
consumido por una operación posterior. El caller debe conservar el buffer
válido e inmutable únicamente mientras el motor espera espacio TX.

## Gate físico

Root de evidencia:

```text
C:\Users\jeykc\AppData\Local\Temp\jwplc_a14_a2_tcp_tx_plj5fpl3
HEAD=a9471e13e2a7c950e4dc8dc6f8a6eec1b4311bb4
COMPILE_EXIT=0
UPLOAD_EXIT=0
BOOT_ID=4248929082
```

| Caso | Resultado | begin máx. | poll write máx. | poll flush máx. | Writes | Bytes |
|---|---|---:|---:|---:|---:|---:|
| NORMAL inicial | PASS | 2,383 us | 40 us | 0 us | 1 | 1,536 |
| FLUSH con SEND pendiente | PASS | 2,369 us | 0 us | 88 us | 1 | 1,536 |
| CANCEL después de SEND | PASS | 1,890 us | 0 us | 0 us | 0 | 0 |
| PEER_CLOSE | PASS | 169 us | 0 us | 0 us | 0 | 0 |
| TIMEOUT por ventana TCP saturada | PASS | 2,536 us | 124 us | 0 us | 1 | 2,048 |
| NORMAL después de recuperación | PASS | 1,907 us | 39 us | 0 us | 1 | 1,536 |

El límite por llamada era `10,000 us`; el peor `begin` fue `2,536 us` y el
peor poll fue `124 us`.

## Integridad y recuperación

```text
INTEGRITY_CASES_PASS=3
RECONNECT_NORMAL_PASS=2
WRITE_PENDING_OBSERVED=YES
FLUSH_PENDING_OBSERVED=YES
CANCEL_CLOSED_SOCKET=YES
TIMEOUT_NATURAL=PASS
CORRUPTION_ERRORS=0
SPI_LOCK_ERRORS=0
DEVICE_RESETS=0
LINK_FINAL=UP
```

Los tres casos que debían entregar datos compararon longitud y FNV-1a exactos.
El timeout se provocó sin hooks de producto: el peer mantuvo la conexión sin
leer y anunció una ventana RX pequeña hasta que el W5500 dejó un SEND
pendiente. El mismo `BOOT_ID` estuvo presente antes, durante y después de los
seis casos.

## Artefactos

```text
FIRMWARE_SHA256=248B2DB9774CC11221D5F682CDC7D2C197E74601DAF48CE6D586DC085ADBE6B7
GATE_SHA256=04AE9C39F3405A2E3BBCEA53AB1DF0B6BB95590041F369A2D4462D6D8D208CB6
RUNNER_SHA256=588869EFDFC1CE3CC2F41E7F3D3CADB91C2BEA6C55ADEFE4B03AF4EFF479F0C7
```

## Decisión

```text
A2_DECISION=KEEP_ADDITIVE_API
FLUSH_PENDING_PATH=PASS
NEXT=S1_10MIN_STABILITY_SOAK
PHYSICAL_STABILITY=PENDING_USER
ALPHA14_CLOSED=NO
```
