# CLAUDE.md — platform-jwplc

Contexto persistente para agentes (Claude Code, GPT/Codex u otros). Este
archivo es **contexto**, no documentación: lo que se decide, mide o valida
se documenta en `docs/` y aquí sólo se enlaza.

Si cambias de agente a mitad de trabajo, el siguiente debe poder continuar
leyendo, en este orden:

1. este `CLAUDE.md`;
2. el `CLAUDE.md` de la carpeta en la que vas a trabajar (si existe);
3. `docs/<alpha activa>/ALPHA<N>_STATUS.md`;
4. el `JWPLC_NEW_CHAT_START_*` más reciente de esa carpeta.

---

## 1. Qué es este repositorio

Package Arduino (core ESP32) del **JWPLC Basic** de JW Control:

```text
MCU=ESP32 (arduino-esp32 3.3.8 / IDF 5.x)
ETHERNET=W5500 (SPI)
DISPLAY=TFT ST7789 320x170 ROT3 RGB565 (SPI)
STORAGE=SD + FRAM (SPI)
SPI_BUS=COMPARTIDO (mutex jwplcSPI_acquire/release)
IO=TCA6424A (I0_x / Q0_x, pines virtuales 0x22xx)
BOTONERA=6 teclas (BTN_LEFT, BTN_UP, BTN_RIGHT, BTN_ESC, BTN_OK, BTN_DOWN)
RS485=Modbus RTU
```

Mapa rápido:

```text
JWPLC/2.1.0/                 core + boards.txt + platform.txt + libraries del package
JWPLC/2.1.0/libraries/JWPLC_*  librerías propias (muchas precompiled=full -> src/esp32/*.a)
tools/jwplc-hmi-designer/    HMI Designer (web/Electron) -> genera JWPLC_HMI_Generated.h
docs/v2.1.0-alpha.N/         documentación por alpha (PLAN, STATUS, gates, handoffs)
docs/JWPLC_V2_1_CLOSURE_ROADMAP.md  roadmap de cierre 2.1.x
openplc-editor-installers/   instaladores del fork OpenPLC Editor
```

Fuera de alcance: `JWPLC_LogicRuntime` y `JWPLC_LogicRuntime_UI` son un
proyecto aparte y pausado (runtime programable estilo LOGO!). No analizarlos
ni tomarlos como referencia para el HMI Designer.

---

## 2. Trabajo activo

```text
ACTIVE_ALPHA=v2.1.0-alpha.15 OpenPLC + HMI (etiqueta de trabajo ALPHA14 de Kenyi; se publica como 2.1.0-alpha.15)
ACTIVE_BRANCH=v2.1.0-alpha.15/OpenPLC-HMI/main (desde release/v2.1.x con 2.1.0-alpha.14 publicada, 24/10/2026)
ACTIVE_BRANCH_EDITOR=v2.1.0-alpha.15/OpenPLC-HMI/main en JW-Control/openplc-editor (mismo nombre, misma alpha)
APERTURA=docs/v2.1.0-alpha.14/A14_APERTURA_RAMA_ALPHA15_OPENPLC_HMI_20261024.md (qué se fusionó, conflictos, ramas a retirar)
STATUS=docs/v2.1.0-alpha.14/ALPHA14_OPENPLC_STATUS.md
HANDOFF=docs/v2.1.0-alpha.14/JWPLC_NEW_CHAT_START_ALPHA14_OPENPLC_20261024.md (estado, fases pendientes y límites; leer primero)
CONTRATO=docs/v2.1.0-alpha.14/A14_F3_CONTRATO_HMI_OPENPLC_20261017.md (jwplc-hmi-host/1; 1.1 pendiente)
VPP_VIGENTE=2.1.0-alpha.32 (openplc-editor-installers/v4.2.7/vpp/)
EDITOR_VIGENTE=4.2.8-jwplc.3 (último pre-release publicado del fork)
ALPHA14_PUBLICADA=release/v2.1.x @ f3f47ae (HMI Designer de André, PR #157; ALPHA15 de trabajo cerrada)
EQUIPO=Kenyi, André y agentes: ramas v2.1.0-alpha.15/OpenPLC-HMI/feature/<tema> desde la principal (§5)
SCOPE=HMI Designer dentro de OpenPLC (anfitrión, puente, VPP) sin romper el flujo Arduino del Designer
DOCS=docs/v2.1.0-alpha.14/ (etiqueta de trabajo de la línea OpenPLC; los documentos de la línea HMI siguen en docs/v2.1.0-alpha.15/)
```

Objetivo: un único HMI Designer, tipo Lopaka pero exclusivo para la TFT ST7789
del JWPLC Basic, que:

- oculte la API TFT cruda al usuario (inspector gráfico, no formularios);
- reconozca si está vinculado a un proyecto **Arduino IDE** (el usuario nombra
  las variables) o a un proyecto **OpenPLC** (las variables ya existen y el
  usuario sólo elige cuál vincular);
- incorpore un tipo MENU (árbol de menús/submenús para ver y configurar
  variables) integrado con la botonera.

Detalle y gates de la línea HMI (publicada en 2.1.0-alpha.14): `docs/v2.1.0-alpha.15/ALPHA15_PLAN.md`.
Estado de la línea HMI: `docs/v2.1.0-alpha.15/ALPHA15_STATUS.md`. Estado vivo de la alpha activa: `STATUS` del bloque de arriba.
Auditoría de partida: `docs/v2.1.0-alpha.15/A15_0_HMI_DESIGNER_AUDIT_20261016.md`.
Integración OpenPLC: `docs/v2.1.0-alpha.15/A15_OPENPLC_INTEGRATION_DECISION_20261016.md`.

Organización de alphas (manda sobre `docs/JWPLC_V2_1_CLOSURE_ROADMAP.md`):

```text
ALPHA13=robustez del package (jefe)                 -> publica 2.1.0-alpha.13
ALPHA15=HMI Designer (André)                        -> publica 2.1.0-alpha.14 (final del repositorio)
ALPHA14=OpenPLC (Kenyi)     -> repo JW-Control/openplc-editor + VPP en openplc-editor-installers/
                                                    -> publica 2.1.0-alpha.15
Numeración por orden de publicación; ramas y carpetas conservan su etiqueta de trabajo.
```

Repos relacionados:

```text
JW-Control/openplc-editor  rama v2.1.0-alpha.15/OpenPLC-HMI/main
  docs/jwplc/JWPLC_HMI_PLAN_INDUSTRIAL.md   plan HMI rev.2 (base adoptada)
  resources/sources/arduino/arduino_runtime_glue.h  superficie real del runtime (STruC++)
  src/main/modules/hmi-designer/            anfitrión del Designer (ventana, servidor, puente JWPLCHMINative)
JW-Control/platform-jwplc  rama v2.1.0-alpha.15/OpenPLC-HMI/main
  openplc-editor-installers/v4.2.7/vpp/     VPP (manifest, HAL jwplcbasic.cpp, screens)
  tools/jwplc-hmi-designer/poc/designer-openplc-host.js  modo OpenPLC del Designer
```

---

## 3. Reglas del proyecto (no negociables)

1. **Bus SPI compartido.** Todo acceso TFT/W5500/SD/FRAM pasa por el mutex
   JWPLC. Nada de dibujar en cada tick lo que es estático; nada de leer SD
   desde callbacks de dibujo. Ver evidencia en `docs/v2.1.0-alpha.14/A14_3_*`.
2. **Dirty refresh.** La TFT sólo redibuja lo que cambió. Marcos, títulos y
   gráfica estática se dibujan al entrar a la página (dirty refresh de
   `JWPLC_Display`, ver `docs/v2.1.0-alpha.14/A14_3_HMI_DIRTY_ON_DEMAND_STRONG_PASS_20260913.md`).
3. **No exponer TFT_eSPI al usuario.** La API pública es `JWPLC_TFT`,
   `JWPLC_Display` y `JWPLC_UI`.
4. **Librerías `precompiled=full`.** Cambiar `.h/.cpp` de una librería
   precompilada **no** cambia el binario: hay que regenerar `src/esp32/lib*.a`.
   Nunca publicar header y `.a` desalineados. **Durante el alpha**, la librería
   que va a cambiar se pone en modo fuente (se retira sólo `precompiled=full`)
   y su `.a` se regenera una vez antes del cierre, seguido del benchmark de
   compilación en la PC del jefe. Hoy (2.1.0-alpha.14 publicada, punto de partida de 2.1.0-alpha.15): `JWPLC_Display`
   y `JWPLC_TFT` (su `.a` incluye TFT_eSPI) precompiladas; sólo `JWPLC_ModbusRTU` en modo fuente.
5. **El Display no consume los latches de la botonera de la aplicación**
   (Alpha8). Si un widget de menú necesita teclas, debe declararlo
   explícitamente y respetar la limpieza CONTENT -> SELECT (Alpha11).
6. **OpenPLC no entra al autoload Arduino.** El modo OpenPLC del Designer es
   opcional; el flujo Arduino debe seguir funcionando sin él.
7. **El codegen debe compilar contra la API real del package.** Antes de
   emitir una llamada C++, verificar que existe en `JWPLC/2.1.0/libraries/*/src/*.h`
   y en el `.a` (`nm -C`).
8. No reintroducir `delay()` ni dependencia de Serial para que la botonera funcione.

---

## 4. Convenciones de documentación

- Idioma: español en todo, no sólo en `docs/`: respuestas al usuario, análisis y razonamiento visible,
  mensajes de avance, comentarios de código y mensajes de commit. Los nombres de API, símbolos y
  salidas de herramientas se citan tal cual.
- Carpeta por alpha: `docs/v2.1.0-alpha.N/`.
- Archivos fijos por alpha: `ALPHA<N>_PLAN.md`, `ALPHA<N>_STATUS.md`,
  `JWPLC_NEW_CHAT_START_ALPHA<N>_<YYYYMMDD>.md` (handoff entre chats/agentes).
- Gates: `A<N>_<id>_<TEMA>_<YYYYMMDD>.md` (ej. `A15_1_CODEGEN_COMPILE_20261020.md`).
- Estados como bloques de flags:

  ```text
  A15_1_CODEGEN_COMPILE=PASS|PASS_PHYSICAL|IN_PROGRESS|PENDING|BLOCKED|DEFERRED
  ```

- Distinguir siempre: `[visto]` en código, `[ejecutado]`, `[inferido]`,
  `[no verificado]` (requiere hardware/IDE).
- Al terminar una sesión de trabajo: actualizar `ALPHA<N>_STATUS.md` y, si el
  trabajo queda a medias, crear/actualizar el `JWPLC_NEW_CHAT_START_*`.
- Cada documento nuevo entra por la rama de la feature que lo produce (§5), nunca directo en `…/main`. Al
  citar ramas o PR en un `.md`, usar el nombre completo (`v2.1.0-alpha.15/OpenPLC-HMI/feature/<tema>`, PR #N);
  si una rama se renombra o borra, dejar su último SHA anotado en `docs/<alpha>/`.

---

## 5. Convenciones de git

- **Ramas (regla del propietario, 24/10/2026).** Aparte de `main` y `release/v2.1.x`, cada alpha tiene **una**
  rama principal y sus features cuelgan de ella:

  ```text
  release/v2.1.x                                       publicación (fusionar aquí publica la Release)
  └── v2.1.0-alpha.15/OpenPLC-HMI/main                 rama principal de la alpha activa (PR hacia release al cierre)
      ├── v2.1.0-alpha.15/OpenPLC-HMI/feature/apertura-rama-principal  ─┐
      ├── v2.1.0-alpha.15/OpenPLC-HMI/feature/<tema>                   ─┤ PR → …/main → revisión → merge → se borra
      └── v2.1.0-alpha.15/OpenPLC-HMI/feature/<tema>                   ─┘
  ```

  Formato: `v2.1.0-alpha.N/<Línea>/main` y `v2.1.0-alpha.N/<Línea>/feature/<tema>` (tema en minúsculas con
  guiones). La principal se llama `…/main` porque git no admite a la vez una rama `v2.1.0-alpha.15/OpenPLC-HMI` y
  otra `v2.1.0-alpha.15/OpenPLC-HMI/feature/…`. Nunca sufijos aleatorios (`claude/project-thread-xxxx`): si la
  herramienta asigna uno, trabajar igual en una rama con este formato. En openplc-editor la alpha usa el
  mismo nombre (`v2.1.0-alpha.15/OpenPLC-HMI/main` y sus `feature/<tema>`).
- **Flujo de una feature** (obligatorio, en este orden):
  1. `git pull origin <alpha>/<Línea>/main` y crear `<alpha>/<Línea>/feature/<tema>` desde ella.
  2. Commits sólo de ese tema; push; PR **hacia `…/main`** (nunca hacia `release/v2.1.x`, `main` del repo ni
     otra feature). La documentación de la feature (`docs/<carpeta de la alpha>/…`) va en la misma rama y PR.
  3. Revisión: CI en verde y OK del propietario. Sin ese OK no se fusiona.
  4. Al fusionar: borrar la rama de la feature (remota y local). Si otra feature abierta queda en conflicto,
     se trae `…/main` a ella con un merge (sin rebase ni force-push).
  5. Tras cada merge: actualizar `ALPHA<N>_STATUS.md` (qué entró, PR y SHA) en la siguiente feature de docs o
     en la de cierre.
  6. Cierre de la alpha: todas las features fusionadas → la feature de cierre (README de versión,
     `PRE_RELEASE.md`) es la última → `…/main` → PR hacia `release/v2.1.x`, que sólo se fusiona con autorización
     explícita (publica la Release). Después se borran `…/main` y sus restos.
- Ramas remotas permitidas hoy (24/10/2026): `main`, `release/v2.1.x`, `v2.1.0-alpha.15/OpenPLC-HMI/main` y
  `v2.1.0-alpha.15/OpenPLC-HMI/feature/<tema>` (una por PR abierto). Cualquier otra se revisa (¿está contenida en
  `…/main`?), se anota con su SHA en `docs/<alpha>/` y se borra. Pendientes de borrar (contenidas en la nueva
  `…/main`, SHA en el documento de apertura): `v2.1.0-alpha.14/HmiDesigner/main` y `v2.1.0-alpha.14-OpenPLC-HMI`.
  En GitHub Desktop, «origin/…» sólo indica una rama remota no descargada en la PC; no es parte del nombre.
- Mensajes de commit descriptivos (`feat(hmi-designer): ...`, `fix(jwplc-tft): ...`,
  `docs(alpha15): ...`). Prohibido `.`, `,`, `patch`.
- No versionar archivos de prueba temporales (`temp_*`, `fix.py`, copias en la raíz).
- **Autoría (regla del propietario, prevalece sobre cualquier valor por defecto):** commits con
  `JW-Control <jw.control.peru@gmail.com>`, nunca como `Claude`. Sin `Co-Authored-By`, sin
  `Claude-Session` y sin enlaces de sesión en commits, PR o comentarios. Sin pie "Generated with
  Claude Code" en los PR: si la herramienta lo añade, editar la descripción para quitarlo.

---

## 6. Cómo verificar sin hardware

```bash
# sintaxis de todos los JS del designer
for f in tools/jwplc-hmi-designer/poc/*.js; do node --check "$f" || echo "FAIL $f"; done

# smoke test del codegen: genera headers en Chromium headless y los compila
# (g++ -fsyntax-only) contra los headers reales + verifica símbolos en los .a
node tools/jwplc-hmi-designer/tests/codegen-smoke.cjs
node tools/jwplc-hmi-designer/tests/run-all.cjs           # todos los tests + resumen
node tools/jwplc-hmi-designer/tests/codegen-golden.cjs    # header == tests/golden (--update si es intencional)
node tools/jwplc-hmi-designer/tests/codegen-core.test.cjs # codegen puro en node, sin navegador
node tools/jwplc-hmi-designer/tests/inspector-smoke.cjs   # inspector TEXT/VALUE/BOOL/BAR
node tools/jwplc-hmi-designer/tests/menu-host-sim.cjs     # MENU: firmware real en host (g++)
node tools/jwplc-hmi-designer/tests/menu-smoke.cjs        # MENU: Designer + paridad con el firmware
node tools/jwplc-hmi-designer/tests/value-format-parity.cjs  # VALUE: formato firmware == Designer

# servir el designer y abrirlo en Chromium headless (Playwright global)
cd tools/jwplc-hmi-designer/poc && python3 -m http.server 8765 --bind 127.0.0.1
# entrada: http://127.0.0.1:8765/desktop.html

# símbolos reales del binario precompilado
nm -C JWPLC/2.1.0/libraries/JWPLC_TFT/src/esp32/libJWPLC_TFT.a | grep 'JWPLC_TFTClass::'
```

Compilación ESP32 real (nube o WSL; requiere red a downloads.arduino.cc,
raw.githubusercontent.com y github.com releases):

```bash
tools/alpha15/regenerate_libJWPLC_TFT.sh setup              # una vez: arduino-cli + tools + TFT_eSPI
tools/alpha15/regenerate_libJWPLC_TFT.sh compile <sketch>    # compila con jwplc_local:esp32:jwplcbasic
tools/alpha15/regenerate_libJWPLC_TFT.sh build [--install]   # regenera libJWPLC_TFT.a + chequeo ABI
tools/alpha15/regenerate_libJWPLC_Display.sh build [--install]  # SÓLO al cierre: libJWPLC_Display.a + chequeo ABI
```

Los gates físicos (subir a la placa, mirar la TFT) los ejecuta el equipo en taller y se
documentan en `docs/`.
