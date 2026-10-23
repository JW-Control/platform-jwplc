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
  // Elige el cuadro de color y aplica un color de la paleta rápida del editor
  // fijo (designer-color-editor.js).
  const pickColor = async (key, value) => {
    await page.$eval(`${W}[data-field-swatch="${key}"]`, (el) => el.click());
    await page.click(`${W}[data-field-palette="${value}"]`);
  };
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
    // Igual que designer-project.js:generatedHeaderText (Actualizar HMI).
    window.JWPLCHMICodegen?.refresh?.();
    await new Promise((r) => setTimeout(r, 0));
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
    await pickColor('value', 0xF800);
    await pickColor('frame', 0xFFE0);
    check(await page.$eval(`${W}[data-field-case="single"]`, (c) => c.width > 0 && c.height > 0), 'Apariencia: mini-modelo del campo');
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

    console.log('VALUE · FORMATO (miles y duración)');
    await sub('content');
    for (let i = 0; i < 3; i++) await page.click(`${W}[data-vkind-panel="NUMBER"] [data-stepper="integerDigits"] [data-step="1"]`);
    await page.click(`${W}[data-set="decimalDigits"][data-value="0"]`);
    await page.click(`${W}[data-vsep=","]`);
    await page.evaluate(() => { const f = window.JWPLCHMIEditor.getSelectedField(); f.preview = '1234567'; window.JWPLCHMIEditor.render(); });
    await page.waitForTimeout(150);
    v = await selected();
    check(v.thousandsSep === ',' && v.integerDigits === 7, 'Número: separador de miles y 7 dígitos');
    check(await page.evaluate(() => window.JWPLCValueFormat.preview(window.JWPLCHMIEditor.getSelectedField())) === '1,234,567', 'Número: el lienzo muestra 1,234,567');
    check(await page.$eval(`${W}[data-readout="reserve"]`, (el) => el.textContent) === '-8,888,888', 'Número: la reserva incluye los separadores');
    await shot('value_formato_miles');
    let vh = await header();
    check(/'\,'\)/.test(vh) || /',',?\)/.test(vh), "header: separador ',' como 5.º argumento");
    check(/int32_t presion = 0;/.test(vh), 'header: contador con miles sin decimales -> int32_t (con signo)');
    await page.click('#canvasWrap canvas, canvas').catch(() => {});
    await page.evaluate(() => { const E = window.JWPLCHMIEditor; const f = E.getAllFields().find((x) => x.variable === 'presion'); E.setSelectedFieldKeys([f.key]); E.render(); });
    await page.waitForTimeout(200);
    await sub('content');
    await page.click(`${W}[data-vkind="DURATION"]`);
    v = await selected();
    check(v.valueKind === 'DURATION' && v.integerDigits === 2 && v.decimalDigits === 0 && v.preview === '303', 'Duración: valores iniciales (2 dígitos, 303 s)');
    check(await page.$eval(`${W}[data-vkind-panel="NUMBER"]`, (el) => el.hidden) && !(await page.$eval(`${W}[data-vkind-panel="DURATION"]`, (el) => el.hidden)), 'Duración: sólo se ven sus controles');
    check(await page.evaluate(() => window.JWPLCValueFormat.preview(window.JWPLCHMIEditor.getSelectedField())) === '05:03', 'Duración MM:SS: 303 s -> 05:03');
    await page.click(`${W}[data-vpattern="HH_MM_SS"]`);
    await page.click(`${W}[data-vunit="MS"]`);
    v = await selected();
    check(v.durationPattern === 'HH_MM_SS' && v.durationUnit === 'MS' && v.preview === '303000', 'Duración: patrón HH:MM:SS y ms (la vista previa se convierte)');
    check(await page.$eval(`${W}[data-readout="leadTitle"]`, (el) => el.textContent) === 'Dígitos de las horas', 'Duración: el título del paso sigue al patrón');
    check(await page.$eval(`${W}[data-readout="formatted"]`, (el) => el.textContent) === '00:05:03', 'Duración: resultado 00:05:03');
    await shot('value_formato_duracion');
    vh = await header();
    check(/JWPLC_UIDurationFormat\(\n\s*JWPLC_UI_DURATION_HH_MM_SS,\n\s*JWPLC_UI_DURATION_MILLIS,\n\s*2,\n\s*true\)/.test(vh), 'header: JWPLC_UIDurationFormat(HH_MM_SS, MILLIS, 2, true)');
    await page.evaluate(() => { const E = window.JWPLCHMIEditor; const f = E.getAllFields().find((x) => x.variable === 'presion'); E.setSelectedFieldKeys([f.key]); E.render(); });
    await page.waitForTimeout(200);
    await sub('content');
    await page.click(`${W}[data-vkind="NUMBER"]`);
    check((await selected()).valueKind === 'NUMBER' && (await selected()).integerDigits === 3, 'volver a Número restablece 3 dígitos');
    await page.evaluate(() => window.JWPLCHMIEditor.undo());
    await page.waitForTimeout(200);
    check((await fieldOf('VALUE')).valueKind === 'DURATION', 'undo vuelve a Duración');
    await page.evaluate(() => window.JWPLCHMIEditor.redo());
    // Deja el formato como estaba (4 enteros, 2 decimales, sin separador) para HEADER.
    await page.click(`${W}[data-vkind-panel="NUMBER"] [data-stepper="integerDigits"] [data-step="1"]`);
    await page.click(`${W}[data-set="decimalDigits"][data-value="2"]`);
    await page.click(`${W}[data-vsep=""]`);
    v = await selected();
    check(v.valueKind === 'NUMBER' && v.integerDigits === 4 && v.decimalDigits === 2 && v.thousandsSep === '', 'Número restaurado (4, 2, sin separador)');

    console.log('VALUE · FECHA/HORA (RTC)');
    await page.evaluate(() => { window.JWPLCHMIEditor.setSelectedFieldKeys([]); window.JWPLCHMIEditor.addValueField(); });
    await page.waitForTimeout(300);
    await sub('content');
    await page.click(`${W}[data-vkind="DATETIME"]`);
    await page.click(`${W}[data-vdt="HH_MM_SS"]`);
    let ck = await selected();
    check(ck.valueKind === 'DATETIME' && ck.dateTimePattern === 'HH_MM_SS', 'Fecha/hora: tipo y patrón');
    check(await page.$eval(`${W}[data-readout="reserve"]`, (el) => el.textContent) === '88:88:88', 'Fecha/hora: reserva 88:88:88');
    check(/^\d\d:\d\d:\d\d$/.test(await page.$eval(`${W}[data-readout="formatted"]`, (el) => el.textContent)), 'Fecha/hora: el lienzo muestra la hora del PC');
    await page.click(`${W}[data-rtc-invalid]`);
    check(await page.$eval(`${W}[data-readout="formatted"]`, (el) => el.textContent) === '--:--:--', 'Fecha/hora: vista «sin reloj válido» --:--:--');
    await shot('value_formato_fecha_hora');
    await page.click(`${W}[data-insp-main="data"]`);
    check(await page.$eval(`${W}[data-clock-only]`, (el) => !el.hidden) && await page.$eval(`${W}[data-not-clock]`, (el) => el.hidden), 'Datos: sin variable, con la nota del reloj');
    await shot('value_fecha_hora_datos');
    const ckh = await header();
    check(/JWPLC_UIDateTimeFormat\(\n\s*JWPLC_UI_DATETIME_HH_MM_SS\)/.test(ckh), 'header: JWPLC_UIDateTimeFormat(HH_MM_SS)');
    await page.evaluate(() => { const E = window.JWPLCHMIEditor; const f = E.getAllFields().find((x) => x.valueKind === 'DATETIME'); E.setSelectedFieldKeys([f.key]); E.render(); });
    await page.waitForTimeout(200);

    console.log('CAPAS (lo nuevo al frente en el lienzo)');
    const layering = await page.evaluate(async () => {
      const E = window.JWPLCHMIEditor;
      E.setSelectedFieldKeys([]);
      E.addTextField();
      const text = E.getSelectedField();
      text.x = 200; text.y = 120;
      E.setSelectedFieldKeys([]);
      window.JWPLCHMIBool.addBoolField();
      const b = E.getSelectedField();
      Object.assign(b, { x: 200, y: 120, stateColors: true, boolValue: true, trueBackground: 0x001F, falseBackground: 0x001F, trueColor: 0xFFFF, falseColor: 0xFFFF });
      E.render();
      await new Promise((r) => setTimeout(r, 100));
      const order = { text: text.layerOrder, bool: b.layerOrder };
      const keys = [text.key, b.key];
      return { order, keys };
    });
    check(layering.order.bool < layering.order.text, `el BOOL nuevo tiene layerOrder menor (al frente) (${JSON.stringify(layering.order)})`);
    const pixelAt = (x, y) => page.evaluate(([px, py]) => {
      const c = document.getElementById('displayCanvas');
      const z = c.width / 320;
      const d = c.getContext('2d').getImageData(Math.floor((px + 0.5) * z), Math.floor((py + 0.5) * z), 1, 1).data;
      return [d[0], d[1], d[2]];
    }, [x, y]);
    const p = await pixelAt(201, 121);
    check(p[2] > 200 && p[0] < 40 && p[1] < 40, `el BOOL nuevo se dibuja encima del TEXT anterior (pixel ${p.join(',')} = azul del fondo TRUE)`);
    await page.evaluate((keys) => {
      const E = window.JWPLCHMIEditor;
      keys.forEach((k) => { E.setSelectedFieldKeys([k]); E.deleteSelectedField(); });
      E.setSelectedFieldKeys([]);
      E.render();
    }, layering.keys);

    console.log('TILDES (cp437)');
    const accents = await page.evaluate(() => {
      const E = window.JWPLCHMIEditor;
      const f = E.getAllFields().find((x) => x.type === 'VALUE');
      const draw = (label) => Array.from(E.renderFieldPreview({ ...f, x: 0, y: 0, label, unit: '' }).buffer.slice(0, 320 * 20)).join(',');
      return { o: draw('ó'), q: draw('?'), plain: draw('o'), deg: draw('°'), snow: draw('☃') };
    });
    check(accents.o !== accents.q && accents.o !== accents.plain, 'el lienzo dibuja ó con su glifo (ni ? ni o)');
    check(accents.deg !== accents.q && accents.snow === accents.q, '° tiene glifo; ☃ se dibuja como ?');

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
    await sub('appearance');
    await page.click(`${W}[data-bool-colormode="state"]`);
    check(await page.$$eval(`${W}[data-field-case]`, (els) => els.map((c) => c.dataset.fieldCase).join(',')) === 'false,true', 'BOOL por estado: mini-modelos FALSE y TRUE');
    await pickColor('false', 0xF800);
    await pickColor('true', 0x07E0);
    await pickColor('falseBg', 0x0000);
    await pickColor('trueBg', 0x001F);
    const boc = await selected();
    check(boc.stateColors === true && boc.falseColor === 0xF800 && boc.trueColor === 0x07E0, 'BOOL: color de texto por estado');
    check(boc.falseBackground === 0x0000 && boc.trueBackground === 0x001F, 'BOOL: color de fondo por estado');
    // Editor de color fijo: el texto de la fila elige el color (como su cuadro),
    // el editor sigue a la vista al cambiar de fila y el HEX aplica en vivo.
    await page.click(`${W}[data-field-appearance] [data-legend-key="label"] .menu-legend-label`);
    const rowPick = await page.evaluate(() => ({
      active: document.querySelector('#fieldInspectorV2 [data-field-swatch="label"]').classList.contains('active'),
      target: document.querySelector('#fieldInspectorV2 [data-ce-target]')?.textContent,
      editor: document.querySelector('#fieldInspectorV2 .color-editor')?.offsetParent !== null,
      native: document.querySelectorAll('#fieldInspectorV2 input[type="color"]').length
    }));
    check(rowPick.active && rowPick.editor && rowPick.native === 0 && /Etiqueta/.test(rowPick.target || ''),
      `Apariencia: clic en el texto de la fila = clic en su cuadro; editor fijo sin selector nativo (${JSON.stringify(rowPick)})`);
    await page.fill(`${W}[data-ce-hex]`, '#ff00ff');
    const live = await page.evaluate(() => window.JWPLCHMIEditor.getSelectedField().labelColor);
    check(live === 0xF81F, 'color en vivo desde el HEX del editor');
    await page.press(`${W}[data-ce-hex]`, 'Enter');
    // Arrastre en el cuadro S/V: cambia el color en vivo y al soltar va al historial.
    const sv = await (await page.$(`${W}[data-ce-sv]`)).boundingBox();
    await page.mouse.move(sv.x + sv.width - 2, sv.y + 2);
    await page.mouse.down();
    await page.mouse.move(sv.x + 4, sv.y + sv.height - 4, { steps: 4 });
    await page.mouse.up();
    const dragged = await page.evaluate(() => window.JWPLCHMIEditor.getSelectedField().labelColor);
    check(dragged !== 0xF81F && dragged < 0x2000, `arrastre en el cuadro S/V oscurece el color (0x${dragged.toString(16)})`);
    await page.evaluate(() => window.JWPLCHMIEditor.undo());
    await shot('bool_apariencia');
    await page.click(`${W}[data-insp-main="data"]`);
    check(bo.boolValue === true && bo.preview === 'MARCHA', 'estado de prueba con interruptor');

    console.log('APARIENCIA A1 (indicador de zona)');
    const zoneInfo = () => page.evaluate(() => [...document.querySelectorAll('#fieldInspectorV2 [data-field-case-body]')].map((body) => ({
      id: body.dataset.fieldCaseBody,
      accent: body.querySelectorAll('svg.zone-indicator rect.zone-outline-accent').length,
      marker: body.querySelectorAll('svg.zone-indicator path.zone-marker').length,
      active: [...body.querySelectorAll('.menu-legend-row.is-active')].map((r) => r.textContent.trim())
    })));
    const casePixels = () => page.$$eval('#fieldInspectorV2 canvas[data-field-case]', (els) => els.map((c) => c.toDataURL()).join('|'));
    // BOOL por estado: la etiqueta es común -> indicador en FALSE y TRUE.
    await sub('appearance');
    await page.$eval(`${W}[data-field-swatch="label"]`, (el) => el.dispatchEvent(new MouseEvent('click', { bubbles: true })));
    await page.waitForTimeout(200);
    let zi = await zoneInfo();
    check(zi.length === 2 && zi.every((z) => z.marker === 1 && z.accent >= 1 && /común/.test(z.active.join(' '))), `BOOL por estado: Etiqueta (común) marcada en los dos casos (${JSON.stringify(zi)})`);
    const pixA = await casePixels();
    await page.$eval(`${W}[data-field-swatch="trueBg"]`, (el) => el.dispatchEvent(new MouseEvent('click', { bubbles: true })));
    await page.waitForTimeout(200);
    zi = await zoneInfo();
    check(zi.find((z) => z.id === 'true')?.marker === 1 && zi.find((z) => z.id === 'false')?.marker === 0, 'BOOL por estado: el fondo TRUE sólo se marca en su caso');
    check(pixA === await casePixels(), 'el indicador no cambia los píxeles de los mini-modelos');
    await shot('bool_apariencia_a1');
    // VALUE con unidad: la zona Etiqueta son dos rectángulos (etiqueta + unidad).
    await page.evaluate(() => { const E = window.JWPLCHMIEditor; const f = E.getAllFields().find((x) => x.type === 'VALUE' && x.unit); E.setSelectedFieldKeys([f.key]); E.render(); });
    await page.waitForTimeout(200);
    await sub('appearance');
    await page.$eval(`${W}[data-field-swatch="label"]`, (el) => el.dispatchEvent(new MouseEvent('click', { bubbles: true })));
    await page.waitForTimeout(200);
    zi = await zoneInfo();
    check(zi.length === 1 && zi[0].accent === 2 && zi[0].marker === 1, `VALUE: Etiqueta = etiqueta + unidad (2 contornos) (${JSON.stringify(zi)})`);
    await shot('value_apariencia_a1');
    await page.evaluate(() => { const E = window.JWPLCHMIEditor; const b = E.getAllFields().filter((f) => f.type === 'BOOL'); E.setSelectedFieldKeys([b[0].key]); E.render(); });
    await page.waitForTimeout(200);

    console.log('BOOL · PILOTO');
    // addBoolField sólo crea uno nuevo si el seleccionado no es BOOL.
    await page.evaluate(() => { window.JWPLCHMIEditor.setSelectedFieldKeys([]); window.JWPLCHMIBool.addBoolField(); });
    await page.waitForTimeout(300);
    await page.evaluate(() => { const f = window.JWPLCHMIEditor.getSelectedField(); f.variable = 'piloto_bomba'; f.label = 'Bomba'; window.JWPLCHMIEditor.render(); });
    await sub('content');
    await page.click(`${W}[data-bool-style="PILOT"]`);
    const bp = await selected();
    check(bp.boolStyle === 'PILOT' && bp.stateColors === true && bp.falseColor === 0xF800 && bp.trueColor === 0x07E0, 'Piloto: color por estado activo (rojo/verde)');
    check(await page.$eval(`${W}[data-insp-panel="design:content"] [data-bool-text-only]`, (el) => el.hidden), 'Piloto: se ocultan los textos de estado');
    await shot('bool_piloto_contenido');
    await sub('appearance');
    const legend = await page.$eval(`${W}[data-field-appearance]`, (el) => el.textContent);
    check(/Etiqueta y aro/.test(legend) && /Piloto/.test(legend) && /apagado/.test(legend), 'Piloto: leyenda «Etiqueta y aro» y «Piloto» por caso');
    await shot('bool_piloto_apariencia');
    const px = await page.evaluate(() => {
      const E = window.JWPLCHMIEditor;
      const f = E.getSelectedField();
      const res = {};
      for (const size of [1, 2, 3, 4]) {
        for (const on of [false, true]) {
          const out = E.renderFieldPreview({ ...f, x: 0, y: 0, valueSize: size, boolValue: on });
          const g = out.g;
          const r = Math.floor((g.valueH - 1) / 2);
          const at = (x, y) => out.buffer[y * out.width + x];
          res[`${size}${on ? 'T' : 'F'}`] = {
            side: g.valueW === 7 * size && g.valueH === 7 * size,
            center: at(g.valueX + r, g.valueY + r) === (on ? f.trueColor : f.falseColor),
            ring: at(g.valueX, g.valueY + r) === (f.labelColor & 0xFFFF),
            corner: at(g.valueX, g.valueY) === (Number(on ? f.trueBackground : f.falseBackground) & 0xFFFF),
            dbg: [at(g.valueX + r, g.valueY + r), at(g.valueX, g.valueY + r), at(g.valueX, g.valueY)]
          };
        }
      }
      return res;
    });
    check(Object.values(px).every((p) => p.side && p.center && p.ring && p.corner), `Piloto 1×..4×: caja 7×escala, centro del estado, aro de la etiqueta, esquinas con el fondo del estado (${JSON.stringify(Object.entries(px).filter(([, p]) => !(p.side && p.center && p.ring && p.corner)).map(([k, p]) => k + ':' + p.dbg.join('/')))})`);
    const ph = await header();
    check(/#ifdef JWPLC_UI_HAS_BOOL_PILOT\n\s*JWPLC_UIBoolPilot\(\n\s*0xF800,\n\s*0x07E0\),/.test(ph), 'header: JWPLC_UIBoolPilot(0xF800, 0x07E0) con respaldo de texto');
    await page.evaluate(() => { const E = window.JWPLCHMIEditor; const b = E.getAllFields().filter((f) => f.type === 'BOOL'); E.setSelectedFieldKeys([b[0].key]); E.render(); });
    await page.waitForTimeout(200);

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
    check(/#ifdef JWPLC_UI_HAS_BOOL_STATE_BACKGROUNDS\n\s*JWPLC_UIBoolText\(\s*"PARADO",\s*"MARCHA",\s*0xF800,\s*0x07E0,\s*0x0000,\s*0x001F\)/.test(h), 'BOOL: texto y fondo por estado con respaldo para packages anteriores');

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

    console.log('ANCLAJE 1×..4×');
    const anchor = await page.evaluate(() => {
      const E = window.JWPLCHMIEditor;
      E.addValueField();
      const f = E.getSelectedField();
      const out = [];
      for (const labelSize of [1, 2, 3, 4]) {
        for (const valueSize of [1, 2, 3, 4]) {
          for (const layout of ['INLINE', 'STACKED']) {
            Object.assign(f, { labelSize, valueSize, layout, x: 40, y: 30 });
            const g = E.computeSelectedGeometry();
            const labelH = 7 * labelSize;
            const expectedY = layout === 'INLINE' ? 33 : 33 + labelH + 4;
            if (g.valueY !== expectedY) out.push(`${layout} L${labelSize} V${valueSize}: valueY=${g.valueY} (esperado ${expectedY})`);
          }
        }
      }
      E.deleteSelectedField();
      return out;
    });
    check(anchor.length === 0, `etiqueta y valor no se desplazan entre 1× y 4×${anchor.length ? `: ${anchor.slice(0, 3).join(' | ')}` : ''}`);

    // El fondo del texto (celda 6×8 completa, con descendentes "gy") no debe
    // borrar ningún píxel del marco en ninguna combinación de tamaños.
    console.log('MARCO 1×..4×');
    const frameCut = await page.evaluate(() => {
      const E = window.JWPLCHMIEditor;
      const FRAME = 0xFFE0;
      const base = { x: 10, y: 10, frame: true, frameColor: FRAME, labelColor: 0xFFFF, valueColor: 0x07FF, backgroundColor: 0x0000, label: 'Lgy', unit: 'Cy', align: 'LEFT' };
      const kinds = [
        { type: 'TEXT', capacity: 4, preview: 'Mgjy' },
        { type: 'VALUE', integerDigits: 3, decimalDigits: 1, preview: '888.8' },
        { type: 'BOOL', falseText: 'gyOFF', trueText: 'ON', boolValue: false, preview: 'gyOFF' },
        { type: 'BOOL', falseText: 'OFF', trueText: 'gyON', boolValue: true, preview: 'gyON', stateColors: true, falseColor: 0xFFFF, trueColor: 0x0000, falseBackground: 0xF800, trueBackground: 0x07E0 }
      ];
      const out = [];
      for (const kind of kinds) {
        for (const labelSize of [1, 2, 3, 4]) {
          for (const valueSize of [1, 2, 3, 4]) {
            for (const layout of ['INLINE', 'STACKED']) {
              const r = E.renderFieldPreview({ ...base, ...kind, labelSize, valueSize, layout });
              const { fieldX, fieldY, fieldW, fieldH } = r.g;
              let bad = 0;
              for (let x = fieldX; x < fieldX + fieldW; x++) {
                if (r.buffer[fieldY * r.width + x] !== FRAME) bad++;
                if (r.buffer[(fieldY + fieldH - 1) * r.width + x] !== FRAME) bad++;
              }
              for (let y = fieldY; y < fieldY + fieldH; y++) {
                if (r.buffer[y * r.width + fieldX] !== FRAME) bad++;
                if (r.buffer[y * r.width + fieldX + fieldW - 1] !== FRAME) bad++;
              }
              if (bad) out.push(`${kind.type} ${layout} L${labelSize} V${valueSize}: ${bad} px`);
            }
          }
        }
      }
      return out;
    });
    check(frameCut.length === 0, `el marco queda completo en TEXT/VALUE/BOOL de 1× a 4×${frameCut.length ? `: ${frameCut.slice(0, 4).join(' | ')}` : ''}`);

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
    for (const tab of ['shape', 'content', 'appearance', 'placement']) {
      await page.click(`#barInspectorWrap .bar-sub-tab[data-sub-tab="${tab}"]`);
      await shot(`bar_${tab}`);
    }
    for (const tab of ['plc', 'tech']) {
      await page.click(`#barInspectorWrap .bar-main-tab[data-main-tab="${tab}"]`);
      await shot(`bar_${tab}`);
    }
    await page.click('#barInspectorWrap .bar-main-tab[data-main-tab="design"]');

    console.log('BAR · APARIENCIA (mini-modelo + indicador A1)');
    await page.click('#barInspectorWrap .bar-sub-tab[data-sub-tab="appearance"]');
    await page.waitForTimeout(250);
    const barApp = await page.evaluate(() => {
      const card = document.getElementById('barAppMini');
      const canvas = card.querySelector('[data-bar-case]');
      return {
        legacyHidden: document.getElementById('barAppLegacyCard').offsetParent === null,
        canvas: canvas ? canvas.width > 0 && canvas.height > 0 : false,
        rows: [...card.querySelectorAll('[data-bar-legend]')].map((r) => r.dataset.barLegend).join(','),
        marker: card.querySelectorAll('svg.zone-indicator path.zone-marker').length,
        active: card.querySelector('.menu-legend-row.is-active')?.dataset.barLegend
      };
    });
    check(barApp.legacyHidden && barApp.canvas && barApp.rows === 'label,value,track' && barApp.marker === 1 && barApp.active === 'value',
      `BAR Apariencia: mini-modelo, leyenda Etiqueta/Barra/Fondo y ▶ en la barra (${JSON.stringify(barApp)})`);
    await page.$eval('#barAppMini [data-bar-swatch="track"]', (el) => el.dispatchEvent(new MouseEvent('click', { bubbles: true })));
    // El texto de la fila también elige el color; el editor sigue a la vista.
    await page.click('#barAppMini [data-bar-legend="label"] .menu-legend-label');
    const barRow = await page.evaluate(() => document.querySelector('#barAppMini .menu-legend-row.is-active')?.dataset.barLegend);
    check(barRow === 'label', `BAR: clic en el texto de la fila elige su color (${barRow})`);
    await page.$eval('#barAppMini [data-bar-swatch="track"]', (el) => el.dispatchEvent(new MouseEvent('click', { bubbles: true })));
    await page.click('#barAppMini [data-bar-palette="63488"]');
    await page.waitForTimeout(200);
    const barAfter = await page.evaluate(() => {
      const f = window.JWPLCHMIEditor.getSelectedField();
      return { track: f.trackColor, bg: f.backgroundColor, active: document.querySelector('#barAppMini .menu-legend-row.is-active')?.dataset.barLegend };
    });
    check(barAfter.track === 0xF800 && barAfter.bg === 0xF800 && barAfter.active === 'track', `BAR: la paleta pinta fondo y pista (un solo color en firmware) (${JSON.stringify(barAfter)})`);
    const barShot = await page.$('#barInspectorWrap');
    if (barShot) await barShot.screenshot({ path: path.join(OUT, 'bar_apariencia_a1.png') });
    await page.evaluate(() => window.JWPLCHMIEditor.undo());

    const barUi = await page.evaluate(() => {
      const wrap = document.getElementById('barInspectorWrap');
      const visible = (el) => el && el.offsetParent !== null;
      return {
        verticalDisabled: wrap.querySelector('.bar-orient-btn[data-orient="VERTICAL"]').disabled,
        simInShape: visible(wrap.querySelector('#barSubpanelShape .bar-top-slider-wrap')),
        heightLocked: wrap.querySelector('#barFormaHeight').disabled,
        dataTab: wrap.querySelector('.bar-main-tab[data-main-tab="plc"]').textContent.trim(),
        contract: wrap.querySelector('#barTechContractCode').textContent
      };
    });
    check(barUi.verticalDisabled && barUi.heightLocked, 'BAR-3/BAR-4: vertical deshabilitada y alto fijo');
    check(!barUi.simInShape, 'BAR-1: simulación fuera de Forma');
    check(barUi.dataTab === 'Datos', 'BAR-5: pestaña "Datos"');
    check(/\n/.test(barUi.contract) && /setBar\(/.test(barUi.contract), 'BAR-6: contrato multilínea con setBar');

    // Paridad lienzo/TFT: traducción literal de computeFieldGeometry()
    // (JWPLC_Display/src/JWPLC_UI.cpp) para JWPLC_UI_FIELD_BAR con ancho fijo.
    const parity = await page.evaluate(() => {
      const PAD = 3; const GAP = 4; const BAR_H = 12;
      const bounds = (text, size) => (text ? { w: text.length * 6 * size - size, h: 7 * size } : { w: 0, h: 0 });
      const firmware = (f) => {
        const lead = PAD; // margen superior/izquierdo fijo
        const pad = Math.max(PAD, f.labelSize + 1); // derecha/abajo (celda 6×8 + marco)
        const label = bounds(f.showLabel ? f.barLabel : '', f.labelSize);
        const unit = bounds(f.barUnit, f.labelSize);
        let valueW = f.barWidth - (lead + pad);
        if (f.layout === 'INLINE' && label.w > 0) valueW -= label.w + GAP;
        if (unit.w > 0) valueW -= unit.w + GAP;
        valueW = Math.max(1, valueW);
        const valueH = BAR_H;
        let fieldH; let valueX; let valueY;
        if (f.layout === 'STACKED') {
          fieldH = (lead + pad) + label.h + (label.h > 0 ? GAP : 0) + Math.max(valueH, unit.h);
          valueX = f.x + lead;
          valueY = f.y + lead + label.h + (label.h > 0 ? GAP : 0);
        } else {
          fieldH = (lead + pad) + Math.max(label.h, valueH, unit.h);
          valueX = f.x + lead + label.w + (label.w > 0 ? GAP : 0);
          valueY = f.y + lead;
        }
        return { fieldW: f.barWidth, fieldH, valueX, valueY, valueW, valueH, unitX: valueX + valueW + GAP, unitY: valueY };
      };
      const E = window.JWPLCHMIEditor;
      const field = E.getSelectedField();
      const mismatches = [];
      let cases = 0;
      for (const layout of ['INLINE', 'STACKED']) {
        for (const showLabel of [true, false]) {
          for (const barUnit of ['%', '']) {
            for (const labelSize of [1, 2, 4]) {
              Object.assign(field, { layout, showLabel, barLabel: 'Nivel', barUnit, labelSize, barWidth: 140, x: 10, y: 20 });
              const g = window.JWPLCHMIBar.computeGeometry(field);
              const fw = firmware(field);
              cases += 1;
              for (const key of ['fieldW', 'fieldH', 'valueX', 'valueY', 'valueW', 'valueH']) {
                if (g[key] !== fw[key]) mismatches.push(`${layout}/${showLabel}/${barUnit || '-'}/${labelSize}: ${key} lienzo=${g[key]} tft=${fw[key]}`);
              }
              if (barUnit && (g.unitX !== fw.unitX || g.unitY !== fw.unitY)) mismatches.push(`${layout}/${labelSize}: unidad lienzo=${g.unitX},${g.unitY} tft=${fw.unitX},${fw.unitY}`);
              if (showLabel && (g.labelX !== field.x + PAD || g.labelY !== field.y + PAD)) mismatches.push(`${layout}/${labelSize}: etiqueta lienzo=${g.labelX},${g.labelY}`);
            }
          }
        }
      }
      return { cases, mismatches };
    });
    check(parity.mismatches.length === 0, `BAR: geometría del lienzo == firmware en ${parity.cases} casos${parity.mismatches.length ? `\n      ${parity.mismatches.join('\n      ')}` : ''}`);

    console.log('PANTALLA IDLE');
    await page.evaluate(() => { window.JWPLCHMIEditor.setSelectedFieldKeys([]); window.JWPLCHMIEditor.render(); });
    await page.waitForTimeout(200);
    const I = '#idleConfigSection ';
    check(await page.$eval('#idleConfigSection', (el) => !el.hidden), 'sin selección se muestra "Pantalla y arranque"');
    await page.click(`${I}[data-idle-set="start"][data-value="HMI"]`);
    await page.click(`${I}[data-idle-set="wake"][data-value="BUTTON"]`);
    await page.click(`${I}[data-idle-key="wake"][data-value="BTN_UP"]`);
    await page.click(`${I}[data-idle-set="back"][data-value="TIMEOUT"]`);
    await page.click(`${I}[data-idle-set="timeoutS"][data-value="60"]`);
    const idleShot = await page.$('#idleConfigSection');
    if (idleShot) await idleShot.screenshot({ path: path.join(OUT, 'idle_config.png') });
    const idleHeader = await header();
    check(/setIdleWakeMode\(JWPLC_DisplayClass::IDLE_WAKE_BUTTON_ONLY\);\n\s*JWPLC_Display\.setIdleWakeButton\(BTN_UP\);\n\s*JWPLC_Display\.setIdleReturnMode\(JWPLC_DisplayClass::IDLE_RETURN_TIMEOUT\);\n\s*JWPLC_Display\.setIdleTimeoutMs\(60000UL\);\n\s*JWPLC_Display\.enterUserUI\(\);\n\}/.test(idleHeader), 'IDLE: la configuración llega a jwplcHMISetup()');
    const idleRound = await page.evaluate(async () => {
      const P = window.JWPLCHMIProject;
      const saved = P.serialize();
      await P.apply({ ...saved, display: null });
      const reset = window.JWPLCHMIIdle.get();
      await P.apply(saved);
      return { saved: saved.display, reset, loaded: window.JWPLCHMIIdle.get() };
    });
    check(idleRound.saved.wakeButton === 'BTN_UP' && idleRound.loaded.timeoutS === 60 && idleRound.loaded.start === 'HMI', 'IDLE: se guarda y se abre con el proyecto');
    check(idleRound.reset.wake === 'ANY' && idleRound.reset.back === 'ESC', 'IDLE: un proyecto sin configuración toma los valores por defecto');

    console.log('BORRAR PÁGINA');
    const pagesBefore = await page.evaluate(() => { const E = window.JWPLCHMIEditor; E.addPage(); E.addPage(); return E.getPages().length; });
    await page.waitForTimeout(300);
    const delButtons = await page.$$('.page-delete-btn');
    await delButtons[delButtons.length - 1].click();
    const afterOne = await page.evaluate(() => window.JWPLCHMIEditor.getPages().length);
    await page.click('.page-delete-btn.is-armed');
    await page.waitForTimeout(200);
    const afterTwo = await page.evaluate(() => window.JWPLCHMIEditor.getPages().length);
    check(afterOne === pagesBefore && afterTwo === pagesBefore - 1, `borrar página: el primer clic pide confirmación y el segundo borra (${pagesBefore} → ${afterOne} → ${afterTwo})`);

    console.log('NAV · CONTADOR');
    await page.evaluate(() => document.querySelector('[data-tool="navBarField"]').click());
    await page.waitForTimeout(300);
    await page.click('#navBarInspectorSection [data-navbar-preset="counter"]');
    await page.click('#navBarInspectorSection [data-navbar-corner="bl"]');
    await page.waitForTimeout(200);
    const tabsHidden = await page.$eval('#navBarInspectorSection [data-navbar-pos]', (el) => el.offsetParent === null);
    const counter = await page.evaluate(async () => {
      const cfg = window.JWPLCHMIPages.getNavBarConfig();
      // Sin selección: el recuadro de selección no tapa el borde del contador.
      window.JWPLCHMIEditor.setSelectedFieldKeys([]);
      window.JWPLCHMIEditor.render();
      await new Promise((r) => setTimeout(r, 100));
      const c = document.getElementById('displayCanvas');
      const z = c.width / 320;
      const px = (x, y) => [...c.getContext('2d').getImageData(Math.round(x * z), Math.round(y * z), 1, 1).data].slice(0, 3).join(',');
      return { cfg, border: px(2, 160) };
    });
    check(counter.cfg.preset === 'counter' && counter.cfg.width === 36 && counter.cfg.height === 12 && counter.cfg.x === 2 && counter.cfg.y === 155, 'NAV Contador: caja 36×12 en la esquina elegida');
    check(counter.border === '255,255,255' && tabsHidden, 'NAV Contador: el lienzo dibuja el NN/TT y se ocultan las opciones de pestañas');
    const counterHeader = await header();
    check(/#ifdef JWPLC_UI_HAS_PAGE_INDICATOR_CONFIG\n\s*JWPLC_Display\.setPageIndicator\(true, 2, 155\);\n#endif/.test(counterHeader) && !/jwplcDrawPageNavBar/.test(counterHeader), 'NAV Contador: el header usa el indicador del firmware y no dibuja barra');

    console.log('NAV · PESTAÑAS (estados y color)');
    await page.click('#navBarInspectorSection [data-navbar-preset="industrial-color"]');
    await page.waitForTimeout(200);
    const navPages = await page.evaluate(() => window.JWPLCHMIEditor.getPages().length);
    if (navPages > 1) {
      await page.click('#navBarInspectorSection [data-nav-state="selected"]');
      const navMode = await page.evaluate(() => window.JWPLCHMIPages.getNavigationMode());
      await page.click('#navBarInspectorSection [data-nav-state="positioned"]');
      check(navMode === 'CONTENT' && (await page.evaluate(() => window.JWPLCHMIPages.getNavigationMode())) === 'SELECT',
        'NAV: la tarjeta «Pestaña de la página actual» cambia el lienzo entre posicionado y seleccionado');
      await page.click('#navBarInspectorSection [data-navbar-tab="pages"]');
      await page.click('#navBarInspectorPagesList [data-navpage-row="1"] .hmi-nav-page-name');
      await page.fill('#navBarPanePages [data-ce-hex]', '#ff8000');
      await page.press('#navBarPanePages [data-ce-hex]', 'Enter');
      const navColor = await page.evaluate(() => window.JWPLCHMIEditor.getPages()[1].color);
      await page.click('#navBarPanePages [data-navpage-palette="2016"]');
      const navPill = await page.evaluate(() => window.JWPLCHMIEditor.getPages()[1].color);
      await page.evaluate(() => window.JWPLCHMIEditor.undo());
      const navUndo = await page.evaluate(() => window.JWPLCHMIEditor.getPages()[1].color);
      check(navColor === 0xFC00 && navPill === 0x07E0 && navUndo === 0xFC00,
        `NAV · Páginas: el editor fijo cambia el color de la pestaña elegida (HEX, paleta y deshacer: ${[navColor, navPill, navUndo].map((v) => v?.toString(16))})`);
      const navPane = await page.$('#navBarPanePages');
      if (navPane) await navPane.screenshot({ path: path.join(OUT, 'nav_paginas_color.png') });
    } else {
      check(false, 'NAV: el proyecto de la prueba necesita 2 páginas o más');
    }

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
