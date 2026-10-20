# Alpha13 G10 — preparación segura del gate físico del package promovido

**19/10/2026. Estado: `PREPARED_READ_ONLY / UPLOAD_NOT_AUTHORIZED / R1_R4_NOT_RUN`.**

**Producto bajo evaluación:** fuentes promovidas **en PR #124 Draft**, rama `codex/alpha13-g10-promote-functional-20261019`; 10 modificadas + 2 SafeAccess añadidas y F1 `w5100.cpp` conservado. La base Alpha13 sigue en `0344ff9cbe42e9efa65e7d4caabb60ce15464270`. El firmware de **COM4 instalado por F1 no contiene la integración G10 productiva del PR #124**; su éxito H0 no certifica este código nuevo.

## 1. Evidencia reutilizable sin recompilar

- Gate N0 Windows/Ubuntu, N2 ambos FQBN y N3 11/11: [CI #39329439143](https://github.com/JW-Control/platform-jwplc/actions/runs/39329439143), 3/3 jobs PASS.
- N1 contratos nativos del compositor funcional y **paridad de 14/14 archivos** con la rama promovida: [CI #39358616596](https://github.com/JW-Control/platform-jwplc/actions/runs/39358616596), 2/2 PASS.
- El diff desde SHA del gate N2/N3 hasta el HEAD documentado de PR #124 **no cambia ninguno de los doce archivos `JWPLC/` ni el compilador usado**.
- E1 `PENDING_GDB_UNAVAILABLE`, E2 `PENDING_SDKCONFIG_NOT_LOCATED` en esa CI; no se les asigna PASS sin evidencia externa.
- Anteriores pruebas físicas G10-H sobre un **overlay experimental** siguen válidas para ese artefacto y SHA histórico, pero **no se transfieren como prueba física del package promovido**.

## 2. Preflight obligatorio antes de pedir permiso para upload

**No ejecutar mientras no exista autorización de banco específica.**

1. Validar el HEAD **exacto** del nuevo candidato, estado Git limpio y lista de 12 archivos. Sin builds desde una rama con merges futuros o fuentes sucias; F1 `JWPLC_ETH_CS=5` y protección GPIO6–11 inalterados.
2. Preparar sketch de prueba que **utilice el package G10 real**, sin copiar su instrumentación experimental `h0_w5100_stage`, RTC checkpoints ni `friend class` de laboratorio. Mantener autoload Display, Ethernet, SD, FRAM, RTC, botonera, RS-485, Modbus RTU y TCA/I/O; comprobar APIs existentes.
3. En el PC del operador, usar plataforma Arduino CLI aislada con namespace temporal para evitar la precedencia Boards Manager/Sketchbook (F132), comprobar en los logs **rutas de las bibliotecas reales**, GCC `.d`→`.o`→ELF, SHA de fuentes, y que los archives congelados coincidan. No asumir que una compilación que devuelve 0 seleccionó los fuentes correctos.
4. Confirmar físicamente identidad, VID/PID CH340, MAC **contra un registro previo independiente**, 4 MB flash y rol. No publicar identificadores privados. **COM4 (Master) y COM14 (Slave) son placas distintas**; no usar la MAC de COM4 para autorizar COM14.
5. Validar banco sin máquinas, sin tensiones ni cargas en relés Q, red aislada, alimentación segura, puertos libres y consentimiento de pérdida del firmware actual antes de cada upload. La excepción documental anterior de **no respaldar placas de desarrollo** no equivale a permiso actual de sobreescritura ni autoriza G10-R R1–R4.
6. Si cualquier requisito falla, `REVIEW_STOP` y preservar firmware y logs. No borrar flash, cambiar GPIO4 ni generar `bootloader.bin` como definitivo.

## 3. Plan secuencial de aceptación física, pendiente de consentimiento

| Gate | Riesgo a observar | Regla de seguridad |
| --- | --- | --- |
| G10-PH0 — identidad y sketch producto | Cada UART demuestra firmware **promovido** y no el F1 anterior u overlay; roles COM4/COM14 reales | No subir sin hashes/FQBN/consentimiento específicos |
| R1 — smoke sostenido | Modbus TCP y UDP con RTU y accesos cooperativos SD/FRAM/RTC/Display/TCA/I/O; medir latencia y watchdog | Red industrial desconectada, sin actuadores |
| R2 — pérdida/restablecimiento de link | PHY, DHCP, DNS y recuperación de TCP/UDP al reconectar | Eventos manuales documentados, abortar ante watchdog |
| R3 — lifecycle / socket ownership | Timeout, cierre diferido, BUS_BUSY, stale lease y reciclaje de sockets en condición controlada | No introducir instrumentación de laboratorio en producto por conveniencia |
| R4 — resistencia | Duración, repetición, memoria/sockets, starvation SPI y estabilidad sostenida | Ventanas y umbrales deben tomarse del protocolo G10-R vigente antes de ejecutar |

**La campaña autorizada para F1/H0 (10 EN + 10 power-cycles) no autoriza G10-PH0 ni R1–R4.** No usar automáticamente el runner experimental que comienza por COM14: genera overlay laboratorio y podría alterar firmware ajeno al objeto de esta promoción.

Antes de implementar o lanzar cualquier runner físico del package se necesita una **nueva confirmación del propietario sobre el alcance de las placas y respaldo**. La autorización debe delimitar COM4 sola o COM4+COM14; el estado actual es **ambas sin permiso de upload G10**.

## 4. Criterios de avance

Para cerrar G10 no basta con N0–N3: se requiere identidad de firmware del package **en hardware**, evidencia física reproducible de los riesgos R1–R4 que se aprueben, resultados con `PASS` / `REVIEW_STOP` sin inferirlos, revisión del PR #124 y decisión expresa de merge. Se conservan las limitaciones E1/E2 hasta su resolución o aceptación explícita.

```text
G10_PACKAGE_NONPHYSICAL=PASS_N0_N1_N2_N3
G10_PRODUCT_BRANCH_ONLY=YES_PR124
G10_PHYSICAL_RUNNER_FOR_PROMOTED_PRODUCT=NOT_IMPLEMENTED
G10_PHYSICAL_UPLOAD_AUTHORIZATION=NO
G10_PHYSICAL_COM4=NOT_RUN
G10_PHYSICAL_COM14=NOT_RUN
G10_R1_R2_R3_R4=NOT_RUN_NOT_AUTHORIZED
G10_BASE_MERGE=NOT_RUN
NEXT_GATE=G10_PHYSICAL_SCOPE_AND_SAFETY_APPROVAL_THEN_RUNNER_N0
```


## 5. Decisión COM4 + COM14 y router, kit productivo

El propietario aprobó preparar **ambas** placas. No equivale a permiso de flash ni a R1–R4. El kit versionado en `tools/alpha13/g10_promoted_bench/` contiene firmware Master/Slave con Ethernet en ambos y banco de router de dos IP DHCP, una sonda Modbus TCP FC03 de **solo lectura** para PC, y compilación aislada por FQBN temporal `jwplc_g10ph0:esp32:jwplcbasic`.

- [CI dual #39368656011](https://github.com/JW-Control/platform-jwplc/actions/runs/39368656011): Windows y Ubuntu **PASS 2/2**, ambos sketches fuente→dependencia GCC→objeto→ELF, `G10P_UPLOAD=NOT_RUN COM4=NOT_OPENED COM14=NOT_OPENED`.
- [CI FC03 #39369086270](https://github.com/JW-Control/platform-jwplc/actions/runs/39369086270): Windows y Ubuntu **PASS sintético 2/2**, sin acceso de red. El test con router requiere ambos conectados realmente al mismo switch/router de laboratorio, IPs privadas distintas y PC en esa red.
- Gate físico individual: registrar antes de cada upload **COM4 Master** y **COM14 Slave**, MAC/CH340/4 MB verificadas con referencia independiente local, ausencia de cargas Q, decisión explícita sobre backup/pérdida de firmware anterior. COM14 **sin microSD**; firmware Slave no accede a SD.
- **No usar** el sketch `g10_r_integrated.ino` o el ejecutor de overlay `a13_g10_r_build_candidate.py` para probar el package promovido.

```text
G10_PH0_SCOPE_COM4_COM14=APPROVED_FOR_PREPARATION
G10_PH0_BUILD=PASS_2_CI_OS
G10_PH0_MODBUS_TCP_SYNTH=PASS_2_CI_OS
G10_PH0_ROUTERS_TEST=NOT_RUN
G10_PH0_UPLOAD_BOTH=NOT_AUTHORIZED
G10_PH0_R1_R4=NOT_AUTHORIZED
```
