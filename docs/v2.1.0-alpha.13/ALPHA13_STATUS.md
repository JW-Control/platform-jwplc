# ESTADO VIGENTE — 20/10/2026 — G10-H R2: UART original no grabada, captura complementaria mínima

**Respuesta operador:** en los tres ciclos R2 completados no se guardó registro UART de COM4 ni COM14. No atribuir a esos ciclos transiciones PHY observadas, reconexión DHCP/renovación de lease ni liveness ininterrumpida RTU; el resultado existente continúa `R2_NETWORK=PASS_3_OF_3` con recuperaciones 1,69 / 10,05 / 6,03 s.

**Decisión operativa sin tocar producto:** ejecutar **un ciclo complementario R2-UART** manual, no relanzar `--phase R2` (que haría otros 3 ciclos). Abrir monitor COM4 115200 ANTES de la maniobra; dejarlo abierto durante toda la ventana y obtener varias líneas heartbeat de baseline, caída RJ45 SOLO COM4 12–15 s, retorno y estabilidad durante 20–30 s. Registrar BOOT estable durante el ciclo, HB progresivo, `PHY != 1` durante desconexión y `PHY=1 ETH=1 TCP=1`/IP tras reconectar; RTU_OK progresivo y RTU_FAIL sin nuevos errores. El monitor serie puede provocar reset inicial por DTR/RTS: esperar nuevo baseline estable **antes de manipular RJ45** y nunca confundir ese reset de apertura con watchdog de la campaña. En COM14, si es viable, comprobar `RTU_REQUESTS` progresivo y `RTU_CRC` estable después del ciclo. Cerrar gate de enlace solo si la captura respalda los marcadores, sin exigir MAC ni backup.

**Límites deliberados:** esta captura complementaria verifica comportamiento UART **en un cuarto ciclo**, no retroactivamente en cada uno de los tres anteriores. IP recuperada con `ETH=1` no demuestra necesariamente nueva transacción DHCP, renovación de lease ni una consulta DNS desde el PLC. Si esos subcriterios siguen obligatorios para cerrar R2 integral, documentar `R2_DHCP_DNS=NOT_TESTED` y diseñar gate específico; nunca asignar PASS solo por la prueba del PC. Mantener R3/R4 sin ejecutar, F136 reservado, PR #124 Draft, BASE_SYNC y KNOWLEDGE_SYNC pendientes.

```text
ALPHA13=OPEN
NEXT_FAILURE_ID=F136
G10_H_R2_NETWORK=PASS_3_OF_3
G10_H_R2_ORIGINAL_UART=NOT_CAPTURED_COM4_COM14
G10_H_R2_SUPPLEMENTARY_UART=READY_NOT_RUN_1_MANUAL_CYCLE
G10_H_R2_DHCP_HANDSHAKE=NOT_TESTED
G10_H_R2_DNS=NOT_TESTED
G10_H_R2_FULL=PENDING_SCOPE_EVIDENCE
G10_H_R3=NOT_RUN_INTERNAL_OPEN
G10_H_R4=NOT_RUN
BASE_SYNC=PENDING_INTENTIONAL
KNOWLEDGE_SYNC=PENDING_F135_REUSABLE_LESSON
G10_E1=PENDING_GDB_UNAVAILABLE
G10_E2=PENDING_SDKCONFIG_NOT_LOCATED
G10_PR124=DRAFT_NOT_MERGED
NEXT_GATE=G10_H_R2_SUPPLEMENTARY_COM4_UART_ONE_CYCLE
```

---
# ESTADO VIGENTE — 20/10/2026 — G10-H R2 red 3/3 PASS, UART PHY/DHCP pendiente

**Evidencia aportada por operador:** PowerShell en rama candidata, HEAD ejecutor `0959781211bc016016c2a626f14ec77559e9085f` y firmware instalado/sellado `7c1974c659be4aa88c877a849db6efc32673da60`. Ejecutó `g10p_r1_r4_campaign.py --phase R2` con el banco dual ya validado y maniobras confirmadas `CONFIRMO` (desconectar/reconectar SOLO RJ45 COM4, mantener COM14 y RS-485). Se conserva `G10P_R2_RESULT.json` local en carpeta temporal del operador; el JSON fue pegado en la conversación, **no se incorporó al repo ni se verificó su SHA de archivo**.

**Evidencia de red:** tres ciclos completos con 3 intentos TCP fallidos **esperados** en COM4 durante la desconexión por cada ciclo (9 en total); el runner verificó TCP FC03 en COM14 durante las caídas y después TCP FC03 a ambos más eco UDP COM4. Recuperaciones observadas: ciclo 1 **1,69 s**, ciclo 2 **10,05 s**, ciclo 3 **6,03 s**; máximo 10,05 s frente a 96 s permitidos. Las 3 recuperaciones de COM4 se produjeron sobre la misma IP conocida. Salida final: `G10P_R2_NETWORK=PASS_3_CYCLES_UART_PHY_DHCP_PENDING`.

**Alcance y cautelas:** el JSON conserva correctamente `status=NETWORK_RECOVERY_PASS_UART_PHY_DHCP_PENDING`, y cada `operator_uart_confirmation=PENDING`. No se entregaron UART de las transiciones R2 en este turno. No afirmar aún que PHY cayó/volvió en cada ciclo, que DHCP hizo un nuevo acuerdo, que RTU permaneció operativo sin cortes ni que no hubo reset durante cada intervalo. Tampoco se ejecutó DNS ni falla de DHCP renewal. `R2_NETWORK=PASS_3_OF_3`, `R2_FULL=REVIEW_PENDING_UART`; **no repetir red R2 automáticamente**. Solicitar extractos UART del Master y, si existen, Slave en cada transición (BOOT idéntico, HB creciente, PHY fuera de 1 → 1, IP/ETH/TCP recuperados, RTU_OK sin nuevos fallos). Si no se guardaron, acordar captura dirigida de validación pendiente sin falsificar los tres ciclos.

**No se ha iniciado R3 ni R4.** Mantener PR #124 Draft, `BASE_SYNC=PENDING`, `E1/E2=PENDING`, `KNOWLEDGE_SYNC=PENDING_F135`, `NEXT_FAILURE_ID=F136`. No cambió firmware, package, GPIO4 ni archivos `.a` y no hubo upload.

```text
ALPHA13=OPEN
NEXT_FAILURE_ID=F136
G10_H_R1=PASS_WITH_POST_WINDOW_UART
G10_H_R2_NETWORK=PASS_3_OF_3
G10_H_R2_TCP_DOWN_SAMPLES=3_3_3
G10_H_R2_RECOVERY_SECONDS=1.69_10.05_6.03
G10_H_R2_COM14_FC03_DURING_COM4_DOWN=PASS_OBSERVED_BY_RUNNER
G10_H_R2_FULL=REVIEW_PENDING_UART_PHY_DHCP_BOOT_RTU
G10_H_R2_DNS=NOT_RUN
G10_H_R3=NOT_RUN_INTERNAL_OPEN
G10_H_R4=NOT_RUN
G10_H_PACKAGE_EDIT=NO
G10_H_UPLOAD=NOT_RUN
G10_E1=PENDING_GDB_UNAVAILABLE
G10_E2=PENDING_SDKCONFIG_NOT_LOCATED
PENDIENTE_DE_PROMOCION=PR124_10_MODIFIED_2_ADDED
BASE_SYNC=PENDING_INTENTIONAL
KNOWLEDGE_SYNC=PENDING_F135_REUSABLE_LESSON
G10_PR124=DRAFT_NOT_MERGED
NEXT_GATE=G10_H_R2_UART_EVIDENCE_RECONCILIATION_BEFORE_R3
```

---
# ESTADO VIGENTE — 20/10/2026 — G10-H R1 físico PASS, NEXT_GATE R2 link recovery

**Fuente de evidencia:** operador Windows PowerShell, dos extractos UART y archivo externo `G10P_R1_RESULT.json` (local, **no** incorporado ni adjuntado al repositorio). SHA-256 del JSON recibido: `dd64b76c0cc317450ddbfb203fa551d56799007326895179cb65f9af6b24fec8`. Script/HEAD `3ebfb96bbeea50a3221af228497b5c3612fce12f`, `firmware_head=7c1974c659be4aa88c877a849db6efc32673da60`. Ejecutado 20/10/2026, duración medida 605,0 s, protocolo G10-H R1 previsto 605 s. `--selftest` y `--check` locales PASS. `--phase R1` emitió `G10P_R1_NETWORK=PASS_589_SAMPLES`. Estado generado automáticamente `NETWORK_PASS_UART_PERIPHERALS_PENDING`, reconciliado con extractos UART aportados **posteriormente** por el operador.

**589 ciclos completos sin error TCP/UDP de los que se reportó, 1767 operaciones:** 589 lecturas Modbus TCP FC03 4 registros exactos de COM4, otras 589 de COM14 y 589 ecos UDP de COM4 en puerto 15020. `read()` comprueba Unit1/TID/protocolo/4 registros; echo confirma contenido y remitente/puerto. Estadísticos calculados sobre JSON (ms, resolución de reloj Windows, valores `0` son redondeo y no latencia nula):

| Canal | Media | P95 | P99 | Máx. |
| --- | ---: | ---: | ---: | ---: |
| TCP/502 COM4 Master | 14,679 | 31 | 32 | 47 |
| TCP/502 COM14 Slave | 11,550 | 31 | 32 | 125 |
| UDP echo COM4:15020 | 0,959 | 15 | 16 | 16 |

**Observaciones UART inmediatamente posteriores:** COM4 HB=2368..2379, mismo `BOOT=FEEDC504` de PH0, `PHY=1 ETH=1 TCP=1`; `UDP_RX=589 UDP_TX=589`, mismos contadores que tráfico 589, RTU_OK 2832→2846, RTU_FAIL **40 sin aumento**, SD_OK 3157→3172, SD_FAIL=0 y FRAM_READ 3157→3172, RTC=1 IO=1 TFT=1. COM14 HB=3000..3009, mismo `BOOT=D998B0A7` de PH0, `PHY=1 ETH=1 TCP=1 RTU=1`, RTU_REQUESTS 2865→2877, CRC=0. UART se abrió *después* de finalizar la ventana: prueba continuidad entre PH0 y final, no serie capturada a cada segundo dentro de R1.

**Dictamen:** `G10_H_R1=PASS_WITH_POST_WINDOW_UART_EVIDENCE` y producto instalado sin WDT/reinicio observado, RTU y periféricos activos al finalizar tráfico concurrente. No inferir ausencia absoluta de starvation SPI, uso de heap o límites internos de socket: no instrumentados. El evento de una latencia COM14 de 125 ms no causó fallo ni timeout. No repetir R1 sin variación de inputs/producto.

**Siguiente gate autorizado:** `R2` 3 ciclos independientes de *retirar SOLO RJ45 COM4*, mantener USB, alimentación, RTU, COM14 y PC intactos; cada ciclo 12 s link down y recuperación de FC03+UDP en máximo 96 s. Operador confirmar maniobras con `CONFIRMO` en runner; grabar UART en COM4 y COM14 para `BOOT` estable, `PHY`, DHCP y RTU. No confundir servidor inaccesible por IP con detección directa de PHY/DHCP. Si el IP cambia tras DHCP, detener, documentar y conciliar; no reflashear. No hay permiso de merge hasta R2–R4 y aceptación explícita de limitaciones R3 y E1/E2.

```text
ALPHA13=OPEN
NEXT_FAILURE_ID=F136
G10_PH0=PASS_FUNCTIONAL_PHYSICAL
G10_H_R1=PASS_WITH_POST_WINDOW_UART_EVIDENCE
G10_H_R1_DURATION_S=605
G10_H_R1_ROUNDS=589
G10_H_R1_TCP_COM4_FC03=PASS_589
G10_H_R1_TCP_COM14_FC03=PASS_589
G10_H_R1_UDP_COM4_ECHO=PASS_589_COUNTER_MATCH
G10_H_R1_RTU=PASS_POSTWINDOW_NO_ADDITIONAL_ERRORS
G10_H_R1_SD_FRAM_RTC_IO_TFT=PASS_POSTWINDOW_OBSERVATION
G10_H_R2=READY_NOT_RUN_3_CYCLES_12S_96S
G10_H_R3=EXTERNAL_NOT_RUN_INTERNAL_OPEN
G10_H_R4=NOT_RUN_1805S
G10_H_FW_CHANGES=NO
G10_H_UPLOAD=NOT_RUN
G10_E1=PENDING_GDB_UNAVAILABLE
G10_E2=PENDING_SDKCONFIG_NOT_LOCATED
PENDIENTE_DE_PROMOCION=PR124_10_MODIFIED_2_ADDED
BASE_SYNC=PENDING_INTENTIONAL_UNTIL_R1_R4_AND_MERGE_DECISION
KNOWLEDGE_SYNC=PENDING_F135_REUSABLE_LESSON
G10_PR124=DRAFT_NOT_MERGED
NEXT_GATE=G10_H_R2_THREE_LINK_LOSS_RECOVERY_CYCLES
```

---
# ESTADO VIGENTE — 20/10/2026 — G10-H R1–R4 autorizado antes de merge, runner N0 CI PASS

**Decisión propietario:** realizar validación R1–R4 antes de fusionar PR #124. Alcance inmediato: preparar y ejecutar tráfico físico **desde PC a ambos PLC ya flasheados**, sin tocar firmware, `JWPLC/`, GPIO4, archives, puertos COM mediante el runner, MAC ni backup. Router de pruebas aislado, Q sin cargas; **R2 requiere maniobras RJ45 confirmadas por operador**. G10-H se ejecuta sin reutilizar el overlay G10-R como firmware del producto.

**Baseline físico reutilizado:** G10-PH0 en COM4 y COM14 PASS funcional (UART RTU real, DHCP dos IP, TCP FC03 real dos servidores) sobre binarios compilados originalmente en `c08147eedf` con fuentes productivas del HEAD `7c1974c659be4aa88c877a849db6efc32673da60`, sellados SHA 2/2. `E1/E2=PENDING`. PR sigue Draft; base Alpha13 intencionalmente desfasada.

**Herramienta nueva versionada:** `tools/alpha13/g10_promoted_bench/g10p_r1_r4_campaign.py` junto a README y workflow `alpha13-g10-ph0-r-campaign.yml`. Usa Modbus TCP FC03 solamente y UDP eco puerto 15020 del Master; no flash, serie ni lecturas MAC. Un modo/una evidencia local por fase para detenerse ante REVIEW_STOP; RFC ventanas **del runner G10-R original**: R1 605 s, R2 3 ciclos con 12 s sin enlace y 96 s máximo para recuperar, R4 1805 s. R3 externo 90 s es únicamente prueba funcional de churn de conexiones, **no demuestra BUS_BUSY ni tokens/stale/deferred close** y no reemplaza la inyección interna. DNS no está instrumentado por el firmware PH0, tampoco se declaran memoria/sockets SPI PASS por tráfico de PC sin UART complementario.

**CI N0 del nuevo runner:** [run #39477304043](https://github.com/JW-Control/platform-jwplc/actions/runs/39477304043), SHA de ejecución `6ef8fdebeb245699250ff67a8d159e6d5945ef78`, Windows y Ubuntu 2/2 SUCCESS, sin pasos omitidos en jobs de protocolo; pruebas sintéticas FC03, direcciones privadas, protocolo/ventanas, guardia no upload/serial. Es evidencia NO física. No hay nueva compilación Arduino ni prueba de hardware: no hubo cambios productivos.

**Revisión source-backed R3:** la rama candidata contiene `StatusReadOutcome::{Read,BusBusy,Stale}` y cierre diferido con token en `JWPLC_W5500_SafeAccess.cpp`, `EthernetClient.cpp` y `JWPLC_ModbusTCP_Client.cpp`. El sketch PH0 actual no llama los cinco probes del overlay (UDP, DNS, TCP, DHCP_CANCEL, DHCP_FINISH), ni publica socket/heap telemetry. Una campaña externa de cierres/reaperturas solo ejercita el observable; mantener G10-H R3 completo OPEN hasta decisión del mecanismo de evidencia. No modificar el package para acomodar una sonda experimental.

```text
ALPHA13=OPEN
NEXT_FAILURE_ID=F136
G10_PH0=PASS_FUNCTIONAL_PHYSICAL_ALREADY_RECORDED
G10_H_SCOPE=R1_R2_R3_R4_APPROVED_BY_OWNER_BEFORE_MERGE
G10_H_R1=READY_NOT_RUN_605S
G10_H_R2=READY_NOT_RUN_3_CYCLES_12S_96S
G10_H_R3_EXTERNAL=READY_NOT_RUN_90S
G10_H_R3_INTERNAL=OPEN_FAULT_INJECTION_UNAVAILABLE_IN_INSTALLED_SKETCH
G10_H_R4=READY_NOT_RUN_1805S_MEMORY_SPI_TELEMETRY_LIMITED
G10_H_R_N0_CI=PASS_2_OF_2_RUN_39477304043_SHA_6EF8FDEB
G10_H_PRODUCT_CHANGE=NO
G10_H_UPLOAD=NOT_RUN
G10_H_FIELD_TRAFFIC=NOT_RUN
G10_E1=PENDING_GDB_UNAVAILABLE
G10_E2=PENDING_SDKCONFIG_NOT_LOCATED
PENDIENTE_DE_PROMOCION=PR124_10_MODIFIED_2_ADDED_NOT_IN_ALPHA13_BASE
BASE_SYNC=PENDING_INTENTIONAL_R1_R4_BEFORE_MERGE
KNOWLEDGE_SYNC=PENDING_F135_REUSABLE_LESSON
G10_PR124=DRAFT_NOT_MERGED
NEXT_GATE=G10_H_R1_605S_FIELD_RUN_AND_UART_EVIDENCE
```

---
# ESTADO VIGENTE — 20/10/2026 — G10-PH0 FÍSICO PASS (COM14+COM4, RTU + router DHCP + Modbus TCP FC03)

**Fuente de evidencia física:** salidas CLI y dos capturas UART remitidas por el operador el 20/10/2026, con hardware de banco y package G10 funcional promovido exclusivamente en PR #124. Último HEAD **usado en upload**, `7c1974c659be4aa88c877a849db6efc32673da60`; último commit productivo `JWPLC/` anterior al kit de uploader y sin modificación posterior (último código productivo `ee27c73d97dd...`). Firmware de ambas placas sin instrumentación G10-R, sin MAC leída explícitamente y sin backup por decisión del operador.

**Identidad de software previa:** `g10p_ph0_upload.py --check` PASS en Windows; dos carpetas de compilación reales `g10p_ph0_c08147eedf3b_*` verificadas por `--seal-existing` `PASS_VERIFIED_NO_USB` contra HEAD `7c1974c6`, con manifiestos y SHA de ELF, binarios, fuentes/objetos y archives. `F135` (el verificador esperó `Sketch.elf` y no `Sketch.ino.elf`) **RESOLVED_LOCAL_SEAL_2_OF_2**, sin defecto de producto demostrado; resultado de la **nueva CI postbuild** permanece **NOT_CONFIRMED** (no convertir el workflow de promoción en CI de build dual). `NEXT_FAILURE_ID=F136`.

**COM14 Slave:** `--upload-com14` devolvió `G10P_PH0_UPLOAD_COMMAND=SUCCESS_COM14_SLAVE`. UART a 115200: 156 heartbeats consecutivos HB=60..215, `BOOT=D998B0A7` inmutable, sin reset/watchdog/panic visible. A HB108 el PHY pasó a `1`; desde HB114 hasta HB215, IP DHCP `192.168.0.31`, `PHY=1 ETH=1 TCP=1`, con RTU listo `RTU=1` y CRC `0`. El log COM14 fue capturado **antes de conectar RTU**, por lo que `RTU_REQUESTS=0` allí no equivale a defecto del Slave. Su banner BOOT inicial no está capturado.

**COM4 Master:** `--upload-com4` completado según la continuación del operador y el firmware observó `BOOT=FEEDC504`, 175 heartbeats consecutivos HB=21..195 sin reset/watchdog/panic. El operador indica secuencia *arranque sin RTU/TCP → conexión RS-485 → conexión RJ45*. Mientras RTU no estaba conectado `RTU_FAIL` subió a 40; desde HB81 al conectar, `RTU_OK` creció de 1 a 142 (HB195) y **RTU_FAIL permaneció 40**, confirmando FC03 RTU real con 4 registros esperados sin nuevos errores observados. PHY=1 a HB148; DHCP otorgó IP `192.168.0.159` a HB164; `ETH=1 TCP=1` sostenidos hasta HB195 mientras RTU_OK continuó creciendo. `SD_FAIL=0`, `SD_OK` y `FRAM_READ` crecieron de 28 a 260; `RTC=1 IO=1 TFT=1` constantes. En este extracto `UDP_RX=0 UDP_TX=0`: UDP no fue ejercitado.

**COM4+COM14 router aislado:** el operador ejecutó `g10p_modbus_tcp_readonly.py --com4-ip 192.168.0.159 --com14-ip 192.168.0.31`, obtuvo exactamente `G10P_TCP_FC03_COM4_MASTER=PASS_4_REGISTERS`, `G10P_TCP_FC03_COM14_SLAVE=PASS_4_REGISTERS`, `G10P_TCP_DUAL_ROUTER=PASS_2_DIFFERENT_IPS`. No hubo escrituras ni acceso serial desde el cliente TCP. Este gate prueba solicitudes reales TCP/502 Unit1 FC03 a ambos PLC y confirma IP DHCP distintas, pero **no prueba tráfico TCP directo PLC→PLC**, UDP echo, DNS, cierre/reuso bajo contención o estabilidad de larga duración.

**Decisión de alcance:** G10-PH0 pasa como validación *funcional física* de package candidato. La campaña profunda G10-H sigue separada y no autorizada: R1 smoke sostenido incluye UDP + convivencia autoload/latencia; R2 desconexión/reconexión link, DHCP/DNS y recuperación TCP/UDP; R3 BUS_BUSY/lease/socket lifecycle; R4 endurance/fairness/starvation SPI. Los 175/156 heartbeats actuales **no** cierran R1–R4. El PR #124 continúa **Draft, NO MERGE**; preparar revisión de decisión/aceptación de R1–R4 y posterior merge a base, nunca auto-merger. No modificar GPIO4, FlashFreq, core/archive congelados ni instrumentación productiva.

```text
ALPHA13=OPEN
F135=RESOLVED_LOCAL_2_SEALS_NEW_CI_POSTBUILD_UNCONFIRMED
NEXT_FAILURE_ID=F136
G10_PH0=PASS_FUNCTIONAL_PHYSICAL
G10_PH0_PRODUCT_UPLOAD_HEAD=7c1974c659be4aa88c877a849db6efc32673da60
G10_PH0_COM14_UPLOAD=PASS
G10_PH0_COM14_UART=PASS_HB60_TO_215_BOOT_STABLE
G10_PH0_COM14_DHCP=PASS_IP_192_168_0_31_PHY1_ETH1_TCP1
G10_PH0_COM4_UPLOAD=PASS_OPERATOR_REPORTED_UART_CONFIRMED
G10_PH0_COM4_UART=PASS_HB21_TO_195_BOOT_STABLE
G10_PH0_RTU_MASTER_SLAVE=PASS_COM4_RTU_OK_142_RTU_FAIL_40_STABLE_POST_CONNECT
G10_PH0_RTU_COM14_SLAVE_REQUEST_COUNT=NOT_CAPTURED_AFTER_CONNECTION
G10_PH0_COM4_DHCP=PASS_IP_192_168_0_159_PHY1_ETH1_TCP1
G10_PH0_TCP_FC03_PHYSICAL=PASS_2_OF_2_READ_ONLY
G10_PH0_UDP_PHYSICAL=NOT_RUN
G10_PH0_PLC_TO_PLC_TCP_CLIENT=NOT_RUN_OPTIONAL
G10_H_R1_R2_R3_R4=NOT_RUN_NOT_AUTHORIZED
G10_E1=PENDING_GDB_UNAVAILABLE
G10_E2=PENDING_SDKCONFIG_NOT_LOCATED
PENDIENTE_DE_PROMOCION=PR124_10_MODIFIED_2_ADDED_NOT_IN_ALPHA13_BASE
BASE_SYNC=PENDING_INTENTIONAL_UNTIL_G10_H_DECISION
KNOWLEDGE_SYNC=PENDING_F135_TOOLING_LESSON_NO_NEW_PH0_RULE_ADDED
G10_PR124=DRAFT_NOT_MERGED
NEXT_GATE=G10_H_R1_R4_SCOPE_AND_PR124_MERGE_DECISION
```

---
# ESTADO VIGENTE — 20/10/2026 — G10-PH0 F135: fail del verificador ELF/bin, sin upload

**Evidencia real operador:** VS Code ve USB-SERIAL CH340 en **COM14 y COM4**; N0 local `c08147eedf` PASS. `--prepare` devolvió `G10P_PH0_REVIEW_STOP_ELF_MISSING_COM14_SLAVE`. El nuevo runner suponía `G10P_COM14_Slave.elf` en vez del nombre Arduino `G10P_COM14_Slave.ino.elf`; el mismo defecto afectaba `.bin`. No atribuirlo a firmware ni producto: el ejecutor de build dual anterior prueba ELF real y fuente→objeto. COM14 y COM4 **no han sido flasheados**.

**F135 (tooling, corregido en rama candidata, CI posterior pendiente):** `g10p_ph0_upload.py` ahora exige ELF `Sketch.ino.elf`, firmware `.ino.bin`, bootloader `.ino.bootloader.bin`, particiones `.ino.partitions.bin`, SHA de manifiesto/objetos/libs y sello; el workflow dual prueba el sellado real sin USB en Windows/Ubuntu. Incluye `--seal-existing` para recuperar la build anterior sin recompilar si el diff Git entre el HEAD compilado y actual no modifica package, sketches, compilador o contrato. Procedimiento versionado en `tools/alpha13/g10_promoted_bench/README.md`; el registro de fallos único está en `A13_TOOLING_FAILURES_AND_PREVENTION_20261006.md`.

**Próximo gate:** verificar CI de sellado postbuild y que las tres imágenes por placa existen; operador hará `git pull --ff-only` en la rama candidata y `--seal-existing` sobre la carpeta temporal previa. Solo luego de PASS y observación de puerto/rol procede upload autorizado COM14→COM4 y UART; RS-485, TCP/router y R1–R4 permanecen no ejecutados.

```text
ALPHA13=OPEN
F135=TOOLING_FIX_PENDING_REAL_POSTBUILD_CI
NEXT_FAILURE_ID=F136
G10_PH0_LOCAL_N0_C08147=PASS
G10_PH0_PREP_C08147=REVIEW_STOP_ELF_MISSING_COM14_SLAVE
G10_PH0_FIRMWARE_CHANGED=NO
G10_PH0_UPLOAD_COM14=NOT_RUN
G10_PH0_UPLOAD_COM4=NOT_RUN
G10_PH0_MAC_EXPLICIT_READ=NOT_RUN_NOT_NEEDED
G10_PH0_FLASH_BACKUP=WAIVED_BY_OWNER
G10_PH0_RTU_TCP_PHYSICAL=NOT_RUN
G10_E1=PENDING_GDB_UNAVAILABLE
G10_E2=PENDING_SDKCONFIG_NOT_LOCATED
PENDIENTE_DE_PROMOCION=PR124_12_SOURCES_NOT_IN_ALPHA13_BASE
BASE_SYNC=PENDIENTE_G10_OPEN
KNOWLEDGE_SYNC=PENDIENTE_F135_REUSABLE_LESSON
NEXT_GATE=G10_PH0_F135_CI_POSTBUILD_THEN_SEAL_EXISTING_AND_UPLOAD_COM14
```

---
# ESTADO VIGENTE — 20/10/2026 — G10-PH0 dual autorizado, sin backup ni consulta MAC

**Decisión del propietario:** COM14 Slave y COM4 Master pueden actualizar el firmware de banco G10-PH0. No se exige backup de flash, comprobación MAC ni consentimiento adicional por cada upload. La decisión sustituye solo esos requisitos de las entradas históricas; se mantienen Q sin cargas, puertos correctos, red aislada y UART posterior. R1–R4, merge y release NO autorizados.

**Preflight del operador:** worktree limpio; rama candidata y HEAD a3019fe62df1a2ee3d58d0f95043822eb0ebda8e confirmados; base local restaurada a 0344ff9cbe42e9efa65e7d4caabb60ce15464270; G10P_N0 y G10P_PREFLIGHT sin hardware PASS. Este PASS corresponde al commit anterior a las herramientas nuevas y no acredita todavía su CI.

**Preparación versionada:** tools/alpha13/g10_promoted_bench/g10p_ph0_upload.py permite --check, --prepare (compilar ambos en espacio temporal, SHA de binarios), --upload-com14 y --upload-com4 en ese orden. No invoca read_flash, consulta explícita MAC ni erase-all. Uso completo documentado en el README del banco. CI dual incorpora N0 read-only del uploader; el resultado del HEAD nuevo se verificará antes de cualquier upload.

**Campaña aún pendiente:** preparar build con package real; upload COM14 y observar BOOT/HEARTBEAT sin watchdog; luego COM4 y UART; después RS-485 Modbus RTU Master/Slave y router aislado con dos IP DHCP distintas, PHY=1, ETH=1, TCP=1 y PC FC03 read-only. Ni éxito de CLI ni CI constituyen PASS físico de estos gates sin UART y observación del operador.

```text
ALPHA13=OPEN
NEXT_FAILURE_ID=F135
G10_PH0_UPLOAD_APPROVAL=COM14_SLAVE_AND_COM4_MASTER_APPROVED
G10_PH0_FLASH_BACKUP=WAIVED_BY_OWNER
G10_PH0_MAC_EXPLICIT_READ=NOT_NEEDED_NOT_REQUESTED
G10_PH0_LOCAL_N0_AT_A3019FE=PASS_OWNER_REPORTED
G10_PH0_NEW_RUNNER_CI=PENDING_CHECK
G10_PH0_UPLOAD_COM14=NOT_RUN
G10_PH0_UPLOAD_COM4=NOT_RUN
G10_PH0_RTU_PHYSICAL=NOT_RUN
G10_PH0_DUAL_TCP_PHYSICAL=NOT_RUN
G10_R_R1_R2_R3_R4=NOT_RUN_NOT_AUTHORIZED
G10_E1=PENDING_GDB_UNAVAILABLE
G10_E2=PENDING_SDKCONFIG_NOT_LOCATED
PENDIENTE_DE_PROMOCION=PR124_12_SOURCES_NOT_IN_ALPHA13_BASE
BASE_SYNC=PENDIENTE_INTENCIONAL_G10_OPEN
KNOWLEDGE_SYNC=OK_THROUGH_F134
NEXT_GATE=G10_PH0_RUNNER_N0_CI_THEN_PREPARE_UPLOAD_COM14_COM4_UART
```

---
# ESTADO VIGENTE — 19/10/2026 — G10-PH0: banco dual COM4+COM14 y router, CI no física PASS

**Nueva decisión del propietario:** preparar y validar ambos JWPLC Basic de desarrollo, **COM4 Master y COM14 Slave**, incluidos sus puertos Ethernet conectados posteriormente al mismo router/switch de laboratorio aislado. Es continuación del [PR #124](https://github.com/JW-Control/platform-jwplc/pull/124) **Draft**. La corrección F1 de `JWPLC_ETH_CS=5` **ya está integrada en la base** de Alpha13 y validada en la placa de COM4; el nuevo código funcional G10 solo está promovido en la rama del PR.

**Sketche(s) de package real:** `tools/alpha13/g10_promoted_bench/G10P_COM4_Master` (RTU Master, Ethernet DHCP/ModbusTCP Server/UDP y lecturas no destructivas de periféricos), `G10P_COM14_Slave` (RTU Slave ID 2 y Ethernet DHCP/ModbusTCP Server; sin lectura SD). Dos puertos RJ45 al **mismo router DHCP aislado**, dos IP privadas distintas; comunicación RTU físicamente por RS-485 independiente. No usar el overlay G10-R H0 con centinela `LINK=-9`.

**Gates no físicos verificados:** [CI build-dual #39368656011](https://github.com/JW-Control/platform-jwplc/actions/runs/39368656011) **2/2 PASS Ubuntu/Windows**, con namespace temporal `jwplc_g10ph0`, selección de plataforma real, dependencia GCC `.d`, objeto `.o` y ELF de ambos sketches. [CI TCP-FC03 #39369086270](https://github.com/JW-Control/platform-jwplc/actions/runs/39369086270) **2/2 PASS sintéticos Windows/Ubuntu**, valida consulta Modbus TCP **solo lectura**, no implica prueba de red real. [Protocolo dual](../../tools/alpha13/g10_promoted_bench/README.md) y [preparación de gate físico](A13_G10_PROMOTED_GATE_FISICO_PREPARACION_20261019.md).

**F134, tooling:** el workflow legado G10-R N0 [run #39368659798](https://github.com/JW-Control/platform-jwplc/actions/runs/39368659798) se activó por archivos `ALPHA13_STATUS` y quiso aplicar a PR #124 una whitelist diseñada para overlay G10-R, provocando fallo de scope sin fallo productivo. La regla quedó **excluida de esa rama PR** con un `if` específico; [run #39369091800](https://github.com/JW-Control/platform-jwplc/actions/runs/39369091800) `SKIPPED_EXPECTED_NOT_APPLICABLE`. Se mantienen los N0/N1/N2/N3 del producto, y la whitelist original de G10-R **no fue relajada**.

**Permisos:** aprobada preparación de banco **para ambas placas**, NO upload. Falta comprobar identidad COM4 y COM14 contra registros previos independientes, 4 MB flash, relés Q aislados, consentimiento explícito de riesgo/backup **por cada equipo** y aprobación separada antes de sobrescribirlos. Aún no se ha ejecutado el ensayo DHCP/TCP del router ni el enlace físico RS-485 del firmware nuevo. R1–R4 G10 continúan NO autorizados. Evitar cambiar la rama base, COM4 con firmware F1, COM14, GPIO4, FlashFreq o archives congelados sin permiso.

```text
ALPHA13=OPEN
F131=F1_PHYSICAL_PASS_OWNER_ACCEPTED
F132=RESOLVED
F133=RESOLVED
F134=RESOLVED_LEGACY_G10_R_SCOPE_MISROUTING
NEXT_FAILURE_ID=F135
G10_PRODUCT_BRANCH=PR124_DRAFT_NOT_MERGED
G10_PH0_DUAL_SCOPE=COM4_MASTER_COM14_SLAVE_PREPARATION_APPROVED
G10_PH0_DUAL_ETHERNET_ROUTER=PLANNED_NOT_EXECUTED
G10_PH0_BUILD_WIN_UBUNTU=PASS_2_OF_2_CI_39368656011
G10_PH0_TCP_FC03_SYNTH_WIN_UBUNTU=PASS_2_OF_2_CI_39369086270
G10_PH0_UPLOAD=NOT_AUTHORIZED_NOT_RUN_BOTH
G10_PH0_TCP_PHYSICAL=NOT_RUN
G10_PH0_RTU_PHYSICAL=NOT_RUN
G10_R_R1_R2_R3_R4=NOT_RUN_NOT_AUTHORIZED
COM14=NOT_TOUCHED
NEXT_GATE=G10_PH0_IDENTITY_BACKUP_AND_EXPLICIT_UPLOAD_APPROVAL_PER_DEVICE
```

---

# ESTADO VIGENTE — 19/10/2026 — G10 promovido en PR #124, N0/N1/N2/N3 PASS no físico

**F1/H0 aprobado con observación por el propietario**, documentado en PR #121 ya fusionado. **G10 funcional promovido a `JWPLC/2.1.0` solamente en [PR #124](https://github.com/JW-Control/platform-jwplc/pull/124), aún Draft**, no en la rama base de Alpha13. Cambiaron 10 fuentes existentes y se añadieron 2 `SafeAccess`; F1, CS W5500, DNS/DHCP y archives `.a` congelados se conservaron. [Matriz de evidencia y límites](A13_G10_PROMOTED_N0_N3_20261019.md).

**Resultados verificados:** N0 Windows/Ubuntu 2/2 PASS, N1 contratos nativos C++ más paridad de **14/14 fuentes SHA** Windows/Ubuntu 2/2 PASS en [CI #39358616596](https://github.com/JW-Control/platform-jwplc/actions/runs/39358616596), N2 ambos FQBN y N3 **11/11 sketches fuente→objeto→ELF** en [CI #39329439143](https://github.com/JW-Control/platform-jwplc/actions/runs/39329439143). La procedencia del ELF sí está acreditada, pero E1 GDB y E2 sdkconfig permanecen `PENDING` en ese runner. No se ha realizado build/upload físico del nuevo package; el firmware de COM4 continúa siendo el F1 previo.

**Seguridad:** la autorización H0 EN/USB no es autorización para **G10-R R1–R4** ni para sobrescribir COM14. Falta revisión del protocolo y aprobación específica del operador para gate físico; G10 no se fusiona ni se declara cerrado todavía. F133 agrupa los incidentes de harness YAML, caché `__pycache__` y etiquetado de DNS/DHCP, todos resueltos y sin defecto productivo.

```text
ALPHA13=OPEN
F131=H0_ACCEPTED_WITH_OBSERVATION
F132=RESOLVED_CI_AND_LOCAL
F133=RESOLVED_G10_PROMOTION_TOOLING
NEXT_FAILURE_ID=F134
G10_PACKAGE_PROMOTION=APPLIED_PR124_NOT_MERGED
G10_N0=PASS_2_OF_2
G10_N1=PASS_NATIVE_PARITY_2_OF_2_14_FILES
G10_N2=PASS_BOTH_FQBN
G10_N3=PASS_11_OF_11
G10_E1=PENDING_GDB_UNAVAILABLE
G10_E2=PENDING_SDKCONFIG_NOT_LOCATED
G10_PHYSICAL_R1_R4=NOT_RUN_NOT_AUTHORIZED
COM4_G10_UPLOAD=NOT_RUN
COM14=NOT_TOUCHED
NEXT_GATE=G10_PHYSICAL_PREP_AND_SPECIFIC_AUTHORIZATION_R1_R4
```

---

# ESTADO VIGENTE — 19/10/2026 — H0-F1 RJ45 conectado: 3 EN + 3 USB PASS; G10-P0 siguiente

**Complemento físico entregado por el operador** al ensayo anterior H0: [evidencia detallada EN/USB con RJ45 conectado](A13_H0_F1_EN_USB_RJ45_CONECTADO_20261019.md). Se observan **3 arranques EN** que regresan `EARLY_LINK_RETURN=1` (LinkON), y **3 arranques tras desconexión/reconexión COM4 por USB**, de los que dos regresan directamente `EARLY_LINK_RETURN=2` (negociación física) y llegan a `LINK=1` desde `COUNT=2`. El tercero solo se captura desde `READY COUNT=1`, pero `STAGE=2` acredita retorno temprano indirecto. Los seis muestran un BOOT diferente, `STAGE=2`, enlace `LINK=1` sostenido en cinco o más contadores y ningún watchdog visible.

**F131:** corrección F1 aceptada por el propietario; H0 físico principal y el complemento RJ45 conectado **PASS**. Los 10 EN y 10 power-cycles con RJ45 desconectado fueron reportados realizados y todos los BOOT visibles retornaron, pero la condición formal de **5 READY capturados en cada maniobra** conserva `REVIEW_EVIDENCE_GAP` para cuatro ventanas; no sustituir la evidencia por los nuevos seis arranques ni inventar `20/20 STRICT PASS`. Se puede preparar **G10-P0 inventario de solo lectura**, sin promocionar código hasta fijar decisión de aceptación de H0 extendido.

**G10-P0 (solo lectura) iniciado:** [inventario estático source-backed](A13_G10_P0_INVENTARIO_ESTATICO_SOURCE_BACKED_20261019.md) derivado de `a13_g10_integrated.py::compose()` y mapa real Git: 14 rutas destino, 12 existentes en package, 2 SafeAccess nuevos; `Dns.cpp`/`Dhcp.cpp` sin cambios previstos. Aún **no** se ejecutó la composición dinámica ni se generó un diff/hashes del candidato. El productivo `w5100.cpp` F1 está fuera de G10-I y se conserva. **No iniciar promoción** hasta resolver la aceptación formal de las ventanas 20/20 y completar P0 dinámico.

**G10-P0 read-only verificado:** [CI #39315048980](https://github.com/JW-Control/platform-jwplc/actions/runs/39315048980), **2/2 PASS Windows/Ubuntu** en PR #122 Draft con HEAD de código `96b4ab31e97aa0c3a9f0c9ab3a67e6628f11b65e`. El `compose()` fuente genera 14 destinos, **10 MODIFIED + 2 ADDED + 2 UNCHANGED**; `w5100.cpp` F1 conserva contenido canónico Git idéntico, producto sin cambios. [Inventario y contrato](A13_G10_P0_INVENTARIO_ESTATICO_SOURCE_BACKED_20261019.md). No se ha copiado ningún archivo G10 al package; N2/N3 de adopción y R1–R4 permanecen pendientes.

**Límites:** rama base del firmware sigue en `2b5802720df552e01a0b4f9d4fe1a7bd2de16daf` sin cambios; esta documentación está en PR #121 Draft. No tocar COM14, GPIO4, autoload, FlashFreq, binarios congelados ni instrumentación de `w5100.cpp`. G10-R R1–R4 permanecen no ejecutados/no autorizados.

```text
ALPHA13=OPEN
F131=F1_OWNER_APPROVED_H0_PRIMARY_PASS_EXTENDED_REVIEW_CAPTURE_GAP
F132=RESOLVED_CI_AND_LOCAL_BUILD
H0_F1_ETH_CONNECTED_EN=PASS_3_OF_3
H0_F1_ETH_CONNECTED_USB=PASS_3_OF_3
H0_F1_ETH_CONNECTED_EARLY_RETURN_DIRECT=5_OF_6
H0_F1_ETH_CONNECTED_EARLY_RETURN_INDIRECT=1_OF_6
H0_F1_ETH_CONNECTED_LINK1_RECOVERY=PASS_6_OF_6
H0_F1_ETH_CONNECTED_TG1WDT=NOT_OBSERVED
H0_F1_EXTENDED_20_OF_20_STRICT=REVIEW_EVIDENCE_GAP
G10_P0_INVENTORY=PASS_DYNAMIC_COMPOSE_CI_2_OF_2_SHA_96B4AB31
G10_PACKAGE_PROMOTION=NOT_STARTED_BLOCKED_ON_H0_ACCEPTANCE
COM14=NOT_TOUCHED
G10_R_R1_R2_R3_R4=NOT_RUN_NOT_AUTHORIZED
NEXT_FAILURE_ID=F133
NEXT_GATE=G10_P0_INVENTORY_AND_H0_EXTENDED_ACCEPTANCE_DECISION
```

---

# ESTADO VIGENTE — 19/10/2026 — H0 extendido 10 EN + 10 power: sin watchdog visible, brechas menores de captura

**Informe del operador:** «Listo, ambos verificados», tras la autorización de 10 pulsaciones EN y 10 desconexiones/reconexiones de alimentación **únicamente COM4**, con RJ45 desconectado y sin recompilar ni reflashear, package F1 en HEAD `2b5802720df552e01a0b4f9d4fe1a7bd2de16daf`. La consola enviada y el fichero adjunto aportan registros de arranque, `H0F1_EARLY_LINK_RETURN=2`, `STAGE=2 LINK=2` y contadores crecientes; **no muestran TG1WDT_SYS_RESET, panic o bloqueo de `linkStatus()`**.

**Reconciliación estricta de los logs, sin inventar cuentas:**
- El **archivo adjunto** muestra **12 identificadores BOOT** entre 14:19:34 y 14:21:15, todos con `EARLY_LINK_RETURN=2`. Nueve de esos doce alcanzaron al menos cinco `H0F1_READY` en la muestra; tres se detienen con `COUNT=3/4` antes del siguiente evento. El archivo por sí solo no delimita inequívocamente qué diez corresponden a EN y cuáles son maniobras adicionales/de otra fase.
- El **bloque de consola posterior** muestra **10 BOOT nuevos** precedidos por cierres/reaperturas del COM4, todos con `EARLY_LINK_RETURN=2`, `STAGE=2` y `LINK=2`. Nueve alcanzan cinco o más contadores, **`BOOT=915169EA` solo muestra `COUNT=1..3`** antes de desconectarse.
- En el archivo inicial aparece un **banner de boot duplicado muy cercano a las 14:21:08**, con bytes UART solapados; podría ser maniobra/reset adicional o captura corrupta. No atribuir causa de hardware sin corroborarla. Los `rst:0x1 POWERON_RESET` observados alrededor de maniobras no prueban un watchdog.
- El operador confirma ambas fases completadas; las muestras son evidencia fuerte de recuperación repetida y ausencia de WDT, pero **no acreditan la regla escrita de cinco heartbeats en cada uno de los veinte ciclos**. Por tanto no registrar `PASS_20_OF_20_STRICT` sin conciliación, ni abrir F133: **`REVIEW_EVIDENCE_GAP`**, no defecto F131 demostrado.

**Decisión sobre Ethernet conectado:** NO es obligatorio repetir otros 20 ciclos con RJ45. El F1 ya pasó LinkON estable y `LINK=1→2→1` dentro del mismo arranque. Como complemento opcional al arranque bajo enlace presente se pueden proponer **2 EN y 2 power-cycles con RJ45 conectado**, con autorización separada del operador; el `EARLY_LINK_RETURN` debe regresar, pero durante la negociación inicial PHY puede informar `2` antes de que `LINK` alcance `1`. La campaña opcional no se ha ejecutado ni autorizado todavía.

**Próxima fase G10:** plan de promoción funcional en [A13_G10_PROMOCION_DESPUES_H0_F1_20261019.md](A13_G10_PROMOCION_DESPUES_H0_F1_20261019.md), **sin modificar package** ni ejecutar R1–R4 hasta reconciliar el gate H0 y aprobar alcance. Preservar `w5100.cpp` corregido; no copiar el overlay de laboratorio H0. Registros sensibles/MAC no se incluyen en Git.

```text
ALPHA13=OPEN
F1_APPROVED=YES
F131=H0_PRIMARY_PASS_EXTENDED_OBSERVED_SUCCESS_REVIEW_CAPTURE_GAPS
F132=RESOLVED_CI_AND_LOCAL_BUILD
H0_F1_EN10=OPERATOR_REPORTS_DONE_CAPTURE_GROUPING_REVIEW
H0_F1_POWER10=OPERATOR_REPORTS_DONE_CAPTURE_9_OF_10_WITH_5_COUNTS
H0_F1_BOOT_OBSERVED_TOTAL=22_ACROSS_TWO_SUPPLIED_SOURCES
H0_F1_EARLY_LINK_RETURN=2_ALL_OBSERVED_BOOT_MARKERS
H0_F1_TG1WDT=NOT_OBSERVED_IN_SUPPLIED_LOGS
H0_F1_STRICT_20_OF_20=REVIEW_EVIDENCE_GAP
H0_F1_ETH_CONNECTED_RESTARTS=OPTIONAL_NOT_AUTHORIZED_NOT_RUN
G10_PACKAGE_PROMOTION=PLANNED_NOT_STARTED
COM14=NOT_TOUCHED
G10_R_R1_R2_R3_R4=NOT_RUN_NOT_AUTHORIZED
NEXT_FAILURE_ID=F133
NEXT_GATE=H0_F1_RECONCILE_CAPTURE_GAPS_AND_DECIDE_OPTIONAL_ETH_ON_THEN_G10_P0
```

---

# ESTADO VIGENTE — 19/10/2026 — F1 APROBADO; H0 extendido 10 EN + 10 power-cycle autorizado; G10 después

**Nueva decisión explícita del propietario:** «Aprobado F1. Pasamos a la campaña extendida y luego la integración G10». **No atribuir esta decisión al cierre completo de Alpha13, G10 ni R1–R4**. El gate físico principal de F131/W5500 ya pasó: early-link real, `STAGE=2`, `LINK=2→1→2→1`, sin watchdog reportado, mismo BOOT hasta `COUNT=1600`, y `POWERON_RESET` previo manual. La prueba extendida de **10 EN y 10 cortes completos de alimentación** está **AUTORIZADA PERO NO EJECUTADA** según evidencia disponible.

**Contrato:** [A13_H0_F1_CAMPANA_EXTENDIDA_EN_POWER_20261019.md](A13_H0_F1_CAMPANA_EXTENDIDA_EN_POWER_20261019.md). Únicamente COM4 de laboratorio, con RJ45 desconectado para aislar init W5500, firmware F1 preexistente en `2b5802720df552e01a0b4f9d4fe1a7bd2de16daf`; ningún `git pull`, recompilación o upload necesarios. Intervenciones estrictamente manuales, observación de boot/early-link/`STAGE=2`/PHY y al menos 5 heartbeats por ciclo; fail-closed al primer WDT, brownout, panic, reset imprevisto o falla de identidad COM. **No tocar COM14 ni GPIO4**.

**Orden posterior:** después de H0-EN10 + H0-PW10 PASS, comenzar [plan G10 productivo](A13_G10_PROMOCION_DESPUES_H0_F1_20261019.md) con P0 inventario exacto del **compositor funcional G10-I**. El compositor experimental G10-R mezcla trazas H0 e instrumentación W5500 para banco; **no trasladar su `w5100.cpp` staged al package**, pues pisaría la corrección F1 validada. Mantener `G10_PACKAGE_PROMOTION=PENDING` y `G10_R_R1_R4=NOT_RUN` hasta nuevas evidencias y autorización específica.

**Rama operativa:** la base sigue en `2b5802720df552e01a0b4f9d4fe1a7bd2de16daf`; esta actualización documental vive en [PR #121](https://github.com/JW-Control/platform-jwplc/pull/121) Draft y no cambia la fuente instalada ni obliga al operador a alterar el checkout.

```text
ALPHA13=OPEN
F1_APPROVED=YES_OWNER_CONFIRMED
F131=H0_PRIMARY_PHYSICAL_PASS_EXTENDED_AUTHORIZED_NOT_RUN
F132=RESOLVED_CI_AND_LOCAL_BUILD
H0_F1_EXTENDED_EN10=AUTHORIZED_NOT_RUN
H0_F1_EXTENDED_POWER10=AUTHORIZED_NOT_RUN
H0_F1_PHYSICAL_TARGET=COM4_ONLY
H0_F1_RJ45_CONDITION=DISCONNECTED_DURING_EXTENDED
H0_F1_PRODUCT_HEAD=2b5802720df552e01a0b4f9d4fe1a7bd2de16daf
COM14=NOT_TOUCHED
G10_R_R1_R2_R3_R4=NOT_RUN_NOT_AUTHORIZED
G10_PACKAGE_PROMOTION=POST_H0_EXTENDED_PENDING
PENDIENTE_DE_PROMOCION=G10_FUNCTIONAL_CANDIDATE
NEXT_FAILURE_ID=F133
BASE_SYNC=PENDING_DRAFT_PR121_TO_PRESERVE_OPERATOR_HEAD
KNOWLEDGE_SYNC=PENDING_DECISION_PR
NEXT_GATE=H0_F1_EXTENDED_EN10_AND_POWER10_THEN_G10_P0_INVENTORY
```

---

# ESTADO VIGENTE — 19/10/2026 — H0-F1 PHY LinkON→LinkOFF→LinkON PASS sin reinicio

**Nueva evidencia de COM4 entregada por operador:** en el sketch físico `A13_H0_F1_SMOKE.ino` del package F1 desde `2b5802720df552e01a0b4f9d4fe1a7bd2de16daf`, **sin recompilación ni nuevo flash**, se desconectó y reconectó **manualmente** RJ45 manteniendo `BOOT=6BCAE2DA`, `STAGE=2` y el contador a 1 Hz. UART 115200:

```text
13:37:36.355 COUNT=1586 STAGE=2 LINK=1
13:37:37.355 COUNT=1587 STAGE=2 LINK=2
13:37:43.355 COUNT=1593 STAGE=2 LINK=2
13:37:44.355 COUNT=1594 STAGE=2 LINK=1
13:37:50.355 COUNT=1600 STAGE=2 LINK=1
```

**Resultado: `H0_F1_RJ45_ON_OFF_ON=PASS`**, transición PHY real `1→2→1`, recuperada en el **mismo arranque** sin `TG1WDT_SYS_RESET` en el extracto y sin reinicios reportados. Desde el arranque, `COUNT=1600` representa ~26 min 40 s de uptime estimado; **no se dispone de captura UART continua completa de toda la ventana**. La maniobra desconectada se observa durante siete muestras de un segundo. El `rst:0x1 POWERON_RESET` visto antes había sido **manual y confirmado por el operador**; no es una anomalía nueva.

**Decisión actual:** `H0_F1_PHYSICAL_PRIMARY_GATE=PASS` con early-link, `STAGE=2`, LinkOFF/LinkON, estabilidad de 15 min previamente confirmada por operador y **una reconexión manual ON→OFF→ON PASS**. El CS F1 se considera validado para la causa F131 de H0; esta prueba no implica PASS de G10 sockets/Modbus/DHCP/DNS/concurrencia ni de las campañas extendidas 10 resets EN + 10 power-cycles, aún pendientes de decisión/autorización. F132 sigue resuelto CI+compilación local. Ningún cambio de firmware, core, autoload, GPIO4, pines, COM14 o configuración de FlashFreq; documentación en PR #121 Draft para preservar HEAD del banco.

```text
ALPHA13=OPEN
F131=FIX_F1_H0_PRIMARY_AND_SINGLE_RJ45_RECONNECT_PASS
F132=RESOLVED_CI_AND_LOCAL_BUILD
H0_F1_PRODUCT_HEAD=2b5802720df552e01a0b4f9d4fe1a7bd2de16daf
H0_F1_PHYSICAL_PRIMARY_GATE=PASS
H0_F1_RJ45_ON_OFF_ON=PASS_ONE_MANUAL_CYCLE_BOOT_6BCAE2DA
H0_F1_LINK_TRACE=COUNT_1586_ON_1587_OFF_1593_OFF_1594_ON_1600_ON
H0_F1_BOOT=6BCAE2DA
H0_F1_COUNT_LAST=1600
H0_F1_UPTIME_ESTIMATE=26MIN40S_FROM_COUNTER
H0_F1_STABILITY_15MIN=PASS_OPERATOR_REPORTED
H0_F1_UNEXPECTED_RESET=NONE_REPORTED_IN_SAMPLE
H0_F1_WDT=NOT_OBSERVED_IN_SUPPLIED_UART
H0_F1_EXTENDED_EN_10=NOT_RUN_NOT_AUTHORIZED
H0_F1_EXTENDED_POWERCYCLE_10=NOT_RUN_NOT_AUTHORIZED
G10_R_R1_R2_R3_R4=NOT_RUN_NOT_AUTHORIZED
COM14=NOT_TOUCHED
G10_PACKAGE_PROMOTION=PENDING_OVERLAY_ONLY
NEXT_FAILURE_ID=F133
NEXT_GATE=H0_F1_DECIDE_EXTENDED_RESTART_CAMPAIGN_AND_G10_R_PROMOTION
```

---

# ESTADO VIGENTE — 19/10/2026 — Alpha13 H0-F1: EARLY_LINK + LinkOFF/LinkON + estabilidad física ~15 min PASS

**Evidencia suministrada por el operador, COM4, package F1 real** desde `v2.1.0-alpha.13/feature/cleanup-robustness` HEAD **`2b5802720df552e01a0b4f9d4fe1a7bd2de16daf`**; sketch `A13_H0_F1_SMOKE.ino`, 115200 baudios. Se conserva la base productiva sin nuevos cambios. `H0F1_UPLOAD=PASS_COM4_ONLY`, `COM14=NOT_TOUCHED`. El operador **aclaró que el `rst:0x1 (POWERON_RESET)` previo fue causado por él**, y que RJ45 permaneció desconectado antes y después de dicho reset. No atribuir ese reset al firmware.

**Resultados UART de la misma sesión/BOOT `6BCAE2DA`:**
- La consulta temprana de `JWPLC_Ethernet.linkStatus()` retorna `H0F1_EARLY_LINK_RETURN=2` sin `TG1WDT_SYS_RESET` en los fragmentos entregados.
- LinkOFF real y `STAGE=2` desde arranque; `COUNT=675..728` mostró `LINK=2`.
- **Transición RJ45 observada a las 13:23:19**, sin reinicio entre muestras: `COUNT=728 LINK=2` → `COUNT=729 LINK=1`, y `COUNT=729..898 LINK=1` de forma sostenida.
- Último log `13:26:08.386 COUNT=898 STAGE=2 LINK=1`. El operador confirmó **~15 minutos totales de estabilidad** del mismo arranque. El contador a 1 Hz documenta ~898 segundos de ejecución; el enlace conectado se observa en las muestras durante **aproximadamente 170 segundos**, no durante 15 minutos.
- ID `BOOT=6BCAE2DA` constante y contador progresivo, sin reinicios espontáneos ni watchdog reportados/observados en los extractos. La muestra no demuestra 15 minutos completos de captura UART continua; se preserva la duración confirmada por el operador.

**Decisión:** **`H0_F1_PHYSICAL_PRIMARY_GATE=PASS`**: CS corregido desde package productivo, enlace PHY real LinkOFF→LinkON y estabilidad ~15 minutos. F131 se marca como **corrección de causa W5500 validada en H0 principal**, sin expandir el resultado a G10-R. La campaña extendida de 10 resets EN y 10 power-cycles, reconexión LinkON→LinkOFF→LinkON, concurrencia SPI/socket/DHCP/Modbus TCP y R1–R4 **no está ejecutada ni autorizada**. No declarar robustez integral Ethernet, ni publicar release/bootloader. `mode:DIO` se observó en el boot y no fija configuración FlashFreq.

Referencia: registro [F131/F132](A13_TOOLING_FAILURES_AND_PREVENTION_20261006.md). Evidencias sensibles (MAC, identidad PnP, manifiestos y logs completos) permanecen locales; no copiar en GitHub.

```text
ALPHA13=OPEN
F130=CONFIRMED_PROCESS_HISTORICAL
F131=FIX_F1_VALIDATED_H0_PHYSICAL_PRIMARY_PASS
F132=RESOLVED_CI_AND_LOCAL_BUILD
H0_F1_PRODUCT_HEAD=2b5802720df552e01a0b4f9d4fe1a7bd2de16daf
H0_F1_PHYSICAL_PRIMARY_GATE=PASS_REAL_PHY_AND_15MIN_OPERATOR_REPORTED
H0_F1_EARLY_LINK=PASS_RETURN_2_WITHOUT_WDT_IN_SAMPLE
H0_F1_BOOT=6BCAE2DA
H0_F1_PHY_LINK_OFF=PASS_2
H0_F1_PHY_LINK_ON=PASS_1_SUSTAINED_170S_SAMPLED
H0_F1_COUNT_LAST=898
H0_F1_STABILITY_15MIN=PASS_OPERATOR_REPORTED_SAME_BOOT
H0_F1_POWERON_RESET=MANUAL_OPERATOR_CONFIRMED
H0_F1_UNEXPECTED_RESET=NONE_REPORTED_IN_OBSERVATION
H0_F1_EXTENDED_RESETS_10=NOT_RUN
H0_F1_EXTENDED_POWERCYCLES_10=NOT_RUN
H0_F1_LINK_ON_OFF_ON=NOT_RUN
COM14=NOT_TOUCHED
G10_R_R1_R2_R3_R4=NOT_RUN_NOT_AUTHORIZED
G10_PACKAGE_PROMOTION=PENDING_OVERLAY_ONLY
ALPHA15=UNMODIFIED
NEXT_FAILURE_ID=F133
NEXT_GATE=H0_F1_OPTIONAL_EXTENDED_STABILITY_THEN_G10_R_AUTHORIZATION
```

---

# ESTADO VIGENTE — 19/10/2026 — H0-F1 primer arranque físico Ethernet COM4: EARLY_LINK PASS, H0 COMPLETO PENDIENTE

**Evidencia entregada por operador**, desde el firmware de package instalado y compilado con HEAD `2b5802720df552e01a0b4f9d4fe1a7bd2de16daf` (PR #120 merged); placa de desarrollo COM4, sin LAN RJ45 conectada. La ejecución con `-UploadCOM4` repitió satisfactoriamente `H0F1_PLATFORM=PASS_ISOLATED_PACKAGE_PATH_CANONICAL`, `H0F1_ETHERNET=PASS_CANONICAL_SAME_STAGED_LIBRARY`, `H0F1_W5100=PASS_GCC_OBJECT_DEPENDENCY_FROM_STAGING` y `H0F1_BUILD=PASS_SOURCE_FIRST`. Preflight local de puerto CH340, MAC, flash 4 MB y consentimiento literal fueron aceptados por el ejecutor; reportó `H0F1_UPLOAD=PASS_COM4_ONLY COM14=NOT_TOUCHED`. **La dirección MAC no se copia en archivos de proyecto ni documentación.** No se ha comprobado de forma independiente que la referencia MAC usada proceda de un registro previo distinto de la lectura recién realizada.

**UART físico a 115200**, dos fragmentos de arranque (antes y después de un `rst:0x1`):

```text
H0F1_READY:BOOT=10EF1BB1 COUNT=10 STAGE=2 LINK=2
H0F1_READY:BOOT=10EF1BB1 COUNT=11 STAGE=2 LINK=2
H0F1_READY:BOOT=10EF1BB1 COUNT=12 STAGE=2 LINK=2
rst:0x1 (POWERON_RESET),boot:0x17 (SPI_FAST_FLASH_BOOT)
mode:DIO, clock div:2
H0F1_BOOT=6BCAE2DA
H0F1_PREVIOUS_STAGE=0
H0F1_EARLY_LINK_BEGIN
H0F1_EARLY_LINK_RETURN=2
H0F1_READY:BOOT=6BCAE2DA COUNT=1 STAGE=2 LINK=2
...
H0F1_READY:BOOT=6BCAE2DA COUNT=12 STAGE=2 LINK=2
```

**H0-F1 early-link físico: PASS OBSERVADO** en el segundo arranque: `linkStatus()` retornó físicamente `2=LinkOFF` en `setup()`, y el sketch progresó con `STAGE=2`, PHY real y contador hasta 12 sin `TG1WDT_SYS_RESET` en los fragmentos proporcionados. A diferencia de H0-F previo, no se usó centinela `LINK=-9` ni overlay. **No declarar H0 completo ni F131 completamente cerrado:** el reinicio intermedio `rst:0x1 POWERON_RESET` necesita confirmación de si fue causado manualmente/por el monitor serie o si fue espontáneo; no es `TG1WDT_SYS_RESET` pero tampoco está explicado. En la muestra de ~12 segundos no hay evidencia de 15 minutos sostenidos, `LinkON=1`, reconexión RJ45 ni ciclos extendidos. `mode:DIO` se registra como diagnóstico observado, sin inferir cambio de FlashFreq/configuración definitiva.

**Siguiente gate, de menor riesgo:** confirmar origen del `POWERON_RESET`, prolongar observación UART con RJ45 desconectado y contador progresivo, y solo después conectar RJ45 a switch/router **aislado de banco** para observar `LINK=1` real y, posteriormente, LinkOFF/LinkON según autorización. No reiniciar/flashear sin motivo. R1–R4 y promoción de G10 siguen fuera del alcance.

```text
ALPHA13=OPEN
F130=CONFIRMED_PROCESS_PREVIOUS
F131=F1_PRODUCT_MERGED_H0_EARLY_PHY_PASS_STABILITY_PENDING
F132=RESOLVED_CI_AND_LOCAL_BUILD
F132_COM4_SOURCE_FIRST=PASS_LOCAL_HEAD_2B580272
F131_COM4_UPLOAD=PASS_COM4_ONLY
F131_H0_EARLY_LINK=PASS_REAL_PHY_LINKOFF_2_STAGE_2
F131_H0_UART_PROGRESSION=PASS_TO_COUNT_12
F131_H0_WDT=NOT_OBSERVED_IN_SUPPLIED_SAMPLE
F131_POWERON_RESET=OBSERVED_ORIGIN_UNCONFIRMED
F131_MODE_DIO=OBSERVED_NOT_CONFIG_DECISION
F131_LINK_ON=NOT_RUN
F131_15MIN_STABILITY=NOT_RUN
F131_H0_FULL=OPEN_PENDING
F131_RJ45_RECONNECT=NOT_RUN
COM14=NOT_TOUCHED
G10-R-R1_R2_R3_R4=NOT_RUN_NOT_AUTHORIZED
G10_PACKAGE_PROMOTION=PENDING_OVERLAY_ONLY
ALPHA15=UNMODIFIED
NEXT_FAILURE_ID=F133
NEXT_GATE=H0_COM4_CONFIRM_POWERON_RESET_THEN_STABLE_UART_AND_LINK_ON
```

---

# ESTADO VIGENTE — 19/10/2026 — F132: compilación local source-first PASS; H0 COM4 físico pendiente

**Evidencia informada por el operador en Windows PowerShell**, repositorio `JW-Control/platform-jwplc`, rama `v2.1.0-alpha.13/feature/cleanup-robustness` al HEAD exacto `2b5802720df552e01a0b4f9d4fe1a7bd2de16daf`. Se ejecutaron `git fetch origin`, `git switch`, `git pull --ff-only`, `git status --short` (sin cambios), y `A13_H0_F1_COM4_BANCO.ps1 -ExpectedHead $head` **sin `-UploadCOM4`**. Salida literal:

```text
H0F1_PLATFORM=PASS_ISOLATED_PACKAGE_PATH_CANONICAL
H0F1_STAGING_VENDOR=jwplc_h0f1_ALIAS_JWPLC_SOURCE_UNMODIFIED
H0F1_ETHERNET=PASS_CANONICAL_SAME_STAGED_LIBRARY
H0F1_W5100=PASS_GCC_OBJECT_DEPENDENCY_FROM_STAGING
H0F1_BUILD=PASS_SOURCE_FIRST HEAD=2b5802720df552e01a0b4f9d4fe1a7bd2de16daf
H0F1_UPLOAD=NOT_RUN COM14=NOT_TOUCHED
```

**Resultado:** F132 `RESOLVED_CI_AND_LOCAL_BUILD`. El manifiesto local en `%TEMP%\jwplc_a13_h0f1_2b5802720df5_20261019124126\MANIFEST.json` queda en el PC del operador y **no se publica**; no se ha inspeccionado su contenido por esta conversación. Este resultado **no** valida Ethernet en hardware, PHY ni watchdog. No cambia `JWPLC/2.1.0`; F131/H0-F1 continúa abierto para autorización y pruebas físicas COM4.

**Próximo gate:** identidad local física de COM4 (CH340, MAC de referencia, flash 4 MB, seguridad de Q/aislamiento) → consentimiento específico de upload del mismo package → UART real 115200 y `EARLY_LINK_RETURN` / STAGE2 / PHY / watchdog. COM14 no tocado. G10-R R1–R4 no autorizados. Documentación de esta evidencia preparada en rama PR aislada para no mover la base tras la compilación y evitar repetir un build ya válido antes de la carga.

```text
ALPHA13=OPEN
F131=F1_PRODUCT_MERGED_H0_PHYSICAL_PENDING
F132=RESOLVED_CI_AND_LOCAL_BUILD
F132_LOCAL_HEAD=2b5802720df552e01a0b4f9d4fe1a7bd2de16daf
F132_LOCAL_BUILD=PASS_SOURCE_FIRST
F132_LOCAL_MANIFEST=ON_OPERATOR_PC_NOT_INSPECTED
F132_COM4_UPLOAD=NOT_RUN
F132_COM14=NOT_TOUCHED
NEXT_FAILURE_ID=F133
H0_F1_H=NOT_RUN
G10_R_R1_R2_R3_R4=NOT_RUN_NOT_AUTHORIZED
G10_PACKAGE_PROMOTION=PENDING_OVERLAY_ONLY
ALPHA15=UNMODIFIED
NEXT_GATE=H0_F1_COM4_IDENTITY_CONSENT_UPLOAD_UART_PHY_REAL
```

---

# ESTADO VIGENTE — 19/10/2026 — Alpha13 F132 corregido y verificado por CI; H0 COM4 físico pendiente

**F132 (tooling) CERRADO A NIVEL NO FÍSICO.** El código del ejecutor en HEAD `c76f7e3a8240c8c6a650445aae2b4be61f5e5488` fue validado por [CI Windows H0-F1 #39289719081](https://github.com/JW-Control/platform-jwplc/actions/runs/39289719081): **N0 y N2 2/2 PASS**, con `H0F1_PLATFORM=PASS_ISOLATED_PACKAGE_PATH_CANONICAL`, `H0F1_ETHERNET=PASS_CANONICAL_SAME_STAGED_LIBRARY`, `H0F1_W5100=PASS_GCC_OBJECT_DEPENDENCY_FROM_STAGING`, `H0F1_BUILD=PASS_SOURCE_FIRST`, `H0F1_UPLOAD=NOT_RUN` y `COM14=NOT_TOUCHED`. El N0 incluye pruebas positivas/negativas para alias NTFS 8.3 y el caso F003 de colección PowerShell de un solo archivo. [G10-R #39289718821](https://github.com/JW-Control/platform-jwplc/actions/runs/39289718821): **2/2 PASS Windows/Ubuntu**. **Total 4/4 PASS, ninguno omitido.**

**Alcance y distinción de SHA:** las pruebas respaldan el SHA de código `c76f7e3a`; cualquier commit documental posterior solo actualiza estado/registros y no constituye otro binario. Para cierre del PR comprobar el diff final y su CI. Se integró F1 original en PR #116, no se modifica nuevamente `JWPLC/2.1.0`. La ausencia de resets/WDT en hardware **no está acreditada**: falta ejecutar **build-only en PC Windows del operador** desde Git limpio; únicamente después se puede iniciar upload COM4 con sus verificaciones MAC/flash y consentimiento local; COM14 intocado y R1–R4 bloqueados.

**Incidentes:** F132 (vendor Arduino CLI duplicado, alias NTFS 8.3, recurrencia F003 al inspeccionar dependencias) resuelto en harness por CI, no se abre ID adicional. [FAILURE F132](A13_TOOLING_FAILURES_AND_PREVENTION_20261006.md) actualizado. Conocimiento privado sincronizado mediante [PR #15](https://github.com/JW-Control/jwplc-dev-mcp/pull/15) y [PR #16](https://github.com/JW-Control/jwplc-dev-mcp/pull/16).

```text
ALPHA13=OPEN
F131=F1_PRODUCT_MERGED_H_PHYSICAL_PENDING
F132=RESOLVED_NONPHYSICAL_CI_SOURCE_FIRST_WINDOWS
F132_CI_N0_N2=PASS_2_OF_2_RUN_39289719081_CODE_SHA_C76F7E3A
F132_CI_G10R=PASS_2_OF_2_RUN_39289718821_CODE_SHA_C76F7E3A
F132_LOCAL_BUILD=AWAITING_OPERATOR
F132_PHYSICAL_COM4=NOT_RUN
NEXT_FAILURE_ID=F133
H0_F1_SOURCE_FIRST_CI=PASS_NO_OVERLAY
H0_F1_H=NOT_RUN
COM4_UPLOAD=NOT_RUN
COM14=NOT_TOUCHED
G10-R-R1_R2_R3_R4=BLOCKED_NOT_AUTHORIZED
G10_PACKAGE_PROMOTION=PENDING
ALPHA15=UNMODIFIED
NEXT_GATE=H0_F1_OPERATOR_LOCAL_BUILD_ONLY_THEN_SEPARATE_COM4_UPLOAD
```

---

# ESTADO VIGENTE — 19/10/2026 — Alpha13 H0-F1: plataforma y biblioteca staged PASS, parser GCC en retest

**Evidencia no física:** [CI #39288903468](https://github.com/JW-Control/platform-jwplc/actions/runs/39288903468) sobre HEAD `ab68119a4319f84d192904e51a7c75375402bcdf`: N0 Windows PASS (alias NTFS 8.3 y negativo), `H0F1_PLATFORM=PASS_ISOLATED_PACKAGE_PATH_CANONICAL`, `H0F1_ETHERNET=PASS_CANONICAL_SAME_STAGED_LIBRARY`; N2 FAIL tras la compilación por recurrencia conocida de **F003** (filtro PowerShell que devuelve una cadena en vez de array y `$deps[0]` pasa a ser `C`). No prueba defecto de producto ni puerto COM. Corregido en PR #120 por array externo `@(...)` que preserva un único archivo `.d`. **CI del nuevo HEAD: aún no PASS**. [Registro F132](A13_TOOLING_FAILURES_AND_PREVENTION_20261006.md).

```text
ALPHA13=OPEN
F131=F1_PRODUCT_MERGED_PHYSICAL_PENDING
F132=HARNESS_CLI_VENDOR_8DOT3_AND_F003_ARRAY_REOCCURRENCE
F132_N0_NTFS=PASS_OLD_SHA
F132_N2_PLATFORM=PASS_OLD_SHA
F132_N2_ETHERNET=PASS_OLD_SHA
F132_N2_GCC_DEP=REVIEW_STOP_OLD_SHA
F132_FINAL_SOURCE_FIRST=NOT_YET_PASS_EXACT_HEAD
NEXT_FAILURE_ID=F133
COM4_UPLOAD=NOT_RUN
COM14=NOT_TOUCHED
G10-R-R1_R2_R3_R4=BLOCKED
NEXT_GATE=F132_EXACT_HEAD_CI_N0_N2_G10R_THEN_OPERATOR_BUILD_ONLY
```

---

# ESTADO VIGENTE — 19/10/2026 — Alpha13 H0-F1: F132 alias NTFS 8.3 identificado, CI retest pendiente

**Última evidencia comprobada (HEAD candidato anterior `a1a39e30e5b8c9028a17fb2c9d21231efc01ddb5`):** [G10-R N0 #39286737779](https://github.com/JW-Control/platform-jwplc/actions/runs/39286737779) **2/2 PASS Windows/Ubuntu**. [Kit COM4 #39286737847](https://github.com/JW-Control/platform-jwplc/actions/runs/39286737847): N0 Windows PASS, N2 Windows FAIL en la **auditoría posterior a compilar**, por `WRONG_ETHERNET_LIBRARY`. El log muestra que la biblioteca fue realmente seleccionada bajo `.../Users/runneradmin/.../jwplc_h0f1/esp32/libraries/JWPLC_Ethernet` pero el ejecutor esperaba `.../Users/RUNNER~1/...`, el mismo directorio en notación corta NTFS 8.3. **No fue un error de compilación productiva ni de selección de otra biblioteca.**

**F132 corrección candidata:** helper Win32 `GetLongPathNameW` para comparar identidad real de rutas, test positivo de alias 8.3 obtenido con `GetShortPathNameW` y negativo de directorio distinto; exigir source GCC `.d`, objeto `.o` y ELF de la copia temporal. La ruta del package `JWPLC/2.1.0`, su F1 y los periféricos se mantienen **SIN CAMBIOS**. Las pruebas Windows de este nuevo HEAD siguen **NO VERIFICADAS**; no se ha ejecutado la prueba física ni upload a COM4. Registro de fallos: [F132](A13_TOOLING_FAILURES_AND_PREVENTION_20261006.md). El conocimiento privado ya registró F132 en [jwplc-dev-mcp PR #15](https://github.com/JW-Control/jwplc-dev-mcp/pull/15), fusionado.

```text
ALPHA13=OPEN
F131=F1_PRODUCT_MERGED_PHYSICAL_PENDING
F132=HARNESS_CLI_PLATFORM_COLLISION_AND_NTFS_8DOT3_ALIAS
F132_CI_39286737779=PASS_G10R_2_OF_2_OLD_SHA
F132_CI_39286737847=FAIL_WINDOWS_N2_LIBRARY_8DOT3_FALSE_NEGATIVE
F132_8DOT3_FIX=PR_120_CANDIDATE_CI_PENDING
F132_LOCAL_BUILD=NOT_RETESTED_AFTER_8DOT3_FIX
F132_FINAL_SOURCE_FIRST=NOT_YET_VERIFIED
NEXT_FAILURE_ID=F133
COM4_UPLOAD=NOT_RUN
COM14=NOT_TOUCHED
G10-R-R1_R2_R3_R4=NOT_RUN_NOT_AUTHORIZED
G10_PACKAGE_PROMOTION=PENDING_OVERLAY_ONLY
ALPHA15=UNMODIFIED
NEXT_GATE=F132_WINDOWS_N0_N2_EXACT_SHA_THEN_OPERATOR_BUILD_ONLY
```

---

# ESTADO VIGENTE — 19/10/2026 — Alpha13 H0-F1 COM4: fallo F132 de selección de plataforma, sin compilación ni upload

**Fuente:** registros del operador, rama base `v2.1.0-alpha.13/feature/cleanup-robustness`, últimos HEAD locales `d813e1008b23e40bf5248e2111ba85595d864b7d` y `01f5aa9a850a332eca5acc2200c30223a2d6f5ea`, ambos con checkout limpio. **F131/F1 de producto permanece MERGED** (PR #116); la prueba H0 física no se inició.

**F132 — HARNESS confirmado:** los dos intentos del ejecutor COM4 se detuvieron en `H0F1_REVIEW_STOP=WRONG_PLATFORM_SELECTED` **antes** de compilar. La segunda ejecución diagnosticó plataforma Arduino Boards Manager `jwplc_local:esp32@2.1.0-dev`, distinta de `sketchbook/hardware/jwplc_local/esp32` temporal. El PR #119 solo mejoró la comparación de rutas, no cambió la selección real. Arduino CLI puede dar prioridad a Boards Manager sobre la plataforma manual de **igual identificador** ([issue arduino-cli#2766](https://github.com/arduino/arduino-cli/issues/2766)).

**Progreso CI intermedio:** [H0-F1 #39284131070](https://github.com/JW-Control/platform-jwplc/actions/runs/39284131070): Windows confirmó `H0F1_PLATFORM=PASS_ISOLATED_PACKAGE_PATH_CANONICAL`, pero la validación de biblioteca se detuvo en `WRONG_ETHERNET_LIBRARY`; el compilador regresó 0, **todavía no hay PASS de procedencia fuente→objeto→ELF**. La corrección de selección explícita `--library` forma parte del mismo F132 y requiere CI nueva con el SHA exacto. Registro ampliado en FAILURE F132. 

**Candidato no físico:** `codex/alpha13-h0-f1-cli-platform-collision-20261019`: monto el mismo package en un *vendor temporal exclusivo* `jwplc_h0f1:esp32:jwplcbasic` y exijo el path real y los archivos `.d/.o/ELF` de staging antes de cualquier upload. No se altera core/librerías/precompilados productivos, IDE ni Arduino15 del operador. **No marcar PASS de compilación ni H0 física sin evidencias exactas del SHA candidato.** G10-R-R1–R4 no autorizados, COM14 intocado.

Registro canónico: [A13_TOOLING_FAILURES_AND_PREVENTION_20261006.md](A13_TOOLING_FAILURES_AND_PREVENTION_20261006.md), incidente F132. La entrega es solamente por repositorio+git pull, no ZIP, conforme a la metodología.

```text
ALPHA13=OPEN
HITO=G10-R/H0-F1
F131=F1_PRODUCT_MERGED_H_PHYSICAL_PENDING
F132=CONFIRMED_HARNESS_PLATFORM_VENDOR_COLLISION
F132_FIX=STAGING_UNIQUE_VENDOR_CANDIDATE_NOT_YET_LOCAL_VALIDATED
F132_LOCAL_BUILD=REVIEW_STOP_BEFORE_COMPILATION_2_OF_2
F132_CI_PLATFORM=PASS_TEMP_VENDOR
F132_CI_LIBRARY=REVIEW_STOP_OLD_SHA
F132_CI_SOURCE_OBJECT_ELF=NOT_YET_PASS
F132_CI=NOT_YET_PASS_EXACT_HEAD
NEXT_FAILURE_ID=F133
COM4_UPLOAD=NOT_RUN
COM14=NOT_TOUCHED
G10-R-R1_R2_R3_R4=NOT_RUN_NOT_AUTHORIZED
G10_PACKAGE_PROMOTION=PENDING_OVERLAY_ONLY
ALPHA15=UNMODIFIED
BASE_SYNC=PENDING_F132_PR_REVIEW
KNOWLEDGE_SYNC=PENDING_F132_PRIVATE_PR
NEXT_GATE=F132_N0_WINDOWS_TEST_AND_SOURCE_FIRST_COM4_BUILD_ONLY
```

---

# ESTADO VIGENTE — 19/10/2026 — Alpha13 H0-F1 integrado en la base, prueba física COM4 preparada

> **Este bloque prevalece sobre estados históricos inferiores que describen PR #116 como Draft, F1 no implementado o E1/E2 pendientes.** [PR #116](https://github.com/JW-Control/platform-jwplc/pull/116) **fusionado** en `v2.1.0-alpha.13/feature/cleanup-robustness` mediante `f2c6821566fa62df744c3af8d4d70a440994e64f`. El único archivo productivo modificado por F1 fue `JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/utility/w5100.cpp`; CS desde `JWPLC_ETH_CS=5`, guarda `#error` GPIO6–11. Ningún variant, archivo congelado, GPIO4, autoload, periférico ni Alpha15 alterado. Documento de prueba: [A13_H_COM4_F1_PHYSICAL_PROTOCOL_20261019.md](A13_H_COM4_F1_PHYSICAL_PROTOCOL_20261019.md).

**CI definitiva del HEAD `ad9d07aa724297708ddfef48b019699ce7c3a120`, previa al merge, 7/7 jobs PASS, 0 omitidos:** [N0–N3 F1 #39207756701](https://github.com/JW-Control/platform-jwplc/actions/runs/39207756701) (4/4, Windows/Ubuntu, ambos FQBN, N3 11/11 sketches del package), [E1/E2 #39207756798](https://github.com/JW-Control/platform-jwplc/actions/runs/39207756798) (1/1, valor en ELF 10→5 y QIO=1), [G10-R legado compatible #39207756690](https://github.com/JW-Control/platform-jwplc/actions/runs/39207756690) (2/2). Su gate antiguo mantiene negativos y contrato original para ramas ajenas a F1; la excepción F1 requiere rama/base/diff productivo **exactos**.

**Solicitud del operador:** pasar a H0 físico con COM4 únicamente. Aún sin upload ni evidencia física nuevos; debe comprobarse banco local, CH340/MAC/flash 4 MB, fuente→objeto→ELF del package instalado y consentimiento textual antes de escribir. COM14 no se toca. No equivale a permiso para R1–R4 ni publicación. F130/F131 se mantienen, sin duplicar incidentes.

```text
ALPHA13=OPEN
HITO=G10-R/H0-F1
F1_PRODUCT_PR_116=MERGED
F1_PRODUCT_MERGE_SHA=f2c6821566fa62df744c3af8d4d70a440994e64f
H0_E1=PASS_ELF_SS_PIN_10_TO_5
H0_E2=PASS_QIO_SDKCONFIG_1
H0_F1_N0_N1_N2_N3=PASS_CI_4_OF_4_RUN_39207756701
H0_G10R_LEGACY_COMPAT=PASS_CI_2_OF_2_RUN_39207756690
H0_F1_TOTAL_CI=PASS_7_OF_7_NO_SKIPPED
H0_H=NOT_RUN_OPERATOR_LOCAL_CHECKS_PENDING
H0_COM4_UPLOAD=NOT_RUN
H0_COM14=NOT_TOUCHED
F130=CONFIRMED_PROCESS
F131=PRODUCT_F1_MERGED_PHYSICAL_OPEN
NEXT_FAILURE_ID=F132
PENDIENTE_DE_PROMOCIÓN=G10_W5500_SAFEACCESS_SOCKETS_ETHERNET_MODBUSTCP
G10-R-R1_R2_R3_R4=NOT_RUN_NOT_AUTHORIZED
A13-006=ADOPTION_BLOCKED
ALPHA15=UNMODIFIED
NEXT_GATE=H0_F1_COM4_OPERATOR_BUILD_IDENTITY_UPLOAD_THEN_UART_PHY
```

---

# ESTADO VIGENTE — 19/10/2026 — Alpha13 H0-F1: N0–N3 y E1/E2 PASS no físicos, H pendiente

> **Candidato NO fusionado:** [PR #116 Draft](https://github.com/JW-Control/platform-jwplc/pull/116), rama `codex/alpha13-h0-f1-ss-pin-default-20261019`, desde base `a9c96273274170195498ff282decf93f0388a450`. **F1 modifica únicamente `JWPLC_Ethernet/src/utility/w5100.cpp` dentro del package**. No hay flasheo, COM, merge ni publicación. Protocolo y evidencia: [A13_H0_F1_PACKAGE_SS_PIN_20261019.md](A13_H0_F1_PACKAGE_SS_PIN_20261019.md).

**Resultados no físicos verificados por SHA:**
- [CI F1 #39205102378](https://github.com/JW-Control/platform-jwplc/actions/runs/39205102378), SHA `9b57cfc324417d578ae25e3b5a9ff11593aaa726`: **4/4 jobs PASS, ninguno skipped**. N0 Ubuntu y Windows, N1 host con fuente literal (`10` original, `5` corregido, GPIO10 prohibido), N2 source-first `jwplcbasic` y `jwplcbasiccore`, N3 **11/11 sketches del package**, sin overlay. Artifact `a13-h0-f1-package-39205102378`; fuente→.d→.o→ELF verificada.
- [E1/E2 #39205926623](https://github.com/JW-Control/platform-jwplc/actions/runs/39205926623), SHA `df2174b6284554a16b55aa57679fce96806eee3d`: **1/1 job PASS**, con ELF original/actual recompilados del mismo package. E1 lee el símbolo `W5100Class::ss_pin` directamente del byte de su sección `PROGBITS`: **10 antes, 5 después**. GDB no estaba disponible; ELF brinda evidencia binaria independiente. E2 inspeccionó el `sdkconfig.h` del SDK efectivo y confirmó **`CONFIG_ESPTOOLPY_FLASHMODE_QIO=1`**. Artifact `a13-h0-e1e2-39205926623`.
- De SHA `9b57cfc` a `df2174b` solo se añadieron dos rutas del gate de evidencias; **0 archivos productivos modificados**, por lo que se reutiliza el PASS N0–N3 sobre el mismo firmware.
- **Nota CI separada:** el workflow legado `alpha13-g10-r-preflight.yml` falló para este PR al detectar intencionalmente `JWPLC/` y nuevas rutas fuera de su whitelist experimental. Es un **FAIL de alcance legado, no PASS**, y no invalida el gate dedicado F1. Se mantiene su política fail-closed sin relajarla.

**Límites:** H0 físico COM4 sigue NO AUTORIZADO; ningún PASS de CI valida la ausencia de WDT en banco. COM14 no tocado. R1–R4 bloqueados. Robustez G10 aún en overlays, pendiente de promoción a package, sin instrumentación diagnóstica.

```text
ALPHA13=OPEN
HITO=G10-R/H0-F1
H0_ROOT_CAUSE=IDENTIFIED_SOURCE_CONFIRMED
H0_FIX_F1=IMPLEMENTED_PACKAGE_BRANCH_CANDIDATE
H0_E1=PASS_ELF_BYTES_SS_PIN_10_TO_5_GDB_NOT_INSTALLED
H0_E2=PASS_SDKCONFIG_CONFIG_ESPTOOLPY_FLASHMODE_QIO_1
H0_F1_N0_N1_N2_N3=PASS_CI_4_OF_4_RUN_39205102378_SOURCE_FIRST_PACKAGE
H0_F1_N3=PASS_11_OF_11_PACKAGE_NO_OVERLAY
H0_E1_E2_CI=PASS_1_OF_1_RUN_39205926623
H0_H=NOT_RUN_NOT_AUTHORIZED
G10_R_LEGACY_SCOPE=FAIL_CLOSED_EXPECTED_FOR_APPROVED_F1_PRODUCT_CHANGE
F130=CONFIRMED_PROCESS
F131=CONFIRMED_PRODUCT_SOURCE_FIX_CANDIDATE
NEXT_FAILURE_ID=F132
PENDIENTE_DE_PROMOCIÓN=G10_W5500_SAFEACCESS_SOCKETS_ETHERNET_MODBUSTCP
G10-R-R1_R2_R3_R4=NOT_RUN
G10-R-H=REVIEW_STOP_NOT_AUTHORIZED
A13-006=ADOPTION_BLOCKED
ALPHA15=UNMODIFIED
BASE_SYNC=OK_AT_START_F1_PR_116_PENDING_OPERATOR_MERGE
KNOWLEDGE_SYNC=OK
NEXT_GATE=H0_F1_OPERATOR_REVIEW_PR_116_THEN_EXPLICIT_COM4_H_AUTH
```

---

# ESTADO VIGENTE — 19/10/2026 — Alpha13 G10-R / H0: causa raíz identificada (SS_PIN_DEFAULT=10 → GPIO10 flash), corrección F1 pendiente

> **Leer este encabezado antes del bloque del 18/10 que continúa debajo.** La auditoría independiente READ-ONLY cerró la pregunta abierta de H0-F. **No había deadlock.** Documento canónico: [A13_H0_ROOT_CAUSE_SS_PIN_20261019.md](A13_H0_ROOT_CAUSE_SS_PIN_20261019.md). Fallos registrados: **F130** (proceso) y **F131** (defecto latente de producto) en [A13_TOOLING_FAILURES_AND_PREVENTION_20261006.md](A13_TOOLING_FAILURES_AND_PREVENTION_20261006.md).

**Causa raíz (confirmada en fuente):**
- La cadena `SS_PIN_DEFAULT` de `utility/w5100.cpp` (código original de Arduino Ethernet, presente desde Alpha4 e idéntica en Alpha12) solo reconoce macros. El variant declara `SS` como `static const`, así que el resultado es **10**.
- `JWPLC_Ethernet.linkStatus()` no aplica `Ethernet.init(JWPLC_ETH_CS)`; solo lo hace `probeHardware()` desde el autoload. Si el sketch consulta antes del primer `service()`, `W5100.init()` → `initSS()` ejecuta `pinMode(10, OUTPUT)`.
- GPIO10 es IO2 de la flash en QIO. La CPU queda sin acceso a flash y salta `rst:0x8 TG1WDT_SYS_RESET`, justo después de la etapa 63.
- Explica A–F: C funcionó porque llegó después del primer `service()`; el resto falló en el primer READY.

**Corrección aprobada en principio, NO implementada:** F1 en `w5100.cpp`, `PIN_SPI_SS_ETHERNET_LIB = JWPLC_ETH_CS` (`jwplc_spi_bus.h`, única fuente de verdad del pin) más una guarda `#error` para GPIO6–11 en ESP32. **F2** (`Ethernet.init(_csPin)` en cada `linkStatus()`) **se descarta**: Alpha12 tampoco lo hacía y con F1 no aporta nada en el uso normal. No se toca el variant, las APIs, el autoload ni los periféricos; `LINK=-9` sigue siendo un centinela de laboratorio y no una solución.

**Hallazgos de alcance para el cierre de Alpha13:**
- **La robustez Ethernet/SPI de G10 existe solo en el overlay temporal de 15 fuentes** (`a13_g10_r_build_candidate.py`). Frente al package de `f0b9075` faltan `JWPLC_W5500_SafeAccess.cpp/.h` y cambios en `EthernetClient/Udp/Server`, `socket.cpp`, `JWPLC_Ethernet.cpp`, `JWPLC_W5x00_Ethernet.h` y `JWPLC_ModbusTCP*`. G1–G8 sí se promovieron (27 archivos). **El cierre exige promover al package lo validado, con F1 incluido, y validar el package instalado; la instrumentación H0 no pasa a producto.**
- La rama base `v2.1.0-alpha.13/feature/cleanup-robustness` no recibe commits desde 14/10. El estado vivo avanzó por la cadena `codex/alpha13-*` (PR #105, #109–#114, Draft). La documentación de esta auditoría está en `claude/w5500-watchdog-audit-5vobvz`, basada en `e5476a8`.

**Evidencia pendiente sin flashear:**
- **E1:** `xtensa-esp32-elf-gdb -batch -ex "p/d 'W5100Class::ss_pin'" <ELF H0-F>`. Se espera 10.
- **E2:** `CONFIG_ESPTOOLPY_FLASHMODE_QIO=1` en `esp32/qio_qspi/include/sdkconfig.h`.

```text
ALPHA13=OPEN
BASE_SYNC=OK KNOWLEDGE_SYNC=OK
HITO=G10-R
H0_ROOT_CAUSE=IDENTIFIED_SOURCE_CONFIRMED
H0_ROOT_CAUSE_OBJECT_E1=PENDING
H0_FLASH_MODE_E2=PENDING
H0_FIX_F1=APPROVED_IN_PRINCIPLE_NOT_IMPLEMENTED
H0_FIX_F2=DISCARDED
F130=CONFIRMED_PROCESS
F131=CONFIRMED_SOURCE_LATENT_SINCE_ALPHA4
NEXT_FAILURE_ID=F132
G10_PACKAGE_PROMOTION=PENDING_OVERLAY_ONLY
G10-R-H=REVIEW_STOP_NOT_AUTHORIZED
G10-R-R1_R2_R3_R4=NOT_RUN
A13-006=ADOPTION_BLOCKED
ALPHA15=UNMODIFIED
NEXT_GATE=H0_E1_E2_THEN_F1_ISOLATED_BRANCH_N0_N1_N2_N3_THEN_AUTHORIZED_COM4_CHECK
```

---

# ESTADO VIGENTE — 18/10/2026 — Alpha13 G10-R / H0: watchdog localizado en ventana initSS, raíz pendiente

> **Leer este encabezado antes del historial de 16–17/10 que continúa debajo.** El estado PRE-H original (`WAIT_OPERATOR_IDENTITY_AND_FULL_FLASH_BACKUPS`) quedó **superado para las dos unidades de desarrollo**, porque el operador comprobó identidades, autorizó por excepción renunciar a backup y realizó cargas controladas. Esa excepción no autoriza subidas futuras ni la campaña R1–R4.

**Prueba física más reciente: H0-F COM4 — upload PASS exclusivamente COM4; diagnóstico FAIL por watchdog reproducido.** Código efectivamente compilado/subido `f0b9075130158b99d89e0f9a574d964236614b78` desde rama `codex/alpha13-h0-w5100-init-stage-20261018` / [PR #114 Draft](https://github.com/JW-Control/platform-jwplc/pull/114). El operador presentó evidencia local: `G10_R_H0_E_W5100_SOURCE_TO_OBJECT=PASS`, `G10_R_BUILD_COM4=PASS_SOURCE_FIRST`, `G10_R_STAGE=PASS_15_SOURCES`, `G10_R_H0_DIAG_UPLOAD=PASS_COM4_ONLY`, `G10_R_H0_COM14=NOT_TOUCHED`. El objeto `w5100.cpp.o` procede de la fuente staged instrumentada. Ficha waiver local dev, identidad MAC/4 MB verificada antes de escritura y consentimiento específico H0-F introducido por el operador; no copiar MAC o waiver a GitHub.

**Evidencia serial F (18/10 ~19:07):** primer `G10R_READY:... LINK=` se trunca, seguido de `rst:0x8 (TG1WDT_SYS_RESET)`. En el boot siguiente:
```text
G10R_H0_PREVIOUS:RUN=1 ATTEMPT=1 LAST_STAGE=63 VALID=1
G10R_H0_TRACE=REBOOT_NO_AUTO_REPEAT
```
`Stage 63` es el checkpoint **después de `SPI.begin()`**, previo a que `initSS()` termine (`Stage 64` no alcanzado). En `JWPLC_Ethernet/src/utility/w5100.h`, la rama ESP32 `initSS()` ejecuta `pinMode(ss_pin, OUTPUT)`. **Bloqueo temporalmente localizado en initSS; NO se ha demostrado error en pinMode, CS ni deadlock específico.** Tras desarmar la sonda, Master `BOOT=B1D755A8` recuperó IP `192.168.0.31` con READY continuo en el intervalo observado, pero `LINK=-9` es centinela ficticio y no una corrección. No confundir el `rst:0x1 (POWERON_RESET)` manual previo con el `rst:0x8` de WDT.

**Historial H0 en una línea:** A inicial (WDT repetitivo en LINK); B omisión de lectura adicional (sin WDT ~30 s); C llamada real tardía (`PHY=1 STAGE=50` y ~25 s sin WDT); D primer READY (`ATTEMPT=1 STAGE=30`, WDT); E getLinkStatus interno (`STAGE=32`, WDT); F initSS (`STAGE=63`, WDT). Todos B–F son sketches/overlays diagnósticos, **NO fuente productiva adoptada**; las lecturas reales siguen siendo necesarias en el package.

**CI del HEAD físico F — 15/15 jobs PASS** (nonphysical, **no valida estabilidad del firmware**): [B2 2/2 #39177861353](https://github.com/JW-Control/platform-jwplc/actions/runs/39177861353), [B4 2/2 #39177861327](https://github.com/JW-Control/platform-jwplc/actions/runs/39177861327), [G10-R source-first Windows/Ubuntu 5/5 #39177861376](https://github.com/JW-Control/platform-jwplc/actions/runs/39177861376), [TCP124 Windows/Ubuntu 2/2 #39177861345](https://github.com/JW-Control/platform-jwplc/actions/runs/39177861345), [B3 N0/N1/N3 4/4 (N3 11/11) #39177861330](https://github.com/JW-Control/platform-jwplc/actions/runs/39177861330). Los commits documentales posteriores al SHA físico requieren distinguir HEAD documental vs código y, si CI se vuelve a disparar, registrar nueva conclusión por SHA exacto.

**Handoff y segunda revisión independiente:** [A13_G10_R_H0_HANDOFF_CLAUDE_CODE_20261018.md](A13_G10_R_H0_HANDOFF_CLAUDE_CODE_20261018.md) y [A13_G10_R_H0_PROMPT_CLAUDE_CODE_20261018.md](A13_G10_R_H0_PROMPT_CLAUDE_CODE_20261018.md). Solicitar primero auditoría read-only sobre `ss_pin` efectivo, `pinMode`/initSS, propiedad SPI/autoload, reentrancia `W5100.init()`, task scheduling y fuente→objeto→ELF, **antes de otra sonda o carga física**. No aceptar bypass `LINK=-9` en producción ni eliminar periféricos/autoload.

```text
ALPHA13=OPEN
HITO=G10-R
H0_F_UPLOAD_COM4=PASS_COM4_ONLY
H0_F_COM14=NOT_TOUCHED
H0_F_PHYSICAL_WDT=REPRODUCED_FIRST_READY_STAGE_63
H0_ROOT_CAUSE=OPEN_PENDING_INDEPENDENT_REVIEW
H0_BACKUP_POLICY=WAIVED_TWO_DEV_BOARDS_ONLY_BY_OPERATOR
G10-R-H=REVIEW_STOP_NOT_AUTHORIZED
G10-R-R1_R2_R3_R4=NOT_RUN
R3_COMPLETE_PHYSICAL_CAMPAIGN_READY=FALSE
R3_PHYSICAL_INJECTION_READY=FALSE
A13-006=ADOPTION_BLOCKED
ALPHA15=UNMODIFIED
NEXT_GATE=H0_ROOT_CAUSE_READ_ONLY_REVIEW_THEN_EXPLICIT_APPROVAL
```

---

> **H0 físico 17/10/2026 — UPLOAD PASS / MASTER WDT REVIEW_STOP.** El operador confirmó `G10_R_UPLOAD_STATUS=PASS_BOTH_EXPLICIT` y HEAD firmware `701196fc5f1b998fcc1b4c0082e9c75e86672a5a` para COM14 Slave y COM4 Master. Ambos TFT inicialmente mostraron IDLE (etiqueta A13); **COM4 Master se reinicia de forma repetida**: `rst:0x8 (TG1WDT_SYS_RESET)` aproximadamente cada 2 s, cambia `G10R_BOOT:NONCE`, llega a `TCP=1 RTU=1`, y el último registro siempre se trunca en `G10R_READY:IP=0.0.0.0 TCP=0 UDP=0 RTU=1 BOOT=<cambiante> LINK=`. Master estaba **sin RJ45, solo USB**; 0.0.0.0/TCP=0 sin router no prueba fallo DHCP. Código del sketch: `G10R_READY` imprime `Serial.print((int)JWPLC_Ethernet.linkStatus())` tras `LINK=`; `JWPLC_EthernetClass::linkStatus()` toma mutex SPI hasta 200 ms y consulta `Ethernet.linkStatus()->W5100.getLinkStatus()`. **Pista de bloqueo, causalidad aún no demostrada**: también existe servicio Ethernet autoload concurrente. Repetidos resets descartan que sea solo refresco visual TFT. El slave fue flasheado pero aún no se acreditó estabilidad RTU física. **G10-R-H=REVIEW_STOP_NO_CAMPAIGN**, sin R1/R2/R3/R4, router/RS-485 sin conectar, sin cambio productivo ni Alpha15. **No repetir flash ni asociar 19/19 CI con PASS físico**. Siguiente acción autorizable: diagnóstico controlado del enlace en router de banco aislado (solo RJ45 COM4, sin RS485, sin cargas, observación 15–30 s, abortar si mantiene resets); si persiste, aislar en variante sketch experimental fuera del package la llamada `linkStatus` y revalidar compilaciones antes de nuevo upload.

> **H0 — banco confirmado por operador (17/10/2026):** Hay dos JWPLC Basic disponibles y aislados. Asignación prevista: **COM4 Master con microSD**, **COM14 Slave RTU sin microSD**. Ambos deberán recibir nuevamente su firmware de laboratorio **solo después** de verificar identidad/ausencia de cargas, realizar respaldos completos de flash y recibir consentimiento de carga. La microSD se prueba únicamente en COM4; no exigirla en COM14. **El respaldo flash no respalda el contenido microSD**. Registro y procedimiento: [B4 banco y backups](A13_G10_R_PREH_B4_BANCO_RESPALDOS_SEGURIDAD_20261017.md). Se conserva `PREH_STEPS_REMAINING=0` (ingeniería), pero `G10-R-H0=WAIT_OPERATOR_IDENTITY_AND_FULL_FLASH_BACKUPS`, `G10-R-H=NOT_RUN`, `R3_COMPLETE_PHYSICAL_CAMPAIGN_READY=FALSE` y `R3_PHYSICAL_INJECTION_READY=FALSE`. Esta confirmación **no autoriza** lecturas ni flasheo.

# ESTADO VIGENTE — 17/10/2026 — PRE-H B1–B4 cerrados NO FÍSICOS

**PRE-H completamente preparado y validado en CI para el candidato de laboratorio.** `B1/B2/B3/B4=CLOSED_NONPHYSICAL`, **0 bloques de ingeniería pendientes para solicitar H0 al operador**. El código exacto verificado es `33990007405cac2b5af9c110943d94f9201e6add`; los cambios documentales posteriores no alteran firmware, staging, guards ni runners. PR #109 sigue **Draft** sin merge.

**CI del mismo código, 19/19 jobs PASS sin omitted/skipped:** [B4 seguridad/backup #39037163647](https://github.com/JW-Control/platform-jwplc/actions/runs/39037163647) 2/2; [N0 G10-R #39037163623](https://github.com/JW-Control/platform-jwplc/actions/runs/39037163623) 2/2; [B1 offline #39037163606](https://github.com/JW-Control/platform-jwplc/actions/runs/39037163606) 2/2; [B2 offline #39037163624](https://github.com/JW-Control/platform-jwplc/actions/runs/39037163624) 2/2; [TCP124 source-first #39037163653](https://github.com/JW-Control/platform-jwplc/actions/runs/39037163653) 2/2; [G10-R build/replay/nativo #39037163667](https://github.com/JW-Control/platform-jwplc/actions/runs/39037163667) 5/5; [B3 integrado N0/N1/N3 #39037163676](https://github.com/JW-Control/platform-jwplc/actions/runs/39037163676) 4/4, N3 candidato **11/11** con artifact y fuente→objeto→ELF. Baseline N3 11/11 reutilizado tras demostrar equivalencia exacta `JWPLC/` con G10-I `a954cabd`. Cinco archives congelados y demás precompilados intactos; Arduino IDE local no ejecutado.

**Documento B4 operativo:** [A13_G10_R_PREH_B4_BANCO_RESPALDOS_SEGURIDAD_20261017.md](A13_G10_R_PREH_B4_BANCO_RESPALDOS_SEGURIDAD_20261017.md). Script `tools/alpha13/gates/a13_g10_r_b4_bench.py` genera ficha LOCAL (vacía por defecto), verifica dos backups completos de flash/identidades/seguridad con 14 negativos y bloquea uso de `--upload`/`--live` sin ficha. R2 requiere UART real LINKOFF y DHCP READY tras las ventanas completas de 12+96 segundos por ciclo. Sólo el **operador** puede verificar banco, leer flash de ambas placas, respaldar, aprobar puertas A/B y aportar la evidencia. **R5/power-cycle** y restauración física siguen separados.

```text
PREH_B1_TCP124=CLOSED_NONPHYSICAL
PREH_B2_DHCP_FAIRNESS=CLOSED_NONPHYSICAL
PREH_B3_N0_N1_N2_N3=CLOSED_NONPHYSICAL_CI_VERIFIED
PREH_B4_BENCH_BACKUP_SAFETY=CLOSED_NONPHYSICAL_CI_VERIFIED
PREH_STEPS_REMAINING=0
G10-R-H0=WAIT_OPERATOR_PHYSICAL_BENCH_IDENTITY_AND_FULL_FLASH_BACKUPS
G10-R-H=NOT_RUN_NOT_AUTHORIZED
R3_COMPLETE_PHYSICAL_CAMPAIGN_READY=FALSE
R3_PHYSICAL_INJECTION_READY=FALSE
A13-006=ADOPTION_BLOCKED
ALPHA15=UNMODIFIED
G10-R-R5=NOT_AUTHORIZED
NEXT_GATE=G10_R_H0_OPERATOR_BENCH_BACKUPS_AND_SEPARATE_AUTH
```

---

# Estado vigente PRE-H — 17/10/2026: B3 integrado CERRADO, solo B4 pendiente

**El bloque 3 N0/N1/N2/N3 está `CLOSED_NONPHYSICAL_CI_VERIFIED`**. Código exacto `ba1b0b1ddad78fbdc574b1590abae06bff1030a6`; los commits posteriores a ese SHA son exclusivamente de documentación. No se ha adoptado G10-R en rama productiva.

**CI de integración del candidato:** [#39015874604](https://github.com/JW-Control/platform-jwplc/actions/runs/39015874604) 4/4 jobs PASS (N0 Ubuntu + Windows, N1 nativo y N3 candidato 11/11). [#39015874548](https://github.com/JW-Control/platform-jwplc/actions/runs/39015874548) 5/5 PASS (replay, source-first COM4/COM14 y C++ source-backed). N0 histórico reconciliado [#39015874600](https://github.com/JW-Control/platform-jwplc/actions/runs/39015874600) 2/2 PASS. N2 TCP124 [#39015874532](https://github.com/JW-Control/platform-jwplc/actions/runs/39015874532) 2/2 PASS. **N3 baseline** reutiliza 11/11 de G10-I [#38843599043](https://github.com/JW-Control/platform-jwplc/actions/runs/38843599043) tras comprobación exacta de `JWPLC/` sin cambios; **N3 candidate** es recompilación nueva de 11/11 con el overlay B1/B2. Los archives, GPIO4 y Alpha15 permanecen intactos.

**Documento de cierre con gates/decisiones/artifact y límites:** [A13_G10_R_PREH_B3_CIERRE_20261017.md](A13_G10_R_PREH_B3_CIERRE_20261017.md).

```text
PREH_BLOCK_1_TCP124=CLOSED_NONPHYSICAL
PREH_BLOCK_2_DHCP_FAIRNESS=CLOSED_NONPHYSICAL
PREH_BLOCK_3_INTEGRATED_N0_N1_N2_N3=CLOSED_NONPHYSICAL_CI_VERIFIED
PREH_BLOCK_4_PHYSICAL_SAFETY_BACKUP_AUTH=OPEN
PREH_STEPS_REMAINING=1
G10-R-H=BLOCKED_NOT_AUTHORIZED
G10-R-PHYSICAL=NOT_RUN
R3_PHYSICAL_INJECTION_READY=FALSE
R3_COMPLETE_PHYSICAL_CAMPAIGN_READY=FALSE
A13-006=ADOPTION_BLOCKED
ALPHA15=UNMODIFIED
NEXT_GATE=PREH_BLOCK_4_PHYSICAL_SAFETY_BACKUP_AND_OPERATOR_AUTH
```

---

# Estado PRE-H vigente — 17/oct/2026: bloques 1 y 2 preparados, 2 restantes

**B2 DHCP/fairness: `CLOSED_NONPHYSICAL_SOURCE_FIRST`.** Se instrumentó mantenimiento DHCP real del backend tras liberar SPI; métricas H3E1 de TFT, edad de scan I/O/TCA cacheado, RTC, SD, FRAM, RTU, UDP y botón; validador fail-closed por **fase+token** para R3. Esta decisión es de preparación y compilación, **NO valida físicamente** renew/rebind, link loss ni fairness del banco.

**Evidencia:** [B2 Windows+Ubuntu #39014708958](https://github.com/JW-Control/platform-jwplc/actions/runs/39014708958) 2/2 SUCCESS; [B1 integración #39014708863](https://github.com/JW-Control/platform-jwplc/actions/runs/39014708863) 2/2 SUCCESS; [N2 source-first de fuentes staging B2/H3E1 #39014564729](https://github.com/JW-Control/platform-jwplc/actions/runs/39014564729) 2/2 SUCCESS (fuentes idénticas tras los cambios de parser/validador). [N2 exacto del HEAD de código #39014708876](https://github.com/JW-Control/platform-jwplc/actions/runs/39014708876) requiere comprobar su conclusión final antes de reutilizarlo como PASS. Detalle: [B2 DHCP y fairness](A13_G10_R_PREH_B2_DHCP_FAIRNESS_20261017.md).

```text
PREH_BLOCK_1_TCP124=CLOSED_NONPHYSICAL_SOURCE_FIRST
PREH_BLOCK_2_DHCP_FAIRNESS=CLOSED_NONPHYSICAL_SOURCE_FIRST
PREH_BLOCK_3_INTEGRATED_N0_N1_N2_N3=OPEN
PREH_BLOCK_4_PHYSICAL_SAFETY_BACKUP_AUTH=OPEN
PREH_STEPS_REMAINING=2
G10-R-H=BLOCKED
G10-R-DHCP_RENEW_REBIND_PHYSICAL=NOT_RUN
G10-R-TFT_TCA_FAIRNESS_PHYSICAL=NOT_RUN
R3_PHYSICAL_INJECTION_READY=FALSE
R3_COMPLETE_PHYSICAL_CAMPAIGN_READY=FALSE
A13-006=ADOPTION_BLOCKED
ALPHA15=UNMODIFIED
NEXT_GATE=PREH_BLOCK_3_INTEGRATED_N0_N1_N2_N3
```

---

# Estado vigente PRE-H — 17/oct/2026 (prioridad sobre registros históricos)

**Bloque 1 TCP124: `CLOSED_NONPHYSICAL_READY_FOR_PREH_INTEGRATION`.** Sólo se cierra la implementación y verificación **no física**, no se declara validación de hardware ni adopción al package. Rama de implementación `codex/alpha13-preh-tcp124-review-20261016`, HEAD de código `dc194c4d6e863d5324eea14e515d1be92b18c9b6`.

- [CI TCP124 offline #39010539929](https://github.com/JW-Control/platform-jwplc/actions/runs/39010539929): **2/2 SUCCESS** Windows y Ubuntu. Incluye host 1/2/4 con sockets loopback, contrato UART crudo, collector y correlación PC-TID↔UART-lease↔lifecycle con mutantes.
- [CI TCP124 source-first #39010539920](https://github.com/JW-Control/platform-jwplc/actions/runs/39010539920): **2/2 SUCCESS** Windows y Ubuntu, COM4 y COM14. Valida plataforma staged, biblioteca seleccionada, fuente `JWPLC_ModbusTCP.cpp` → dependencia `.d` → objeto `.o`, así como W5500 SafeAccess; el producto sigue intacto.
- Implementación: hook sólo en 14 fuentes del staging experimental; `G10R_TCP_OBS`/ring UART, `CAPTURE`, `DROPPED`, `OVERFLOW`, TID y tupla SID/EPOCH/ALLOC/OWNER. PC: 1/2/4 clientes contra banco de dos JWPLC como máximo; estrés integrado en una sola R3 y evaluador fail-closed del manifest.
- **Límites:** el código source-backed no es captura física. Los cierres W5500 durante SPI_BUSY, DHCP real ante pérdida de enlace, fairness de TFT/TCA/I/O y endurance NO se han probado con este HEAD en hardware. El servidor Modbus mantiene una sola sesión activa; 4 sockets TCP abiertos no prueban 4 FC03 simultáneos.
- El workflow legado G10-R usa una lista fija de archivos anterior al PR PRE-H y puede rechazar el nuevo alcance. Su rechazo por whitelist **no se reinterpreta como PASS**, y no invalida los dos workflows específicos de B1; N3 integrado del package será el paso 3.

```text
PREH_BLOCK_1_TCP124=CLOSED_NONPHYSICAL_SOURCE_FIRST
PREH_BLOCK_2_DHCP_FAIRNESS=OPEN
PREH_BLOCK_3_INTEGRATED_N0_N1_N2_N3=OPEN
PREH_BLOCK_4_PHYSICAL_SAFETY_BACKUP_AUTH=OPEN
PREH_STEPS_REMAINING=3
G10-R-PHYSICAL=NOT_RUN
R3_PHYSICAL_INJECTION_READY=FALSE
R3_COMPLETE_PHYSICAL_CAMPAIGN_READY=FALSE
A13-006=ADOPTION_BLOCKED
ALPHA15=UNMODIFIED
NEXT_GATE=PREH_BLOCK_2_DHCP_RECOVERY_AND_PERIPHERAL_FAIRNESS
```

---

# Alpha13 — G10-R, estado vivo de preparación (16/10/2026)

> **16-oct — R3 preparado para inyección en banco, SIN EJECUCIÓN FÍSICA:** el único sketch COM4 temporal `g10_r_integrated.ino` incluye cinco sondas bajo mutex W5500 real (`UDP.stop`, `DNSClient.cancelResolveAsync`, `EthernetClient.cancelConnectAsync`, `DhcpClass.cancelDHCP`, `DhcpClass.finishAsync`). Las dos rutas DHCP usan un objeto `DhcpClass` **local** cuyo UDP privado se arma exclusivamente en puerto **15023** por un `friend` inyectado solo al **staging**: no emite DHCP DISCOVER, no modifica el DHCP global ni productivo. El runner incorpora `r3_suite()` para una **única campaña R1→R2→R3(5)→R4** con abortos y captura de identidad de cada lease. Nuevo handshake `R1R2R3R4_FAULT_LAB_V2`; build y runner rechazan protocolo viejo. Se mantiene `--upload/--live` **fail-closed**: aún faltan cobertura TCP 1/2/4, DHCP renew/rebind físico, fairness TFT/TCA, revisión N2 Windows/Ubuntu del HEAD exacto y autorización del banco. **No se tocó Alpha15 ni se flashearon COM4/COM14.** [Plan R3 y límites](A13_G10_R_CAMPANA_INTEGRADA_Y_TRANSFERENCIA_ALPHA15_20261016.md).

```text
G10-R-R3-STAGED_FIVE_MODES=UDP_DNS_TCP_DHCP_CANCEL_DHCP_FINISH
G10-R-R3-DHCP_PROBE=PACKET_FREE_ISOLATED_INSTANCE
G10-R-R3-FIELD_RUNNER=ONE_INTEGRATED_R1_R2_R3_R4_CAMPAIGN_CODE_PRESENT
G10-R-R3-UDP_DNS_TCP_DHCP_PHYSICAL=NOT_RUN
G10-R-R3-CONCURRENT_TCP_1_2_4=REVIEW_REQUIRED
G10-R-R3-PHYSICAL_DHCP_RENEW_REBIND=NOT_RUN
G10-R-R3-TFT_TCA_FAIRNESS=NOT_MEASURED
G10-R-R3-H=FAIL_CLOSED
G10-R-FLASH=BLOCKED
A13-006=ADOPTION_BLOCKED
ALPHA15=UNMODIFIED
NEXT_GATE=PREH_CONCURRENT_TCP_FAIRNESS_AND_FINAL_CI_THEN_AUTHORIZED_H
```


> **16/10 — Consolidación PRE-H y Alpha15:** la CI de referencia [#38998540944](https://github.com/JW-Control/platform-jwplc/actions/runs/38998540944) fue **5/5 SUCCESS** para `5e4a6d78` (anterior). En HEAD posteriores, se integró al host la captura `r3_udp_lines/sessions/verdicts` con lease exacto y cierres paralelos permitidos, **17 mutantes de contrato offline**, y se blindaron tanto `--live` como `--upload` antes de abrir COM o flashear mientras R3 completo no exista. **Los cambios más recientes necesitan CI por su HEAD exacto**. Se realizó comparación READ-ONLY de blobs entre producto Alpha13 y `v2.1.0-alpha.15-HmiDesigner`: `Dns.cpp`, `EthernetClient.cpp`, `EthernetUdp.cpp`, `JWPLC_TFT.h` y archive TFT **divergen**; `platform.txt`, `JWPLC_Display_API.h`, `JWPLC_Display.cpp`, `DhcpAsyncControl.cpp`, `JWPLC_Ethernet.cpp` y header RTU **coinciden** en los SHA consultados. [Matriz de transferencia](A13_ALPHA15_COMPAT_READONLY_PRETRANSFER_20261016.md). Alpha15 no fue modificada; **G10-H físico R3 no ejecutado, `A13-006=ADOPTION_BLOCKED`**.

```text
G10-R-R3-UDP_HOST=SOURCE_INTEGRATED_17_OFFLINE_NEGATIVES_CI_FINAL_PENDING
G10-R-LIVE=BLOCKED_BEFORE_COM
G10-R-UPLOAD=BLOCKED_BEFORE_COM
G10-R-R3_DNS_DHCP_TCP_PHYSICAL=NOT_READY
G10-R-H=NOT_AUTHORIZED
G10-R-ALPHA15_DIFF=READ_ONLY_CRITICAL_ETHERNET_TFT_DIVERGENCE
ALPHA15_BRANCH_MODIFIED=NO
PRODUCT_MODIFIED=NO
A13-006=ADOPTION_BLOCKED
```


> **PRE-H / R3-UDP 16-oct:** corregida la correlación de lease en presencia de cierres TCP/DNS/DHCP concurrentes: ahora se exige exactamente **un cierre de la sonda UDP identificada**, no un único cierre global. El contrato añade el caso positivo paralelo y **17 negativos**. El runner PC conserva `r3_udp_lines/r3_udp_sessions/r3_udp_verdicts` en el manifiesto y el build gate tiene un bloqueo físico explícito (`R3_COMPLETE_PHYSICAL_CAMPAIGN_READY=False`) adicional al bloqueo de captura (`R3_PHYSICAL_INJECTION_READY=False`). **CI #38998540944 5/5 SUCCESS corresponde a HEAD anterior `5e4a6d78`; verificar CI del HEAD vigente antes de atribuir aprobación.** No hay prueba de campo R3, merge ni flash nuevos. [PRE-H integrado](A13_G10_R_CAMPANA_INTEGRADA_Y_TRANSFERENCIA_ALPHA15_20261016.md).

```text
G10-R-R3_UDP_PCHOST=SOURCE_WIRED_17_NEGATIVES
G10-R-HARDWARE_UPLOAD=BLOCKED_BEFORE_COM
G10-R-HARDWARE_CAPTURE=BLOCKED_BEFORE_COM
G10-R-R3_COMPLETE=NOT_READY
A13-006=ADOPTION_BLOCKED
ALPHA15_TRANSFER=NOT_AUTHORIZED
```


> **R3 — avance de fault injection (16/10/2026):** el sketch temporal agrega una única sonda explícita `G10R_R3_UDP:<token>`: task FreeRTOS con `BusScope(0)` con espera nominal de **80 ms** y duración real **medida en microsegundos** (rango contractual 60–120 ms; cualquier exceso `HOLD_TIMING_REVIEW`), `EthernetUDP::stop()` real durante la contención y lectura del lease `SID/EPOCH/ALLOC/OWNER` antes de inyectar, para correlacionar `QueueRequest→DrainClose` del mismo socket. Esta subfase requiere una sesión de banco autorizada para demostrar el comportamiento físico; **todavía no se ejecutó en hardware**. `a13_g10_r_r3_probe_contract.py` integra 16 mutantes sintéticos, sin reemplazar los 7 mutantes C++ ni los 14 métodos DNS/DHCP/UDP ya validados en host. **La campaña H integral sigue bloqueada en `R3_PHYSICAL_INJECTION_READY=False`** porque faltan inyectores físicos DNS/DHCP/TCP y 1/2/4 clientes con pruebas de seguridad. **No usar `--live` ni flashear COM4/COM14.** [Plan PRE-H](A13_G10_R_CAMPANA_INTEGRADA_Y_TRANSFERENCIA_ALPHA15_20261016.md).

```text
G10-R-R3-UDP=SOURCE_ONLY_LOCK80_PARTIAL_NOT_PHYSICALLY_RUN
G10-R-R3-UDP-LEASE=EXACT_PROVENANCE_SID_EPOCH_ALLOC_OWNER
G10-R-R3-UDP-REPLAY=14_NEGATIVES_SYNTHETIC
G10-R-R3-DNS_DHCP_TCP=PHYSICAL_INJECTION_NOT_IMPLEMENTED
G10-R-H=FAIL_CLOSED
A13-006=ADOPTION_BLOCKED
ALPHA15_PRODUCT_TRANSFER=NOT_AUTHORIZED
```


> **Actualización PRE-H integrada (16/10/2026):** al preparar una única campaña física R1→R2→R3→R4 se detectó que el runner anterior ejecutaba solo R1/R2/R4. `a13_g10_r_field_runner.py --live` queda **bloqueado intencionalmente** antes de abrir COM, con `G10R_PHYSICAL_BLOCKED_R3_PHASE_MISSING_IN_INTEGRATED_CAMPAIGN`. Su `--selftest` conserva este requisito. La batería nativa del job R3 integra ahora **14 métodos fuente reales** DNS/DHCP/UDP (finish, cancel, timeout, renew/rebind y multicast) con el SafeAccess y trazador del **mismo overlay temporal de 14 fuentes**. En la CI del commit `e9aca4df` el job nativo informó **12 marcadores positivos** y **3 mutaciones negativas rechazadas**; el job previo de SafeAccess sigue cubriendo 7 mutaciones. La compilación completa Windows/Ubuntu del **HEAD vigente** requiere conclusión explícita antes de marcar cierre N0–N2. Documentado en [PRE-H integrado y transferencia Alpha15](A13_G10_R_CAMPANA_INTEGRADA_Y_TRANSFERENCIA_ALPHA15_20261016.md). Ninguna modificación productiva ni transferencia a la rama Alpha15 autorizada.

```text
G10-R-PREH=INTEGRATED_NONPHYSICAL_IMPLEMENTED_CI_HEAD_PENDING
G10-R-R3-NATIVE=14_REAL_DNS_DHCP_UDP_METHODS_WITH_R3_TRACE
G10-R-R3-NEGATIVES=3_PROTOCOL_AND_7_SAFEACCESS_SOURCE_BACKED
G10-R-H=FAIL_CLOSED_R3_INJECTION_PHASE_NOT_IMPLEMENTED
G10-R-R1_R2_R3_R4-PHYSICAL=NOT_RUN
G10-R-ALPHA15=READ_ONLY_COMPATIBILITY_ALLOWED_NO_PRODUCT_MERGE
A13-006=ADOPTION_BLOCKED
NEXT_GATE=R3_DETERMINISTIC_PHYSICAL_FAULT_INJECTION_AND_INTEGRATED_H_PREP
```


> **Actualización R3 / historial (16-oct-2026):** el overlay de laboratorio incorpora un anillo fijo de **64 eventos**, con `try-lock` no bloqueante, secuencia, propietario, epoch, allocation y detección de `dropped/overrun`. Sin usar GPIO4 ni escribir SD/FRAM/Q, `G10R_EVENTS:<token>:AFTER=<seq>` exporta eventos al runner, que conserva `r3_lifecycle_trace` y el resultado del auditor semántico. El [job nativo R3 de CI #38995714214](https://github.com/JW-Control/platform-jwplc/actions/runs/38995714214) verifica **7 mutantes de C++ real** rechazados y `TRACE_OVERRUN=PASS_REVIEW_DETECTED`; el contrato UART host rechaza **7 negativos** y el auditor de propietario/epoch **10 negativos** (exclusivamente sintéticos). **La compilación COM4/COM14 Windows+Ubuntu debe terminar en el HEAD exacto antes del PASS global.** Faltan fault injection físico DNS/DHCP/UDP y endurance. `A13-006=ADOPTION_BLOCKED`; ninguno de los firmwares de G10-R se ha subido.

```text
G10-R-R3-HISTORICAL_TRACE=SOURCE_BACKED_NATIVES_7_NEGATIVES
G10-R-R3-UART_SEMANTICS=HOST_7_NEGATIVES_AND_OWNER_EPOCH_10_NEGATIVES
G10-R-R3-PHYSICAL=NOT_RUN
G10-R-R3-FAULT_INJECTION=NOT_RUN
G10-R-R1_R2_R4=NOT_RUN_PHYSICALLY
PRODUCT_MODIFIED=NO
GPIO4_RESET=DISABLED
A13-006=ADOPTION_BLOCKED
NEXT_GATE=G10-R_FINAL_CI_AND_PRE_PHYSICAL_REVIEW
```


> **Actualización de evidencia R3 (16/10/2026):** nueva suite C++ host con **SafeAccess.cpp real compuesto** y exclusión SPI simulada no recursiva; el test verifica `BUS_BUSY`, ticket de cierre pendiente, drenaje de la cola y rechazo de un token obsoleto al reciclar socket. En [CI #38994618424](https://github.com/JW-Control/platform-jwplc/actions/runs/38994618424), el job R3 declaró `G10_R_R3_SOURCE_MUTANTS=4_PASS_REJECTED` junto con los dos casos positivos. **La compilación Arduino Windows/Ubuntu de esa corrida puede continuar en progreso: no se atribuye 5/5 PASS hasta verificarla.** Sigue sin haber fault injection físico DNS/DHCP/UDP ni telemetría histórica real de lifecycle. [Detalles del test y límites](A13_G10_R_PLAN_RECUPERACION_Y_RESISTENCIA_20261016.md). `A13-006=ADOPTION_BLOCKED`, PR #108 Draft, sin nuevas cargas ni modificación de producto.


> **Hito actual: seguridad del harness y observabilidad R3; hardware NO ejecutado.** Baseline del código anterior: [CI #38864224010](https://github.com/JW-Control/platform-jwplc/actions/runs/38864224010), **4/4 SUCCESS Windows/Ubuntu** para HEAD `e11e3638858e5063c432c67490feeb8f04764bae`. En la rama `codex/alpha13-g10-r-recovery-prep` se corrigió el parser de nonce HEX Arduino con mayúsculas y se exige concordancia **HEAD local+worktree limpio↔manifiesto de upload**. Los snapshots de ocho sockets se delimitan `G10R_SOCK_BEGIN/G10R_SOCK_END`, se guardan por sesión cada ~15 s y las capturas incompletas/BUS_BUSY quedan clasificadas explícitamente. Nuevo código y CI de Windows/Ubuntu en revisión: **NO atribuirles el PASS del baseline previo hasta verificar la corrida exacta**. R3 no tiene todavía secuencias físicas verificadas de fault injection `SPI_BUSY`. Código productivo y cinco archives intactos; GPIO4 deshabilitado. Los dos equipos aún contienen G10-H diagnóstico sin restauración. `A13-006=ADOPTION_BLOCKED`; PR #108 Draft; sin merge, upload, manipulación RJ45 ni release.

```text
HITO=G10-R
BRANCH=codex/alpha13-g10-r-recovery-prep
G10-R=PREP_NONPHYSICAL_SOURCE_INSTRUMENTED_CI_FINAL_PENDING
G10-R-BASE_CI=PASS_4X4_RUN_38864224010_HEAD_e11e3638
G10-R-N0=PASS_SYNTHETIC_CONTRACT_WINDOWS_UBUNTU
G10-R-R1_R2_R4=SKETCH_REPLAY_CAPTURE_AND_UPLOAD_GATES_IMPLEMENTED_PHYSICAL_NOT_RUN
G10-R-R3=HOST_CPP_SOURCE_BACKED_PASS_4_MUTANTS_PHYSICAL_FAULT_INJECTION_NOT_RUN
G10-R-UPLOAD=CONSENT_GATED_NOT_EXECUTED
G10-R-CAPTURE=CONSENT_GATED_NOT_EXECUTED
G10-R-R5=NOT_AUTHORIZED
G10-I=NONPHYSICAL_PASS_CANDIDATE
G10-H=PASS_PHYSICAL_SMOKE_MANIFEST_REVIEWED
G10-C=CLOSED_DECISION_DEFER_ADOPTION
A13-006=ADOPTION_BLOCKED
G10-FINAL=NOT_CLOSED_FOR_PRODUCT
PRODUCT_MODIFIED=NO
AUTOLOAD_NORMAL=UNCHANGED
FROZEN_ARCHIVES=5_UNCHANGED
GPIO4_RESET=DISABLED
OPERATOR_FIRMWARE_RESTORED=NO
NEXT_GATE=G10-R_FINAL_CI_REVIEW_AND_R3_FAULT_INJECTION_PLAN
PHYSICAL_UPLOAD=NOT_AUTHORIZED
PHYSICAL_LINK_FLAP=NOT_AUTHORIZED
PHYSICAL_POWER_CYCLE=NOT_AUTHORIZED
MERGE=NOT_AUTHORIZED
RELEASE=NOT_AUTHORIZED
OPENPLC=OUT_OF_SCOPE
HMI_DESIGNER=OUT_OF_SCOPE
TFT_NEW_FEATURES=OUT_OF_SCOPE
```

---


> **ESTADO VIGENTE DE G10 — 16/10/2026 (rama experimental G10-C).** Este encabezado corrige entradas cronológicas antiguas de este archivo; **NO equivale al estado de adopción de la rama productiva**. G10-I: candidato N1–N3 PASS en CI #38843599043, HEAD `a954cabd9c6e37876b59995a41ff0a449308ccd1`. G10-H: smoke físico COM4/COM14 **PASS** con manifiesto revisado, HEAD ejecutado `c776ab92388e8765e933bf50cab8215e7c18325b`, `47 TCP OK/0 FAIL`, `47 UDP OK/0 FAIL`, `49 RTU Master OK/0 FAIL`, `52 solicitudes Slave/0 CRC`, SD `56 OK/0 FAIL`, FRAM `56`, RTC válido, botón `6`. G10-C: **decisión documental de diferir adopción**, no nueva campaña técnica. No se han acreditado recuperación Ethernet/DNS/DHCP, estrés prolongado, reset/power-cycle, pruebas IDE finales ni integración productiva. `A13-006=ADOPTION_BLOCKED`, `G10-FINAL=NOT_CLOSED_FOR_PRODUCT`; [decisión y checklist](A13_G10_C_DECISION_Y_CIERRE_20261016.md). PR #105 y PR #106 siguen Draft, sin merge/PreRelease. Los dos PLC contienen firmware de diagnóstico y no fueron restaurados.

```text
HITO=G10-C
G10-I=NONPHYSICAL_PASS_CANDIDATE
G10-H=PASS_PHYSICAL_SMOKE_MANIFEST_REVIEWED
G10-C=CLOSED_DECISION_DEFER_ADOPTION
G10-FINAL=NOT_CLOSED_FOR_PRODUCT
A13-006=ADOPTION_BLOCKED
PRODUCT_MUTATION=NO
AUTOLOAD_NORMAL=UNCHANGED
FROZEN_ARCHIVES=5_UNCHANGED
GPIO4_RESET=DISABLED
OPERATOR_FIRMWARE_RESTORED=NO
NEXT_GATE=G10-R_PLAN_IF_PRODUCT_ADOPTION_IS_REQUESTED
MERGE=NOT_AUTHORIZED
RELEASE=NOT_AUTHORIZED
OPENPLC=OUT_OF_SCOPE
HMI_DESIGNER=OUT_OF_SCOPE
TFT_NEW_FEATURES=OUT_OF_SCOPE
```


> **G10-I B03 — CI guard agregado 2026-10-14:** script `tools/alpha13/gates/a13_g10_b03_source_contract.py` (commit `2f3b1807`) y paso en `.github/workflows/alpha13-g10-integrated.yml` (commit `a9858dc3`). Detecta en fuente real los patrones DNS finish/cancel y DHCP finish de stop seguido de cierre lógico, con dos mutaciones negativas. **La prueba valida el reconocimiento del riesgo, NO el arreglo**. CI posterior al commit todavía no comprobado desde este chat. `B03=OPEN`, `A13-006=ADOPTION_BLOCKED`, `G10-H=NOT_STARTED`.

> **Revisión de contrato B03 — 2026-10-14:** se identificaron rutas de abandono de `EthernetUDP::stop()` bajo `SPI_BUSY` en `DNSClient::finishResolveAsync/cancelResolveAsync` y `DhcpClass::finishAsync`. Contrato y matriz de aceptación versionados en [`A13_G10_B03_DEFERRED_CLOSE_CONTRACT_20261014.md`](A13_G10_B03_DEFERRED_CLOSE_CONTRACT_20261014.md), commit `eca13d17`. **NO se implementó aún el fix ni se ejecutó CI nueva; G10-I continúa REVIEW/BLOCKED, G10-H sin autorización, A13-006 ADOPTION_BLOCKED.**

> **Esta cabecera prevalece sobre entradas históricas** de G10
> situadas más abajo. El historial se conserva por trazabilidad;
> no usar sus antiguas líneas `NEXT_GATE=S2...` como instrucción activa.

```text
HITO=G10-I
ETAPA=FULL_PLATFORM_2X2_COMPILE_PASS_INTEGRATION_REVIEW_OPEN
BRANCH=v2.1.0-alpha.13/feature/cleanup-robustness
G10-I-PREP-CI=PASS_WINDOWS_LINUX_RUN_38453811488
G10-I-PREP-CI-HEAD=b5dcfeecd01a75a8b12cc78cfe28c01a8ccc23fb
G10-I-PREP-STATUS=COMPOSED_REVIEW_BLOCKED
G10-I-ARDUINO-LIBRARIES=PASS_BASELINE_2_CANDIDATE_2_RUN_38457826687
G10-I-ARDUINO-LIBRARIES-CI-HEAD=5a82cdd9a3b9315e2c619b65dc096fee2350b26f
G10-I-ARDUINO-TEST-FQBN=jwplc:esp32:jwplcbasic
G10-I-ARDUINO-TEST-CORE=BOOTSTRAP_2.1.0-alpha.10_WITH_BRANCH_HEADER_PATH
G10-I-SAFEACCESS_CPP=COMPILED_AS_CANDIDATE_OBJECT
G10-I-SOURCE_MMD_AND_ELF=PASS_2_CANDIDATE_SKETCHES_RUN_38484339095
G10-I-SOURCE_MMD_AND_ELF_CI_HEAD=1438fb883f83cddc1b839fb2961f5f9b4a335feb
G10-I-INTEGRATED_CPP_ARDUINO=BRANCH_FULL_PLATFORM_2_SKETCHES_PASS_AUTOLOAD_INCOMPLETE
G10-I-SOURCE_OVERLAY=GENERATED_TEMPORARY_ONLY
G10-I-FULL_PACKAGE_AND_IDE=ISOLATED_PLATFORM_2_SKETCHES_PASS_IDE_NOT_RUN
G10-I-PHYSICAL=NOT_RUN
G10-I-OPEN_BLOCKERS=8
G10-I-TRISTATE-PRIMITIVE=PASS_NATIVE_WINDOWS_LINUX_RUN_38485428889
G10-I-TRISTATE-CI-HEAD=456890bb3ace659841a59db4cabba57cf06e3f31
G10-I-TRISTATE-CONTRACT=READ_BUSY_STALE_NO_FALSE_CLOSED
G10-I-TRISTATE-WIRED_TO_CLIENT_OR_SOCKETSTATUS=PARTIAL_MODBUS_CLIENT_ONLY
G10-I-MODBUS-STATUS-PROBE=OVERLAY_ONLY_NO_PUBLIC_API_CHANGE
G10-I-MODBUS-CLOSE-RETRY=PASS_NATIVE_WINDOWS_LINUX_3_SOURCE_METHODS_3_NEGATIVES_RUN_38499744270
G10-I-MODBUS-CLOSE-RETRY-DETAIL=PRESERVES_HANDLE_WHEN_SPI_BUSY_BUSY_FLAG_RETRY_IN_TASK
G10-I-MODBUS-CLOSE-RETRY-LIMIT=REQUIRES_COOPERATIVE_TASK_CALL_AND_NO_LINK_DOWN_DROP
G10-I-ARDUINO-CANDIDATE-B01-B04=PASS_2_SKETCHES_RUN_38499744270
G10-I-ARDUINO-CANDIDATE-MMD-ELF=PASS_2_SKETCHES_RUN_38499744270
G10-I-NATIVE-REGRESSION=PASS_WINDOWS_LINUX_RUN_38499744270
G10-I-OVERLAY-COMPOSED=12_SOURCE_PATHS
G10-I-PENDING-BLOCKERS=8_STILL_OPEN
G10-I-FULL_PACKAGE_ALPHA13_BUILD=ISOLATED_BRANCH_PLATFORM_2_SKETCHES_PASS_RUN_38501699426
G10-I-ARDUINO-BASELINE-CANDIDATE=PASS_2X2_RUN_38485428889
G10-I-ARDUINO-LATEST=PASS_BASELINE_2_CANDIDATE_2_RUN_38499744270
G10-I-ARDUINO-OBJECT_AND_ELF=PASS_2_CANDIDATE_SKETCHES_RUN_38485428889
G10-I-ARDUINO-CORE_FULL_ALPHA13=ISOLATED_BRANCH_PLATFORM_PASS_NOT_DISTRIBUTED_INSTALL
G10-I-B04-MODBUS-CLIENT-CLOSESESSION=HANDLE_DROP_ON_SPI_BUSY_CONFIRMED_SOURCE
G10-I-B04-DEFERRED-CLOSE=OVERLAY_CLIENT_SERVER_CI_PASS_NOT_PRODUCT
G10-I-SERVER-CPP-METHODS=3_SOURCE_BACKED_TASK_DROP_RETRY
G10-I-SERVER-NEGATIVES=5_PASS_WINDOWS_LINUX_RUN_38501699426
G10-I-SERVER-CLOSE-SPI_BUSY=RETAIN_TOKEN_UNTIL_CLOSED_OR_STALE
G10-I-FULL-PLATFORM-DEV-FQBN=jwplc_local:esp32:jwplcbasic
G10-I-FULL-PLATFORM-BASELINE-CANDIDATE=PASS_2_SKETCHES_EACH_RUN_38501699426
G10-I-FULL-PLATFORM-SOURCE_PROVENANCE=PARTIAL_PLATFORM_PATH_AND_CANDIDATE_OBJECT_ONLY
G10-I-FULL-PLATFORM-CI-HEAD=2471ac18cac6aa25c401651decc9fc4725fa012b
G10-I-FULL-PLATFORM-JOBS=5_OF_5_PASS
G10-I-FULL-PLATFORM-CANDIDATE-OBJECT=UNIQUE_2_OF_2
G10-I-FULL-PLATFORM-MMD_ELF=PASS_2_CANDIDATE_SKETCHES_RUN_38508580631
G10-I-FULL-PLATFORM-LIBRARY_DISCOVERY=PASS_2_CANDIDATE_SKETCHES_RUN_38508580631
G10-I-CI_NODE24_UBUNTU24=PASS_5_OF_5_RUN_38508580631
G10-I-CI_ARDUINO_ACTION_NODE20=WARNING_RESIDUAL
G10-I-REPRESENTATIVE_PERIPHERAL_COMPILE=PASS_BASELINE_11_CANDIDATE_11_RUN_38510083279
G10-I-REPRESENTATIVE_CI_5_JOBS=PASS
G10-I-REPRESENTATIVE_MMD_ELF=PASS_11_CANDIDATE_SKETCHES
G10-I-PERIPHERALS_COVERED=DI_BUTTONS_RTC_FRAM_SD_RS485_MODBUS_RTU_DISPLAY_DHCP_MODBUS_TCP
G10-I-REPRESENTATIVE_MATRIX_LIMIT=COMPILE_ONLY_NOT_CONCURRENT_HARDWARE
G10-I-B03_UDP_DNS_DHCP_CLOSE_LIFETIME=OPEN_CONFIRMED_SOURCE
G10-I-B03_SOURCE_BACKED_NATIVE=RISK_REPRODUCED_WINDOWS_UBUNTU_4_METHODS_RUN_38511031959
G10-I-B03_CI_INTERPRETATION=REPRODUCTION_NOT_FIX
G10-I-ARDUINO-IDE_AND_PHYSICAL=NOT_RUN
G10-I-LOCAL_WINDOWS=NOT_REQUIRED_UNTIL_INTEGRATED_READY
G10-H=NOT_STARTED_REQUIRES_PHYSICAL_AUTHORIZATION
G10-C=NOT_STARTED
G10-P1G-C2-S2C2B1=LOCAL_WINDOWS_CLOSED_PASS
G10-P1G-C2-S2C2B1-RUN=g10_p1g_c2_s2c2b1_20261013_135948_9b7cb66e
A13-006=ADOPTION_BLOCKED
G10_FINAL=NOT_CLOSED
PRODUCT_CHANGED=NO_FOR_G10
PRECOMPILED_ARCHIVES=5_FROZEN
GPIO4_RESET=255_DISABLED
ALPHA13=IN_PROGRESS
NEXT_ACTION=G10-I_REVIEW_B03_UDP_DNS_DHCP_DEFERRED_CLOSE_AND_REMAINING_OWNERSHIP
```

**Decisión de flujo aprobada:** dejar de pedir una ejecución manual
por cada método; el siguiente trabajo es **G10-I Integración**,
luego **G10-H Hardware autorizado** y **G10-C Cierre**.
Las validaciones más recientes alcanzan 11 sketches por variante
con el core Alpha13 de la rama, instalado aisladamente como
`jwplc_local:esp32:jwplcbasic`, y comprobaciones fuente/objeto/ELF.
El bootstrap Alpha10 se usa **solo en otra prueba independiente**
de bibliotecas. El PASS no certifica ejecución física simultánea
de periféricos, Arduino IDE ni ausencia de carreras SPI.
[CI #38510083279](https://github.com/JW-Control/platform-jwplc/actions/runs/38510083279).

[Plan G10-I/H/C](A13_G10_INTEGRATED_MILESTONES_20261013.md).
Ejecutor único: `tools/alpha13/gates/run_a13_g10_integrated.bat`.
No ejecutar manualmente más microgates ni flashear hasta preparar
un test integrado reproducible y aprobado.

**Otros hitos Alpha13 (como TFT-CLOSURE) mantienen su estado
independiente**; no presuponer que su worktree local
sea limpio ni repetir cambios productivos no versionados.

---

## G10-P1G-C2-S2C2A local CLOSED_PASS y S2C2B1 UDP CI PASS (2026-10-13)

```text
G10_P1G_C2_S2C2A_WINDOWS_LOCAL=CLOSED_PASS_RUN_g10_p1g_c2_s2c2a_20261013_134702_6bdf6565
G10_P1G_C2_S2C2A_WINDOWS_HEAD=116dd26a953e8838e4d14ce376ae4d8ff063bd32
G10_P1G_C2_S2C2A_WINDOWS_METHODS=7
G10_P1G_C2_S2C2A_WINDOWS_NEGATIVES=5_PASS
G10_P1G_C2_S2C2A_WINDOWS_ARCHIVES=5_FROZEN
G10_P1G_C2_S2C2B1=REAL_UDP_BEGIN_STOP_ASYNC_SEND_SOURCE_OVERLAY
G10_P1G_C2_S2C2B1_CI=PASS_WINDOWS_LINUX_RUN_38450704316
G10_P1G_C2_S2C2B1_CI_HEAD=1ddab59452bcfa10fa7cb5f30c3453cae9608416
G10_P1G_C2_S2C2B1_SOURCE_CPP_METHODS=6
G10_P1G_C2_S2C2B1_OVERLAY_FILES=2
G10_P1G_C2_S2C2B1_NEGATIVES=5_PASS_REJECTED
G10_P1G_C2_S2C2B1_ARCHIVES=5_FROZEN
G10_P1G_C2_S2C2B1_WINDOWS_LOCAL=NOT_YET_EXECUTED
G10_P1G_C2_S2C2B1_UDP_PARSE_READ_WRITE=NOT_COVERED
G10_P1G_C2_S2C2B1_REAL_DNS=NOT_COVERED
G10_P1G_C2_S2C2B1_DHCP=NOT_COVERED
G10_P1G_C2_S2C2B1_ARDUINO_BUILD=NOT_RUN
G10_P1G_C2_S2C2B1_PRODUCT_FILES=UNCHANGED
G10_P1G_C2_S2C2B1_GPIO4_ENABLED=NO
G10_P1G_C2_S2C2B2=DNS_UDP_WRITE_PARSE_RECEIVE_NEXT
A13-006=ADOPTION_BLOCKED
G10_FINAL=NOT_CLOSED
ALPHA13=IN_PROGRESS
```

El operador cerró S2C2A localmente. S2C2B1 prueba seis cuerpos C++
reales de UDP en overlay con lease propietario, mutex anidable,
BUS_BUSY sin pérdida de handle, comienzo de SEND diferido y rechazo de
handles obsoletos; cinco negativos y CI Windows/Linux PASS. Los archivos
productivos y cinco precompilados permanecen sin cambios. No demuestra
DNS, parser, RX/TX completo ni fairness físico.

[Informe C2-S2C2B1](A13_G10_P1G_C2_S2C2B1_UDP_LIFECYCLE_SOURCE_GATE_20261013.md).

---

## G10-P1G-C2-S2C1 Windows CLOSED_PASS y S2C2A CI PASS (2026-10-13)

```text
G10_P1G_C2_S2C1_WINDOWS_LOCAL=CLOSED_PASS_RUN_g10_p1g_c2_s2c1_20261013_133216_bf7c8acb
G10_P1G_C2_S2C1_WINDOWS_HEAD=07e72ce9498208ed573dc80741b8fe6c5ef1ef57
G10_P1G_C2_S2C1_WINDOWS_METHODS=5
G10_P1G_C2_S2C1_WINDOWS_NEGATIVES=5_PASS
G10_P1G_C2_S2C1_WINDOWS_ARCHIVES=5_FROZEN
G10_P1G_C2_S2C2A=LEGACY_IP_AND_HOST_PREDNS_SOURCE_BACKED
G10_P1G_C2_S2C2A_CI=PASS_WINDOWS_LINUX_RUN_38448242437
G10_P1G_C2_S2C2A_CI_HEAD=ab7ee0ea6b0f2235c16b17cd87edf77c286c328d
G10_P1G_C2_S2C2A_REAL_CPP_METHODS=7
G10_P1G_C2_S2C2A_OVERLAY_FILES=3
G10_P1G_C2_S2C2A_NEGATIVES=5_PASS
G10_P1G_C2_S2C2A_FROZEN_ARCHIVES=5
G10_P1G_C2_S2C2A_WINDOWS_LOCAL=NOT_YET_EXECUTED
G10_P1G_C2_S2C2A_DNS_SOURCE_REAL=NOT_TESTED_MOCK_ONLY
G10_P1G_C2_S2C2A_TCP_IP_ALLOCATED_TIMEOUT_BUSY_CLEANUP=OPEN
G10_P1G_C2_S2C2A_PRODUCT_CHANGED=NO
G10_P1G_C2_S2C2A_ARDUINO_BUILD=NOT_RUN
G10_P1G_C2_S2C2A_GPIO4_ENABLED=NO
G10_P1G_C2_S2C2B=DNS_AND_UDP_SOURCE_PENDING
A13-006=ADOPTION_BLOCKED
G10_FINAL=NOT_CLOSED
ALPHA13=IN_PROGRESS
```

El operador cerró S2C1 en Windows. Se validó S2C2A en CI
Windows/Linux: siete métodos C++ reales compilados con
DNS **simulado**, y cinco negativos rechazados. El overlay
de `connect(host)` protege únicamente la liberación de
la sesión anterior y libera el mutex **antes** de DNS.
DNS/UDP real continúa sin garantía de mutex, así como el
cleanup automático de socket asignado con SPI_BUSY después
de timeout. Ninguna modificación del package productivo.

[Informe técnico y reproducción S2C2A](A13_G10_P1G_C2_S2C2A_LEGACY_CONNECT_PRE_DNS_20261013.md).

---

## G10-P1G-C2-S2C1 — cancelWriteAsync CI PASS Windows/Linux (2026-10-13)

```text
G10_P1G_C2_S2B2B_LOCAL=CLOSED_PASS_RUN_g10_p1g_c2_s2b2b_20261013_124846_9fdc28d7
G10_P1G_C2_S2B2B_LOCAL_HEAD=755a9b91b379685bd5404b2a448d9d59e32c888a
G10_P1G_C2_S2B2B_LOCAL_NEGATIVES=5_PASS
G10_P1G_C2_S2B2B_LOCAL_ARCHIVES=5_FROZEN
G10_P1G_C2_S2C1=TX_CANCEL_SOURCE_BACKED_WITH_OUTBOUND
G10_P1G_C2_S2C1_CI=PASS_WINDOWS_LINUX_RUN_38442717818
G10_P1G_C2_S2C1_CI_HEAD=7d21d2b3ff7013ba86db6846619988da0ec75c80
G10_P1G_C2_S2C1_SOURCE_CPP_METHODS=5
G10_P1G_C2_S2C1_OVERLAY_FILES=3
G10_P1G_C2_S2C1_NEGATIVES=5_PASS_REJECTED
G10_P1G_C2_S2C1_PRECOMPILED=5_FROZEN
G10_P1G_C2_S2C1_WINDOWS_LOCAL=NOT_YET_EXECUTED
G10_P1G_C2_S2C1_ARDUINO_BUILD=NOT_RUN
G10_P1G_C2_S2C1_PRODUCT_SOURCE=UNCHANGED
G10_P1G_C2_S2C1_PHYSICAL_GPIO4=NOT_ENABLED
G10_P1G_C2_S2C2=LEGACY_CONNECT_DNS_PENDING
G10_P1G_C2_S2C1_TX_POLL_READ_WRITE=NOT_INTEGRATED
A13-006=ADOPTION_BLOCKED
G10_FINAL=NOT_CLOSED
ALPHA13=IN_PROGRESS
```

S2B2B quedó cerrado localmente con prueba del operador.
S2C1 compila el método real `cancelWriteAsync()` junto
con cuatro métodos de outbound dentro de un overlay C++,
sin cambiar el producto; CI Win/Linux y cinco mutaciones
negativas PASS. La validación TX completa y el comportamiento
Arduino legacy `connect`/DNS aún necesitan gates.
La integración no es adoptable y GPIO4 continúa deshabilitado.

[Informe C2-S2C1](A13_G10_P1G_C2_S2C1_TX_CANCEL_SOURCE_GATE_20261013.md).

---

## G10-P1G-C2-S2B2B — Windows local CLOSED_PASS (2026-10-13)

```text
G10_P1G_C2_S2B2B_LOCAL=PASS_WINDOWS
G10_P1G_C2_S2B2B_LOCAL_HEAD=755a9b91b379685bd5404b2a448d9d59e32c888a
G10_P1G_C2_S2B2B_LOCAL_RUN=g10_p1g_c2_s2b2b_20261013_124846_9fdc28d7
G10_P1G_C2_S2B2B_LOCAL_POSITIVE=PASS_REAL_OUTBOUND_LIFECYCLE
G10_P1G_C2_S2B2B_LOCAL_NEGATIVES=5_PASS_REJECTED
G10_P1G_C2_S2B2B_LOCAL_METHODS=4
G10_P1G_C2_S2B2B_LOCAL_OVERLAY_FILES=3
G10_P1G_C2_S2B2B_LOCAL_ARCHIVES=5_FROZEN
G10_P1G_C2_S2B2B_LOCAL_PRODUCT_CHANGED=NO
G10_P1G_C2_S2B2B_CI=PASS_WINDOWS_LINUX_RUN_38440505069
G10_P1G_C2_S2B2B_CONCLUSION=CLOSED_PASS_NATIVE_SLICE_ONLY
G10_P1G_C2_S2C1=TX_CANCEL_OWNER_CLOSE_NEXT
G10_P1G_C2_S2C2=LEGACY_CONNECT_DNS_AND_PENDING_CLEANUP
A13-006=ADOPTION_BLOCKED
G10_FINAL=NOT_CLOSED
ALPHA13=IN_PROGRESS
```

El operador confirmó el gate S2B2B sobre `755a9b91`,
incluidos cinco negativos y cinco archives intactos.
La revisión de `EthernetClient.cpp` ratifica el cierre
directo sin token en `cancelWriteAsync()` cuando
`_writeAsyncState==2`, y el cierre previo a DNS
sin token en `connect(const char*)`. Para no mezclar
políticas, S2C se divide en S2C1 **cancelación TX** y
S2C2 **Arduino connect/DNS bloqueantes**. Todo bajo
overlays y CI; sin cambios productivos ni GPIO4.

---

## G10-P1G-C2-S2B2A local CLOSED_PASS y S2B2B CI PASS (2026-10-13)

```text
G10_P1G_C2_S2B2A_WINDOWS_LOCAL=CLOSED_PASS_RUN_g10_p1g_c2_s2b2a_20261013_121841_efdbfb69
G10_P1G_C2_S2B2A_WINDOWS_HEAD=e66021828971d82fa5b78e3319aaa44e977ef150
G10_P1G_C2_S2B2A_WINDOWS_CPP_METHODS=3
G10_P1G_C2_S2B2A_WINDOWS_NEGATIVES=4_PASS
G10_P1G_C2_S2B2A_WINDOWS_ARCHIVES=5_FROZEN
G10_P1G_C2_S2B2B=OUTBOUND_TCP_BEGIN_POLL_IN_PROGRESS_CANCEL_SOURCE_OVERLAY
G10_P1G_C2_S2B2B_CI=PASS_WINDOWS_LINUX_RUN_38440505069
G10_P1G_C2_S2B2B_CI_HEAD=da4fca4effa0580510ec6d102c9c05b4831c8b22
G10_P1G_C2_S2B2B_REAL_CPP_METHODS=4
G10_P1G_C2_S2B2B_OVERLAY_FILES=3
G10_P1G_C2_S2B2B_NEGATIVES=5_PASS_CI
G10_P1G_C2_S2B2B_ARCHIVES=5_FROZEN
G10_P1G_C2_S2B2B_WINDOWS_LOCAL=NOT_YET_EXECUTED
G10_P1G_C2_S2B2B_LEGACY_CONNECT_AND_DNS=NOT_COVERED
G10_P1G_C2_S2B2B_SEND_WRITE_CLOSE_INTEGRATION=NOT_COVERED
G10_P1G_C2_S2B2B_CONNECTED_SPI_BUSY_FINAL_POLICY=NOT_RESOLVED
G10_P1G_C2_S2B2B_ARDUINO_BUILD=NOT_RUN
G10_P1G_C2_S2B2B_PRODUCT_FILES=UNCHANGED
G10_P1G_C2_S2B2B_GPIO4_ENABLED=NO
G10_P1G_C2_S2C=LEGACY_CONNECT_AND_TCP_WRITE_NEXT
A13-006=ADOPTION_BLOCKED
G10_FINAL=NOT_CLOSED
ALPHA13=IN_PROGRESS
```

Se cierra S2B2A local por evidencia Windows del operador. Se implementó
S2B2B en overlay nativo: cuatro métodos de conexión TCP saliente,
cinco negativos, arranque diferido por SPI_BUSY, protección owner en
cancelación y revalidación de socket durante polling. CI definitiva
Windows y Linux PASS. No se compilaron librerías Arduino reales en esta
etapa ni se corrigieron aún los caminos legacy `connect(IPAddress)`,
`connect(host)`, DNS, `read/write`, UDP y DHCP. Sigue **bloqueado**.

[Informe S2B2B y límites](A13_G10_P1G_C2_S2B2B_OUTBOUND_TCP_SOURCE_GATE_20261013.md).

---

## G10-P1G-C2-S2B1 Windows PASS y C2-S2B2A nativo iniciado (2026-10-13)

```text
G10_P1G_C2_S2B1_WINDOWS_LOCAL=CLOSED_PASS_RUN_g10_p1g_c2_s2b1_20261013_120240_ccb0080a
G10_P1G_C2_S2B1_WINDOWS_HEAD=9d59ff4d1471cc563d59c078903367d60d59ae40
G10_P1G_C2_S2B1_WINDOWS_NEGATIVES=4_PASS
G10_P1G_C2_S2B1_WINDOWS_ARCHIVES=5_FROZEN
G10_P1G_C2_S2B2A=COOPERATIVE_STOP_REAL_CPP_SLICE
G10_P1G_C2_S2B2A_CI_RUN=38436363316\nG10_P1G_C2_S2B2A_CI_HEAD=7e23720e4c5201d7834f8df45c3228f41b1ac266
G10_P1G_C2_S2B2A_CI_LINUX=PASS
G10_P1G_C2_S2B2A_CI_WINDOWS=PASS
G10_P1G_C2_S2B2A_NATIVE_METHODS=3
G10_P1G_C2_S2B2A_NEGATIVES=4_REQUIRED
G10_P1G_C2_S2B2A_LOCAL_WINDOWS=NOT_YET_EXECUTED
G10_P1G_C2_S2B2A_PRODUCT_FILES=UNCHANGED
G10_P1G_C2_S2B2A_ARDUINO_BUILD=NOT_RUN
G10_P1G_C2_S2B2A_PHYSICAL=NOT_RUN
G10_P1G_C2_S2B2A_GPIO4_ENABLED=NO
G10_P1G_C2_S2B2A_LEGACY_STOP_BUSY_RETRY=REVIEW_OPEN
G10_P1G_C2_S2B2B=OUTBOUND_CONNECT_NOT_STARTED
A13-006=ADOPTION_BLOCKED
G10_FINAL=NOT_CLOSED
ALPHA13=IN_PROGRESS
```

El usuario validó S2B1 localmente. El nuevo S2B2A extrae tres
métodos reales `beginStopAsync/pollStopAsync/stop` desde un overlay,
valida owner y no confunde BUS_BUSY con cierre; el semántico de
`stop()` void tras timeout debe resolverse antes de adopción.
No se cambió el producto.

[Contrato y evidencia S2B2A](A13_G10_P1G_C2_S2B2A_COOPERATIVE_STOP_SOURCE_GATE_20261013.md).

---

## G10-P1G-C2-S2A local PASS y C2-S2B1 CI PASS (2026-10-13)

```text
G10_P1G_C2_S2A_WINDOWS_LOCAL=CLOSED_PASS_RUN_g10_p1g_c2_s2a_20261013_113859_460838be
G10_P1G_C2_S2A_WINDOWS_HEAD=d40b7ae25e6dc5e4b54034766abaa13b11bd7dba
G10_P1G_C2_S2A_WINDOWS_NEGATIVES=4_PASS
G10_P1G_C2_S2A_WINDOWS_ARCHIVES=5_FROZEN
G10_P1G_C2_S2B1=CLIENT_SERVER_HANDLE_TOKEN_AND_CANCELSTOP_SLICE
G10_P1G_C2_S2B1_CI=PASS_WINDOWS_LINUX_RUN_38433524896
G10_P1G_C2_S2B1_CI_HEAD=0acb927cfebe43460ae2a758c8002c17d1454a16
G10_P1G_C2_S2B1_SOURCE_CPP_METHODS=4
G10_P1G_C2_S2B1_HEADER_CONSTRUCTORS=1
G10_P1G_C2_S2B1_OVERLAY_FILES=3
G10_P1G_C2_S2B1_NEGATIVES=4_PASS
G10_P1G_C2_S2B1_FROZEN=5
G10_P1G_C2_S2B1_WINDOWS_LOCAL=NOT_YET_EXECUTED
G10_P1G_C2_S2B1_BEG_STOP_POLL_STOP_OUTBOUND=NOT_INTEGRATED
G10_P1G_C2_S2B1_UDP_DHCP_DNS=NOT_INTEGRATED
G10_P1G_C2_S2B1_RAW_CONNECTED_BUSY=NOT_RESOLVED
G10_P1G_C2_S2B1_PRODUCT_CHANGED=NO
G10_P1G_C2_S2B1_ARDUINO_COMPILE=NOT_RUN
G10_P1G_C2_S2B1_GPIO4_ENABLED=NO
G10_P1G_C2_S2B2=OUTBOUND_AND_COOPERATIVE_STOP_PENDING
A13-006=ADOPTION_BLOCKED
G10_FINAL=NOT_CLOSED
ALPHA13=IN_PROGRESS
```

El operador validó S2A localmente. S2B1 compila cuatro cuerpos C++
reales y un constructor extraído del header en overlay, prueba copia
de handle, invalidación después de reciclaje/reset, reintento tras
SPI_BUSY y cuatro negativos. El método `socketBegin` está mockeado
en este arnés, respaldado por gate S2A real separado. Este PASS
**NO** cubre TCP saliente/stop cooperativo/UDP ni Arduino completo;
no promover a firmware productivo.

[Informe y reproducción S2B1](A13_G10_P1G_C2_S2B1_CLIENT_SERVER_HANDLE_LEASE_20261013.md).

---

## G10-P1G-C2-S1 Windows PASS y C2-S2A CI PASS — 2026-10-13

```text
G10_P1G_C2_S1_LOCAL=CLOSED_PASS_RUN_g10_p1g_c2_s1_20261013_101730_2d1e241a
G10_P1G_C2_S1_LOCAL_HEAD=edbb0082f4277b1ab651da370328788ed7206d23
G10_P1G_C2_S1_LOCAL_NEGATIVES=4_PASS
G10_P1G_C2_S1_LOCAL_ARCHIVES=5_FROZEN
G10_P1G_C2_S2A=SOURCE_BACKED_SOCKET_BEGIN_LEASE_CANDIDATE
G10_P1G_C2_S2A_CI=PASS_WINDOWS_LINUX_RUN_38422090537
G10_P1G_C2_S2A_CI_HEAD=83f0c874ab339e26a5560056551d72fdaff6cd2c
G10_P1G_C2_S2A_NEGATIVES=4_PASS
G10_P1G_C2_S2A_WINDOWS_LOCAL=NOT_YET_RUN
G10_P1G_C2_S2A_PRODUCT_FILES=UNCHANGED
G10_P1G_C2_S2A_ARDUINO_BUILD=NOT_RUN
G10_P1G_C2_S2A_CLIENT_SERVER_UDP_LEASE=NOT_INTEGRATED
G10_P1G_C2_S2A_DEFERRED_STOP_CLOSE_GUARD=NOT_INTEGRATED
G10_P1G_C2_S2A_CONNECTED_SPI_BUSY_POLICY=UNRESOLVED
G10_P1G_C2_S2A_GPIO4_ENABLED=NO
G10_P1G_C2_S2B=CLIENT_SERVER_HANDLE_OWNERSHIP_PENDING
A13-006=ADOPTION_BLOCKED
G10_FINAL=NOT_CLOSED
ALPHA13=IN_PROGRESS
```

C2-S1 quedó cerrado por evidencia Windows local. C2-S2A prueba solo `socketBegin`
real en overlay con lease `{epoch,allocation,owner}`; NO conecta las
APIs Client/Server/UDP ni autoriza cambios de producto.

[Informe S2A](A13_G10_P1G_C2_S2A_SOCKET_ALLOCATION_SOURCE_GATE_20261013.md).

---

## G10-P1G-C2-S1 — primer overlay real CI Windows/Linux PASS (2026-10-13)

```text
G10_P1G_C1_WINDOWS_LOCAL=CLOSED_PASS_RUN_g10_p1g_c1_20261013_092038_8703b3da
G10_P1G_C1_WINDOWS_NATIVE_COMPILER=MSYS2_UCRT64_GXX
G10_P1G_C1_WINDOWS_NEGATIVES=4_PASS
G10_P1G_C2_S1=REAL_SOURCE_SPI_VERTICAL_SLICE
G10_P1G_C2_S1_CI=PASS_WINDOWS_LINUX_RUN_38414490706
G10_P1G_C2_S1_CI_HEAD=5c3da4b10c2f22d8ee9241b8f7503cc6b01d08f6
G10_P1G_C2_S1_OVERLAY_FILES=5
G10_P1G_C2_S1_SOURCE_METHODS_COMPILED=8
G10_P1G_C2_S1_MUTATION_NEGATIVES=4_PASS
G10_P1G_C2_S1_PRECOMPILED=5_FROZEN
G10_P1G_C2_S1_WINDOWS_LOCAL=NOT_YET_EXECUTED
G10_P1G_C2_S1_KNOWN_SEMANTIC_GAP=RAW_CONNECTED_BUSY_RETURNS_FALSE
G10_P1G_C2_S1_SOCKET_EPOCH_OWNER=NOT_INTEGRATED
G10_P1G_C2_S1_UDP_DHCP_DNS_OTHER_TCP=NOT_COVERED
G10_P1G_C2_S1_ARDUINO_BUILD=NOT_RUN
G10_P1G_C2_S1_FIRMWARE_PRODUCT=NOT_MODIFIED
G10_P1G_C2_S1_PHYSICAL_GPIO4=NOT_ENABLED
G10_P1G_C2_S1_ADOPTION=BLOCKED_INCOMPLETE
G10_P1G_C2_S2=NEXT_COVERAGE_SOCKET_LEASE_AND_BUSY_SEMANTICS
A13-006=ADOPTION_BLOCKED
G10_FINAL=NOT_CLOSED
ALPHA13=IN_PROGRESS
```

C1 está cerrado localmente y en CI. C2-S1 **no** es una
corrección completa: prueba de manera nativa ocho métodos
C++ reales extraídos de un overlay de cinco unidades,
con cuatro mutaciones rechazadas, sin tocar el package.
P1G-C2-S2 debe cerrar cobertura TCP/UDP y ownership
antes de gate Arduino o flasheo. Si BUS_BUSY se presenta,
`EthernetClient.connected()==0` no debe interpretarse
como demostración de link off ni cierre de socket.

[Informe C2-S1](A13_G10_P1G_C2_S1_REAL_SOURCE_SPI_SLICE_20261013.md).

---

## G10-P1G-C1 — Windows local nativo PASS (2026-10-13)

```text
G10_P1G_C1_WINDOWS_LOCAL=PASS_NATIVE_SOURCE_BACKED_ISOLATED
G10_P1G_C1_WINDOWS_HEAD=7750917312bfb46cf2b7a1c719abc1b7f8fa5fe4
G10_P1G_C1_WINDOWS_RUN=g10_p1g_c1_20261013_092038_8703b3da
G10_P1G_C1_WINDOWS_COMPILER=C:\\msys64\\ucrt64\\bin\\g++.exe
G10_P1G_C1_WINDOWS_POSITIVE=PASS_BUS_NESTING_SOCKET_LEASE_RESET
G10_P1G_C1_WINDOWS_NEGATIVES=4_PASS_REJECTED
G10_P1G_C1_WINDOWS_ARCHIVES=5_FROZEN
G10_P1G_C1_PREVIOUS_WINDOWS_FAIL=EXIT_1_EMPTY_STDERR_STDOUT
G10_P1G_C1_FAIL_ROOT_CAUSE=CONSISTENT_WITH_MSYS2_PATH_NOT_PROVEN
G10_P1G_C1_CI=PASS_WINDOWS_LINUX_RUN_38412055418
G10_P1G_C2=EXPERIMENTAL_REAL_METHOD_OVERLAY_IN_PROGRESS
G10_P1G_C1_PRODUCT_MODIFIED=NO
G10_P1G_C1_GPIO4_ENABLED=NO
A13-006=ADOPTION_BLOCKED
G10_FINAL=NOT_CLOSED
```

El operador confirmó compilación y ejecución C++ host con MSYS2 UCRT64,
cuatro mutaciones rechazadas y cinco archives intactos. El `compile.log`
de la ejecución fallida anterior solo contiene `EXIT=1` sin salida;
no atribuir causa exacta a DLL sin prueba adicional. El candidato C1
continúa fuera del package real. Se habilita trabajo de integración en
overlay P1G-C2, no la adopción productiva.

---

## G10-P1G-C1 — g++ UCRT64 encontrado, compilación Windows local detenida (2026-10-13)

```text
G10_P1G_C1_PYTHON_EXE=PASS_PYTHON_3_11_5
G10_P1G_C1_LOCAL_CXX_DETECTED=C:\\msys64\\ucrt64\\bin\\g++.exe
G10_P1G_C1_LOCAL_CXX_KIND=gcc
G10_P1G_C1_LOCAL_RUN=g10_p1g_c1_20261013_091528_3b490b65
G10_P1G_C1_LOCAL_POSITIVE_COMPILE=REVIEW_STOP_HOST_COMPILE_FAILED
G10_P1G_C1_LOCAL_EXACT_COMPILER_ERROR=PENDING_COMPILE_LOG
G10_P1G_C1_RUNNER_FIX=TEMPORARY_GCC_BIN_PATH_PLUS_DIAGNOSTICS
G10_P1G_C1_RUNNER_FIX_HEAD=ac499a6b8aa3eec86b23def1984da77fa0bc044c
G10_P1G_C1_RUNNER_FIX_CI=PASS_WINDOWS_LINUX_RUN_38412055418
G10_P1G_C1_LOCAL_RETRY=NOT_YET_EXECUTED
G10_P1G_C1_INTEGRATED_IN_PRODUCT=NO
G10_P1G_C2_PRODUCT_CHANGE=NONE
G10_P1G_C2_GPIO4_DEFAULT_CHANGE=NO
A13-006=ADOPTION_BLOCKED
G10_FINAL=NOT_CLOSED
```

El operador confirmó Python 3.11.5 con ejecutable `python.exe` y detección
C++ host `C:\\msys64\\ucrt64\\bin\\g++.exe`. La ejecución C1 dejó
`HOST_COMPILE_FAILED_positive_SEE_COMPILE_LOG` antes del test nativo.
**No se ha recibido el contenido de `positive/compile.log`; la causa exacta
sigue no determinada.** Se encontró una posible causa en la invocación:
GCC UCRT fuera de PATH. El gate añade exclusivamente al entorno
del proceso hijo el directorio del compilador tanto para compilar
como para ejecutar el binario, y expone exit/stderr en consola.
Esta corrección volvió a pasar CI Windows/Linux, pero no prueba
por sí sola la configuración MSYS2 concreta del operador.

El estado local se mantiene REVIEW_STOP hasta una nueva ejecución
o revisión del `compile.log`. No se modificaron libraries/core,
archives precompilados ni hardware.

---

## G10-P1G-C1 — diagnóstico Python Windows: comando sin extensión interceptado (2026-10-13)

```text
G10_P1G_C1_WINDOWS_PYTHON_COMMAND=C:\\Windows\\System32\\python
G10_P1G_C1_WINDOWS_REAL_PYTHON_EXE=C:\\Users\\jeykc\\AppData\\Local\\Programs\\Python\\Python311\\python.exe
G10_P1G_C1_WINDOWS_PYTHON_SHADOW=OBSERVED_OPEN_WITH_DIALOG
G10_P1G_C1_BAT_RUNNER=UPDATED_TO_PYTHON_DOT_EXE
G10_P1G_C1_LOCAL_COMPILER_PROBE=RETRY_WITH_EXPLICIT_PYTHON_EXE_PENDING
G10_P1G_C1_WINDOWS_NATIVE=NOT_YET_VERIFIED
G10_P1G_C1_CI_PRIOR=PASS_WINDOWS_LINUX_RUN_38349898868
G10_P1G_C2=DESIGN_CONTINUITY_ALLOWED_NOT_ADOPTED
G10_P1G_C2_PRODUCT_CHANGES=NONE
A13-006=ADOPTION_BLOCKED
```

Captura de PowerShell: `Get-Command python -All` resuelve primero
`C:\\Windows\\System32\\python` (sin extensión), seguido del
`python.exe` instalado en Python311 y del alias WindowsApps. Ejecutar
`python --version`, `python -c` o el `--compiler-probe` desde
`python` abre «¿Cómo quieres abrir este archivo?» y no produce
salida. Por tanto, el primer REVIEW_STOP `HOST_CXX_NOT_FOUND`
se conserva como ejecución original, pero no se da por válido
el diagnóstico posterior de descubrimiento hasta repetirlo usando
`python.exe` explícito. No eliminar el elemento de System32 sin
identificarlo; no hay evidencia de necesidad de reinstalar Python.

El BAT C1 invoca ahora `python.exe` (extensión explícita) para evitar
shadowing por `python` sin extensión. No se han alterado archivos
productivos, archivos .a, GPIO4 ni el firmware físico.

---

## G10-P1G-C1 — Windows local sin compilador host; CI revalidada (2026-10-13)

```text
G10_P1G_C1_WINDOWS_LOCAL_RUN=g10_p1g_c1_20261013_000509_97cf9df1
G10_P1G_C1_WINDOWS_LOCAL=REVIEW_STOP_ENV_HOST_CXX_NOT_FOUND
G10_P1G_C1_WINDOWS_LOCAL_CPP_COMPILE=NOT_RUN
G10_P1G_C1_LOCAL_PRODUCT_FIX=NOT_PROVEN
G10_P1G_C1_COMPILER_DISCOVERY=EXTENDED_MSVC_VSWHERE_VCVARS_LLVM_MSYS2_SCOOP
G10_P1G_C1_COMPILER_PROBE=AVAILABLE_AFTER_PULL
G10_P1G_C1_CI_REVALIDATED=PASS_WINDOWS_LINUX_RUN_38349898868
G10_P1G_C2_DESIGN_WORK=ALLOWED_CI_PROVENANCE
G10_P1G_C2_REAL_METHOD_INTEGRATION=NOT_STARTED
G10_P1G_C2_ADOPTION=BLOCKED
G10_P1G_C2_GPIO4_ENABLED=NO
G10_P1G_C2_PRODUCT_MUTATIONS=NONE
A13-006=ADOPTION_BLOCKED
G10_FINAL=NOT_CLOSED
```

El operador no tenía un compilador C++ nativo en el PATH de PowerShell,
por lo que C1 se detuvo antes de compilar; no es una regresión de firmware.
Se amplió la detección de Visual Studio/MSYS2/LLVM, sin descargar ni
instalar herramientas automáticamente. CI C1 pasó de nuevo Windows/Linux;
C2 puede avanzar en overlay, no puede declarar PASS local ficticio.

[Diagnóstico y continuidad](A13_G10_P1G_C1_WINDOWS_TOOLCHAIN_BLOCKER_20261013.md).

---

## G10-P1G-B Windows PASS y P1G-C1 nativo CI PASS (2026-10-13 UTC)

```text
G10_P1G_B_WINDOWS_LOCAL=CLOSED_PASS_RUN_g10_p1g_b_20261012_234850_ab1c26f8
G10_P1G_B_WINDOWS_HEAD=6ef3a7fa5b621affff20a3e3673a9b5d601ea71e
G10_P1G_B_WINDOWS_WITNESSES=19
G10_P1G_B_WINDOWS_NEGATIVES=5_PASS
G10_P1G_B_WINDOWS_ARCHIVES=5_FROZEN
G10_P1G_C1_CANDIDATE=TEMPORARY_CPP_BUS_GUARD_SOCKET_LEASE
G10_P1G_C1_NATIVE_CI=PASS_WINDOWS_LINUX_RUN_38349405593
G10_P1G_C1_CI_HEAD=4d2ff6d2617d6975c53635b6bdfbef304285fdac
G10_P1G_C1_CPP_TESTS=PASS_POSITIVE_AND_4_NEGATIVES
G10_P1G_C1_WINDOWS_LOCAL=NOT_YET_EXECUTED
G10_P1G_C1_INTEGRATION_CLIENT_SERVER_UDP=NOT_DONE
G10_P1G_C1_ARDUINO_BASELINE_CANDIDATE=NOT_RUN
G10_P1G_C1_PHYSICAL=NOT_RUN
G10_P1G_C1_GPIO4_DEFAULT_CHANGE=NO
G10_P1G_C1_FIRMWARE_PRODUCT_CHANGE=NONE
G10_P1G_C2=REAL_SOURCE_OVERLAY_INTEGRATION_PENDING
A13-006=ADOPTION_BLOCKED
G10_FINAL=NOT_CLOSED
ALPHA13=IN_PROGRESS
```

Se cerró P1G-B local. C1 compila un prototipo **real C++** aislado
con mutex W5500 anidable por task sobre el mutex SPI de core
(no modificado) y leases de socket {epoch, allocation, owner}.
El callback de reset está excluido bajo el SPI mutex completo;
su duración/fairness física está SIN VALIDAR. No se conectó
a Client/Server/UDP ni al autoload. No atribuir PASS como
corrección productiva.

[Resultado, riesgos y plan C2](A13_G10_P1G_C1_NATIVE_PROTOTYPE_RESULT_AND_C2_PLAN_20261013.md).

---

## G10-P1G-A local PASS y G10-P1G-B CI PASS (2026-10-12)

```text
G10_P1G_A_WINDOWS_LOCAL=CLOSED_PASS_RUN_g10_p1g_a_20261012_231402_0953c0ca
G10_P1G_A_WINDOWS_HEAD=e53b0e4d93ca9cd921a313d5e37b246d02856d4c
G10_P1G_A_LOCAL_NEGATIVES=PASS_4
G10_P1G_A_LOCAL_MATRIX_METHODS=257
G10_P1G_A_LOCAL_ARCHIVES=FROZEN_5
G10_P1G_A_CI=PASS_WINDOWS_LINUX_RUN_38339370374
G10_P1G_B_CI=PASS_WINDOWS_LINUX_RUN_38346137383
G10_P1G_B_CI_HEAD=92549d497c4460871815a3d9c87c4bcf081cb89e
G10_P1G_B_SOURCE_WITNESSES=19
G10_P1G_B_NEGATIVES=5_PASS_CI
G10_P1G_B_KNOWN_HAZARDS=5
G10_P1G_B_WINDOWS_LOCAL=NOT_YET_EXECUTED
G10_P1G_C=EXPERIMENTAL_CANDIDATE_DESIGN_NEXT
G10_P1G_PRODUCT_SOURCE_MODIFICATIONS=NONE
G10_P1G_GPIO4_ACTIVATION=NO
A13-006=ADOPTION_BLOCKED
G10_FINAL=NOT_CLOSED
```

Se confirmó P1G-A local y se creó P1G-B source-backed con 19 rutas y
cinco negativos. Las dos primeras CI de P1G-B fallaron por mutaciones
parciales de test, corregidas con PASS final Windows/Linux en
`38346137383`. Ninguna de esas fallas es defecto de producto.

[Informe y próximo contrato](A13_G10_P1G_B_SOURCE_WITNESSES_AND_CANDIDATE_DECISION_20261012.md).

---

## G10-P1G-A — CI Windows/Linux PASS, validación local pendiente (2026-10-12)

```text
G10_P1G_A_CI=PASS_WINDOWS_LINUX_RUN_38339370374
G10_P1G_A_CI_HEAD=355572d39f6001c82b7044ed4dcf0daee1894f7e
G10_P1G_A_BASELINE=CALLPATH_BASELINE_CAPTURED_ADOPTION_BLOCKED
G10_P1G_A_SOURCE_METHODS=257_ACROSS_8_CPP
G10_P1G_A_LOCAL_WRAPPED_METHODS=19
G10_P1G_A_LOWER_DIRECT_WITHOUT_LOCAL_LOCK=81_NOT_BUG_COUNT
G10_P1G_A_NEGATIVES=4_CI_PASS
G10_P1G_A_FROZEN_ARCHIVES=5
G10_P1G_A_WINDOWS_LOCAL=NOT_YET_EXECUTED
G10_P1G_B=CALL_GRAPH_AND_CANDIDATE_OPEN
G10_P1G_B_PRODUCT_PATCH=NOT_STARTED
G10_P1G_B_GPIO4_ENABLE=NO
G10_P1G_B_HARDWARE_UPLOAD=NO
A13-006=ADOPTION_BLOCKED_PENDING_EPOCH_OWNER_INTERLOCK
G10_FINAL=NOT_CLOSED
```

G10-P1G-A genera una matriz CSV de primeros accesos y verifica cuatro
negativos. No constituye un call graph semántico exhaustivo ni aceptación
de un fix. No se alteró producto ni hardware. P1G-B debe demostrar
exclusión real bajo mutex y protección de propiedad/generación de sockets
sin romper el autoload, Display, SD ni FRAM.

[Gate y próximo contrato](A13_G10_P1G_A_SOURCE_MATRIX_GATE_AND_NEXT_20261012.md).

---

## G10-P1G — inventario de mutex Ethernet y pin RST GPIO4 (2026-10-12)

```text
G10_P1G_SOURCE_MUTEX_INVENTORY=CAPTURED_READONLY
G10_P1G_HIGH_LEVEL_LOCAL_GUARDS=19
G10_P1G_DIRECT_BACKEND_METHODS_NO_LOCAL_JWPLC_MUTEX=81
G10_P1G_GENUINELY_UNPROTECTED_CALLGRAPH=REVIEW_OPEN_NOT_81_CONFIRMED_BUGS
G10_P1G_RST_ETH_HARDWARE_GPIO=4
G10_P1G_RESET_DEFAULT_FW_PIN=255
G10_P1G_GPIO4_PULSE_IN_NORMAL_AUTOLOAD=NOT_CONFIGURED_IN_REVIEWED_SOURCES
G10_P1G_RESET_PIN_DEFAULT_CHANGE=BLOCKED_PENDING_INTERLOCK_AND_HARDWARE_GATE
G10_P1G_CAUSE_ALPHA12_LATENCY_UNPROVEN=YES
G10_P1G_FIRMWARE_MODIFICATIONS=NONE
G10_FINAL=OPEN
```

Se verificó el esquema suministrado: RST-ETH->GPIO4. La librería
`JWPLC_Ethernet` conserva por defecto `JWPLC_ETH_RESET_PIN=255`
y boards.txt no la sobrescribe. La capa runtime/Modbus sí posee protección
explícita en 19 métodos operativos; las APIs directas de Client/Server/UDP
utilizan el contrato de protección del llamador. El recuento estático de
81 métodos sin lock **local** NO representa 81 carreras demostradas.

Inventario y reglas del recuento:
[A13_G10_P1G_ETHERNET_SPI_PROTECTION_INVENTORY_20261012.md](A13_G10_P1G_ETHERNET_SPI_PROTECTION_INVENTORY_20261012.md).

---

## Alpha13 G10 — P1F Windows cerrado, P1G contrato seguro abierto (2026-10-12)

```text
ALPHA13=IN_PROGRESS
BRANCH=v2.1.0-alpha.13/feature/cleanup-robustness
G10_P1F_WINDOWS_HEAD=f5d8d32369ecbc64163e65ed3e52e0a4f88733f6
G10_P1F_WINDOWS_RUN=g10_p1f_20261012_162335_c18278de
G10_P1F_WINDOWS=PASS_READONLY_12_PATTERNS_5_FROZEN_ARCHIVES
G10_P1F_CI=PASS_WINDOWS_LINUX_RUN_38299316198
G10_P1F_SELFTEST_NEGATIVES=2_IN_CI_NOT_SEPARATELY_REPORTED_LOCAL
G10_P1F_STATUS=SOCKET_EPOCH_BASELINE_CAPTURED_ADOPTION_BLOCKED
G10_P1F_PRODUCT_FILES_MODIFIED=NONE
G10_P1G=DESIGN_SOURCE_REVIEW_OPEN_CANDIDATE_NOT_EXECUTED
G10_P1G_REQUIREMENTS=EPOCH_OWNER_LISTENER_RESET_INTERLOCK
G10_P1G_ADDITIONAL_RISK=RAW_ETHERNET_SPI_TRANSACTIONS_WITHOUT_EXPLICIT_JWPLC_MUTEX
G10_P1G_DOUBLE_LOCK_RISK=JWPLC_FREERTOS_MUTEX_NON_RECURSIVE
A13-006=REVISION_REQUIRED_ADOPTION_BLOCKED
G10_FINAL=NOT_CLOSED
F120_POST_UPLOAD_FREEZE=UNKNOWN_PENDING
COM4_COM14_DIAGNOSTICS=NOT_RESTORED
RELEASE_PR_MERGE_PUBLICATION=NO
```

El operador ejecutó P1F después de fast-forward 37366d10 -> f5d8d323,
con 12 patrones confirmados y cinco archives intactos. Se confirma riesgo de
reset GPIO antes del mutex SPI, estado isReady() que no prueba generación,
y tabla estática EthernetServer::server_port[] sin ownership/epoch.
La CI P1F pasó Windows/Linux; NO se ha probado ningún fix ni hardware.

P1G comienza con una revisión de diseño y después exige candidato temporal
y pruebas C++ nativas sobre métodos reales; no basta un contador epoch ni
un modelo abstracto. Las comprobaciones de identidad y stop/cancel deben
ser coherentes con la exclusión de reset. Decidir A (quiescencia W5500)
o B (mutex SPI durante ~90 ms) solo tras pruebas de cobertura y de fairness.
La adopción A13-006 continúa bloqueada.

Plan y cierre documentados en
[A13_G10_P1F_WINDOWS_CLOSURE_AND_P1G_EXECUTION_CONTRACT_20261012.md](A13_G10_P1F_WINDOWS_CLOSURE_AND_P1G_EXECUTION_CONTRACT_20261012.md).

---

## Alpha13 G10 — P1E Windows PASS, P1F inventory CI PASS (2026-10-12)

```text
ALPHA13_STATUS=IN_PROGRESS
G9_FINAL=CLOSED_PASS_WITH_EXPLICIT_COVERAGE_LIMITS
G10_P1C_ARDUINO_BUILDS=PASS_4_OF_4_BASELINE_CANDIDATE
G10_P1D_LOCAL=PASS_SOURCE_CONTRACT_ADOPTION_BLOCKED
G10_P1E_LOCAL_RUN=g10_p1e_20261012_142935_e37850cd
G10_P1E_LOCAL_HEAD=37366d10858864e6cdee9e9467191b858a939ee3
G10_P1E_LOCAL_STATUS=RESET_GENERATION_BASELINE_CAPTURED_ADOPTION_BLOCKED
G10_P1E_RUNTIME_RESET_BEFORE_BUS_LOCK=True
G10_P1E_W5100_INIT_STATIC_GUARD=True
G10_P1E_ISREADY_IS_BOOLEAN=True
G10_P1E_ARCHIVES_FROZEN=5
G10_P1F_CONTRACT_CI=PASS_WINDOWS_LINUX_RUN_38299316198
G10_P1F_CI_ASSERTIONS=12_SOURCE_PATTERNS_2_NEGATIVES
G10_P1F_CI=SOCKET_EPOCH_BASELINE_CAPTURED_ADOPTION_BLOCKED
G10_P1F_LOCAL_WINDOWS=NOT_YET_EXECUTED
G10_P1F_NEXT_GATE=tools/alpha13/gates/run_a13_g10_p1f_socket_epoch_baseline.bat
G10_P1F_DESIGN=EPOCH_RESET_LISTENER_AND_SPI_QUIESCENCE_NOT_YET_ADOPTED
G10_P1F_PRODUCT_CHANGE=NONE
G10_P1F_HARDWARE_ACCESS=NONE
A13-006=ADOPT_CONFIRMED_FIX_REVISION_REQUIRED
A13-006_P1B_CANDIDATE=ADOPTION_BLOCKED
A13-007=REVIEW_OPEN
A13-008=REVIEW_OPEN
A13-015=ADOPT_PENDING
A13-016=ADOPT_PENDING
A13-018=ADOPT_PENDING
G10_FINAL=NOT_CLOSED
F120_POST_UPLOAD_FREEZE_ROOT_CAUSE=UNKNOWN_PENDING
COM4_FIRMWARE=G9_DIAGNOSTIC_NOT_RESTORED
COM14_FIRMWARE=G8_DIAGNOSTIC_NOT_RESTORED
RELEASE_PR_MERGE_PUBLICATION=NO
```

El operador completó P1E en el JWPLC workspace,
`RESET_GENERATION_BASELINE_CAPTURED_ADOPTION_BLOCKED`,
con diez patrones confirmados y hashes de cinco
archives. G10-P1F amplió la inspección del código
para incluir reset GPIO **anterior** a mutex SPI y
`EthernetServer::server_port[]`, que mapea índice
de socket a puerto sin generación de reinicio.
La CI [38299316198](https://github.com/JW-Control/platform-jwplc/actions/runs/38299316198)
PASÓ Windows y Linux, doce patrones y dos
pruebas negativas, **sin modificar producto**.

Se elaboró una propuesta de arquitectura:
[P1F_SOCKET_GENERATION_DESIGN_AND_GATES](A13_G10_P1F_SOCKET_GENERATION_DESIGN_AND_GATES_20261012.md).
No afirmar que un epoch simple resuelva el problema:
se requiere exclusión de reset/operaciones,
protección del listener, y comprobar
otras conexiones/dueños. Candidato P1B no se
adopta; P1G queda como diseño/experimento
posterior con pruebas nativas negativas.

---

## Alpha13 G10 — P1D local PASS; P1E baseline generaciones CI PASS (2026-10-12)

```text
ALPHA13_STATUS=IN_PROGRESS
G9_FINAL=CLOSED_PASS_WITH_EXPLICIT_COVERAGE_LIMITS
G10_P1C_WINDOWS_RUN=g10_p1c_20261012_140152_a7211c08
G10_P1C=PASS_FOUR_ARDUINO_BASELINE_CANDIDATE_BUILDS
G10_P1D_WINDOWS_RUN=g10_p1d_20261012_141926_035d46be
G10_P1D_LOCAL=SOURCE_CONTRACT_PASS_NATIVE_NOT_RUN_ADOPTION_BLOCKED
G10_P1D_NATIVE_CI=PASS_WINDOWS_LINUX_RUN_38296384365
G10_P1D_REUSE_RISK=REPRODUCED_IN_NATIVE_MOCK_NOT_PHYSICAL
A13-006=ADOPT_CONFIRMED_FIX_REVISION_REQUIRED
G10_P1E_CI=PASS_WINDOWS_LINUX_RUN_38297416611
G10_P1E_READONLY_SOURCE_CONTRACT=TEN_PATTERNS_AND_NEGATIVE_PASS
G10_P1E_PHYSICAL_RESET_OPTIONAL=TRUE
G10_P1E_W5100_INIT_HAS_STATIC_GUARD=TRUE
G10_P1E_ISREADY_ONLY_BOOL=TRUE
G10_P1E_LINK_OFF_CAN_SET_NOT_READY=TRUE
G10_P1E_RUNTIME_GENERATION_EXPOSED=NO
G10_P1E_FROZEN_ARCHIVES=5
G10_P1E_LOCAL_WINDOWS=NOT_YET_EXECUTED
G10_P1E_NEXT_GATE=tools/alpha13/gates/run_a13_g10_p1e_ethernet_generation_readonly.bat
G10_P1E_PRODUCT_CHANGE=NONE
G10_P1E_HARDWARE_UPLOAD=NONE
A13-007=REVIEW_OPEN
A13-008=REVIEW_OPEN
A13-015=ADOPT_PENDING
A13-016=ADOPT_PENDING
A13-018=ADOPT_PENDING
G10_FINAL=NOT_CLOSED
F120_POST_UPLOAD_FREEZE_ROOT_CAUSE=UNKNOWN_PENDING
COM4_FIRMWARE=G9_DIAGNOSTIC_NOT_RESTORED
COM14_FIRMWARE=G8_DIAGNOSTIC_NOT_RESTORED
RELEASE_PR_MERGE_PUBLICATION=NO
```

El operador confirmó P1D local PASS, que
solo comprueba procedencia del candidato con
seis funciones. La CI nativa P1D ya había
reproducido bajo mocks el cierre erróneo de
un índice de socket perteneciente a otra
conexión tras reset simulado. Se mantiene
A13-006 **BLOQUEADO para adopción**.

El inventario de fuente P1E concluyó
[CI PASS Win/Linux 38297416611](https://github.com/JW-Control/platform-jwplc/actions/runs/38297416611).
Se verificaron 10 patrones de fuente y negativos:
`JWPLC_Ethernet.isReady()` únicamente expone
`_ready` y puede estar falso por caída de enlace,
`probeHardware()` utiliza reset físico condicionado
por pin, y `W5100.init()` tiene una guarda `initialized`.
Por tanto ni `isReady()` ni contar solo init() sirven
como generación única de socket. Un cambio de
producto requerirá contrato de generación y
pruebas de dos dueños + reset/link + contención,
sin romper las APIs.

Siguiente paso: P1E en Windows operador, solo lectura,
con manifiestos P1C/P1D locales. Plan:
[A13_G10_P1D_LOCAL_CLOSURE_AND_P1E_DESIGN_20261012.md](A13_G10_P1D_LOCAL_CLOSURE_AND_P1E_DESIGN_20261012.md).

---

## Alpha13 G10 — P1C 4/4 PASS; P1D nativa revela riesgo reset/reuso (2026-10-12)

```text
ALPHA13_STATUS=IN_PROGRESS
G9_FINAL=CLOSED_PASS_WITH_EXPLICIT_COVERAGE_LIMITS
G10_P1B_LOCAL_CANDIDATE_RUN=g10_p1b_20261012_124640_8458554e
G10_P1C_LOCAL_BUILD_RUN=g10_p1c_20261012_140152_a7211c08
G10_P1C_HEAD_AT_TEST=802c6260
G10_P1C_BASELINE_SERVER=PASS_COMPILE_AND_GCC_DEPENDENCY
G10_P1C_BASELINE_CLIENT=PASS_COMPILE_AND_GCC_DEPENDENCY
G10_P1C_CANDIDATE_SERVER=PASS_COMPILE_AND_GCC_DEPENDENCY
G10_P1C_CANDIDATE_CLIENT=PASS_COMPILE_AND_GCC_DEPENDENCY
G10_P1C_FINAL_AUDIT=PASS
G10_P1C_STATUS=ARDUINO_BASELINE_AND_CANDIDATE_BUILDS_PASS_PRODUCT_UNCHANGED
F129=RESOLVED_CI_AND_LOCAL_RETEST
G10_P1D_CI=PASS_WINDOWS_LINUX_RUN_38296384365
G10_P1D_NATIVE=SOURCE_BACKED_SIX_FUNCTIONS_LIFECYCLE_RESET_REUSE_HAZARD_REPRO
G10_P1D_MEANING=ADOPTION_BLOCKED_NOT_FIX_PASS
G10_P1D_WINDOWS_LOCAL=NOT_YET_RUN
G10_P1D_LOCAL_GATE=tools/alpha13/gates/run_a13_g10_p1d_a13_006_lifecycle.bat
A13-006=ADOPT_CONFIRMED_FIX_REVISION_REQUIRED
A13-006_CURRENT_P1B_CANDIDATE=REJECT_FOR_ADOPTION_UNTIL_ETHERNET_GENERATION_PROTECTION
G10_PRODUCT_FILES_CHANGED=NONE
G10_HARDWARE_UPLOAD=NONE
A13-007=REVIEW_OPEN
A13-008=REVIEW_OPEN
A13-015=ADOPT_PENDING
A13-016=ADOPT_PENDING
A13-018=ADOPT_PENDING
G10_FINAL=NOT_CLOSED
F120_POST_UPLOAD_FREEZE_ROOT_CAUSE=UNKNOWN_PENDING
COM4_FIRMWARE=G9_DIAGNOSTIC_NOT_RESTORED
COM14_FIRMWARE=G8_DIAGNOSTIC_NOT_RESTORED
RELEASE_PR_MERGE_PUBLICATION=NO
```

El operador confirmó las cuatro compilaciones reales
P1C con selección correcta de librerías, source
objects/dependencias y hashes; el gate final fue PASS,
sin cambio de producto.

Al extender las pruebas negativas a seis cuerpos
reales C++ candidatos, la CI P1D
[38296384365](https://github.com/JW-Control/platform-jwplc/actions/runs/38296384365)
pasó Windows y Linux, **reproduciendo un riesgo de
cerrar un socket de otro propietario** bajo un
escenario simulado de reset W5500 y reutilización
de índice. Es una alerta del diseño del candidato,
no de un fallo observado en campo ni de la versión
productiva original. `CI_PASS` significa
que la detección del riesgo funcionó, **no que la
corrección A13-006 esté lista**.

Decisión: bloquear adopción actual y revisar
identidad/generación de sockets del runtime Ethernet.
No modificar P1B productivo, ni empezar DHCP/UDP
hasta conciliar esta puerta. Se conserva
A13-006 `ADOPT_CONFIRMED_FIX_REVISION_REQUIRED`.

Evidencias: [cierre P1C](A13_G10_P1C_ARDUINO_OVERLAY_CLOSURE_20261012.md),
[plan y resultados negativos P1D](A13_G10_P1D_NEGATIVE_TCP_LIFECYCLE_PLAN_20261012.md).

---

## Alpha13 G10 — P1C Windows cuatro builds PASS; P1D ciclo de vida pendiente (2026-10-12)

```text
ALPHA13_STATUS=IN_PROGRESS
G9_FINAL=CLOSED_PASS_WITH_EXPLICIT_COVERAGE_LIMITS
G10_P0_WINDOWS=BASELINE_CAPTURED_REVIEW_FINDINGS_OPEN
G10_P1A_WINDOWS=SOURCE_CONTRACT_PASS_NATIVE_NOT_RUN
G10_P1B_WINDOWS_RUN=g10_p1b_20261012_124640_8458554e
G10_P1B_WINDOWS=PASS_FOUR_PRIVATE_CANDIDATES
G10_P1C_SUCCESSFUL_WINDOWS_RUN=g10_p1c_20261012_140152_a7211c08
G10_P1C_SUCCESSFUL_HEAD=802c6260
G10_P1C_BASELINE_SERVER=PASS_COMPILATION_SOURCE_PROVENANCE
G10_P1C_BASELINE_CLIENT=PASS_COMPILATION_SOURCE_PROVENANCE
G10_P1C_CANDIDATE_SERVER=PASS_COMPILATION_SOURCE_PROVENANCE
G10_P1C_CANDIDATE_CLIENT=PASS_COMPILATION_SOURCE_PROVENANCE
G10_P1C_FINAL_AUDIT=PASS
G10_P1C_STATUS=ARDUINO_BASELINE_AND_CANDIDATE_BUILDS_PASS_PRODUCT_UNCHANGED
F129=RESOLVED_CI_AND_WINDOWS_LOCAL_RETEST
G10_P1D=NOT_YET_RUN
G10_P1D_EXPECTED=NEGATIVE_STATE_MACHINE_AND_RESET_REUSE_TESTS
A13-006=ADOPT_CONFIRMED_CANDIDATE_BUILD_PASS_NOT_ADOPTED
G10_PRODUCT_CHANGE=NONE
G10_HARDWARE_UPLOAD=NONE
A13-007=REVIEW_OPEN
A13-008=REVIEW_OPEN
A13-015=ADOPT_PENDING
A13-016=ADOPT_PENDING
A13-018=ADOPT_PENDING
G10_FINAL=NOT_CLOSED
F120_POST_UPLOAD_FREEZE_ROOT_CAUSE=UNKNOWN_PENDING
RELEASE_PR_MERGE_PUBLICATION=NO
```

El operador confirmó P1C PASS en Windows con las
cuatro compilaciones (baseline Client/Server y
candidato Client/Server), procedencia `.cpp.o/.d`
independiente para cada uno, cinco archives inmutados
y `FINAL_AUDIT_PASS`. El primer intento corregido
se interrumpió después de los dos baseline por
`History restored`; la segunda ejecución completó
todo y es la **evidencia oficial**.

**No autorizar adopción A13-006 todavía.** P1B cubre
cierres con mocks de `closeSession()/dropClient()`,
pero no caminos reales `task(),begin(),end()` ni
el comportamiento bajo reset W5500 y reutilización
de índices. Preparar P1D como gate de estados y
escenarios negativos, manteniendo candidato y
firmware productivo intactos. El cierre P1C se
registra en
[A13_G10_P1C_ARDUINO_OVERLAY_CLOSURE_20261012.md](A13_G10_P1C_ARDUINO_OVERLAY_CLOSURE_20261012.md).

---

## Alpha13 G10 — F129 corregido y CI PASS, P1C retest Windows pendiente (2026-10-12)

```text
ALPHA13_STATUS=IN_PROGRESS
G9_FINAL=CLOSED_PASS_WITH_EXPLICIT_COVERAGE_LIMITS
G10_P1B_WINDOWS=CANDIDATE_SOURCE_CONTRACT_PASS_NATIVE_NOT_RUN
G10_P1C_FIRST_WINDOWS_RUN=g10_p1c_20261012_134431_10e91e4c
G10_P1C_FIRST_BASELINE_SERVER_COMPILE_EXIT=0
G10_P1C_FIRST_STATUS=REVIEW_STOP_HARNESS_F129
G10_P1C_FIRST_REMAINING_THREE_COMPILES=NOT_RUN
F129_ROOT_CAUSE=VERBOSE_LOG_PATH_STRING_NOT_SOURCE_COMPILATION_PROOF
F129_FIX=COMPILER_MMD_DEPENDENCY_CPP_D_OR_CPP_O_D_PLUS_OBJECT_SHA
F129_CI_FINAL=PASS_WINDOWS_LINUX_RUN_38293904677
G10_P1C_NEXT_GATE=tools/alpha13/gates/run_a13_g10_p1c_a13_006_build.bat
G10_P1C_LOCAL_RETEST=AWAITING_OPERATOR
G10_P1C_PRODUCT_MODIFICATION=NONE
G10_P1C_HARDWARE_ACCESS=NONE
A13-006=ADOPT_CONFIRMED_REPRO_FIX_PENDING
A13-007=REVIEW_OPEN
A13-008=REVIEW_OPEN
G10_FINAL=NOT_CLOSED
F120_POST_UPLOAD_FREEZE_ROOT_CAUSE=UNKNOWN_PENDING
RELEASE_PR_MERGE_PUBLICATION=NO
```

El operador obtuvo `COMPILE_BASELINE_SERVER_EXIT=0`;
el gate falló al comprobar formato de ruta C++ en el
log verbose (no por fallo del compilador). El contrato
se reforzó para exigir archivo objeto y dependencia
real de GCC `-MMD` en formatos `.cpp.o.d` o
`.cpp.d`; en ambos casos la fuente debe pertenecer
a la librería Modbus TCP seleccionada. El selftest
incluye casos negativos de dependencia errónea
o faltante y rutas Win/Linux. La CI actualizada
[38293904677](https://github.com/JW-Control/platform-jwplc/actions/runs/38293904677)
terminó PASS en los dos sistemas. Todavía se requieren
las cuatro compilaciones reales P1C en la PC del
operador; no hay adopción de producto ni upload.

Error/prevención: [F129](A13_TOOLING_FAILURES_AND_PREVENTION_20261006.md).

---

## Alpha13 G10 — P1C primer build EXIT 0, auditoría F129 corregida (2026-10-12)

```text
ALPHA13_STATUS=IN_PROGRESS
G9_FINAL=CLOSED_PASS_WITH_EXPLICIT_COVERAGE_LIMITS
G10_P0_WINDOWS=BASELINE_CAPTURED_REVIEW_FINDINGS_OPEN
G10_P1A_WINDOWS=SOURCE_CONTRACT_PASS_NATIVE_NOT_RUN
G10_P1B_WINDOWS=CANDIDATE_SOURCE_CONTRACT_PASS_NATIVE_NOT_RUN
F128=CLOSED_PASS_CI_AND_LOCAL
G10_P1C_FAILED_RUN_ID=g10_p1c_20261012_134431_10e91e4c
G10_P1C_FAILED_GATE_PHASE=ARDUINO_BUILDS_RUNNING
G10_P1C_FAILED_BASELINE_SERVER_COMPILE_EXIT=0
G10_P1C_FAILED_REASON=TCP_COMPILE_INPUT_NOT_CORRECT_FOLDER_BASELINE_SERVER_JWPLC_ModbusTCP_Client.cpp
G10_P1C_FAILURE_CLASS=F129_HARNESS_SOURCE_PROVENANCE_VERBOSE_MATCH
G10_P1C_CLIENT_AND_CANDIDATE_BUILDS=NOT_EXECUTED_IN_FAILED_RUN
F129_FIX=VERIFY_GCC_MMD_D_FILE_AND_CPP_O_WITH_SOURCE_LIBRARY
F129_CI=PASS_WINDOWS_LINUX_RUN_38293634614
G10_P1C_LOCAL_RETEST=NOT_YET_EXECUTED
G10_P1C_NEXT_GATE=tools/alpha13/gates/run_a13_g10_p1c_a13_006_build.bat
G10_P1C_PRODUCT_ADOPTION=NONE
G10_P1C_HARDWARE_UPLOAD=NONE
A13-006=ADOPT_CONFIRMED_REPRO_FIX_PENDING
A13-007=REVIEW_OPEN
A13-008=REVIEW_OPEN
A13-015=ADOPT_PENDING
A13-016=ADOPT_PENDING
A13-018=ADOPT_PENDING
G10_FINAL=NOT_CLOSED
F120_POST_UPLOAD_FREEZE_ROOT_CAUSE=UNKNOWN_PENDING
RELEASE_PR_MERGE_PUBLICATION=NO
```

El primer P1C del operador pasó preflight y overlay;
`arduino-cli` compiló el ejemplo Server original con
`EXIT=0`, pero un verificador adicional exigía la
ruta absoluta C++ en el formato verbose `-v`.
La fuente concreta del `COMPILE_BASELINE_SERVER.log`
local no está descargada aquí, de modo que **no
afirmar cuál representación real contenía ese log**.
Solo el verificador de procedencia originó
`REVIEW_STOP`; los restantes tres ejemplos
no llegaron a ejecutarse.

Se sustituyó la búsqueda de texto `-v` por la
comprobación de objetos `.cpp.o` y dependencias
`.cpp.d` emitidas mediante `-MMD` por
`platform.txt`, conservando la comprobación exacta
de `Using library JWPLC_ModbusTCP...`, hashes y
Git limpio. CI
[38293634614](https://github.com/JW-Control/platform-jwplc/actions/runs/38293634614)
PASS Windows/Linux con caso positivo y negativos de
dependencia incorrecta o faltante. El nuevo gate aún
no se ha ejecutado localmente; ningún producto G10
ha sido adoptado o programado en placas.

Registro: [F129](A13_TOOLING_FAILURES_AND_PREVENTION_20261006.md).

---

## Alpha13 G10 — P1B Windows PASS; P1C build Arduino preparado (2026-10-12)

```text
ALPHA13_STATUS=IN_PROGRESS
G9_FINAL=CLOSED_PASS_WITH_EXPLICIT_COVERAGE_LIMITS
G10_P0_WINDOWS=BASELINE_CAPTURED_REVIEW_FINDINGS_OPEN
G10_P1A_WINDOWS=SOURCE_CONTRACT_PASS_NATIVE_NOT_RUN
G10_P1B_WINDOWS_RUN_ID=g10_p1b_20261012_124640_8458554e
G10_P1B_WINDOWS_STATUS=CANDIDATE_SOURCE_CONTRACT_PASS_NATIVE_NOT_RUN
F128=RESOLVED_AFTER_WINDOWS_LOCAL_RETEST
G10_P1B_NATIVE_CI=PASS_WINDOWS_LINUX_RUN_38284253191
G10_P1C_CONTRACT_CI=PASS_WINDOWS_LINUX_RUN_38287866687
G10_P1C_NEXT_GATE=tools/alpha13/gates/run_a13_g10_p1c_a13_006_build.bat
G10_P1C_BASELINE_TCP_SERVER=NOT_YET_COMPILED_LOCALLY
G10_P1C_BASELINE_TCP_CLIENT=NOT_YET_COMPILED_LOCALLY
G10_P1C_CANDIDATE_TCP_SERVER=NOT_YET_COMPILED_LOCALLY
G10_P1C_CANDIDATE_TCP_CLIENT=NOT_YET_COMPILED_LOCALLY
G10_P1C_PRODUCT_ADOPTION=NONE
G10_P1C_HARDWARE_UPLOAD=NONE
A13-006=ADOPT_CONFIRMED_REPRO_FIX_PENDING
A13-006_TASK_RESET_W5500_EDGE_CASES=NOT_YET_VALIDATED
A13-007=REVIEW_OPEN
A13-008=REVIEW_OPEN
A13-015=ADOPT_PENDING
A13-016=ADOPT_PENDING
A13-018=ADOPT_PENDING
G10_FINAL=NOT_CLOSED
F120_POST_UPLOAD_FREEZE_ROOT_CAUSE=UNKNOWN_PENDING
RELEASE_PR_MERGE_PUBLICATION=NO
```

El usuario ejecutó P1B nuevamente en el Windows del operador
tras corregir F128 y obtuvo los cuatro candidatos de fuente
`PASS_FOUR_PRIVATE_CANDIDATES`, estado
`CANDIDATE_SOURCE_CONTRACT_PASS_NATIVE_NOT_RUN`.
Se preserva el resultado fallido anterior como evidencia del
defecto del harness, ya cerrado.

El nuevo P1C comprobará **cuatro compilaciones reales**
con Arduino CLI del operador: TCP Server y Client del producto
original y los dos ejemplos usando overlay aislado de fuentes
P1B. Exigirá selección de librería, objetos C++ Client/Server,
enlace core JWPLC Basic, cinco archives congelados, hashes
de candidatos, Git limpio y manifests locales previos.
La CI del harness [38287866687](https://github.com/JW-Control/platform-jwplc/actions/runs/38287866687)
pasó Windows y Linux (selftest, no build Arduino).
No declarar `A13-006=CLOSED_PASS` ni adoptar producto
antes de verificar corrección de flujos begin/end/task/fail,
reconexión, reset del W5500 y reutilización de socket.

Detalle: [A13_G10_P1C_ARDUINO_OVERLAY_EXECUTION_20261012.md](A13_G10_P1C_ARDUINO_OVERLAY_EXECUTION_20261012.md).

---

## Alpha13 G10 — F128 Windows P1B corregido, CI PASS; operador pendiente (2026-10-12)

```text
ALPHA13_STATUS=IN_PROGRESS
G9_FINAL=CLOSED_PASS_WITH_EXPLICIT_COVERAGE_LIMITS
G10_P0_WINDOWS=BASELINE_CAPTURED_REVIEW_FINDINGS_OPEN
G10_P1A_WINDOWS=SOURCE_CONTRACT_PASS_NATIVE_NOT_RUN
G10_P1B_LOCAL_ATTEMPT_RUN=g10_p1b_20261012_122341_30dd9ea2
G10_P1B_LOCAL_ATTEMPT=REVIEW_STOP_HARNESS_KEYERROR
F128_CLASS=WINDOWS_MANIFEST_POSIX_PATH_KEY_MISMATCH
F128_ROOT_CAUSE=STR_PATH_WINDOWS_BACKSLASH_VS_MANIFEST_POSIX
F128_FIX=REPO_KEY_PATH_AS_POSIX_ALL_MAPS
F128_CI=PASS_WINDOWS_LINUX_RUN_38284253191
F128_CI_NEGATIVE_SHA_AND_FULL_CANDIDATE_KEY_TEST=PASS
G10_P1B_LOCAL_RETEST=NOT_YET_EXECUTED
G10_P1B_NEXT_GATE=tools/alpha13/gates/run_a13_g10_p1b_a13_006_candidate.bat
G10_P1B_PRODUCT_CHANGE=NONE
G10_P1B_ARDUINO_BUILD=NOT_YET_EXECUTED
G10_P1B_HARDWARE_UPLOAD=NONE
A13-006=ADOPT_CONFIRMED_REPRO_FIX_PENDING
A13-007=REVIEW_OPEN
A13-008=REVIEW_OPEN
G10_FINAL=NOT_CLOSED
F120_POST_UPLOAD_FREEZE_ROOT_CAUSE=UNKNOWN_PENDING
RELEASE_PR_MERGE_PUBLICATION=NO
```

El operador detectó `KeyError('JWPLC\\2.1.0\\...')` en
P1B. La investigación confirmó un defecto en el harness:
el informe P0/P1A guarda claves POSIX y `str(Path)`
en Windows produjo barras inversas. La primera CI
de candidato había omitido deliberadamente los manifests
locales mediante `--ci-standalone`, por lo que ese PASS
**no cubría** el código fallido.

Se normalizaron **todas** las claves de rutas de los
manifiestos y del mapa `old` usando `Path.as_posix()`.
Se añadió una prueba de la misma función de validación
local, caminos Windows/Posix, hash negativo y ejecución
íntegra de `make()` + `build_native()`.
La CI [38284253191](https://github.com/JW-Control/platform-jwplc/actions/runs/38284253191)
finalizó PASS en Windows y Linux.

**P1B sigue pendiente del retest en Windows real**;
no hay adopción de firmware ni prueba física por este
gate. No repetir P0/P1A ni saltar a P1C todavía.

---

## Alpha13 G10 — P1A local PASS; P1B candidato nativo CI PASS (2026-10-12)

```text
ALPHA13_STATUS=IN_PROGRESS
G9_FINAL=CLOSED_PASS_WITH_EXPLICIT_COVERAGE_LIMITS
G10_P0_WINDOWS_RUN_ID=g10_p0_20261012_114921_34102c6b
G10_P0=BASELINE_CAPTURED_REVIEW_FINDINGS_OPEN
G10_P1A_WINDOWS_RUN_ID=g10_p1a_20261012_120529_a174356e
G10_P1A_LOCAL=SOURCE_CONTRACT_PASS_NATIVE_NOT_RUN
G10_P1A_NATIVE_CI=PASS_WINDOWS_LINUX_RUN_38280631554
G10_P1A_DEFECT=REPRODUCED_NOT_FIXED
A13-006=ADOPT_CONFIRMED_REPRO_FIX_PENDING
G10_P1B_CANDIDATE_SOURCE_PATHS=FOUR_TCP_PRIVATE_IMPLEMENTATION_FILES
G10_P1B_NATIVE_CI=PASS_WINDOWS_LINUX_RUN_38282694249
G10_P1B_NATIVE=EIGHT_SOURCE_BACKED_MARKERS_PASS
G10_P1B_LOCAL_CANDIDATE=NOT_YET_RUN
G10_P1B_NEXT_GATE=tools/alpha13/gates/run_a13_g10_p1b_a13_006_candidate.bat
G10_P1B_PRODUCT_ADOPTION=NONE
G10_P1B_ARDUINO_BUILD=NOT_YET_EXECUTED
G10_P1B_HARDWARE_UPLOAD=NONE
G10_P1B_LINK_RESET_REUSE_NEGATIVES=FURTHER_VALIDATION_REQUIRED
A13-007=REVIEW_OPEN
A13-008=REVIEW_OPEN
A13-015=ADOPT_PENDING
A13-016=ADOPT_PENDING
A13-018=ADOPT_PENDING
G10_FINAL=NOT_CLOSED
F120_POST_UPLOAD_FREEZE_ROOT_CAUSE=UNKNOWN_PENDING
RELEASE_PR_MERGE_PUBLICATION=NO
```

El operador ejecutó G10-P1A fuente con estado
`SOURCE_CONTRACT_PASS_NATIVE_NOT_RUN`. La CI
G10-P1A reprodujo previamente el problema de
ownership al fallar SPI con código cliente y servidor.

Se creó un **candidato P1B exclusivamente temporal**
con estados de cierre pendiente privados, reintento
acotado desde task/solicitud y rechazo de reuso de
socket hasta cerrar. La prueba nativa con los cuerpos
de cierre candidatos terminó **PASS** Windows/Linux:
[CI 38282694249](https://github.com/JW-Control/platform-jwplc/actions/runs/38282694249).
No hay compilación Arduino del candidato ni
adopción productiva. El caso reset W5500/número
de socket reutilizado y las rutas completas de
`task/begin/end` deben probarse antes de commit.

Contrato y alcance:
[A13_G10_P1B_A13_006_FIX_CONTRACT_20261012.md](A13_G10_P1B_A13_006_FIX_CONTRACT_20261012.md).
Evidencia:
[A13_G10_P1B_CANDIDATE_NATIVE_RESULTS_20261012.md](A13_G10_P1B_CANDIDATE_NATIVE_RESULTS_20261012.md).

---

## Alpha13 G10 — P0 Windows PASS y P1A A13-006 reproducido (2026-10-12)

```text
ALPHA13_STATUS=IN_PROGRESS
G9_FINAL=CLOSED_PASS_WITH_EXPLICIT_COVERAGE_LIMITS
G10_P0_WINDOWS_RUN_ID=g10_p0_20261012_114921_34102c6b
G10_P0_WINDOWS_HEAD=bd6787711c1480f9abf2fe79ca16352e03ad4e02
G10_P0_WINDOWS_STATUS=BASELINE_CAPTURED_REVIEW_FINDINGS_OPEN
G10_P0_SOURCE_PATTERNS=A13-006_TRUE_A13-007_TRUE_A13-008_TRUE
G10_P0_CI_SMOKE_TCP=False
G10_P0_CI_SMOKE_RTU_MASTER=False
G10_P0_ARCHIVE_EPOCH_POLICY=True
G10_P0_BINARY_BYTES_BIN_SUM=True
G10_P0_ARCHIVES_FROZEN=5
G10_P1A_SCOPE=A13-006_MODBUS_TCP_CLIENT_CLOSE_AND_SERVER_DROP
G10_P1A_CI=PASS_WIN_LINUX_RUN_38280631554
G10_P1A_CI_NATIVE=SOURCE_BACKED_SIX_SCENARIOS_PASS
G10_P1A_BASELINE=LOCK_DENIED_ORPHAN_REPRODUCED_NOT_FIXED
G10_P1A_LOCAL_SOURCE_CONTRACT=NOT_YET_EXECUTED
G10_P1A_NEXT_GATE=tools/alpha13/gates/run_a13_g10_p1a_a13_006_repro.bat
A13-006=ADOPT_CONFIRMED_REPRO_FIX_PENDING
A13-007=REVIEW_OPEN
A13-008=REVIEW_OPEN
A13-015=ADOPT_PENDING
A13-016=ADOPT_PENDING
A13-018=ADOPT_PENDING
G10_FINAL=NOT_CLOSED
COM4_FIRMWARE=G9_DIAGNOSTIC_NOT_AUTOMATICALLY_RESTORED
COM14_FIRMWARE=G8_DIAGNOSTIC_NOT_AUTOMATICALLY_RESTORED
F120_POST_UPLOAD_FREEZE_ROOT_CAUSE=UNKNOWN_PENDING
RELEASE_PR_MERGE_PUBLICATION=NO
```

El P0 local reportado por el operador verificó Git, cinco
archives y seis patrones/deficiencias de verificación. La
CI nativa G10-P1A compiló los cuerpos reales C++ de
`JWPLC_ModbusTCPClientClass::closeSession()` y
`JWPLC_ModbusTCPClass::dropClient()` con mocks de
SPI/W5500: al denegar lock se reproduce la pérdida
de ownership, y controles con lock/con socket vacío
pasan. Evidencia:
[GitHub Actions 38280631554](https://github.com/JW-Control/platform-jwplc/actions/runs/38280631554).
**No se ha corregido A13-006**, no se ha probado en
hardware y no se han editado archivos de producto G10.
Decisión: adoptar corrección con diseño de cierre eventual
seguro, no bloqueante y sin reuso prematuro de socket.

Detalle y límites:
[A13_G10_P0_AND_P1A_SOURCE_REPRO_PLAN_20261012.md](A13_G10_P0_AND_P1A_SOURCE_REPRO_PLAN_20261012.md).

---

## Alpha13 — G9 CERRADO; G10-P0 baseline CI PASS (2026-10-12)

```text
ALPHA12_STATUS=CLOSED_PUBLISHED
ALPHA13_STATUS=IN_PROGRESS
BRANCH=v2.1.0-alpha.13/feature/cleanup-robustness
G8_FINAL=CLOSED_PASS_WITH_EXPLICIT_COVERAGE_LIMITS
G9_P0=CLOSED_PASS
G9_P1_A13-017=CLOSED_PASS_DOC
G9_P2_A13-012=CLOSED_PASS_PRODUCT_AND_PHYSICAL
G9_P3_A13-014=CLOSED_PASS_PRODUCT_AND_PHYSICAL
G9_P4_GIT_COMMIT=63e0291a7b1480af846e955559ab175aa78e7300
G9_P5_RUN_ID=g9_p5_20261012_112751_200b0593
G9_P5_PORT=COM4
G9_P5_MICROSD=PRESENT_1_READY_1_ROOT_READ_ONLY_1
G9_P5_RTC=PRESENT_1_VALID_1_LOST_0
G9_P5_BUTTON=UP_ID_1_PRESS_1_RELEASE_1
G9_P5_HEARTBEATS_POST_TEST=PASS_3
G9_P5_FINAL_AUDIT=PASS
G9_P5_STATUS=PHYSICAL_PASS_COM4_SD_READONLY_RTC_BUTTONS
G9_COM14=NOT_TOUCHED
G9_COM4_FIRMWARE_RESTORE=NOT_AUTOMATED
G9_FINAL=CLOSED_PASS_WITH_EXPLICIT_COVERAGE_LIMITS
G10_P0_CI=PASS_WINDOWS_LINUX_RUN_38278669640
G10_P0_READONLY_RUNNER=tools/alpha13/gates/run_a13_g10_p0_readonly.bat
G10_P0_LOCAL=NOT_YET_EXECUTED
G10_P0_TCP_SOCKET_DROP_PATTERN=True
G10_P0_DHCP_UNCHECKED_READ_PATTERN=True
G10_P0_UDP_UNBOUNDED_WAIT_PATTERN=True
G10_P0_CI_SMOKE_TCP_PRESENT=False
G10_P0_CI_SMOKE_RTU_MASTER_PRESENT=False
G10_P0_FREEZE_GIT_EPOCH_POLICY=True
G10_P0_BINARY_BYTES_BIN_SUM=True
G10_P0_ARCHIVES_FROZEN=5
G10_A13-006=REVIEW_OPEN
G10_A13-007=REVIEW_OPEN
G10_A13-008=REVIEW_OPEN
G10_A13-015=ADOPT_PENDING
G10_A13-016=ADOPT_PENDING
G10_A13-018=ADOPT_PENDING
G10_FINAL=NOT_CLOSED
F120_POST_UPLOAD_FREEZE_ROOT_CAUSE=UNKNOWN_PENDING
OPENPLC=OUT_OF_SCOPE
HMI_DESIGNER=OUT_OF_SCOPE
TFT_NEW_FEATURES=OUT_OF_SCOPE
RELEASE_MERGE_PR_PUBLICATION=NO
NEXT_GATE=G10_P0_LOCAL_READ_ONLY_BASELINE
```

El operador terminó el smoke de G9 con `COMPILE_EXIT=0`,
selección del archive SD productivo, upload autorizado,
microSD detectada/montada y raíz abierta con `FILE_READ`,
RTC válido sin `lostPower`, PRESS/RELEASE del botón
UP y tres heartbeats. La auditoría final fue PASS.
Con la evidencia previa Git/producto y regresiones
4/4 se cierra G9 **con límites de cobertura explícitos**.
COM4 sigue con sketch de diagnóstico, COM14 queda con
el firmware de test G8 según última evidencia.
No hay restauración automática ni solución demostrada de F120.

En la conciliación con el informe canónico
`Markdown pegado.md`, G5/G6 resultan correctos en
sus pruebas, pero **no cierran por sí mismos**
A13-006 (closeSession pierde socket si falla SPI),
A13-007 (DHCP parcial) ni A13-008 (UDP sin deadline).
La inspección read-only actual encontró los tres
patrones. A13-015/016/018 siguen abiertos como deuda
de CI, procedencia de archive y metricas. La nueva
CI G10-P0 [38278669640](https://github.com/JW-Control/platform-jwplc/actions/runs/38278669640)
pasó Windows y Linux, pero es una foto de hallazgos:
**no implica corrección de esos defectos**.

Cierre canónico de G9:
[A13_G9_P5_PHYSICAL_CLOSURE_20261012.md](A13_G9_P5_PHYSICAL_CLOSURE_20261012.md),
[A13_G9_FINAL_CLOSURE_CHECKLIST_20261012.md](A13_G9_FINAL_CLOSURE_CHECKLIST_20261012.md).
Plan G10:
[A13_G10_OPERATIVE_PLAN_20261012.md](A13_G10_OPERATIVE_PLAN_20261012.md).

---

## G9-P5 — Smoke físico COM4 preparado; microSD presente (2026-10-12)

```text
ALPHA13_STATUS=IN_PROGRESS
G9_GIT_PRODUCT=CLOSED_PASS_COMMIT_63e0291a
G9_FUNCTIONAL=IN_PROGRESS_PHYSICAL_SMOKE_PENDING
G9_P5_SELECTED_HARDWARE=JWPLC_BASIC_COM4
G9_P5_MICROSD_CONFIRMED_BY_OPERATOR=YES
G9_P5_COM14=NOT_SELECTED_AND_NOT_TOUCHED
G9_P5_SD_TEST=ROOT_DIRECTORY_FILE_READ_ONLY
G9_P5_RTC_TEST=JWPLC_TIME_CACHED_PRESENT_VALID_LOST_POWER
G9_P5_BUTTON_TEST=ONE_REAL_PRESS_AND_RELEASE_SAME_ID
G9_P5_POST_TEST_HEARTBEATS=3_REQUIRED_NOT_F120_RESOLUTION
G9_P5_CONTRACT_CI=PASS_WINDOWS_LINUX_RUN_38274903580
G9_P5_PHYSICAL=NOT_EXECUTED
G9_P5_UPLOAD=NOT_EXECUTED
G9_P5_GATE=tools/alpha13/gates/run_a13_g9_p5_physical_com4.bat
G9_P5_SKETCH=tools/alpha13/sketches/g9_p5_sd_rtc_buttons/g9_p5_sd_rtc_buttons.ino
G9_P5_UPLOAD_AUTHORIZATION=REQUIRED_INTERACTIVELY_BEFORE_FLASH
G9_P5_PREVIOUS_APPLICATION_RESTORE=NOT_AUTOMATED
F120_POST_UPLOAD_FREEZE_ROOT_CAUSE=UNKNOWN_PENDING
G10=NOT_STARTED
OPENPLC=OUT_OF_SCOPE
HMI_DESIGNER=OUT_OF_SCOPE
TFT_NEW_FEATURES=OUT_OF_SCOPE
RELEASE_MERGE_PR_PUBLICATION=NO
```

El operador confirmó microSD insertada en COM4. Se creó un
sketch de lectura SD root con `JWPLC_SD.open("/", FILE_READ)`,
consulta RTC mediante snapshot `JWPLC_Time`, y captura
real de PRESS/RELEASE de la misma tecla frontal.
El gate compilará desde producto G9 normal, verificará
SHA del archive precompilado y exigirá confirmación
explícita antes del upload **solo COM4**; COM14 no se usa.
El runner no restaura firmware previo y deja evidencias
locales. La CI de contrato [38274903580](https://github.com/JW-Control/platform-jwplc/actions/runs/38274903580)
terminó PASS en Windows y Linux. Esto aún no es
evidencia de hardware. Para decidir PASS físico se exige
el log posterior a upload, RTC y directorio SD válidos,
un botón presionado/liberado y tres heartbeats.

Plan:
[A13_G9_P5_COM4_SD_RTC_BUTTONS_EXECUTION_20261012.md](A13_G9_P5_COM4_SD_RTC_BUTTONS_EXECUTION_20261012.md).

---

## Alpha13 G9 — Cierre Git CONFIRMADO; smoke periférico pendiente (2026-10-12)

```text
ALPHA13_STATUS=IN_PROGRESS
G8_FINAL=CLOSED_PASS_WITH_EXPLICIT_COVERAGE_LIMITS
G9_P0=CLOSED_PASS_READ_ONLY
G9_P1_A13-017=CLOSED_PASS_DOC_ONLY
G9_P2_A13-012=PRODUCT_COMMITTED_BUILD_PASS_PHYSICAL_PENDING
G9_P3_A13-014=PRODUCT_COMMITTED_BUILD_PASS_PHYSICAL_PENDING
G9_ADOPTION_RUN_ID=g9_adopt_20261012_101458_1e395a6e
G9_GIT_RUN_ID=g9_git_20261012_102731_5f38ca7a
G9_GIT_PRODUCT=CLOSED_PASS
G9_PRODUCT_COMMIT=63e0291a7b1480af846e955559ab175aa78e7300
G9_PRODUCT_REMOTE_HEAD_VERIFIED=63e0291a7b1480af846e955559ab175aa78e7300
G9_PRODUCT_REMOTE_PATHS=EXACT_THREE
G9_SD_ARCHIVE_SHA256_OPERATOR=58e86fdf31c9bf94204fcbcf7462656fd9f4d9a7d73bff35bd660dbff538b89e
G9_CANDIDATE_PRODUCT_REGRESSIONS=PASS_4_OF_4
G9_GIT_CLOSE_CI=PASS_WINDOWS_LINUX_RUN_38270082029
G9_PHYSICAL_SD_RTC_BUTTONS=NOT_EXECUTED
G9_HARDWARE_MICROSD_CARD_AND_PORT=NEEDS_CONFIRMATION
G9_FUNCTIONAL=IN_PROGRESS_PHYSICAL_SMOKE_PENDING
G9_PHYSICAL_PLAN=docs/v2.1.0-alpha.13/A13_G9_PHYSICAL_SMOKE_PLAN_20261012.md
G10=NOT_STARTED
F120_POST_UPLOAD_FREEZE_ROOT_CAUSE=UNKNOWN_PENDING
OPENPLC=OUT_OF_SCOPE
HMI_DESIGNER=OUT_OF_SCOPE
TFT_NEW_FEATURES=OUT_OF_SCOPE
RELEASE_MERGE_PR_PUBLICATION=NO
```

El operador confirmó `G9_GIT_PREFLIGHT=PASS`,
`G9_GIT_STAGED_AUDIT=PASS`, `G9_GIT_COMMIT_PUSH=PASS`.
La verificación independiente en GitHub identificó el
commit productivo [`63e0291a`](https://github.com/JW-Control/platform-jwplc/commit/63e0291a7b1480af846e955559ab175aa78e7300)
como HEAD remoto y comprobó sus tres rutas exactas:
`JW_SD.cpp`, `libJW_SD.a`,
`JWPLC_GlobalPeripherals.cpp`. El diff remoto de texto
coincide con los cambios mínimos A13-012/A13-014; archive
SHA fue verificado por las pruebas locales del operador.

**La fase Git/producto de G9 está cerrada.** La fase
funcional física permanece abierta: se requiere smoke
SD de solo lectura, RTC y botón en un JWPLC real por
tratarse de periféricos. Los dos equipos de G8 siguen
con sketches diagnósticos según la última evidencia;
no asumir restauración ni microSD presente. Cualquier
upload físico requerirá confirmación de placa/COM,
aislamiento y autorización explícita.

Cierre Git:
[A13_G9_GIT_PRODUCT_CLOSURE_20261012.md](A13_G9_GIT_PRODUCT_CLOSURE_20261012.md).
Plan físico:
[A13_G9_PHYSICAL_SMOKE_PLAN_20261012.md](A13_G9_PHYSICAL_SMOKE_PLAN_20261012.md).

---

## G9-P2/P3 — Adopción producto local PASS, commit controlado pendiente (2026-10-12)

```text
ALPHA13_STATUS=IN_PROGRESS
G8_FINAL=CLOSED_PASS_WITH_EXPLICIT_COVERAGE_LIMITS
G9_P0=CLOSED_PASS_READ_ONLY
G9_P1_A13-017=CLOSED_PASS_DOC_ONLY
G9_P2_A13-012=PRODUCT_ADOPTED_BUILD_PASS_PENDING_COMMIT
G9_P3_A13-014=PRODUCT_ADOPTED_BUILD_PASS_PENDING_COMMIT
G9_PRODUCT_ADOPTION_RUN=g9_adopt_20261012_101458_1e395a6e
G9_ADOPTION_PREFLIGHT=PASS
G9_ADOPTION_BACKUP=PASS
G9_ADOPTION_SCOPE=PASS_EXACT_THREE_PRODUCT_PATHS
G9_ADOPTION_REGRESSIONS=PASS_4_OF_4
G9_ADOPTION_FINAL_AUDIT=PASS
G9_ADOPTION_ARCHIVE_SHA256=58e86fdf31c9bf94204fcbcf7462656fd9f4d9a7d73bff35bd660dbff538b89e
G9_PRODUCT_COMMIT=NONE
G9_PRODUCT_GIT_CLOSURE_RUNNER=tools/alpha13/gates/run_a13_g9_git_close.bat
G9_GIT_CLOSURE_CI=PASS_WINDOWS_LINUX_RUN_38270082029
G9_PHYSICAL_SD_RTC_BUTTONS=NOT_EXECUTED
G9_FINAL=IN_PROGRESS
G10_FINAL=PENDING
F120_POST_UPLOAD_FREEZE_ROOT_CAUSE=UNKNOWN_PENDING
RELEASE_PR_MERGE_PUBLICATION=NO
```

El operador realizó la adopción local real de los tres cambios
de producto. Se respaldaron y verificaron los originales,
el archive SD es el nuevo `58e86fdf...` y los cuatro
ejemplos compilaron con la selección productiva normal
y `-lJW_SD` exacto. Git quedó con exactamente las tres
rutas permitidas modificadas, índice sin stage.
**Todavía no hay commit ni push del producto G9**.

Se versionó el cierre Git controlado, sin recompilar o
acceder al hardware, con verificación de manifests
P0/candidato/adopción, SHA de backups y blobs HEAD,
triple scope y push. Su autoprueba read-only pasó
en [Windows y Linux CI 38270082029](https://github.com/JW-Control/platform-jwplc/actions/runs/38270082029).
El operador solo debe hacer `git pull --ff-only` de las
herramientas y ejecutar el BAT de cierre, **sin cambiar
manual ni staged los tres archivos**.

Evidencia:
[A13_G9_PRODUCT_ADOPTION_PASS_20261012.md](A13_G9_PRODUCT_ADOPTION_PASS_20261012.md).
No declarar G9 `CLOSED_PASS` hasta cierre Git y
decisión explícita de cobertura física o riesgo
residual; Alpha13 permanece en progreso.

---

## G9-P2/P3 — candidato compilado PASS en Windows, adopción preparada (2026-10-12)

```text
ALPHA13_STATUS=IN_PROGRESS
G8_FINAL=CLOSED_PASS_WITH_EXPLICIT_COVERAGE_LIMITS
G9_P0_WINDOWS=CLOSED_PASS_READ_ONLY
G9_P1_A13-017=DOC_ONLY_PASS
G9_CANDIDATE_RUN_ID=g9_p2p3_20261012_000449_23842b4f
G9_CANDIDATE_SOURCE_CONTRACT=PASS
G9_CANDIDATE_SOURCE_BUILD=PASS
G9_CANDIDATE_ARCHIVE_MEMBER_BYTE_PARITY=PASS
G9_CANDIDATE_NEW_SD_ARCHIVE_SHA256=58e86fdf31c9bf94204fcbcf7462656fd9f4d9a7d73bff35bd660dbff538b89e
G9_CANDIDATE_REGRESSIONS=PASS_SD_BASIC_GLOBAL_SD_GLOBAL_RTC_GLOBAL_BUTTONS
G9_CANDIDATE_LINK=PASS_EXACT_PATH_FLAG_SHA_ALL_FOUR
G9_CANDIDATE_FINAL_AUDIT=PASS
G9_CANDIDATE_STATUS=CANDIDATE_BUILD_PASS_PRODUCT_UNCHANGED
G9_PRODUCT_ADOPTION=PREPARED_NOT_EXECUTED
G9_ADOPTION_CI=PASS_WINDOWS_LINUX_RUN_38209971080
G9_NEXT=tools/alpha13/gates/run_a13_g9_adopt.bat
G9_EXPECTED_PRODUCT_DIFF=JW_SD_CPP_LIBJW_SD_A_GLOBALPERIPHERALS_CPP
G9_PRODUCT_COMMIT=NONE
G9_HARDWARE_TEST=NONE
G9_FINAL=OPEN
G10_FINAL=NOT_STARTED
RELEASE_MERGE_PR_PUBLICATION=NO
```

El candidato de A13-012 y A13-014 compiló exitosamente
con el Arduino CLI local del operador y realizó cuatro regresiones
de enlace normal. Se generó un nuevo `libJW_SD.a` con paridad
del miembro extraído y objeto real; la fuente y el archive
originales permanecen intactos. El ejecutor de adopción
`a13_g9_adopt_local.py` ya está versionado y sus pruebas
de contrato Git de tres rutas, escenarios negativos y compatibilidad
Windows/Linux pasaron:
[CI 38209971080](https://github.com/JW-Control/platform-jwplc/actions/runs/38209971080).
No se ejecutó adopción, commit ni flash todavía.

Detalles:
[A13_G9_P2P3_CANDIDATE_AND_ADOPTION_GATE_20261012.md](A13_G9_P2P3_CANDIDATE_AND_ADOPTION_GATE_20261012.md).

---

## Alpha13 — G9-P0 Windows PASS; P2/P3 candidatos preparados (2026-10-11)

```text
ALPHA13_STATUS=IN_PROGRESS
G8=CLOSED_PASS_WITH_EXPLICIT_COVERAGE_LIMITS
G9_P0_WINDOWS=BASELINE_CAPTURED_READ_ONLY
G9_P0_RUN_ID=g9_p0_20261011_234245_6727f0b0
G9_P0_HEAD=93cbd1e7e04ac4f5aedac95bf35fa29453733c38
G9_P0_SD_NUL=1_OFFSET_28732
G9_P0_SD_ARCHIVE_SHA256=1b9619ba37295782ade1edc6f06439a38e7e534fc08f5b05757c9af7a645acc0
G9_P0_GLOBAL_RTC_REFS=3
G9_P0_GLOBAL_THROTTLE_REFS=2
G9_P0_README_OUTDATED_ALPHA11=False
G9_P1_A13-017=DOC_ONLY_APPLIED_AUDITED
G9_P2_A13-012=CANDIDATE_RUNNER_READY_NOT_YET_BUILT
G9_P3_A13-014=CANDIDATE_RUNNER_READY_NOT_YET_BUILT
G9_P2P3_CI_CONTRACT=PASS_WIN_LINUX_RUN_38205406022
G9_P2P3_SOURCE_BUILD_WINDOWS=NOT_YET_EXECUTED
G9_P2P3_SD_NEW_ARCHIVE=NOT_YET_BUILT
G9_P2P3_PRODUCT_ADOPTION=NONE
G9_P2P3_HARDWARE_ACCESS=NONE
F125_F126_F127=RESOLVED_CI_PASS_HARNESS_ONLY
G9_NEXT_GATE=tools/alpha13/gates/run_a13_g9_p2p3_candidate.bat
G9_FINAL=IN_PROGRESS
G10_FINAL=NOT_STARTED
RELEASE_MERGE_PR_PUBLICATION=NO
```

El operador confirmó P0 desde
`93cbd1e7e04ac4f5aedac95bf35fa29453733c38`;
el resultado coincide con baseline CI (NUL SD,
archive y referencias GlobalPeripherals) y confirma
que A13-017 ya no tiene el reclamo antiguo sobre Alpha11.
Se creó el ejecutor **P2/P3 offline** que construye
el SD desde fuente, regenera `libJW_SD.a`,
comprueba la paridad miembro/objeto y compila
regresiones BasicReadWrite, microSD, RTC y Buttons
en el package normal con GlobalPeripherals candidata.
El contrato exacto pasó en
[CI 38205406022](https://github.com/JW-Control/platform-jwplc/actions/runs/38205406022)
Windows/Linux, después de corregir solo el harness
por posición NUL, consola `--help` y CRLF/LF.
No se ha compilado aún el nuevo archivo SD en la PC
ni se han modificado fuentes/archives de producto.

Ejecución preparada:
[A13_G9_P2P3_CANDIDATE_EXECUTION_PLAN_20261011.md](A13_G9_P2P3_CANDIDATE_EXECUTION_PLAN_20261011.md).

---

## Alpha13 — G9 en curso: P0 CI PASS y A13-017 documento corregido (2026-10-11)

```text
ALPHA12=CLOSED_PUBLISHED
ALPHA13=IN_PROGRESS
BRANCH=v2.1.0-alpha.13/feature/cleanup-robustness
G8_FINAL=CLOSED_PASS_WITH_EXPLICIT_COVERAGE_LIMITS
G9_AUDIT_SCOPE=A13-012_A13-014_A13-017
G9_P0_CI=PASS_WIN_LINUX_RUN_38204046284
G9_P0_READONLY_RUNNER=tools/alpha13/gates/a13_g9_p0_readonly.py
G9_P0_WINDOWS_USER=NOT_YET_EXECUTED
G9_P0_SD_NUL_COUNT=1
G9_P0_SD_NUL_OFFSET=28732
G9_P0_SD_ARCHIVE_SHA256=1b9619ba37295782ade1edc6f06439a38e7e534fc08f5b05757c9af7a645acc0
G9_P0_GLOBAL_RTC_REFS=3
G9_P0_GLOBAL_THROTTLE_REFS=2
G9_P1_A13-017=DOC_ONLY_APPLIED_AND_AUDITED
G9_P1_HEADERS_NONCOMMENT_LINES_UNCHANGED=True
G9_P2_A13-012=PENDING_SOURCE_ARCHIVE_SAFE_CHANGE
G9_P3_A13-014=PENDING_GLOBALPERIPHERALS_EQUIVALENCE
G9_FINAL=IN_PROGRESS_NOT_CLOSED
G10_FINAL_FREEZE=PENDING
G10_A13-015=ADOPT_PENDING_CI_SMOKE
G10_A13-016=ADOPT_PENDING_ARCHIVE_FINGERPRINT
G10_A13-018=ADOPT_PENDING_BINARY_METRICS
G10_G5_G6_CANONICAL_CORRESPONDENCE=PENDING
G9_CI_INITIAL_SHALLOW_CHECKOUT_REVIEW=RESOLVED_NO_PRODUCT_CHANGE
F120_POST_UPLOAD_FREEZE_ROOT_CAUSE=UNKNOWN_PENDING
NEXT_GATE=G9-P0_READONLY_LOCAL_THEN_G9_INTEGRATED_P2_P3
OPENPLC=OUT_OF_SCOPE
HMI_DESIGNER=OUT_OF_SCOPE
TFT_NEW_FEATURES=OUT_OF_SCOPE
RELEASE_MERGE_PR_PUBLICATION=NO
```

Después de cerrar G8 con dos JWPLC Basic físicos
se abrió G9 según el grupo canónico de cleanup.
La auditoría original adjudica A13-012 al NUL embebido
en `JW_SD.cpp` (archivo precompilado),
A13-014 a variables privadas invariables en
`JWPLC_GlobalPeripherals.cpp`, y A13-017 a textos activos
anacrónicos de README y headers.

El gate **read-only P0** validó en Windows y Linux:
[CI 38204046284](https://github.com/JW-Control/platform-jwplc/actions/runs/38204046284).
Identificó un solo NUL en offset 28732, 3 referencias
RTC y 2 de refresh; fue necesario habilitar historia
Git completa de checkout CI para contrastar la ascendencia
del commit de G8. A13-017 corrigió exclusivamente texto
activo de README, `JWPLC_ModbusTCP.h` y `JW_SD.h`,
con non-comment lines de ambos headers idénticas
al baseline de G8.

**No hay nuevas modificaciones de firmware**,
ni archive SD reconstruido, ni prueba física de G9.
Los trabajos de P2/P3 exigen respaldo y validaciones
normales Arduino/archives antes de commit de producto.

Plan:
[A13_G9_CLEANUP_PLAN_20261011.md](A13_G9_CLEANUP_PLAN_20261011.md).
Baseline y corrección textual:
[A13_G9_P0_P1_BASELINE_AND_DOC_CLOSURE_20261011.md](A13_G9_P0_P1_BASELINE_AND_DOC_CLOSURE_20261011.md).

---

## G8 — CIERRE FUNCIONAL PASS tras validación física Master COM4 / Slave COM14 (2026-10-11)

```text
ALPHA13_STATUS=IN_PROGRESS
G7C=CLOSED_PASS
G8_P0=CLOSED_PASS
G8_P1P2=CLOSED_PASS_NATIVE_AND_ARCHIVE_LINK
G8_P3=CLOSED_PASS_PRODUCT_ADOPTED_AND_COMMITTED
G8_P3_PRODUCT_COMMIT=fb71d65eb2c842b3f46e6949760e693f3b73d95e
G8_P4_RUN_ID=g8_p4_20261011_223555_543f69c4
G8_P4_MASTER=COM4
G8_P4_SLAVE=COM14
G8_P4_HARDWARE=2_JWPLC_BASIC
G8_P4_BAUD_FORMAT=115200_8N1
G8_P4_RTU=FC03_SLAVE_ID_2
G8_P4_DUAL_READY=PASS
G8_P4_UPLOAD_SLAVE=PASS
G8_P4_UPLOAD_MASTER=PASS
G8_P4_FC03_CORRECT=12_OF_12
G8_P4_MASTER_RX_BYTES=156_EXPECTED_156
G8_P4_SLAVE_RX_BYTES=96_EXPECTED_96
G8_P4_MASTER_RX_TX_FRAMES=12_12
G8_P4_SLAVE_RX_TX_FRAMES=12_12
G8_P4_SLAVE_REQUESTS=12
G8_P4_MASTER_CRC_TIMEOUTS=0_0
G8_P4_SLAVE_CRC=0
G8_P4_FINAL_AUDIT=PASS
G8_P4_STATUS=PHYSICAL_PASS_2JWPLC_12FC03_RX_EXACT
G8_FINAL=CLOSED_PASS_WITH_EXPLICIT_COVERAGE_LIMITS
A13-010=CLOSED_PASS_COMMITTED_PHYSICAL_RX_CORRECT
A13-011=CLOSED_PASS_COMMITTED_UART_READY_REAL_NATIVE_NEGATIVE
G8_REMAINING_HARDWARE_TESTS=OPTIONAL_SEPARATE_QA
G8_TWO_BOARD_FIRMWARE=DIAGNOSTIC_SKETCH_LOADED_NOT_AUTO_RESTORED
F120_POST_UPLOAD_FREEZE_CAUSE=UNKNOWN_PENDING_SEPARATE
G8_EXTRA_PRODUCT_EDITS=NONE
G8_PR_MERGE_TAG_RELEASE=NO
ALPHA13_FINAL_CLOSURE=NOT_YET
NEXT_ALPHA13=RECONCILE_REMAINING_GATES_AND_RELEASE_CHECKLIST
```

El operador ejecutó exitosamente el gate de dos JWPLC Basic
en banco (COM4 Master y COM14 Slave), con confirmación de carga
en ambas placas y telemetría de ambos extremos. Se recibieron
las 12 respuestas FC03 con valores válidos y contadores de bytes
exactos, 156/96; UART READY=1 en ambos, CRC y timeouts=0,
auditoría Git final PASS. Esto confirma físicamente los cambios
A13-010/A13-011 en el alcance positivo ensayado. La ruta
negativa de error de driver UART y begin/end/begin permanecen
cubiertas en los fixtures C++ nativos, no por inyección física
de errores en estas placas.

**G8 queda cerrado funcionalmente**, con límites documentados
(no declarar ensayados FC adicionales, bulk físico, EMC,
falla física UART ni F120). El resultado NO cierra Alpha13
ni autoriza publicación o cambio de alpha. Las placas siguen
con sketches de banco y requieren restauración antes de servicio.

Evidencia canónica:
[A13_G8_P4_PHYSICAL_CLOSURE_20261011.md](A13_G8_P4_PHYSICAL_CLOSURE_20261011.md).
Checklist:
[A13_G8_CLOSURE_CHECKLIST_20261011.md](A13_G8_CLOSURE_CHECKLIST_20261011.md).

---

## G8-P4 — Gate físico preparado para Master COM4 y Slave COM14 (2026-10-11)

```text
ALPHA13_STATUS=IN_PROGRESS
G8_GIT_PRODUCT=CLOSED_PASS_COMMIT_fb71d65e
G8_P4_HARDWARE=2_JWPLC_BASIC
G8_P4_MASTER_PORT=COM4
G8_P4_SLAVE_PORT=COM14
G8_P4_RTU=115200_8N1_SLAVE_ID_2_FC03
G8_P4_RTU_CYCLES=12
G8_P4_EXPECTED_RX_MASTER=156
G8_P4_EXPECTED_RX_SLAVE=96
G8_P4_CI_SELFTEST=PASS_RUN_38194119661_WIN_AND_LINUX
G8_P4_UPLOAD=NOT_EXECUTED
G8_P4_PHYSICAL_RESULT=NOT_YET_TESTED
G8_P4_WRAPPER=tools/alpha13/gates/run_a13_g8_p4_dual_plc_physical.bat
G8_PHYSICAL_RISK=OVERWRITES_CURRENT_FIRMWARE_BOTH_PLCS_NO_AUTO_RESTORE
G8_PHYSICAL_GATE=PREPARED_AWAITING_OPERATOR_BENCH_CONFIRMATION
G8=CURRENT_NOT_CLOSED
RELEASE_MERGE_PR_PUBLICATION=NO
```

El operador dispone de dos JWPLC Basic: COM4 Master y COM14
Slave. Se versionaron dos sketches de banco sin uso de relés,
un runner Python y un BAT único para compilar contra librerías
productivas, validar archivo RTU precompilado y acceso COM,
exigir confirmación interactiva para sobrescribir ambos
firmwares y probar 12 FC03 con token específico. El PASS requiere
datos y contadores RX exactos desde ambas placas (156/96),
cero errores CRC/timeout y telemetría READY de las dos UART.
La autorización de upload no está implícita: requiere confirmar
que ambos equipos están en banco aislado y se puede sustituir
su firmware. No se restaura firmware anterior automáticamente.

El selftest del gate, con casos negativos de datos, conteos,
roles y timeout, pasó en ambos entornos:
[CI G8 P4](https://github.com/JW-Control/platform-jwplc/actions/runs/38194119661).
No hay todavía evidencia de tráfico eléctrico.

Documento:
[A13_G8_P4_DUAL_JWPLC_COM4_COM14_EXECUTION_PLAN_20261011.md](A13_G8_P4_DUAL_JWPLC_COM4_COM14_EXECUTION_PLAN_20261011.md).

---

## G8 — CI nativa post-commit reparada y PASS, F124 registrado (2026-10-11)

```text
G8_PRODUCT_COMMIT=fb71d65eb2c842b3f46e6949760e693f3b73d95e
G8_GIT_PRODUCT=CLOSED_PASS
G8_POST_COMMIT_CI_INITIAL=FAIL_RUN_38189443172_FIXTURE_ONLY
F124_ROOT_CAUSE=NON_IDEMPOTENT_TEST_AFTER_CANDIDATE_ADOPTION
F124_CORRECTION=CI_EPHEMERAL_DIFF_ALL_THREE_PATHS
G8_POST_COMMIT_CI_RETEST=PASS_RUN_38191980973
G8_NATIVE_CPP=PASS_20_TESTS
G8_ADOPTION_SOURCE_CONTRACT_CI=PASS
G8_GIT_DIFF_SCOPE_CI=PASS
G8_PRODUCTION_ARCHIVE_CHANGED_BY_CI_FIX=NO
F122_F123=RESOLVED_WINDOWS_PRODUCT_PASS
F124=RESOLVED_CI_PASS
G8_PHYSICAL_RS485=PENDING_HARDWARE_PEER_CONFIRMATION
G8_FUNCTIONAL_GATE=NOT_CLOSED
ALPHA13=IN_PROGRESS
RELEASE_MERGE_PR_PUBLICATION=NO
```

La CI disparada inmediatamente por el commit de producto falló
exclusivamente porque su fixture asumía que copiar las fuentes
candidatas siempre las modificaría; después de la adopción
eran idénticas y solo se modificaba el archive temporal.
Se reparó el workflow para producir tres cambios verificables
exclusivamente en el checkout efímero **después** de ejecutar
los casos nativos reales. El retest CI terminó PASS, sin
nuevo commit de producto y sin cambiar el archive RTU publicado
en la rama.

Evidencia:
[CI G8 post-commit PASS](https://github.com/JW-Control/platform-jwplc/actions/runs/38191980973);
[A13_TOOLING_FAILURES_AND_PREVENTION_20261006.md](A13_TOOLING_FAILURES_AND_PREVENTION_20261006.md)
F124.

---

## G8 — Cierre Git de producto confirmado; prueba física pendiente (2026-10-11)

```text
ALPHA13_STATUS=IN_PROGRESS
G7C=CLOSED_PASS
G8_P0=CLOSED_PASS_READ_ONLY
G8_P1P2=CANDIDATE_BUILD_PASS
G8_P3_ADOPTION=CLOSED_PASS_LOCAL
G8_P3_ADOPTION_RUN_ID=g8_adopt_20261011_193839_57f9bb49
G8_GIT_COMMIT_PUSH=PASS
G8_GIT_PRODUCT_COMMIT=fb71d65eb2c842b3f46e6949760e693f3b73d95e
G8_REMOTE_BRANCH_HEAD_VERIFIED=fb71d65eb2c842b3f46e6949760e693f3b73d95e
G8_COMMITTED_FILES=EXACT_THREE_CPP_RTU_CPP_RS485_ARCHIVE_RTU
G8_ARCHIVE_SHA256=f239100fbb6770a3a6f336179fbfa3a4afd27cc16e12c6f239cf05f28f7a1416
A13-010=PRODUCT_COMMITTED_REGRESSIONS_PASS_PHYSICAL_PENDING
A13-011=PRODUCT_COMMITTED_REGRESSIONS_PASS_PHYSICAL_PENDING
G8_GIT_TECHNICAL_CLOSURE=PASS
G8_PHYSICAL_RS485=NOT_EXECUTED_PENDING_HARDWARE_CONFIRMATION
G8_FUNCTIONAL_GATE=IN_PROGRESS_NOT_CLOSED
G8_NEXT=P4_PHYSICAL_PRECONDITIONS_PEER_WIRING_AUTHORIZATION
F120_POST_UPLOAD_FREEZE_CAUSE=UNKNOWN_PENDING
OPENPLC=OUT_OF_SCOPE
HMI_DESIGNER=OUT_OF_SCOPE
TFT_NEW_FEATURES=OUT_OF_SCOPE
RELEASE_MERGE_PR_PUBLICATION=NO
```

El operador informó PASS del cierre Git `g8_git_20261011_194840_716ae090`.
La lectura independiente de GitHub confirmó que el commit de producto
incluye exactamente `JWPLC_ModbusRTU.cpp`, `JWPLC_RS485.cpp` y
`libJWPLC_ModbusRTU.a` y que corresponde al HEAD de la rama.
Los cambios **ya están versionados** en remoto y dejan atrás los
candidatos TEMP; sin embargo, aún no se ha realizado transmisión
física Modbus RTU/RS-485 ni nuevo upload. La evidencia de CI nativo
con mocks, los tres builds productivos y el enlace del archive no
son prueba de comunicación eléctrica. El siguiente hito es definir
un montaje de prueba real sin alterar equipos operativos.

Cierre Git:
[A13_G8_GIT_PRODUCT_CLOSURE_20261011.md](A13_G8_GIT_PRODUCT_CLOSURE_20261011.md).
Plan físico:
[A13_G8_RS485_MODBUS_PHYSICAL_VALIDATION_PLAN_20261011.md](A13_G8_RS485_MODBUS_PHYSICAL_VALIDATION_PLAN_20261011.md).

---

## G8 — Adopción local PASS; cierre Git preparado, prueba física pendiente (2026-10-11)

```text
ALPHA13_STATUS=IN_PROGRESS
G7C=CLOSED_PASS
G8_P0=CLOSED_PASS_READ_ONLY
G8_P1P2=CANDIDATE_BUILD_PASS_PRODUCT_UNCHANGED
G8_P3_ADOPTION=PASS_LOCAL
G8_P3_RUN_ID=g8_adopt_20261011_193839_57f9bb49
G8_P3_PREFLIGHT_BACKUP_PRODUCT_ADOPTION=PASS
G8_P3_REGRESSIONS=PASS_SLAVE_MASTER_RS485_STATUS
G8_P3_SOURCE_CONTRACT=PASS_PRE_AND_POST
G8_P3_ARCHIVE_LINK=PASS_SLAVE_MASTER_EXACT_PATH_FLAG_SHA
G8_P3_FINAL_GIT_DIRTY_PATHS=EXACT_THREE_PRODUCT_FILES
G8_P3_RTU_ARCHIVE_SHA256=f239100fbb6770a3a6f336179fbfa3a4afd27cc16e12c6f239cf05f28f7a1416
A13-010=PRODUCT_ADOPTED_LOCAL_PENDING_COMMIT
A13-011=PRODUCT_ADOPTED_LOCAL_PENDING_COMMIT
G8_GIT_CLOSE_RUNNER=tools/alpha13/gates/a13_g8_git_close.py
G8_GIT_CLOSE_BAT=tools/alpha13/gates/run_a13_g8_git_close.bat
G8_GIT_CLOSURE_LOCAL=NOT_YET_EXECUTED
G8_PHYSICAL_RS485=NOT_EXECUTED_PENDING_DECISION
G8_NO_UPLOAD_COM_FRAM=YES
G8_PRODUCT_COMMIT=NO
G8=CURRENT_NOT_CLOSED
NEXT=G8_GIT_CLOSE_THREE_FILES
OPENPLC=OUT_OF_SCOPE
HMI_DESIGNER=OUT_OF_SCOPE
TFT_NEW_FEATURES=OUT_OF_SCOPE
RELEASE_MERGE_PR_PUBLICATION=NO
```

La adopción productiva local en Windows ha pasado y la compilación
normal enlaza el archive RTU correcto. El operador conserva
**tres cambios locales intencionales** hasta versionarlos juntos
en el commit de producto. No publicar release ni confundir CI con
prueba eléctrica. Se ha preparado un cierre Git que verificará
la procedencia de ambos manifests, backup exacto, hashes del objeto
y archive, y staging/commit/push solo de estas tres rutas; no
modifica firmware adicional, no realiza upload ni prueba física.

Registro detallado:
[A13_G8_PRODUCT_ADOPTION_PASS_20261011.md](A13_G8_PRODUCT_ADOPTION_PASS_20261011.md).

---

## G8 — D0 confirma Git limpio; F122/F123 por rutas Windows resueltos en harness (2026-10-11)

```text
ALPHA13_STATUS=IN_PROGRESS
G7C=CLOSED_PASS
G8_P0=CLOSED_PASS_READ_ONLY
G8_P1P2=CANDIDATE_BUILD_PASS_PRODUCT_UNCHANGED
G8_D0_RUN_ID=g8_d0_20261011_192744_4cb40613
G8_D0_CLEAN=True
G8_D0_INDEX_TRACKS_ALL=True
G8_D0_INDEX_FLAGS=H_H_H
F122_ROOT_CAUSE=WINDOWS_PATH_BACKSLASH_VS_GIT_POSIX_SLASH
F123_ROOT_CAUSE=WINDOWS_PATH_BACKSLASH_VS_GIT_POSIX_SLASH
G8_GIT_SCOPE_FIX=PATH_RELATIVE_TO_ROOT_AS_POSIX
G8_OLD_P1P2_MANIFEST_KEYS=WINDOWS_LEGACY_PRESERVED
G8_GIT_SCOPE_SELFTEST_WIN_LINUX=PASS_RUN_38188441089
G8_ADOPTION_PRODUCT_WINDOWS=NOT_YET_RETRIED_AFTER_FIX
G8_PRODUCT_COMMIT=NO
G8_PHYSICAL_RS485=NOT_EXECUTED_PENDING_DECISION
A13-010=CANDIDATE_VALIDATED_NOT_ADOPTED
A13-011=CANDIDATE_VALIDATED_NOT_ADOPTED
G8=IN_PROGRESS_NOT_CLOSED
NEXT_GATE=RUN_G8_ADOPTION_RETRY_WITH_VERSIONED_BAT
RELEASE_MERGE_PR_PUBLICATION=NO
```

El D0 del operador confirma que Git sí rastrea las tres rutas
en HEAD con marcas H y worktree limpio. Una prueba CI Windows
del mismo helper reprodujo el falso negativo:
las rutas generadas por `str(Path.relative_to(ROOT))` usaban
backslash y las rutas de Git, slash. Se corrigió la
comparación a `Path.relative_to(ROOT).as_posix()` para
Git **sin transformar las claves heredadas del manifiesto P1/P2
generado en Windows**. El test del helper real con los tres
archivos modificados pasó en
[CI 38188441089](https://github.com/JW-Control/platform-jwplc/actions/runs/38188441089)
tanto Linux como Windows. Los intentos anteriores restauraron
correctamente producto o se detuvieron antes de adoptar.
No se considera G8 cerrado por esto: requiere adopción
productiva con compilaciones reales, revisión de prueba
física y commit exacto fuente/archive.

Registro de fallos:
[A13_TOOLING_FAILURES_AND_PREVENTION_20261006.md](A13_TOOLING_FAILURES_AND_PREVENTION_20261006.md),
F122 y F123 con reconciliación posterior.

---

## G8 — Segundo intento: índice Git no reconocido en PREFLIGHT (2026-10-11)

```text
G8_ADOPTION_ATTEMPT_2=g8_adopt_20261011_192425_df5a28db
G8_ADOPT_PHASE=PREFLIGHT_RUNNING
G8_ADOPT_REASON=INDEX_NOT_TRACKING_ALL_PRODUCT_FILES
G8_PRODUCT_BACKUP=NOT_STARTED
G8_PRODUCT_ADOPTION=NOT_STARTED
G8_PRODUCT_REGRESSIONS=NOT_STARTED
G8_PRODUCT_COMMIT=NONE
G8_PRODUCT=UNCHANGED_IN_THIS_ATTEMPT
G8_ROOT_CAUSE=UNKNOWN_PENDING_WINDOWS_GIT_INDEX_DIAGNOSTIC
NEXT_GATE=G8_D0_READ_ONLY_GIT_INDEX_DIAGNOSTIC
```

El nuevo preflight detuvo antes del backup al no poder reconocer
las tres rutas mediante `git ls-files -v`. El log no incluye
las rutas devueltas ni el motivo. No asumir que los tres archivos
hayan desaparecido, que el usuario usó comandos incorrectos,
ni marcar PASS sin diagnosticar. El nuevo gate D0 de solo lectura
registrará `ls-files -v` normal/NUL, `ls-files --stage`,
`ls-tree HEAD`, objetos blobs de HEAD/working tree,
`git status`, `git diff`, diferencias de caso y origen
de las tres rutas. No modifica producto, índice ni configuración.
No repetir la adopción hasta revisar la evidencia D0.

---

## G8 — Intento de adopción detenido y rollback PASS; harness diagnóstico corregido (2026-10-11)

```text
ALPHA13_STATUS=IN_PROGRESS
G7C=CLOSED_PASS
G8_P0=CLOSED_PASS_READ_ONLY
G8_P1P2=CANDIDATE_BUILD_PASS_PRODUCT_UNCHANGED
G8_ADOPTION_ATTEMPT=g8_adopt_20261011_192007_949944b4
G8_ADOPTION_PRECHECK=PASS
G8_ADOPTION_BACKUP=PASS
G8_ADOPTION_SCOPE=REVIEW_STOP_DIRTY_SCOPE_NOT_EXACT_THREE_FILES
G8_ADOPTION_ROLLBACK=PASS
G8_PRODUCT_REGRESSIONS_AFTER_ADOPTION=NOT_EXECUTED
G8_PRODUCT_COMMIT_PUSH=NONE
G8_PRODUCT_STATUS=UNCHANGED_AFTER_ROLLBACK
G8_INDEX_FLAG_ROOT_CAUSE=NOT_YET_DETERMINED
G8_SCOPE_RUNNER=DIAGNOSTICS_ENHANCED_VERSIONED
G8_SCOPE_CI_THREE_FILES=PASS_RUN_38187763080
G8_NEXT=RETRY_CONTROLLED_ADOPTION_WITH_EXACT_GIT_DIAGNOSTICS
A13-010=CANDIDATE_VALIDATED_NOT_ADOPTED
A13-011=CANDIDATE_VALIDATED_NOT_ADOPTED
F120_ROOT_CAUSE=UNKNOWN_PENDING
RELEASE_MERGE_PR_PUBLICATION=NO
```

Operador informó `G8_ADOPT_STATUS=REVIEW_STOP_ROLLBACK` antes de
compilaciones productivas, con recuperación completa. La
causa exacta del mismatch no se puede deducir de ese log.
Se corrigió el harness para emitir rutas Git observadas y
esperadas, marcas de índice y diferencia de bytes con `HEAD`;
no se eliminó la exigencia de los tres archivos ni se hizo
commit/push de producto. CI del nuevo auditor sobre los
tres archivos (incluyendo archive binario) obtuvo PASS.

Detalle:
[A13_G8_ADOPTION_FIRST_ATTEMPT_REVIEW_20261011.md](A13_G8_ADOPTION_FIRST_ATTEMPT_REVIEW_20261011.md).

---

## G8 — P1/P2 candidato PASS y adopción controlada preparada (2026-10-11)

```text
ALPHA13_STATUS=IN_PROGRESS
G1_G7=CLOSED_PASS
G7C=CLOSED_PASS
G8_P0=CLOSED_PASS_READ_ONLY
G8_P1P2=CANDIDATE_BUILD_PASS_PRODUCT_UNCHANGED
G8_P1P2_RUN_ID=g8_p1p2_20261011_190500_d59b4022
G8_SOURCE_OBJECT_ARCHIVE_PARITY=PASS
G8_RTU_CANDIDATE_ARCHIVE_SHA256=f239100fbb6770a3a6f336179fbfa3a4afd27cc16e12c6f239cf05f28f7a1416
G8_CANDIDATE_COMPILES=PASS_SLAVE_MASTER_RS485_STATUS
G8_CANDIDATE_RTU_LINK=PASS_SLAVE_MASTER_EXACT_PATH_FLAG_SHA
G8_CANDIDATE_FINAL_AUDIT=PASS_PRODUCT_UNCHANGED
G8_NATIVE_CI_20_CASES=PASS
G8_ADOPTION_RUNNER=tools/alpha13/gates/a13_g8_adopt_local.py
G8_ADOPTION_WRAPPER=tools/alpha13/gates/run_a13_g8_adopt.bat
G8_ADOPTION_RUNNER_CI=PASS_RUN_38187430145
G8_ADOPTION_WINDOWS=NOT_EXECUTED
G8_PRODUCT_COMMIT_PUSH=NO
G8_PHYSICAL_RS485=NOT_EXECUTED_DECISION_PENDING
A13-010=CANDIDATE_VALIDATED_NOT_ADOPTED
A13-011=CANDIDATE_VALIDATED_NOT_ADOPTED
G8=IN_PROGRESS_NOT_CLOSED
NEXT_GATE=G8_CONTROLLED_PRODUCT_ADOPTION
F120_ROOT_CAUSE=UNKNOWN_PENDING
RELEASE_MERGE_PR_PUBLICATION=NO
```

El candidato generó archive RTU con SHA exacto y 3/3 regresiones
sin tocar producto. El siguiente hito **no** realizará una
publicación Git: su ejecutor respaldará tres archivos productivos,
los reemplazará solo si coinciden todas las evidencias y compilará
de nuevo tres consumidores con bibliotecas normales del package
(`--libraries` sin overlay, object RS485 + `-L/-l` RTU),
con rollback ante fallo. Antes del cierre Git de G8 se revisará
el riesgo de validación física del nuevo estado UART/RS485.

Documentos:
[A13_G8_P1P2_CANDIDATE_RESULTS_20261011.md](A13_G8_P1P2_CANDIDATE_RESULTS_20261011.md),
[A13_G8_PRODUCT_ADOPTION_PLAN_20261011.md](A13_G8_PRODUCT_ADOPTION_PLAN_20261011.md).

---

## G8-P0 cerrado; G8-P1/P2 candidato aislado y CI nativo PASS (2026-10-11)

```text
ALPHA13_STATUS=IN_PROGRESS
G7C=CLOSED_PASS
G8_P0=CLOSED_PASS_READ_ONLY
G8_P0_RUN_ID=g8_p0_20261011_185223_e0c4e0cc
G8_P0_NORMAL_COMPILES=PASS_3_OF_3
G8_P0_RTU_PRECOMPILED_LINK=PASS_SLAVE_MASTER
G8_A13-010=DEFECT_CONFIRMED_CANDIDATE_NOT_ADOPTED
G8_A13-011=DEFECT_CONFIRMED_CANDIDATE_NOT_ADOPTED
G8_P1_CANDIDATE_SOURCE=VERSIONED_UNDER_TOOLS_ONLY
G8_P1_NATIVE_TESTS=PASS_20_OF_20_CI_RUN_38186444071
G8_P2_RUNNER=tools/alpha13/gates/a13_g8_p1_p2_candidate.py
G8_P2_WRAPPER=tools/alpha13/gates/run_a13_g8_p1_p2_candidate.bat
G8_P2_RUNNER_SYNTAX_AND_NATIVE_CI=PASS_RUN_38186574868
G8_P2_WINDOWS_BUILD_REGRESSIONS=NOT_EXECUTED
G8_PRODUCT_CHANGED=NO
G8_ARCHIVE_PRODUCT_UPDATED=NO
G8_USB_UPLOAD_OR_FRAM_ACCESS=NONE
G8_RELEASE_MERGE_PR_PUBLICATION=NO
G8_NEXT_ACTION=RUN_LOCAL_G8_P1P2_CANDIDATE
```

P0 fue ejecutado en Windows por operador con preflight, tres
compilaciones normales y auditoría final PASS. G8-P1/P2 dispone
de candidatos fuente no productivos, verificación de delta exacto
y 20 fixtures C++ nativos PASS en CI; el runner preparará
`libJWPLC_ModbusRTU.a` en TEMP con paridad de objeto y enlace
normal de sketches sin alterar producto. Los fixtures **no son**
evidencia de runtime eléctrico; el test real Windows sigue
pendiente. No iniciar adopción de producto por CI aislada.

Documentos:
[A13_G8_P0_BASELINE_CLOSURE_20261011.md](A13_G8_P0_BASELINE_CLOSURE_20261011.md) y
[A13_G8_P1P2_CANDIDATE_PLAN_20261011.md](A13_G8_P1P2_CANDIDATE_PLAN_20261011.md).

---

## G8-P0 — Ejecutable read-only preparado y CI PASS (2026-10-11)

```text
ALPHA13_STATUS=IN_PROGRESS
G7C=CLOSED_PASS
A13-009=CLOSED_PASS
A13-013=CLOSED_PASS
G8_SCOPE=A13-010_MODBUS_RTU_RX_BYTES+A13-011_RS485_UART_READY
G8_P0_RUNNER=tools/alpha13/gates/a13_g8_p0_baseline.py
G8_P0_WRAPPER=tools/alpha13/gates/run_a13_g8_p0_baseline.bat
G8_P0_CI_SOURCE_STATIC=PASS_RUN_38185801023
G8_P0_WINDOWS_COMPILE_RUN=NOT_EXECUTED
G8_P0_PRODUCT_CHANGE=NO
G8_P0_USB_SERIAL_FRAM_ACCESS=NO
G8_P0_GIT_COMMIT_PUSH=NO
NEXT_GATE=A13-G8_P0_MODBUS_RTU_RS485_BASELINE_RUN
F120_ROOT_CAUSE=UNKNOWN_PENDING
RELEASE_MERGE_PR_PUBLICATION=NO
```

Baseline P0 preparado bajo `tools/alpha13/gates`: preflight
rama/HEAD, CLI 1.5.1, fuentes/bibliotecas/archives, contrato exacto
de contadores RTU y readiness RS485, compilaciones normales Slave,
Master y RS485 Status; registrar SHA, linker `-L/-lJWPLC_ModbusRTU`
y objeto `JWPLC_RS485.cpp.o`. Sin acceso físico, sin Git de producto.
La [CI G8 P0](https://github.com/JW-Control/platform-jwplc/actions/runs/38185801023)
ha validado sintaxis y detección estática sobre el código fuente real.
Aún falta ejecutar las compilaciones normales en el Windows del operador.
Plan: [A13_G8_P0_MODBUS_RTU_RS485_BASELINE_PLAN_20261011.md](A13_G8_P0_MODBUS_RTU_RS485_BASELINE_PLAN_20261011.md).

---

## G7C — Cierre canónico FRAM productivo verificado (2026-10-11)

```text
ALPHA13_STATUS=IN_PROGRESS
G1=CLOSED_PASS
G2=CLOSED_PASS
TFT_CLOSURE=CLOSED_PASS
G3=CLOSED_PASS
G4=CLOSED_PASS
G5=CLOSED_PASS
G6=CLOSED_PASS
G7_FRAM=CLOSED_PASS
G7C_FRAM_CANONICAL=CLOSED_PASS
A13-009=CLOSED_PASS
A13-013=CLOSED_PASS
G7C_PRODUCT_COMMIT=f2a93a8e5cbb736bbaa98ded9c7d2845582b7b32
G7C_PRODUCT_PUSH=PASS_VERIFIED_REMOTE
G7C_GIT_RUN_ID=g7c_git_20261011_184501_0e9245c3
G7C_ADOPTION_RUN_ID=g7c_adopt_20261011_182119_efba4050
G7C_OFFLINE_RUN_ID=g7c_20261011_175217_3b6759cd
G7C_FRAM_ARCHIVE_SHA256=df390f3379da13c6d4503a9fb83bc098f2944590fe3ad083348e95bfdcaf7299
G7C_REGRESSIONS=PASS_2_OF_2
G7C_NATIVE_TESTS=PASS_11_OF_11
G7C_CI_POST_PRODUCT_COMMIT=PASS_RUN_38185454578
G7C_RUNTIME_PHYSICAL_RETEST=NOT_EXECUTED_NOT_REQUIRED_BY_CURRENT_DECISION
G7C_FRAM_WRITE=NONE
G7C_RELEASE_MERGE_PR_PUBLICATION=NO
F120_ROOT_CAUSE=UNKNOWN_PENDING
G8_A13-010=PENDING
G8_A13-011=PENDING
G8_P0=NOT_EXECUTED
NEXT_GATE=A13-G8_P0_MODBUS_RTU_RS485_BASELINE
OPENPLC=OUT_OF_SCOPE
HMI_DESIGNER=OUT_OF_SCOPE
TFT_NEW_FEATURES=OUT_OF_SCOPE
```

GitHub confirmó el commit productivo `f2a93a8` con SOLO
`JW_FRAM.cpp`, `JW_FRAM.h` y `libJW_FRAM.a`; CI posterior PASS.
No confundir este cierre técnico con ZIP/PreRelease publicado.
Se preservan el cierre original G7 y la causa desconocida de F120.
Documento detallado: [A13_G7C_FRAM_CLOSURE_20261011.md](A13_G7C_FRAM_CLOSURE_20261011.md).

**Precedencia:** este estado al inicio del documento reemplaza los
`G7C=NOT_ADOPTED` y `NEXT_GATE=G7C_OFFLINE_INTEGRATED` históricos
que permanecen debajo para trazabilidad. Abrir G8 únicamente por P0
read-only; no modificar todavía código productivo de Modbus RTU/RS485.

---

## G7C — Adopción local controlada preparada, NO ejecutada (2026-10-11)

```text
G7C_CANDIDATE_OFFLINE=PASS
G7C_NATIVE_CI_11_OF_11=PASS
G7C_ADOPTION_RUNNER=tools/alpha13/gates/a13_g7c_adopt_local.py
G7C_ADOPTION_WRAPPER=tools/alpha13/gates/run_a13_g7c_adopt.bat
G7C_ADOPTION_RUNNER_STATUS=VERSIONED_NOT_EXECUTED_WINDOWS
G7C_ADOPTION_SOURCE_RUN=g7c_20261011_175217_3b6759cd
G7C_ADOPTION_ARCHIVE_SHA256=df390f3379da13c6d4503a9fb83bc098f2944590fe3ad083348e95bfdcaf7299
G7C_ADOPTION_MODE=BACKUP_THEN_3_PRODUCT_FILES_THEN_2_REGRESSIONS
G7C_ON_FAILURE=ROLLBACK_AND_REVIEW_STOP
G7C_ON_PASS=PRODUCT_ADOPTED_REGRESSIONS_PASS_PENDING_COMMIT
G7C_UPLOAD_OR_FRAM_WRITE=NO
G7C_GIT_COMMIT_OR_PUSH=NO
G7C_NEXT_GATE=RUN_LOCAL_PRODUCT_ADOPTION
A13-009=CANDIDATE_VALIDATED_NOT_ADOPTED
A13-013=CANDIDATE_VALIDATED_NOT_ADOPTED
G8=NOT_STARTED
```

El ejecutor usa exclusivamente las fuentes y el archive de la corrida
offline identificada y valida SHA de sus artefactos, CI nativo, baseline
productivo, core/TFT y rama. Respalda los tres archivos en TEMP; ante
fallo posterior al respaldo restaura bytes y hashes. Tras adoptar exige
dos compilaciones productivas con enlace del archive nuevo, sin modificar
el firmware grabado, sin tocar FRAM y sin `git add/commit/push`.

**No marcar G7C cerrado** antes de recibir el resultado Windows y
decidir el commit productivo exacto. No iniciar G8.

---

## G7C — Candidato validado offline y nativo, NO adoptado (2026-10-11)

```text
G1_G7=CLOSED_PASS
G7C_WINDOWS_RUN_ID=g7c_20261011_175217_3b6759cd
G7C_WINDOWS_HEAD=20858de6d25bb23428488800f0f4e03d2fb846d5
G7C_SOURCE_CONTRACT=PASS
G7C_REBUILD_ARCHIVE_LINK=PASS
G7C_REGRESSIONS=PASS_2_OF_2
G7C_FINAL_AUDIT=PASS_PRODUCT_UNCHANGED
G7C_NATIVE_WINDOWS=NOT_AVAILABLE_PENDING_IN_THAT_RUN
G7C_NATIVE_GITHUB_ACTIONS=PASS_11_OF_11
G7C_NATIVE_CI_RUN=38183543776
G7C_NATIVE_CI_SHA=cd5172d99fb9d0ade76e3b36f106436cc4c08434
G7C_SOURCE_OBJECT_SHA256=2d635936aeb0096a7e3dd8051e2c4a23e56dc863922f0a114cc3b2b56323366c
G7C_CANDIDATE_ARCHIVE_SHA256=df390f3379da13c6d4503a9fb83bc098f2944590fe3ad083348e95bfdcaf7299
G7C_CANDIDATE_VALIDATION=PASS
A13-009=CANDIDATE_VALIDATED_NOT_ADOPTED
A13-013=CANDIDATE_VALIDATED_NOT_ADOPTED
G7C_PRODUCT_FILES=UNCHANGED
G7C_PRODUCT_COMMIT=NO
G7C_PHYSICAL_UPLOAD_OR_FRAM_ACCESS=NONE
G7C_NEXT_GATE=ADOPTION_WITH_ROLLBACK_AND_PRODUCT_REGRESSIONS
G8=NOT_STARTED
F120_POST_UPLOAD_FREEZE_CAUSE=UNKNOWN_PENDING
RELEASE_MERGE_PR_PUBLICATION=NO
```

Evidencia Windows: ejecución enviada por el operador; reconstrucción fuente→objeto→archive,
enlace `-L/-lJW_FRAM`, regresiones `GLOBAL_FRAM` y `CONFIG_BLOCK`,
auditoría final sin escrituras de producto. La prueba C++ nativa no estuvo
disponible en Windows y **no** se contabiliza como PASS de ese entorno.

Evidencia nativa independiente: [GitHub Actions G7C #1](https://github.com/JW-Control/platform-jwplc/actions/runs/38183543776),
compilación Linux g++ del cuerpo productivo extraído con 11/11 escenarios PASS,
`G7C_NATIVE_CPP=PASS_ACTUAL_EXTRACTED_METHOD`. El mock necesitó conservar el
valor predeterminado de `maxLen`, corregido solo en tooling (`a97e908`).
Workflow de pruebas en `cd5172d`.

El candidato tiene evidencia suficiente para **preparar** adopción controlada:
copiar fuentes + archive exacto con backup, compilar regresiones contra el archive
productivo y revisar hashes, Git y rollback. No actualizar aún A13-009/013 a
`CLOSED_PASS`, ni iniciar G8, hasta validar esa adopción y cerrar Git.

---

# v2.1.0-alpha.13 — Estado operativo y continuidad

## G7C — Candidato canónico preparado, pendiente de ejecución offline (2026-10-11)

```text
G1_G7=CLOSED_PASS
G7C_SCOPE=A13-009_readString+A13-013_private_cleanup
G7C_CANDIDATES=VERSIONED_NOT_ADOPTED
G7C_SRC_CPP=tools/alpha13/candidates/g7c/JW_FRAM.cpp
G7C_SRC_H=tools/alpha13/candidates/g7c/JW_FRAM.h
G7C_SOURCE_CONTRACT=READY_NOT_EXECUTED
G7C_NATIVE_RUNTIME=READY_NOT_EXECUTED
G7C_OBJECT_ARCHIVE_REBUILD=READY_NOT_EXECUTED
G7C_CANDIDATE_LINK_REGRESSIONS=READY_NOT_EXECUTED
G7C_NORMAL_PRODUCT_FILES=UNCHANGED
G7C_FRAM_PHYSICAL=NOT_REQUESTED
G7C_GIT_PRODUCT_COMMIT=NO
G7C_GIT_PUSH_PRODUCT=NO
A13-009=IN_PROGRESS_NOT_CLOSED
A13-013=IN_PROGRESS_NOT_CLOSED
NEXT_GATE=G7C_OFFLINE_INTEGRATED
RELEASE_MERGE_PR_PUBLICATION=NO
```

El cierre G7 ya confirmado no reemplaza A13-009/013.
Se preparó nuevo candidato mínimo y runner offline de
fuente→objeto→archive→enlace→2 regresiones; la
prueba unitaria nativa extrae el cuerpo C++ real y
tiene 11 escenarios. El resultado debe interpretarse
como candidato técnico, NO cierre productivo G7C.
Plan: `A13_G7C_FRAM_CANONICAL_PLAN_20261011.md`.

---


## Conciliación canónica G7 y preparación G8 (2026-10-11)

```text
G7_TECHNICAL_GATE=CLOSED_PASS
G7_COMMIT_PRODUCT=a166d08e4eb1467c6237ba6196b687f381554b44
G7_COMMIT_DOCS=cd3fe89d2dbf882bbdb6b8a04f2f7d1d916df16a
A13-009=PENDING_CANONICAL_FIX_readString_length_SPI_failure
A13-013=PENDING_CANONICAL_CLEANUP_FRAM_ctors_debugPrint
G7_CANONICAL_MAPPING=RECONCILED_WITH_PENDING_SCOPE
G8_SCOPE=MODBUS_RTU_RX_COUNTER_AND_RS485_UART_READY
G8_A13-010=PENDING
G8_A13-011=PENDING
G8_P0=READ_ONLY_PLAN_PREPARED_NOT_EXECUTED
NEXT_ACTION=DECIDE_G7_CANONICAL_DELTA_PRIORITY_VS_G8_P0
F120_POST_UPLOAD_FREEZE_CAUSE=UNKNOWN_PENDING
RELEASE_MERGE_PR_PUBLICATION=NO
```

El hito G7 cerrado es válido para lo que implementó y probó,
pero la auditoría canónica exige además dos correcciones
FRAM aún ausentes: `readString()` no distingue error
leyendo longitud del texto vacío; y las dos sobrecargas
privadas + guards de constructores siguen en el producto.
No confundir `G7=CLOSED_PASS` con
`A13-009/013=RESOLVED`. Ver
`A13_G7_CANONICAL_RECONCILIATION_AND_G8_SCOPE_20261011.md`.

G8 debe limitarse a estadísticas RX de Modbus RTU y
readiness de RS-485, no a Serial USB genérico.
No se cambia firmware/producto en esta revisión.

---


## Estado vigente — G7 cerrado, G8 siguiente (2026-10-11)

ALPHA13_STATUS=IN_PROGRESS
G1=CLOSED_PASS
G2=CLOSED_PASS
G3=CLOSED_PASS
G4=CLOSED_PASS
G5=CLOSED_PASS
G6=CLOSED_PASS
G7_FRAM=CLOSED_PASS
G7_RUN_ID=g7_close_20261011_173351_a4c16676
G7_PHYSICAL_RUN_ID=g7_20261011_164228_e5e86ddc
G7_FRAM_RESTORED=PASS
G7_F120_POST_UPLOAD_CAUSE=UNKNOWN_PENDING
G7_ARCHIVE_SHA256=477ba0ccc3b20e638eccd7dda7d22b209d1ce73003df34917dc9ea7d476d83dd
G7_SOURCE_ARCHIVE_PARITY=PASS
G7_A13_009_013_CANONICAL_MAPPING=REVIEW_PENDING
NEXT_GATE=A13-G8_SERIAL_A13_010_A13_011
NEXT_FAILURE_ID=F122
RELEASE_MERGE_PR_PUBLICATION=NO

Cierre: A13_G7_FRAM_CLOSURE_20261011.md

---


## G7 — F121 corregido en harness, regeneración documental pendiente (2026-10-11)

```text
ALPHA13_STATUS=IN_PROGRESS
G1_G6=CLOSED_PASS
G7_FRAM_PHYSICAL=PASS
G7_FRAM_RESTORED=PASS
G7_PRODUCT_REGRESSIONS=PASS_2_OF_2
G7_FINAL_AUDIT=PASS
G7_CLOSE_PREVIOUS=REVIEW_STOP_KEYERROR_product_regressions
G7_CLOSE_ROLLBACK=PASS
G7_CLOSE_PRODUCT_COMMIT=NO
G7_CLOSE_DOCS_COMMIT=NO
G7_CLOSE_PUSH=NO
G7_F121_FIX=VERSIONED_NOT_EXECUTED_WINDOWS
G7_DOCUMENT_SELFTEST=PREPARED_NOT_EXECUTED
G7_NEW_UPLOAD_REQUIRED=NO
G7_PHYSICAL_RETEST_REQUIRED=NO
G7_ARCHIVE_SHA256=477ba0ccc3b20e638eccd7dda7d22b209d1ce73003df34917dc9ea7d476d83dd
G7_F120_POST_UPLOAD_FREEZE_CAUSE=UNKNOWN
NEXT_FAILURE_ID=F122
NEXT_GATE=A13_G7_CLOSE_FROM_PHYSICAL_PASS_RETRY
RELEASE_MERGE_PR_PUBLICATION=NO
```

El cierre R4 falló sólo al leer una clave de manifiesto
mal nombrada mientras generaba los documentos, después de
regresiones y auditoría PASS. Se restauró todo el producto,
no hubo cambios Git ni nuevas operaciones físicas.
El runner y un test real de generación documental ya se
versionaron para impedir este error al final del hito.

---


## G7 — PHYSICAL_PASS con NVS restaurado, falta cierre productivo (2026-10-11)

```text
BRANCH=v2.1.0-alpha.13/feature/cleanup-robustness
ALPHA13_STATUS=IN_PROGRESS
G1_G6=CLOSED_PASS
G7_PHYSICAL_RUN_ID=g7_20261011_164228_e5e86ddc
G7_UPLOAD=PASS_PREVIOUS_ATTEMPT
G7_APP_SHA256=30cb8c596d212b6209e3fb179c1b9f216588c2fc83225834dfb2bce7088c663a
G7_PHYSICAL=PASS_BACKUP_RESTORE_CHECKSUM_NULL_BOUNDS
G7_FRAM_STALE_RECOVERY=1
G7_FRAM_BACKUP_PASS=1
G7_FRAM_NULL_AND_BOUNDS_ERRORS=0
G7_FRAM_VALID_BLOCK_ERRORS=0
G7_FRAM_CORRUPT_BLOCK_ERRORS=0
G7_FRAM_VERSION_ERRORS=0
G7_FRAM_RESTORE_PASS=1
G7_FRAM_TOTAL_ERRORS=0
G7_FRAM_DONE=PASS_EXACT_TOKEN
G7_FRAM_RECOVERY_PROOF=PHYSICAL_PASS_JSON_CREATED
G7_NEW_UPLOAD_REQUIRED=NO
G7_ADOPTION_AND_PRODUCT_REGRESSIONS=PENDING
G7_GIT_CLOSURE=PENDING
G7_CLOSE_FROM_PHYSICAL_PASS=PREPARED_NOT_EXECUTED
F120_CAUSE_POST_UPLOAD_FREEZE=UNKNOWN
G7_CANONICAL_A13_009_013_MAPPING=REVIEW_PENDING
NEXT_FAILURE_ID=F121
RELEASE_MERGE_PR_PUBLICATION=NO
```

El operador ejecutó una única orden `G7_FRAM_RUN` sobre
firmware existente, recibida tras `READY` por COM4 con
DTR/RTS desactivados. La FRAM restauró los 128 bytes originales,
sin diferencias y con 0 errores. La prueba no repetirá
el acceso a FRAM. Se preparó cierre desde evidencia en
`a13_g7_close_from_physical_pass.py`, que comprueba
manifiesto, hashes y logs; sólo después adopta dos fuentes y
`libJW_FRAM.a` y ejecuta regresiones Arduino IDE.
**G7 no está CLOSED_PASS hasta terminar regresiones y Git.**
F120 sigue con causa de freeze post-upload no demostrada.

---


## G7 — F120 power-cycle recuperado y UART READY probado; físico pendiente (2026-10-11)

```text
ALPHA13_STATUS=IN_PROGRESS
G1_G6=CLOSED_PASS
G7_R3_SOURCE_AND_ARCHIVE=PASS
G7_R3_CANDIDATE_COMPILE_AND_LINK=PASS
G7_R3_PHYSICAL_UPLOAD=PASS
G7_R3_READY_TIMEOUT=REVIEW_STOP
G7_POWER_CYCLE_AFTER_FAILURE=OPERATOR_DISCONNECTED_AND_RECONNECTED
G7_TFT_RTC_AFTER_POWER_CYCLE=NORMAL_OPERATOR_OBSERVATION
G7_UART_PASSIVE=29_READY_EXACT_IN_20_SECONDS
G7_UART_PANIC_REBOOT_MARKERS=0
G7_FIRMWARE_TOKEN=g7_20261011_164228_e5e86ddc
G7_APP_SHA256=30cb8c596d212b6209e3fb179c1b9f216588c2fc83225834dfb2bce7088c663a
G7_FRAM_RUN_COMMAND=NOT_YET_SENT
G7_FRAM_BACKUP=NOT_YET_STARTED
G7_FRAM_RESTORE=NOT_YET_TESTED
G7_PHYSICAL_ONLY_RECOVERY=PREPARED_NOT_EXECUTED
G7_REFLASH=NOT_REQUIRED
G7_PRODUCT_ADOPTION=NO
F120_SYMPTOM=RECOVERED_AFTER_POWER_CYCLE_CAUSE_UNKNOWN
NEXT_FAILURE_ID=F121
NEXT_GATE=A13_G7_PHYSICAL_ONLY_SAME_FIRMWARE_TOKEN
RELEASE_MERGE_PR_PUBLICATION=NO
```

Tras el primer timeout del ensayo físico G7, el operador
desconectó/reconectó el JWPLC. La TFT volvió a actualizar RTC.
El puerto COM4, abierto en modo pasivo DTR=RTS=False,
recibió 29 veces `G7_FRAM_READY` con el token exacto,
sin señales de panic ni reboot. Esto confirma
`setup()`/`loop()` tras el arranque frío, pero **no explica**
el bloqueo transitorio posterior a la subida.

Se modificó el host serial para abrir COM4 con DTR/RTS desactivados,
igual que la captura pasiva exitosa, y se versionó una
reanudación sólo física del sketch ya cargado, sin reconstrucción
ni flash. El ensayo que escribe FRAM aún NO se ha ejecutado;
no hay evidencia de bytes alterados por G7.
El resultado `fram_restored=False` del intento anterior significa
**sin validación porque no se envió G7_FRAM_RUN**, no un
fallo demostrado de restauración.

---


## G7 — F120: upload PASS, READY_TIMEOUT y TFT detenida; UART pasiva pendiente (2026-10-11)

```text
ALPHA13_STATUS=IN_PROGRESS
G1_G6=CLOSED_PASS
G7_SOURCE_ARCHIVE_BUILD=PASS
G7_CANDIDATE_LINK_AND_REGRESSIONS=PASS
G7_PHYSICAL_BUILD=PASS
G7_UPLOAD=PASS
G7_FIRMWARE_SHA256=30cb8c596d212b6209e3fb179c1b9f216588c2fc83225834dfb2bce7088c663a
G7_FRAM_SERIAL=REVIEW_READY_TIMEOUT
G7_TFT_CLOCK=STOPPED_AT_16_46_54_OPERATOR_OBSERVATION
G7_FRAM_TEST_COMMAND=NOT_SENT
G7_FRAM_RESTORE_VERIFIED=NO_NOT_RUN
G7_PRODUCT_ADOPTION=NO
G7_GIT_CLOSE=NO
G7_PHYSICAL_CAUSE=UNKNOWN
F120=POST_UPLOAD_READY_TIMEOUT
F120_PASSIVE_SERIAL_DIAG=VERSIONED_NOT_EXECUTED
NEXT_FAILURE_ID=F121
NEXT_GATE=A13_G7_UART_READONLY_DIAGNOSIS
DO_NOT_REFLASH_OR_RESET_BEFORE_SERIAL_EVIDENCE=YES
RELEASE_MERGE_PR_PUBLICATION=NO
```

No repetir G7 ni declarar CLOSED_PASS. Primero captar pasivamente
UART de COM4 a 115200, sin transmitir `G7_FRAM_RUN`, sin reset
del ESP32 y sin sobrescribir firmware/NVS. El core sí incluye un
`jwplcSystemTask` para pantalla y RTC independiente del sketch.
La coincidencia reloj detenido + READY_TIMEOUT merece diagnóstico de
arranque, serial o tareas, sin atribución causal prematura.
Documento detallado: `A13_G7_POST_UPLOAD_READY_TIMEOUT_20261011.md`.

---


## G7 — F119 enlace FRAM verificado; reanudación preparada (2026-10-11)

```text
ALPHA13_STATUS=IN_PROGRESS
G1_G6=CLOSED_PASS
G7_R2_SOURCE=PASS
G7_R2_ARCHIVE=PASS
G7_R2_ARCHIVE_SHA256=477ba0ccc3b20e638eccd7dda7d22b209d1ce73003df34917dc9ea7d476d83dd
G7_R2_CANDIDATE_COMPILE=PASS
G7_R2_LINK_FLAGS=-L_CANDIDATE_AND_-lJW_FRAM
G7_R2_REVIEW_REASON=HARNESS_ARCHIVE_LITERAL_MISSING
G7_R2_PRODUCT_MUTATION=NO
G7_R2_FIRMWARE_UPLOAD=NO
G7_R2_FRAM_WRITE=NO
G7_R3_REUSE_FROM_RUN=g7_20261011_162621_6ca7668e
G7_R3_RUNNER=VERSIONED_NOT_EXECUTED
G7_R3_START_PHASE=CANDIDATE_LINK
G7_R3_GIT_AUTO_CLOSE=ONLY_AFTER_ALL_PASS
NEXT_FAILURE_ID=F120
NEXT_GATE=A13_G7_RESUME_CANDIDATE_ARCHIVE
RELEASE_MERGE_PR_PUBLICATION=NO
```

El linker recibió `-lJW_FRAM` y el directorio
`-L` de `TEMP/candidate/JW_FRAM/src/esp32`; el problema fue
exigir que apareciera el literal `libJW_FRAM.a`.
El harness ahora valida ambas opciones, existencia y SHA del
archive exacto. La nueva autoprueba ejecuta casos negativos reales.
La reanudación reutiliza la fuente y archive generados, con
manifiesto/hashes del intento detenido: no prueba aún G7 en hardware.
El respaldo NVS permanece sin usar porque la FRAM no fue escrita.

---


## G7 — F118 harness corregido; prueba física aún pendiente (2026-10-11)

```text
ALPHA13_STATUS=IN_PROGRESS
G1_G6=CLOSED_PASS
G7_R1_PREFLIGHT=PASS
G7_R1_F112=PASS
G7_R1_CONTRACT_SELFTEST=REVIEW_HARNESS_WRONG_LABEL
G7_R1_SOURCE_BUILD=NOT_EXECUTED
G7_R1_FRAM_UPLOAD=NOT_EXECUTED
G7_R1_FRAM_WRITE=NONE
G7_R1_PRODUCT_ADOPTION=NO
G7_R1_GIT_COMMITS=NO
G7_F118_FIX=VERSIONED_NOT_RETESTED
NEXT_FAILURE_ID=F119
NEXT_GATE=A13_G7_FRAM_INTEGRATED_RETRY_AFTER_HARNESS_FIX
RELEASE_MERGE_PR_PUBLICATION=NO
```

F118 se debió a buscar una cadena ficticia
`regression(cli,FRAM,"PRODUCT")` mientras el runner contiene
`regression(cli,FRAM,"PRODUCT_REGRESSIONS")`.
Se corrigió el autocontrato y se hicieron idempotentes los
directorios de trabajo reutilizados; la clasificación de fallos
de la autoprueba ahora es HARNESS. No declarar G7 PASS por
la revisión: requiere nueva ejecución en el banco.

---


## G7 — candidato e integración listos; pendiente de ensayo en PC (2026-10-11)

```text
ALPHA13_STATUS=IN_PROGRESS
G1_G6=CLOSED_PASS
G7=FRAM_PROBE_SOURCE_ARCHIVE_INTEGRATED_READY
G7_OPERATOR_FRAM_WRITE_CONSENT=YES
G7_SAFE_WINDOW=0x1F00..0x1F7F
G7_WINDOW_BACKUP=NVS_RESTORE_WITH_BYTE_COMPARE
G7_CANDIDATE_NULL_GUARDS=VERSIONED_NOT_APPLIED
G7_CANDIDATE_BLOCK_CHECKSUM_PRESERVE_OUTPUT=VERSIONED_NOT_APPLIED
G7_SOURCE_TO_OBJECT_ARCHIVE_LINK=RUNNER_PREPARED_NOT_EXECUTED
G7_APP_PHYSICAL=NOT_EXECUTED
G7_PRODUCT_ADOPTION=NO
G7_AUTO_GIT_CLOSE=ONLY_AFTER_PASS
G7_NEXT_FAILURE_ID=F118
G7_CANONICAL_009_013_MAPPING=REVIEW_PENDING
NEXT_GATE=A13_G7_FRAM_INTEGRATED
RELEASE_MERGE_PR_PUBLICATION=NO
```

El operador confirmó que la FRAM física puede usarse para ensayos.
Se prepararon candidato, prueba física con respaldo persistente
en NVS, archivo `.a` generado desde objeto y ejecutor integrado.
La ventana 0x1F00..0x1F7F se lee, respalda en NVS antes de modificar y
se restaura/verifica tras la prueba. Una marca `pending` permite
recuperación si ocurre reinicio; si la recuperación falla, se detiene
G7 y queda el respaldo NVS para análisis.

Sin cambios productivos hasta superar reconstrucción, enlace, carga,
serial y restauración; no se solicita `AUTORIZO_COMMIT_G7`.
Requiere `BANCO_FRAM` únicamente por seguridad de actuadores
antes de reemplazar el sketch. G7 no equivale a cierre semántico
de A13-009/013 sin disponer del hallazgo canónico original.

---


## Estado vigente — G7 FRAM preflight preparado, producto intacto (2026-10-11)

```text
BRANCH=v2.1.0-alpha.13/feature/cleanup-robustness
ALPHA13_STATUS=IN_PROGRESS
G1_DNS=CLOSED_PASS
G2_TCA_STARTUP=CLOSED_PASS
TFT_CLOSURE=CLOSED_PASS
G3_TCA_RMW=CLOSED_PASS
G4_TFT_BATCH=CLOSED_PASS
G5_TCP=CLOSED_PASS
G6_UDP=CLOSED_PASS
G6_PRODUCT_COMMIT=575144f49f1a392615ee3e0c1b9e137202ff532d
G6_DOCS_COMMIT=17c37355e851bcc4306b8dd737eedfa45eef05b2
G7=A13-009_PLUS_A13-013_FRAM
G7_PREP_COMMIT=d90401c79dd69b9d54a0844ac43038bcf3a123a9
G7_PREFLIGHT=VERSIONED_NOT_EXECUTED
G7_ARCHIVE_MODE=PRECOMPILED_FULL
G7_PRODUCT_CHANGED=NO
G7_PHYSICAL=NOT_EXECUTED
G7_FRAM_DATA_POLICY=PRESERVE_UNTIL_SAFE_TEST_WINDOW_IS_DEFINED
G7_SOURCE_ARCHIVE_REBUILD=PENDING
NEXT_GATE=A13_G7_FRAM_PREFLIGHT_AND_CANDIDATE_QUALIFICATION
NEXT_FAILURE_ID=F118
OPENPLC=OUT_OF_SCOPE
HMI_DESIGNER=OUT_OF_SCOPE
TFT_NEW_FEATURES=OUT_OF_SCOPE
RELEASE_MERGE_PR_PUBLICATION=NO
```

G6 ha cerrado con prueba física, regresiones y push sin fuerza;
no hay que repetirlo. G7 requiere especial tratamiento por
`JW_FRAM` `precompiled=full` y `libJW_FRAM.a`: el siguiente
cambio productivo debe incluir build fuente→objeto→archive→link
y verificar un espacio seguro antes de cualquier escritura FRAM.
Se versionó el plan
`A13_G7_FRAM_PREPARATION_PLAN_20261011.md` y un preflight que
solo compila, sin upload ni escritura FRAM. A13-009/A13-013 no
se declaran corregidos sin contrastar alcance con auditoría.

---


## Estado vigente — G6 cerrado, G7 siguiente (2026-10-11)

ALPHA13_STATUS=IN_PROGRESS
G1=CLOSED_PASS
G2=CLOSED_PASS
TFT_CLOSURE=CLOSED_PASS
G3=CLOSED_PASS
G4=CLOSED_PASS
G5=CLOSED_PASS
G6=CLOSED_PASS
G6_RUN_ID=20261011_150515_c7ab9c7e
G6_PHYSICAL=PASS_UDP_2048_TX_256_PARTIAL_16_RECOVERED_REOPEN
G6_REGRESSIONS=3_OVERLAY_PASS_3_PRODUCT_PASS
G6_PRODUCT_FILES=2
G6_A13_008=REVIEW_MAPPING_PENDING
NEXT_GATE=A13-G7_FRAM_A13_009_A13_013
NEXT_FAILURE_ID=F115
OPENPLC=OUT_OF_SCOPE
HMI_DESIGNER=OUT_OF_SCOPE
TFT_NEW_FEATURES=OUT_OF_SCOPE
RELEASE_MERGE_PR_PUBLICATION=NO

Cierre: A13_G6_CLOSURE_20261011.md
Checklist: A13_G6_CLOSURE_CHECKLIST_20261011.md

---


## G6 — MTU confirmado y reanudación sin upload preparada (2026-10-11)

```text
ALPHA13_STATUS=IN_PROGRESS
G1=CLOSED_PASS
G2=CLOSED_PASS
G3=CLOSED_PASS
G4=CLOSED_PASS
G5=CLOSED_PASS
G6_NETWORK_R2=REVIEW_EXPECTED_ONE_DATAGRAM
G6_UPLOAD_R2=PASS_ARDUINO_CLI_1_5_1
G6_TX_DIAG=PASS_SPLIT_1472_576_CONCAT_EXACT
G6_MTUSTREAM_SEMANTICS=W5500_2_UDP_MESSAGES
G6_PRODUCT_MUTATION=NO
G6_GIT_CLOSURE=NO
G6_RESUME_RUNNER=VERSIONED_NOT_EXECUTED
G6_RESUME_UPLOAD=NOT_REQUIRED
F117=CONFIRMED_HARNESS_FIX_AWAITING_FULL_RETEST
NEXT_FAILURE_ID=F118
NEXT_GATE=A13_G6_RESUME_UPLOADED
RELEASE_MERGE_PR_PUBLICATION=NO
```

Los 2048 bytes se recibieron en dos datagramas UDP distintos,
1472+576, ambos desde la IP del JWPLC y con bytes concatenados
exactos. WIZnet documenta la división automática por MTU.
La prueba PC admitía erróneamente sólo un paquete de 2048 bytes.
Reanudación preparada reutilizando el mismo firmware/token,
sin nueva compilación de overlay ni upload.

---


## G6 — F117 diagnóstico UDP TX tras upload PASS (2026-10-11)

```text
ALPHA13_STATUS=IN_PROGRESS
G1=CLOSED_PASS
G2=CLOSED_PASS
G3=CLOSED_PASS
G4=CLOSED_PASS
G5=CLOSED_PASS
G6_R2_CLI=ARDUINO_CLI_OFFICIAL_1_5_1_PASS
G6_R2_COM4=PASS
G6_R2_BUILD=PASS
G6_R2_UPLOAD=PASS
G6_R2_PC_UDP=REVIEW_TX_PAYLOAD_MISMATCH
G6_DIAGNOSTIC=PREPARED_NO_REFLASH
G6_PRODUCT_MUTATION=NO
G6_COMMITS=NO
F115=PENDING_PHYSICAL_CLASSIFICATION
F116=RESOLVED_LAUNCHER_FIX
F117=OPEN_DIAGNOSTIC_PENDING
NEXT_FAILURE_ID=F118
NEXT_GATE=A13_G6_UDP_TX_DIAGNOSE
RELEASE_MERGE_PR_PUBLICATION=NO
```

El script de recepción G6 R2 no discrimina diferencia de origen,
longitud y contenido del datagrama. El firmware de prueba quedó
cargado correctamente y puede reutilizarse para recopilar evidencia
antes de cualquier cambio de producto o compilación nueva.

---


## G6 — F116: falsa ausencia del core en preflight PowerShell (2026-10-11)

```text
ALPHA13_STATUS=IN_PROGRESS
G1=CLOSED_PASS
G2=CLOSED_PASS
G3=CLOSED_PASS
G4=CLOSED_PASS
G5=CLOSED_PASS
G6_R1=REVIEW_COM4_OPEN_FAILED
G6_R2=NOT_STARTED_POWERSHELL_CORE_LIST_FALSE_NEGATIVE
G6_PRODUCT_MUTATION=NO
G6_GIT_COMMIT=NO
G6_PHYSICAL=NOT_EXECUTED
ARDUINO_CLI_OFFICIAL_1_5_1=USER_VERIFIED
JWPLC_LOCAL_CORE_2_1_0_DEV=USER_VERIFIED
COM4_RECHECK=NOT_EXECUTED
F115=OPEN_AWAITING_REAL_ENVIRONMENT_VALIDATION
F116=HARNESS_FALSE_NEGATIVE_FIX_PREPARED
NEXT_FAILURE_ID=F117
NEXT_GATE=A13_G6_COM4_DIAGNOSIS_AND_RETEST
RELEASE_MERGE_PR_PUBLICATION=NO
```

PowerShell aplicó `-notmatch` sobre las múltiples líneas de
`arduino-cli core list`, interpretando erróneamente que el core no
existía. No hubo compilación, upload, mutación de producto ni commit
en este intento. El CLI oficial y el core están reconocidos según el
diagnóstico anterior del operador; se retira el chequeo PowerShell
redundante. Pendiente confirmar COM4 antes de reintentar G6.

---


## G6 — CLI y COM4 en revisión (2026-10-11)

```text
ALPHA13_STATUS=IN_PROGRESS
G1=CLOSED_PASS
G2=CLOSED_PASS
G3=CLOSED_PASS
G4=CLOSED_PASS
G5=CLOSED_PASS
G6_R1_PREFLIGHT=PASS
G6_R1_SELFTEST=PASS
G6_R1_CANDIDATE_BUILD=PASS_3_OF_3
G6_R1_PHYSICAL_BUILD=PASS
G6_R1_UPLOAD=REVIEW_COM4_OPEN_PERMISSION_ERROR_13
G6_R1_CLI=ARDUINO_PLC_IDE_TOOLS_UNEXPECTED
G6_R1_PRODUCT_MUTATION=NO
G6_R1_GIT_COMMIT=NO
G6_R1_TCP_UDP_PHYSICAL=NOT_RUN
G6_CLI_POLICY=EXPLICIT_ARDUINO_IDE_CLI_REQUIRED
G6_RETEST=NOT_EXECUTED
F115=OPEN_CODE_FIX_VERSIONED
NEXT_FAILURE_ID=F116
NEXT_GATE=A13_G6_ENVIRONMENT_DIAGNOSE_THEN_RESUME
RELEASE_MERGE_PR_PUBLICATION=NO
```

El fallback que seleccionó `Arduino PLC IDE Tools` fue un error del
harness; la causa de `PermissionError(13)` de COM4 no está todavía
establecida. El runner ahora exige CLI por ruta absoluta, no acepta
aquel fallback y registra versión y SHA. No reejecutar G6 antes de
seleccionar el CLI correcto y diagnosticar el puerto. Mantener fuentes
productivas y G1–G5 intactos.

---


## Estado vigente — G6 listo para ejecutar gate integrado (2026-10-11)

```text
ALPHA13_STATUS=IN_PROGRESS
BRANCH=v2.1.0-alpha.13/feature/cleanup-robustness
G1_DNS=CLOSED_PASS
G2_TCA_STARTUP=CLOSED_PASS
TFT_CLOSURE=CLOSED_PASS
G3_TCA_RMW_SHADOW=CLOSED_PASS
G4_TFT_BATCH_OWNERSHIP=CLOSED_PASS
G5_TCP_CORRECTNESS=CLOSED_PASS
G6=A13-007_UDP_DATAGRAM_BOUNDS_AND_A13-008_REVIEW
G6_SOURCE_CANDIDATES=VERSIONED_OUTSIDE_PRODUCT
G6_RUNNER=VERSIONED_NOT_EXECUTED
G6_CONTRACT_SELFTEST=VERSIONED_NOT_EXECUTED
G6_HARDWARE=NOT_EXECUTED
G6_AUTO_CLOSE_GIT=ENABLED_AFTER_ALL_GATES_PASS
G6_BENCH_APPROVAL=REQUIRED_BEFORE_UPLOAD
G6_PRODUCT_FILES=NOT_MODIFIED
NEXT_GATE=A13_G6_INTEGRATED
NEXT_FAILURE_ID=F115
RELEASE_MERGE_PR_PUBLICATION=NO
OPENPLC=OUT_OF_SCOPE
HMI_DESIGNER=OUT_OF_SCOPE
TFT_NEW_FEATURES=OUT_OF_SCOPE
```

El ejecutor G6 incluye selftest de Python/Git (F112) y contrato
generador↔prueba↔cierre, compilación source-first, validación física
UDP PC↔JWPLC, rollback previo al primer commit, y cierre Git automático
sin la cadena `AUTORIZO_COMMIT_G6`. Solo solicita `BANCO_UDP` antes del
upload físico. Los paths productivos permanecen intactos hasta PASS.

**Esta preparación todavía no demuestra PASS de Arduino CLI ni de la
prueba física.** El operador debe ejecutar el hito en su entorno;
cualquier fallo queda REVIEW y no se reintenta ciegamente.

---


## Estado vigente — G6 preparación y cierre automatizado (2026-10-11)

```text
ALPHA13_STATUS=IN_PROGRESS
G1_DNS=CLOSED_PASS
G2_TCA_STARTUP=CLOSED_PASS
TFT_CLOSURE=CLOSED_PASS
G3=CLOSED_PASS
G4=CLOSED_PASS
G5=CLOSED_PASS
G5_CLOSURE_COMMIT=cc965e89311c7a54465818bf8fdda7cca1b59ff7
G6=A13-007_DATAGRAM_ROBUSTNESS_PLUS_A13-008_REVIEW
G6_PREPARATION=IN_PROGRESS
G6_PRODUCT_MUTATION=NOT_EXECUTED
G6_PHYSICAL_GATE=NOT_EXECUTED
G6_AUTO_GIT_CLOSURE=ENABLED_ONLY_AFTER_ALL_GATES_PASS
G6_FLASH_PERMISSION=BENCH_SAFETY_ONLY
G6_RELEASE_PUBLICATION=NO
NEXT_GATE=A13_G6_PREPARATION_AND_INTEGRATED_TEST
NEXT_FAILURE_ID=F115
OPENPLC=OUT_OF_SCOPE
HMI_DESIGNER=OUT_OF_SCOPE
TFT_NEW_FEATURES=OUT_OF_SCOPE
```

G5 ya está cerrado. Por petición expresa del operador se retira la
solicitud textual `AUTORIZO_COMMIT_GN`: el cierre Git en la rama
Alpha13 pasa a ser automático **solo con preflight, evidencia, hashes,
contratos y scope íntegramente PASS**. Conserva confirmación de banco
para cualquier upload físico. La política canónica está en
`docs/JWPLC_COLLABORATION_WORKFLOW.md`, sección 12.

---


## Estado vigente — G5 cerrado y G6 siguiente (2026-10-11)

El bloque es el estado vigente; los antiguos bloques G4/G5 preparado
permanecen como histórico.

ALPHA13_STATUS=IN_PROGRESS
G1_DNS=CLOSED_PASS
G2_TCA_STARTUP=CLOSED_PASS
TFT_CLOSURE=CLOSED_PASS
G3=CLOSED_PASS
G4=CLOSED_PASS
G5=CLOSED_PASS
G5_RUN_ID=20261010_175509_dbd543a2
G5_PHYSICAL=PASS_TCP_5000_TX_128_RX_64_SERVER
G5_REGRESSIONS=3_OF_3_OVERLAY_AND_3_OF_3_PRODUCT
G5_PRODUCT_FILES=2
G5_PRODUCT_COMMIT=c45b77f512fd5829da33c491fd512c0a810846b9
G5_TOOLING_COMMIT=dc2c9c59d68fe3b79e901a9103267951d065dd76
G5_SYNC_MERGE_COMMIT=699d8dc8a4d3281cc2fb144cb3fa9c337d9d712b
F112=CLOSED_PREVENTION_REGISTERED
F113=CLOSED_HARNESS_REMEDIATED
F114=CLOSED_HARNESS_REMEDIATED
NEXT_FAILURE_ID=F115
NEXT_GATE=A13-G6_DATAGRAM_ROBUSTNESS_A13_007_A13_008_REVIEW
OPENPLC=OUT_OF_SCOPE
HMI_DESIGNER=OUT_OF_SCOPE
TFT_NEW_FEATURES=OUT_OF_SCOPE

Cierre: A13_G5_CLOSURE_20261011.md.
Checklist: A13_G5_CLOSURE_CHECKLIST_20261011.md.
Alpha13 todavía no tiene merge release, PR ni PreRelease.

---


## G5 — F114 reparado, cierre Git todavía pendiente (2026-10-11)

```text
G1=CLOSED_PASS
G2=CLOSED_PASS
G3=CLOSED_PASS
G4=CLOSED_PASS
G5_PHYSICAL=PASS
G5_REGRESSIONS=PASS_3_OVERLAY_3_PRODUCT
G5_GIT_CLOSURE=PENDING
F113=PENDING_CLOSURE_RETEST
F114=FIX_VERSIONED_PENDING_RETEST
NEXT_FAILURE_ID=F115
CMAKE_BUILD_IGNORED=/build/
NEXT_GATE=A13_G5_GIT_CLOSURE
RELEASE_MERGE_PR_PUBLICATION=NO
```

El manifiesto G5 usa claves de producto; el finalizador ahora refleja
exactamente ese contrato. No repetir gate ni reflashear el hardware.

---

## Seguimiento G5 — prueba PASS, cierre Git F113 pendiente (2026-10-11)

```text
ALPHA13_STATUS=IN_PROGRESS
G1_DNS=CLOSED_PASS
G2_TCA_STARTUP=CLOSED_PASS
G3=CLOSED_PASS
G4=CLOSED_PASS
G5_PHYSICAL=PASS
G5_REGRESSIONS=3_OVERLAY_PASS_3_PRODUCT_PASS
G5_PRODUCT_FILES=2_LOCAL_MODIFIED_UNCOMMITTED
G5_FINALIZER_R1=REVIEW_STOP_UNTRACKED_FILES
G5_FINALIZER_R2=VERSIONED_NOT_EXECUTED
G5_CLOSURE=PENDING
F113=OPEN_FIX_VERSIONED_PENDING_RETEST
NEXT_FAILURE_ID=F114
NEXT_GATE=A13_G5_GIT_CLOSURE
RELEASE_MERGE_PR_PUBLICATION=NO
```

Las rutas untracked de la PC aún no fueron identificadas en la consola.
El finalizador corregido las conservará sin tocar; únicamente incorporará
los dos archivos Ethernet validados y la documentación autorizada.
No repetir las pruebas físicas de G5.

---


## Estado vigente — G5 preparado para validación integrada (2026-10-10)

**Prevalece sobre el bloque histórico previo en relación con G5.**

```text
BRANCH=v2.1.0-alpha.13/feature/cleanup-robustness
ALPHA13_STATUS=IN_PROGRESS
G1_DNS=CLOSED_PASS
G2_TCA_STARTUP=CLOSED_PASS
TFT_CLOSURE=CLOSED_PASS
G3_TCA_RMW_SHADOW=CLOSED_PASS
G4_TFT_BATCH_TASK_OWNERSHIP=CLOSED_PASS
F112=CLOSED_PREVENTION_REGISTERED
G5=A13-005_PLUS_A13-006_TCP_CORRECTNESS
G5_CANDIDATE=VERSIONED_OUTSIDE_PRODUCT
G5_PRODUCT_SOURCE_MUTATION=NOT_EXECUTED
G5_TCP_PHYSICAL=NOT_EXECUTED
G5_NORMAL_SOURCE_REGRESSIONS=NOT_EXECUTED
G5_INTEGRATED_TOOLING=VERSIONED_READY_TO_RUN
G5_PRODUCT_COMMIT=NO
NEXT_GATE=A13-G5-INTEGRATED-TCP-CORRECTNESS
NEXT_FAILURE_ID=F113
OPENPLC=OUT_OF_SCOPE
HMI_DESIGNER=OUT_OF_SCOPE
TFT_NEW_FEATURES=OUT_OF_SCOPE
```

Las fuentes de `JWPLC_Ethernet` permiten observar tres contratos TCP en riesgo:
`EthernetClient::write(size>SSIZE)`, `EthernetClient::read(size_t)`
y `EthernetServer::write` cuando no hay clientes o el envío falla.
**No se presenta como equivalencia verificada de cada ID A13-005/A13-006
con el informe canónico no disponible en la rama;** los cambios quedan
acotados al código comprobado y la homologación final de IDs debe
contrastarse antes del cierre G5.

El <u>único</u> gate integrado G5 usa overlay temporal de fuentes,
compilación Arduino CLI desde fuente, prueba de red con token serial y
LAN real, regresiones de producto y rollback; **sin commits automáticos**.
La fase física requiere PC y JWPLC en la misma red y equipo desconectado
de actuadores. Ver [A13_G5_INTEGRATED_PLAN_20261010.md](A13_G5_INTEGRATED_PLAN_20261010.md).

---


## Prevención F112 — cierre G4 y portabilidad Python/Git (2026-10-10)

```text
ALPHA13_STATUS=IN_PROGRESS
G1_DNS=CLOSED_PASS
G2_TCA_STARTUP=CLOSED_PASS
TFT_CLOSURE=CLOSED_PASS
G3=CLOSED_PASS
G4=CLOSED_PASS
G4_CLOSURE_COMMIT=bd1581035af09eff47a08cae959312f8eae1d06f
F110=CLOSED
F111=CLOSED
F112=CLOSED_PREVENTION_REGISTERED
F112_SCOPE=POWERSHELL_PYTHON_GIT_FINALIZER_PORTABILITY
F112_PORTABILITY_SELFTEST=VERSIONED
NEXT_GATE=A13-G5_TCP_CORRECTNESS_A13_005_A13_006
NEXT_FAILURE_ID=F113
OPENPLC=OUT_OF_SCOPE
HMI_DESIGNER=OUT_OF_SCOPE
TFT_NEW_FEATURES=OUT_OF_SCOPE
```

F112 documenta bloqueos de cierre debidos a invocación de Python,
estado HEAD local asumido y comparación byte a byte de textos Git
en Windows. La compilación, prueba física y cierre G4 permanecen PASS;
no hubo cambio productivo adicional por registrar esta prevención.
Consultar `A13_TOOLING_FAILURES_AND_PREVENTION_20261006.md` y
la sección 9 de `docs/JWPLC_COLLABORATION_WORKFLOW.md` antes de
preparar finalizadores G5+.

---


## Estado vigente — G4 cerrado (2026-10-10)

Este bloque es canonico y prevalece sobre las entradas anteriores.

ALPHA13_STATUS=IN_PROGRESS
G1_DNS=CLOSED_PASS
G2_TCA_STARTUP=CLOSED_PASS
TFT_CLOSURE=CLOSED_PASS
G3=CLOSED_PASS
G4=CLOSED_PASS
G4_RUN_ID=20261010_141745_907ce6dc
G4_PHYSICAL=50_OF_50_PASS
G4_ERRORS=0
G4_VISUAL=PASS_OPERATOR_REPORTED
G4_REGRESSIONS=4_OF_4_PASS
G4_TFT_ARCHIVE_SHA256=4c606ba66e29c1337d42450a29fd09fe76c6988f72aa51d3882b81db59f82ecc
G4_PRODUCT_COMMIT=46a695756ecae205f585e59bf8e129493fa51d91
G4_SYNC_MERGE_COMMIT=dae9e674b3bc1212210fe9ca780b3000c72cbab2
F111=CLOSED_HARNESS_REMEDIATED
NEXT_GATE=A13-G5_TCP_CORRECTNESS_A13_005_A13_006
NEXT_FAILURE_ID=F112
OPENPLC=OUT_OF_SCOPE
HMI_DESIGNER=OUT_OF_SCOPE
TFT_NEW_FEATURES=OUT_OF_SCOPE

Cierre: A13_G4_CLOSURE_20261010.md. Checklist: A13_G4_CLOSURE_CHECKLIST_20261010.md.
Sin publicacion ni merge a release.

---


## Estado vigente — G4 R2: F111 identificado/corregido, validación pendiente (2026-10-10)

```text
BRANCH=v2.1.0-alpha.13/feature/cleanup-robustness
ALPHA13_STATUS=IN_PROGRESS
G1_DNS=CLOSED_PASS
G2_TCA_STARTUP=CLOSED_PASS
TFT_CLOSURE=CLOSED_PASS
G3=CLOSED_PASS
G4_R1=REVIEW_HARNESS_PIN
G4_R2=REVIEW_HARNESS_PIN
G4_R2_LOCAL_BACKEND_MATCHES_OFFICIAL=YES
G4_R2_UPSTREAM_BACKEND_MATCHES_OFFICIAL=YES
G4_F111_ROOT_CAUSE=TRUNCATED_PIN_LITERAL
G4_PIN_FIX_VERSIONED=YES
G4_HISTORICAL_SHA_GUARD_ADDED=YES
G4_PRODUCT_CHANGED=NO
G4_PHYSICAL_UPLOAD=NOT_EXECUTED
G4_RETEST=NOT_EXECUTED
F111=OPEN_PENDING_G4_RETEST
NEXT_GATE=A13_G4_INTEGRATED_RETEST
NEXT_FAILURE_ID=F112
OPENPLC=OUT_OF_SCOPE
HMI_DESIGNER=OUT_OF_SCOPE
TFT_NEW_FEATURES=OUT_OF_SCOPE
```

Se comprobó que la cabecera de init instalada coincide por bytes SHA-256 con la fuente oficial del tag TFT_eSPI `V2.5.43` y con el pin del gate TFT-CLOSURE. El fallo era de nuestra constante G4, corregida en tooling, no del usuario. No reinstalar ni modificar `TFT_eSPI`. Al completar el gate integrado, cerrar G4 y F111 según evidencia.

---


## Estado vigente — G4 R1 bloqueado por diferencia de backend local (2026-10-10)

```text
ALPHA13_STATUS=IN_PROGRESS
G1_DNS=CLOSED_PASS
G2_TCA_STARTUP=CLOSED_PASS
TFT_CLOSURE=CLOSED_PASS
G3=CLOSED_PASS
G4_R1_PREFLIGHT=PASS
G4_R1_BACKEND_RECIPE=REVIEW_ENVIRONMENT_SHA_MISMATCH
G4_R1_LOCAL_BACKEND_INIT_SHA256=NOT_YET_OBSERVED
G4_R1_SOURCE_REBUILD=NOT_EXECUTED
G4_R1_PHYSICAL_UPLOAD=NOT_EXECUTED
G4_R1_PRODUCT_ADOPTION=NOT_EXECUTED
G4_BACKEND_RECOVERY=VERSIONED_READY_TO_RETEST
G4_PRODUCT_COMMIT=NO
NEXT_GATE=A13_G4_INTEGRATED_RETEST
NEXT_FAILURE_ID=F111
OPENPLC=OUT_OF_SCOPE
HMI_DESIGNER=OUT_OF_SCOPE
TFT_NEW_FEATURES=OUT_OF_SCOPE
```

La versión actual del gate compara variantes SHA fijadas y puede recuperar
la fuente original desde TFT_eSPI V2.5.43 oficial **solo en TEMP**,
sin sobrescribir archivos de usuario. No considerar G4 cerrado hasta
obtener compilaciones, serial físico, confirmación visual y auditoría de producto.

---

## Estado vigente — G4 A13-003 preparado (2026-10-10)

**Hito integrado en preparación:** esta entrada prevalece sobre el `NEXT_GATE` histórico de G3. El candidato está versionado **solo bajo tooling**, no se han cambiado fuentes ni binarios de producto.

```text
BRANCH=v2.1.0-alpha.13/feature/cleanup-robustness
ALPHA13_STATUS=IN_PROGRESS
G1_DNS=CLOSED_PASS
G2_TCA_STARTUP=CLOSED_PASS
TFT_CLOSURE=CLOSED_PASS
G3_TCA_RMW_SHADOW=CLOSED_PASS
G4=A13-003_TFT_BATCH_TASK_OWNERSHIP
G4_PRODUCT_CHANGE=NOT_EXECUTED
G4_ARCHIVE_REBUILD=NOT_EXECUTED
G4_PHYSICAL_TEST=NOT_EXECUTED
G4_TOOLING=VERSIONED_READY_TO_RUN
NEXT_GATE=A13_G4_INTEGRATED
NEXT_FAILURE_ID=F111
OPENPLC=OUT_OF_SCOPE
HMI_DESIGNER=OUT_OF_SCOPE
TFT_NEW_FEATURES=OUT_OF_SCOPE
```

Criterios y ejecutor: [A13_G4_INTEGRATED_PLAN_20261010.md](A13_G4_INTEGRATED_PLAN_20261010.md). Mantener G3 core.a y TFT startup sin cambios. El gate G4 debe reconstruir el archive TFT desde las fuentes candidatas y backend 2.5.43 aislado, comprobar enlace normal, ejecutar test con tareas FreeRTOS y requerir observación humana. No marcar G4 PASS hasta obtener evidencia del operador.

---


## Estado vigente — G3 A13-004 cerrado (2026-10-10)

**Prevalece sobre los bloques anteriores de G3.**

G1_DNS=CLOSED_PASS
G2_TCA_STARTUP=CLOSED_PASS
TFT_CLOSURE=CLOSED_PASS
G3_P0=PASS_BASELINE_RACE_REPRODUCED
G3_SOURCE_CORE_REBUILD=PASS
G3_NORMAL_PRECOMPILED_LINK=PASS
G3_REGRESSIONS=3_OF_3_PASS
G3_PHYSICAL=150_OF_150_PASS
G3_SERIAL_ERRORS=0
G3_FINAL_OUTPUT=0x00
G3=CLOSED_PASS
G3_RUN_ID=20261010_091949_a04d2223
G3_CORE_SHA256=a1a985f64c22838a1987d4280b8c1dc431a42d5789c6692cbfa36dfb6252246d
G3_PRODUCT_COMMIT=8b8b9a7b491feb92d8fc91f06ae75234771cde70
G3_SYNC_NOT_REQUIRED=NOT_REQUIRED
F110=CLOSED_HARNESS_REMEDIATED
NEXT_GATE=A13-G4_TFT_BATCH_TASK_OWNERSHIP
NEXT_FAILURE_ID=F111
OPENPLC=OUT_OF_SCOPE
HMI_DESIGNER=OUT_OF_SCOPE
TFT_NEW_FEATURES=OUT_OF_SCOPE

Documentos: [cierre G3](A13_G3_CLOSURE_20261010.md) y
[checklist G3](A13_G3_CLOSURE_CHECKLIST_20261010.md).
Sin publicación ni merge a release.

---

## Estado vigente — G3 integrado: candidata corregida, prueba pendiente (2026-10-10)

**Prevalece sobre los bloques anteriores de G3.** El intento inicial falló por siete firmas duplicadas generadas en las fuentes candidatas; no por TCA ni por el toolchain. Restauración local validada.

```text
ALPHA13_STATUS=IN_PROGRESS
BRANCH=v2.1.0-alpha.13/feature/cleanup-robustness
G1_DNS=CLOSED_PASS
G2_TCA_STARTUP=CLOSED_PASS
TFT_CLOSURE=CLOSED_PASS
G3_P0=PASS_BASELINE_RACE_REPRODUCED
G3_ATTEMPT_1=REVIEW_CANDIDATE_SYNTAX
G3_ATTEMPT_1_BUILD_OFFICIAL_CORE=FAIL
G3_ATTEMPT_1_ROOT_CAUSE=DUPLICATED_RETURN_TYPES_IN_GENERATED_CANDIDATE
G3_ATTEMPT_1_PRODUCT_FAILURE=NO_VALID_PRODUCT_CANDIDATE
G3_ATTEMPT_1_PHYSICAL=NOT_EXECUTED
G3_ATTEMPT_1_ROLLBACK=PASS
G3_ATTEMPT_1_ORIGINAL_CORE_SHA256=6f328eeb796091070c8d852a2c0e90f71047d8d2b5786fe3cd5079b2fb6ff983
G3_CANDIDATE_SIGNATURES_FIXED=YES
G3_HARNESS_STATIC_SIGNATURE_GUARD=ADDED
G3_RETEST=NOT_EXECUTED
G3_PRODUCT_COMMIT=NO
NEXT_GATE=A13-G3_INTEGRATED_RETEST
NEXT_FAILURE_ID=F111
OPENPLC=OUT_OF_SCOPE
HMI_DESIGNER=OUT_OF_SCOPE
TFT_NEW_FEATURES=OUT_OF_SCOPE
```

Causa confirmada por `source-Basic.log` compartido por el operador. No repetir P0. Los cambios de candidato y harness se documentan en [A13_G3_INTEGRATED_PLAN_20261010.md](A13_G3_INTEGRATED_PLAN_20261010.md). El siguiente intento solo debe ejecutarse con worktree limpio y con los actuadores desconectados. No dar por aprobada la prueba física hasta obtener evidencia serial y revisión humana.

---

## Estado vigente — G3 integrado preparado (2026-10-10)

**Nuevo estado operativo:** prevalece sobre el bloque anterior "G3-P0 preparado". G1/G2/TFT permanecen cerrados.

```text
BRANCH=v2.1.0-alpha.13/feature/cleanup-robustness
ALPHA13_STATUS=IN_PROGRESS
G1_DNS=CLOSED_PASS
G2_TCA_STARTUP=CLOSED_PASS
TFT_CLOSURE=CLOSED_PASS
G3_P0=PASS_BASELINE_RACE_REPRODUCED
G3_P0_SOURCE_CONTRACT=PASS
G3_P0_PHYSICAL=NOT_EXECUTED
G3_P0_HEAD=c33aff2bad3c3a94e988ae030ceb99ad241c9eef
G3_P0_HARDWARE_INTERLEAVING=NOT_MEASURED
G3_INTEGRATED_TOOLING=VERSIONED_READY_TO_RUN
G3_CANDIDATE_PRODUCT_COMMIT=NOT_EXECUTED
G3_CANDIDATE_SOURCE_ARCHIVE_VALIDATION=NOT_EXECUTED
G3_PHYSICAL=NOT_EXECUTED
NEXT_GATE=A13-G3_INTEGRATED_SOURCE_ARCHIVE_PHYSICAL
NEXT_FAILURE_ID=F111
OPENPLC=OUT_OF_SCOPE
HMI_DESIGNER=OUT_OF_SCOPE
TFT_NEW_FEATURES=OUT_OF_SCOPE
```

El usuario ejecutó `run_a13_g3_p0_tca_rmw_baseline.bat` con `STATUS=PASS_BASELINE_RACE_REPRODUCED`, `SEQUENTIAL_CONTROL=0x03`, `INTERLEAVED_AB_HARDWARE=0x02`, `INTERLEAVED_BA_HARDWARE=0x01`, `SHADOW_FALSE_NOOP_POSSIBLE=YES`, sin modificar producto. La prueba fue un **modelo determinista**, no una medición de hardware. La continuación no requiere más rondas manuales para P1: el gate integrado con candidata reversible, core.a, compilación normal y ensayo físico supervisado está en [A13_G3_INTEGRATED_PLAN_20261010.md](A13_G3_INTEGRATED_PLAN_20261010.md).

**Precaución física:** dos salidas de relé se conmutan; solo equipo con cargas/actuadores desconectados, y confirmación explícita. No usar físicamente si el entorno no está aislado. Producto permanece sin commit hasta pasar el gate.

---


## Estado vigente — G3-P0 preparado (2026-10-10)

**Prevalece para la próxima acción sobre los bloques cronológicos anteriores.** El cierre TFT permanece retenido.

```text
BRANCH=v2.1.0-alpha.13/feature/cleanup-robustness
ALPHA13_STATUS=IN_PROGRESS
G1_DNS=CLOSED_PASS
G2_TCA_STARTUP=CLOSED_PASS
TFT_CLOSURE=CLOSED_PASS
G3=A13-004_TCA_RMW_SHADOW_ATOMICITY
G3_P0_TOOLING=VERSIONED_READY_TO_RUN
G3_P0_RESULT=NOT_EXECUTED
G3_PRODUCT_CHANGE=NO
NEXT_GATE=A13-G3-P0_TCA_RMW_BASELINE
NO_UPLOAD=YES
NO_RECOMPILE_CORE=YES
OPENPLC=OUT_OF_SCOPE
HMI_DESIGNER=OUT_OF_SCOPE
TFT_NEW_FEATURES=OUT_OF_SCOPE
```

Ejecutor: `tools/alpha13/gates/run_a13_g3_p0_tca_rmw_baseline.bat`. Diseño y criterio de evidencia: [A13_G3_P0_TCA_RMW_BASELINE_PLAN_20261010.md](A13_G3_P0_TCA_RMW_BASELINE_PLAN_20261010.md). El resultado no se clasificará como PASS hasta recibir la salida ejecutada; en P0 se modela una intercalación, no se mide concurrencia física. No repetir TFT.

---


## Estado vigente — TFT-CLOSURE cerrado (2026-10-09)

**TFT_CLOSURE_CLOSED_PASS_CURRENT_20261009**. Este bloque prevalece sobre las entradas históricas P2A/P2B y NEXT_GATE=TFT-CLOSURE más abajo, preservadas para trazabilidad.

```text
BRANCH=v2.1.0-alpha.13/feature/cleanup-robustness
ALPHA13_STATUS=IN_PROGRESS
G1_DNS=CLOSED_PASS
G2_TCA_STARTUP=CLOSED_PASS
TFT_PRE6_P2A=CLOSED_PASS
TFT_PRE6_P2B_PHYSICAL=PASS
TFT_CLOSURE=CLOSED_PASS
TFT_VISUAL=PASS_OPERATOR_REPORTED
TFT_VIDEO_REVIEW=NOT_PERFORMED
TFT_REBUILD_FUNCTIONAL=PASS
TFT_REBUILD_BIT_IDENTICAL=NO
TFT_REGRESSION_NORMAL_BUILDS=4_OF_4_PASS
TFT_RUN_ID=20261009_220034_167fa2ad
TFT_ARCHIVE_SHA256=ab73b244c44ebd75d29a4eeb3cd97f5d18c08470f535eb16d55c2fdbf2310ff8
TFT_TOOLING_COMMIT=b85cb9c37cde39c33b56c56fd7ef325ca6670355
TFT_PRODUCT_COMMIT=e211ff6acc85b57f97bf7a9d28fbb91025cd0bae
TFT_SYNC_MERGE_COMMIT=dbcc68019590cc5a37c2042b09db5ef6ac131b77
NEXT_GATE=A13-G3-TCA_RMW_SHADOW_ATOMICITY
PRODUCT_WORKTREE_EXPECTED=CLEAN_AFTER_COMMIT
RELEASE_MERGE=NOT_EXECUTED
ALPHA13_PUBLICATION=NOT_EXECUTED
NEXT_FAILURE_ID=F111
OPENPLC=OUT_OF_SCOPE
HMI_DESIGNER=OUT_OF_SCOPE
TFT_NEW_FEATURES=OUT_OF_SCOPE
```

Fuentes: [cierre TFT](A13_TFT_CLOSURE_20261009.md), [evidencia física](A13_TFT_CLOSURE_PHYSICAL_EVIDENCE_20261009.md) y [checklist](A13_TFT_CLOSURE_CHECKLIST_20261009.md). No repetir gates de TFT ya cerrados. El próximo avance es G3.

---

Actualizado: 2026-10-09 — G2 / A13-002 CLOSED_PASS

> Fuente viva de continuidad del alpha. Un chat nuevo debe verificar el estado real
> del repositorio y continuar desde `NEXT_GATE`.

## Identidad

```text
RELEASE_VERSION=v2.1.0-alpha.13
BRANCH=v2.1.0-alpha.13/feature/cleanup-robustness
BASELINE_HEAD=20f1b66075dac04160311b7f40c939e88534073f
ALPHA12_STATUS=CLOSED_PUBLISHED
ALPHA13_STATUS=IN_PROGRESS
```

## Alcance

```text
PURPOSE=cleanup + technical debt + robustness + safe optimization
OPENPLC=OUT_OF_SCOPE
HMI_DESIGNER=OUT_OF_SCOPE
TFT_NEW_FEATURES=OUT_OF_SCOPE
```

## P0

```text
A13_P0=PASS_WITH_KNOWN_BASELINE_DEFECT
P0A=PASS
P0B=PASS
P0C=PASS
P0D1=PASS_WITH_KNOWN_BASELINE_DEFECT
P0D2=PASS
```

Known baseline defect:

```text
Adafruit_BusIO source fallback -> Wire.h unresolved
release/precompiled path affected=NO
```

## G1 — A13-001 DNS truncation

```text
G1_PRE1=PASS
G1_P1=PASS
G1_P2=PASS
G1_P3_ATTEMPT_1=REVIEW_ENVIRONMENT
G1_P3_R1=REVIEW_HARNESS
G1_P3_R2=REVIEW_HARNESS
G1_P3_R3=REVIEW_PRECONDITION
G1_P3_R4=REVIEW_HARNESS
G1_P3_R5=PASS
G1_STATUS=CLOSED_PASS
```

Candidato cerrado:

```text
FILE=JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/Dns.cpp
SHA256=08291b4b89274f014e1ce6073bd16303192e5bfe1b4e2fdeaa21d133fb783995
PRODUCT_COMMIT=8bc48b73074859d367b2d949e333f0b16c83cbd7
COMMIT_MESSAGE=fix(ethernet): evitar bloqueo DNS con respuestas truncadas
REMOTE_PUSH=PASS
WORKTREE_AFTER_COMMIT=CLEAN
```

Evidencia cerrada:

```text
BASELINE_TRUNCATED_LOOP_REPRODUCED=True
PATCH_COMPILE=PASS
VALID_DNS_PACKETS=3/3
TRUNCATED_PREFIXES=136/136
```

P3 intento 1:

```text
DNS_NORMAL_PATH_CONTRACT=True
COMPILE_EXIT=0
REPO_ETHERNET_SELECTED=True
DNS_SOURCE_OBJECT_COUNT=1
PORTS_VISIBLE=COM1
REQUESTED_PORT=COM4
UPLOAD_EXIT=1
PHYSICAL_REGRESSION=NOT_EXECUTED
PRODUCT_FAILURE=NO
ENVIRONMENT_FAILURE=YES
```

P3 R1 — wrapper de sintaxis:

```text
BAT_SYNTAX_PREFLIGHT=FAIL
ERROR=PowerShell recibió el token literal ^ antes de |
PHYSICAL_REGRESSION=NOT_EXECUTED
PRODUCT_FAILURE=NO
HARNESS_FAILURE=YES
```

Clasificación conforme al registro de fallos del proyecto:

```text
F091_APPLIES=YES
CHAT_AS_LONG_GATE_EDITOR=DO_NOT_REPEAT
VERSIONED_PS1_BAT_LOGS_STATUS=REQUIRED
```

P3 R2 — carrera READY serial:

```text
A13_GATE_SYNTAX=PASS
COMPILE_EXIT=0
REPO_ETHERNET_SELECTED=True
DNS_SOURCE_OBJECT_COUNT=1
UPLOAD_EXIT=0
CLIENT_EXIT=4
NB3_DNS_READY_TIMEOUT=YES
PHYSICAL_DNS_SEQUENCE=NOT_STARTED
PRODUCT_FAILURE=NO
HARNESS_FAILURE=YES
ENVIRONMENT_FAILURE=YES
```

Hipótesis planteada durante R2, posteriormente NO confirmada como causa raíz:

```text
probe READY = one-shot
gate delay after upload = 800 ms
client wait starts after serial open
posible pérdida del marcador READY
```

La evidencia posterior mostró que el JWPLC todavía no tenía la precondición
Ethernet completa (RJ45 + red con DHCP) durante esos intentos. Por tanto:

```text
R2_READY_RACE_AS_ROOT_CAUSE=NOT_PROVEN
PRODUCT_FAILURE=NO
LESSON=no promover una hipótesis temporal a causa raíz sin instrumentación
```

Corrección R3:

```text
probe Alpha13 propio
READY reemitido cada 500 ms mientras WAIT_COMMAND
sin cambios en Dns.cpp
sin cambios en API/producto
```

P3 R3 — READY no alcanzado:

```text
A13_GATE_SYNTAX=PASS
COMPILE_EXIT=0
REPO_ETHERNET_SELECTED=True
DNS_SOURCE_OBJECT_COUNT=1
UPLOAD_EXIT=0
CLIENT_EXIT=4
NB3_DNS_READY_TIMEOUT=YES
DNS_SEQUENCE_STARTED=NO
PRODUCT_FAILURE=NO
ROOT_CAUSE=PRECONDITION_NOT_YET_CLASSIFIED
```

La reemisión periódica elimina la hipótesis de marcador READY perdido.
R4 añade telemetría serial no invasiva de runtime Ethernet mientras READY=false:

```text
BEGIN_ATTEMPTED
READY
BUSY
runtimeState
lastError
diagnosticCode
```

Esto permite separar LINK_OFF / DHCP / SPI / HW / otro estado antes de
atribuir cualquier regresión a Dns.cpp.

Higiene de evidencia R4:

```text
BUILD_PATH=%TEMP%
RESULTS=logs locales ignorados por Git
GITHUB_DESKTOP=no debe listar objetos .o/.d/.bin/.elf/.map del build
```

Se recupera así el patrón de los gates físicos Alpha14, que compilaban fuera
del working tree.

P3 R4 — fallo del reader serial:

```text
A13_GATE_SYNTAX=PASS
COMPILE_EXIT=0
REPO_ETHERNET_SELECTED=True
DNS_SOURCE_OBJECT_COUNT=1
UPLOAD_EXIT=0
CLIENT_EXIT=4
SERIAL_READER_EXCEPTION=UnicodeEncodeError
CONSOLE_ENCODING=cp1252
INVALID_SERIAL_BYTE_DECODED_AS=U+FFFD
NB2_ETH_DIAG_CAPTURED=NO
PRODUCT_FAILURE=NO
HARNESS_FAILURE=YES
```

R5 usa un cliente Alpha13 propio. Los bytes seriales inválidos se representan
con escapes ASCII y la escritura a stdout aplica `backslashreplace`; un byte
de arranque corrupto ya no puede matar el thread de captura.

P3 R5 — cierre físico:

```text
A13_GATE_SYNTAX=PASS
DNS_SHA256=08291b4b89274f014e1ce6073bd16303192e5bfe1b4e2fdeaa21d133fb783995
DNS_NORMAL_PATH_CONTRACT=True
COMPILE_EXIT=0
REPO_ETHERNET_SELECTED=True
DNS_SOURCE_OBJECT_COUNT=1
UPLOAD_EXIT=0
CLIENT_EXIT=0
DUT_IP_EFFECTIVE=192.168.0.31
PC_DNS_SERVER_IP=192.168.0.4
RESULT_CODE=1
PROBE_FAILED=NO
SUCCESS_RESULT_IP=10.20.30.40
SUCCESS_DURATION_MS=1
SUCCESS_POLL_COUNT=5
TIMEOUT_DURATION_MS=453
TIMEOUT_POLL_COUNT=5663
DNS_BEGIN_HOLD_MAX_US=1460
DNS_POLL_HOLD_MAX_US=748
LOOP_GAP_MAX_US=644
SPI_LOCK_ERRORS=0
DNS_VALID_QUERY_COUNT=1
DNS_TIMEOUT_QUERY_COUNT=1
DNS_OTHER_QUERY_COUNT=0
NB3_DNS_CLIENT_PASS=YES
DIRTY_SCOPE_VALID=True
DIFF_CHECK_PASS=True
STATUS=PASS
PRODUCT_FAILURE=NO
HARNESS_FAILURE=NO
HARDWARE_FAILURE=NO
ENVIRONMENT_FAILURE=NO
```

Precondición física confirmada para este gate:

```text
USB/serial -> COM4
W5500/RJ45 -> LAN con DHCP
PC -> misma LAN
```

Cierre Git:

```text
FINAL_DIFF_AUDIT=PASS
STAGED_FILE_COUNT=1
STAGED_FILE=JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/Dns.cpp
GIT_DIFF_CACHED_CHECK=PASS
PRODUCT_COMMIT=8bc48b73074859d367b2d949e333f0b16c83cbd7
REMOTE_PUSH=PASS
G1=CLOSED_PASS
```

## Infraestructura G1-P3-R5

Versionada en commits de tooling/docs descendientes de `BASELINE_HEAD`. El gate
acepta esa cadena sólo si los archivos commiteados desde el baseline pertenecen
al allowlist de tooling/docs y confirma que `Dns.cpp` sigue sin commit:

```text
tools/alpha13/gates/common.ps1
tools/alpha13/gates/a13_g1_p3_dns_physical.ps1
tools/alpha13/gates/a13_g1_p3_dns_physical_client.py
tools/alpha13/gates/run_a13_g1_p3_dns_physical.bat
tools/alpha13/firmware/a13_g1_p3_dns_physical_probe/a13_g1_p3_dns_physical_probe.ino
tools/alpha13/results/.gitignore
tools/alpha13/results/.gitkeep
```

Contrato del gate:

```text
SerialPort explícito = prioridad 1
autodetección = sólo un único puerto distinto de COM1
sin candidato inequívoco = REVIEW_ENVIRONMENT
preflight de puerto fallido = no compilar / no subir / no abrir cliente
compile/upload/client/SUMMARY = logs persistentes por run
Dns.cpp = nunca stageado ni commiteado por el gate
wrapper BAT = sin pipes escapados entre cmd.exe y powershell -Command
topología = baseline ancestro + allowlist de commits tooling/docs
```

## NEXT_GATE

```text
NEXT_GATE=TFT-CLOSURE
NEXT_PHYSICAL_GATE=TFT-PRE6-P2B
OBJECTIVE=confirm boot-clean on physical JWPLC with normal product precompiled archive, then regress, finalize rebuild recipe and close
STATE=PRODUCT_ADOPTED_LOCALLY_UNCOMMITTED_AWAITING_PHYSICAL_GATE
PREREQUISITE=TFT_PRE6_P2A_CLOSED_PASS
P1A_R1=REVIEW_HARNESS_F109
P1A_R2=REVIEW_HARNESS_F109
P1A_R3=CLOSED_PASS
P1B=CLOSED_PASS
P2A=CLOSED_PASS
P2A_PRODUCT_FILES_CHANGED=3
PRODUCT_FIX_COMMITTED=NO
PRODUCTION_ARCHIVE_REFRESH=APPLIED_LOCAL_ONLY
NEXT_FAILURE_ID=F111
```

No modificar todavía código productivo de G2 hasta completar su preflight
dirigido y fijar el contrato del gate.

## G2 — A13-002 TCA startup / EN_IO

```text
G2_PRE1=PASS
G2_CLASSIFICATION=ROBUSTNESS_FIX
G2_PRIORITY=P0
G2_CONFIDENCE=HIGH
G2_STATUS=CLOSED_PASS
G2_P1_R2=PASS
G2_P2_R5=PASS
G2_P3=PASS
G2_P4=PASS
G2_P5=PASS
PRODUCT_CHANGE=COMMITTED_PUSHED
PRODUCT_COMMIT=6a585693
REMOTE_PUSH=PASS
WORKTREE_AFTER_COMMIT=CLEAN
```

Preflight read-only confirmado sobre HEAD:

```text
HEAD=690ca995bbced130f389b2432814fb85f7feb59b
SOURCE=JWPLC/2.1.0/cores/jwcontrol/peripherals_init.cpp
```

Hallazgos:

```text
EN_IO starts LOW=YES
TCA connection failure keeps EN_IO LOW=YES
five post-probe TCA configuration results ignored=YES
EN_IO goes HIGH after those calls regardless of result=YES
g_ioState.initialized becomes true before I2C/TCA completion=YES
JWPLC_IO.ready() consumes that initialized flag=YES
```

Operaciones cuyo resultado se ignora actualmente:

```text
1 TCA6424A_writeBank(bank1, 0x00)
2 TCA6424A_writeBank(bank2, 0x00)
3 TCA6424A_setBankDirection(bank0, 0xFF)
4 TCA6424A_setBankDirection(bank1, 0x00)
5 TCA6424A_setBankDirection(bank2, 0xFF)
```

Dependencia de build:

```text
normal jwplcbasic -> jwcontrol_precompiled_stub + precompiled/core/JWPLCBASIC/core.a
jwplcbasic source core direct at normal build=NO
core.a changed since Alpha13 baseline=NO
inherited Alpha12 core.a bytes=3042444
inherited Alpha12 core.a SHA256=78d0c0ab14f156b96116529e88872340d51877af40d24ba3559f6081e0bf34fb
```

Política obligatoria si A13-002 modifica `cores/jwcontrol`:

```text
SOURCE_CHANGE
-> ARCHIVE_INVALIDATED
-> SOURCE-FIRST PASS
-> REBUILD core.a
-> VERIFY normal jwplcbasic stub + archive
-> physical gate
-> product commit
```

G2-P1 debe reproducir baseline mediante instrumentación temporal y segura:

```text
5 failure legs + 1 normal/control leg
compile real peripherals_init.cpp from source using full jwplcbasic profile
do not mutate versioned core.a
temporary build under %TEMP%
restore any temporary source/boards.local mutation byte-for-byte
during failure legs intercept EN_IO HIGH request and keep physical EN_IO LOW
record requested EN_IO state, actual EN_IO state, operation result,
TCA register snapshot and JWPLC_IO.ready()
```

Criterio de reproducción baseline:

```text
for each injected failed operation:
OP_RESULT=FAIL
EN_IO_HIGH_REQUESTED=YES
EN_IO_ACTUAL=LOW   # safety interlock of harness
IO_READY=TRUE      # demonstrates current false-ready behavior
PRODUCT_FAILURE=REPRODUCED_BASELINE_DEFECT
```

La lectura de registros se conserva como evidencia, pero no se exige energizar
salidas para demostrar el defecto.

No implementar todavía el fix A13-002 hasta cerrar G2-P1.

Infraestructura G2-P1 versionada:

```text
tools/alpha13/gates/a13_g2_p1_tca_startup_baseline.ps1
tools/alpha13/gates/a13_g2_tca_startup_client.py
tools/alpha13/gates/run_a13_g2_p1_tca_startup_baseline.bat
tools/alpha13/firmware/a13_g2_tca_startup_probe/a13_g2_tca_startup_probe.ino
```

Salvaguardas:

```text
product source instrumentation=TEMPORARY_ONLY
source restored before physical uploads=REQUIRED
boards.local restored byte-for-byte=REQUIRED
versioned core.a mutation=FORBIDDEN
builds=%TEMP%
failure legs physical EN_IO=FORCED_LOW
control leg physical EN_IO=normal
```

G2-P1 R1 — evidencia:

```text
6/6 source-core compiles=PASS
source profile full jwplcbasic=PASS
peripherals_init.cpp count=1
stub core=0
versioned core.a linked=NO
source restore=PASS
boards.local restore=PASS
core.a SHA preserved=PASS
failure legs 1..5 contract=PASS
control leg contract=REVIEW
```

Las cinco piernas inyectadas reprodujeron de forma consistente:

```text
EN_IO_HIGH_REQUESTED=YES
IO_VIEW_READY=YES
OP_OK_MASK=30/29/27/23/15
```

El único fallo de contrato fue la pierna control porque `digitalRead(EN_IO)`
devolvió LOW pese a que el baseline había solicitado HIGH y toda la
configuración TCA era correcta. Esto se clasifica como F097: el harness usó
readback de pad para un GPIO configurado `GPIO_MODE_OUTPUT` sin input-enable.

R2 corrige sólo la observabilidad:

```text
EN_IO_OUTPUT_ENABLE <- GPIO_ENABLE_REG
EN_IO_OUTPUT_LATCH  <- GPIO_OUT_REG
EN_IO_PAD_READBACK  <- diagnóstico no contractual
PRODUCT_CHANGE=NO
```

G2-P1 R2 — cierre baseline:

```text
HEAD=0dd5e236604391c497839ae08452e507a131db78
A13_GATE_SYNTAX=PASS
COMPILE_LEGS=6/6 PASS
SOURCE_CORE=YES
STUB_CORE=NO
VERSIONED_CORE_A_LINKED=NO
FULL_JWPLCBASIC_PROFILE=YES
SOURCE_RESTORED=True
BOARDS_LOCAL_RESTORED=True
CORE_SHA256_PRESERVED=True
CONTROL_STEP_CONTRACT=PASS
FAULT_STEP_1_CONTRACT=PASS
FAULT_STEP_2_CONTRACT=PASS
FAULT_STEP_3_CONTRACT=PASS
FAULT_STEP_4_CONTRACT=PASS
FAULT_STEP_5_CONTRACT=PASS
STATUS=PASS
REASON=BASELINE_DEFECT_REPRODUCED_SAFELY
PRODUCT_FAILURE=REPRODUCED_BASELINE_DEFECT
HARNESS_FAILURE=NO
HARDWARE_FAILURE=NO
ENVIRONMENT_FAILURE=NO
```

Control normal:

```text
OP_OK_MASK=31
EN_IO_HIGH_REQUESTED=YES
EN_IO_OUTPUT_ENABLE=YES
EN_IO_OUTPUT_LATCH=HIGH
IO_VIEW_READY=YES
```

En las cinco fallas inyectadas el baseline conserva el defecto:

```text
EN_IO_HIGH_REQUESTED=YES
EN_IO_OUTPUT_LATCH=LOW   # interlock de seguridad del harness
IO_VIEW_READY=YES
PERIPHERALS_INITIALIZED=YES
```

Por tanto A13-002 deja de ser sólo riesgo estático: queda reproducido en
hardware con fault injection controlado.

Siguiente paso: diseñar el cambio mínimo productivo. Debe mantener `EN_IO`
en LOW y `JWPLC_IO.ready()==false` ante cualquiera de las cinco fallas, y
sólo declarar ready después de completar correctamente la configuración TCA.

No regenerar todavía `core.a`: primero debe pasar source-first el candidato.

Diseño G2-P2 fijado:

```text
PRODUCT_FILES=3
1 cores/jwcontrol/peripherals_init.cpp
2 cores/jwcontrol/jwplc_peripherals.cpp
3 cores/jwcontrol/jwplc_peripherals.h
PUBLIC_API_BREAK=NO
CORE_A_REFRESH=NOT_YET
```

Cambios mínimos:

```text
jwplcSystemInitState() -> IO ready=false
post-probe TCA ops -> every return checked
any failed op -> return with EN_IO still LOW
successful startup -> EN_IO HIGH -> settle -> peripheral init true -> IO ready=true
non-Basic path -> explicit IO ready=true after system state init
```

Identidad esperada del candidato local:

```text
peripherals_init.cpp blob=23efb3935a34e6b5649875b57c804e60538827cd
jwplc_peripherals.cpp blob=c3d53566d274e95b7dda111327140b5393f7db35
jwplc_peripherals.h blob=288667f1caa08142e2a155b8c85f24b2aa5beb44
staged=0
commit=NO
core.a SHA256=78d0c0ab14f156b96116529e88872340d51877af40d24ba3559f6081e0bf34fb
```

Infraestructura G2-P2:

```text
tools/alpha13/gates/apply_a13_g2_p2_candidate.ps1
tools/alpha13/gates/run_a13_g2_p2_apply_candidate.bat
tools/alpha13/gates/run_a13_g2_p2_apply_and_test.bat
tools/alpha13/gates/a13_g2_p2_tca_startup_candidate.ps1
tools/alpha13/gates/a13_g2_tca_startup_candidate_client.py
tools/alpha13/gates/run_a13_g2_p2_tca_startup_candidate.bat
tools/alpha13/firmware/a13_g2_tca_startup_candidate_probe/a13_g2_tca_startup_candidate_probe.ino
```

Contrato source-first:

```text
control:
attempt=31
ok=31
EN_IO latch=HIGH
peripherals initialized=YES
IO ready=YES

fault step N:
attempt=(1<<N)-1
ok=(1<<(N-1))-1
EN_IO high requested=NO
EN_IO latch=LOW
peripherals initialized=NO
IO ready=NO
```

El gate vuelve a compilar las seis piernas desde `cores/jwcontrol` con el
perfil completo `jwplcbasic`, preserva el `core.a` versionado y restaura la
instrumentación temporal al candidato exacto antes de cualquier upload.

G2-P2 R1 — REVIEW_HARNESS antes de ejecutar producto:

```text
APPLY_SYNTAX=PASS
APPLY_RESULT=FAIL
APPLY_REASON=Get-FileHash unavailable in local PowerShell environment
PATCH_APPLIED=NO
PRODUCT_CHANGE_EXECUTED=NO

CANDIDATE_GATE_SYNTAX=FAIL
PHYSICAL_GATE_EXECUTED=NO
PRODUCT_FAILURE=NO
HARNESS_FAILURE=YES
```

Corrección R2:

```text
SHA helper -> Get-A13Sha256 from versioned common.ps1
Get-FileHash dependency -> removed
duplicate malformed contract block in candidate gate -> removed
BAT parser preflight -> retained
candidate still must start from clean baseline
```

G2-P2 R2 — REVIEW_HARNESS:

```text
APPLY_SYNTAX=PASS
BASELINE_BLOBS=PASS
CORE_ARCHIVE_SHA=PASS
GIT_APPLY_CHECK=FAIL
PATCH_APPLIED=NO
PRODUCT_CHANGE_EXECUTED=NO

CANDIDATE_GATE_SYNTAX=PASS
CANDIDATE_GATE_STATUS=REVIEW
REASON=UNEXPECTED_HEAD_TOPOLOGY
UNEXPECTED_COMMITTED=
  docs/v2.1.0-alpha.13/A13_TOOLING_FAILURES_AND_PREVENTION_20261006.md
  tools/alpha13/gates/run_a13_g2_p2_apply_candidate.bat
PRODUCT_FAILURE=NO
HARNESS_FAILURE=YES
```

Corrección R3:

```text
git apply candidate mechanism -> removed
candidate application -> deterministic exact-once transforms
baseline blob guard -> retained
candidate blob guard -> retained
core.a SHA guard -> retained
allowlist -> completed with all G2-P2 tooling/docs paths
combined apply+test runner -> added
test cannot start if apply fails
```
G2-P2 R3 — REVIEW_HARNESS:

```text
A13_APPLY_SYNTAX=PASS
CANDIDATE_BLOB_MISMATCH=YES
FILE=jwplc_peripherals.cpp
ACTUAL=2e54c7950e2d23db2c19548e56f5b81fd483f92f
EXPECTED=875a50fd64552e8c4a4300e07b9494d2c32605d7
CATCH_RESTORE=EXECUTED
PHYSICAL_GATE_EXECUTED=NO
PRODUCT_FAILURE=NO
HARNESS_FAILURE=YES
```

Diagnóstico byte-a-byte:

```text
wrapper forced powershell.exe (PowerShell legacy) + UTF-8 without BOM
todavía -> todavÃ­a
están   -> estÃ¡n
MOJIBAKE_VARIANT_BLOB=2e54c7950e2d23db2c19548e56f5b81fd483f92f
MATCHES_OBSERVED_ACTUAL=YES
```

Corrección R4:

```text
product-generating PS1 literals -> ASCII-only
new expected jwplc_peripherals.cpp blob=c3d53566d274e95b7dda111327140b5393f7db35
candidate bytes are generated and hashed under %TEMP% first
repo product files are not touched unless all 3 temp blobs match
combined BAT parses common + apply + physical gate before apply
```

Entorno PowerShell confirmado por el desarrollador:

```text
PSEdition=Core
PSVersion=7.6.6
EXECUTABLE=pwsh
```

Regla aplicada desde G2-P2-R4:

```text
material BAT wrappers -> require pwsh / PowerShell Core 7+
no silent fallback to powershell.exe
wrapper prints resolved pwsh version before parser/apply/gate
```

G2-P2 R4 — REVIEW_HARNESS por parser de evidencia:

```text
PWSH_VERSION=7.6.6
CANDIDATE_APPLY=PASS
DIRTY_COUNT=3
STAGED_COUNT=0
6/6 COMPILE=PASS
SOURCE_CORE=True
STUB_CORE=False
ARCHIVE_LINKED=False
FULL_PROFILE=True
6/6 UPLOAD=PASS
6/6 CLIENT_EXIT=0
FAULT_STEPS_1_3_4_5 CONTRACT=PASS
STEP0/STEP2 FALSE_NEGATIVE=YES
PRODUCT_FAILURE=NO
HARNESS_FAILURE=YES
```

Evidencia visible del producto en R4:

```text
CONTROL:
attempt=31
ok=31
EN_IO high requested=YES
EN_IO latch=HIGH
peripherals initialized=YES
IO ready=YES

FAULT 1:
attempt=1
ok=0
EN_IO high requested=NO
EN_IO latch=LOW
peripherals initialized=NO
IO ready=NO

FAULT 2:
attempt=3
ok=1
EN_IO high requested=NO
EN_IO latch=LOW
peripherals initialized=NO
IO ready=NO

FAULT 3:
attempt=7
ok=3
EN_IO high requested=NO
EN_IO latch=LOW
peripherals initialized=NO
IO ready=NO

FAULT 4:
attempt=15
ok=7
EN_IO high requested=NO
EN_IO latch=LOW
peripherals initialized=NO
IO ready=NO

FAULT 5:
attempt=31
ok=15
EN_IO high requested=NO
EN_IO latch=LOW
peripherals initialized=NO
IO ready=NO
```

R5 cambia sólo el harness: el cliente emite claves canónicas
`A13_G2_CLIENT_*` del único bloque que ya validó y el gate consume sólo esas
claves. El candidato productivo local no cambia.

G2-P2 R5 — cierre source-first:

```text
STATUS=PASS
REASON=CANDIDATE_SOURCE_FIRST_PASS
6/6 COMPILE=PASS
6/6 UPLOAD=PASS
6/6 CLIENT=PASS
6/6 CONTRACT=PASS
SOURCE_CORE=True
STUB_CORE=False
ARCHIVE_LINKED=False
FULL_PROFILE=True
TRACKED_DIRTY_FINAL=3
STAGED_FINAL=0
DIFF_CHECK_FINAL=True
PRODUCT_FAILURE=NO
HARNESS_FAILURE=NO
HARDWARE_FAILURE=NO
ENVIRONMENT_FAILURE=NO
```

G2-P3 queda desbloqueado. Debe regenerar `core.a` desde los tres blobs
candidatos exactos, usar `%TEMP%` para outputs de build/verify, y demostrar
que el FQBN normal `jwplcbasic` compila con
`jwcontrol_precompiled_stub + core.a`.

Ante cualquier fallo de build/verify, el gate debe restaurar el archive
anterior y dejar sólo los tres sources candidatos dirty.


G2-P3 — refresh de core y enlace normal:

```text
HEAD=0cfec7d38719bea643baab97a5c4d7dac9833c48
PWSH_VERSION=7.6.6
ARDUINO_CLI=1.0.2

SOURCE_BUILD=PASS
SOURCE_BUILD_TIME_S=110.125
SOURCE_COMPILE_DB_ENTRIES=79
SOURCE_JWCONTROL_TUS=64
SOURCE_STUB_TUS=0
SOURCE_PERIPHERALS_INIT_COUNT=1

CORE_BEFORE_BYTES=3042444
CORE_BEFORE_SHA256=78d0c0ab14f156b96116529e88872340d51877af40d24ba3559f6081e0bf34fb
CORE_AFTER_BYTES=3043670
CORE_AFTER_SHA256=6f328eeb796091070c8d852a2c0e90f71047d8d2b5786fe3cd5079b2fb6ff983

NORMAL_VERIFY=PASS
NORMAL_VERIFY_TIME_S=73.107
NORMAL_COMPILE_DB_ENTRIES=16
NORMAL_SOURCE_TUS=0
NORMAL_STUB_TUS=1
NORMAL_PRECOMPILED_STUB_COUNT=1
NORMAL_JWPLCBASIC_STUB=True
NORMAL_JWPLCBASIC_SOURCE_CORE=False
NORMAL_JWPLCBASIC_CORE_A_LINKED=True
APP_BYTES=411376

BOARDS_LOCAL_UNCHANGED=True
TRACKED_DIRTY_FINAL=4
STAGED_FINAL=0
CANDIDATE_BLOBS_FINAL=True
DIFF_CHECK_FINAL=True
STATUS=PASS
REASON=CORE_REFRESH_AND_NORMAL_LINK_PASS
```

G2-P4 queda desbloqueado. Debe compilar un probe con el FQBN normal
`jwplcbasic`, demostrar nuevamente stub + archive sin source core, subirlo al
JWPLC y verificar físicamente:

```text
JWPLC_IO.ready()=YES
EN_IO output-enable=YES
EN_IO latch=HIGH
lastScanMs>0
```

Los fault steps no se repiten en P4: ya quedaron validados source-first en
G2-P2 y el archive de P3 fue generado desde esos mismos blobs cualificados.

G2-P4 — validación física con package normal precompilado:

```text
HEAD=5cf8dba1495de150bc9c7161a0233732417bb4b7
SERIAL_PORT=COM4
ARDUINO_CLI=1.0.2
COMPILE_EXIT=0
USES_STUB_CORE=True
USES_SOURCE_CORE=False
CORE_A_LINKED=True
SOURCE_TU_COUNT=0
STUB_TU_COUNT=1
PRECOMPILED_STUB_COUNT=1
FULL_PROFILE=True
UPLOAD_EXIT=0
CLIENT_EXIT=0
IO_READY=YES
EN_IO_OUTPUT_ENABLE=YES
EN_IO_OUTPUT_LATCH=HIGH
LAST_SCAN_MS=2280
CORE_SHA256=6f328eeb796091070c8d852a2c0e90f71047d8d2b5786fe3cd5079b2fb6ff983
SOURCE_BLOBS_FINAL=True
TRACKED_DIRTY_FINAL=4
STAGED_FINAL=0
DIFF_CHECK_FINAL=True
STATUS=PASS
REASON=NORMAL_PRECOMPILED_PHYSICAL_PASS
PRODUCT_FAILURE=NO
HARNESS_FAILURE=NO
HARDWARE_FAILURE=NO
ENVIRONMENT_FAILURE=NO
```

Cadena A13-002 completada:

```text
baseline defect reproduced -> PASS
source-first candidate physical -> PASS
core.a refresh -> PASS
normal stub + archive link -> PASS
normal precompiled physical -> PASS
```

Pendiente antes del commit:

```text
final unstaged diff audit
git diff --check
explicit staging of exactly 4 product files
staged diff audit
single product commit
push
status closure
```

## Observación mapeada — delay() y temporización no bloqueante

No forma parte del fix A13-002 ni abre un gate nuevo en Alpha13.

Estado actual del core:

```text
delay(ms) -> vTaskDelay(ms / portTICK_PERIOD_MS)
BLOCKS_ENTIRE_ESP32=NO
BLOCKS_USER_LOOP_TASK=YES
JWPLC_SYSTEM_TASK_CONTINUES=YES
```

Mientras el sketch está dentro de `delay(ms)`, la tarea independiente
`jwplcSystemTask` puede seguir atendiendo:

```text
I/O scan
RTC
Ethernet service
DataLog service
Display
```

La botonera también tiene su propia tarea de escaneo.

Sí quedan pausados hasta que retorna el `loop()` del usuario:

```text
código secuencial restante del loop
serialEventRun
JWPLC_ModbusTCP.task() autoservice pre/post-loop
JWPLC_ModbusRTU.task() si el usuario lo atiende desde loop
cualquier máquina de estados del sketch
```

Decisión de diseño:

```text
DO_NOT_OVERRIDE_ARDUINO_DELAY=YES
ASYNC_DELAY_API=CANDIDATE_BACKLOG
CURRENT_ALPHA_PRODUCT_SCOPE=NO_CHANGE
```

Dirección propuesta futura: una API de temporizadores no bloqueantes, por
ejemplo `JWPLC_Timer`, basada en `millis()` y segura ante rollover, con
`start()`, `done()`, `every()`, `restart()`, `cancel()` y
`remaining()`. Evitar callbacks/tareas por defecto para no introducir
concurrencia innecesaria en sketches de PLC.

`delayMicroseconds()` merece una revisión separada porque su implementación
espera activamente y no tiene la misma semántica cooperativa de `delay(ms)`.


## Observación mapeada — TFT sucia al energizar por USB

No se clasifica todavía como regresión productiva y no se mezcla con A13-002.

Estado actual del source:

```text
JWPLC_TFT_RST=GPIO14
initPeripherals():
  TFT_CS -> OUTPUT/HIGH
  TFT_RST -> OUTPUT/LOW
JWPLC_TFT.begin():
  TFT_eSPI backend init()
Alpha11 contract:
  TFT_RST_HELD_LOW_DURING_AUTOLOAD=YES
  FIRST_IDLE_FRAME_IMMEDIATE_AFTER_DISPLAY_BEGIN=YES
```

La retención de reset existe y no fue eliminada por A13-002. Sin embargo,
`initPeripherals()` se ejecuta dentro de `loopTask`, después de
`initArduino()`. El backlight del Basic v2 no tiene control por software.
Por ello existe una ventana estrictamente anterior a `initPeripherals()` en
la que el firmware de aplicación todavía no gobierna GPIO14 y el GRAM del
ST7789 puede hacerse visible al energizar.

Interpretación provisional:

```text
dirty pattern only before first IDLE frame -> pre-firmware/power-on window
dirty pattern persists after IDLE/display begin -> DISPLAY REGRESSION
```

Acción:

```text
TFT_STARTUP_DIRTY=OPEN_ROBUSTNESS_OBSERVATION
TFT_PRE1=BASELINE_TIMING_WITH_NORMAL_PACKAGE
TFT_NEW_FEATURES=OUT_OF_SCOPE
PRODUCT_CHANGE_BEFORE_MEASUREMENT=NO
G3_BLOCKED_UNTIL_TFT_PRE1_CLASSIFIED=YES
```

TFT-PRE1 — baseline con package normal:

```text
HEAD=ce7831e214a88551a85acf2be83dad569131881f
POWER_SOURCE=USB_ONLY
24VDC=DISCONNECTED
SERIAL_PORT=COM4
USES_STUB_CORE=True
USES_SOURCE_CORE=False
CORE_A_LINKED=True
SETUP_ENTRY_MS=664
DISPLAY_READY=YES
IO_READY=YES
TFT_RST_OUTPUT_ENABLE=YES
TFT_RST_OUTPUT_LATCH=HIGH
TFT_CS_OUTPUT_ENABLE=NO
TFT_CS_OUTPUT_LATCH=LOW
TRACKED_DIRTY_FINAL=0
STAGED_FINAL=0
DIFF_CHECK_FINAL=True
STATUS=PASS
REASON=BASELINE_TIMING_CAPTURED
```

Interpretación:

```text
first valid display frame occurs before setup()
software autoload before setup <= 664 ms from Arduino millis epoch
multi-second firmware-delay hypothesis=NOT_SUPPORTED_BY_PRE1
exact RST-low and first-frame timestamps=NOT_YET_MEASURED
TFT_CS post-init state=DIAGNOSTIC_ONLY
```

PRE1 no mide el instante exacto de power-on, `initPeripherals()` ni del primer
frame; sólo fija un límite superior antes de `setup()`. TFT-PRE2 debe usar
instrumentación temporal source-first y restaurarla byte-for-byte antes del
upload para medir `app_main`, `initArduino`, entrada a `initPeripherals`, RST,
I2C, RTC, FRAM, SD, botones, Display begin, primer refresh y TCA.

Comparación de backend relevante para el arranque:

```text
ALPHA11_BACKEND=Adafruit_ST7789
ALPHA12_CURRENT_BACKEND=JWPLC_TFT -> TFT_eSPI
CURRENT_JWPLC_TFT_BEGIN=g_backend.init() -> setRotation() -> release
CURRENT_EXPLICIT_BLACK_CLEAR_INSIDE_TFT_BEGIN=NO
FIRST_BLACK_CLEAR=Idle phase 0 after jwplcDisplayBeginCallback() returns
BACKLIGHT_SOFTWARE_CONTROL=NO
RST_HOLD_BEFORE_DISPLAY_BEGIN=YES
```

El código Alpha11 de JWPLC tampoco hacía un `fillScreen(BLACK)` dentro de
`tft.init()`: la corrección histórica fue mantener RST bajo durante el autoload
y hacer la inicialización antes de `setup()`. Por tanto la diferencia actual a
investigar es la secuencia interna del backend TFT_eSPI, no la desaparición de
la protección RST.

TFT-PRE2 cerró PASS con instrumentación source-first:

```text
DISPLAY_BEGIN_DURATION_US=558180
FIRST_REFRESH_DURATION_US=14067
RST_TO_DISPLAY_START_US=13145
BOOT_TO_FIRST_REFRESH_US=659341
SETUP_ENTRY_US=664542
SOURCE_RESTORED=True
CORE_SHA_PRESERVED=True
WORKTREE_FINAL=CLEAN
```

Clasificación:

```text
RTC_FRAM_SD_BUTTONS_AS_MAIN_CAUSE=NO
IDLE_FIRST_REFRESH_AS_MAIN_CAUSE=NO
DISPLAY_BEGIN_BACKEND_WINDOW=CONFIRMED
PRODUCT_CHANGE=NO
```

TFT-PRE3 reutiliza el probe de cualificación TFT_eSPI ya existente para
identificar la instalación 2.5.43 usada por el entorno de mantenimiento,
verificar hashes del source y caracterizar la secuencia ST7789 antes de
construir un candidato.

TFT-PRE3 R1 — atribución válida, parser semántico incompleto:

```text
TFT_ESPI_LIB_LIST_VERSION=2.5.43
TFT_ESPI_SELECTED_VERSION=2.5.43
TFT_ESPI_SELECTED_ROOT=C:\Users\jeykc\Documentos\Programacion\Arduino\libraries\TFT_eSPI
TFT_ESPI_HEADER_VERSION=2.5.43
TFT_ESPI_CPP_SHA256=01ed6edb0530d38b94ddeac079ba81633aa21d77d049b12da21a37f4bec69ee1
ST7789_INIT_SHA256=e21cae2ac84285dc0e77648eca67ca753f41f7da3c271594ede750b725136c10
TFT_ESPI_HEADER_SHA256=b1b2789ace7ac8fd4a4c414054757d91e6e62649a23d74226ea28ceb8d6f4462
RESET_HIGH_LOW_HIGH_PRESENT=True
RESET_DELAY_150_PRESENT=True
ST7789_DELAY_VALUES_MS=120,10,120,120,120,10,120,120
STATUS=REVIEW
REASON=ST7789_INIT_SEQUENCE_UNEXPECTED
PRODUCT_FAILURE=NO
HARNESS_FAILURE=YES
ENVIRONMENT_FAILURE=NO
```

F105: el parser asumió únicamente macros `TFT_*`; R2 acepta también
`ST7789_*` y valores literales 0x11/0x13/0x29, y reporta la expresión real
capturada para SLPOUT/NORON/DISPON.

TFT-PRE3 R2 — abortado por syntax preflight:

```text
A13_PWSH_VERSION=7.6.6
A13_TFT_PRE3_SYNTAX=FAIL
COMPILE_EXECUTED=NO
UPLOAD_EXECUTED=NO
PRODUCT_FAILURE=NO
HARNESS_FAILURE=YES
ENVIRONMENT_FAILURE=NO
FAILURE=F106
```

La edición remota R2 se corrompió por semántica de replacement de JavaScript
al insertar texto PowerShell que contenía `$'`. El BAT detectó la sintaxis
inválida antes de ejecutar el gate. R3 fue reconstruido desde el script R1
que sí había ejecutado, conservando el parser semántico corregido con un
mecanismo de replacement seguro.

TFT-PRE3 R3 — cierre:

```text
STATUS=PASS
REASON=TFT_ESPI_2543_SOURCE_ATTRIBUTED
TFT_ESPI_SELECTED_VERSION=2.5.43
TFT_ESPI_CPP_SHA256=01ed6edb0530d38b94ddeac079ba81633aa21d77d049b12da21a37f4bec69ee1
ST7789_INIT_SHA256=e21cae2ac84285dc0e77648eca67ca753f41f7da3c271594ede750b725136c10
TFT_ESPI_HEADER_SHA256=b1b2789ace7ac8fd4a4c414054757d91e6e62649a23d74226ea28ceb8d6f4462
RESET_HIGH_LOW_HIGH_PRESENT=True
RESET_DELAY_150_PRESENT=True
ST7789_SLPOUT_EXPR=ST7789_SLPOUT
ST7789_NORON_EXPR=ST7789_NORON
ST7789_DISPON_EXPR=ST7789_DISPON
ST7789_SLPOUT_BEFORE_DISPON=True
ST7789_FIRST_DELAY_AFTER_DISPON_MS=120
WORKTREE_FINAL=CLEAN
```

Conclusión:

```text
DISPLAY_BEGIN_DURATION_US=558180
backend delay blocks account for nearly all measured duration
visible dirty window begins after backend DISPON
candidate direction=defer DISPON, clear GRAM black, then DISPON
do not mutate installed TFT_eSPI in place
```

TFT-PRE4 sólo genera el candidato bajo `%TEMP%`: no modifica producto ni la
instalación TFT_eSPI. El candidato conserva el delay post-DISPON de 120 ms,
pero lo mueve después de limpiar GRAM a negro.

TFT-PRE4 cerró PASS:

```text
STATUS=PASS
REASON=TEMP_DEFERRED_DISPON_CANDIDATE_READY
CANDIDATE_JWPLC_TFT_CPP_SHA256=494440b00e74e74a7b23975420574e035ef2138fdf5a0cea2fed51c986c29d25
CANDIDATE_TFT_SETUP_SHA256=8fa079444ca130772d3a642eaf814035100bc098032b403c922c458d351ae5e1
CANDIDATE_ST7789_INIT_SHA256=44873be82fe836084934a328df77f098e9ab88d212dd1d570da5e8aac74671bc
BACKEND_ORIGINAL_MUTATED=NO
REPO_PRODUCT_MUTATED=NO
WORKTREE_FINAL=CLEAN
```

TFT-PRE5 R2 — REVIEW_HARNESS (F108):

```text
A13_TFT_PRE5_SYNTAX=PASS
SERIAL_PORT=COM4
PRE4_SHA_GUARDS=PASS
STATUS=REVIEW
REASON=UNEXPECTED_GATE_EXCEPTION
EXCEPTION=No se encuentra la propiedad "Count" en este objeto
COMPILE_CANDIDATE=NOT_REACHED
UPLOAD=NOT_EXECUTED
VISUAL_VERIFICATION=NOT_PERFORMED
```

R3 conserva PRE4 sin alteraciones, fuerza `@(...)` para los resultados de
`Get-ChildItem`, adelanta el control de cardinalidad y añade comprobaciones
sobre la unidad de traducción real `TFT_eSPI.cpp`: si no recibe
`ST7789_DRIVER` y `JWPLC_TFT_DEFER_DISPON`, el build aborta por `#error`.
La guardia se inyecta sólo bajo `%TEMP%` y se registran los hashes de
archivos originales y temporales. Cualquier anomalía evita la subida.

Para evitar un PASS falso por firmware anterior, la copia temporal del
probe imprime en cada bloque canónico el identificador
`A13_TFT_PRE5_CANDIDATE_SHA256` de PRE4. El cliente lo recopila y el gate
exige el valor esperado además de `DISPLAY_READY=YES`, `IO_READY=YES` y
estado válido de TFT_RST. La salida no declara corrección visual hasta que
se revise el video de tres encendidos USB completos.

Importante: un eventual PASS de PRE5-R3 significa compilación, subida y
runtime serial correctos, pero NO demuestra por sí solo la eliminación
visual del blanco/GRAM. Esa conclusión necesita el video de energizado USB
sin 24 V, comparado con el baseline PRE1/PRE2.

TFT-PRE5 R1 — REVIEW_HARNESS:

```text
A13_TFT_PRE5_SYNTAX=PASS
PRE4 candidate SHA guards=PASS
SERIAL_PORT=COM4
COMPILE_EXIT=1
TEMP_JWPLC_TFT_SELECTED=YES
TEMP_TFT_ESPI_SELECTED=YES
ERROR_CLASS=driver setup symbols missing
UPLOAD_EXECUTED=NO
STATUS=REVIEW
REASON=SOURCE_FIRST_CANDIDATE_COMPILE_FAILED
PRODUCT_FAILURE=NO
HARNESS_FAILURE=YES
FAILURE=F107
```

R2 elimina la sustitución de `User_Setup.h` / `User_Setup_Select.h` y usa
el mecanismo nativo de TFT_eSPI 2.5.43: una copia temporal del probe con el
`tft_setup.h` candidato en la misma carpeta del `.ino`. El backend global y
el repositorio permanecen intactos.

TFT-PRE5:

```text
- localiza el último candidato PRE4 PASS y verifica sus tres SHA;
- copia TFT_eSPI 2.5.43 a `%TEMP%` y aplica allí ST7789_Init.h candidato;
- sustituye User_Setup/User_Setup_Select únicamente en la copia temporal para
  fijar de forma reproducible el perfil privado JWPLC;
- crea JWPLC_TFT source-only temporal;
- exige selección de ambas librerías temporales y exactamente un
  JWPLC_TFT.cpp.o + un TFT_eSPI.cpp.o;
- mantiene JWPLC_Display y core en sus rutas precompiladas normales;
- sube el probe PRE1 y exige DISPLAY_READY=YES + IO_READY=YES;
- no modifica el repo ni la instalación TFT_eSPI del mantenedor;
- deja el firmware candidato cargado para la prueba visual de power-cycle.
```

Referencia externa de drivers:

```text
Adafruit ST7789 generic:
  SWRESET delay 150 ms
  SLPOUT delay 10 ms
  NORON delay 10 ms
  DISPON delay 10 ms

TFT_eSPI ST7789 upstream:
  hardware reset high/low/high + reset wait
  SLPOUT delay 120 ms
  delay 120 ms before DISPON
  DISPON
  delay 120 ms after DISPON
```

Esto es consistente con el video nuevo: la pantalla se hace blanca cerca de
+750 ms y el primer frame limpio empieza ~180 ms después. TFT-PRE2 medirá el
tiempo real de `jwplcDisplayBeginCallback()` ejecutando el archive actual, sin
cambiar aún el producto.

Si el fenómeno es sólo transitorio, la mejora software posible es adelantar
CS=HIGH/RST=LOW al punto seguro más temprano de `app_main()`; eso reduce la
ventana pero no puede eliminar el intervalo anterior al firmware. La solución
eléctrica absoluta requeriría mantener RST definido durante power-on o controlar
backlight, lo cual pertenece a hardware/revisión de placa y no se asumirá sin
revisar el esquemático.

## TFT-PRE5 R3 — cierre del candidato físico y visual (2026-10-09)

```text
TFT_PRE5_R3=PASS
REASON=SOURCE_FIRST_CANDIDATE_FLASHED
PROBE_INO_COUNT=1
COMPILE_EXIT=0
JWPLC_TFT_TEMP_SELECTED=True
TFT_ESPI_TEMP_SELECTED=True
JWPLC_TFT_SOURCE_OBJECT_COUNT=1
TFT_ESPI_SOURCE_OBJECT_COUNT=1
JWPLC_TFT_PRECOMPILED=False
JWPLC_DISPLAY_PRECOMPILED=True
USES_STUB_CORE=True
CORE_A_LINKED=True
UPLOAD_EXIT=0
CLIENT_EXIT=0
RUNTIME_CANDIDATE_SHA256=494440b00e74e74a7b23975420574e035ef2138fdf5a0cea2fed51c986c29d25
DISPLAY_READY=YES
IO_READY=YES
TFT_RST_OUTPUT_ENABLE=YES
TFT_RST_OUTPUT_LATCH=HIGH
TFT_CS_OUTPUT_ENABLE=YES
TFT_CS_OUTPUT_LATCH=HIGH
SETUP_ENTRY_MS=676
BASELINE_SETUP_ENTRY_MS=664
USER_VISUAL_WHITE_GRAM=RESOLVED_IN_TEMP_CANDIDATE
IDLE_PROGRESSIVE_RENDERING=OBSERVED_REMAINING
PACKAGE_LIBRARY_ARCHIVE_NOT_REFRESHED=YES
PRODUCT_COMMIT=NO
```

El usuario confirmó por nueva grabación y mensaje que el fondo blanco
desapareció y el arranque permanece limpio. El IDLE sigue renderizándose por
fases: no es parte del defecto de GRAM y no autoriza features nuevas.

PRE5 R3 conservó íntegros el repositorio, la librería externa TFT_eSPI y
los archives precompilados versionados. Por eso **la solución aún NO está
disponible para un sketch normal del package**: el candidato se compiló desde
dos librerías temporales seleccionadas por Arduino CLI, mientras
`JWPLC_TFT` distribuida sigue enlazando el archive anterior.

Documento de cierre: `A13_TFT_PRE5_PHYSICAL_VISUAL_20261009.md`.

```text
TFT-PRE6=NEXT
BUILD_RECIPE=TEMPORARY_COPY_OF_VERIFIED_TFT_ESPI_2_5_43
PRECOMPILED_ARCHIVE_REBUILD=REQUIRED
NORMAL_PACKAGE_NO_USER_TFT_ESPI_DEPENDENCY=REQUIRED
DISPLAY_UI_CONSUMER_REGRESSION=REQUIRED
NORMAL_PACKAGE_PHYSICAL_USB_STARTUP_VISUAL=REQUIRED
COMMIT_ONLY_AFTER_PASS=YES
```

## TFT-PRE6 — plan y primer gate sin mutación productiva

Documento: `A13_TFT_PRE6_INTEGRATION_PLAN_20261009.md`.

```text
TFT_PRE6_P0=PREPARED_NOT_EXECUTED
TFT_PRE6_P0_GATE=tools/alpha13/gates/a13_tft_pre6_p0_archive_candidate.ps1
TFT_PRE6_P0_RUNNER=tools/alpha13/gates/run_a13_tft_pre6_p0_archive_candidate.bat
P0_ARCHIVE_INPUT=PRE5_R3_COMPILED_SOURCE_OBJECTS
P0_OUTPUT=%TEMP%/candidate-libraries/JWPLC_TFT/src/esp32/libJWPLC_TFT.a
P0_NORMAL_CASES=DIRECT_TFT,DISPLAY_INTEGRATION,NORMAL_AUTOLOAD
P0_USES_NORMAL_PRECOMPILED_TFT=YES
P0_USER_EXTERNAL_TFT_ESPI_REQUIRED=NO
P0_OFFICIAL_ARCHIVE_MUTATION=NO
P0_USB_FLASH=NO
P1_CANONICAL_SOURCE_REBUILD=PENDING_P0_PASS
PRODUCT_COMMIT=NO
```

Prevención heredada: F105 macros semánticas; F106 reemplazos seguros;
F107 setup por sketch-local `tft_setup.h`; F108 arrays explícitos
`@(...)`, no reportar PASS físico en una compilación meramente estática.
El run P0 sólo garantiza archivo temporal autocontenido y compilación
normal (no demuestra aún fuente canónica ni un archive productivo instalado).

## TFT-PRE6-P0 — archive autónomo temporal (2026-10-09)

```text
TFT_PRE6_P0_STATUS=CLOSED_PASS
REASON=TEMP_PRECOMPILED_ARCHIVE_QUALIFIED
HEAD=89e461123474310e8528e4daf50c97262a86390b
CANDIDATE_ARCHIVE_SHA256=ff9dd89cb267bc270d2ca6fa2d0f1362b76595b8dc6b49f764d05a8e28bb6705
CANDIDATE_ARCHIVE_BYTES=1091942
TFT_OBJECT_SHA256=0a84fc685255b7b2e42a8050d0c643ce12afd6fcb628273993822b77755b74bb
BACKEND_OBJECT_SHA256=f4b771ba6847cb2b8dca3a7d9c29879725e3e160047df2e9b68b5944fae244cf
ARCHIVE_MEMBER_COUNT=2
ARCHIVE_MEMBER_PARITY=PASS
DIRECT_TFT_COMPILE_EXIT=0
DISPLAY_INTEGRATION_COMPILE_EXIT=0
NORMAL_AUTOLOAD_COMPILE_EXIT=0
COMPILE_CASES_PASS=3
GLOBAL_TFT_ESPI_REQUIRED=NO
OFFICIAL_ARCHIVES_PRESERVED=YES
WORKTREE_FINAL=CLEAN
UPLOAD_EXECUTED=NO
PRODUCT_REPO_MODIFIED=NO
```

Registro detallado: `A13_TFT_PRE6_P0_CLOSURE_20261009.md`.

PRE6-P0 prueba que los objetos físicamente validados en PRE5 R3 se pueden
empaquetar en una librería autónoma enlazable en tres casos Arduino normales.
Todavía no demuestra reconstrucción desde las fuentes canónicas.

## TFT-PRE6-P1A — reproducción exacta desde fuente (preparado)

```text
TFT_PRE6_P1A=PREPARED_NOT_EXECUTED
SOURCE_GEN=tools/alpha13/gates/a13_tft_pre6_p1_source_regen.py
GATE=tools/alpha13/gates/a13_tft_pre6_p1a_source_regen.ps1
RUNNER=tools/alpha13/gates/run_a13_tft_pre6_p1a_source_regen.bat
EXPECTED_SOURCE_CPP_SHA256=494440b00e74e74a7b23975420574e035ef2138fdf5a0cea2fed51c986c29d25
EXPECTED_SETUP_SHA256=8fa079444ca130772d3a642eaf814035100bc098032b403c922c458d351ae5e1
EXPECTED_ST7789_INIT_SHA256=44873be82fe836084934a328df77f098e9ab88d212dd1d570da5e8aac74671bc
PRODUCT_MUTATION=NO
COMPILE=NO
UPLOAD=NO
P1B_SOURCE_BUILD=BLOCKED_UNTIL_P1A_PASS
```

P1A usa únicamente sources actuales con identidad de Git comprobada y
TFT_eSPI 2.5.43 con hashes SHA-256 exactos. Transforma sólo archivos
temporales, sin modificar el código producto ni la instalación global
de TFT_eSPI. La coincidencia de los tres SHA físicos PRE4 es requisito
obligatorio para pasar a recompilación P1B. En Windows sólo se permite
normalizar CRLF/LF durante la comparación de identidad del blob Git.

## TFT-PRE6-P1A R1 — REVIEW_HARNESS / F109 (2026-10-09)

```text
P0_PREVIOUS=CLOSED_PASS
P1A_R1=REVIEW_HARNESS
REASON=P0_PASS_PROOF_NOT_FOUND
PHASE=P0_PROVENANCE
P0_ACTUAL_SUMMARY=tools/alpha13/results/tft_pre6_p0_20261009_123147/SUMMARY.log
P0_ACTUAL_STATUS=PASS
P0_ACTUAL_REASON=TEMP_PRECOMPILED_ARCHIVE_QUALIFIED
P0_ACTUAL_ARCHIVE_SHA256=ff9dd89cb267bc270d2ca6fa2d0f1362b76595b8dc6b49f764d05a8e28bb6705
P1A_COMPILE_EXECUTED=NO
P1A_UPLOAD_EXECUTED=NO
PRODUCT_REPO_MODIFIED=NO
FAILURE=F109
```

P1A R1 no encontró la prueba previa a pesar de que P0 pasó y dejó
evidencia canónica. R2 prueba el parser con un fixture sintético, hace la
llamada a `Get-A13LogValue` explícita y muestra los valores
`STATUS/REASON/SHA` para cada `SUMMARY.log` candidato. La causa exacta
queda condicionada al diagnóstico R2; P0 no se repite.

Se reforzó también el generador de `ST7789_Init.h` para comprobar,
contra el SHA físico PRE4, las variantes exactas LF/CRLF de los here-strings
PowerShell. No flexibilizar hashes ni modificar el producto.
`NEXT_FAILURE_ID=F111`; sin nuevas hipótesis tratadas como hechos.

## TFT-PRE6-P1A R2 — evidencia faltante (2026-10-09)

```text
P1A_R2=REVIEW_HARNESS
PROOF_PARSER_SELF_TEST=PASS
PROOF_SEARCH_DIRECTORY_COUNT=1
PROOF_SUMMARY_MISSING=tools/alpha13/results/tft_pre6_p0_20261009_123147/SUMMARY.log
COMPILE_EXECUTED=NO
UPLOAD_EXECUTED=NO
PRODUCT_REPO_MODIFIED=NO
F109_CAUSE_CLASS=LOCAL_PROOF_UNAVAILABLE
P0_PREVIOUS=CLOSED_PASS
NEXT=READ_ONLY_CHECK_OF_P0_DIR_AND_TEMP_ARCHIVE
```

La prueba del parser ha descartado la hipótesis de R1. El script
encuentra el directorio P0, pero no el `SUMMARY.log` que P0 anunció.
Se desconoce por qué el archivo ya no es accesible; no atribuirlo a
limpieza de archivos sin evidencia. P0 conserva un cierre Git durable,
`A13_TFT_PRE6_P0_CLOSURE_20261009.md`, pero el gate aún debe exigir
prueba suficientemente fuerte antes de aceptar un handoff recuperado.

P1A no se relanza hasta verificar si el archive de P0 en `%TEMP%`
sigue disponible. No regenerar el `SUMMARY.log` histórico ni repetir
P0 innecesariamente. Seguir `F109`; no crear `F110` sin otra clase
independiente de fallo.

## TFT-PRE6-P1A — recuperación de evidencia y R3 (2026-10-09)

```text
P0_DIRECTORY_EXISTS=True
P0_SUMMARY_EXISTS=False
P0_ARCHIVE_EXISTS=True
P0_ARCHIVE_BYTES=1091942
P0_ARCHIVE_SHA256=ff9dd89cb267bc270d2ca6fa2d0f1362b76595b8dc6b49f764d05a8e28bb6705
P0_RESULT=CLOSED_PASS
P1A_R3=PREPARED_NOT_EXECUTED
REQUIRES_HISTORICAL_P0_SUMMARY=NO
SOURCE_OF_TRUTH=P0_PRE4_PRE3_VERSIONED_DOCS_PLUS_P0_ARCHIVE_SHA
PRE4_CPP_SETUP_ST7789_EXPECTED_SHA=UNCHANGED
PRODUCT_MUTATION=NO
COMPILE_EXECUTED=NO
UPLOAD_EXECUTED=NO
NEXT_FAILURE_ID=F111
```

P1A R2 identificó que el parser pasa pero el archivo
`tools/alpha13/results/tft_pre6_p0_20261009_123147/SUMMARY.log`
ya no está disponible. El usuario confirmó que su archive P0 en
`%TEMP%` sigue presente, mide 1 091 942 bytes y coincide con el SHA
cerrado. La causa de desaparición del `SUMMARY.log` no se conoce.

R3 usa como evidencia durable los documentos versionados:
`A13_TFT_PRE6_P0_CLOSURE_20261009.md`,
`A13_TFT_PRE4_CANDIDATE_20261009.md` y
`A13_TFT_PRE3_SOURCE_ATTRIBUTION_20261009.md`.
Comprueba blobs Git desde HEAD, campos, compilaciones P0 documentadas,
archive SHA/tamaño, y backend TFT_eSPI 2.5.43 con hashes originales.
No se reconstruye ni falsifica el `SUMMARY.log` previo. El generador
de fuentes de P1A permanece sin modificaciones en R3. Sólo se progresa
a P1B después de `STATUS=PASS` real en P1A.

## TFT-PRE6-P1A R3 — CLOSED_PASS; P1B listo

```text
P1A_R3=PASS
REASON=CANONICAL_SOURCE_TRANSFORM_REPRODUCED
P0_HANDOFF_RECOVERED=YES
P0_PROOF_SOURCE=VERSIONED_CLOSURE_AND_ARCHIVE_SHA256
P0_HISTORICAL_SUMMARY=NOT_REQUIRED_MISSING
P0_ARCHIVE_SHA256=ff9dd89cb267bc270d2ca6fa2d0f1362b76595b8dc6b49f764d05a8e28bb6705
P1A_CPP_SHA256=494440b00e74e74a7b23975420574e035ef2138fdf5a0cea2fed51c986c29d25
P1A_SETUP_SHA256=8fa079444ca130772d3a642eaf814035100bc098032b403c922c458d351ae5e1
P1A_INIT_SHA256=44873be82fe836084934a328df77f098e9ab88d212dd1d570da5e8aac74671bc
P1A_BACKEND_CPP_INSTRUMENTED_SHA256=3fe3c601830aad09e579e8fec1223c6a0e9b6a31a82f491743fbe166a5b16350
P1A_MANIFEST=%TEMP%/jwplc_a13_tft_pre6_p1a_20261009_165802/MANIFEST.json
P1A_COMPILE_EXECUTED=NO
P1A_UPLOAD_EXECUTED=NO
P1A_PRODUCT_MUTATED=NO
P1B_GATE=tools/alpha13/gates/a13_tft_pre6_p1b_rebuild_archive.ps1
P1B_WRAPPER=tools/alpha13/gates/run_a13_tft_pre6_p1b_rebuild_archive.bat
P1B_STATE=READY_NOT_EXECUTED
P1B_SOURCE_COMPILE=REQUIRED
P1B_ARCHIVE_OUTPUT=TEMP_ONLY
P1B_NORMAL_COMPILE_CASES=3
P1B_UPLOAD=NO
P1B_PRODUCT_MUTATION=NO
NEXT_FAILURE_ID=F111
```

Documento de cierre: `A13_TFT_PRE6_P1A_CLOSURE_20261009.md`.

P1B selecciona el manifiesto P1A que declare las mismas fuentes y
verifica los hashes físicos PRE4 antes de compilar. El gate se basa en el
modelo de archive y tres consumidores de P0, que ya pasaron.
Exige compilación source-first de `JWPLC_TFT.cpp.o` y
`TFT_eSPI.cpp.o`, archive de dos miembros con paridad byte a byte,
y builds Arduino normales que NO seleccionen la TFT_eSPI global.
No se permite publicar el archive ni dar por corregido el package antes
de P2 y su prueba física.

## TFT-PRE6-P1B — archive reconstruido desde fuentes (2026-10-09)

```text
P1B_STATUS=CLOSED_PASS
REASON=CANONICAL_SOURCE_ARCHIVE_REBUILT_AND_LINKED
HEAD=8598a0dbee6006b27c28f1232ca2877d882423fe
SOURCE_COMPILE_EXIT=0
SOURCE_JWPLC_TFT_TEMP_SELECTED=True
SOURCE_TFT_eSPI_TEMP_SELECTED=True
SOURCE_CORE_STUB=True
SOURCE_CORE_ARCHIVE_LINKED=True
SOURCE_DISPLAY_PRECOMPILED=True
REGENERATED_TFT_OBJECT_SHA256=03063a848b5d5e20d8010b7d0bc19189b7cc5be412489588c1b14ded66815adf
REGENERATED_BACKEND_OBJECT_SHA256=59b4050dd44b5f6db7a99b2050aee7ff076008eb28032d18d222f4cf57b06732
P1B_ARCHIVE_SHA256=ab73b244c44ebd75d29a4eeb3cd97f5d18c08470f535eb16d55c2fdbf2310ff8
P1B_ARCHIVE_BYTES=1091990
ARCHIVE_MEMBER_COUNT=2
ARCHIVE_MEMBER_PARITY=PASS
NORMAL_COMPILE_CASES_PASS=3
GLOBAL_TFT_ESPI_REQUIRED_FOR_NORMAL_SKETCH=NO
OFFICIAL_ARCHIVES_PRESERVED=YES
WORKTREE_FINAL=CLEAN
PRODUCT_MUTATION=NO
UPLOAD_EXECUTED=NO
```

Cierre versionado: A13_TFT_PRE6_P1B_CLOSURE_20261009.md.
Diferencia entre P0 y P1B archive SHA aceptada: objetos compilados
independientemente con fuentes equivalentes y gates normales PASS.

## TFT-PRE6-P2A — adopción local reversible, gate preparado

```text
P2A_STATE=PREPARED_NOT_EXECUTED
P2A_GATE_PY=tools/alpha13/gates/a13_tft_pre6_p2a_adopt_product.py
P2A_GATE_PS1=tools/alpha13/gates/a13_tft_pre6_p2a_adopt_product.ps1
P2A_GATE_BAT=tools/alpha13/gates/run_a13_tft_pre6_p2a_adopt_product.bat
P2A_CHANGES=JWPLC_TFT.cpp,tft_setup.h,libJWPLC_TFT.a
P2A_GIT_COMMIT=NO
P2A_UPLOAD=NO
P2A_COMPILE_CASES=DIRECT_TFT,DISPLAY_INTEGRATION,NORMAL_AUTOLOAD,STARTUP_PROBE
P2A_FAIL_ROLLBACK=BYTE_IDENTICAL_BACKUPS_REQUIRED
P2A_SUCCESS_WORKTREE=3_EXPECTED_TRACKED_FILES_MODIFIED
P2A_SUCCESS_NEEDS_PHYSICAL_P2B=YES
PRODUCTIVE_ARCHIVE_REFRESH=NOT_YET_APPLIED
NEXT_FAILURE_ID=F111
```

P2A valida previamente hashes originales del core, Display, fuentes TFT
y libJWPLC_TFT.a. El candidato se selecciona por manifiesto de P1B
bajo %TEMP%, hashes de objeto, SHA del archive, tamaño y fuentes P1A.
Sólo después copia los tres archivos productivos, guarda originales
en %TEMP%, compila cuatro casos mediante la JWPLC_TFT del repositorio
y comprueba autoload, core precompilado, ausencia de fuente backend y
de TFT_eSPI externa seleccionada.

Si falla una condición, restaura los tres archivos originales y exige
worktree CLEAN. Si pasa, **deja tres modificaciones locales SIN COMMIT**
para realizar P2B físico. No hacer git pull, reset ni cambios de
rama hasta completar P2B o restaurar explícitamente la copia de
seguridad. Mantener F110 disponible sólo para fallos nuevos.

## TFT-PRE6-P2A — CLOSED_PASS; hito TFT-CLOSURE pendiente (2026-10-09)

**Estado canónico para nuevo chat.** El usuario ejecutó P2A con estos resultados:

```text
GATE=A13-TFT-PRE6-P2A
STATUS=PASS
REASON=PRODUCT_WORKTREE_ADOPTED_AND_FOUR_NORMAL_BUILDS_PASS
BRANCH=v2.1.0-alpha.13/feature/cleanup-robustness
HEAD_LOCAL_DURING_P2A=ac69e41922f512c280ad116bcf79b24596befa02
HEAD_REMOTO_AFTER_DOCS=CHECK_ON_ENTRY
P2A_CPP_SHA256=494440b00e74e74a7b23975420574e035ef2138fdf5a0cea2fed51c986c29d25
P2A_TFT_SETUP_SHA256=8fa079444ca130772d3a642eaf814035100bc098032b403c922c458d351ae5e1
P2A_TFT_ARCHIVE_SHA256=ab73b244c44ebd75d29a4eeb3cd97f5d18c08470f535eb16d55c2fdbf2310ff8
P2A_TFT_ARCHIVE_BYTES=1091990
P2A_CORE_SHA256=6f328eeb796091070c8d852a2c0e90f71047d8d2b5786fe3cd5079b2fb6ff983
P2A_DISPLAY_SHA256=c960d718433e29a40e3cc55bc745c9a2e121ee1ee598ec72c04872327592dc02
P2A_NORMAL_COMPILE_CASES_PASS=4
P2A_DIRECT_TFT=PASS
P2A_DISPLAY_INTEGRATION=PASS
P2A_NORMAL_AUTOLOAD=PASS
P2A_STARTUP_PROBE=PASS
P2A_GLOBAL_TFT_ESPI_REQUIRED=NO
P2A_PRODUCT_CHANGED_FILE_COUNT=3
P2A_PRODUCT_COMMIT_EXECUTED=NO
P2A_UPLOAD_EXECUTED=NO
P2A_BACKUP_ROOT=%TEMP%/jwplc_a13_tft_pre6_p2a_20261009_172339/originals
LOCAL_WORKTREE=3_TRACKED_PRODUCT_FILES_MODIFIED_UNCOMMITTED
NEXT_FAILURE_ID=F111
```

Los tres archivos locales adoptados en el equipo Windows **y sólo esos**:

```text
JWPLC/2.1.0/libraries/JWPLC_TFT/src/JWPLC_TFT.cpp
JWPLC/2.1.0/libraries/JWPLC_TFT/src/tft_setup.h
JWPLC/2.1.0/libraries/JWPLC_TFT/src/esp32/libJWPLC_TFT.a
```

**No se han versionado aún esos cambios productivos** ni se ha efectuado
upload del archive productivo. La validación visual positiva anterior de PRE5
aplica al **candidato temporal**, no sustituye la prueba normal P2B.
La rama remota sí puede contener nuevos commits **documentales** desde
`ac69e419`, sin significar que los cambios productivos estén en GitHub.

### Siguiente hito: TFT-CLOSURE (flujo integrado)

Contrato operativo: `docs/JWPLC_COLLABORATION_WORKFLOW.md`.

1. **No repetir gates cerrados.** Empezar por verificación de hashes de
   los tres archivos modificados y `git status`, FQBN/COM disponible y
   backup existente. No forzar `git pull` sobre worktree intencionalmente
   modificado. Para traer únicamente un ejecutor nuevo, usar método
   explícito que no actualice los tres archivos productivos.
2. Ejecutar un **gate integrador**, automatizando compilación normal,
   upload por COM verificado, identificación del firmware por serial y
   condición `DISPLAY_READY=YES` / `IO_READY=YES`. Asegurar prueba
   real del archive productivo `ab73b244...` sin TFT_eSPI externa;
   evitar falso PASS por firmware serial anterior (provenance).
3. Pedir al usuario observación de arranque **USB-only** y video/capturas:
   fondo blanco ausente, pantalla limpia, IDLE coherente. No inferir
   resultado visual solo de serial. El dibujado progresivo de IDLE es
   observación distinta, no bug de GRAM.
4. Tras PASS visual, validar regresiones de Display y consumers de
   Alpha12/Alpha13, autoload, APIs y reconstrucción del archive con
   receta capaz de partir de los **nuevos fuentes ya parcheados**.
   La receta P1A original usa como entrada el baseline viejo:
   **requiere adaptación idempotente o nueva receta versionada**.
5. Preparar commit exacto de producto y documentación, revisión de
   `git diff --check` y checklist de Alpha13; commit/publicación
   sujetos a confirmación. No tocar OpenPLC ni HMI Designer.

```text
NEXT_GATE=TFT-CLOSURE
P2B_PHYSICAL=NOT_EXECUTED
P3_REGRESSION_AND_REBUILD_RECIPE=NOT_EXECUTED
P3_COMMIT=NOT_EXECUTED
PRODUCT_REPO_REMOTE_CHANGED=NO_FOR_TFT_THREE_FILES
WORKFLOW=INTEGRATED_MILESTONES
MIGRATION_HANDOFF_SEPARATE_FILE=NOT_REQUIRED
```

**Nota de precedencia:** este bloque y el `NEXT_GATE` vigente
reemplazan las menciones históricas `P2A_PREPARED_NOT_EXECUTED` en
secciones anteriores, conservadas como registro cronológico.

## Gates restantes

```text
G2  A13-002 TCA startup / EN_IO                  CLOSED_PASS
G3  A13-004 TCA RMW/shadow atomicity             PENDING
G4  A13-003 TFT batch task ownership             PENDING
G5  A13-005 + A13-006 TCP correctness            PENDING
G6  A13-007 + review A13-008 datagram robustness PENDING
G7  A13-009 + A13-013 FRAM                       PENDING
G8  A13-010 + A13-011 serial                     PENDING
G9  A13-012 + A13-014 + A13-017 cleanup          PENDING
G10 final regression/freeze                       PENDING
```

## KNOWN_RISKS

```text
Adafruit_BusIO source fallback baseline defect
stale precompiled archives can mask source changes
serial COM can change between sessions
```

## DO NOT DO

```text
DO_NOT_REPEAT_P0
DO_NOT_REPEAT_G1_PRE1_P1_P2_WITHOUT_NEW_EVIDENCE
DO_NOT_REMOVE_NORMAL_AUTOLOAD_PERIPHERALS
DO_NOT_ASSUME_OPENPLC_INTEGRATED
DO_NOT_ASSUME_OTA_DEFINED
DO_NOT_FIX_FINAL_FLASH_FREQ_WITHOUT_DECISION
DO_NOT_PUBLISH_BOOTLOADER_BIN_AS_FINAL
DO_NOT_IMPLEMENT_JW_BUSIO_YET
G1_CLOSED_G2_MAY_START_WITH_READ_ONLY_PREFLIGHT
```
