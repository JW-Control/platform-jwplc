# Traspaso: integración y publicación de 2.1.0-alpha.14 (23/10/2026)

Documento para retomar en otro chat (nube o local) el trabajo hecho en la sesión local del 22–23/10.
Leer antes: `CLAUDE.md` de este repo, `jwplc-dev-mcp/CLAUDE.md` y
`jwplc-dev-mcp/knowledge/projects/platform-jwplc/RAMAS_Y_TAGS.md` (en `main`).

## 0. Estado al 24/10/2026, tarde (leer primero; el resto del documento es la historia hasta el 23/10)

```text
FASE_1_INTEGRACION=CLOSED      CI del #141 en verde (9a27b13); hoy PR #157
FASE_3_FREEZE=DONE             Display .a ced1f93 y TFT .a 161e406 reconstruidos el 24/10: ABI_COMPAT=PASS,
                               0 símbolos +/-, ST7789_DISPON_DEFERRED=YES; solo JWPLC_ModbusRTU en modo fuente
CIERRE_NUBE_24_10=PASS         83/83 sketches (76 ejemplos + 7 gates) en Basic y en Basic Core, sin warnings;
                               tests del Designer 16/16; Live Bridge recompilado desde el package congelado (#160)
FASE_4_DOCS=DONE               READMEs de librerías (#159), PRE_RELEASE.md y README raíz (marcador 2.1.0-alpha.14);
                               workflow temporal retirado (PR de cierre)
FASE_2_FISICA=PASS_PHYSICAL    24/10, gates/A14_F2_Combinado (Master COM6 + Slave COM3): pantalla, TCP, RTU, 10 min, LIVE
BENCHMARK_PC=PASS              24/10, Compare-JWPLCBuildBenchmark.ps1: frío +7 a +12 s, caliente sin cambios (PRE_RELEASE)
```

Ramas (24/10): principal `v2.1.0-alpha.14/HmiDesigner/main`; features `v2.1.0-alpha.14/HmiDesigner/feature/<tema>` (CLAUDE.md §5).
PR de release: #157 (borrador, sustituye a #141). Fusionadas en la principal el 24/10: #164 (convención de ramas),
#158 (instalador), #159 (READMEs), #160 (gate físico + Live Bridge), #161 (benchmark), #162 (consolidación) y
#163 (cierre: README/PRE_RELEASE/workflow, el último). Al quitar `alpha14-integracion.yml` el #157 ya no corre
`designer-tests`. Los PRs #141 y #151–#156 quedaron sustituidos y sus ramas borradas.

Siguiente (merge y Release solo con autorización explícita del propietario):
1. (Opcional) Instalar el Setup .exe del artifact de la CI en una PC y abrirlo desde el Arduino IDE 2.
2. Sacar el #157 de borrador (con autorización).
3. Fusionar el #157 (publica la Release) y la fase 5 de §4.

## 1. Dónde está cada cosa

```text
RAMA_INTEGRACION=v2.1.0-alpha.14/feature/hmi-designer   (desde release/v2.1.x; HEAD d7afe353 al escribir esto)
PR_INTEGRACION=#141 (borrador, base release/v2.1.x; NO lleva README de versión: fusionar NO publica todavía)
LINEA_HMI=v2.1.0-alpha.15-HmiDesigner                   (ya contiene los wrappers RS-485/ModbusRTU, PR #138)
LINEA_OPENPLC=v2.1.0-alpha.14-OpenPLC-HMI               (Kenyi; se publicará como 2.1.0-alpha.15)
CIERRE_LINEA_HMI=docs/v2.1.0-alpha.15/A15_CIERRE_Y_TRANSFERENCIA_20261019.md  (§3 integración, §4 pendientes)
WORKFLOW_TEMPORAL=.github/workflows/alpha14-integracion.yml  (se retira en el cierre)
```

`alpha14-integracion.yml` corre en el PR #141: `designer-tests` en cada push; con la etiqueta `regen-tft`
o `regen-display` regenera el `.a` correspondiente y lo deja como artifact (se revisa el log y se sube a mano).
No usa `workflow_dispatch` porque exigiría el archivo en `main`, que debe seguir idéntica a `release/v2.1.x`.

## 2. Qué quedó cerrado en la sesión

- **Alpha13: `CLOSED_PUBLISHED`.** PR #139 (release `22e7531e`) y sincronización por árbol PR #140 (main `efbb626f`).
  Prueba postpublicación: alpha.13 instalada desde Arduino IDE 2.3.4 con el índice dev, `01.ModbusTCP_Server`
  compilado con sketchbook vacío y subido a COM4; FC01–06/15/16 correctos desde la PC.
- **Limpieza de ramas** en platform-jwplc, jwplc-dev-mcp y jwplc-sim. Quedan solo las permanentes y las líneas activas.
- **jwplc-dev-mcp:** CI Evidence v0.5.3 en `main` (#31), modelo temporal de Remote I/O como pendiente (#32),
  presupuesto de tokens y `RAMAS_Y_TAGS.md` (#30). Abierto: #28 (conflicto previo en `OPERATING_KNOWLEDGE.md`).
- **Wrappers RS-485 / Modbus RTU** (PR #138 en la línea HMI): `JWPLC_RS485`/`JWPLC_ModbusRTU` 1.1.0 envuelven
  `JW_RS485`/`JW_ModbusRTU` 0.1.1. N1 equivalencia PASS, N2 10/10, prueba física COM4 Master ↔ COM14 Slave:
  FC03 30/30 y FC06 15/15, CRC 0. Desde fuente: +24 s en la primera compilación, igual en caliente.
- **JW-Libraries:** PR #3 en `main`; repos de distribución `JW_RS485`, `JW_ModbusRTU` (v0.1.1) y `JW_DWIN` (v0.1.0)
  publicados; `DIST_PUSH_TOKEN` ya tiene acceso. La sincronización hacia platform-jwplc apunta a la línea HMI y
  solo a `JW_RS485`/`JW_ModbusRTU`: `JW_MatrixButtons`, `JW_FRAM`, `JW_RTC` y `JW_SD` difieren del package.
- **Benchmark de compilación** (`Run-JWPLCBuildBenchmark.ps1`, 01_empty + 02_io_basic, JWPLC Basic, alpha.13 instalada):

  | arduino-cli | Frío | Caliente |
  |---|---|---|
  | 1.0.2 (Alpha9–12) | 45,5 s | 18,7 s |
  | 1.1.1 (Arduino IDE 2.3.4) | 45,6 s | 18,2 s |
  | 1.4.1 | 43,6 s | 14,3 s |
  | 1.5.1 (Arduino IDE 2.3.10) | 41,5 s | **9,0 s** |

  Decisión: referencia arduino-cli 1.5.1; documentar en alpha.14 "Arduino IDE 2.3.10 o superior". Con 1.0.2,
  Alpha13 frente a Alpha12: frío 12–22 s más rápido, caliente +1,5 a +3 s.

## 3. Integración alpha.14 (fase 1, hecha)

1. `release/v2.1.x` y `main` idénticos; `merge -s ours origin/main`; fusión de la línea HMI (commit `06a636a5`).
2. Conflictos resueltos: `.gitignore` (versión HMI), `JWPLC_ModbusRTU.cpp`/`JWPLC_RS485.cpp` (envoltorio),
   README de ambas (cabecera alpha.14 candidata), `libJWPLC_TFT.a`.
3. `libJWPLC_TFT.a` regenerado en CI desde la fuente fusionada (run 39909857085): `ABI_COMPAT=PASS`,
   0 símbolos públicos eliminados, 36 añadidos, 1 726 706 bytes, sha256 `cb53b88d…f192318`, `precompiled=full`
   (commit `d7afe353`). Motivo: en modo fuente TFT exige TFT_eSPI, que el package no distribuye.
4. Modo fuente durante la alpha: **solo `JWPLC_Display` y `JWPLC_ModbusRTU`**.
5. CI en el SHA `d7afe353`: tests del HMI Designer 10/10 PASS (incluye `codegen-smoke` y `menu-host-sim`);
   `ci-jwplc-package-smoke.yml` PASS.
6. Compilación local (arduino-cli 1.5.1, package de la rama vía `hardware/jwplc_a14`, sketchbook vacío):
   16 PASS en JWPLC Basic (Display 01–04, gates A15_8_Menu, A15_8_Menu_Designer, A15_I1/I2/I3, Ethernet,
   Modbus TCP Server/Client, Modbus RTU Slave/Master, RS485_Echo).
7. Compilación en la nube `[ejecutado 2026-10-23]` (arduino-cli de `tools/alpha15/regenerate_libJWPLC_TFT.sh`,
   `jwplc_local`, HEAD `8e5924f`, con `libJWPLC_TFT.a` de `161e406` con DISPON diferido). Cierra lo pendiente del punto 6:

   ```text
   A14_FASE1_COMPILE_BASIC=PASS        17/17 (la lista de arriba repetida con el .a nuevo + Codegen_Static + F0)
   A14_FASE1_COMPILE_BASIC_CORE=PASS   17/17 (mismos sketches; fqbn jwplc_local:esp32:jwplcbasiccore)
   A14_FASE1=CLOSED
   ```

   - `A15_1_3_Codegen_Static`: el sketch es `gates/A15_1_3_Codegen_Static/A15_1_3_Codegen_Static/`.
     Basic 461 909 B flash / 36 092 B RAM; Core 419 331 B.
   - `A15_F0_OpenPLC_Bool_Rele` no es un `.ino`: se arma con `gates/A15_F0_OpenPLC_Bool_Rele/check_f0_compile.sh [dir]`
     (HAL del VPP + bloque F0 + stub de `openplc.h`). Basic 435 273 B / 37 060 B; Core 392 811 B.
   - Flash en Basic: 436 713–456 177 B (Display 01–04 y gates A15_*, Ethernet, Modbus TCP/RTU, RS485_Echo).
     Core (sin Ethernet/SD/FRAM): 392 811–413 687 B.

## 4. Lo que falta (en este orden)

**Fase 2: validación física (local, operador presente; Display aún desde fuente)**
1. Sketch combinado en COM4 (pantalla + Ethernet) con COM14 como Slave RTU: DHCP + Modbus TCP, Modbus RTU,
   MENU, pestañas, reloj, `°`, buzzer y tecla de despertar.
2. Live Bridge: `tools/alpha15/build_live_bridge_firmware.sh --install` sobre el package integrado; probar
   «⚡ Instalar Firmware Live» y LIVE conectado.
3. Regenerar e instalar la app de escritorio (`electron/main.js` cambió).

**Fase 3: congelar**
4. Etiqueta `regen-display` en el PR #141 → revisar `ABI_COMPAT=PASS` → subir `libJWPLC_Display.a` y volver a
   `precompiled=full` → CI (`codegen-smoke` verifica símbolos en el `.a`).
5. Compilación y subida corta con todo precompilado; benchmark con arduino-cli 1.5.1 en la PC del jefe.

**Fase 4: documentación y publicación**
6. Versiones: `JWPLC_Display` 1.1.0, `JWPLC_TFT` 0.3.0, package `2.1.0-alpha.14`.
7. README raíz y de librerías, `PRE_RELEASE.md` (con la recomendación de Arduino IDE ≥ 2.3.10 y la tabla de arriba),
   `ALPHA15_STATUS.md` recortado a ≤ 150 líneas, `RAMAS_Y_TAGS.md` en jwplc-dev-mcp.
   **Hecho el 23/10 (borrador, sin el marcador `JWPLC_RELEASE_VERSION`):** `docs/v2.1.0-alpha.14/PRE_RELEASE.md`
   (con `[pendiente]` para fase 2, benchmark y CI final), README raíz, README de `JWPLC_Display` (sección MENU,
   compilada), `JWPLC_TFT`, `JWPLC_GlobalPeripherals` 1.1.0 (Buzzer), `ALPHA15_STATUS.md` en 146 líneas con el
   registro en `history/`. Falta: README del HMI Designer (lo revisa el operador) y `RAMAS_Y_TAGS.md`.
8. Retirar `alpha14-integracion.yml`; añadir el README de versión; sacar el #141 de borrador.
   **Fusionar solo con autorización explícita del propietario: la fusión dispara la Release.**

**Fase 5: postpublicación**
9. ZIP, SHA-256, PR automático del índice dev a `main`; instalación con Arduino IDE 2.3.10, compilación y subida.
10. Sincronizar `main` por árbol (§5 de `RAMAS_Y_TAGS.md`), cerrar el estado vivo, borrar la línea HMI y la rama
    de integración, resolver el PR #28 de jwplc-dev-mcp.

## 5. Límites y reglas para quien continúe

- **Chat en la nube:** puede leer y editar el repo, abrir PR, poner etiquetas y leer la CI. **No** puede usar las
  herramientas `jwplc_*`, ni puertos COM, ni la PC del operador: las fases 2, 3.5 y 5.9 se hacen en local.
- Commits como `JW-Control <jw.control.peru@gmail.com>`; sin `Co-Authored-By`, `Claude-Session`, enlaces de sesión
  ni pie "Generated with Claude Code" (`CLAUDE.md` §5).
- Merges, Releases, `flash`, borrado de ramas y cambios de guards: solo con autorización explícita del propietario.
- Un `.a` en conflicto o con fuente cambiada no se elige: se regenera y se exige `ABI_COMPAT=PASS`.

## 6. Entorno local del operador (para sesiones locales)

- Worktree de integración: `C:\Users\jeykc\Documentos\GitHub\platform-jwplc-a14`.
- `jwplc_local` (junction en Arduino15) apunta al checkout principal `platform-jwplc`, no al worktree.
- arduino-cli 1.5.1 en `C:\Program Files\Arduino CLI`; `jwplc-dev-mcp/config/projects.toml` (ignorado) ya apunta a él.
- Sketchbook: se retiraron las copias `JWPLC_ModbusRTU`/`JWPLC_RS485` que tapaban al package; quedan
  `JW_RS485`/`JW_ModbusRTU` (Laundry HMI 1v5v0).
- Placas: COM4 (Master, con Ethernet) y COM14 (Slave), unidas por RS-485.
