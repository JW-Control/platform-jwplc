## Evidencia física C — recibida 17/10/2026 13:38

La variante C instalada en COM4 (mantenida fuera del package publicado) emitió:
- `G10R_H0_STAGE_DIAG=ONESHOT_REAL_LINK_RTC_NO_R1_R4` y `G10R_H0_TRACE=FIRST_BOOT_ARMED_ONCE` tras un `rst:0x1 (POWERON_RESET)` a las 13:38:20; el motivo preciso de ese reset no está demostrado.
- A las **13:38:22.777** imprimió `G10R_H0_PROBE_BEGIN=REAL_LINK_STATUS`; a las **13:38:22.780** imprimió `G10R_H0_PROBE_RETURN:PHY=1 STAGE=50`. Lectura real satisfactoria de W5500 PHY `LinkON`, mutex tomado/liberado y retorno de la API.
- Desde **13:38:21.626 hasta 13:38:46.625** el `BOOT=9F83BFAD` se mantuvo constante, con `G10R_READY` sin `TG1WDT_SYS_RESET`; `IP=192.168.0.31`, `TCP=1`, `UDP=1`, `RTU=1` por el período posterior a obtención de DHCP.
- El `LINK=-9` de READY se mantiene como **centinela exclusivamente de laboratorio**, no indica fallo PHY.

**Interpretación:** El A/B previo (lectura repetida en cada READY -> TG1WDT, desactivada -> estabilidad) más C (UNA lectura real tras 1,5 s desde setup -> PHY=1/STAGE=50) **rechazan la hipótesis de bloqueo incondicional del método `linkStatus()`**. Permanecen abiertas posibles dependencias del momento en boot, estado DHCP/autoload, acceso simultáneo al mutex SPI, frecuencia de consulta o timing. En el A original la llamada ocurría en `G10R_READY` durante la ventana inicial, mientras que C difiere en temporización y llama una sola vez. **C no prueba seguridad de las llamadas repetidas.**

**Siguiente experimento preferible (no ejecutado):** D instrumentar una consulta única dentro de la ubicación original `G10R_READY` al primer mensaje, con marcador de fase y registro RTC, y **no repetir después de WDT**. Solo si retorna correctamente, considerar un número pequeño de consultas periódicas acotadas que deje evidencia de cuántas se completaron, evitando repetir tras reset. Mantener el hook solo staged y el autoload normal. NO sustituir la API original por `LINK=-9` en producto, ni reducir periféricos para obtener PASS.

```text
H0_C_REAL_PHY=LINK_ON_1
H0_C_MUTEX_PHY_RELEASE=STAGE_50_PASS_ONE_CALL
H0_C_BOOT_STABILITY=OBSERVED_25S_WITHOUT_TG1WDT
H0_C_REPEATED_LINK_STATUS=NOT_TESTED
H0_ROOT_CAUSE=OPEN_TIMING_OR_REENTRANCY_UNPROVEN
G10-R-H=REVIEW_STOP_NOT_AUTHORIZED
```

---

# Alpha13 H0 — Duplicación de LINK y trazas de WDT por etapas

**Fecha:** 17/10/2026. **Revisión experimental aislada** basada en PR #110. No modifica ni adopta código productivo.

## Respuesta a la observación del propietario

La librería `JWPLC_Ethernet` **sí** consulta el enlace durante el `autoload`: `service()` hace `Ethernet.linkStatus()` dentro del mutex SPI, y `probeHardware()` también lee PHY. Por tanto, `g10_r_integrated.ino`, que llamaba `JWPLC_Ethernet.linkStatus()` cada segundo en READY y HEARTBEAT, introducía consultas adicionales por SPI. **No** se encontró una caché física dedicada `lastLinkStatus` ni un método `cachedLinkStatus` en la API Alpha13 consultada.

- `JWPLC_Ethernet.isReady()` devuelve `_ready`, sin consulta SPI.
- `JWPLC_Ethernet.runtimeState()` devuelve `_runtimeState`, sin consulta SPI, con `LINK_OFF`, `DHCP_PENDING`, `READY`, etc. No equivale a un valor PHY `LinkON/LinkOFF/Unknown` atómico de lectura física.
- `JWPLC_Ethernet.diagnosticCode()` usa los estados almacenados y `Ethernet.dhcpMaintenanceInProgress()` (no llama al PHY).
- `JWPLC_Ethernet.linkUp()` **vuelve a llamar** a `linkStatus()`, por tanto NO evita SPI.
- `JWPLC_Ethernet.statusString()` **puede llamar** a `linkStatus()` estando READY, por tanto no debe confundirse con lectura siempre cacheada.
- `JWPLC_IO` y `JWPLC_Time` sí usan vistas cacheadas separadas (`JWPLC_RuntimeView`), **pero eso no implica caché física dedicada Ethernet**.

**Hipótesis H0:** la lectura redundante en el sketch introduce un disparador de WDT; aún no está localizada la instrucción causal. El A/B de PR #110 estabilizó 30 s con `LINK=-9` ficticio, pero ese marcador no representa enlace real ni es una corrección.

## Experimento C — única llamada PHY, instrumentada

Nuevo branch `codex/alpha13-h0-wdt-link-stage-20261017`, separado de PR #110. El sketch COM4 mantiene las dos impresiones de `LINK=-9` de telemetría rutinaria, para no repetir el WDT por cada segundo; **solo una sonda real** llama a `JWPLC_Ethernet.linkStatus()` después de 1,5 s desde el `setup`. El autoload y todos los periféricos siguen activos.

Solo en la plataforma **staged** la implementación de `JWPLC_EthernetClass::linkStatus()` invoca un callback débil `g10rH0LinkStage(stage)` en estos lugares:

| Etapa | Significado |
|---|---|
| 1 | El sketch entrará a la función física |
| 10 | Entró a `linkStatus()`; antes de esperar mutex SPI |
| 11 | Mutex SPI no adquirido dentro de 200 ms (retorno normal Unknown) |
| 20 | Mutex SPI adquirido |
| 30 | CS deseleccionados, justo antes de `Ethernet.linkStatus()` (PHY) |
| 40 | Se completó consulta PHY |
| 50 | Se liberó mutex SPI |

La sonda solo escribe dos enteros de 32 bits complementados a memoria `RTC_NOINIT_ATTR`, **sin Serial, delays, I/O, SPI extra ni cambio en la región crítica**, por lo que tras un `TG1WDT_SYS_RESET` un nuevo arranque puede imprimir `G10R_H0_PREVIOUS:RUN=1 LAST_STAGE=<n> VALID=1`. La doble palabra reduce el riesgo de interpretar una escritura parcial. Si la llamada retorna, se imprime `G10R_H0_PROBE_RETURN:PHY=<0|1|2> STAGE=50`; no se imprime valor PHY supuesto al quedar bloqueado.

- El estado `RUN=1` se marca antes de ejecutar la consulta.
- Tras reset **el sketch no reintenta la sonda**, para evitar un bucle deliberado de watchdog. Con reset de energía la retención RTC no está garantizada y puede volver a ejecutarse una vez.
- El hook es débil y está definido solo en el sketch experimental, no en las librerías publicadas.
- Los demás métodos/lecturas del package no se modifican.

### Cómo interpretar

- `RUN=1 STAGE=10`: posible espera en mutex, aún sin retorno; confirmar frente a temporización 200 ms y otros tasks.
- `RUN=1 STAGE=20`: fallo entre adquisición y preparación CS.
- `RUN=1 STAGE=30`: fallo durante consulta SPI/PHY, o interacciones concurrentes.
- `RUN=1 STAGE=40`: fallo durante liberación del mutex.
- `RUN=2 STAGE=50`: la sonda real retornó. Si aparece watchdog después, ocurrió en **otro** lugar y la sonda no explica el reset por sí sola.
- `VALID=0`: registro RTC corrupto o incompleto; no declarar etapa con certeza.
- `RUN=0`: no llegó a iniciar la sonda; investigar autoload/arranque.

**Limitación:** la captura puede reflejar callbacks de otras invocaciones concurrentes a la misma API durante la sonda; si hay ambigüedad, instrumentar la identidad de task como prueba posterior. La persistencia RTC tras todos los tipos de reset no está garantizada.

## Seguridad/alcance

- Recompilar **source-first** en overlay `jwplc_hbench` manteniendo `JWPLC/2.1.0` byte a byte igual. Son 14 archivos de fuentes stageados (solo uno gana nuevas marcas), sin tocar archivos `.a`, APIs, GPIO4, CS físicos, autoload ni Alpha15.
- `--upload-h0-diag-com4 --dev-waiver <ficha_privada>` solo COM4; `--upload` dual rechazado incluso con waiver. MAC, VID/PID y flash 4 MB se verifican antes de la confirmación explícita `G10R AUTORIZO SOLO COM4 ETAPAS LINK SIN BACKUP`. No leer/guardar contenido flash por decisión previa de propietario.
- `COM14` Slave se mantiene tal cual; sin RS-485 ni relés externos. Router desconectado para primera ejecución de la sonda.
- Se guardan build ELF SHA, salida de upload y UART en disco local; no registrar MAC ni identificadores personales en GitHub.
- **No R1–R4**, no adoptar `LINK=-9`, no merge, no release, no declarar solución productiva hasta ubicar raíz y validar fisicamente.

**Operativa:** abrir monitor COM4 115200 antes de prueba si es posible (sin conflicto con upload), obtener línea `G10R_H0_PROBE_BEGIN`, `G10R_H0_PROBE_RETURN` o nuevo boot `G10R_H0_PREVIOUS` y 20–30 s adicionales. Enviar registro. Detener ante cualquier anomalía.
