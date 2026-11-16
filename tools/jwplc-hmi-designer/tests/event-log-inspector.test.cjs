#!/usr/bin/env node
'use strict';

const fs = require('fs');
const path = require('path');
const http = require('http');
const { execFileSync } = require('child_process');

const POC = path.resolve(__dirname, '..', 'poc');
const evtJs = fs.readFileSync(path.join(POC, 'designer-event-log.js'), 'utf8');
const menuJs = fs.readFileSync(path.join(POC, 'designer-menu.js'), 'utf8');
const appJs = fs.readFileSync(path.join(POC, 'app.js'), 'utf8');

console.log('=== event-log-inspector.test.cjs — Estilo unificado MENU/EVENT_LOG, fuente fija GLCD y secciones Memoria/Eventos');

const failures = [];
function check(condition, message) {
  if (condition) {
    console.log(`  PASS ${message}`);
  } else {
    console.log(`  FAIL ${message}`);
    failures.push(message);
  }
}

function loadPlaywright() {
  try { return require('playwright'); } catch (_) {}
  try {
    const globalRoot = execFileSync(process.platform === 'win32' ? 'npm.cmd' : 'npm', ['root', '-g'], { shell: process.platform === 'win32' }).toString().trim();
    return require(path.join(globalRoot, 'playwright'));
  } catch (_) {
    return null;
  }
}

function chromiumPath() {
  const base = process.env.PLAYWRIGHT_BROWSERS_PATH || '/opt/pw-browsers';
  const dir = fs.existsSync(base) ? fs.readdirSync(base).find((d) => /^chromium-\d+$/.test(d)) : null;
  const exe = dir ? path.join(base, dir, 'chrome-linux', 'chrome') : null;
  return exe && fs.existsSync(exe) ? exe : undefined;
}

const MIME = {
  '.html': 'text/html',
  '.js': 'text/javascript',
  '.css': 'text/css',
  '.svg': 'image/svg+xml',
  '.png': 'image/png',
  '.json': 'application/json',
  '.webmanifest': 'application/manifest+json'
};

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

check(!evtJs.includes('data-evt-font-family'), 'EVENT_LOG bloqueado a fuente única GLCD (sin selector multifuente)');
check(!menuJs.includes('data-menu-font-family'), 'MENU bloqueado a fuente única GLCD (sin selector multifuente)');
check(evtJs.includes('data-evt-preview-canvas'), 'Incluye canvas de vista previa en Apariencia');
check(evtJs.includes('window.JWPLCZoneIndicator'), 'Usa JWPLCZoneIndicator en Apariencia');
check(evtJs.includes('window.JWPLCColorEditor'), 'Usa JWPLCColorEditor fijo en Apariencia');
check(evtJs.includes('evt-desc-tree') && evtJs.includes('data-evt-desc-editor'), 'Eventos usa arquitectura lista + editor de detalle');
check(evtJs.includes('data-evt-ram="64"') && evtJs.includes('data-evt-ram="128"'), 'Memoria usa tarjetas visuales de capacidad RAM');
check(!evtJs.includes('<select class="field-input" data-evt-font='), 'Elimina los selects antiguos de fuente');
check(!evtJs.includes('<input type="color" data-evt-color='), 'Elimina inputs nativos <input type="color">');
check(appJs.includes("} else if (field.type === 'EVENT_LOG')"), 'app.js soporta renderFieldPreview para EVENT_LOG');

async function main() {
  const pw = loadPlaywright();
  if (!pw) {
    console.log('  SKIP Playwright no disponible: omitiendo prueba E2E de navegador.');
    process.exit(failures.length ? 1 : 0);
  }

  const server = await serve();
  const { port } = server.address();
  const browser = await pw.chromium.launch({ executablePath: chromiumPath() });
  const errors = [];

  try {
    const page = await browser.newPage({ viewport: { width: 1280, height: 720 } });
    page.on('pageerror', (e) => errors.push(`pageerror: ${e.message}`));
    page.on('console', (m) => { if (m.type() === 'error') errors.push(`console.error: ${m.text()}`); });

    await page.goto(`http://127.0.0.1:${port}/index.html`, { waitUntil: 'networkidle' });
    await page.waitForFunction(() => Boolean(window.JWPLCHMIEditor && window.JWPLCHMIMenu && window.JWPLCHMIEventLog && window.JWPLCColorEditor && window.JWPLCZoneIndicator));

    // 1. Verificar estilo de MENU (colores tipo Historial + fuente fija GLCD)
    const menuDefaults = await page.evaluate(() => {
      document.querySelector('[data-tool="menuField"]').click();
      const f = window.JWPLCHMIEditor.getSelectedField();
      return {
        font: f.font,
        colors: { ...f.colors },
        hasFamilyBtns: document.querySelectorAll('#menuInspectorSection [data-menu-font-family]').length,
        hasTextSizeBtns: document.querySelectorAll('#menuInspectorSection [data-menu-set="textSize"]').length
      };
    });
    check(menuDefaults.font === 'GLCD' && menuDefaults.hasFamilyBtns === 0 && menuDefaults.hasTextSizeBtns === 2, 'MENU: fuente fija GLCD con escala 1×/2× sin otras fuentes');
    check(
      menuDefaults.colors.titleBackground === 0x2124 &&
      menuDefaults.colors.title === 0xFFFF &&
      menuDefaults.colors.selectBackground === 0x03FF &&
      menuDefaults.colors.selectText === 0xFFFF &&
      menuDefaults.colors.text === 0xC618,
      'MENU: colores por defecto adoptan estilo de Historial de Eventos'
    );

    // 2. Insertar EVENT_LOG usando la UI en página nueva (MENU y EVENT_LOG conviven en pruebas)
    const evtOpened = await page.evaluate(() => {
      window.JWPLCHMIEditor.deleteSelectedField?.();
      const btn = document.querySelector('[data-tool="eventLogField"]');
      if (!btn) return false;
      btn.click();
      return true;
    });
    check(evtOpened, 'Botón [data-tool="eventLogField"] inserta EVENT_LOG');

    await page.waitForSelector('#eventLogInspectorSection:not([hidden])');
    const shapeInfo = await page.evaluate(() => {
      const f = window.JWPLCHMIEditor.getSelectedField();
      const card = document.querySelector('#eventLogInspectorSection [data-evt-font-card]');
      return {
        headerFont: f.headerFont,
        rowFont: f.rowFont,
        hasMultiFont: document.querySelectorAll('#eventLogInspectorSection [data-evt-font-family], #eventLogInspectorSection select[data-evt-font]').length > 0,
        badgeText: card?.textContent || ''
      };
    });
    check(shapeInfo.headerFont === 'GLCD' && shapeInfo.rowFont === 'GLCD' && !shapeInfo.hasMultiFont && shapeInfo.badgeText.includes('Clásica 5×7'), 'EVENT_LOG Forma: tipografía única Clásica 5×7 (GLCD)');

    // 3. Apariencia sin scroll + preview + zonas + color editor
    await page.click('#eventLogInspectorSection [data-evt-sub="appearance"]');
    await page.waitForTimeout(80);

    const appMetrics = await page.evaluate(() => {
      const section = document.getElementById('eventLogInspectorSection');
      const panel = section.querySelector('[data-evt-panel="design:appearance"]');
      const scrollHost = document.querySelector('.inspector-panel') || section.closest('.panel');
      const canvas = section.querySelector('[data-evt-preview-canvas]');
      const svg = section.querySelector('[data-evt-preview-body] > svg.zone-indicator');
      const ctx = canvas.getContext('2d');
      const px = ctx.getImageData(0, 0, canvas.width, canvas.height).data;
      let nonZero = 0;
      for (let i = 0; i < px.length; i += 4) {
        if (px[i] || px[i + 1] || px[i + 2]) { nonZero++; break; }
      }
      return {
        panelHidden: panel.hidden,
        panelHeight: Math.round(panel.getBoundingClientRect().height),
        hostScrollHeight: scrollHost ? scrollHost.scrollHeight : 0,
        hostClientHeight: scrollHost ? scrollHost.clientHeight : 0,
        hostScrollWidth: scrollHost ? scrollHost.scrollWidth : 0,
        hostClientWidth: scrollHost ? scrollHost.clientWidth : 0,
        canvasW: canvas.width,
        canvasH: canvas.height,
        nonZero,
        svgPaths: svg ? svg.children.length : 0
      };
    });

    check(!appMetrics.panelHidden, 'Panel Apariencia visible');
    check(appMetrics.canvasW === 148 && appMetrics.canvasH === 88 && appMetrics.nonZero > 0, `Vista previa renderizada (${appMetrics.canvasW}×${appMetrics.canvasH})`);
    check(appMetrics.svgPaths >= 3, `Indicador de zona SVG dibujado (${appMetrics.svgPaths} elementos)`);
    check(appMetrics.hostScrollHeight <= appMetrics.hostClientHeight, `Apariencia sin scroll vertical en 1280×720 (scrollH=${appMetrics.hostScrollHeight} <= clientH=${appMetrics.hostClientHeight}, panelH=${appMetrics.panelHeight})`);
    check(appMetrics.hostScrollWidth <= appMetrics.hostClientWidth, `Apariencia sin scroll horizontal (scrollW=${appMetrics.hostScrollWidth} <= clientW=${appMetrics.hostClientWidth})`);

    await page.click('[data-evt-swatch="sevCrit"]');
    await page.click('[data-evt-palette="65504"]'); // Amarillo
    await page.click('[data-evt-swatch="selectBackground"]');
    await page.click('[data-evt-palette="63488"]'); // Rojo
    const cRect = await page.locator('[data-evt-preview-canvas]').boundingBox();
    await page.mouse.click(cRect.x + cRect.width * 0.7, cRect.y + 8);
    await page.waitForTimeout(40);
    const headerHead = await page.textContent('[data-evt-color-editor] .color-editor-head');
    check((headerHead || '').includes('Cabecera'), `Clic en vista previa selecciona Cabecera (${(headerHead || '').trim()})`);

    // 4. Pestaña Memoria reestructurada (RAM 64/128 + FRAM + MicroSD condicional)
    await page.click('#eventLogInspectorSection [data-evt-main="storage"]');
    await page.waitForTimeout(40);
    await page.click('#eventLogInspectorSection [data-evt-ram="128"]');
    await page.click('#eventLogInspectorSection [data-evt-set="enableFram"][data-value="0"]');
    await page.click('#eventLogInspectorSection [data-evt-set="enableSd"][data-value="0"]');
    const sdHiddenOff = await page.evaluate(() => document.querySelector('#eventLogInspectorSection [data-evt-sd-file]').hidden);
    await page.click('#eventLogInspectorSection [data-evt-set="enableSd"][data-value="1"]');
    await page.fill('#eventLogInspectorSection [data-evt-prop="sdFileName"]', '/ALARMAS.CSV');
    await page.dispatchEvent('#eventLogInspectorSection [data-evt-prop="sdFileName"]', 'change');
    const storageState = await page.evaluate(() => {
      const f = window.JWPLCHMIEditor.getSelectedField();
      const sdWrap = document.querySelector('#eventLogInspectorSection [data-evt-sd-file]');
      return { ramCapacity: f.ramCapacity, enableFram: f.enableFram, enableSd: f.enableSd, sdFileName: f.sdFileName, sdHiddenOn: sdWrap.hidden };
    });
    check(
      storageState.ramCapacity === 128 &&
      storageState.enableFram === false &&
      storageState.enableSd === true &&
      storageState.sdFileName === '/ALARMAS.CSV' &&
      sdHiddenOff === true &&
      storageState.sdHiddenOn === false,
      'Memoria: RAM 128, toggle FRAM, MicroSD y ruta CSV condicional funcionan correctamente'
    );

    // 5. Pestaña Eventos reestructurada (lista .evt-desc-tree + editor de detalle con chips)
    await page.click('#eventLogInspectorSection [data-evt-main="descriptors"]');
    await page.waitForTimeout(40);
    await page.click('#eventLogInspectorSection [data-evt-add-desc]');
    await page.fill('#eventLogInspectorSection [data-evt-desc-prop="label"]', 'Sobrepresion caldera');
    await page.click('#eventLogInspectorSection [data-evt-desc-sev="CRIT"]');
    await page.click('#eventLogInspectorSection [data-evt-desc-cat="ALARM"]');
    const descState = await page.evaluate(() => {
      const f = window.JWPLCHMIEditor.getSelectedField();
      const last = f.descriptors[f.descriptors.length - 1];
      const rowsCount = document.querySelectorAll('#eventLogInspectorSection .evt-desc-tree [data-evt-desc-row]').length;
      const scrollHost = document.querySelector('.inspector-panel');
      return {
        count: f.descriptors.length,
        rowsCount,
        last,
        hasInlineOnchange: document.querySelector('#eventLogInspectorSection .evt-desc-tree')?.innerHTML.includes('onchange='),
        noScroll: scrollHost ? (scrollHost.scrollHeight <= scrollHost.clientHeight) : true
      };
    });
    check(
      descState.count === descState.rowsCount &&
      descState.last?.label === 'Sobrepresion caldera' &&
      descState.last?.severity === 'CRIT' &&
      descState.last?.category === 'ALARM' &&
      !descState.hasInlineOnchange,
      'Eventos: agrega, edita texto/severidad/categoría en vista catálogo + detalle sin onchange inline'
    );
    check(descState.noScroll, 'Eventos: pestaña organizada sin scroll excesivo en 1280×720');

    check(errors.length === 0, `Sin errores en consola/página (${errors.join('; ')})`);
  } finally {
    await browser.close();
    server.close();
  }

  console.log(`\nResultado: ${failures.length === 0 ? 'TODAS LAS PRUEBAS PASARON' : `${failures.length} fallos`}`);
  process.exit(failures.length ? 1 : 0);
}

main().catch((err) => {
  console.error(err);
  process.exit(1);
});
