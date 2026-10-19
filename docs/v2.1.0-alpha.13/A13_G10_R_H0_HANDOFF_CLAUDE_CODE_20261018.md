# JWPLC Basic v2.0.0 — Alpha13 / G10-R / H0 — Transferencia técnica a Claude Code

**Fecha de corte:** 18 de octubre de 2026, aproximadamente 19:07 hora de Perú.  
**Repositorio:** `JW-Control/platform-jwplc` (https://github.com/JW-Control/platform-jwplc)  
**Hito:** `v2.1.0-alpha.13`, robustez Ethernet/Modbus TCP/SPI, puerta previa a campaña física R1–R4.  
**Clasificación:** incidente físico **REPRODUCIDO / SIN CORRECCIÓN**, **G10-R-H=REVIEW_STOP** y **A13-006=ADOPTION_BLOCKED**.  
**Este documento es evidencia/handoff, NO aprobación para escribir firmware, fusionar PR ni publicar release.**

## 0. Resumen ejecutivo

Con un JWPLC Basic real (COM4 Master), el sketch diagnóstico G10-R entra en `TG1WDT_SYS_RESET` en el **primer** `G10R_READY` cuando éste consulta físicamente `JWPLC_Ethernet.linkStatus()` durante el arranque. El mensaje se trunca después de `LINK=`. Al desactivar esa lectura adicional de sketch, el mismo equipo funciona sin watchdog observado y obtiene IP `192.168.0.31`, pero eso **no constituye reparación**.

Los ensayos instrumentados A–F localizaron progresivamente el tramo: `JWPLC_Ethernet.linkStatus()` → `Ethernet.linkStatus()` → `W5100Class::getLinkStatus()` → `W5100Class::init()` → **después del retorno de `SPI.begin()` y antes del retorno de `initSS()`**. H0-F reportó literalmente `RUN=1 ATTEMPT=1 LAST_STAGE=63 VALID=1`. Bajo la rama ESP32 de `w5100.h`, `initSS()` usa `pinMode(ss_pin, OUTPUT)`. Esta es una **ventana de ejecución delimitada, NO una demostración de que pinMode sea la causa**: podrían intervenir el valor del CS, acceso desde distintas tareas, doble inicialización, bloqueo de GPIO, temporización o símbolos/backend enlazados.

**No recomendar simplemente eliminar `linkStatus()`**, porque es API existente, y el autoload también lee el enlace. No confundir `LINK=-9` (centinela inventado en sketch de laboratorio) con estado PHY real.

## 1. Fuentes autoritativas y ramas

Orden recomendado:
1. Código **en la rama específica**, `docs/v2.1.0-alpha.13/ALPHA13_STATUS.md` y este handoff.
2. `docs/v2.1.0-alpha.13/A13_H0_WDT_LINK_ISOLATION_20261017.md`, `A13_H0_LINK_STAGE_TRACE_20261017.md`, `A13_H0_READY_WINDOW_20261017.md`, `A13_H0_W5100_PHY_STAGE_20261018.md` y `A13_H0_W5100_INIT_STAGE_20261018.md`.
3. Sketches `tools/alpha13/sketches/g10_r_integrated/g10_r_integrated.ino` (Master), `tools/alpha13/sketches/g8_p4_slave/g8_p4_slave.ino` (Slave).
4. Implementaciones en `JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/JWPLC_Ethernet.cpp`, `JWPLC_W5x00_Ethernet.cpp`, `utility/w5100.cpp`, `utility/w5100.h`, core `jwplc_spi_bus.cpp`, `peripherals_init.cpp` y los scripts del overlay.
5. CI exacta vinculada al HEAD, logs reales y `MANIFEST.json` local para afirmar PASS físico; los archivos históricos de status pueden incluir estados PRE-H superados, leer **lo más reciente primero**.

| Papel | Rama / PR | Estado |
|---|---|---|
| Candidato PRE-H original | `codex/alpha13-preh-tcp124-review-20261016` / [#109](https://github.com/JW-Control/platform-jwplc/pull/109) | Draft, sin merge |
| H0-B, omisión lectura LINK de sketch | `codex/alpha13-h0-wdt-link-isolation-20261017` / [#110](https://github.com/JW-Control/platform-jwplc/pull/110) | Draft, físico observado |
| H0-C, sonda real una vez + RTC | `codex/alpha13-h0-wdt-link-stage-20261017` / [#111](https://github.com/JW-Control/platform-jwplc/pull/111) | Draft, físico observado |
| H0-D, READY hasta 8 accesos | `codex/alpha13-h0-wdt-ready-window-20261017` / [#112](https://github.com/JW-Control/platform-jwplc/pull/112) | Draft, WDT reprodujo intento 1 |
| H0-E, marcas W5100 31–39 | `codex/alpha13-h0-w5500-phy-stage-20261018` / [#113](https://github.com/JW-Control/platform-jwplc/pull/113) | Draft, WDT etapa 32 |
| **H0-F, marcas init 60–77** | `codex/alpha13-h0-w5100-init-stage-20261018` / [#114](https://github.com/JW-Control/platform-jwplc/pull/114) | **HEAD de código probado `f0b9075130158b99d89e0f9a574d964236614b78`**, físico WDT etapa 63 |

Los PR #110–#114 son **ramas experimentales encadenadas**, no cinco modificaciones adoptadas al producto. Una documentación posterior al HEAD de código no permite trasladar sus CI automáticamente sin verificar qué cambió. `v2.1.0-alpha.15-HmiDesigner` es trabajo **independiente** en Windows y no debe sufrir checkout/merge accidental. No usar `main` como autoridad para Alpha13.

## 2. Banco físico, autorización y resultados generales

- Hardware de desarrollo: ESP32-D0WD-V3 rev 3.1, cristal 40 MHz, flash identificada de **4 MB** por placa.
- `COM4` **Master**, con microSD; `COM14` **Slave**, sin microSD. Los MAC ya se validaron localmente; **no publicarlos en GitHub**.
- Dos placas propias de desarrollo, operador autorizó **explícitamente** sobrescritura sin backup full-flash por cada carga. **No extender esta renuncia a otras unidades ni a nuevas operaciones sin consentimiento**. No se tocó contenido microSD.
- Banco USB y router de ensayo aislado durante observaciones; sin cargas Q externas ni RS-485. No se realizó campaña R1–R4 ni pruebas de falla inyectada R3.
- Original H0: subida de ambas placas en HEAD `701196fc5f1b998fcc1b4c0082e9c75e86672a5a` → upload PASS de COM4 y COM14, TFT encendió correctamente. COM4 empezó a reiniciarse cada ~2 s con `rst:0x8 (TG1WDT_SYS_RESET)`; secuencia `G10R_READY:... LINK=` incompleta. COM14 no fue reprogramado en B–F.
- Tras watchdog, los sketches instrumentados desarman la sonda al siguiente boot utilizando `RTC_NOINIT_ATTR` cuando se conserva; IP/TCP/UDP/RTU luego aparecen como activos, **pero esos flags no constituyen prueba de una campaña TCP/Modbus bajo carga**.
- `rst:0x1 (POWERON_RESET)` observado tras reset manual es **distinto** del `TG1WDT_SYS_RESET` inducido por la consulta.

## 3. Cronología A–F y conclusiones delimitadas

| Ensayo | Cambio de diagnóstico, solo stage/ sketch | Evidencia física | Interpretación |
|---|---|---|---|
| **A / original** | `JWPLC_Ethernet.linkStatus()` en READY/HEARTBEAT | WDT repetitivo ~2 s, UART truncado en `LINK=`; con y sin RJ45 | La consulta desde READY participa en la reproducción; no localiza qué instrucción |
| **B** | Sustituye dos consultas físicas del sketch por `LINK=-9` | `BOOT=94B40EC4` constante ~30 s, IP `192.168.0.31` | Sin lecturas adicionales del sketch no se reprodujo WDT en esa ventana. `-9` NO es enlace real |
| **C** | Una consulta física real ~1,5 s tras `setup()`, marcadores wrapper 10–50 en RTC | `G10R_H0_PROBE_RETURN:PHY=1 STAGE=50`, luego ≥25 s sin WDT | PHY, mutex y API sí retornan en al menos una llamada tardía; no hay fallo incondicional |
| **D** | Consulta en **primer READY**, máximo 8; guard RTC | `TG1WDT_SYS_RESET`; `RUN=1 ATTEMPT=1 LAST_STAGE=30 VALID=1` | Ya adquirió mutex JWPLC y deseleccionó CS; no terminó llamada interna `Ethernet.linkStatus()`. NO 8/8 PASS |
| **E** | Mismo primer READY, marcas internas `W5100.getLinkStatus()` 31–39 | `TG1WDT_SYS_RESET`; `RUN=1 ATTEMPT=1 LAST_STAGE=32 VALID=1` | Entró al backend pero no terminó `W5100.init()` |
| **F** | Mismo primer READY, etapas `W5100.init()` 60–77 | `TG1WDT_SYS_RESET`; `RUN=1 ATTEMPT=1 LAST_STAGE=63 VALID=1` | **`SPI.begin()` retornó; no alcanzó marca posterior a `initSS()`** |

**Log F textual relevante (18/10 19:07):**

```text
G10R_H0_F_INIT_STAGES=60_TO_77
G10R_H0_F_READY_PROBE=ONCE_TASK_FILTER_RTC_NO_R1_R4
G10R_H0_TRACE=ARMED_SINGLE_READY_ON_INITIAL_BOOT
G10R_READY:IP=0.0.0.0 TCP=0 UDP=0 RTU=1 BOOT=A666A262 LINK=
rst:0x8 (TG1WDT_SYS_RESET)
G10R_H0_PREVIOUS:RUN=1 ATTEMPT=1 LAST_STAGE=63 VALID=1
G10R_H0_TRACE=REBOOT_NO_AUTO_REPEAT
G10R_READY:IP=192.168.0.31 TCP=1 UDP=1 RTU=1 BOOT=B1D755A8 LINK=-9 VER=R1R2R3R4_FAULT_LAB_V2
```

**Interpretación correcta del stage 63:** en el instrumentador `tools/alpha13/gates/a13_g10_r_h0_init_stage.py`, `h0W5100Stage(62)` se ejecuta antes de `SPI.begin()`, **63 inmediatamente después**, y **64 después de `initSS()`**. El código en `utility/w5100.h`, rama preprocesada ESP32 (fallback `#else`), es:

```cpp
inline static void initSS() {
    pinMode(ss_pin, OUTPUT);
}
```

No se ha verificado aún el **valor efectivo** de `ss_pin` en la sonda, la definición/selección final de CS, ni si el watchdog indica espera dentro de `pinMode()` o starvation/concurrencia de otro componente. Tampoco se ha demostrado un deadlock FreeRTOS con dump de tareas/backtrace. Evitar afirmar que `pinMode` o `SPI.begin` está defectuoso.

## 4. Ruta de ejecución concreta y la cuestión del estado cacheado

```text
Sketch g10_r_integrated.ino [primer READY]
  -> JWPLC_Ethernet.linkStatus() [mutex JWPLC SPI acquire(200)]
    -> jwplcSPI_deselectAll()
    -> Ethernet.linkStatus()
      -> W5100Class::getLinkStatus()
        -> W5100Class::init()
           static bool initialized = false
           if (initialized) return 1
           SPI.begin()       [stage 63 ALCANZADO]
           initSS()          [stage 64 NO ALCANZADO]
             pinMode(ss_pin, OUTPUT) [rama ESP32, en header]
           resetSS()
           SPI.beginTransaction()
           ... detección W5200 / W5500 ...
```

`JWPLC_Ethernet.service()` desde el autoload ya realiza lecturas `Ethernet.linkStatus()` para mantener estado y gestionar DHCP. `JWPLC_Ethernet.linkStatus()` y `linkUp()` hacen lecturas **físicas nuevas**; `statusString()` también puede llamar a la primera cuando Ethernet está READY. `JWPLC_Ethernet.isReady()` y `runtimeState()` leen estado interno disponible **sin nueva lectura PHY** y pueden ser preferibles para telemetría no crítica, pero `runtimeState` no equivale exactamente a `LinkON/LinkOFF/Unknown`. **No existe, en la revisión actual, una API pública de caché PHY exacta que sustituya silenciosamente `linkStatus()`.**

La función `W5100Class::init()` usa `static bool initialized` **no atómico** que solo se pone en true al completar la inicialización. Si múltiples tareas alcanzan esta ruta demasiado pronto, la hipótesis de doble entrada/reentrancia debe verificarse, no suponerse confirmada. `EthernetClass::init(sspin)` delega en `W5100.setSS(sspin)`; estudiar pin CS real y fuente de `SS_PIN_DEFAULT`. Existe un segundo nivel de bloqueo del driver SPI por `SPI.beginTransaction()`, pero el stage 63 aparece **antes de esa instrucción dentro de init**.

Preguntas prioritarias para revisión independiente:
1. ¿Quién llama por primera vez `W5100.init()` y cuándo? ¿Puede entrar desde dos tareas (`autoload` y READY) mientras `initialized==false`? ¿Se respeta el ownership SPI de punta a punta?
2. ¿Qué `ss_pin` pasa a `pinMode` durante esa invocación? ¿Proviene de `Ethernet.init(_csPin)`? Verificar macro efectiva, mapping, pin usado y efectos de volver a configurar CS.
3. ¿El compilador realmente compila la rama `#else` de `initSS()` para ESP32 y qué implementation de `pinMode` se enlaza? Confirmar fuente→objeto→ELF/mapa, símbolos W5100 y que no hay definiciones duplicadas.
4. ¿Por qué la sonda tardía C retorna `STAGE=50` pero primer READY D/E/F entra a WDT? Verificar orden de inicialización, tareas y cortes de scheduler, **sin introducir prints dentro del mutex**.
5. Si se necesita parche productivo: ¿cómo evitar re-init indebida y restaurar lectura confiable del enlace manteniendo **contratos y APIs existentes**? Comparar alternativa cacheada como mejora separada, sin sustituir API real.

## 5. CI, manifiestos y alcance

**H0-F código físico cargado:** `f0b9075130158b99d89e0f9a574d964236614b78`.

**CI de ese HEAD antes del upload: 15/15 jobs PASS en cinco workflows**, con N3 **11/11 sketches** y Windows/Ubuntu:
- [B2 DHCP 2/2](https://github.com/JW-Control/platform-jwplc/actions/runs/39177861353)
- [B4 seguridad 2/2](https://github.com/JW-Control/platform-jwplc/actions/runs/39177861327)
- [G10-R source-first, 5/5](https://github.com/JW-Control/platform-jwplc/actions/runs/39177861376)
- [TCP124 source-first, 2/2](https://github.com/JW-Control/platform-jwplc/actions/runs/39177861345)
- [B3 N0/N1/N3, 4/4](https://github.com/JW-Control/platform-jwplc/actions/runs/39177861330)

**Upload H0-F confirmado por terminal del operador:** `G10_R_H0_E_W5100_SOURCE_TO_OBJECT=PASS`, `G10_R_STAGE=PASS_15_SOURCES`, `G10_R_BUILD_COM4=PASS_SOURCE_FIRST`, `G10_R_H0_TARGET_MAC_AND_FLASH4MB=PASS_BEFORE_WRITE`, `G10_R_H0_DIAG_UPLOAD=PASS_COM4_ONLY`, `G10_R_H0_COM14=NOT_TOUCHED`. El marcador `H0_E_W5100_SOURCE_TO_OBJECT` conserva la etiqueta E en el builder reutilizado: **en F también hubo 18 marcas adicionales de init** según selftest y composición de fuentes.

Manifiesto físico **local**, no commitear: `tools/alpha13/results/g10_r_upload_20261018_190302_439fb824/MANIFEST.json` dentro del worktree H0-F Windows. Copiar solo SHA y métricas no sensibles para PR. No confundir `G10_R_PHYSICAL_TEST=NOT_RUN` del **manifiesto de build** (campaña R1–R4 no ejecutada) con el **upload H0-F** observado en terminal.

Scripts clave:
- `tools/alpha13/gates/a13_g10_r_build_candidate.py`: staging 15 fuentes, verificación ELF SHA/objetos y upload exclusivo COM4 bajo waiver/consentimiento; `R3_COMPLETE_PHYSICAL_CAMPAIGN_READY=False`.
- `tools/alpha13/gates/a13_g10_r_h0_link_stage.py`: puntos de wrapper mutex/PHY.
- `tools/alpha13/gates/a13_g10_r_h0_w5100_stage.py`: `getLinkStatus` stages 31–39.
- `tools/alpha13/gates/a13_g10_r_h0_init_stage.py`: `init` stages 60–77.
- `tools/alpha13/gates/a13_g10_r_b3_scope.py` y workflows PRE-H: alcance/gates/negativos.

Los overlays solo se construyen en el clon `jwplc_hbench:esp32:jwplcbasic`, **sin cambios de fuentes productivas `JWPLC/2.1.0`, archives precompilados, core normal, GPIO4 o Alpha15**. La carga de diagnóstico sí reemplaza temporalmente firmware del Master de desarrollo: no confundir con adopción productiva.

## 6. Instrucciones de continuidad para Claude Code

**Hito siguiente propuesto: `H0_ROOT_CAUSE_READ_ONLY_REVIEW` antes de crear otra sonda.** Evitar una secuencia automática de H0-G, H0-H… sin primero analizar conjuntamente historial A–F y código. Revisar en modo read-only: control de reentrancia de `W5100.init()`, CS real, tareas, inicialización en autoload y uso de SPI, símbolos/backend y precompilación. Formular hipótesis falsables y **plan mínimo de validación**.

Si se demuestra un defecto y el usuario autoriza implementación, trabajar en **rama aislada nueva** desde la referencia correcta, proponer el cambio mínimo sin romper API ni quitar autoload, crear N0/N1 y N2 source-first (con negativas) y ejecutar N3 cuando corresponda. Mostrar diff, manifest, gates y límites. **No flashear, abrir COM, cambiar cableado, hacer reset físico, editar Alpha15, fusionar PR ni publicar releases** sin autorización humana específica. No exigir backups si ya existe renuncia H0 para dos equipos de desarrollo, pero no inferir que esa renuncia por sí sola autoriza futuras subidas.

Mantener explícitamente:
```text
ALPHA13_STATUS=OPEN_G10_R_H0_WDT_ROOT_CAUSE_PENDING
LAST_PHYSICAL_TEST=H0_F_FIRST_READY_STAGE_63
CURRENT_COM4=H0_F_DIAGNOSTIC_BOOT_AFTER_WDT_STABLE_AS_OBSERVED
CURRENT_COM14=H0_ORIGINAL_SLAVE_NO_NEW_FLASH
G10-R-H=REVIEW_STOP_NOT_AUTHORIZED
G10-R-R1_R2_R3_R4=NOT_RUN
A13-006=ADOPTION_BLOCKED
ALPHA15=UNMODIFIED
NEXT_GATE=H0_ROOT_CAUSE_READ_ONLY_REVIEW_THEN_APPROVAL
```

**No asumir:** OpenPLC integrado en este alpha, OTA definida, frecuencia final de flash o `bootloader.bin` definitivo. HMI Designer/TFT nuevas funciones fuera del alcance Alpha13. Priorización: estabilidad, Arduino IDE/CLI, mantener APIs, conservar Display/Ethernet/SD/FRAM/RTC/botonera/RS485/Modbus RTU y TCP/TCA-I/O, documentación y PR en español.

## 7. Criterios de éxito para revisión independiente

1. Explicar por qué `STAGE=63` aparece sistemáticamente en el primer READY y cómo encaja con la única lectura tardía que finalizó.
2. Presentar evidencia concreta de fuentes/símbolos/tareas/CS y separar hallazgos **CONFIRMADOS**, **HIPÓTESIS**, **RIESGOS** y **PENDIENTES**.
3. Demostrar que el parche propuesto corrige el caso físico **sin ocultar `linkStatus()`**, sin aumentar riesgo de concurrencia y sin tocar funciones ajenas.
4. Verificar compatibilidad compilación/consumidores y posteriormente, solo si se autoriza, comprobar el PHY real `LinkON/LinkOFF`, conectividad Ethernet, periodos prolongados, periféricos y recuperación DHCP.
5. Solo después retomar R1–R4 y contemplar cierre Alpha13/transferencia separada a Alpha15.
