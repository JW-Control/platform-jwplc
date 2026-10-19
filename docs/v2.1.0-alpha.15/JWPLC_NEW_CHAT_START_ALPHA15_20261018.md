# Inicio de chat — JWPLC Alpha15 — HMI Designer (2026-10-18)

Reemplaza a `JWPLC_NEW_CHAT_START_ALPHA15_20261017.md`. Sirve para Claude, GPT/Codex o cualquier agente que retome el trabajo.

Leer en orden:

```text
CLAUDE.md
tools/jwplc-hmi-designer/CLAUDE.md
.claude/skills/hmi-designer-ux/SKILL.md          (antes de tocar UI del Designer)
docs/v2.1.0-alpha.15/ALPHA15_STATUS.md
docs/v2.1.0-alpha.15/A15_INDUSTRIAL_ARQUITECTURA_PLAN_20261018.md   (§G: decisiones aprobadas)
docs/v2.1.0-alpha.15/A15_I1_FORMATOS_VALUE_BOOL_20261018.md
docs/v2.1.0-alpha.15/A15_I2_FECHA_HORA_RTC_20261018.md
docs/v2.1.0-alpha.15/A15_I3_TILDES_CP437_20261018.md
docs/v2.1.0-alpha.15/A15_7_APARIENCIA_A1_ANALISIS_20261018.md        (§6 implementación)
docs/v2.1.0-alpha.15/A15_F0_OPENPLC_RUTA_KENYI_20261018.md
```

## Estado

```text
BRANCH=v2.1.0-alpha.15-HmiDesigner   (git pull antes de empezar)
ALPHA15_SCOPE (§G.1)=I1 + I2 + I3 + Apariencia A1 + JWPLC_Buzzer -> HECHO en host y compilación ESP32
FIRMWARE=JWPLC_Display 1.1.0 en MODO FUENTE (sin precompiled=full; .a histórico sin enlazar)
         JWPLC_TFT 0.3.0 precompilada (sin cambios en este alpha desde 0.3.0)
         JWPLC_Buzzer en JWPLC_GlobalPeripherals (autoload, sin include)
TESTS=node tools/jwplc-hmi-designer/tests/run-all.cjs -> 9/9 PASS (con g++ y playwright)
DESKTOP_APP=confirmada por el jefe (abre el Arduino IDE). Tras el 2026-10-19 regenerarla: IPC write-guide
GATES_FISICOS_LISTOS=A15_1_3_Codegen_Static, A15_8_Menu, A15_8_Menu_Designer, A15_I1_Formatos,
                     A15_I2_Reloj, A15_I3_Tildes, A15_F0_OpenPLC_Bool_Rele, 07.Buzzer_Basic
```

## Para cerrar Alpha15 (en este orden)

1. **Taller (A15-9).** Correr los gates físicos y anotar el resultado en `docs/v2.1.0-alpha.15/` con flags `*_PHYSICAL`. Lo crítico:
   - `A15_I3_Tildes`: el `°` debe verse como `°`; si sale `≈`, `JWPLC_TFT` no está en `cp437(false)`;
   - `A15_I2_Reloj`: la medianoche y el RTC sin pila;
   - `A15_I1_Formatos`: el piloto y que no queden restos de dígitos;
   - `07.Buzzer_Basic`: volumen y que no bloquee.
   - App de escritorio (Electron, en la PC), instalada con `tools/jwplc-hmi-designer/Generar-e-Instalar-App-Escritorio.cmd`:
     - «Escribir en sketch» deja `JWPLC_HMI_Generated.h`, `<sketch>.jwhmi` y `JWPLC_HMI_LEEME.md`;
     - el sketch base de la guía, copiado a un `.ino` vacío, compila y muestra las páginas;
     - Apariencia: el editor de color fijo (sin selector nativo) en campos, MENU y BAR;
     - despertar la IDLE con OK debe dejar el selector NN/TT negro (LEFT/RIGHT cambian de página).

     Alpha15 se compila con la placa `jwplc_local` (la publicada alpha.12 no tiene MENU).

     Menú del sketch y apertura del Arduino IDE: confirmados por el jefe el 2026-10-18.
2. **Corregir** lo que salga del taller.
3. **Regenerar `libJWPLC_Display.a`** una sola vez:
   - `tools/alpha15/regenerate_libJWPLC_Display.sh build --install` (incluye `JWPLC_UI_Format.cpp`);
   - volver a poner `precompiled=full` en `JWPLC_Display/library.properties`;
   - correr el benchmark de compilación en la PC del jefe.
4. **A15-10**: paquete y docs finales; actualizar STATUS y cerrar.

## Pendientes conocidos (no bloquean el cierre)

- A15-6: `.jwhmi` v2 con modo OpenPLC (elegir variable PLC existente).
- A15-7E: mini-modelos de Apariencia para BAR, figuras, RAW_TEXT y NAV. Usar `designer-zone-indicator.js`.
- A15-5 resto: estática, NAV y PixelMap al núcleo puro del codegen.
- De la crítica de A1: cuadros de color de 24×24 y quitar las letras T/F/V/M.
- Opcional: «Agregar #include al .ino» aditivo y con confirmación (nunca reescribir el `.ino`).
- Glifos propios `Á Í Ó Ú` en la fuente GLCD (regenerar `libJWPLC_TFT.a`): otra alpha, decidido por el jefe.
- FRAM: el mapa del último KB (plan §G.6) se fija en código en I9/I10 (Alpha16).

## Alpha16 (no empezar sin el jefe)

I4–I15 del plan industrial:

- avisos ALARM/INTERLOCK por encima de IDLE/USER y árbitro de teclas;
- SEQUENCE;
- plantillas RECIPE, EVENT_LOG e IO_DIAGNOSTIC;
- tendencia y gauge;
- confirmación CANCELAR/ACEPTAR.

Kenyi: F0e `%MX` en `openplc-editor`.

## Reglas que se aprendieron en esta ronda

- **Paridad por construcción.** El formato de VALUE (`JWPLC_UI_Format.cpp` ↔ `designer-value-format.js`) y el texto cp437 (`utf8ToGlcd` ↔ `designer-cp437.js`) tienen gemelos. Si se cambia uno, se cambia el otro y se corre `tests/value-format-parity.cjs`, que compila el `.cpp` real.
- **Texto C++ siempre por `JWPLCCp437.cppEscape`:** octal, nunca `\xNN`.
- **Variables TEXT con `2 × capacidad + 1` bytes**, porque el sketch escribe UTF-8.
- **Un VALUE de reloj no tiene variable ni setter**, y `setValue` sobre él se ignora.
