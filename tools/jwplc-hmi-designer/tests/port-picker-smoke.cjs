#!/usr/bin/env node
// Selector de puertos serie de la app de escritorio (poc/designer-port-picker.js).
// Simula electron/preload.cjs con un window.JWPLCHMINative en memoria.
//
// Uso: node tools/jwplc-hmi-designer/tests/port-picker-smoke.cjs [carpeta-salida]
'use strict';

const fs = require('fs');
const os = require('os');
const path = require('path');
const http = require('http');
const { execFileSync } = require('child_process');

const POC = path.resolve(__dirname, '..', 'poc');
const OUT = path.resolve(process.argv[2] || fs.mkdtempSync(path.join(os.tmpdir(), 'jwplc-hmi-ports-')));

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


// Doble de electron/preload.cjs: sólo el selector de puertos.
function nativeMock() {
  const mock = { chosen: [], ready: false };
  window.__portMock = mock;
  window.JWPLCHMINative = {
    isNative: true,
    getLinkedSketch: async () => ({ ok: false }),
    onSketchOpened: () => {},
    registerSerialPortPicker: ({ onRequest, onList }) => { mock.ready = true; mock.request = onRequest; mock.list = onList; },
    chooseSerialPort: (requestId, portId) => mock.chosen.push({ requestId, portId })
  };
}

// Lo que electron/main.js manda (describeSerialPort): ids en hexadecimal.
const PORTS = [
  { portId: 'bt', portName: 'COM3', displayName: 'Bluetooth', vendorId: '', productId: '', chip: '' },
  { portId: 'plc', portName: 'COM5', displayName: 'USB-SERIAL CH340', vendorId: '1a86', productId: '7523', chip: 'CH340 / CH343' }
];

async function main() {
  fs.mkdirSync(OUT, { recursive: true });
  const server = await serve();
  const { chromium } = loadPlaywright();
  const browser = await chromium.launch({ executablePath: chromiumPath() });
  const page = await browser.newPage({ viewport: { width: 1400, height: 900 } });
  const errors = [];
  page.on('pageerror', (e) => errors.push(e.message));
  await page.addInitScript(nativeMock);

  try {
    await page.goto(`http://127.0.0.1:${server.address().port}/desktop.html`);
    await page.waitForFunction(() => window.JWPLCHMIPortPicker && window.__portMock.ready, null, { timeout: 20000 });

    console.log('SELECTOR DE PUERTOS');
    await page.evaluate((ports) => window.__portMock.request({ requestId: 1, ports }), PORTS);
    await page.waitForTimeout(300);
    const first = await page.evaluate(() => [...document.querySelectorAll('.jwplc-port-item')].map((b) => ({
      id: b.dataset.portId, selected: b.classList.contains('selected'), text: b.textContent.replace(/\s+/g, ' ').trim()
    })));
    check(first.length === 2 && first[0].id === 'plc' && first[0].selected, `el puerto del PLC (CH340) va primero y preseleccionado (${JSON.stringify(first)})`);
    check(/COM5/.test(first[0].text) && /1a86:7523/.test(first[0].text) && /CH340/.test(first[0].text), 'muestra COM, VID:PID en hexadecimal y el chip');
    await page.screenshot({ path: path.join(OUT, 'port-picker.png') });

    await page.click('[data-port-id="bt"]');
    await page.click('[data-action="connect"]');
    await page.waitForTimeout(300);
    let chosen = await page.evaluate(() => window.__portMock.chosen);
    check(chosen.length === 1 && chosen[0].requestId === 1 && chosen[0].portId === 'bt', `«Conectar» devuelve el puerto elegido (${JSON.stringify(chosen)})`);
    check(!(await page.$('.jwplc-port-list')), 'el modal se cierra al conectar');

    // Lista vacía; al enchufar el PLC aparece sin cerrar el modal; Escape cancela.
    await page.evaluate(() => window.__portMock.request({ requestId: 2, ports: [] }));
    await page.waitForTimeout(200);
    check(Boolean(await page.$('.jwplc-port-empty')) && await page.$eval('[data-action="connect"]', (b) => b.disabled), 'sin puertos: aviso y «Conectar» desactivado');
    await page.evaluate((ports) => window.__portMock.list({ requestId: 2, ports: [ports[1]] }), PORTS);
    await page.waitForTimeout(100);
    check(await page.$eval('.jwplc-port-item', (b) => b.dataset.portId === 'plc' && b.classList.contains('selected')), 'el puerto enchufado aparece y queda seleccionado');
    await page.keyboard.press('Escape');
    await page.waitForTimeout(300);
    chosen = await page.evaluate(() => window.__portMock.chosen);
    check(chosen.length === 2 && chosen[1].requestId === 2 && chosen[1].portId === '', 'Escape cancela (puerto vacío)');

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
