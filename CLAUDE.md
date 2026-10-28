# CLAUDE.md — platform-jwplc

Contexto persistente para agentes (Claude Code, GPT/Codex u otros). Este
archivo es **contexto**, no documentación: lo que se decide, mide o valida
se documenta en `docs/` y aquí sólo se enlaza.

Si cambias de agente o de hilo, el siguiente debe poder continuar leyendo, en
este orden (y nada más hasta saber qué pide la tarea):

1. este `CLAUDE.md` (reglas estables; **no** guarda estado);
2. `RAMAS_Y_TAGS.md` del repo privado `JW-Control/jwplc-dev-mcp`
   (`knowledge/projects/platform-jwplc/`): línea activa, ramas y número;
3. el estado vivo de la línea, `docs/v2.1.0-alpha.N/ALPHA<N>_STATUS.md` en su
   rama (máx. 150 líneas; su sección «Para retomar» es el traspaso);
4. el `CLAUDE.md` de la carpeta en la que vas a trabajar (si existe).

Reglas de hilo, validación por niveles e integración entre componentes:
`knowledge/projects/platform-jwplc/chatgpt-project/` (00 y 01) en `jwplc-dev-mcp`.

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

## 2. Trabajo activo y componentes

El estado (línea activa, rama, PR, SHAs, gates) **no** se escribe aquí: se
queda viejo y se paga en cada turno. Está en `RAMAS_Y_TAGS.md` y en el estado
vivo de la línea.

El JWPLC Basic tiene tres componentes que avanzan juntos por línea:

```text
platform-jwplc   package + HMI Designer (tools/jwplc-hmi-designer/) + VPP (openplc-editor-installers/)
openplc-editor   OpenPLC Editor JWPLC Edition; empaqueta el Designer y el VPP desde este repo
                 en SHAs fijados (PLATFORM_DESIGNER_SHA / PLATFORM_VPP_SHA en
                 .github/workflows/jwplc-edition-prerelease.yml)
jwplc-dev-mcp    conocimiento privado (knowledge/) y MCP local de build/flash/gate
```

Una línea que toca el editor usa **la misma rama** en los dos repos. Si cambia
el Designer o el VPP, el SHA fijado en el editor se actualiza en el mismo hito,
apuntando a un commit alcanzable desde `…/main` de la línea (no de una feature
que se borrará), y se anota en la matriz de versiones del estado vivo.

Objetivo permanente del HMI Designer: un único diseñador, exclusivo para la TFT
ST7789 del JWPLC Basic, que oculte la API TFT cruda, reconozca si el proyecto es
**Arduino IDE** (el usuario nombra las variables) u **OpenPLC** (elige variables
existentes) e incluya MENU integrado con la botonera.

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
   compilación en la PC del jefe. Qué librerías están en modo fuente en la línea
   activa se anota en su estado vivo (el `.a` de `JWPLC_TFT` incluye TFT_eSPI).
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
- Carpeta por alpha: `docs/v2.1.0-alpha.N/`, con `N` = número de publicación
  reservado en `RAMAS_Y_TAGS.md` (propuesta 27/10, pendiente de aprobación).
- Archivos fijos por alpha: `ALPHA<N>_PLAN.md` y `ALPHA<N>_STATUS.md`
  (plantilla `knowledge/global/templates/LIVING_STATUS_TEMPLATE.md`).
- Gates: `A<N>_<id>_<TEMA>_<YYYYMMDD>.md` (ej. `A15_1_CODEGEN_COMPILE_20261020.md`).
- Estados como bloques de flags:

  ```text
  A15_1_CODEGEN_COMPILE=PASS|PASS_PHYSICAL|IN_PROGRESS|PENDING|BLOCKED|DEFERRED
  ```

- Distinguir siempre: `[visto]` en código, `[ejecutado]`, `[inferido]`,
  `[no verificado]` (requiere hardware/IDE).
- Al terminar un hito: reescribir `ALPHA<N>_STATUS.md` una vez, incluida su
  sección «Para retomar». No se crean `JWPLC_NEW_CHAT_START_*` nuevos (los
  existentes quedan como histórico).
- Cada documento nuevo entra por la rama de la feature que lo produce (§5), nunca directo en `…/main`. Al
  citar ramas o PR en un `.md`, usar el nombre completo (`v2.1.0-alpha.N/<Línea>/feature/<tema>`, PR #N);
  si una rama se renombra o borra, dejar su último SHA anotado en `docs/<alpha>/`.

---

## 5. Convenciones de git

- **Ramas (regla del propietario, 24/10/2026).** Aparte de `main` y `release/v2.1.x`, cada alpha tiene **una**
  rama principal y sus features cuelgan de ella:

  ```text
  release/v2.1.x                               publicación (fusionar aquí publica la Release)
  └── v2.1.0-alpha.N/<Línea>/main              rama principal de la alpha (PR hacia release al cierre)
      ├── v2.1.0-alpha.N/<Línea>/feature/<a>   ─┐
      └── v2.1.0-alpha.N/<Línea>/feature/<b>   ─┘ PR → …/main → revisión → merge → se borra
  ```

  Formato: `v2.1.0-alpha.N/<Línea>/main` y `v2.1.0-alpha.N/<Línea>/feature/<tema>` (tema en minúsculas con
  guiones). La principal se llama `…/main` porque git no admite a la vez una rama `v2.1.0-alpha.14/HmiDesigner` y
  otra `v2.1.0-alpha.14/HmiDesigner/feature/…`. Nunca sufijos aleatorios (`claude/project-thread-xxxx`): si la
  herramienta asigna uno, trabajar igual en una rama con este formato. Si la línea toca el editor, la
  misma rama existe en `openplc-editor`.
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
- **`main` = `release/v2.1.x` (regla del propietario, 27/10/2026).** Mientras dure 2.1.x las dos ramas quedan en el
  mismo commit: `release/v2.1.x → main` se fusiona siempre con *merge commit* (nunca squash) y, tras cada merge en
  `main` (sincronización o PR de índices), se avanza `release/v2.1.x` por fast-forward a `main`. Detalle en
  `docs/v2.1.0-alpha.15/A15_REPUBLICACION_20261027.md`.
- Ramas remotas permitidas: `main`, `release/v2.1.x`, la `…/main` de cada línea activa y una
  `…/feature/<tema>` por PR abierto (lista vigente en `RAMAS_Y_TAGS.md`). Cualquier otra se revisa
  (¿está contenida en `…/main`?), se anota con su SHA en `docs/<alpha>/` y se borra.
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
