# Iconos Industriales V2: Torre Modular, Pilotos Configurables, Motores, Pistones y Color Personalizable

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Implementar la segunda versión del sistema de iconos industriales del HMI Designer: unificar pilotos en un componente configurable por estados y colores, rediseñar el motor trifásico y añadir motorreductor, incorporar pistones neumáticos animados, crear la torre de señalización modular con control bit a bit y enlace a variables OpenPLC, y permitir editar el color principal de cualquier icono.

**Architecture:** Modificación de `library.cjs` para generar los nuevos componentes y retirar duplicados; actualización de `designer-icons-core.js` para soportar sobreescritura dinámica de paletas (`field.stateColors` y `field.customColors`); ampliación de `designer-icons.js` con controles de inspector para personalización de colores, estados y pisos; y actualización de `designer-openplc-host.js` para enlazar cada piso de la torre a variables PLC booleanas independientes.

**Tech Stack:** JavaScript (ES6 / CommonJS sin bundler para el Diseñador Web), C++17 (Firmware JWPLC Basic / ESP32-S3 TFT SPI), Node.js (herramientas de compilación y pruebas del catálogo).

**Spec:** C:\Users\Andre\.gemini\antigravity\brain\ef8b1cbc-7e26-43fd-bae4-e031f1f22a3e\diseno-torre-pilotos-motores-pistones.md

## Global Constraints
- Target hardware: JWPLC Basic (ESP32-S3, ST7789 320x170 SPI a 40 MHz).
- Máximo 16 colores (paleta 4 bpp) por icono.
- Tamaños estándar requeridos: 24, 32 y 48 px (y 64 px donde aplique).
- Línea de código C++ generada y proyectos `.jwhmi` existentes deben mantener 100% de compatibilidad hacia atrás.
- Respetar saltos de línea y comprobación en Windows CRLF/LF.

## Review Focus
- Desempeño en ESP32: El cambio de color de paleta no debe duplicar imágenes en memoria flash (reutiliza `images[]` cambiando únicamente `palette[]`).
- La torre en modo pisos independientes debe generar variables limpias en OpenPLC sin colisionar nombres.
- El piloto unificado debe permitir transitar fluidamente entre `bool` (2 estados) y `uint8_t` (hasta 8 estados) sin errores en tiempo de ejecución.
- Las animaciones de motor, motorreductor y pistones deben ser fluidas y estables (intervalos entre 90 y 130 ms).

---

### Task 1: Limpieza del catálogo y consolidación del Piloto configurable en `library.cjs`
**Files:**
- Modify: `tools/jwplc-hmi-designer/icons/library.cjs`
- Modify: `tools/jwplc-hmi-designer/icons/build-catalog.cjs`
- Test: `tools/jwplc-hmi-designer/tests/icon-catalog.test.cjs`

- [x] 1.1 Retirar de `library.cjs` las variantes duplicadas de pilotos (`piloto_verde`, `piloto_rojo`, `piloto_amarillo`, `piloto_azul`, `piloto_cuadrado`, `piloto_cuadrado_rojo`, `piloto_bisel`).
- [x] 1.2 Unificar la definición de `pilotos/piloto`:
  - Definir roles de color: `R` (borde/aro exterior metálico), `G` (reflejo de bisel), `B` (cuerpo de luz encendida/apagada), `L` (núcleo brillante de iluminación).
  - Diseñar 2 estados base (`0 = Apagado`, `1 = Encendido`) con soporte para extenderse a N estados.
- [x] 1.3 Recompilar catálogo con `node tools/jwplc-hmi-designer/icons/build-catalog.cjs` y verificar que no haya referencias rotas.
- [x] 1.4 Ejecutar `node tools/jwplc-hmi-designer/tests/icon-catalog.test.cjs` y confirmar PASS.

---

### Task 2: Rediseño total de Motores (`motores/motor` y nuevo `motores/motorreductor`)
**Files:**
- Modify: `tools/jwplc-hmi-designer/icons/library.cjs`
- Modify: `tools/jwplc-hmi-designer/icons/build-catalog.cjs`
- Test: `tools/jwplc-hmi-designer/tests/icon-catalog.test.cjs`

- [x] 2.1 Rediseñar `motores/motor` desde cero:
  - Carcasa cilíndrica de fundición con aletas horizontales.
  - Bornera de conexiones superior bien delimitada.
  - Patas de anclaje sólidas.
  - Tapa de ventilador trasera con rejilla y 4 fotogramas de aspas en rotación a 120 ms.
  - Eje frontal con chavetero.
- [x] 2.2 Diseñar el nuevo componente `motores/motorreductor`:
  - Carcasa de motor eléctrico acoplada a caja de reducción de engranajes con nervaduras y tapón de aceite.
  - Eje de salida perpendicular de gran calibre con chaveta giratoria en 4 fotogramas a 130 ms.
- [x] 2.3 Recompilar catálogo y verificar en los 4 tamaños (24, 32, 48, 64 px).
- [x] 2.4 Ejecutar `node tools/jwplc-hmi-designer/tests/icon-catalog.test.cjs`.

---

### Task 3: Nuevos Cilindros Neumáticos / Pistones animados (`equipos/piston` y `equipos/piston_vertical`)
**Files:**
- Modify: `tools/jwplc-hmi-designer/icons/library.cjs`
- Test: `tools/jwplc-hmi-designer/tests/icon-catalog.test.cjs`

- [x] 3.1 Diseñar `equipos/piston` (horizontal):
  - Camisa neumática de perfil con tirantes y culatas de aluminio.
  - Vástago de acero cromado con rosca y horquilla.
  - Estado 0: Retraído.
  - Estado 1: En movimiento (animación multi-cuadro de 4 fotogramas a 100 ms con avance del vástago).
  - Estado 2: Extendido.
  - Estado 3: Alarma/Falla (cilindro en rojo).
- [x] 3.2 Diseñar `equipos/piston_vertical` con orientación vertical correspondiente.
- [x] 3.3 Recompilar catálogo con `node tools/jwplc-hmi-designer/icons/build-catalog.cjs`.
- [x] 3.4 Verificar decodificación de cuadros y dimensiones en `tests/icon-catalog.test.cjs`.

---

### Task 4: Torre de Señalización Modular (`pilotos/baliza`)
**Files:**
- Modify: `tools/jwplc-hmi-designer/icons/library.cjs`
- Test: `tools/jwplc-hmi-designer/tests/icon-catalog.test.cjs`

- [x] 4.1 Diseñar la silueta de baliza modular:
  - Base y mástil de sujeción inferior.
  - 4 niveles de lentes prismáticas apiladas (Piso 1: Verde, Piso 2: Ámbar, Piso 3: Rojo, Piso 4: Azul).
  - Tapa de remate superior.
- [x] 4.2 Generar las imágenes y estados para permitir encender cualquier combinación de pisos (máscara de bits 0..15).
- [x] 4.3 Recompilar catálogo y verificar integridad.

---

### Task 5: Soporte en `designer-icons-core.js` para paletas dinámicas y color de equipo
**Files:**
- Modify: `tools/jwplc-hmi-designer/poc/designer-icons-core.js`
- Test: `tools/jwplc-hmi-designer/tests/icon-catalog.test.cjs`

- [x] 5.1 En `decode(entry, state, frame)`:
  - Permitir que si el campo o la entrada define `customPalette` o `stateColors[state]`, se aplique sobre la paleta base del estado.
- [x] 5.2 En `assetBlock(asset)`:
  - Emitir la paleta personalizada en C++ (`sym_PAL`) cuando el campo tenga colores sobreescritos, preservando compatibilidad exacta con `JWPLC_UI.h`.

---

### Task 6: Inspector en `designer-icons.js` para Piloto, Torre y Color de Cuerpo
**Files:**
- Modify: `tools/jwplc-hmi-designer/poc/designer-icons.js`
- Modify: `tools/jwplc-hmi-designer/poc/app.js`

- [x] 6.1 En el Inspector de `pilotos/piloto`:
  - Selector de modo: `bool` (ON/OFF de 2 estados) vs `uint8_t` (2 a 8 estados).
  - Configuración de color interactiva por estado (con el selector RGB565 existente).
- [x] 6.2 En el Inspector de cualquier icono (*Diseño > Apariencia*):
  - Añadir selector interactivo para "Color del equipo / cuerpo", permitiendo personalizar el tono principal de la carcasa.
- [x] 6.3 En el Inspector de la Torre de señalización:
  - Selector de 1 a 4 pisos.
  - Selector de color de cada piso.
  - Selector de modo: Máscara (`uint8_t`) vs Pisos independientes (`bool`).

---

### Task 7: Integración OpenPLC para sub-entradas de la Torre en `designer-openplc-host.js`
**Files:**
- Modify: `tools/jwplc-hmi-designer/poc/designer-openplc-host.js`

- [x] 7.1 Detectar en `findVariableInputs()` y `describeTarget()` las variables independientes de cada piso de la baliza.
- [x] 7.2 Permitir que el selector de "Variable PLC" se asocie a cada piso (`piso1`, `piso2`, `piso3`, `piso4`) con su correspondiente `plcBinding`.

---

### Task 8: Verificación integral y pruebas automáticas
**Files:**
- Test: `tools/jwplc-hmi-designer/tests/icon-catalog.test.cjs`
- Test: `tools/jwplc-hmi-designer/tests/codegen-core.test.cjs`
- Test: `tools/jwplc-hmi-designer/cli/jwhmi.cjs validate --fast`

- [x] 8.1 Ejecutar `node tools/jwplc-hmi-designer/icons/build-catalog.cjs --check`.
- [x] 8.2 Ejecutar `node tools/jwplc-hmi-designer/tests/icon-catalog.test.cjs`.
- [x] 8.3 Ejecutar `node tools/jwplc-hmi-designer/tests/codegen-core.test.cjs`.
- [x] 8.4 Validar todos los proyectos de prueba con `jwhmi.cjs validate --fast`.
