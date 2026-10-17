> **CIERRE NO FÍSICO VERIFICADO — 17/10/2026.** `B4=CLOSED_NONPHYSICAL_SAFETY_CI_VERIFIED`, código HEAD exacto `33990007405cac2b5af9c110943d94f9201e6add`. [B4 Windows/Ubuntu #39037163647](https://github.com/JW-Control/platform-jwplc/actions/runs/39037163647) **2/2 SUCCESS**: 14 negativas de backup, 5 negativas de R2 UART, falta de ficha, bloqueo de `--upload`/`--live`. La misma fuente supera [N0 #39037163623](https://github.com/JW-Control/platform-jwplc/actions/runs/39037163623) 2/2, [B1 #39037163606](https://github.com/JW-Control/platform-jwplc/actions/runs/39037163606) 2/2, [B2 #39037163624](https://github.com/JW-Control/platform-jwplc/actions/runs/39037163624) 2/2, [TCP124 source-first #39037163653](https://github.com/JW-Control/platform-jwplc/actions/runs/39037163653) 2/2, [G10-R #39037163667](https://github.com/JW-Control/platform-jwplc/actions/runs/39037163667) 5/5 y [N0/N1/N3 #39037163676](https://github.com/JW-Control/platform-jwplc/actions/runs/39037163676) 4/4, incluido N3 candidato 11/11 con artifact `alpha13-b3-n3-39037163676`. **Total 19/19 jobs PASS, ninguno omitido.**

**No confundir cierre técnico con autorización física:** `PREH_STEPS_REMAINING=0` se refiere exclusivamente a preparación de ingeniería; `B4_OPERATOR_FULL_FLASH_BACKUPS=WAIT_OPERATOR`, `G10-R-H=NOT_RUN`, flags flash/live `False`, firma de operador y dos consentimientos todavía pendientes. Cambiar esos flags exige autorización futura, revisión de diff y gates correspondientes; NO se han habilitado en este PR. `NEXT_GATE=G10_R_H0_OPERATOR_BENCH_BACKUPS_AND_SEPARATE_AUTH`.

---

# JWPLC Alpha13 — PRE-H B4: seguridad del banco, backups y autorización

**Estado:** CERRADO en preparación no física, CI verificada. No implica permisos sobre ningún JWPLC.  
**Repositorio:** `JW-Control/platform-jwplc`; PR #109, rama aislada `codex/alpha13-preh-tcp124-review-20261016`.  
**Campaña:** dos JWPLC Basic como máximo, PC Windows y router de banco. **Cuatro clientes TCP son cuatro conexiones de PC, no cuatro placas.**

## 1. Fronteras de seguridad obligatorias

**No ejecutar upload, abrir COM para maniobras, desconectar RJ45 ni energizar equipos por el cierre documental B4.** El flujo requiere después autorizaciones explícitas y diferenciadas del operador. `R3_COMPLETE_PHYSICAL_CAMPAIGN_READY=False` en el builder y `R3_PHYSICAL_INJECTION_READY=False` en el runner siguen activos. Al concluir B4 NO se cambian por CI: cualquier desbloqueo posterior es otro cambio revisado que requiere consentimiento y su gate de no-regresión.

**Primero aislar ambos equipos:** alimentación estable y protegida, sin cargas conectadas a relés Q, sin tensión de red en contactos Q, sin actuadores, motores, contactores ni circuito productivo. Usar un router/switch de pruebas, nunca LAN industrial del cliente. Solo enlace RS-485 de banco, con referencia/terminación conforme a diseño, y RJ45 de la unidad COM4 identificado. **Nunca GPIO4 reset.** R5 (power-cycle manual) excluido.

## 2. Identificación y conservación del firmware instalado

Antes de autorizar flash:

1. Reconocer dos puertos **actuales** de Windows y verificar que realmente correspondan a dos JWPLC independientes. Los nombres `COM4` y `COM14` son los esperados por el runner, **no una suposición válida para cualquier PC**. VID/PID CH340 esperado: `1A86:7523`. Anotar localmente el identificador PnP/ubicación de USB de cada unidad, distinto en ambas, sin publicarlo. Asignar COM4 = maestro diagnóstico y COM14 = esclavo RTU. Detenerse si están invertidos, no existen, se reasignan o hay conflicto.
2. Con banco aislado y solo tras consentimiento de respaldo, usar una herramienta ESP32 compatible (p. ej. esptool) **exclusivamente para identificar el tamaño real de flash y leerlo entero**, sin erase ni write. No asumir 8 MB por especificaciones históricas. Guardar un **binario completo distinto por placa**, fuera del repositorio, fuera de GitHub y en una carpeta protegida. La lectura de flash puede reiniciar/interrumpir la aplicación actual; no realizarla en operación industrial.
3. Confirmar tamaño físico detectado, igualdad exacta de longitud del archivo con ese tamaño, cálculo local SHA-256 y que ambos respaldos son legibles. Guardar versión de herramienta y logs de lectura locales. No aceptar dos rutas idénticas ni dos identificadores USB iguales.
4. Preparar y revisar el procedimiento de restauración del **binario original completo** para el mismo chip, puerto, tamaño y configuración. **No ejecutar restauración como prueba**: sería escritura física nueva, con consentimiento separado. Anotar que el runner G10-R **no realiza rollback automático** y que su mensaje original confirma este límite. Preservar backup también fuera del directorio temporal; no confiar en un ELF de CI como firmware anterior.
5. Completar la ficha local generada por `a13_g10_r_b4_bench.py`. **No subir a GitHub la ficha con datos PnP ni los backups.** La verificación local de SHA/tamaño no prueba criptográficamente el contenido real de la placa: el operador debe confirmar lectura física correcta.

Comandos **solo preparatorios y offline**, que no abren COM:

```powershell
py tools/alpha13/gates/a13_g10_r_b4_bench.py --selftest
py tools/alpha13/gates/a13_g10_r_b4_bench.py --template "C:\\JWPLC_Bench\\A13_G10R_B4_LOCAL.json"
# Tras obtener ambos backups en un banco ya autorizado y completar los campos:
py tools/alpha13/gates/a13_g10_r_b4_bench.py --check "C:\\JWPLC_Bench\\A13_G10R_B4_LOCAL.json"
```

Esquema `JWPLC-A13-G10R-BENCH-BACKUP-V1`: roles, VID/PID, fingerprint USB, tamaño flash detectado, ruta binaria, SHA256, confirmaciones de lectura/restauración y diez verificaciones de aislamiento. Ficha en blanco = `REVIEW_STOP`. El positivo se llama `READY_FOR_SEPARATE_AUTH_ONLY`, **no** «flash autorizado». Los campos `flash_consent`, `capture_and_rj45_consent` y `power_cycle_consent` deben permanecer `NOT_GIVEN`; las autorizaciones reales se recogerán aparte, en el momento de las operaciones.

## 3. Dos autorizaciones independientes

**Puerta A — carga de firmware de laboratorio:** después de respaldos locales verificables, identidad de puertos, estado del banco y CI de código idéntico, el operador debe autorizar explícitamente flash COM14 y COM4. `a13_g10_r_build_candidate.py --upload` exige `--bench-evidence` y después tiene sus propias guardias físicas y diálogo de consentimiento. En el estado actual `R3_COMPLETE_PHYSICAL_CAMPAIGN_READY=False` bloquea la carga aun con ficha completa. **No cambiarlo ahora.**

**Puerta B — captura y maniobras:** después de upload comprobado (logs, hashes ELF, HEAD, puerto/VID/PID y handshake de firmware), autorización nueva para COM4/COM14 + manipulación manual de **solo RJ45 COM4**. `a13_g10_r_field_runner.py --live` exige `--upload-evidence`, `--bench-evidence`, comprueba evidencia y mantiene bloqueo `R3_PHYSICAL_INJECTION_READY=False` hasta aprobación posterior. No concede permisos implícitos por haber autorizado Puerta A.

**Puerta C — restauración y R5:** cada escritura de backup, reinicio voluntario o power-cycle se autoriza independientemente. Nunca se añade de forma automática a R1–R4.

## 4. Una sola campaña integrada (cuando esté autorizada)

| Fase | Secuencia y duración | Evidencia y aborto |
|---|---|---|
| R1 | ≥605 s de tráfico de referencia, sin tocar RJ45 | UART maestro/esclavo, TCP/UDP, RTU, I/O, RTC, SD/FRAM, TFT |
| R2 | Tres ciclos: desconexión RJ45 COM4 y observación hasta 12 s; reconexión y recuperación observada dentro de 96 s | El runner exige confirmación **textual** del operador + heartbeat COM4 fresco `LINK=2` y después `LINK=1, ETH=1, DHCP_STATE=5, IP válida`. Sin telemetría real = `REVIEW_STOP` |
| R3 | Cinco sondas reales source-backed: UDP, DNS, TCP, DHCP_CANCEL, DHCP_FINISH; tráfico PC concurrente 1/2/4 sockets | Se exige token/epoch/owner/allocation, lease, cierre inmediato o diferido, ring sin pérdidas y ausencia de reciclado prematuro. No confundir 4 conexiones con 4 sesiones Modbus atendidas en paralelo |
| R4 | ≥1805 s bajo tráfico y coexistencia normal de todos los periféricos | Comprobar ausencia de watchdog/bloqueo, avance TFT H3E1, edad scan I/O/TCA y RTC, FRAM/SD/RS485/RTU |
| R5 | **NO INCLUIDO** | Power-cycle y recuperación de firmware original siempre separados |

**Duración:** mínimo `605 + 3×(12+96) + 1805 = 2734 s`, es decir, unos **46 minutos**, más R3, preparación y maniobras. Reservar aproximadamente una hora o más; no truncar R1/R4.

Observaciones de DHCP: el contador `RENEW/REBIND` registra intentos y éxitos/fallos si suceden, pero la duración del lease del router puede impedir observarlos. Si no ocurren en la ventana autorizada, marcar `NOT_OBSERVED` y no fingir PASS. La ruta de cancelación DHCP de laboratorio no equivale a renovación real. La métrica I/O procede de edad del escaneo cacheado, no de una lectura individual independiente del TCA.

## 5. Aborto obligatorio y conservación de evidencias

Detener R1–R4 en `REVIEW_STOP`, pérdida de COM, cambio inesperado de BOOT/HEAD, watchdog/reset no pedido, caída permanente del enlace, pérdida de alimentación, relés/cargas activos, brownout, sobrecalentamiento, anomalía del bus RS-485, CRC, fallos de SPI, `trace dropped/overrun`, callback con propietario ajeno, doble cierre, timeout de cierre/lock o ausencia de progreso periférico. **Ante problema con RJ45, reconectarlo manualmente; ante problema eléctrico, utilizar el corte de alimentación seguro de banco**. No disparar GPIO4 ni reinicio remoto. El runner guarda `REVIEW_STOP` y el operador puede abortar con Ctrl+C; no omitir revisión manual del sistema al salir.

Conservar en carpeta de trabajo protegida `SUMMARY.log`, `MANIFEST.json`, logs UART COM4/COM14, reportes de tráfico PC, cronología de acciones RJ45, snapshots/trazas W5500, evidencia ELF/`.d`/`.o`, hashes de firmware y upload, incidentes y valoración humana. No subir backups crudos ni fingerprints a PR público.

## 6. Decisión PRE-H B4

**Cierre de preparación no física** requiere CI de negativos de ficha, CLI fail-closed `--upload/--live` sin autorización, evaluación R2 por heartbeat real, conservación de todos los periféricos, código/archivos protegidos intactos y documento operativo completo. **No** requiere ni permite realizar backups/validaciones físicas sin el operador.

```text
B4_NONPHYSICAL_PROCEDURE=READY_AFTER_CI
B4_OPERATOR_HARDWARE_IDENTITY=WAIT_OPERATOR
B4_OPERATOR_FULL_FLASH_BACKUPS=WAIT_OPERATOR
B4_OPERATOR_RESTORE_PLAN_CHECK=WAIT_OPERATOR
B4_FLASH_CONSENT=NOT_GIVEN
B4_CAPTURE_RJ45_CONSENT=NOT_GIVEN
B4_POWER_CYCLE_CONSENT=NOT_GIVEN
R3_COMPLETE_PHYSICAL_CAMPAIGN_READY=FALSE
R3_PHYSICAL_INJECTION_READY=FALSE
G10-R-H=NOT_RUN
A13-006=ADOPTION_BLOCKED
```
