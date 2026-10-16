# Alpha13 — G10-R: campaña integrada de recuperación y resistencia

**Fecha:** 2026-10-16 (Perú).  
**Estado:** `G10-R=PREP_NONPHYSICAL`; **sin autorización de upload ni ensayo físico**.  
**Rama:** `codex/alpha13-g10-r-recovery-prep`, sobre la rama documental G10-C, **no** sobre producto.  
**Propósito:** cubrir riesgos residuales antes de una eventual nueva decisión de adopción. G10-C permanece cerrado con `CLOSED_DECISION_DEFER_ADOPTION`. No cambiar por anticipado `A13-006=ADOPTION_BLOCKED`.

## 1. Línea base confiable

- **Candidato G10-I:** `a954cabd9c6e37876b59995a41ff0a449308ccd1`. [CI #38843599043](https://github.com/JW-Control/platform-jwplc/actions/runs/38843599043): 5/5 SUCCESS, matriz N3 **11 baseline + 11 candidate**.
- **G10-H:** ejecución física `g10_h_20261015_234401_713114fc`, HEAD `c776ab92388e8765e933bf50cab8215e7c18325b`. `PASS_DUAL_ETHERNET_SPI_RTU`, `exit_code=0`. 47 FC03 TCP OK/0 FAIL; 47 UDP OK/0 FAIL; 49 FC03 RTU Master OK/0 FAIL; 52 solicitudes Slave/0 CRC; SD 56 OK/0 FAIL; FRAM 56 sondeos; RTC válido; 6 eventos de botones; 40 heartbeats. **Smoke acotado ~40 s, no resistencia.**
- **CI ejecutor G10-H:** [#38850064913](https://github.com/JW-Control/platform-jwplc/actions/runs/38850064913): 3/3 SUCCESS Windows+Ubuntu, FQBN temporal `jwplc_hbench:esp32:jwplcbasic` y procedencia de bibliotecas/ELF.
- **Decisión previa:** [G10-C](A13_G10_C_DECISION_Y_CIERRE_20261016.md), `G10-C=CLOSED_DECISION_DEFER_ADOPTION`. No reabrir N1–N3 o repetir H salvo cambio de fuente/flags/core/archives o defecto demostrado.

El operador aportó el texto de `SUMMARY.log` y `MANIFEST.json`; **no** han sido versionados como archivos los `SERIAL_COM4.log`/`SERIAL_COM14.log`. Su ausencia se conserva como limitación de reproducción, no se inventa evidencia.

## 2. Riesgos que motivan G10-R

| ID | Riesgo residual | Qué NO ha acreditado G10-H |
| --- | --- | --- |
| R-NET | Link-loss y reconexión con DHCP | Una IP inicial y 47 operaciones satisfactorias no muestran recuperación |
| R-DNS | DNS, DHCP y `EthernetUDP::stop()` con `SPI_BUSY` | Que cierres diferidos sobrevivan a `finish/cancel`, cuando las máquinas ya no están activas |
| R-SOCK | Epoch/ownership y reciclaje de sockets, `close`, `stop`, cancel y timeout | Ausencia de cierre de socket de otro dueño y ausencia de fugas con varios clientes |
| R-SPI | Fairness bajo SD, FRAM, RTC, Display, TCA/I/O y Modbus RTU | 40 s no determinan starvation, latencias ni contención deliberada |
| R-LIVE | Watchdog, arranque, power-cycle y estabilidad | Un smoke aislado no demuestra recuperaciones repetidas ni resistencia temporal |
| R-IDE | Arduino IDE/package instalado final | El CLI aislado no es una prueba IDE distribuida |

**B01 permanece limitado:** los probes triestado `Read/BusBusy/Stale` son optativos; `connected/status/socketStatus/operator bool` siguen siendo escalares y pueden presentar `BUS_BUSY` como `false/CLOSED` al consumidor legacy. No declarar que se corrigió esa limitación sin nueva implementación/validación de consumidor.

## 3. Diseño de campaña única (fases automáticas internas)

### R0 — N0 preflight sin hardware (implementado)

Ejecutor disponible: `tools/alpha13/gates/a13_g10_r_preflight.py`.

- `--selftest`: corpus **sintético**, 13 mutaciones negativas, no es evidencia física.
- `--check-g10h-manifest <ruta>`: **solo lee** un `MANIFEST.json` G10-H existente; comprueba HEAD G10-I/G10-H, hashes de core/objeto/ELF, 14 fuentes, FQBN temporal, CH340 COM4/COM14, Flash previa, campos de seguridad y mínimos H. No abre serial, no habla por TCP, no compila ni sube.
- Si falla un SHA, FQBN, identidad, guardia o estado, devolver `REVIEW_STOP`, sin reinterpretar como fallo de firmware.
- La selección de COM en futuras fases debe volver a comprobarse **en la sesión real**. Un manifiesto de H no certifica que COM4 y COM14 sigan conectados, ni que los programas sigan cargados.

### R1 — Estabilidad sin intervención física, baseline ampliado

**Propuesta de aceptación, todavía no implementada ni aprobada:** 10 min de carga controlada en red de banco, una conexión Modbus TCP FC03 periódica y UDP echo periódico desde PC, con Master RTU→Slave continuo y accesos read-only a SD/FRAM/RTC, TFT/botonera y TCA/I/O **sin deshabilitar autoload**.

- Escribir hora monotónica por operación, IP y estado de link, latencia p50/p95/p99, heartbeat y resets observados; sincronizar eventos PC/UART.
- Para declarar PASS en estado estable: `tcp_fail=0`, `udp_fail=0`, `RTU_FAIL=0`, `SD_FAIL=0`, `RTC_VALID=1`, sin reinicios inesperados, gap de heartbeat ≤ 3 s y counters de SD/FRAM/RTU crecientes. **Es un objetivo de ingeniería propuesto**, no una medición ejecutada.
- Si la latencia p95 supera 2× el baseline local o 1000 ms, el que sea mayor, registrar `REVIEW` para análisis de contención, **no atribuir causalidad automática**.

### R2 — Pérdida de link y recuperación controlada

Tres ciclos de **desconexión de RJ45 exclusivamente en banco autorizado**, 10–15 s por ciclo, manteniendo USB, RS-485 y alimentación. Requiere indicación inequívoca del operador, no se automatiza por software ni se simula como prueba física.

- Las fallas TCP/UDP **durante** la ventana deliberada de link-down no se computan como regresión; conservar métricas por ventana: `STEADY`, `FAULT_INJECTED`, `RECOVERY`.
- Tras reconectar, permitir hasta 90 s para recuperar IP/servidor y después exigir al menos 5 FC03 TCP y 5 ecos UDP sucesivos sin error en ≤ 30 s, así como continuidad RTU; si no se puede observar IP/epoch nuevos, `REVIEW_STOP=INSTRUMENTATION_GAP`.
- Exigir cero resets inesperados, cero cierre de socket ajeno y contadores no regresivos donde la semántica los prometa. No activar `GPIO4` para facilitar el PASS.
- DHCP puede reasignar IP; usar la IP **observada** tras cada ciclo. **No** asumir que `192.168.0.31` persistirá.

### R3 — Cierre diferido y socket lifecycle

**Necesita instrumentación/firmware diagnóstico distinto de G10-H**, fuente y objeto enlazados al mismo overlay; por ahora **no hay runner ni sketch listos para esta fase**.

- Una batería con 1, 2 y hasta 4 clientes TCP de solo lectura, UDP, DNS local controlado y DHCP; diseñar carga según presupuesto de sockets reales del W5500 y del firmware.
- Fault injection determinista de `SPI_BUSY` en puntos `DNSClient::finish/cancel`, `DhcpClass::finish`, `EthernetUDP::stop`, `EthernetClient::stop`, `cancelConnectAsync`, con una prueba positiva de reintento y negativas de token/epoch reciclado.
- Instrumentar `owner, epoch, lease, CLOSE_PENDING, STALE` **solo en diagnóstico**; cero cierre confirmado durante `SPI_BUSY`, cero double-close, cero cierre del nuevo dueño, liberación final demostrada tras salida de contención.
- No dar PASS al observar solo tráfico TCP/UDP. Un diagnóstico sin telemetría de propiedad/epoch no acredita R-SOCK. No extender APIs públicas Arduino por el mero ensayo.
- El sketch actual `g10_h_integrated.ino` **no** cubre estas invariantes; no reutilizarlo como prueba de ello.

### R4 — Convivencia y resistencia

**Propuesta:** 30 min en banco aislado, 1–2 clientes TCP y eco UDP periódico, RTU cruzado, SD/FRAM/RTC y tareas completas de autoload. Recoger 30 min de series temporales, conteo de errores por ventana y métricas de latencia/jitter, sin salidas Q activas. Cero fallos no explicados en ventanas estables; cero resets no autorizados; requerir evidencia explícita de que Display y TCA/I/O no quedaron sin servicio. No atribuir seguridad o integridad FRAM solo a `FRAM.read8(0)`.

### R5 — Power-cycle/IDE/distribución (frontera de seguridad distinta)

**No incluido en la autorización de R1–R4.** Preparar un procedimiento separado para restauración tras corte, sin accionamientos conectados; no simular un power-cycle desde GPIO4. Arduino IDE/instalación productiva requieren verificación local y no se declaran cubiertos por el FQBN de banco.

## 4. Evidencia, clasificación y parada

Un futuro runner físico consolidado deberá guardar `SUMMARY.log`, `MANIFEST.json`, logs USB COM4/COM14, logs PC TCP/UDP/DNS, hashes fuente→.d→.o→ELF, versión CLI, lista de puertos con VID/PID y horario por fase. Calcular estadísticas desde intervalos monotónicos, distinguir contadores finales de snapshots y anotar hora de cada desconexión.

| Condición | Tratamiento |
| --- | --- |
| Falta de consentimiento físico, cargas o actuadores conectados, puerto no identificado | `PRECONDITION_REVIEW_STOP`: cero acciones |
| Fuente/candidato/biblioteca/precompilado o HEAD distintos | `HARNESS_REVIEW_STOP`: no subir |
| Ausencia de instrumento para DNS, epoch/ownership o nuevo link/IP | `INSTRUMENTATION_GAP`: no afirmar PASS |
| Fallo de red por cable retirado dentro de ventana marcada | Esperado; medir recuperación, no marcar como fallo de firmware aislado |
| Resets imprevistos, watchdog o falta persistente de recuperación | `PHYSICAL_REVIEW_STOP`, conservar trazas sin declarar causa |
| Errores fuera de fault window | `REVIEW_STOP` hasta clasificar `PRODUCT/HARNESS/HARDWARE/ENVIRONMENT/PRECONDITION` |
| Cualquier acceso a salidas Q, escritura SD/FRAM, uso de reset GPIO4 | `SAFETY_STOP`; nunca solicitado por este plan |

**No ejecutar `git clean/reset/rebase/pull` en worktrees sin preflight**. El futuro ejecutor deberá preparar staging temporal con namespace único `jwplc_hbench` o equivalente que **no colisione** con `Arduino15/packages/jwplc_local`; verificar ambas bibliotecas, 5 archives y core antes de pedir permiso. **Ningún upload automáticamente autorizado por `G10-R=PREP`.**

## 5. Entregables y criterios antes de convocar al operador

1. Runner **único**, completo, con `--dry-run`, preflight reproducible y abortos seguros por fase (R1–R4); CI Windows + Ubuntu y negativos para fault windows, epochs, reuso y recuperación. El script R0 actual **no** sustituye ese runner.
2. Sketch de diagnóstico específico R3/R4 con telemetría verificable, sin relés ni escritura SD/FRAM ni GPIO4, autoload normal; compilación source-first y regresión **contra el mismo candidato exacto** que G10-H.
3. Identidad del firmware que ya está en ambas placas, decisión explícita de si se necesita flashear; backup/rollback operativo, no prometer restauración automática.
4. Validación de presupuesto de sockets, red de banco, acceso DHCP/DNS local y riesgo por desconexiones; orden de fases, criterios de abortado, señales de reinicio.
5. **Una autorización expresa del operador** para las acciones físicas elegidas: flash y/o desconexión de cable y/o power-cycle. No agrupar una autorización nueva de power-cycle con una antigua de upload.
6. Solo entonces ejecutar, revisar evidencia y decidir si cambiar `A13-006`; **nunca cambiar estado productivo automáticamente por un PASS de banco**.

## 6. Estado y siguiente acción

```text
G10-I=NONPHYSICAL_PASS_CANDIDATE
G10-H=PASS_PHYSICAL_SMOKE_MANIFEST_REVIEWED
G10-C=CLOSED_DECISION_DEFER_ADOPTION
G10-R=PREP_NONPHYSICAL
G10-R-N0=SOURCE_CONTRACT_READY_CI_PENDING
G10-R-R1_R4=DESIGNED_NOT_IMPLEMENTED
G10-R-R5=NOT_AUTHORIZED
A13-006=ADOPTION_BLOCKED
PRODUCT_MODIFIED=NO
PR_105_106_107=DRAFT
AUTOLOAD_NORMAL=UNCHANGED
GPIO4_RESET=DISABLED
FROZEN_ARCHIVES=5_UNCHANGED
OPENPLC=OUT_OF_SCOPE
HMI_DESIGNER=OUT_OF_SCOPE
TFT_NEW_FEATURES=OUT_OF_SCOPE
NEXT_GATE=G10-R_BUILD_SAFE_RUNNER_AND_INSTRUMENTED_SKETCH
```

**No cerrar G10-R con esta ficha:** sirve para fijar riesgos, alcance y condiciones de preparación; ni el autodiagnóstico sintético ni la CI sin hardware equivalen a R1–R4 reales.
