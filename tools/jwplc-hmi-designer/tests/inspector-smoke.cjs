#!/usr/bin/env node
// Smoke test del inspector Propuesta 2 de TEXT / VALUE / BOOL (A15-7B/7C/7D).
//
// Abre desktop.html en Chromium headless, opera el inspector como un usuario
// (clics en tarjetas, chips, steppers, paleta, pestañas de página) y verifica
// que cada cambio llegue al modelo y al header generado, que undo funcione y
// que no haya errores de consola. Guarda capturas de cada pestaña.
//
// Uso (desde la raíz del repo):
//   node tools/jwplc-hmi-designer/tests/inspector-smoke.cjs [carpeta-salida]
// Requiere: playwright (local o global) con Chromium. Sin g++ ni python.
'use strict';

const fs = require('fs');
const os = require('os');
const path = require('path');
const http = require('http');
const { execFileSync } = require('child_process');

const POC = path.resolve(__dirname, '..', 'poc');
const OUT = path.resolve(process.argv[2] || fs.mkdtempSync(path.join(os.tmpdir(), 'jwplc-hmi-inspector-')));

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

const failures = [];
function check(condition, message) {
  if (condition) console.log(`  PASS ${message}`);
  else { console.log(`  FAIL ${message}`); failures.push(message); }
}

async function main() {
  fs.mkdirSync(OUT, { recursive: true });
  const server = await serve();
  const { chromium } = loadPlaywright();
  const browser = await chromium.launch({ executablePath: chromiumPath() });
  const page = await browser.newPage({ viewport: { width: 1600, height: 1400 } });
  const errors = [];
  page.on('pageerror', (e) => errors.push(e.message));
  page.on('console', (m) => { if (m.type() === 'error') errors.push(m.text()); });

  const W = '#fieldInspectorV2 ';
  const selected = () => page.evaluate(() => JSON.parse(JSON.stringify(window.JWPLCHMIEditor.getSelectedField())));
  const fieldOf = (type) => page.evaluate((t) => JSON.parse(JSON.stringify(window.JWPLCHMIEditor.getAllFields().find((f) => f.type === t))), type);
  const sub = async (name) => { await page.click(`${W}[data-insp-main="design"]`); await page.click(`${W}[data-insp-sub="${name}"]`); };
  const fill = async (selector, text) => { await page.fill(W + selector, text); await page.press(W + selector, 'Tab'); };
  const shot = async (name) => {
    const panel = await page.$('#textFieldControlsSection');
    if (panel) await panel.screenshot({ path: path.join(OUT, `${name}.png`) });
  };
  const header = () => page.evaluate(async () => {
    const E = window.JWPLCHMIEditor;
    E.setSelectedFieldKeys([]);
    E.render();
    document.getElementById('contractTab')?.click();
    await new Promise((r) => setTimeout(r, 400));
    return document.getElementById('codeOutput').textContent;
  });

  try {
    await page.goto(`http://127.0.0.1:${server.address().port}/desktop.html`);
    await page.waitForFunction(() => window.JWPLCHMIFieldInspector && window.JWPLCHMIBool && window.JWPLCHMIProject, null, { timeout: 20000 });

    console.log('VALUE');
    await page.evaluate(() => window.JWPLCHMIEditor.addValueField());
    await page.waitForTimeout(300);
    const oldVisible = await page.evaluate(() => [...document.querySelectorAll('#textFieldControlsSection > details')].filter((d) => d.offsetParent).length);
    check(oldVisible === 0, 'los <details> viejos quedan ocultos');
    await sub('shape');
    await page.click(`${W}[data-set="layout"][data-value="STACKED"]`);
    await page.click(`${W}[data-set="valueSize"][data-value="3"]`);
    await page.click(`${W}[data-set="frame"][data-value="1"]`);
    await shot('value_forma');
    await sub('content');
    await page.click(`${W}[data-set="decimalDigits"][data-value="2"]`);
    await page.click(`${W}[data-stepper="integerDigits"] [data-step="1"]`);
    await page.click(`${W}[data-toggle="signedValue"]`);
    await page.click(`${W}[data-set="align"][data-value="CENTER"]`);
    await fill('[data-proxy="fieldLabel"]', 'Presion');
    await fill('[data-proxy="fieldUnit"]', 'bar');
    await shot('value_contenido');
    await sub('appearance');
    await page.click(`${W}[data-color-key="value"][data-color="${0xF800}"]`);
    await page.click(`${W}[data-color-key="frame"][data-color="${0xFFE0}"]`);
    await shot('value_apariencia');
    await page.click(`${W}[data-insp-main="data"]`);
    await fill('[data-only="VALUE"] [data-proxy="fieldPreview"]', '-12.34');
    await fill('[data-proxy="fieldVariable"]', 'presion');
    await shot('value_datos');
    let v = await selected();
    check(v.layout === 'STACKED' && v.valueSize === 3 && v.frame === true, 'Forma: layout, tamaño y marco');
    check(v.decimalDigits === 2 && v.integerDigits === 4 && v.signedValue === true, 'Contenido: formato numérico');
    check(v.align === 'CENTER' && v.label === 'Presion' && v.unit === 'bar', 'Contenido: alineación, etiqueta y unidad');
    check(v.valueColor === 0xF800 && v.frameColor === 0xFFE0, 'Apariencia: colores desde la paleta');
    check(v.preview === '-12.34' && v.variable === 'presion', 'Datos: valor de prueba y variable');
    await page.evaluate(() => window.JWPLCHMIEditor.undo());
    await page.waitForTimeout(200);
    check((await fieldOf('VALUE')).variable !== 'presion', 'undo revierte el último cambio del inspector');
    await page.evaluate(() => window.JWPLCHMIEditor.redo());
    await page.click(`${W}[data-insp-main="tech"]`);
    await shot('value_tecnico');

    console.log('TEXT');
    await page.evaluate(() => window.JWPLCHMIEditor.addTextField());
    await page.waitForTimeout(300);
    await sub('content');
    await page.click(`${W}[data-stepper="capacity"] [data-step="-1"]`);
    await page.click(`${W}[data-stepper="capacity"] [data-step="-1"]`);
    await shot('text_contenido');
    await page.click(`${W}[data-insp-main="data"]`);
    await fill('[data-text-preview]', 'ABCDEFGHIJKLMN');
    const t = await selected();
    check(t.capacity === 10, 'capacidad con stepper');
    check(t.preview.length <= 10, 'valor de prueba recortado a la capacidad');

    console.log('BOOL');
    await page.evaluate(() => window.JWPLCHMIBool.addBoolField());
    await page.waitForTimeout(300);
    await sub('content');
    await page.click(`${W}[data-bool-preset="PARADO|MARCHA"]`);
    await shot('bool_contenido');
    await page.click(`${W}[data-insp-main="data"]`);
    await page.click(`${W}[data-bool-preview="1"]`);
    const bo = await selected();
    check(bo.falseText === 'PARADO' && bo.trueText === 'MARCHA', 'preset de textos');
    check(bo.boolValue === true && bo.preview === 'MARCHA', 'estado de prueba con interruptor');

    await page.evaluate(() => window.JWPLCHMIEditor.addPage());
    await page.evaluate(() => {
      const E = window.JWPLCHMIEditor;
      E.setActivePage(0);
      E.setSelectedFieldKeys([E.getAllFields().find((f) => f.type === 'BOOL').key]);
    });
    await sub('placement');
    await page.click(`${W}[data-page-id="1"]`);
    check((await fieldOf('BOOL')).page === 1, 'Ubicación: mover a otra página');

    console.log('HEADER');
    const h = await header();
    fs.writeFileSync(path.join(OUT, 'JWPLC_HMI_Generated.h'), h);
    check(/JWPLC_UI_LAYOUT_STACKED/.test(h) && /JWPLC_UI_ALIGN_CENTER/.test(h), 'layout y alineación emitidos');
    check(/JWPLC_UIValueFormat\(\s*4,\s*2,\s*true/.test(h), 'formato numérico emitido');
    check(/0xF800/.test(h) && /"PARADO",\s*"MARCHA"/.test(h), 'color y textos BOOL emitidos');

    console.log('PROYECTO VIEJO');
    const bg = await page.evaluate(async () => {
      const P = window.JWPLCHMIProject;
      const s = P.serialize();
      s.fields.find((f) => f.type === 'VALUE').backgroundColor = 'TRANSPARENT';
      await P.apply(s);
      await new Promise((r) => setTimeout(r, 400));
      const E = window.JWPLCHMIEditor;
      E.setActivePage(0);
      E.setSelectedFieldKeys([E.getAllFields().find((f) => f.type === 'VALUE').key]);
      await new Promise((r) => setTimeout(r, 300));
      return E.getAllFields().find((f) => f.type === 'VALUE').backgroundColor;
    });
    check(bg === 0, 'fondo TRANSPARENT de un campo se normaliza a 0x0000');
    const h2 = await header();
    check(!/0xTRANSPARENT|TRANSPARENT\)/.test(h2), 'el header no contiene TRANSPARENT como color');

    console.log('BAR');
    const barState = await page.evaluate(async () => {
      window.JWPLCHMIEditor.addBarField();
      await new Promise((r) => setTimeout(r, 400));
      return {
        v2: !document.getElementById('fieldInspectorV2').hidden,
        bar: document.getElementById('barInspectorWrap')?.offsetParent !== null
      };
    });
    check(!barState.v2 && barState.bar, 'BAR conserva su propio inspector');

    check(errors.length === 0, `sin errores de consola${errors.length ? `: ${errors.join(' | ')}` : ''}`);
  } finally {
    await browser.close();
    server.close();
  }

  console.log(`\nCapturas y header en ${OUT}`);
  if (failures.length) {
    console.log(`${failures.length} verificaciones fallaron.`);
    process.exit(1);
  }
  console.log('Todas las verificaciones pasaron.');
}

main().catch((error) => { console.error(error); process.exit(1); });
