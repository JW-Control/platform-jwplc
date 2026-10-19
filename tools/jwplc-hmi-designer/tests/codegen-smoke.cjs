#!/usr/bin/env node
// Smoke test del codegen del HMI Designer (A15-1..A15-3).
//
// 1. Sirve poc/ en 127.0.0.1 y abre desktop.html en Chromium headless.
// 2. Carga proyectos de prueba, genera JWPLC_HMI_Generated.h y lo guarda.
// 3. Compila cada header con g++ -fsyntax-only contra los headers reales de
//    JWPLC_Display / JWPLC_UI / JWPLC_TFT (host-stub/ sólo reemplaza Arduino.h
//    y los periféricos que no intervienen).
// 4. Verifica que cada llamada C++ emitida exista en la librería: en el .a si es
//    precompilada (nm -C) o en src/*.cpp si está en modo fuente.
//
// Uso (desde la raíz del repo):
//   node tools/jwplc-hmi-designer/tests/codegen-smoke.cjs [carpeta-salida]
// Requiere: playwright (global), g++, nm, python3.
'use strict';

const fs = require('fs');
const os = require('os');
const path = require('path');
const { spawn, execFileSync } = require('child_process');

const ROOT = path.resolve(__dirname, '..', '..', '..');
const POC = path.join(ROOT, 'tools', 'jwplc-hmi-designer', 'poc');
const STUB = path.join(__dirname, 'host-stub');
const LIBS = path.join(ROOT, 'JWPLC', '2.1.0', 'libraries');
const OUT = path.resolve(process.argv[2] || fs.mkdtempSync(path.join(os.tmpdir(), 'jwplc-hmi-smoke-')));
const PORT = 8765 + Math.floor(Math.random() * 200);

function loadPlaywright() {
  try { return require('playwright'); } catch (_) { /* global */ }
  const globalRoot = execFileSync('npm', ['root', '-g']).toString().trim();
  return require(path.join(globalRoot, 'playwright'));
}

function chromiumPath() {
  const base = process.env.PLAYWRIGHT_BROWSERS_PATH || '/opt/pw-browsers';
  const dir = fs.existsSync(base) ? fs.readdirSync(base).find((d) => /^chromium-\d+$/.test(d)) : null;
  const exe = dir ? path.join(base, dir, 'chrome-linux', 'chrome') : null;
  return exe && fs.existsSync(exe) ? exe : undefined;
}

const COLORS = { labelColor: 65535, valueColor: 2047, backgroundColor: 0, frameColor: 65535 };
const BAR_BASE = {
  ...COLORS, label: 'Nivel', unit: '%', capacity: 1, preview: '', valueSize: 1, labelSize: 1,
  frame: false, layout: 'STACKED', align: 'LEFT', barMin: 0, barMax: 100, barValue: 50,
  showLabel: true, barLabel: 'Nivel', barUnit: '%', barWidth: 110
};

const FULL_PROJECT = {
  // A15-7: IDLE con tecla de entrada y arranque en el HMI. back: 'BUTTON' es de
  // proyectos anteriores ("Con otra tecla", retirado): debe abrir como ESC.
  display: { start: 'HMI', wake: 'BUTTON', wakeButton: 'BTN_OK', back: 'BUTTON', backButton: 'BTN_LEFT', timeoutS: 15 },
  pages: [{ id: 0, name: 'Principal' }, { id: 1, name: 'Alarmas' }, { id: 2, name: 'Ajustes' }],
  fields: [
    { ...COLORS, type: 'TEXT', name: 'T', id: 'FIELD_ESTADO', variable: 'estado', capacity: 12, x: 4, y: 4, preview: 'READY', label: 'Estado', unit: '', valueSize: 2, labelSize: 1, frame: false, layout: 'INLINE', align: 'LEFT', page: 0, layerOrder: 5 },
    { ...COLORS, type: 'VALUE', name: 'V', id: 'FIELD_TEMP', variable: 'temperatura', x: 4, y: 40, preview: '25.6', label: 'Temp', unit: 'C', integerDigits: 3, decimalDigits: 1, signedValue: false, leadingZeros: false, valueSize: 2, labelSize: 1, frame: false, layout: 'INLINE', align: 'RIGHT', page: 0, layerOrder: 6 },
    { ...COLORS, type: 'BOOL', name: 'B', id: 'FIELD_MOTOR', variable: 'motor', x: 4, y: 70, label: 'Motor', unit: '', falseText: 'PARADO', trueText: 'MARCHA', valueSize: 2, labelSize: 1, frame: false, layout: 'INLINE', align: 'CENTER', page: 1, layerOrder: 7 },
    { ...BAR_BASE, type: 'BAR', name: 'G', id: 'FIELD_NIVEL', variable: 'nivel', x: 160, y: 40, page: 2, barPreset: 'CIRCULAR', layerOrder: 8 },
    { ...BAR_BASE, type: 'BAR', name: 'H', id: 'FIELD_GRAD', variable: 'grad', x: 160, y: 90, page: 2, barPreset: 'LINEAR', barFillMode: 'GRADIENT', layerOrder: 9 },
    { type: 'RECT', name: 'Marco', id: 'R1', x: 2, y: 2, x2: 150, y2: 30, frameColor: 0xF800, fillColor: 0x001F, fill: true, cornerRadius: 4, page: 0, layerOrder: 1 },
    { type: 'RAW_TEXT', name: 'Titulo', id: 'RAW_1', text: 'Planta "A"', font: 'SANS_9', size: 1, x: 200, y: 4, textColor: 0xFD20, backgroundColor: 0, transparentBackground: false, page: 0, layerOrder: 2 },
    { type: 'ELLIPSE', name: 'Elipse', id: 'E1', x: 200, y: 40, x2: 260, y2: 70, frameColor: 0xFFFF, fill: true, fillColor: 0x07E0, page: 0, layerOrder: 3 },
    { type: 'TRIANGLE', name: 'Tri', id: 'T1', x: 270, y: 40, x2: 310, y2: 80, frameColor: 0xFFFF, fill: true, fillColor: 0xFFE0, page: 1, layerOrder: 3 },
    { type: 'POLYGON', name: 'Hex', id: 'P1', x: 100, y: 90, x2: 150, y2: 130, sides: 6, frameColor: 0xFFFF, fill: true, fillColor: 0x001F, page: 1, layerOrder: 4 },
    { type: 'LINE', name: 'Linea', id: 'L1', x: 0, y: 140, x2: 319, y2: 140, frameColor: 0x7BEF, page: 2, layerOrder: 10 },
    { key: 'NAV_BAR', type: 'NAV_BAR', name: 'Barra de Navegación', label: 'NAV_BAR', x: 0, y: 147, preset: 'industrial-color', position: 'bottom', height: 22, global: true, hidden: false, locked: false, layerOrder: 999 }
  ]
};

const SHAPES_ONLY = {
  pages: [{ id: 0, name: 'Principal' }],
  fields: [{ type: 'RECT', name: 'Marco', id: 'R1', x: 2, y: 2, x2: 150, y2: 30, frameColor: 0xF800, fill: false, page: 0, layerOrder: 1 }]
};

const LEGACY_GAP = {
  pages: [{ id: 0, name: 'Principal' }, { id: 2, name: 'Ajustes' }],
  fields: [{ ...COLORS, type: 'VALUE', name: 'V', id: 'FIELD_X', variable: 'x', x: 4, y: 40, label: 'X', unit: '', integerDigits: 3, decimalDigits: 0, valueSize: 2, labelSize: 1, frame: false, layout: 'INLINE', align: 'RIGHT', page: 2, layerOrder: 1 }]
};

const failures = [];
function check(condition, message) {
  if (condition) console.log(`  PASS ${message}`);
  else { console.log(`  FAIL ${message}`); failures.push(message); }
}

async function main() {
  fs.mkdirSync(OUT, { recursive: true });
  const server = spawn('python3', ['-m', 'http.server', String(PORT), '--bind', '127.0.0.1'], { cwd: POC, stdio: 'ignore' });
  await new Promise((resolve) => setTimeout(resolve, 800));

  const { chromium } = loadPlaywright();
  const browser = await chromium.launch({ executablePath: chromiumPath() });
  const page = await browser.newPage({ viewport: { width: 1600, height: 1000 } });
  const pageErrors = [];
  page.on('pageerror', (error) => pageErrors.push(error.message));

  try {
    await page.goto(`http://127.0.0.1:${PORT}/desktop.html`);
    await page.waitForFunction(() => window.JWPLCHMIProject?.apply && window.JWPLCHMICodegen?.refresh, null, { timeout: 15000 });
    await page.waitForTimeout(1500);

    const load = (project) => page.evaluate(async (proj) => {
      const P = window.JWPLCHMIProject;
      const data = { ...P.serialize(), display: null, ...JSON.parse(JSON.stringify(proj)), pixelMaps: [] };
      let error = null;
      try { await P.apply(data); } catch (e) { error = e.message; }
      await new Promise((r) => setTimeout(r, 600));
      return error;
    }, project);

    const header = () => page.evaluate(async () => {
      window.JWPLCHMIEditor.setSelectedFieldKeys([]);
      window.JWPLCHMIEditor.render();
      for (let k = 0; k < 8; k += 1) {
        document.getElementById('contractTab')?.click();
        await new Promise((r) => setTimeout(r, 150));
        window.JWPLCHMICodegen.refresh();
        await new Promise((r) => setTimeout(r, 150));
        const text = document.getElementById('codeOutput').textContent;
        if (text.startsWith('// Código generado por JWPLC HMI Designer')) return text;
      }
      return null;
    });

    const headers = {};

    console.log('Escenario: proyecto completo (3 páginas, NAV_BAR, figuras, BAR circular/degradado, fuente no GLCD)');
    check((await load(FULL_PROJECT)) === null, 'el proyecto abre');
    headers.full = await header();
    check(Boolean(headers.full), 'se genera el header');

    console.log('Escenario: borrar página intermedia y reabrir (A15-2)');
    const afterDelete = await page.evaluate(async () => {
      const E = window.JWPLCHMIEditor;
      E.deletePage(1);
      const data = JSON.parse(JSON.stringify(window.JWPLCHMIProject.serialize()));
      let error = null;
      try { await window.JWPLCHMIProject.apply(data); } catch (e) { error = e.message; }
      await new Promise((r) => setTimeout(r, 600));
      return { error, pages: E.getPages().map((p) => p.id) };
    });
    check(afterDelete.error === null, 'el proyecto reabre tras borrar una página intermedia');
    check(JSON.stringify(afterDelete.pages) === '[0,1]', `ids contiguos tras borrar (${JSON.stringify(afterDelete.pages)})`);
    headers.afterDelete = await header();

    console.log('Escenario: proyecto legado con ids [0, 2]');
    check((await load(LEGACY_GAP)) === null, 'se normaliza en lugar de rechazarse');
    const legacyPages = await page.evaluate(() => window.JWPLCHMIEditor.getAllFields().map((f) => f.page));
    check(JSON.stringify(legacyPages) === '[1]', 'el campo de la página 2 pasa a la página 1');

    console.log('Escenario: sólo gráfica estática');
    check((await load(SHAPES_ONLY)) === null, 'el proyecto abre');
    headers.shapesOnly = await header();
    check(Boolean(headers.shapesOnly), 'se genera el header sin campos dinámicos');

    console.log('Escenario: VALUE con miles y duración, BOOL piloto (Alpha15 I1)');
    check((await load(require('./fixtures.cjs').i1Formats)) === null, 'el proyecto abre');
    headers.i1Formats = await header();
    const vf = headers.i1Formats || '';
    check(/JWPLC_UIDurationFormat\(\n\s*JWPLC_UI_DURATION_MM_SS/.test(vf) && /'\.'\)/.test(vf) && /int32_t t_restante = 0;/.test(vf),
      'emite JWPLC_UIDurationFormat, el separador y variables enteras');
    check(/JWPLC_UIValueFormat JWPLC_UIDurationFormat\(/.test(fs.readFileSync(path.join(LIBS, 'JWPLC_Display', 'src', 'JWPLC_UI_Format.cpp'), 'utf8')),
      'JWPLC_UIDurationFormat está definido en el firmware (modo fuente)');
    check(/JWPLC_UIBoolPilot\(\n\s*0x4208,\n\s*0x07E0\),/.test(vf) && /JWPLC_UIBoolPilot\(\n\s*0x4208,\n\s*0xFFE0,\n\s*0x0000,\n\s*0xF800\),/.test(vf),
      'BOOL piloto: JWPLC_UIBoolPilot de 2 argumentos y de 4 (fondo por estado)');

    console.log('Escenario: VALUE de fecha/hora del RTC (Alpha15 I2)');
    check((await load(require('./fixtures.cjs').i2Clock)) === null, 'el proyecto abre');
    headers.i2Clock = await header();
    const ck = headers.i2Clock || '';
    check(/JWPLC_UIDateTimeFormat\(\n\s*JWPLC_UI_DATETIME_HH_MM_SS\)/.test(ck) && /JWPLC_UI_DATETIME_DD_MM_YYYY\)/.test(ck),
      'emite JWPLC_UIDateTimeFormat');
    check(!/\bhora\b/.test(ck.slice(ck.indexOf('// Variables HMI'), ck.indexOf('// Definición declarativa'))) && !/setValue\(FIELD_HORA|setValue\(FIELD_FECHA/.test(ck) && /setValue\(FIELD_TEMP, temperatura\)/.test(ck),
      'los relojes no declaran variable ni setter (la misma variable repetida no es un duplicado)');
    check(!(await page.evaluate(() => window.JWPLCHMICodegen.validateIdentifiers().length)), 'sin avisos de identificadores duplicados');

    console.log('Escenario: tildes, ñ y ° en cp437 (Alpha15 I3)');
    check((await load(require('./fixtures.cjs').i3Accents)) === null, 'el proyecto abre');
    headers.i3Accents = await header();
    const ac = headers.i3Accents || '';
    check(/JWPLC_UIText\("Presi\\242n", "\\367C"\)/.test(ac) && /tft\.print\("L\\241nea 2 \\371 \\250Listo\?"\)/.test(ac),
      'etiquetas, unidades y RAW_TEXT GLCD con bytes cp437 en octal');

    console.log('Escenario: barra de navegación (6 estilos x 3 tipografías x horizontal/vertical)');
    check((await load(FULL_PROJECT)) === null, 'el proyecto abre');
    const presets = ['industrial-color', 'dark-pill', 'segmented-bar', 'compact-dots', 'edge-tabs', 'numeric-pills'];
    const fontSizes = ['micro', 'normal', 'medium'];
    const positions = ['bottom', 'left'];
    let navCount = 0;
    for (const preset of presets) {
      for (const fontSize of fontSizes) {
        for (const position of positions) {
          await page.evaluate((cfg) => {
            window.JWPLCHMIPages.setNavBarConfig(cfg);
          }, { preset, fontSize, position, textAlign: navCount % 3 === 0 ? 'left' : (navCount % 3 === 1 ? 'center' : 'right'), labelFormat: navCount % 2 ? 'num_short' : 'short', borderMode: 'line' });
          const text = await header();
          const key = `nav_${preset}_${fontSize}_${position}`;
          if (!text || !/static void jwplcDrawPageNavBar/.test(text)) {
            check(false, `${key}: barra generada`);
          } else {
            headers[key] = text;
            const usesMicroText = /jwplcHMIDrawMicroText\(tft,/.test(text);
            const expectsMicro = fontSize === 'micro' && preset !== 'compact-dots';
            if (usesMicroText !== expectsMicro) check(false, `${key}: microfuente ${usesMicroText ? 'presente' : 'ausente'}`);
            // Alpha15: pestaña activa en dos estados + redibujo al cambiar de modo.
            const tabFn = text.slice(text.indexOf('static void jwplcDrawPageNavTab'), text.indexOf('static void jwplcDrawPageNavBar'));
            if (!/if \(selecting\)/.test(tabFn) || !/jwplcDrawPageNavTab\(activePage, JWPLC_Display\.isUserPageSelection\(\)\)/.test(text) ||
                !/extern "C" void jwplcUIPageModeChanged\(uint8_t page, bool selecting\)/.test(text)) {
              check(false, `${key}: pestaña activa en dos estados y jwplcUIPageModeChanged`);
            }
          }
          navCount += 1;
        }
      }
    }
    check(Object.keys(headers).filter((k) => k.startsWith('nav_')).length === presets.length * fontSizes.length * positions.length,
      `${navCount} combinaciones generan jwplcDrawPageNavBar, la pestaña activa en dos estados y jwplcUIPageModeChanged`);

    check(pageErrors.length === 0, `sin errores JS (${pageErrors.join(' | ') || 'ninguno'})`);

    console.log('Contenido del header');
    const full = headers.full || '';
    check(!/getTFT/.test(Object.values(headers).join('\n')), 'no usa JWPLC_Display.getTFT()');
    check(!/JWPLC_UICircularField|JWPLC_UIGradientBarField/.test(full), 'no usa helpers BAR inexistentes');
    const fontCallsOutsideGuard = full.split('\n').reduce((state, line) => {
      if (/^\s*#if JWPLC_HMI_TFT_FONTS/.test(line)) state.inside = true;
      else if (/^\s*#(else|endif)/.test(line)) state.inside = false;
      else if (/setFont|drawString|setTextDatum/.test(line) && !state.inside) state.count += 1;
      return state;
    }, { inside: false, count: 0 }).count;
    check(/tft\.setFont\(JWPLC_FONT_SANS_9\)/.test(full), 'RAW_TEXT con FreeFont usa setFont(JWPLC_FONT_SANS_9)');
    check(fontCallsOutsideGuard === 0, 'la API de fuentes sólo aparece dentro de #if JWPLC_HMI_TFT_FONTS');
    const update = full.slice(full.indexOf('void jwplcUIUpdate()'));
    check(!/tft\.|jwplcHMIFill|jwplcHMIDraw|jwplcDrawPageNavBar/.test(update), 'jwplcUIUpdate() sólo contiene setters (A15-3)');
    check(/void jwplcUIPageEnter\(uint8_t page\)/.test(full), 'la gráfica estática está en jwplcUIPageEnter()');
    check(/setIdleWakeButton\(BTN_OK\);\n\s*JWPLC_Display\.setIdleReturnMode\(JWPLC_DisplayClass::IDLE_RETURN_ESC_ONLY\);\n\s*JWPLC_Display\.enterUserUI\(\);\n\}/.test(full) && !/setIdleReturnButton/.test(full), 'IDLE: tecla de entrada, vuelta "otra tecla" antigua -> ESC y arranque en el HMI al final de jwplcHMISetup()');

    console.log('Compilación host (g++ -fsyntax-only) contra headers reales');
    // .a si la librería es precompilada; definiciones en src/*.cpp si está en modo fuente.
    const { librarySymbols } = require('./lib-symbols.cjs');
    const symbols = new Set();
    for (const lib of ['JWPLC_Display', 'JWPLC_TFT']) {
      const info = librarySymbols(path.join(LIBS, lib));
      info.symbols.forEach((s) => symbols.add(s));
      console.log(`  ${lib}: símbolos desde ${info.mode === 'precompiled' ? `lib${lib}.a` : 'el fuente (modo fuente durante el alpha)'}`);
    }

    let navCompiled = 0;
    for (const [name, text] of Object.entries(headers)) {
      if (!text) continue;
      const hPath = path.join(OUT, `${name}.h`);
      const cPath = path.join(OUT, `${name}.cpp`);
      fs.writeFileSync(hPath, text);
      fs.writeFileSync(cPath, `#include "${name}.h"\nint main() { jwplcHMISetup(); jwplcUIPageEnter(0); jwplcUIUpdate(); return 0; }\n`);
      let ok = true;
      let log = '';
      try {
        execFileSync('g++', ['-std=gnu++17', '-fsyntax-only', '-Wall', `-I${OUT}`, `-I${STUB}`,
          `-I${path.join(LIBS, 'JWPLC_Display', 'src')}`, `-I${path.join(LIBS, 'JWPLC_TFT', 'src')}`, cPath],
        { stdio: 'pipe' });
      } catch (error) {
        ok = false;
        log = String(error.stderr || error.message).split('\n').slice(0, 6).join('\n');
      }
      if (!name.startsWith('nav_') || !ok) check(ok, `${name}.h compila${ok ? '' : `\n${log}`}`);
      else navCompiled += 1;

      const calls = new Set();
      for (const m of text.matchAll(/\btft\.([A-Za-z]+)\(/g)) calls.add(`JWPLC_TFTClass::${m[1]}`);
      for (const m of text.matchAll(/JWPLC_Display\.([A-Za-z]+)\(/g)) calls.add(`JWPLC_DisplayClass::${m[1]}`);
      const inlineOk = new Set(['JWPLC_TFTClass::print', 'JWPLC_DisplayClass::setValue']);
      const missing = [...calls].filter((c) => !symbols.has(c) && !inlineOk.has(c));
      if (!name.startsWith('nav_') || missing.length) check(missing.length === 0, `${name}.h: todas las llamadas existen en los .a (${missing.join(', ') || 'ok'})`);
    }
    if (navCompiled) check(true, `${navCompiled} headers de barra de navegación compilan y sus llamadas existen en los .a`);
  } finally {
    await browser.close();
    server.kill();
  }

  console.log(`\nHeaders guardados en ${OUT}`);
  if (failures.length) {
    console.log(`${failures.length} verificación(es) fallaron.`);
    process.exit(1);
  }
  console.log('Todas las verificaciones pasaron.');
}

main().catch((error) => { console.error(error); process.exit(1); });
