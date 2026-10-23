// El HMI Designer real en Chromium headless, para la CLI.
//
// El header C++ se arma por capas dentro del navegador (designer-codegen.js y
// los módulos que lo parchean), así que la CLI no lo reimplementa: sirve poc/,
// abre desktop.html con Playwright, carga el proyecto con
// JWPLCHMIProject.apply() y lee #codeOutput, igual que tests/codegen-golden.cjs.
// El resultado es byte a byte el mismo que «Escribir en sketch».
'use strict';

const fs = require('fs');
const path = require('path');
const http = require('http');
const { execFileSync } = require('child_process');

const POC = path.resolve(__dirname, '..', '..', 'poc');
const HEADER_MARK = '// Código generado por JWPLC HMI Designer';
const MIME = { '.html': 'text/html', '.js': 'text/javascript', '.css': 'text/css', '.svg': 'image/svg+xml', '.png': 'image/png', '.json': 'application/json', '.webmanifest': 'application/manifest+json', '.bin': 'application/octet-stream' };

function loadPlaywright() {
  try { return require('playwright'); } catch (_) { /* global */ }
  try {
    const globalRoot = execFileSync(process.platform === 'win32' ? 'npm.cmd' : 'npm', ['root', '-g'], { shell: process.platform === 'win32' }).toString().trim();
    return require(path.join(globalRoot, 'playwright'));
  } catch (_) {
    throw new Error('Falta Playwright. Instálalo una vez: npm install -g playwright && npx playwright install chromium');
  }
}

function chromiumPath() {
  const base = process.env.PLAYWRIGHT_BROWSERS_PATH || '/opt/pw-browsers';
  const dir = fs.existsSync(base) ? fs.readdirSync(base).find((d) => /^chromium-\d+$/.test(d)) : null;
  const exe = dir ? path.join(base, dir, 'chrome-linux', 'chrome') : null;
  return exe && fs.existsSync(exe) ? exe : undefined;
}

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

const wait = (ms) => new Promise((r) => setTimeout(r, ms));

class DesignerSession {
  static async open() {
    const session = new DesignerSession();
    session.server = await serve();
    const { chromium } = loadPlaywright();
    session.browser = await chromium.launch({ executablePath: chromiumPath() });
    session.page = await session.browser.newPage({ viewport: { width: 1600, height: 1000 } });
    session.pageErrors = [];
    session.page.on('pageerror', (error) => session.pageErrors.push(String(error.message || error)));
    // Sin vínculo a un sketch ni diálogos del navegador.
    session.page.on('dialog', (dialog) => dialog.dismiss().catch(() => {}));
    await session.page.goto(`http://127.0.0.1:${session.server.address().port}/desktop.html`);
    await session.page.waitForFunction(() => window.JWPLCHMIProject?.apply && window.JWPLCHMICodegen?.refresh &&
      window.JWPLCHMIMenu && window.JWPLCHMIBar && window.JWPLCHMIBool && window.JWPLCHMIPages, null, { timeout: 30000 });
    await wait(800);
    return session;
  }

  async close() {
    await this.browser?.close().catch(() => {});
    this.server?.close();
  }

  // Carga un proyecto normalizado en el Designer.
  async load(project) {
    await this.page.evaluate(async (proj) => {
      await window.JWPLCHMIProject.apply(proj);
      await new Promise((r) => setTimeout(r, 300));
      const E = window.JWPLCHMIEditor;
      E.setSelectedFieldKeys([]);
      E.render();
    }, JSON.parse(JSON.stringify(project)));
  }

  // Proyecto tal como lo guardaría el Designer («Guardar»).
  async serialize() {
    return this.page.evaluate(() => window.JWPLCHMIProject.serialize());
  }

  // JWPLC_HMI_Generated.h del proyecto cargado.
  async header() {
    const text = await this.page.evaluate(async (mark) => {
      for (let k = 0; k < 10; k += 1) {
        document.getElementById('contractTab')?.click();
        await new Promise((r) => setTimeout(r, 120));
        window.JWPLCHMICodegen.refresh();
        await new Promise((r) => setTimeout(r, 120));
        const out = document.getElementById('codeOutput')?.textContent || '';
        if (out.startsWith(mark) || out.startsWith('// ERROR CODEGEN')) return out;
      }
      return null;
    }, HEADER_MARK);
    if (text === null) throw new Error('El Designer no generó el header.');
    return text.endsWith('\n') ? text : `${text}\n`;
  }

  // Cajas reales de cada objeto (métricas del lienzo) y diagnósticos del Designer.
  async inspect() {
    return this.page.evaluate(() => {
      const E = window.JWPLCHMIEditor;
      const boxes = E.getAllFields().map((f, index) => {
        const b = E.getFieldBounds(f);
        return { index, id: f.id, type: f.type, page: Number(f.page || 0), global: f.type === 'NAV_BAR', minX: b.minX, minY: b.minY, maxX: b.maxX, maxY: b.maxY };
      });
      const issues = (window.JWPLCHMICodegen.validateIdentifiers?.() || []).map((i) => ({ property: i.property, canonical: i.canonical }));
      return { boxes, issues };
    });
  }

  // PNG de cada página, como se verá en la TFT (lienzo del Designer sin selección).
  async previews(scale = 2) {
    const shots = await this.page.evaluate(async (k) => {
      const E = window.JWPLCHMIEditor;
      const out = [];
      for (const page of E.getPages()) {
        E.setActivePage(page.id);
        E.setSelectedFieldKeys([]);
        E.render();
        await new Promise((r) => setTimeout(r, 150));
        const src = document.getElementById('previewCanvas');
        const canvas = document.createElement('canvas');
        canvas.width = src.width * k;
        canvas.height = src.height * k;
        const ctx = canvas.getContext('2d');
        ctx.imageSmoothingEnabled = false;
        ctx.drawImage(src, 0, 0, canvas.width, canvas.height);
        out.push({ id: page.id, name: page.name, data: canvas.toDataURL('image/png').split(',')[1] });
      }
      E.setActivePage(0);
      return out;
    }, Math.max(1, Math.min(6, Math.trunc(scale) || 1)));
    return shots.map((s) => ({ ...s, png: Buffer.from(s.data, 'base64') }));
  }
}

// Avisos de geometría a partir de inspect(): fuera de pantalla y campos que se pisan.
function geometryWarnings(inspection, project) {
  const warnings = [];
  const W = 320;
  const H = 170;
  const dynamic = new Set(['TEXT', 'VALUE', 'BOOL', 'BAR', 'MENU']);
  const label = (b) => `fields[${b.index}]${b.id && b.type !== 'NAV_BAR' ? ` (${b.id})` : ''}`;
  inspection.boxes.forEach((b) => {
    if (b.minX < 0 || b.minY < 0 || b.maxX >= W || b.maxY >= H) {
      warnings.push({ path: label(b), message: `sale de la pantalla: ocupa x ${b.minX}..${b.maxX}, y ${b.minY}..${b.maxY} (pantalla 0..319 × 0..169)` });
    }
  });
  const overlap = (a, b) => a.minX <= b.maxX && b.minX <= a.maxX && a.minY <= b.maxY && b.minY <= a.maxY;
  const boxes = inspection.boxes.filter((b) => dynamic.has(b.type) || b.type === 'NAV_BAR');
  for (let i = 0; i < boxes.length; i += 1) {
    for (let j = i + 1; j < boxes.length; j += 1) {
      const a = boxes[i];
      const b = boxes[j];
      if (!(a.global || b.global) && a.page !== b.page) continue;
      if (a.global && b.global) continue;
      if (b.type === 'NAV_BAR' && project.navigationBar?.preset === 'counter') continue;
      if (overlap(a, b)) warnings.push({ path: label(a), message: `se pisa con ${label(b)}${a.global || b.global ? '' : ` en la página ${a.page}`}: el último que se redibuja tapa al otro` });
    }
  }
  inspection.issues.forEach((i) => warnings.push({ path: 'fields', message: `${i.property === 'id' ? 'ID' : 'variable'} C++ duplicado: ${i.canonical}` }));
  return warnings;
}

module.exports = { DesignerSession, geometryWarnings, HEADER_MARK, POC };
