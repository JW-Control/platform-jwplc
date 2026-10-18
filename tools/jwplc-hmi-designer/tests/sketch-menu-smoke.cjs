#!/usr/bin/env node
// Flujo con el sketch Arduino (Alpha15): «Escribir en sketch», menú «Sketch: nombre ▾»
// y apertura del .jwhmi del sketch al vincular. Simula la app de escritorio con
// un window.JWPLCHMINative en memoria (mismos métodos que electron/preload.cjs).
//
// Uso: node tools/jwplc-hmi-designer/tests/sketch-menu-smoke.cjs [carpeta-salida]
'use strict';

const fs = require('fs');
const os = require('os');
const path = require('path');
const http = require('http');
const { execFileSync } = require('child_process');

const POC = path.resolve(__dirname, '..', 'poc');
const OUT = path.resolve(process.argv[2] || fs.mkdtempSync(path.join(os.tmpdir(), 'jwplc-hmi-sketch-')));

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


// Doble de electron/preload.cjs con dos sketches en memoria.
function nativeMock() {
  const sketches = {
    'C:/Arduino/demo': { name: 'demo', files: { 'demo.ino': '', 'JWPLC_HMI_Generated.h': '// viejo' } },
    'C:/Arduino/otro': { name: 'otro', files: { 'otro.ino': '' } }
  };
  let linked = 'C:/Arduino/demo';
  let nextPick = 'C:/Arduino/otro';
  const calls = [];
  const info = () => {
    const s = sketches[linked];
    if (!s) return { ok: false };
    return {
      ok: true, name: s.name, path: linked,
      headerExists: 'JWPLC_HMI_Generated.h' in s.files,
      projectFile: `${s.name}.jwhmi`,
      projectExists: `${s.name}.jwhmi` in s.files
    };
  };
  window.__sketchMock = { sketches, calls, setNextPick: (p) => { nextPick = p; } };
  window.JWPLCHMINative = {
    isNative: true,
    selectSketchDirectory: async () => { calls.push('select'); if (!nextPick) return { ok: false, canceled: true }; linked = nextPick; return info(); },
    getLinkedSketch: async () => info(),
    writeGeneratedHeader: async (text, overwrite) => { calls.push(`header:${overwrite}`); sketches[linked].files['JWPLC_HMI_Generated.h'] = text; return { ok: true }; },
    writeProject: async (fileName, text, overwrite) => { calls.push(`project:${fileName}:${overwrite}`); sketches[linked].files[fileName] = text; return { ok: true }; },
    readProject: async () => { const i = info(); return i.projectExists ? { ok: true, fileName: i.projectFile, text: sketches[linked].files[i.projectFile] } : { ok: false, missing: true, fileName: i.projectFile }; },
    openSketchIno: async () => { calls.push('open-ino'); return { ok: true }; },
    openSketchFolder: async () => { calls.push('open-folder'); return { ok: true }; }
  };
}

async function main() {
  fs.mkdirSync(OUT, { recursive: true });
  const server = await serve();
  const { chromium } = loadPlaywright();
  const browser = await chromium.launch({ executablePath: chromiumPath() });
  const page = await browser.newPage({ viewport: { width: 1600, height: 1000 } });
  const errors = [];
  page.on('pageerror', (e) => errors.push(e.message));
  const dialogs = [];
  page.on('dialog', (d) => { dialogs.push(d.message()); d.accept(); });
  await page.addInitScript(nativeMock);

  try {
    await page.goto(`http://127.0.0.1:${server.address().port}/desktop.html`);
    await page.waitForFunction(() => window.JWPLCHMISketchMenu && window.JWPLCHMIProject && window.JWPLCHMIEditor, null, { timeout: 20000 });
    await page.waitForTimeout(4200); // los módulos terminan de reescribir los botones

    console.log('BOTONES');
    const ui = await page.evaluate(() => ({
      generateHidden: getComputedStyle(document.getElementById('generateButton')).display === 'none',
      update: document.getElementById('updateHmiButton').textContent.trim(),
      updateDisabled: document.getElementById('updateHmiButton').disabled,
      link: document.getElementById('linkSketchButton').textContent.trim(),
      menuClass: document.getElementById('linkSketchButton').classList.contains('sketch-menu-button')
    }));
    check(ui.generateHidden, '«Generar C++» oculto (el panel «Código generado» ya muestra el header)');
    check(ui.update === 'Escribir en sketch' && !ui.updateDisabled, `«Escribir en sketch» habilitado con sketch vinculado (${ui.update})`);
    check(/demo/.test(ui.link) && ui.menuClass, `«Sketch: demo ▾» abre menú (${ui.link})`);
    await page.locator('.tb-group-start, #updateHmiButton').first().scrollIntoViewIfNeeded().catch(() => {});
    const bar = await page.$('.toolbar, header, #toolbar');
    if (bar) await bar.screenshot({ path: path.join(OUT, 'toolbar.png') }).catch(() => {});

    console.log('ESCRIBIR EN SKETCH');
    await page.evaluate(() => { window.JWPLCHMIEditor.addValueField(); });
    const fieldsBefore = await page.evaluate(() => window.JWPLCHMIEditor.getAllFields().length);
    dialogs.length = 0;
    await page.click('#updateHmiButton');
    await page.waitForTimeout(1200);
    const written = await page.evaluate(() => {
      const f = window.__sketchMock.sketches['C:/Arduino/demo'].files;
      return { header: f['JWPLC_HMI_Generated.h'], project: f['demo.jwhmi'], calls: window.__sketchMock.calls.slice() };
    });
    check(dialogs.length === 1 && /JWPLC_HMI_Generated\.h/.test(dialogs[0]) && !/demo\.jwhmi/.test(dialogs[0]), `una sola confirmación, sólo por lo que se reemplaza (${JSON.stringify(dialogs)})`);
    check(/^\/\/ Código generado por JWPLC HMI Designer/.test(written.header || '') && /#pragma once/.test(written.header || ''), 'escribe un JWPLC_HMI_Generated.h válido');
    const savedProject = JSON.parse(written.project || '{}');
    check(savedProject.projectName === 'demo' && savedProject.fields?.length === fieldsBefore, `escribe demo.jwhmi con el diseño (${savedProject.fields?.length} objetos)`);

    console.log('MENÚ DEL SKETCH');
    await page.click('#linkSketchButton');
    await page.waitForSelector('.sketch-menu');
    const items = await page.$$eval('.sketch-menu [data-sketch-action]', (els) => els.map((e) => `${e.dataset.sketchAction}${e.disabled ? '(off)' : ''}`).join(','));
    check(items === 'open-project,open-ino,open-folder,change', `Abrir HMI · Arduino IDE · Carpeta · Cambiar (${items})`);
    const menuEl = await page.$('.sketch-menu');
    if (menuEl) await menuEl.screenshot({ path: path.join(OUT, 'sketch_menu.png') });
    await page.click('[data-sketch-action="open-ino"]');
    await page.waitForTimeout(200);
    check((await page.evaluate(() => window.__sketchMock.calls)).includes('open-ino'), 'Abrir en Arduino IDE llama a openSketchIno');

    // Cambiar el diseño y volver al HMI guardado del sketch.
    await page.evaluate(() => { window.JWPLCHMIEditor.addTextField(); window.JWPLCHMIEditor.addTextField(); });
    dialogs.length = 0;
    await page.click('#linkSketchButton');
    await page.click('[data-sketch-action="open-project"]');
    await page.waitForTimeout(900);
    const reopened = await page.evaluate(() => window.JWPLCHMIEditor.getAllFields().length);
    check(dialogs.length === 1 && reopened === fieldsBefore, `Abrir HMI del sketch pide confirmar y restaura el diseño escrito (${reopened}/${fieldsBefore})`);

    console.log('CAMBIAR SKETCH');
    // «otro» no tiene .jwhmi: no pregunta.
    dialogs.length = 0;
    await page.click('#linkSketchButton');
    await page.click('[data-sketch-action="change"]');
    await page.waitForTimeout(500);
    const otro = { dialogs: dialogs.slice(), label: await page.textContent('#linkSketchButton'), calls: await page.evaluate(() => window.__sketchMock.calls.slice(-3)) };
    check(otro.dialogs.length === 0 && /otro/.test(otro.label), `vincular un sketch sin .jwhmi no pregunta nada (${JSON.stringify(otro)})`);
    // Volver a «demo», que sí tiene .jwhmi: pregunta si abrirlo.
    await page.evaluate(() => { window.JWPLCHMIEditor.addTextField(); window.__sketchMock.setNextPick('C:/Arduino/demo'); });
    await page.click('#linkSketchButton');
    await page.click('[data-sketch-action="change"]');
    await page.waitForTimeout(900);
    check(dialogs.length === 1 && /ya tiene un HMI guardado/.test(dialogs[0]) && (await page.evaluate(() => window.JWPLCHMIEditor.getAllFields().length)) === fieldsBefore,
      'al vincular un sketch con .jwhmi pregunta y lo abre');

    check(errors.length === 0, `sin errores JS (${errors.join(' | ') || 'ninguno'})`);
  } finally {
    await browser.close();
    server.close();
  }

  console.log(`\nCapturas en ${OUT}`);
  if (failures.length) { console.log(`${failures.length} verificaciones fallaron.`); process.exit(1); }
  console.log('Todas las verificaciones pasaron.');
}

main().catch((e) => { console.error(e); process.exit(1); });
