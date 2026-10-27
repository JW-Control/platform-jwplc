#!/usr/bin/env node
// Vista de enlaces del Designer en la pestaña de OpenPLC (contrato jwplc-hmi-host/1.4,
// desktop.html?host=openplc&view=enlaces). Simula el preload de openplc-editor y verifica: sin
// herramientas de dibujo ni «Nuevo/Abrir/Actualizar HMI», inspector sólo con «Datos», arrastrar,
// doble clic y Supr no cambian el diseño, un clic en el lienzo selecciona, el MENU sólo muestra la
// variable del nodo, el botón «Abrir HMI Designer para diseñar» llama a openEditor y con la ventana abierta la vista queda en
// sólo lectura.
//
// Uso: node tools/jwplc-hmi-designer/tests/openplc-bindings-smoke.cjs [carpeta-salida]
'use strict';

const fs = require('fs');
const os = require('os');
const path = require('path');
const http = require('http');
const { execFileSync } = require('child_process');

const POC = path.resolve(__dirname, '..', 'poc');
const OUT = path.resolve(process.argv[2] || fs.mkdtempSync(path.join(os.tmpdir(), 'jwplc-hmi-bindings-')));

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

// Doble de preload/hmi-designer-preload.ts de openplc-editor.
function openplcMock() {
  const calls = [];
  window.__openplc = { calls };
  window.JWPLCHMINative = {
    isNative: true,
    getLinkedSketch: async () => ({ ok: true, name: 'HMIProbe', path: 'C:/OpenPLC/HMIProbe/hmi', headerExists: false, projectFile: 'JWPLC_HMI.jwhmi', projectExists: false }),
    selectSketchDirectory: async () => ({ ok: true }),
    readProject: async () => ({ ok: false, missing: true, fileName: 'JWPLC_HMI.jwhmi' }),
    writeGeneratedHeader: async () => ({ ok: true }),
    writeProject: async () => ({ ok: true, fileName: 'JWPLC_HMI.jwhmi' }),
    openSketchFolder: async () => ({ ok: true }),
    registerCloseGuard: () => calls.push('close-guard'),
    confirmClose: () => calls.push('close-confirmed'),
    registerSerialPortPicker: () => calls.push('serial-picker'),
    chooseSerialPort: () => {}
  };
  window.JWPLCHMIHost = {
    version: 1,
    mode: 'openplc',
    getContext: async () => ({ projectName: 'HMIProbe', device: 'JWPLC BASIC [2.0.0]', display: { width: 320, height: 170 } }),
    listVariables: async () => [
      { name: 'RELE', scope: 'program', pou: 'main', iecType: 'BOOL', address: '%QX0.0', hmiTypes: ['bool'], access: 'R' },
      { name: 'CONTADOR', scope: 'program', pou: 'main', iecType: 'INT', address: null, hmiTypes: ['float', 'int32_t'], access: 'RW' }
    ],
    loadProject: async () => null,
    onVariablesChanged: () => {},
    setDirty: (dirty) => calls.push(`dirty:${dirty}`),
    openEditor: async () => { calls.push('open-editor'); return { ok: true }; },
    onEditorWindow: (callback) => { window.__openplc.editorWindow = callback; }
  };
}

const node = (key, parent, type, label, extra = {}) => ({
  key, parent, type, label, variable: '', valueType: '', initial: 0, min: 0, max: 100, step: 1,
  decimals: 1, unit: '', falseText: 'OFF', trueText: 'ON', ...extra
});
const MENU = {
  key: 'menu-1', type: 'MENU', name: 'Ajustes', id: 'MENU_AJUSTES', title: 'Ajustes',
  x: 10, y: 90, width: 300, height: 76, page: 0, textSize: 1, frame: true, layerOrder: 0,
  colors: { text: 0xFFFF, background: 0x0000, value: 0x07FF, selectText: 0x0000, selectBackground: 0x07FF, editText: 0x0000, editBackground: 0xFD20, title: 0x0000, titleBackground: 0xFFFF, frame: 0xFFFF },
  nodes: [
    node('c', null, 'EDIT_NUM', 'Consigna', { variable: 'consigna', valueType: 'float', initial: 45 }),
    node('b', null, 'EDIT_BOOL', 'Bomba', { variable: 'bomba', valueType: 'bool' })
  ]
};

async function main() {
  fs.mkdirSync(OUT, { recursive: true });
  const server = await serve();
  const { chromium } = loadPlaywright();
  const browser = await chromium.launch({ executablePath: chromiumPath() });
  try {
    const page = await browser.newPage({ viewport: { width: 1300, height: 820 } });
    const errors = [];
    page.on('pageerror', (e) => errors.push(e.message));
    await page.addInitScript(openplcMock);
    await page.goto(`http://127.0.0.1:${server.address().port}/desktop.html?host=openplc&view=enlaces`);
    await page.waitForFunction(() => window.JWPLCHMIEditor && window.JWPLCHMIProject && window.JWPLCHMIBindingsView, null, { timeout: 20000 });
    await page.waitForTimeout(4500);

    console.log('VISTA RECORTADA');
    const hidden = await page.evaluate(() => {
      const shown = (sel) => { const el = document.querySelector(sel); return Boolean(el && el.offsetParent); };
      return {
        toolbar: shown('.canvas-toolbar'), nuevo: shown('#newProjectButton'), abrir: shown('#openProjectButton'),
        actualizar: shown('#updateHmiButton'), live: shown('#liveConnectButton'),
        codigo: document.querySelector('.code-panel').getBoundingClientRect().height > 0,
        addPage: shown('.page-tabs .add-tab'), edit: shown('#jwplcEditDesignButton'), objetos: shown('#objectsNavBlock'),
        preview: shown('#sidebarPreviewBlock')
      };
    });
    check(!hidden.toolbar && !hidden.nuevo && !hidden.abrir && !hidden.actualizar && !hidden.live && !hidden.codigo && !hidden.addPage,
      `sin herramientas de dibujo, Nuevo/Abrir/Actualizar, Live, código ni «+» de páginas (${JSON.stringify(hidden)})`);
    check(hidden.edit && hidden.objetos && hidden.preview, 'con el botón para abrir la ventana, vista previa y árbol de objetos');
    await page.evaluate(() => window.JWPLCHMIEditor.setSelectedFieldKeys([]));
    await page.waitForTimeout(500);
    const hint = await page.evaluate(() => { const h = document.getElementById('jwplcBindingsEmpty'); return h && !h.hidden && h.offsetParent ? h.textContent : ''; });
    check(/Elige un objeto/.test(hint), `sin selección explica qué hacer («${hint}»)`);

    console.log('NO SE EDITA EL DISEÑO');
    await page.evaluate(() => window.JWPLCHMIEditor.addValueField());
    await page.waitForTimeout(400);
    const geometry = () => page.evaluate(() => JSON.stringify(window.JWPLCHMIEditor.getAllFields().map((f) => [f.key, f.x, f.y, f.width, f.height])));
    const before = await geometry();
    const box = await page.locator('#displayCanvas').boundingBox();
    await page.mouse.move(box.x + box.width * 0.2, box.y + box.height * 0.25);
    await page.mouse.down();
    await page.mouse.move(box.x + box.width * 0.6, box.y + box.height * 0.7, { steps: 8 });
    await page.mouse.up();
    await page.keyboard.press('Delete');
    await page.keyboard.press('ArrowRight');
    await page.waitForTimeout(400);
    check(await geometry() === before, 'arrastrar, Supr y las flechas no cambian el diseño');
    const selected = await page.evaluate(() => window.JWPLCHMIEditor.getSelectedField()?.type || null);
    check(selected === 'TEXT', `un clic en el lienzo selecciona el objeto (${selected})`);

    console.log('DATOS');
    await page.evaluate(() => { const E = window.JWPLCHMIEditor; E.setSelectedFieldKeys([E.getAllFields().find((f) => f.type === 'VALUE').key]); });
    await page.waitForTimeout(700);
    const data = await page.evaluate(() => ({
      tabs: [...document.querySelectorAll('#fieldInspectorV2 [data-insp-main]')].filter((b) => b.offsetParent).length,
      select: Boolean(document.querySelector('.jwplc-plc-select')?.offsetParent),
      options: [...document.querySelectorAll('.jwplc-plc-select option')].map((o) => o.value)
    }));
    check(data.tabs === 0 && data.select && data.options.includes('CONTADOR'), `inspector sólo con «Variable PLC» (${JSON.stringify(data)})`);
    await page.screenshot({ path: path.join(OUT, 'enlaces_value.png') });

    await page.evaluate(() => window.JWPLCHMIBar.addBarField());
    await page.waitForTimeout(800);
    const bar = await page.evaluate(() => ({
      selects: document.querySelectorAll('.bar-inspector-wrap .jwplc-plc-select').length,
      tabs: [...document.querySelectorAll('.bar-main-tab[data-main-tab]')].filter((b) => b.offsetParent).length
    }));
    check(bar.selects === 1 && bar.tabs === 0, `BAR: un solo selector «Variable PLC», sin pestañas (${JSON.stringify(bar)})`);

    await page.evaluate(async (menu) => {
      const P = window.JWPLCHMIProject; const s = P.serialize(); s.fields.push(menu); await P.apply(s);
    }, MENU);
    await page.waitForTimeout(1200);
    await page.evaluate(() => { const E = window.JWPLCHMIEditor; E.setSelectedFieldKeys([E.getAllFields().find((f) => f.type === 'MENU').key]); });
    await page.waitForTimeout(700);
    await page.click('[data-menu-node="c"]');
    await page.waitForTimeout(700);
    const menu = await page.evaluate(() => ({
      add: Boolean(document.querySelector('[data-menu-add]')?.offsetParent),
      remove: Boolean(document.querySelector('[data-menu-remove]')?.offsetParent),
      label: Boolean(document.querySelector('[data-menu-node-editor] [data-node-prop="label"]')?.offsetParent),
      variable: Boolean(document.querySelector('[data-menu-node-editor] .jwplc-plc-select')?.offsetParent)
    }));
    check(!menu.add && !menu.remove && !menu.label && menu.variable, `MENU: sólo la variable del nodo (${JSON.stringify(menu)})`);
    await page.screenshot({ path: path.join(OUT, 'enlaces_menu.png') });

    console.log('VENTANA DEL EDITOR');
    const label = await page.textContent('#jwplcEditDesignButton');
    check(/Abrir HMI Designer/.test(label), `con la ventana cerrada el botón dice «${label}»`);
    await page.click('#jwplcEditDesignButton');
    await page.evaluate(() => window.__openplc.editorWindow(true));
    await page.waitForTimeout(500);
    const locked = await page.evaluate(() => ({
      calls: window.__openplc.calls.slice(),
      note: Boolean(document.querySelector('.jwplc-bindings-lock')?.offsetParent),
      disabled: [...document.querySelectorAll('.jwplc-plc-select')].every((s) => s.disabled),
      button: document.getElementById('jwplcEditDesignButton').textContent
    }));
    check(locked.calls.includes('open-editor') && /Ir a la ventana/.test(locked.button), 'el botón abre la ventana del Designer (y con ella abierta pasa a «Ir a la ventana…»)');
    check(locked.note && locked.disabled, `con la ventana abierta: aviso y selectores en sólo lectura (${JSON.stringify({ note: locked.note, disabled: locked.disabled })})`);
    await page.screenshot({ path: path.join(OUT, 'enlaces_bloqueada.png') });
    // Cerrada la ventana, el botón vuelve a abrirla.
    await page.evaluate(() => window.__openplc.editorWindow(false));
    await page.waitForTimeout(300);
    await page.click('#jwplcEditDesignButton');
    const reopen = await page.evaluate(() => ({ calls: window.__openplc.calls.filter((c) => c === 'open-editor').length, label: document.getElementById('jwplcEditDesignButton').textContent }));
    check(reopen.calls === 2 && /Abrir HMI Designer/.test(reopen.label), `cerrada la ventana, el botón la vuelve a abrir (${JSON.stringify(reopen)})`);
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
