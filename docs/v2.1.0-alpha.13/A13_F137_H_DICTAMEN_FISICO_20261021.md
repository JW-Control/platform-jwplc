# Alpha13 F137-H — Dictamen de validación física mínima (21/10/2026)

**Resultado:** `PASS_FUNCIONAL_ACOTADO` para el package candidato con archivos precompilados de `JWPLC_Ethernet` y `JWPLC_ModbusTCP` (PR #125 Draft). **No es cierre ni publicación de Alpha13**. Evidencia física aportada por el operador mediante consola PowerShell y captura de monitores VS Code; no se asume acceso remoto del asistente al hardware.

## Banco y autorización

- Placas de desarrollo JWPLC Basic: **COM4 Master** con microSD y **COM14 Slave** sin microSD, conectadas entre sí por RS-485. Ambos PLC en LAN Ethernet de laboratorio con DHCP; IP privadas distintas. Salidas Q sin cargas según contrato del gate.
- Por decisión del propietario del 17/10/2026, el firmware previo de las **placas de pruebas** es prescindible: no solicitar backup de sketches ni full-flash. No aplicar esta excepción a placas instaladas o de clientes.
- `tools/alpha13/h_f137_dual.py`: `--inventory` PASS dos CH340 distintos, `--selftest` PASS, `--prepare-slave` PASS reutilizando sello Master, `--check-dual` PASS. Operador confirmó en consola autorización conjunta y ejecutó `--upload-both` (primero COM14, después COM4). **Resultados:** `H_F137_UPLOAD_COM14=COMMAND_SUCCESS_UART_PENDING`, `H_F137_UPLOAD_COM4=COMMAND_SUCCESS_UART_PENDING`, `H_F137_DUAL_UPLOAD=COMMAND_SUCCESS_2_OF_2_UART_PENDING`. Sin `erase-all`, restore ni acceso a cliente. Los recibos de upload acreditan comando finalizado, no lectura de flash posterior.

## Observación UART física (115200, ventana visible)

- En COM4 y COM14, `PHY=1`, `ETH=1`, `TCP=1`; DHCP/estado de IP privadas diferentes. BOOT nonce estable y heartbeat creciente en el tramo visible, sin `TG0/TG1`, panic ni cambios de nonce observados.
- RTU: Master `RTU_OK` crece y `RTU_FAIL=0`; Slave `RTU_REQUESTS` crece y `RTU_CRC=0`. Esto acredita comunicación RTU real en la ventana visible.
- En Master: `SD_OK` y `FRAM_READ` aumentan, `SD_FAIL=0`, `RTC=1`, `IO=1`, `TFT=1`. Se observaron estos estados, no se realizaron escrituras en Q, SD o FRAM.
- **Límite de procedencia:** la captura visible contiene heartbeats, pero NO muestra línea `VER=G10_PROMOTED_PRODUCT_F137_V1` del arranque. Ambos builds sellados contenían el marcador esperado y ambos uploads finalizaron SUCCESS, pero **no declarar `BOOT_MARKER_UART=PASS`** a partir de esa captura. No se justifica otro upload ni reinicio solo para buscar la línea.

## Transacciones de red independientes — evidencia enviada por el operador

Se realizaron después de los uploads, desde el PC en la misma LAN. No se escribieron registros Modbus, ni se abrió USB para estas sondas.

**FC03 Modbus TCP** con `g10p_modbus_tcp_readonly.py`:
```text
G10P_TCP_FC03_COM4_MASTER=PASS_4_REGISTERS
G10P_TCP_FC03_COM14_SLAVE=PASS_4_REGISTERS
G10P_TCP_DUAL_ROUTER=PASS_2_DIFFERENT_IPS
G10P_TCP_WRITE=NOT_RUN COM4_SERIAL=NOT_OPENED COM14_SERIAL=NOT_OPENED
```

**UDP eco COM4** con `h_f137_udp_echo.py --samples 3`:
```text
F137_UDP_SELFTEST=PASS_4_NEGATIVES_NO_NETWORK_NO_USB
F137_UDP_COM4=PASS_3_OF_3_ECHO
F137_UDP_LATENCY_MS=0.00,0.00,0.00
F137_UDP_USB=NOT_OPENED
F137_UDP_MODBUS_WRITE=NOT_RUN
```
Los `0.00 ms` son salida redondeada del cronómetro; **no demostrar latencia cero** ni usar estos datos como benchmark Ethernet.

**Nota sobre capturas:** en una captura de heartbeat previa a la ejecución del eco UDP aparecía `UDP_RX=0 TX=0`; no representa fallo ni debe trasladarse como contador posterior al probe. El PASS 3/3 es evidencia de eco de datagramas extremo a extremo, sin exigir una nueva captura innecesaria.

## CI que sustenta el firmware y scope

- Compilación/benchmark F137 y paridad de archives: [#39531758494](https://github.com/JW-Control/platform-jwplc/actions/runs/39531758494), tareas de N2 y mediciones componentes PASS, aunque run global terminó falla del primer harness N3 de Windows (posteriormente corregido).
- **N3 definitiva:** [#39538155047](https://github.com/JW-Control/platform-jwplc/actions/runs/39538155047), 13/13 Windows y 13/13 Ubuntu PASS.
- **Build Master:** [#39551136908](https://github.com/JW-Control/platform-jwplc/actions/runs/39551136908) PASS ambos SO.
- **Build dual Master/Slave:** [#39622285204](https://github.com/JW-Control/platform-jwplc/actions/runs/39622285204) PASS global ambos SO en HEAD `e60934960c582a520abcd18ca12598747598b766`.
- **UDP sintético:** [#39625796508](https://github.com/JW-Control/platform-jwplc/actions/runs/39625796508), jobs Ubuntu y Windows `SUCCESS` observados; verificar conclusión global del run si aún está en finalización. El PASS físico 3/3 es evidencia independiente.

## Límites que NO quedan convertidos en PASS

- Marcador UART BOOT F137 de ambas placas no fue capturado textualmente (identidad del binario establecida por cadena de sellos/hashes + upload SUCCESS, **sin readback**).
- No se ejecutó reconexión RJ45, ciclo R2, campaña de estrés de 605/1805 s, leak-check R3 interno, E1/E2, DNS/DHCP exhaustivo ni medición de socket leases internos en este gate. Reutilizar evidencias anteriores **solo** con procedencia verificable; respetar riesgos aceptados R3/R4.
- Basic Core `jwplcbasiccore` queda validado en N2/N3 de código/ELF, **no** físicamente aquí. No inferir control físico de W5500, GPIO ni estado de su red.
- El gate no constituye revisión final de integración con la rama base, revisión de PR, release, pre-release ni instalación desde ZIP público/IDE.

```text
F137_H_DUAL_USB_UPLOAD=PASS_COMMAND_2_OF_2
F137_H_UART_RUNTIME=PASS_BOTH_ROLES_VISIBLE_WINDOW
F137_H_ETHERNET_PHY_DHCP_TCP_STATUS=PASS_BOTH
F137_H_RTU_MASTER_SLAVE=PASS_COUNTERS_AND_CRC_ZERO
F137_H_FC03_COM4=PASS_4_REGISTERS
F137_H_FC03_COM14=PASS_4_REGISTERS
F137_H_UDP_COM4=PASS_3_OF_3
F137_H_MASTER_PERIPHERALS=PASS_OBSERVED_READONLY
F137_H_BOOT_MARKER_CAPTURE=PENDING_NOT_REFLASH_REQUIRED
F137_H_VERDICT=PASS_FUNCIONAL_ACOTADO
F137_H_PRODUCT_BASIC_CORE_PHYSICAL=NOT_TESTED
PR125=DRAFT_NO_MERGE
RELEASE=BLOCKED
BASE_SYNC=STATUS_DOC_ONLY_PRODUCT_NOT_PROMOTED
KNOWLEDGE_SYNC=PR27_DRAFT
NEXT_GATE=F137_C_REVIEW_PR125_BASE_RECONCILIATION_AND_RELEASE_PREP
```
