# Alpha14 — S1 — soak TX TCP asíncrono

Fecha: `2026-09-30`

## Resultado

```text
A14_S1_TCP_ASYNC_TX_SOAK=PASS_DATA_ONLY
TRAFFIC_DURATION_MS=600002
PAYLOAD_INTEGRITY=PASS
CORRUPTION_ERRORS=0
SPI_LOCK_ERRORS=0
TRANSPORT_ERRORS=0
DEVICE_RESETS=0
RECOVERY_RECONNECT=PASS
LINK_FINAL=UP
PHYSICAL_STABILITY=PENDING_USER
ALPHA14_CLOSED=NO
```

S1 mantuvo una única conexión TCP con tráfico TX cooperativo durante más de
10 minutos. El PC verificó cada byte en streaming contra el patrón esperado,
sin guardar el flujo completo en memoria. Al cerrar esa conexión, abrió otra
sesión y repitió tráfico durante 2 segundos para probar recuperación.

## Métricas

| Métrica | Soak principal | Recuperación |
|---|---:|---:|
| duración de tráfico | 600.002 s | 2 s solicitados |
| writes completados | 242,187 | — |
| bytes verificados | 371,999,232 | 1,271,808 |
| throughput sostenido | 4.959973 Mbps | — |
| `beginWriteAsync` máximo | 3,159 us | incluido en el gate |
| `pollWriteAsync` máximo | 3,258 us | incluido en el gate |
| corrupción | 0 | 0 |
| fallos SPI | 0 | 0 |
| errores de transporte | 0 | 0 |
| resets del dispositivo | 0 | 0 |

El límite por llamada cooperativa era `10,000 us`; los máximos quedaron por
debajo de un tercio del límite. El `BOOT_ID=1492429159` permaneció constante
desde el snapshot inicial hasta el final.

## Contrato del gate

```text
PACKAGE_DEVELOPMENT_MODE=SOURCE_FIRST
RX_CHANGED=NO
SPI_W5500_HZ=26000000
PERSISTENT_TCP_CONNECTION=YES
PAYLOAD_BYTES_PER_WRITE=1536
WRITE_PENDING_OBSERVED=YES
RECOVERY_AFTER_CLOSE_REOPEN=PASS
LINK_FINAL=UP
```

## Evidencia

```text
HEAD=766334b7625f97a0acf5b77d2b85aea6a169b1ef
RESULT_ROOT=C:\Users\jeykc\AppData\Local\Temp\jwplc_a14_s1_tcp_soak_4o3x73a1
CASE_LOG=C:\Users\jeykc\AppData\Local\Temp\jwplc_a14_s1_tcp_soak_4o3x73a1\case.log
COMPILE_LOG=C:\Users\jeykc\AppData\Local\Temp\jwplc_a14_s1_tcp_soak_4o3x73a1\compile.log
UPLOAD_LOG=C:\Users\jeykc\AppData\Local\Temp\jwplc_a14_s1_tcp_soak_4o3x73a1\upload.log
```

```text
FIRMWARE_SHA256=F20D402C956EE9A7E653A1573A439EB4BBF99D674FCA83F0CF1A0EB41D672DCB
GATE_SHA256=10BEF94ED42DDA9B05CE19D3916408A8DFCCF3DFE9CC7D39B884B0B048B4729B
RUNNER_SHA256=51B9F54B0A1785294ACD88F0C34E4FE09819CB6817C80C7DCB4E5477025086BE
```

## Decisión

S1 satisface la aceptación automática de estabilidad Ethernet del roadmap.
La revisión de TFT y periféricos no se puede observar remotamente y queda
pendiente del usuario. No se recomienda cambiar flags OFF por default hasta
completar esa revisión.

```text
S1_DECISION=PASS_DATA_ONLY
OPTIONAL_NEXT=S2_SHARED_SPI_PERIPHERAL_SOAK_IF_ENVIRONMENT_AVAILABLE
PHYSICAL_STABILITY=PENDING_USER
```
