# Alpha13 — G10-R: campaña integrada de recuperación y resistencia

**Fecha:** 2026-10-16 (Perú).  
**Estado:** `G10-R=PREP_NONPHYSICAL`; **sin autorización de upload ni ensayo físico**. CI N0 [#38855037866](https://github.com/JW-Control/platform-jwplc/actions/runs/38855037866), 2/2 SUCCESS Windows/Ubuntu (13 mutaciones sintéticas, ningún hardware).  
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

**Instrumento de enlace agregado en el sketch R1/R2/R4:** `JWPLC_Ethernet.linkStatus()` devuelve `LinkON=1`, `LinkOFF=2` o `Unknown=0`. `Unknown` puede indicar indisponibilidad SPI y **no cuenta como cable retirado**. En R2 se exige evidencia real de `LinkOFF` en al menos dos heartbeats durante cada ventana y `LinkON` después de reconectar. `JWPLC_Ethernet.isReady()` puede quedar temporalmente activo después de perder PHY; tampoco constituye prueba de que el enlace esté conectado.

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

## 6. Implementación incremental R1/R2/R4 — ejecutables no físicos

**Estado de esta iteración: CI de fuentes y contrato PASS, hardware NO ejecutado.** [Run #38860689736](https://github.com/JW-Control/platform-jwplc/actions/runs/38860689736), HEAD ejecutable `921ca6cd6a94f3874ef425be057e2af287d1feaf`, `completed/success`, **4/4 jobs PASS**: replay sintético **15 mutaciones negativas rechazadas** Windows/Ubuntu; compilación source-first **COM4 + COM14** Windows/Ubuntu con 14 fuentes del overlay, core congelado y FQBN de banco. Dos compilaciones producen ELF/SHA de host distintos; no se exige identidad binaria Windows/Linux, sino identidad de fuentes, core y procedencia, y se registraron ambos ELF en logs.

- `tools/alpha13/sketches/g10_r_integrated/g10_r_integrated.ino`: diagnóstico separado de G10-H. Conserva autoload Ethernet/W5500, microSD, FRAM, RTC, botonera, RS-485/Modbus RTU, Display y TCA/I/O; añade `G10R_BOOT:NONCE`, `G10R_READY` con IP/boot/PHY interpretado como `LINK`, `G10R_HEARTBEAT` con IP/boot/`LINK` y contadores, `G10R_START:<token>:RTU=1`, `G10R_STOP:<token>`. Las órdenes START/STOP afectan únicamente a la sesión de diagnóstico, no a relés ni a cargas. **Es firmware nuevo, todavía NO flasheado.**
- `tools/alpha13/gates/a13_g10_r_build_candidate.py`: staging source-first del mismo overlay G10-I, FQBN temporal `jwplc_hbench:esp32:jwplcbasic`, 14 SHA fuentes, cinco precompilados congelados, atribución de plataforma/bibliotecas, objeto `.d/.o` de SafeAccess y ELF COM4 y COM14. Solo tiene `--selftest` y `--build` **sin upload**.
- `tools/alpha13/gates/a13_g10_r_replay.py`: contrato JSON `G10-R-REPLAY-V1`, modo `--selftest` con negativos sintéticos y `--evaluate <archivo>` **únicamente offline**. Exige >=600 s R1, >=1800 s R4, gaps heartbeat <=3 s, huecos de muestreo tráfico <=20 s, progreso periféricos, boot estable, y tres ciclos R2 (10–15 s link-down, caída **observada** en heartbeat y TCP/UDP, RTU mantenido, readyness <=90 s y cinco operaciones TCP/UDP/RTU consecutivas <=30 s). Clasifica `PASS_REPLAY_CONTRACT` o `REVIEW_STOP`, **nunca** `PASS_PHYSICAL`.
- `.github/workflows/alpha13-g10-r-source-build.yml`: contrato/replay Windows+Ubuntu y compilación Arduino CLI real del candidato COM4 y Slave COM14, sin COM ni upload. Contratos y build se reportan como jobs diferentes.

**Actualización 16/10 (R3 parcial):** se agregó **instrumentación de laboratorio al overlay TEMPORAL** (mismas 14 rutas, pero dos fuentes de SafeAccess extendidas): `LabSocketSnapshot` con `socketIndex/epoch/allocation/ownerCookie/pendingCloseAllocation/valid`, y `labCaptureSockets()` protegida por `BusScope`, resultado `Captured/BusBusy/InvalidOutput`. El sketch puede consultarla con `G10R_SOCKETS:<token>`, imprimiendo `G10R_SOCK` por slot o `G10R_SOCK_STATUS` si SPI está ocupado. Esta API **solo existe en el paquete staged de diagnóstico**, nunca en el package productivo. El ejecutor de compilación liga el símbolo source-first. Además `a13_g10_r_owner_epoch.py` implementa un verificador OFFLINE de transiciones de propietarios/epoch con **14 casos negativos sintéticos**. Estas lecturas son snapshots; **R3 todavía NO comprueba la transición física DNS/DHCP/UDP bajo SPI_BUSY ni la integridad de una secuencia de eventos emitida por firmware**. El primer CI del nuevo código debe corroborarse antes de darlo por válido.

**Ejecutor anfitrión:** `a13_g10_r_field_runner.py` implementa `--selftest`, `--dry-run` y una modalidad `--live` que no realiza uploads, exige un **registro externo verificable de dos cargas G10-R** (`G10-R-AUTHORIZED-UPLOAD-V1`), dos CH340 y respuestas UART G10-R/Slave antes de iniciar. En modalidad live solicita permiso exacto para captura y maniobras **RJ45 exclusivamente manuales**, conserva UART + mediciones PC y somete eventos al replay, **sin afirmar PASS físico automático**. No usar `--live` todavía: **NO se ha creado el gate aprobado de carga con evidencias ni existe autorización de intervención material**, y no hay garantía criptográfica de contenido flash a partir de un protocolo serial. El runner no ejecuta resets GPIO4, power-cycle ni manipulación de relés.

**Próximos requisitos:** formalizar el uploader fuente→ELF→flash con consentimiento separado y atestación documental, asegurar compatibilidad de sesión/arranque y revisión de abortos antes del banco; construir fault injection y telemetría histórica real de R3; y obtener autorización física independiente. **No indicar ejecutar BAT ni conectarse a COM4/COM14 todavía**.

**Nota de procedencia:** el firmware G10-H actualmente grabado en COM4/COM14 no habla necesariamente `G10R_...`; el nuevo sketch R1/R2/R4 requiere autorización de flash futura. La identificación de puerto observada en H es histórica, no una verificación de presencia actual.

## 7. Implementación de los dos ejecutores de campo — preparados, NO ejecutados

**Contrato de build→upload y seguridad:** `tools/alpha13/gates/a13_g10_r_build_candidate.py` ofrece `--build` sin hardware y `--upload` como **operación física distinta**. Esta última solo acepta Windows, Arduino CLI **1.5.1**, la rama de banco sincronizada con `origin` y el package staged de FQBN `jwplc_hbench:esp32:jwplcbasic`. Recompila COM4 y COM14 de forma source-first, comprueba identidad de plataforma y las bibliotecas seleccionadas, core congelado, cinco archives, objeto SafeAccess `.d/.o` y SHA de ambos ELF. Verifica CH340 en COM4/COM14, y exige escribir literalmente `G10R AUTORIZO FLASH COM4 COM14`. **Primero sube Slave COM14, luego master COM4**, registrando `UPLOAD_COM14.log` y `UPLOAD_COM4.log` con SHA y `MANIFEST.json` en `tools/alpha13/results/g10_r_upload_*/`. En caso de fallo parcial conserva el estado real: **sin rollback automático**. Ningún upload se ha realizado ni autorizado con este documento.

**Contrato de captura:** `tools/alpha13/gates/a13_g10_r_field_runner.py` permite `--dry-run`, `--selftest` y, de forma protegida, `--live --upload-evidence <MANIFEST.json>`. Antes de abrir los COM, `--live` exige el manifiesto de un **upload previo y explícito**, SHA correcto de los dos logs, hashes ELF, dos VID/PID CH340 y etiqueta R3 staged; después pide consentimiento diferente: `G10R AUTORIZO CAPTURA COM4 COM14 Y MANIOBRAS RJ45`. Verifica en UART el protocolo periódico `R1R2R4_R3SNAPSHOT_V1` y Slave `G8_P4_READY`, no solo una IP vieja. Captura UART y PC por hilos independientes, R1 de 605 s, R2 tres ciclos manuales de 12 s y recuperación, R4 de 1805 s; solicita al Slave estadísticas `CRC`/solicitudes y conserva trazas y `MANIFEST.json`. **El script NO realiza flash, no manipula cable, no reinicia el W5500, no acciona Q y no certifica por sí solo el firmware mediante el marcador UART**. El evaluador offline y la revisión humana siguen necesarios.

**Endurecimiento del contrato, posterior al primer ejecutor:** el parser host ahora acepta y normaliza **BOOT hexadecimal Arduino con letras mayúsculas**, sin confundirlo con reboot; una captura física debe corresponder al **HEAD exacto** del manifiesto de upload y al worktree tracked limpio; verifica los SHA de ambos sketches y ambos logs de flash, no solamente la cadena de protocolo UART. Las consultas R3 de solo lectura incorporan `G10R_SOCK_BEGIN:T_MS` y `G10R_SOCK_END` por sesión, para agrupar ocho sockets, conservar `BUS_BUSY` e identificar cualquier muestra incompleta antes de interpretarla. El runner recoge snapshots observacionales cada ~15 s durante tráfico, **sin convertirlas en eventos demostrados de cierre de socket**. Hay selftests host para END ausente, SID fuera de rango, BUS_BUSY y pérdida de delimitación; el estado de CI de este cambio debe leerse sobre su SHA.

**Fronteras pendientes antes del uso físico:** revisión final de CI en el HEAD exacto, validación del protocolo en una placa real de banco, decisión explícita sobre estado de firmware previo y forma de restauración, confirmación del conexionado aislado y autorización humana separada. Una declaración en archivo local de upload + respuesta UART no es atestación criptográfica del binario en flash. R3 requiere pruebas de contención `SPI_BUSY` con transiciones reales y correlación `epoch/owner`, no solo snapshots; R5 power-cycle e IDE permanecen fuera de esta autorización.

**Controles reforzados:** el replay R1/R4 exige `RTC=1`, link y servidor listos, heartbeat y RTU/SD/FRAM en progreso y rechazo de ausencia de tráfico. El selftest del runner usa un COM14 falso para detectar errores de terminador LF y manifiestos de upload sintéticos para impedir aceptar un flash parcial. Esas pruebas son **sintéticas** y no sustituyen banco.

## 8. Estado y siguiente acción

```text
G10-I=NONPHYSICAL_PASS_CANDIDATE
G10-H=PASS_PHYSICAL_SMOKE_MANIFEST_REVIEWED
G10-C=CLOSED_DECISION_DEFER_ADOPTION
G10-R=PREP_NONPHYSICAL
G10-R-N0=PASS_SYNTHETIC_CONTRACT_WINDOWS_UBUNTU_RUN_38855037866
G10-R-R1_R2_R4=SOURCE_FIRST_AND_HOST_RUNNER_PREP_NO_PHYSICAL
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
NEXT_GATE=G10-R_FINAL_CI_REVIEW_R3_LIFECYCLE_FAULT_INJECTION_AND_OPERATOR_DECISION
```

**No cerrar G10-R con esta ficha:** sirve para fijar riesgos, alcance y condiciones de preparación; ni el autodiagnóstico sintético ni la CI sin hardware equivalen a R1–R4 reales.
