#!/usr/bin/env node
// Smoke test del objeto MENU en el Designer (A15-8).
//
// 1. Ejecuta tests/menu-host-sim.cjs (firmware real en host) para obtener las
//    capturas RGB565 de referencia.
// 2. Abre desktop.html en Chromium headless: crea un MENU con la herramienta,
//    edita el árbol desde el inspector, verifica undo, guardar/abrir y el
//    límite de profundidad.
// 3. Carga el mismo menú que la simulación y compara el lienzo del Designer
//    con el framebuffer del firmware píxel a píxel (raíz, nivel 2, nivel 3).
// 4. Compila el header generado con g++ -fsyntax-only contra los headers
//    reales y verifica que setMenus exista en libJWPLC_Display.a.
//
// Uso (desde la raíz del repo):
//   node tools/jwplc-hmi-designer/tests/menu-smoke.cjs [carpeta-salida]
// Requiere: playwright (local o global) con Chromium, g++ y nm.
'use strict';

const fs = require('fs');
const os = require('os');
const path = require('path');
const http = require('http');
const { execFileSync } = require('child_process');

const ROOT = path.resolve(__dirname, '..', '..', '..');
const POC = path.resolve(__dirname, '..', 'poc');
const STUB = path.join(__dirname, 'host-stub');
const LIBS = path.join(ROOT, 'JWPLC', '2.1.0', 'libraries');
const OUT = path.resolve(process.argv[2] || fs.mkdtempSync(path.join(os.tmpdir(), 'jwplc-hmi-menu-')));

function loadPlaywright() {
  try { return require('playwright'); } catch (_) { /* global */ }
  const globalRoot = execFileSync(process.platform === 'win32' ? 'npm.cmd' : 'npm', ['root', '-g'], { shell: process.platform === 'win32' }).toString().trim();
  return require(path.join(globalRoot, 'playwright'));
}

function chromiumPath() {
  const base = process.env.PLAYWRIGHT_BROWSERS_PATH || '/opt/pw-browsers';
  const dir = fs.existsSync(base) ? fs.readdirSync(base).find((d) => /^chromium-\d+$/.test(d)) : null;
  const exe = dir ? path.join(base, dir, 'chrome-linux', 'chrome') : null;
  return exe && fs.existsSync(exe) ? exe : undefined;
}

const MIME = { '.html': 'text/html', '.js': 'text/javascript', '.css': 'text/css', '.svg': 'image/svg+xml', '.png': 'image/png', '.json': 'application/json', '.webmanifest': 'application/manifest+json' };

function serve() {
  const server = http.createServer((req, res) => {
    const rel = decodeURIComponent(new URL(req.url, 'http://x').pathname).replace(/^\/+/, '') || 'index.html';
    const file = path.join(POC, rel);
    if (!file.startsWith(POC) || !fs.existsSync(file) || fs.statSync(file).isDirectory()) {
      res.writeHead(404); res.end(); return;
    }
    res.writeHead(200, { 'Content-Type': MIME[path.extname(file)] || 'application/octet-stream' });
    fs.createReadStream(file).pipe(res);
  });
  return new Promise((resolve) => server.listen(0, '127.0.0.1', () => resolve(server)));
}

function hasTool(cmd, args) {
  try { execFileSync(cmd, args, { stdio: 'ignore' }); return true; } catch (_) { return false; }
}
const HAS_GPP = hasTool('g++', ['--version']);
const HAS_NM = hasTool('nm', ['--version']);

const failures = [];
function skip(message) {
  console.log(`  SKIP ${message}`);
}
function check(condition, message) {
  if (condition) console.log(`  PASS ${message}`);
  else { console.log(`  FAIL ${message}`); failures.push(message); }
}

// Mismo árbol que tests/menu-host/menu_sim.cpp y gates/A15_8_Menu.
const n = (key, parent, type, label, extra = {}) => ({
  key, parent, type, label, variable: '', valueType: '', initial: 0, min: 0, max: 100, step: 1,
  decimals: 1, unit: '', falseText: 'OFF', trueText: 'ON', ...extra
});
const GATE_MENU = {
  key: 'menu-1', type: 'MENU', name: 'Ajustes', id: 'MENU_AJUSTES', title: 'Ajustes',
  x: 10, y: 20, width: 300, height: 140, page: 0, textSize: 1, frame: true, layerOrder: 1,
  colors: { text: 0xFFFF, background: 0x0000, value: 0x07FF, selectText: 0x0000, selectBackground: 0x07FF, editText: 0x0000, editBackground: 0xFD20, title: 0x0000, titleBackground: 0xFFFF, frame: 0xFFFF },
  nodes: [
    n('t', null, 'VIEW', 'Temperatura', { variable: 'temperatura', valueType: 'float', initial: 21.5, decimals: 1, unit: 'C' }),
    n('c', null, 'EDIT_NUM', 'Consigna', { variable: 'consigna', valueType: 'float', initial: 45, min: 0, max: 100, step: 0.5, decimals: 1, unit: 'C' }),
    n('b', null, 'EDIT_BOOL', 'Bomba', { variable: 'bomba', valueType: 'bool', initial: 0, falseText: 'PARADA', trueText: 'MARCHA' }),
    n('a', null, 'SUBMENU', 'Avanzado'),
    n('ci', 'a', 'VIEW', 'Ciclos', { variable: 'ciclos', valueType: 'int32', initial: 0 }),
    n('r', 'a', 'EDIT_NUM', 'Retardo', { variable: 'retardo', valueType: 'int32', initial: 30, min: 0, max: 600, step: 5, decimals: 0, unit: 's' }),
    n('al', 'a', 'SUBMENU', 'Alarmas'),
    n('h', 'al', 'EDIT_NUM', 'Alta', { variable: 'alarmaAlta', valueType: 'float', initial: 80, min: 0, max: 150, step: 1, decimals: 0, unit: 'C' }),
    n('hb', 'al', 'EDIT_BOOL', 'Habilitada', { variable: 'alarmaHabilitada', valueType: 'bool', initial: 1, falseText: 'NO', trueText: 'SI' })
  ]
};

async function main() {
  fs.mkdirSync(OUT, { recursive: true });

  console.log('SIMULACIÓN DEL FIRMWARE');
  const simDir = path.join(OUT, 'sim');
  if (HAS_GPP) {
    let simOk = true;
    try {
      execFileSync(process.execPath, [path.join(__dirname, 'menu-host-sim.cjs'), simDir], { stdio: 'pipe' });
    } catch (error) {
      simOk = false;
      console.log(String(error.stdout || '').split('\n').filter((l) => /FAIL/.test(l)).join('\n'));
    }
    check(simOk, 'menu-host-sim.cjs pasa (firmware real en host)');
  } else {
    skip('sin g++: no se simula el firmware ni se compara el lienzo con él');
  }

  const server = await serve();
  const { chromium } = loadPlaywright();
  const browser = await chromium.launch({ executablePath: chromiumPath() });
  const page = await browser.newPage({ viewport: { width: 1600, height: 1400 } });
  const errors = [];
  page.on('pageerror', (e) => errors.push(e.message));
  page.on('console', (m) => { if (m.type() === 'error') errors.push(m.text()); });
  const S = '#menuInspectorSection ';
  const shot = async (name) => {
    const el = await page.$('#menuInspectorSection');
    if (el) await el.screenshot({ path: path.join(OUT, `${name}.png`) });
  };
  const menuField = () => page.evaluate(() => JSON.parse(JSON.stringify(window.JWPLCHMIEditor.getAllFields().find((f) => f.type === 'MENU') || null)));
  const header = () => page.evaluate(async () => {
    const E = window.JWPLCHMIEditor;
    E.setSelectedFieldKeys([]);
    E.render();
    document.getElementById('contractTab')?.click();
    await new Promise((r) => setTimeout(r, 400));
    window.JWPLCHMICodegen.refresh();
    return document.getElementById('codeOutput').textContent;
  });

  try {
    await page.goto(`http://127.0.0.1:${server.address().port}/desktop.html`);
    await page.waitForFunction(() => window.JWPLCHMIMenu && window.JWPLCHMIProject && window.JWPLCHMICodegen, null, { timeout: 20000 });

    console.log('CREAR DESDE LA HERRAMIENTA');
    await page.evaluate(() => document.querySelector('[data-tool="menuField"]').click());
    await page.waitForTimeout(300);
    let m = await menuField();
    check(Boolean(m) && m.nodes.length > 0, 'la herramienta crea un MENU con un árbol de ejemplo');
    const visible = await page.evaluate(() => ({
      menu: document.getElementById('menuInspectorSection').offsetParent !== null,
      field: document.getElementById('textFieldControlsSection').offsetParent !== null
    }));
    check(visible.menu && !visible.field, 'se muestra el inspector MENU y no el de campos');
    await shot('menu_forma');
    await page.click(`${S}[data-menu-sub="appearance"]`);
    await page.$eval(`${S}[data-menu-swatch="selectText"]`, (el) => el.dispatchEvent(new MouseEvent('click', { bubbles: true })));
    await page.waitForTimeout(150);
    // Indicador A1: ▶ a la altura de la fila activa, contorno de doble trazo
    // y sin tocar los píxeles del mini-modelo.
    const indicator = await page.evaluate((sel) => {
      const out = [];
      document.querySelectorAll(`${sel}[data-menu-case-body]`).forEach((body) => {
        const marker = body.querySelector('svg.zone-indicator path.zone-marker');
        const row = body.querySelector('.menu-legend-row.is-active');
        if (!marker) return;
        const m = marker.getBoundingClientRect();
        const r = row?.getBoundingClientRect();
        out.push({
          accent: body.querySelectorAll('svg.zone-indicator rect.zone-outline-accent').length,
          dark: body.querySelectorAll('svg.zone-indicator rect.zone-outline-dark').length,
          aligned: Boolean(r) && Math.abs((m.top + m.bottom) / 2 - (r.top + r.bottom) / 2) <= r.height / 2
        });
      });
      return out;
    }, S);
    check(indicator.length >= 1 && indicator.every((i) => i.accent === 1 && i.dark === 1 && i.aligned),
      `Apariencia A1: contorno doble y ▶ a la altura de la fila activa (${JSON.stringify(indicator)})`);
    const pixels = async () => page.$$eval(`${S}canvas[data-menu-case]`, (els) => els.map((c) => c.toDataURL()).join('|'));
    const pixelsA = await pixels();
    await page.$eval(`${S}[data-menu-swatch="titleBackground"]`, (el) => el.dispatchEvent(new MouseEvent('click', { bubbles: true })));
    await page.waitForTimeout(150);
    check(pixelsA === await pixels(), 'Apariencia A1: los píxeles del mini-modelo no cambian con el indicador');
    await page.$eval(`${S}[data-menu-swatch="selectText"]`, (el) => el.dispatchEvent(new MouseEvent('click', { bubbles: true })));
    await page.waitForTimeout(150);
    // El texto de la fila elige su primer color (o conserva el activo si es
    // de esa fila); el editor fijo sigue a la vista.
    await page.click(`${S}[data-menu-legend-row] .menu-legend-label >> text=Título`);
    const rowPick = await page.evaluate((sel) => ({
      active: document.querySelector(`${sel}[data-menu-swatch="titleBackground"]`)?.classList.contains('active'),
      target: document.querySelector(`${sel}[data-ce-target]`)?.textContent,
      native: document.querySelectorAll(`${sel}input[type="color"]`).length
    }), S);
    check(rowPick.active && rowPick.native === 0, `Apariencia: clic en el texto «Título» elige su color (${JSON.stringify(rowPick)})`);
    await page.$eval(`${S}[data-menu-swatch="selectText"]`, (el) => el.dispatchEvent(new MouseEvent('click', { bubbles: true })));
    await page.waitForTimeout(150);
    // Pastilla del editor con el mismo color: no cambia el modelo.
    const current = (await menuField()).colors.selectText & 0xFFFF;
    const pill = await page.$(`${S}[data-menu-palette="${current}"]`);
    if (pill) await pill.click();
    await shot('menu_apariencia');
    await page.click(`${S}[data-menu-sub="shape"]`);

    console.log('INSPECTOR');
    await page.click(`${S}[data-menu-main="tree"]`);
    const before = (await menuField()).nodes.length;
    await page.click(`${S}[data-menu-add="VIEW"]`);
    m = await menuField();
    check(m.nodes.length === before + 1, 'agregar un ítem "Ver"');
    await page.fill(`${S}[data-node-prop="label"]`, 'Presion');
    await page.press(`${S}[data-node-prop="label"]`, 'Tab');
    await page.click(`${S}[data-node-kind="int32"]`);
    m = await menuField();
    const added = m.nodes[m.nodes.length - 1];
    check(added.label === 'Presion' && added.valueType === 'int32', 'editar texto y tipo del ítem');
    await shot('menu_arbol');
    await page.evaluate(() => window.JWPLCHMIEditor.undo());
    await page.waitForTimeout(150);
    m = await menuField();
    check(m.nodes[m.nodes.length - 1].valueType === 'float', 'undo revierte el último cambio del árbol');
    await page.evaluate(() => window.JWPLCHMIEditor.redo());

    // Profundidad: raíz > Avanzado (nivel 2) > submenú nuevo (nivel 2 -> abre nivel 3).
    const advKey = m.nodes.find((x) => x.type === 'SUBMENU').key;
    await page.click(`${S}[data-menu-node="${advKey}"]`);
    await page.click(`${S}[data-menu-add="SUBMENU"]`);
    m = await menuField();
    const sub2 = m.nodes[m.nodes.length - 1];
    check(sub2.parent === advKey, 'submenú dentro de "Avanzado"');
    const disabled = await page.evaluate(() => document.querySelector('#menuInspectorSection [data-menu-add="SUBMENU"]').disabled);
    check(disabled, 'no permite un submenú en el nivel 3 (profundidad máxima 3)');
    await page.click(`${S}[data-menu-node="${sub2.key}"] [data-menu-remove]`);
    m = await menuField();
    check(!m.nodes.some((x) => x.key === sub2.key), 'eliminar un ítem');
    await page.click(`${S}[data-menu-add="GO_IDLE"]`);
    m = await menuField();
    const goIdle = m.nodes[m.nodes.length - 1];
    check(goIdle.type === 'GO_IDLE' && !goIdle.variable, 'agregar "Ir al inicio" (acción sin variable)');

    console.log('GUARDAR / ABRIR');
    const roundTrip = await page.evaluate(async () => {
      const P = window.JWPLCHMIProject;
      const s = P.serialize();
      await P.apply(JSON.parse(JSON.stringify(s)));
      await new Promise((r) => setTimeout(r, 300));
      const menu = window.JWPLCHMIEditor.getAllFields().find((f) => f.type === 'MENU');
      return JSON.stringify(menu?.nodes) === JSON.stringify(s.fields.find((f) => f.type === 'MENU')?.nodes);
    });
    check(roundTrip, 'el árbol se conserva al guardar y abrir');

    console.log('PARIDAD LIENZO / FIRMWARE');
    await page.evaluate(async (menu) => {
      const P = window.JWPLCHMIProject;
      const s = P.serialize();
      s.pages = [{ id: 0, name: 'Principal' }];
      s.fields = [menu];
      await P.apply(s);
      await new Promise((r) => setTimeout(r, 300));
    }, GATE_MENU);
    fs.writeFileSync(path.join(OUT, 'A15_8_Menu_Designer.jwhmi'),
      JSON.stringify(await page.evaluate(() => window.JWPLCHMIProject.serialize()), null, 2));
    const states = [['menu_root', null], ['menu_level2', 'ci'], ['menu_level3', 'h']];
    for (const [name, nodeKey] of states) {
      const ref = path.join(simDir, `${name}.rgb565`);
      if (!HAS_GPP) continue;
      if (!fs.existsSync(ref)) { check(false, `${name}: falta la referencia del firmware`); continue; }
      const raw = fs.readFileSync(ref);
      const firmware = Array.from({ length: raw.length / 2 }, (_, i) => raw.readUInt16LE(i * 2));
      const designer = await page.evaluate((key) => {
        const field = window.JWPLCHMIEditor.getAllFields().find((f) => f.type === 'MENU');
        window.JWPLCHMIMenu.selectNode(field, key, false);
        return Array.from(window.JWPLCHMIMenu.renderToArray(field));
      }, nodeKey);
      let diff = 0;
      let first = -1;
      for (let i = 0; i < firmware.length; i++) {
        if (firmware[i] !== designer[i]) { diff++; if (first < 0) first = i; }
      }
      check(diff === 0, `${name}: lienzo == framebuffer del firmware${diff ? ` (${diff} px distintos, primero en x=${first % 320} y=${Math.floor(first / 320)})` : ' (54400 px)'}`);
    }
    await page.evaluate(() => {
      const field = window.JWPLCHMIEditor.getAllFields().find((f) => f.type === 'MENU');
      window.JWPLCHMIMenu.selectNode(field, null, false);
      window.JWPLCHMIEditor.setSelectedFieldKeys([field.key]);
    });
    const canvas = await page.$('#displayCanvas');
    if (canvas) await canvas.screenshot({ path: path.join(OUT, 'designer_canvas.png') });

    console.log('HEADER');
    // Ítem "Ir al inicio" al final de la raíz: debe emitirse y compilar.
    await page.evaluate(() => {
      const E = window.JWPLCHMIEditor;
      const field = E.getAllFields().find((f) => f.type === 'MENU');
      field.nodes = [...field.nodes, { key: 'goidle', parent: null, type: 'GO_IDLE', label: 'Ir al inicio', variable: '', valueType: '', initial: 0, min: 0, max: 100, step: 1, decimals: 1, unit: '', falseText: 'OFF', trueText: 'ON' }];
      E.render();
    });
    const h = await header();
    check(/JWPLC_UIMenuGoIdle\(JWPLC_UI_MENU_ROOT, "Ir al inicio"\)/.test(h), 'el ítem "Ir al inicio" se emite como JWPLC_UIMenuGoIdle');
    fs.writeFileSync(path.join(OUT, 'JWPLC_HMI_Generated.h'), h);
    check(/JWPLC_Display\.setMenus\(\s*HMI_MENUS/.test(h), 'jwplcHMISetup() registra los menús');
    check(/float consigna = 45\.0f;/.test(h) && /int32_t retardo = 30;/.test(h) && /bool alarmaHabilitada = true;/.test(h), 'declara las variables con su tipo y valor inicial');
    check(/MENU_AJUSTES_ALARMAS = 6,/.test(h) && /JWPLC_UIMenuEditNum\(MENU_AJUSTES_ALARMAS, "Alta"/.test(h), 'índices de nodo simbólicos y padres antes que hijos');
    check(!/HMIFieldId[\s\S]*MENU_AJUSTES[\s\S]*\};/.test(h.slice(0, h.indexOf('// Variables HMI'))), 'el MENU no ocupa un HMIFieldId');
    check((h.match(/JWPLC_Display\.setMenus\(/g) || []).length === 1, 'setMenus aparece una sola vez (parche idempotente)');

    if (HAS_GPP && HAS_NM) {
      const cPath = path.join(OUT, 'menu_header.cpp');
      fs.writeFileSync(cPath, '#include "JWPLC_HMI_Generated.h"\nint main() { jwplcHMISetup(); jwplcUIUpdate(); return 0; }\n');
      let compiled = true;
      let log = '';
      try {
        execFileSync('g++', ['-std=gnu++17', '-fsyntax-only', '-Wall', `-I${OUT}`, `-I${STUB}`,
          `-I${path.join(LIBS, 'JWPLC_Display', 'src')}`, `-I${path.join(LIBS, 'JWPLC_TFT', 'src')}`, cPath], { stdio: 'pipe' });
      } catch (error) {
        compiled = false;
        log = String(error.stderr || error.message).split('\n').slice(0, 8).join('\n');
      }
      check(compiled, `el header compila contra los headers reales${compiled ? '' : `\n${log}`}`);
      const { librarySymbols } = require('./lib-symbols.cjs');
      const display = librarySymbols(path.join(LIBS, 'JWPLC_Display'));
      check(display.symbols.has('JWPLC_DisplayClass::setMenus') && display.symbols.has('JWPLC_UIMenuEditNum') && display.symbols.has('JWPLC_UIMenuGoIdle'),
        `setMenus y los constructores existen en JWPLC_Display (${display.mode === 'precompiled' ? 'libJWPLC_Display.a' : 'fuente'})`);
    } else {
      skip('sin g++/nm: no se compila el header ni se revisan los símbolos del .a');
    }

    check(errors.length === 0, `sin errores de consola${errors.length ? `: ${errors.slice(0, 3).join(' | ')}` : ''}`);
  } finally {
    await browser.close();
    server.close();
  }

  console.log(`\nCapturas, header y referencias en ${OUT}`);
  if (failures.length) {
    console.log(`${failures.length} verificaciones fallaron.`);
    process.exit(1);
  }
  console.log('Todas las verificaciones pasaron.');
}

main().catch((error) => { console.error(error); process.exit(1); });
