# Alpha15 (HMI Designer) — cierre de desarrollo y transferencia a la integración

Fecha: `2026-10-19` · Rama de trabajo: `v2.1.0-alpha.15-HmiDesigner` · Responsable del alpha: André

Documento canónico de cierre. Lo usa el agente que integra esta rama con Alpha13 y prepara la
publicación. El estado detallado de cada gate está en [ALPHA15_STATUS.md](ALPHA15_STATUS.md).

## 1. Resultado

```text
ALPHA15_DEV=CLOSED (desarrollo y pruebas físicas cerradas por el jefe el 2026-10-19)
ALPHA15_PHYSICAL=PASS (MENU, RTC, °, BOOL piloto, formatos, buzzer, despertar, pestañas, color de pestaña)
ALPHA15_RELEASE=PENDIENTE_INTEGRACION (se publica como 2.1.0-alpha.14, ver §2)
PENDIENTE_DE_PROMOCION=libJWPLC_Display.a (JWPLC_Display sigue en modo fuente; ver §4)
OVERLAYS_O_SKETCHES_CON_PRODUCTO=NINGUNO (todo lo validado está en el package de esta rama)
```

[ejecutado] `node tools/jwplc-hmi-designer/tests/run-all.cjs`: 9/9 PASS. Los gates de `tools/jwplc-hmi-designer/gates/`
compilan para ESP32 con `jwplc_local:esp32:jwplcbasic`. La placa publicada `jwplc:esp32:jwplcbasic`
(2.1.0-alpha.12) **no** compila los headers de Alpha15, porque su `JWPLC_Display` no tiene MENU.

## 2. Numeración (decisión del jefe, 2026-10-19)

Por el orden en que se publican, **esta Alpha15 termina siendo la 2.1.0-alpha.14 final del repositorio**, y la
Alpha14 de Kenyi (OpenPLC) termina siendo la **2.1.0-alpha.15**. La Alpha13 (robustez, el jefe) se cierra primero
y conserva su número. Las ramas y carpetas de trabajo conservan su etiqueta.

| Trabajo | Responsable | Etiqueta de trabajo (rama / docs) | Publicación |
|---|---|---|---|
| Robustez del package | jefe (GPT) | `v2.1.0-alpha.13/...` · `docs/v2.1.0-alpha.13/` | **2.1.0-alpha.13** |
| HMI Designer | André | `v2.1.0-alpha.15-HmiDesigner` · `docs/v2.1.0-alpha.15/` | **2.1.0-alpha.14** |
| OpenPLC + integración con el HMI Designer | Kenyi | sus ramas actuales | **2.1.0-alpha.15** |

- **No renombrar ramas ni carpetas.** `docs/v2.1.0-alpha.14/` ya contiene 251 documentos históricos de la
  época Alpha12 (Modbus TCP, rendimiento, HMI dirty refresh) etiquetados «Alpha14». Además, `docs/v2.1.0-alpha.15/`
  y la rama de esta alpha están citados en código, tests, skills y commits. El README raíz ya fija el criterio:
  «la etiqueta de los documentos no cambia la numeración final del release».
- La rama de integración que se cree para publicar sí usa el número de publicación (§3).
- El modo OpenPLC del Designer (A15-6) pasa al trabajo de Kenyi (publicación 2.1.0-alpha.15).

## 3. Integración con Alpha13 (verificada en modo lectura el 2026-10-19)

**Esta rama no contiene Alpha13.** Trae Alpha12 publicado (`origin/main`, `aa0bfed1`) y la VPP 2.1.0-alpha.29
de Kenyi. La base de Alpha13 usada en la prueba fue `v2.1.0-alpha.13/feature/cleanup-robustness` @ `a9c96273`.

| Medición | Resultado |
|---|---|
| Ancestro común que calcula git | `e407a776` (Alpha8): Alpha9–12 llegaron a `main` por commits de sincronización y a Alpha13 por `release/v2.1.x` |
| `git diff release/v2.1.x main` | **0 archivos**: los dos árboles de Alpha12 publicado son idénticos |
| `git merge` directo | **68 conflictos** (44 add/add, 24 de contenido), casi todos falsos |
| Con Alpha12 publicado como ancestro | **2 conflictos**: `.gitignore` (texto) y `JWPLC_TFT/src/esp32/libJWPLC_TFT.a` (binario) |
| `JWPLC_TFT` fusionado (fuente) | Sin conflicto de texto: queda `_batchOwner` atómico y `JWPLC_TFT_DEFER_DISPON` (Alpha13), más las primitivas y fuentes 0.3.0 (Alpha15) |
| `libJWPLC_TFT.a` regenerado desde la fuente fusionada | [ejecutado en la nube, sin publicar] `ABI_COMPAT=PASS`, 1.726.286 bytes, `sha256 270b12bc…a1c3d9` |

Archivos de producto que tocan las dos ramas: sólo `JWPLC_TFT` (fuente y `.a`) y `.gitignore`. Alpha13 no toca
`JWPLC_Display`; Alpha15 no toca Ethernet, SD, FRAM, ModbusRTU ni el core.

**Procedimiento verificado** (en la rama de integración, partiendo de la base de Alpha13 ya cerrada):

```bash
git switch -c v2.1.0-alpha.14/feature/hmi-designer <base-alpha13-cerrada>
git diff --quiet origin/release/v2.1.x origin/main && echo IDENTICOS   # condición obligatoria
git merge -s ours origin/main -m "merge: registrar Alpha12 publicado (main) como ancestro; árbol sin cambios"
git merge origin/v2.1.0-alpha.15-HmiDesigner                            # quedan 2 conflictos
# .gitignore: conservar las líneas de ambos lados
# libJWPLC_TFT.a: NO elegir ninguna versión; regenerar desde la fuente fusionada:
tools/alpha15/regenerate_libJWPLC_TFT.sh build --install               # exige ABI_COMPAT=PASS
```

`-s ours` contra `main` no cambia ningún archivo, porque los árboles son idénticos. Sólo registra en la historia
que Alpha12 publicado es el ancestro común. Si la condición `IDENTICOS` falla, detenerse y revisar.
Si Alpha13 cierra con más commits (por ejemplo F1/H0), repetir la medición sobre su HEAD final.

## 4. Pendiente para publicar 2.1.0-alpha.14 (en este orden)

1. Integración (§3) y CI N0–N3 sobre el SHA exacto. Incluye:
   - `node tools/jwplc-hmi-designer/tests/run-all.cjs` (requiere g++ y Playwright);
   - los gates de `tools/jwplc-hmi-designer/gates/`;
   - los ejemplos de Alpha13.

   Si un workflow de Alpha13 con lista blanca de alcance rechaza los archivos del HMI Designer, se ajusta su
   alcance para la 2.1.0-alpha.14. No se omiten pruebas.
2. Freeze de `JWPLC_Display`:
   - `tools/alpha15/regenerate_libJWPLC_Display.sh build --install`;
   - volver a poner `precompiled=full` en `JWPLC_Display/library.properties`;
   - repetir `codegen-smoke.cjs`, que verifica los símbolos en el `.a`;
   - benchmark de compilación en la PC del jefe.
3. Versiones:
   - `JWPLC_Display` (hoy `1.1.0`, con MENU, formatos, reloj, cp437, piloto, pestañas y el hook `jwplcUIPageModeChanged`);
   - `JWPLC_TFT` `0.3.0` (+ cambios de Alpha13);
   - versión del package `2.1.0-alpha.14`.
4. Documentación de publicación:
   - notas del release;
   - roadmap del README raíz con la numeración de §2;
   - cierre en `docs/v2.1.0-alpha.15/ALPHA15_STATUS.md` con el número publicado.
5. Prueba física corta del package integrado, autorizada por el operador: Ethernet + RS-485 + HMI con MENU y
   pestañas en la misma placa. Ese mismo día:
   - regenerar el Live Bridge con `tools/alpha15/build_live_bridge_firmware.sh --install` sobre el package
     integrado;
   - probar «⚡ Instalar Firmware Live» y LIVE conectado;
   - regenerar la app de escritorio, porque `electron/main.js` cambió (protocolo `jwplc-hmi://`).
6. Publicar en el índice dev. Después, Kenyi integra su trabajo sobre 2.1.0-alpha.14 publicada.

## 5. Diferido (no bloquea la publicación)

```text
A15_6_JWHMI_V2_MODES=TRANSFERIDO al trabajo OpenPLC de Kenyi (publicación 2.1.0-alpha.15)
A15_7E_RAW_SHAPES_NAV_PIXEL=DEFERRED (mini-modelos de figuras, texto libre y PixelMap)
A15_5_PURE_CODEGEN_TESTS (resto)=DEFERRED (estática, NAV y PixelMap aún por capas)
A15_4_DESIGNER_CI=DEFERRED (correr run-all.cjs en CI)
GLIFOS_A_I_O_U=DEFERRED (requiere regenerar libJWPLC_TFT.a con glifos propios)
A15_FRAM_MAP=DEFERRED al plan industrial I9/I10
```

## 6. Lecciones de esta alpha (también registradas en knowledge)

- **Ramas con historias paralelas:** cuando una publicación llegó a `main` y a `release/*` por caminos
  distintos, git calcula un ancestro muy antiguo y produce decenas de conflictos falsos. Hay que verificar que
  los árboles publicados son idénticos y registrar ese ancestro con `merge -s ours` antes de fusionar.
- **Un `.a` precompilado en conflicto no se elige:** se regenera desde la fuente fusionada y se exige el chequeo de ABI.
- **Botonera al despertar:** la tecla que despierta la IDLE debe descartarse cuando el sistema (selector de
  páginas o MENU) es dueño de las teclas. Si no, un OK entra además al contenido. Sin páginas ni MENU, el sketch
  la sigue recibiendo (Alpha8).
- **Un alpha en modo fuente sólo compila con la placa local:** con la placa publicada, el `#error` de capacidad
  (MENU) aparece aunque el sketch esté bien. Hay que decirlo en la guía de prueba.
