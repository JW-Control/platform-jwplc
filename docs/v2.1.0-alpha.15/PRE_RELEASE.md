# v2.1.0-alpha.15 — OpenPLC Editor + HMI Designer: Ladder con pantalla enlazada a sus variables

**Canal:** PreRelease de desarrollo (índice dev). No sustituye a `v2.0.0` estable.
**Origen:** línea de trabajo OpenPLC + HMI (etiqueta de trabajo «ALPHA14», documentación en
`docs/v2.1.0-alpha.14/`), rama `v2.1.0-alpha.15/OpenPLC-HMI/main` en platform-jwplc y en openplc-editor.
Se publica sobre la 2.1.0-alpha.14. El core del package (`JWPLC/2.1.0`) no cambia: ninguna librería es distinta
de la 2.1.0-alpha.14.

## Descargas

| Qué | Dónde |
|---|---|
| **OpenPLC Editor – JWPLC Edition 4.2.8-jwplc.7** (Windows x64, con el HMI Designer adentro) | [Instalador](https://github.com/JW-Control/openplc-editor/releases/download/jwplc-editor-v4.2.8-jwplc.7/OpenPLC.Editor.-.JWPLC.Edition_4.2.8-jwplc.7.exe) · [notas y SHA-256](https://github.com/JW-Control/openplc-editor/releases/tag/jwplc-editor-v4.2.8-jwplc.7) |
| **Paquete de placas JWPLC Basic (VPP) 2.1.0-alpha.15**, firmado (`jwcontrol-2026`) | [VPP](https://github.com/JW-Control/openplc-editor/releases/download/jwplc-editor-v4.2.8-jwplc.7/jwplc-basic-openplc-2.1.0-alpha.15.jwcontrol-signed.vpp) |
| **JWPLC HMI Designer 2.1.0-alpha.15** para Arduino IDE (Setup y portable) | adjuntos a esta Release |
| **Package Arduino** `jwplc:esp32@2.1.0-alpha.15` | índice dev (ver README) |

Los instaladores no tienen firma de código: si Windows SmartScreen avisa, **Más información → Ejecutar de todas
formas**.

## Qué trae

- **HMI Designer dentro de OpenPLC Editor.** En el árbol del proyecto, bajo Device, la entrada **HMI Designer**
  abre el Designer completo en su ventana (cada clic la abre o la trae al frente). De vuelta en OpenPLC, la pestaña
  «HMI Designer» es una **vista de enlaces**: vista previa, páginas y árbol de objetos, y para cada objeto sólo
  «Variable PLC». Ahí no se dibuja ni se mueve nada; el botón «Abrir HMI Designer para diseñar» vuelve a la ventana.
  Mientras la ventana está abierta la vista queda en sólo lectura y se actualiza cada vez que guardas.
- **Variables del Ladder sin dirección** (BOOL, SINT…DINT, USINT…UDINT, BYTE/WORD/DWORD, REAL), además de las que
  tienen `%IX/%QX/%IW/%QW/%MW/%MD`: el HMI las lee y escribe por la tabla de depuración que STruC++ ya compila en el
  firmware. La escritura no fuerza la variable. (Contrato `jwplc-hmi-host` 1.2.)
- **La pantalla se guarda con el proyecto** en `hmi/` (`JWPLC_HMI_Generated.h` + `JWPLC_HMI.jwhmi`): Ctrl+S,
  «Guardar proyecto» y el guardado antes de compilar la incluyen; la pestaña marca los cambios sin guardar. Cerrar
  la ventana con cambios pregunta; cerrar OpenPLC guarda sin preguntar.
- **Variables en vivo:** lo que declares o cambies en el programa aparece en «Variable PLC» sin reabrir. Si el
  proyecto no tiene variables, el Designer explica dónde declararlas.
- `hmi/JWPLC_HMI.jwhmi` sin header ya no detiene la compilación: aviso y la placa arranca con la pantalla IDLE.
- El diagnóstico de la pantalla («Medir tiempos de scan») pasa a *Configuration › Pantalla HMI*; el árbol tiene una
  sola entrada de HMI.
- OpenPLC Editor: *Display › Theme* alterna sólo Claro ↔ Oscuro (sin el tema 90's) y el editor de escritorio ya no
  se oculta en laptops con el aviso de «dispositivos móviles».
- VPP 2.1.0-alpha.15 firmado con la clave `jwcontrol-2026` y verificado con el verificador del editor.
- HMI Designer 2.1.0-alpha.15: versión visible, modo OpenPLC (contrato `jwplc-hmi-host` 1.1–1.4) y arreglo del
  selector «Variable PLC» que se duplicaba en el inspector del BAR.

## Instalación

1. Package: en Arduino IDE agregar el índice dev e instalar **JW Control ESP32 Boards 2.1.0-alpha.15** (ver README).
2. OpenPLC Editor: ejecutar el instalador 4.2.8-jwplc.7 (actualiza una JWPLC Edition anterior).
3. En el editor, importar el VPP 2.1.0-alpha.15 y elegir la placa **JWPLC BASIC [2.0.0]**.
4. En el proyecto, **Device › HMI Designer**.

## Verificación

- `[ejecutado]` HMI Designer: `tools/jwplc-hmi-designer/tests/run-all.cjs` (18 PASS, incluye
  `openplc-host-smoke.cjs` y `openplc-bindings-smoke.cjs`).
- `[ejecutado]` openplc-editor: `tsc`, `eslint`, Jest (los dos fallos de `compiler-module.spec.ts`
  applyPlatformOptions y `debug-spec.test.ts` son anteriores a esta alpha); prueba con Electron real de la ventana
  + pestaña (14/14) y de reabrir la ventana.
- `[ejecutado]` compilación ESP32 del HAL del VPP con enlaces sin dirección; CI del instalador del editor y del
  Designer en verde.
- `[ejecutado]` en la app instalada y en un JWPLC Basic por el propietario (27/10/2026): proyecto Ladder con
  variables enlazadas en la pestaña, compilar, subir y ver la TFT.

## Limitaciones conocidas

- Sólo Windows x64 para los instaladores.
- TEXT todavía no se enlaza a variables PLC (usar VALUE, BOOL o BAR).
- `F0d_JITTER_TFT`: el redibujo de página de la TFT retrasa el scan en ese instante (package, pendiente).
- Al salir del depurador y volver a entrar puede aparecer «Could not verify MD5»; se recupera reconectando el USB.

## Versiones que forman esta Release

| Componente | Versión | Commit |
|---|---|---|
| Package `jwplc:esp32` | 2.1.0-alpha.15 (core = 2.1.0-alpha.14) | `release/v2.1.x` al publicar |
| HMI Designer | 2.1.0-alpha.15 | platform-jwplc `fbb61e6` |
| VPP JWPLC Basic | 2.1.0-alpha.15 (firma `3b46176`) | platform-jwplc `a8458dc` |
| OpenPLC Editor – JWPLC Edition | 4.2.8-jwplc.7 | openplc-editor `9a88922` (tag `jwplc-editor-v4.2.8-jwplc.7`) |
