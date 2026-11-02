#!/usr/bin/env node
// Smoke test del proyecto «Runtime» (Alpha16, designer-runtime.js).
//
// Abre desktop.html en Chromium headless, elige «Runtime» en Proyecto · Pantalla
// y arranque, enlaza un BOOL a Q1 y un VALUE a B02 (tiempo) desde Datos >
// Variable del Runtime y verifica el header (hooks jwplcRuntimeHMI*), el
// guardado (.jwhmi con projectType) y la vuelta a Arduino.
//
// Uso (desde la raíz del repo):
//   node tools/jwplc-hmi-designer/tests/runtime-smoke.cjs [carpeta-salida]
// Requiere: playwright (local o global) con Chromium.
'use strict';

const fs = require('fs');
const os = require('os');
const path = require('path');
const http = require('http');
const { execFileSync } = require('child_process');

const POC = path.resolve(__dirname, '..', 'poc');
const OUT = path.resolve(process.argv[2] || fs.mkdtempSync(path.join(os.tmpdir(), 'jwplc-hmi-runtime-')));

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
  const header = () => page.evaluate(async () => {
    const E = window.JWPLCHMIEditor;
    E.setSelectedFieldKeys([]);
    E.render();
    document.getElementById('contractTab')?.click();
    await new Promise((r) => setTimeout(r, 400));
    window.JWPLCHMICodegen?.refresh?.();
    await new Promise((r) => setTimeout(r, 0));
    return document.getElementById('codeOutput').textContent;
  });
  const frame = () => new Promise((r) => setTimeout(r, 150));

  try {
    await page.goto(`http://127.0.0.1:${server.address().port}/desktop.html`);
    await page.waitForFunction(() => window.JWPLCHMIRuntime && window.JWPLCHMIBool && window.JWPLCHMIProject, null, { timeout: 20000 });

    console.log('TIPO DE PROYECTO');
    await page.evaluate(() => { window.JWPLCHMIEditor.setSelectedFieldKeys([]); window.JWPLCHMIEditor.render(); });
    await frame();
    check(await page.isVisible('#idleConfigSection [data-rt-type="runtime"]'), 'la tarjeta «Tipo de proyecto» está en Proyecto · Pantalla y arranque');
    await page.click('#idleConfigSection [data-rt-type="runtime"]');
    await frame();
    check(await page.evaluate(() => window.JWPLCHMIRuntime.get()) === 'runtime', 'clic en Runtime: projectType = runtime');
    check(!(await page.isVisible('#idleConfigSection [data-idle-card="wake"]')), 'Runtime: se ocultan las tarjetas de la IDLE (la RUN es del Runtime)');
    await page.locator('#idleConfigSection').screenshot({ path: path.join(OUT, 'runtime_tipo.png') });

    console.log('DATOS');
    await page.evaluate(() => window.JWPLCHMIBool.addBoolField());
    await page.click(`${W}[data-insp-main="data"]`);
    await frame();
    const sel = `${W}.insp-panel[data-insp-panel="data"] .jwplc-rt-select`;
    check(await page.isVisible(sel), 'BOOL: selector «Variable del Runtime» en Datos');
    check(await page.$eval(`${W}.insp-mode-chip`, (el) => el.textContent) === 'Runtime', 'BOOL: el chip del modo dice Runtime');
    await page.selectOption(sel, 'rt_q1');
    await frame();
    check(await page.evaluate(() => window.JWPLCHMIEditor.getSelectedField().variable) === 'rt_q1', 'BOOL: elegir Q1 deja la variable rt_q1');
    await page.locator('#textFieldControlsSection').screenshot({ path: path.join(OUT, 'runtime_datos_bool.png') });

    await page.evaluate(() => { window.JWPLCHMIEditor.setSelectedFieldKeys([]); window.JWPLCHMIEditor.addValueField(); });
    await page.click(`${W}[data-insp-main="data"]`);
    await frame();
    const options = await page.$$eval(`${sel} option`, (els) => els.map((e) => e.value));
    check(options.includes('rt_b02_t') && options.includes('rt_b02_c'), 'VALUE: el catálogo trae tiempo y cuenta de los bloques');
    await page.selectOption(sel, 'rt_b02_t');
    await frame();
    check(await page.evaluate(() => window.JWPLCHMIEditor.getSelectedField().variable) === 'rt_b02_t', 'VALUE: elegir B02 tiempo deja rt_b02_t');

    // El mismo Q1 en un segundo BOOL: nombre único con sufijo.
    await page.evaluate(() => { window.JWPLCHMIEditor.setSelectedFieldKeys([]); window.JWPLCHMIBool.addBoolField(); });
    await page.click(`${W}[data-insp-main="data"]`);
    await frame();
    await page.selectOption(sel, 'rt_q1');
    await frame();
    check(await page.evaluate(() => window.JWPLCHMIEditor.getSelectedField().variable) === 'rt_q1_2', 'Q1 en un segundo BOOL: rt_q1_2');

    console.log('HEADER');
    const h = await header();
    fs.writeFileSync(path.join(OUT, 'JWPLC_HMI_Generated_runtime.h'), h);
    check(h.includes('#include <JWPLC_Runtime.h>'), 'incluye JWPLC_Runtime.h');
    check(h.includes('extern "C" void jwplcRuntimeHMISetup(void)') && h.includes('JWPLC_Runtime.setHMI('), 'jwplcRuntimeHMISetup con setHMI');
    check(h.includes('extern "C" void jwplcRuntimeHMIUpdate(uint8_t page)'), 'jwplcRuntimeHMIUpdate(page)');
    check(/JWPLC_Display\.setBool\(\w+, JWPLC_Runtime\.output\(1\)\);[\s\S]*JWPLC_Display\.setBool\(\w+, JWPLC_Runtime\.output\(1\)\);/.test(h), 'los dos BOOL leen JWPLC_Runtime.output(1)');
    check(/JWPLC_Display\.setValue\(\w+, JWPLC_Runtime\.blockSeconds\(2\)\);/.test(h), 'VALUE lee JWPLC_Runtime.blockSeconds(2)');
    check(!/\bjwplcHMISetup\b|\bjwplcUIUpdate\b|^bool rt_q1/m.test(h), 'sin hooks ni variables del modo Arduino');

    console.log('GUARDADO');
    const saved = await page.evaluate(() => window.JWPLCHMIProject.serialize());
    check(saved.projectType === 'runtime', '.jwhmi guarda projectType = runtime');

    await page.evaluate(() => { window.JWPLCHMIEditor.setSelectedFieldKeys([]); window.JWPLCHMIEditor.render(); });
    await frame();
    await page.click('#idleConfigSection [data-rt-type="arduino"]');
    const back = await header();
    check(back.includes('void jwplcUIUpdate()') && !back.includes('JWPLC_Runtime'), 'vuelta a Arduino: header Arduino de siempre');
    check(/^bool rt_q1 = false;/m.test(back), 'en Arduino, rt_q1 es una variable más del sketch');

    check(errors.length === 0, `sin errores de consola${errors.length ? `: ${errors.slice(0, 3).join(' | ')}` : ''}`);
  } finally {
    await browser.close();
    server.close();
  }
  console.log(`\nCapturas en ${OUT}`);
  console.log(failures.length ? `RESULTADO: FAIL (${failures.length})` : 'RESULTADO: PASS');
  process.exit(failures.length ? 1 : 0);
}

main().catch((error) => { console.error(error); process.exit(1); });
