#!/usr/bin/env node
// Smoke test E2E de Código QR en el Designer (Playwright sobre desktop.html):
//   - Menú Texto (T) -> Código QR (QR)
//   - Inspector Propuesta 2 (Diseño · Forma / Contenido / Apariencia / Ubicación, Datos, Técnico)
//   - Presets (URL, WIFI), escala, ECC, quiet zone, marco, modo STATIC y VARIABLE
//   - Codegen C++ (static const uint8_t QR_*_BITS[] + jwplcHMIDrawQR / JWPLC_UIQR + jwplcHMIUpdateQR)
//   - Guardar / reabrir .jwhmi (serialize/apply)
//   - Compilación host (g++ -fsyntax-only) cuando está disponible
'use strict';

const assert = require('assert');
const fs = require('fs');
const http = require('http');
const os = require('os');
const path = require('path');
const { execFileSync } = require('child_process');

const DESIGNER = path.resolve(__dirname, '..');
const ROOT = path.resolve(DESIGNER, '..', '..');
const POC = path.join(DESIGNER, 'poc');
const LIBS = path.join(ROOT, 'JWPLC', '2.1.0', 'libraries');
const STUB = path.join(__dirname, 'host-stub');

const MIME = {
  '.html': 'text/html', '.js': 'text/javascript', '.css': 'text/css',
  '.svg': 'image/svg+xml', '.png': 'image/png', '.json': 'application/json',
  '.webmanifest': 'application/manifest+json'
};

function loadPlaywright() {
  try { return require('playwright'); } catch (_) {}
  const globalRoot = execFileSync(process.platform === 'win32' ? 'npm.cmd' : 'npm', ['root', '-g'], { shell: process.platform === 'win32' }).toString().trim();
  return require(path.join(globalRoot, 'playwright'));
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

let failures = 0;
function check(cond, msg) {
  console.log(`  ${cond ? 'PASS' : 'FAIL'} ${msg}`);
  if (!cond) failures += 1;
}

async function main() {
  console.log('=== qr-smoke.cjs — Código QR en HMI Designer ===');
  const server = await serve();
  const { chromium } = loadPlaywright();
  const browser = await chromium.launch();
  const page = await browser.newPage({ viewport: { width: 1440, height: 920 } });
  const pageErrors = [];
  page.on('pageerror', (e) => pageErrors.push(e.message));

  const shotDir = process.env.QR_SHOT_DIR || '';

  try {
    await page.goto(`http://127.0.0.1:${server.address().port}/desktop.html`);
    await page.waitForFunction(() => window.JWPLCHMIEditor?.addQrField && window.JWPLCHMIQR && window.JWPLCHMICodegen?.refresh, null, { timeout: 15000 });
    await page.waitForTimeout(500);

    // 1. Abrir menú T y elegir Código QR
    await page.click('#textToolBtn');
    await page.waitForSelector('#qrToolBtn', { state: 'visible' });
    await page.click('#qrToolBtn');
    await page.waitForTimeout(250);

    const qrVisible = await page.isVisible('#qrInspectorSection');
    check(qrVisible, 'clic en T -> Código QR crea objeto QR y abre #qrInspectorSection');

    const initField = await page.evaluate(() => window.JWPLCHMIEditor.getSelectedField());
    check(initField?.type === 'QR' && initField.sourceMode === 'STATIC' && initField.scale === 3, 'QR creado con defaults (STATIC, 3x)');

    if (shotDir) {
      fs.mkdirSync(shotDir, { recursive: true });
      await page.screenshot({ path: path.join(shotDir, 'qr-forma.png') });
    }

    // 2. Probar Forma: cambiar escala 2x, ECC Q, quietZone 2, marco ON
    await page.click('#qrInspectorSection [data-qr-scale="2"]');
    await page.click('#qrInspectorSection [data-qr-ecc="Q"]');
    await page.click('#qrInspectorSection [data-qr-quiet="2"]');
    await page.click('#qrInspectorSection [data-qr-frame="1"]');
    await page.waitForTimeout(150);

    const afterShape = await page.evaluate(() => {
      const f = window.JWPLCHMIEditor.getSelectedField();
      const b = window.JWPLCHMIEditor.getFieldBounds(f);
      return { f, b };
    });
    check(afterShape.f.scale === 2 && afterShape.f.ecc === 'Q' && afterShape.f.quietZone === 2 && afterShape.f.frame === true, 'Forma actualiza scale/ecc/quietZone/frame');
    check(afterShape.b.width === afterShape.b.height && afterShape.b.width > 40, `bounds cuadrados (${afterShape.b.width}x${afterShape.b.height})`);

    // 3. Probar Contenido: preset Wi-Fi y edición de SSID
    await page.click('#qrInspectorSection [data-qr-sub="content"]');
    await page.click('#qrInspectorSection [data-qr-preset="WIFI"]');
    await page.waitForTimeout(100);
    const wifiVisible = await page.isVisible('#qrInspectorSection [data-qr-wifi-box]');
    check(wifiVisible, 'preset WIFI muestra sub-panel de red Wi-Fi');
    await page.fill('#qrInspectorSection [data-qr-wifi="ssid"]', 'Fabrica_Norte');
    await page.waitForTimeout(150);
    const wifiText = await page.evaluate(() => window.JWPLCHMIEditor.getSelectedField().text);
    check(wifiText.includes('S:Fabrica_Norte'), `Wi-Fi arma el texto automáticamente (${wifiText})`);

    if (shotDir) {
      await page.screenshot({ path: path.join(shotDir, 'qr-contenido-wifi.png') });
    }

    // 4. Probar Apariencia: mini-modelo y colores
    await page.click('#qrInspectorSection [data-qr-sub="appearance"]');
    await page.waitForTimeout(200);
    const appOk = await page.evaluate(() => {
      const c = document.querySelector('#qrInspectorSection [data-qr-preview-canvas]');
      return Boolean(c && c.width > 0 && document.querySelectorAll('#qrInspectorSection [data-qr-swatch]').length === 3);
    });
    check(appOk, 'Apariencia dibuja mini-modelo y 3 muestras (Módulos, Fondo, Marco)');

    if (shotDir) {
      await page.screenshot({ path: path.join(shotDir, 'qr-apariencia.png') });
    }

    // 5. Leer header estático generado
    const staticHeader = await page.evaluate(async () => {
      window.JWPLCHMIEditor.setSelectedFieldKeys([]);
      document.getElementById('contractTab')?.click();
      await new Promise((r) => setTimeout(r, 150));
      window.JWPLCHMICodegen.refresh();
      await new Promise((r) => setTimeout(r, 150));
      return document.getElementById('codeOutput').textContent;
    });
    check(/static const uint8_t QR_FIELD_QR_1_BITS\[\]/.test(staticHeader), 'header estático emite arreglo Flash QR_*_BITS[]');
    check(/jwplcHMIDrawQR\(tft,/.test(staticHeader), 'header estático dibuja con jwplcHMIDrawQR en jwplcUIPageEnter()');
    check(!/#include <JWPLC_UI_Widgets\.h>/.test(staticHeader), 'QR estático no requiere #include <JWPLC_UI_Widgets.h>');

    // 6. Agregar un segundo QR en modo VARIABLE (dinámico)
    await page.click('#textToolBtn');
    await page.click('#qrToolBtn');
    await page.waitForTimeout(150);
    await page.click('#qrInspectorSection [data-qr-main="data"]');
    await page.click('#qrInspectorSection [data-qr-source="VARIABLE"]');
    await page.click('#qrInspectorSection [data-qr-maxlen="128"]');
    await page.waitForTimeout(150);

    const mixedHeader = await page.evaluate(async () => {
      window.JWPLCHMIEditor.setSelectedFieldKeys([]);
      document.getElementById('contractTab')?.click();
      await new Promise((r) => setTimeout(r, 150));
      window.JWPLCHMICodegen.refresh();
      await new Promise((r) => setTimeout(r, 150));
      return document.getElementById('codeOutput').textContent;
    });
    check(/#include <JWPLC_UI_Widgets\.h>/.test(mixedHeader), 'QR dinámico agrega #include <JWPLC_UI_Widgets.h>');
    check(/char qrTexto2\[129\] = "/.test(mixedHeader), 'QR dinámico declara char[maxLength+1]');
    check(/static JWPLC_UIQR jwplcHMIQr_FIELD_QR_2;/.test(mixedHeader), 'QR dinámico declara instancia JWPLC_UIQR');
    check(/jwplcHMIQrDrawn_FIELD_QR_2 = false;/.test(mixedHeader.slice(mixedHeader.indexOf('void jwplcUIPageEnter'), mixedHeader.indexOf('void jwplcUIUpdate'))), 'jwplcUIPageEnter resetea flag de dibujado del QR dinámico');
    check(/jwplcHMIUpdateQR\(jwplcHMIQr_FIELD_QR_2,/.test(mixedHeader.slice(mixedHeader.indexOf('void jwplcUIUpdate'))), 'jwplcUIUpdate llama a jwplcHMIUpdateQR');

    // 7. Round-trip serialize / apply (.jwhmi)
    const roundTrip = await page.evaluate(async () => {
      const saved = JSON.parse(JSON.stringify(window.JWPLCHMIProject.serialize()));
      await window.JWPLCHMIProject.apply(saved);
      await new Promise((r) => setTimeout(r, 250));
      return window.JWPLCHMIEditor.getAllFields().filter((f) => f.type === 'QR');
    });
    check(roundTrip.length === 2 && roundTrip[0].preset === 'WIFI' && roundTrip[1].sourceMode === 'VARIABLE', 'serialize/apply conserva ambos QR y sus propiedades');

    // 8. Compilación C++ en host si hay g++
    let hasGpp = false;
    try { execFileSync('g++', ['--version'], { stdio: 'ignore' }); hasGpp = true; } catch (_) {}
    if (hasGpp) {
      const dir = fs.mkdtempSync(path.join(os.tmpdir(), 'jwplc-qr-smoke-'));
      fs.writeFileSync(path.join(dir, 'JWPLC_HMI_Generated.h'), mixedHeader);
      fs.writeFileSync(path.join(dir, 'sketch.cpp'), '#include "JWPLC_HMI_Generated.h"\nint main() { jwplcHMISetup(); jwplcUIPageEnter(0); jwplcUIUpdate(); return 0; }\n');
      let ok = true;
      try {
        execFileSync('g++', ['-std=gnu++17', '-fsyntax-only', '-Wall', `-I${dir}`, `-I${STUB}`,
          `-I${path.join(LIBS, 'JWPLC_Display', 'src')}`, `-I${path.join(LIBS, 'JWPLC_TFT', 'src')}`,
          `-I${path.join(LIBS, 'JWPLC_UI_Widgets', 'src')}`, path.join(dir, 'sketch.cpp')], { stdio: 'pipe' });
      } catch (e) {
        ok = false;
        console.error(String(e.stderr || e.message));
      }
      fs.rmSync(dir, { recursive: true, force: true });
      check(ok, 'header mixto (estático + dinámico) compila contra JWPLC_Display + JWPLC_UI_Widgets');
    } else {
      console.log('  SKIP g++ no disponible: se omite compilación host');
    }

    check(pageErrors.length === 0, `sin errores JS en consola (${pageErrors.join(' | ') || 'ninguno'})`);
  } finally {
    await browser.close();
    server.close();
  }

  if (failures) {
    console.log(`\n${failures} verificación(es) fallaron.`);
    process.exit(1);
  }
  console.log('\nTodas las pruebas de QR smoke pasaron.');
}

main().catch((err) => { console.error(err); process.exit(1); });
