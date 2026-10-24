#!/usr/bin/env node
// Golden tests del codegen (A15-5).
//
// Carga cada proyecto de tests/fixtures.cjs en el Designer (Chromium headless),
// genera JWPLC_HMI_Generated.h y lo compara byte a byte con tests/golden/<n>.h.
// Un cambio intencional del codegen se acepta regenerando los golden:
//
//   node tools/jwplc-hmi-designer/tests/codegen-golden.cjs            # comparar
//   node tools/jwplc-hmi-designer/tests/codegen-golden.cjs --update   # aceptar cambios
//
// Requiere: playwright (local o global) con Chromium. Sin g++.
'use strict';

const fs = require('fs');
const path = require('path');
const http = require('http');
const { execFileSync } = require('child_process');

const POC = path.resolve(__dirname, '..', 'poc');
const GOLDEN = path.join(__dirname, 'golden');
const UPDATE = process.argv.includes('--update');
const FIXTURES = require('./fixtures.cjs');

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

function firstDiff(a, b) {
  const la = a.split('\n');
  const lb = b.split('\n');
  for (let i = 0; i < Math.max(la.length, lb.length); i++) {
    if (la[i] !== lb[i]) return `línea ${i + 1}\n      esperado: ${JSON.stringify(lb[i] ?? '(fin)')}\n      obtenido: ${JSON.stringify(la[i] ?? '(fin)')}`;
  }
  return '';
}

async function main() {
  fs.mkdirSync(GOLDEN, { recursive: true });
  const server = await serve();
  const { chromium } = loadPlaywright();
  const browser = await chromium.launch({ executablePath: chromiumPath() });
  const page = await browser.newPage({ viewport: { width: 1600, height: 1000 } });
  const failures = [];

  try {
    await page.goto(`http://127.0.0.1:${server.address().port}/desktop.html`);
    await page.waitForFunction(() => window.JWPLCHMIProject?.apply && window.JWPLCHMICodegen?.refresh && window.JWPLCHMIMenu, null, { timeout: 20000 });
    await page.waitForTimeout(1000);

    for (const [name, project] of Object.entries(FIXTURES)) {
      const text = await page.evaluate(async (proj) => {
        const P = window.JWPLCHMIProject;
        await P.apply({ ...P.serialize(), display: null, ...JSON.parse(JSON.stringify(proj)), pixelMaps: [] });
        await new Promise((r) => setTimeout(r, 400));
        const E = window.JWPLCHMIEditor;
        E.setSelectedFieldKeys([]);
        E.render();
        for (let k = 0; k < 8; k += 1) {
          document.getElementById('contractTab')?.click();
          await new Promise((r) => setTimeout(r, 120));
          window.JWPLCHMICodegen.refresh();
          await new Promise((r) => setTimeout(r, 120));
          const out = document.getElementById('codeOutput').textContent;
          if (out.startsWith('// Código generado por JWPLC HMI Designer')) return out;
        }
        return null;
      }, project);

      const file = path.join(GOLDEN, `${name}.h`);
      if (text === null) {
        console.log(`  FAIL ${name}: el Designer no generó un header`);
        failures.push(name);
        continue;
      }
      if (UPDATE || !fs.existsSync(file)) {
        fs.writeFileSync(file, text);
        console.log(`  WROTE ${name}.h (${text.length} B)`);
        continue;
      }
      const expected = fs.readFileSync(file, 'utf8').replace(/\r\n/g, '\n');
      if (expected === text) console.log(`  PASS ${name}.h idéntico al golden`);
      else {
        console.log(`  FAIL ${name}.h difiere del golden: ${firstDiff(text, expected)}`);
        fs.writeFileSync(path.join(GOLDEN, `${name}.actual.h`), text);
        failures.push(name);
      }
    }
  } finally {
    await browser.close();
    server.close();
  }

  if (failures.length) {
    console.log(`\n${failures.length} golden difieren. Si el cambio es intencional: --update (y revisar el diff en git).`);
    process.exit(1);
  }
  console.log(UPDATE ? '\nGolden actualizados.' : '\nTodos los golden coinciden.');
}

main().catch((error) => { console.error(error); process.exit(1); });
