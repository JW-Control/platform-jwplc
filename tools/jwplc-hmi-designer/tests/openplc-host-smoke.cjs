#!/usr/bin/env node
// El Designer dentro de OpenPLC (contrato jwplc-hmi-host/1, puente 1.1). Simula la ventana que abre
// openplc-editor (src/main/modules/hmi-designer/index.ts + preload/hmi-designer-preload.ts): el
// «sketch» vinculado es la carpeta hmi/ del proyecto, en memoria. Verifica textos «OpenPLC: …» y
// «Actualizar HMI» estables, menú sin acciones de Arduino, «Actualizar HMI» y «Guardar» escriben
// header + JWPLC_HMI.jwhmi (sin guía), el selector «Variable PLC», «Nuevo» sin desvincular, la
// apertura del HMI guardado al iniciar y la guarda de cierre.
//
// Uso: node tools/jwplc-hmi-designer/tests/openplc-host-smoke.cjs [carpeta-salida]
'use strict';

const fs = require('fs');
const os = require('os');
const path = require('path');
const http = require('http');
const { execFileSync } = require('child_process');

const POC = path.resolve(__dirname, '..', 'poc');
const OUT = path.resolve(process.argv[2] || fs.mkdtempSync(path.join(os.tmpdir(), 'jwplc-hmi-openplc-')));
const HEADER = 'JWPLC_HMI_Generated.h';
const PROJECT = 'JWPLC_HMI.jwhmi';

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

// Doble de preload/hmi-designer-preload.ts de openplc-editor (mismos métodos) con hmi/ en memoria.
function openplcMock(seed) {
  const files = { ...(seed || {}) };
  const calls = [];
  const info = () => ({
    ok: true, name: 'HMIProbe', path: 'C:/OpenPLC/HMIProbe/hmi',
    headerExists: 'JWPLC_HMI_Generated.h' in files,
    projectFile: 'JWPLC_HMI.jwhmi',
    projectExists: 'JWPLC_HMI.jwhmi' in files
  });
  window.__openplc = { files, calls };
  window.JWPLCHMINative = {
    isNative: true,
    selectSketchDirectory: async () => { calls.push('select'); return info(); },
    getLinkedSketch: async () => info(),
    readProject: async () => (files['JWPLC_HMI.jwhmi']
      ? { ok: true, fileName: 'JWPLC_HMI.jwhmi', text: files['JWPLC_HMI.jwhmi'] }
      : { ok: false, missing: true, fileName: 'JWPLC_HMI.jwhmi' }),
    writeGeneratedHeader: async (text) => {
      calls.push('header');
      if (!text.startsWith('// Código generado por JWPLC HMI Designer')) return { ok: false, error: 'no es un header' };
      files['JWPLC_HMI_Generated.h'] = text;
      return { ok: true };
    },
    // El anfitrión ignora el nombre: siempre hmi/JWPLC_HMI.jwhmi.
    writeProject: async (fileName, text) => { calls.push(`project:${fileName}`); files['JWPLC_HMI.jwhmi'] = text; return { ok: true, fileName: 'JWPLC_HMI.jwhmi' }; },
    openSketchFolder: async () => { calls.push('open-folder'); return { ok: true }; },
    registerCloseGuard: (onCloseRequested) => { calls.push('close-guard'); window.__openplc.requestClose = onCloseRequested; },
    confirmClose: () => calls.push('close-confirmed'),
    registerSerialPortPicker: () => calls.push('serial-picker'),
    chooseSerialPort: () => calls.push('serial-chosen')
  };
  window.JWPLCHMIHost = {
    version: 1,
    mode: 'openplc',
    getContext: async () => ({ projectName: 'HMIProbe', device: 'JWPLC BASIC [2.0.0]', display: { width: 320, height: 170 } }),
    listVariables: async () => [
      { name: 'RELE', scope: 'global', iecType: 'BOOL', address: '%QX0.0', hmiTypes: ['bool'], access: 'R' },
      { name: 'CONTADOR', scope: 'global', iecType: 'INT', address: '%MW0', hmiTypes: ['float', 'int32_t'], access: 'RW' },
      { name: 'LIMITE', scope: 'global', iecType: 'INT', address: '%MW1', hmiTypes: ['float', 'int32_t'], access: 'RW' }
    ],
    loadProject: async () => (files['JWPLC_HMI.jwhmi'] ? JSON.parse(files['JWPLC_HMI.jwhmi']) : null)
  };
}

async function openDesigner(browser, port, seed, dialogs, answers) {
  const page = await browser.newPage({ viewport: { width: 1600, height: 1000 } });
  const errors = [];
  page.on('pageerror', (e) => errors.push(e.message));
  page.on('console', (m) => { if (m.type() === 'error') errors.push(m.text()); });
  page.on('dialog', (d) => { dialogs.push(`NATIVO: ${d.message()}`); d.dismiss(); });
  // Avisos del Designer (designer-dialog.js): se responden con el botón que diga `answers`.
  await page.exposeFunction('__onAppDialog', (message) => { dialogs.push(message); return answers.shift() || 'ok'; });
  await page.addInitScript(() => {
    new MutationObserver(async () => {
      const box = document.querySelector('.jw-dialog:not([data-seen])');
      if (!box) return;
      box.dataset.seen = '1';
      const id = await window.__onAppDialog(box.querySelector('.jw-dialog-body').firstChild.textContent);
      (box.querySelector(`[data-jw-dialog="${id}"]`) || box.querySelector('[data-jw-dialog="ok"]')).click();
    }).observe(document, { childList: true, subtree: true });
  });
  await page.addInitScript(openplcMock, seed);
  await page.goto(`http://127.0.0.1:${port}/desktop.html?host=openplc`);
  await page.waitForFunction(() => window.JWPLCHMISketchMenu && window.JWPLCHMIProject && window.JWPLCHMIEditor && window.JWPLCHMISaveFlow, null, { timeout: 20000 });
  await page.waitForTimeout(4200); // los módulos terminan de reescribir los botones
  return { page, errors };
}

const labels = (page) => page.evaluate(() => ({
  link: document.getElementById('linkSketchButton').textContent.trim(),
  status: document.getElementById('sketchLinkStatus')?.textContent.trim() || '',
  update: document.getElementById('updateHmiButton').textContent.trim(),
  updateDisabled: document.getElementById('updateHmiButton').disabled
}));

async function main() {
  fs.mkdirSync(OUT, { recursive: true });
  const server = await serve();
  const port = server.address().port;
  const { chromium } = loadPlaywright();
  const browser = await chromium.launch({ executablePath: chromiumPath() });
  const dialogs = [];
  const answers = [];
  let saved = null;

  try {
    console.log('PROYECTO OPENPLC SIN HMI');
    const a = await openDesigner(browser, port, {}, dialogs, answers);
    const samples = [];
    for (let i = 0; i < 4; i++) { samples.push(await labels(a.page)); await a.page.waitForTimeout(400); }
    const first = samples[0];
    check(samples.every((s) => JSON.stringify(s) === JSON.stringify(first)), `textos estables (sin parpadeo entre módulos): ${JSON.stringify(first)}`);
    check(first.link === 'OpenPLC: HMIProbe' && /^OpenPLC: HMIProbe · hmi\//.test(first.status), 'botón y barra de estado nombran el proyecto OpenPLC');
    check(first.update === 'Actualizar HMI' && !first.updateDisabled, '«Actualizar HMI» habilitado');
    const registered = await a.page.evaluate(() => window.__openplc.calls.slice());
    check(registered.includes('close-guard') && registered.includes('serial-picker'), `registra la guarda de cierre y el selector de puertos (${registered.join(',')})`);

    await a.page.click('#linkSketchButton');
    await a.page.waitForSelector('.sketch-menu');
    const items = await a.page.$$eval('.sketch-menu [data-sketch-action]', (els) => els.map((e) => `${e.dataset.sketchAction}${e.disabled ? '(off)' : ''}`).join(','));
    check(items === 'open-project(off),open-folder', `menú sin acciones de Arduino (${items})`);
    const menuEl = await a.page.$('.sketch-menu');
    if (menuEl) await menuEl.screenshot({ path: path.join(OUT, 'openplc_menu.png') });
    await a.page.click('[data-sketch-action="open-folder"]');
    await a.page.waitForTimeout(200);
    check((await a.page.evaluate(() => window.__openplc.calls)).includes('open-folder'), '«Abrir carpeta hmi/» llama a openSketchFolder');

    console.log('VARIABLE PLC + ACTUALIZAR HMI');
    const fieldCount = () => a.page.evaluate(() => window.JWPLCHMIEditor.getAllFields().length);
    const initial = await fieldCount(); // el diseño de partida del Designer
    await a.page.evaluate(() => window.JWPLCHMIEditor.addValueField());
    await a.page.waitForTimeout(300);
    await a.page.click('#fieldInspectorV2 [data-insp-main="data"]');
    await a.page.waitForTimeout(300);
    const select = '.insp-panel[data-insp-panel="data"] .jwplc-plc-select';
    await a.page.selectOption(select, 'CONTADOR');
    await a.page.waitForTimeout(300);
    dialogs.length = 0;
    await a.page.click('#updateHmiButton');
    await a.page.waitForTimeout(1500);
    const written = await a.page.evaluate(() => ({ files: { ...window.__openplc.files }, calls: window.__openplc.calls.slice(), toast: document.getElementById('a15SketchToast')?.textContent || '' }));
    const project = JSON.parse(written.files[PROJECT] || '{}');
    const value = (project.fields || []).find((f) => f.type === 'VALUE');
    check(dialogs.length === 0, `sin confirmaciones con hmi/ vacío (${JSON.stringify(dialogs)})`);
    check(/^\/\/ Código generado por JWPLC HMI Designer/.test(written.files[HEADER] || '') && (written.files[HEADER] || '').includes('contador'), 'escribe hmi/JWPLC_HMI_Generated.h con la variable enlazada');
    check(value?.plcBinding?.var === 'CONTADOR' && project.projectName === 'HMIProbe', `escribe hmi/JWPLC_HMI.jwhmi con plcBinding (${JSON.stringify(value?.plcBinding)})`);
    check(!Object.keys(written.files).some((n) => /LEEME/.test(n)) && !/LEEME|vuelve a generar/.test(written.toast), `sin guía JWPLC_HMI_LEEME.md ni aviso de app vieja («${written.toast}»)`);
    await a.page.screenshot({ path: path.join(OUT, 'openplc_actualizar.png') });

    console.log('GUARDAR Y NUEVO');
    await a.page.evaluate(() => window.JWPLCHMIEditor.addTextField());
    const headersBefore = written.calls.filter((c) => c === 'header').length;
    dialogs.length = 0;
    await a.page.keyboard.press('Control+s');
    await a.page.waitForTimeout(1500);
    const afterSave = await a.page.evaluate(() => ({ calls: window.__openplc.calls.slice(), fields: JSON.parse(window.__openplc.files['JWPLC_HMI.jwhmi']).fields.length }));
    check(afterSave.calls.filter((c) => c === 'header').length === headersBefore + 1 && afterSave.fields === initial + 2 && dialogs.length === 0,
      `Ctrl+S = «Actualizar HMI» sin preguntar (${afterSave.fields} objetos, ${JSON.stringify(dialogs)})`);
    await a.page.click('#newProjectButton');
    await a.page.waitForTimeout(1200);
    const afterNew = await labels(a.page);
    check(afterNew.link === 'OpenPLC: HMIProbe' && !afterNew.updateDisabled && (await fieldCount()) === initial,
      `«Nuevo» vacía el diseño y sigue en el proyecto OpenPLC (${JSON.stringify(afterNew)})`);

    console.log('CERRAR LA VENTANA');
    await a.page.evaluate(() => window.JWPLCHMIEditor.addTextField());
    await a.page.waitForTimeout(800);
    dialogs.length = 0;
    answers.push('discard');
    await a.page.evaluate(() => window.__openplc.requestClose());
    await a.page.waitForTimeout(800);
    const closed = await a.page.evaluate(() => window.__openplc.calls.slice(-1)[0]);
    check(dialogs.length === 1 && /cerrar/.test(dialogs[0]) && closed === 'close-confirmed', `con cambios pregunta antes de cerrar y luego confirma (${JSON.stringify(dialogs)} → ${closed})`);
    check(a.errors.length === 0, `sin errores JS (${a.errors.join(' | ') || 'ninguno'})`);
    saved = { [HEADER]: written.files[HEADER], [PROJECT]: written.files[PROJECT] };
    await a.page.close();

    console.log('REABRIR EL DESIGNER');
    dialogs.length = 0;
    const b = await openDesigner(browser, port, saved, dialogs, answers);
    const reopened = await b.page.evaluate(() => window.JWPLCHMIEditor.getAllFields().map((f) => ({ type: f.type, plc: f.plcBinding?.var || null })));
    check(reopened.length === initial + 1 && reopened.filter((f) => f.plc === 'CONTADOR').length === 1, `abre hmi/JWPLC_HMI.jwhmi al iniciar (${JSON.stringify(reopened)})`);
    check(/proyecto OpenPLC «HMIProbe»/.test(await b.page.textContent('#a11NativeProjectToast')), 'el aviso de carga nombra el proyecto OpenPLC');
    await b.page.evaluate(() => window.JWPLCHMIEditor.addTextField());
    await b.page.click('#updateHmiButton');
    await b.page.waitForTimeout(1500);
    check(dialogs.length === 0, `el HMI abierto del proyecto se reescribe sin preguntar (${JSON.stringify(dialogs)})`);
    // Sin cambios: cierra sin preguntar.
    await b.page.waitForTimeout(800);
    await b.page.evaluate(() => window.__openplc.requestClose());
    await b.page.waitForTimeout(500);
    check(dialogs.length === 0 && (await b.page.evaluate(() => window.__openplc.calls.slice(-1)[0])) === 'close-confirmed', 'sin cambios cierra sin preguntar');
    check(b.errors.length === 0, `sin errores JS (${b.errors.join(' | ') || 'ninguno'})`);
  } finally {
    await browser.close();
    server.close();
  }

  console.log(`\nCapturas en ${OUT}`);
  if (failures.length) { console.log(`${failures.length} verificaciones fallaron.`); process.exit(1); }
  console.log('Todas las verificaciones pasaron.');
}

main().catch((e) => { console.error(e); process.exit(1); });
