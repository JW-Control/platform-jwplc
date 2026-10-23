#!/usr/bin/env node
// Iconos con estado en el Designer (Alpha16).
//
// 1. Biblioteca: categorías, búsqueda, clic y arrastre al lienzo agregan un ICON
//    en un hueco libre; el inspector ICON reemplaza al de campos.
// 2. Inspector: estado de vista previa, tamaño, etiqueta, tipo de variable
//    (aviso con bool y más de 2 estados) y deshacer.
// 3. Guardar/abrir: el .jwhmi lleva iconAssets y un icono que no está en el
//    catálogo se abre, se dibuja y se genera desde esa copia.
// 4. Con g++: el header generado se dibuja con el JWPLC_UI.cpp real
//    (icon-host/header_sim.cpp) y el framebuffer es igual píxel a píxel al
//    lienzo que el Designer envía por LIVE (#previewCanvas).
//
//   node tools/jwplc-hmi-designer/tests/icon-smoke.cjs [carpeta-salida]
// Requiere: playwright (local o global) con Chromium (+ g++ opcional).
'use strict';

const fs = require('fs');
const os = require('os');
const path = require('path');
const http = require('http');
const { execFileSync } = require('child_process');

const POC = path.resolve(__dirname, '..', 'poc');
const OUT = path.resolve(process.argv[2] || fs.mkdtempSync(path.join(os.tmpdir(), 'jwplc-icon-smoke-')));

function loadPlaywright() {
  try { return require('playwright'); } catch (_) { /* global */ }
  const globalRoot = execFileSync(process.platform === 'win32' ? 'npm.cmd' : 'npm', ['root', '-g'], { shell: process.platform === 'win32' }).toString().trim();
  return require(path.join(globalRoot, 'playwright'));
}

function hasTool(cmd, args) {
  try { execFileSync(cmd, args, { stdio: 'ignore' }); return true; } catch (_) { return false; }
}
const HAS_GPP = hasTool('g++', ['--version']);

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

let failures = 0;
function check(ok, what) {
  console.log(`  ${ok ? 'OK  ' : 'FAIL'} ${what}`);
  if (!ok) failures += 1;
}

const COLORS = { labelColor: 0xFFFF, valueColor: 0xFFFF, backgroundColor: 0x0000, frameColor: 0xFFFF };
const ICON = (extra) => ({
  ...COLORS, type: 'ICON', unit: '', preview: '', capacity: 1, valueSize: 1, labelSize: 1, frame: false,
  layout: 'STACKED', align: 'LEFT', page: 0, label: '', iconVarType: 'bool', iconState: 0, ...extra
});

// Paridad: tamaños 24..64, etiqueta arriba/izquierda, marco, fondo, estados
// que cambian sólo la paleta y estados con otra imagen.
const PARITY = [
  ICON({ name: 'Motor', id: 'ICON_MOTOR', variable: 'motor', iconId: 'motores/motor', iconSize: 32, x: 4, y: 4, iconState: 1, iconVarType: 'uint8' }),
  ICON({ name: 'Valvula', id: 'ICON_VALVULA', variable: 'valvula', iconId: 'valvulas/valvula', iconSize: 48, x: 50, y: 4, iconState: 2, iconVarType: 'uint8', label: 'Válvula', frame: true, backgroundColor: 0x0010, frameColor: 0x07FF, labelColor: 0xFFE0 }),
  ICON({ name: 'Tanque', id: 'ICON_TANQUE', variable: 'tanque', iconId: 'equipos/tanque', iconSize: 64, x: 120, y: 4, iconState: 2, iconVarType: 'uint8', label: 'T1', layout: 'INLINE', labelSize: 2 }),
  ICON({ name: 'Piloto', id: 'ICON_PILOTO', variable: 'piloto', iconId: 'pilotos/piloto', iconSize: 24, x: 4, y: 120, iconState: 3, iconVarType: 'uint8' }),
  ICON({ name: 'Pulsador', id: 'ICON_PULSADOR', variable: 'pulsador', iconId: 'botones/pulsador', iconSize: 24, x: 40, y: 120, iconState: 1, backgroundColor: 0x2104 }),
  ICON({ name: 'Sensor', id: 'ICON_SENSOR', variable: 'sensor', iconId: 'sensores/proximidad', iconSize: 32, x: 230, y: 110, iconState: 1, label: 'S1' }),
  // Sin fondo sobre un RECT: se ve el RECT fuera de la silueta común de los estados.
  ICON({ name: 'Bomba', id: 'ICON_BOMBA', variable: 'bomba', iconId: 'bombas/bomba', iconSize: 32, x: 140, y: 100, iconState: 1, iconVarType: 'uint8', label: 'B1', transparentBackground: true, labelColor: 0x0000 }),
  ICON({ name: 'Valvula2', id: 'ICON_VALVULA2', variable: 'valvula2', iconId: 'valvulas/valvula', iconSize: 24, x: 186, y: 112, iconState: 0, iconVarType: 'uint8', transparentBackground: true, backgroundColor: 0x001F })
].map((f, i) => ({ ...f, layerOrder: i }));
const PARITY_RECT = { type: 'RECT', name: 'Placa', key: 'rect-paridad', x: 130, y: 96, x2: 222, y2: 150, fill: true, fillColor: 0xFD20, frameColor: 0xFD20, layerOrder: 90 };

async function main() {
  fs.mkdirSync(OUT, { recursive: true });
  const server = await serve();
  const { chromium } = loadPlaywright();
  const browser = await chromium.launch();
  const page = await browser.newPage({ viewport: { width: 1600, height: 1000 } });
  const errors = [];
  page.on('pageerror', (e) => errors.push(e.message));

  try {
    await page.goto(`http://127.0.0.1:${server.address().port}/desktop.html`);
    await page.waitForFunction(() => window.JWPLCHMIProject?.apply && window.JWPLCHMIIcons && window.JWPLCHMICodegen?.refresh, null, { timeout: 20000 });
    await page.waitForTimeout(800);
    const icons = () => page.evaluate(() => window.JWPLCHMIEditor.getAllFields().filter((f) => f.type === 'ICON').map((f) => ({ ...f })));
    const header = () => page.evaluate(async () => {
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
      return '';
    });
    const L = '#iconDropdownMenu';
    const openMenu = async () => {
      if (!(await page.locator(L).isVisible())) await page.click('#iconToolBtn');
    };
    const S = '#iconInspectorSection';

    console.log('BIBLIOTECA');
    const total = await page.evaluate(() => window.JWPLCIconCatalog.icons.length);
    check(await page.evaluate(() => Boolean(document.querySelector('.canvas-toolbar #iconToolBtn')) && !document.querySelector('.left-panel [data-icons-grid]')),
      'la biblioteca está en la barra del lienzo, no en el panel izquierdo');
    check(!(await page.locator(L).isVisible()), 'el menú Iconos empieza cerrado');
    await openMenu();
    await page.click(`${L} [data-icons-cat=""]`);
    // Alto fijo que cabe en el lienzo (pantalla baja) y rueda sin mover el lienzo.
    await page.setViewportSize({ width: 1366, height: 640 });
    await page.click('#iconToolBtn'); await page.click('#iconToolBtn');
    const geo = async () => page.evaluate(() => {
      const m = document.getElementById('iconDropdownMenu').getBoundingClientRect();
      const v = document.getElementById('canvasViewport').getBoundingClientRect();
      return { h: Math.round(m.height), bottom: m.bottom, limit: Math.min(window.innerHeight, v.bottom) };
    });
    const gAll = await geo();
    check(gAll.bottom <= gAll.limit, `el menú cabe sobre el borde del lienzo (fondo ${Math.round(gAll.bottom)} <= ${Math.round(gAll.limit)})`);
    await page.click(`${L} [data-icons-cat="valvulas"]`);
    const gValves = await geo();
    check(gValves.h === gAll.h, `el alto no cambia con la categoría (${gAll.h} / ${gValves.h} px)`);
    await page.click(`${L} [data-icons-cat=""]`);
    const pan0 = await page.evaluate(() => ({ ...window.JWPLCHMIEditor.getPan() }));
    await page.hover(`${L} [data-icons-grid]`);
    await page.mouse.wheel(0, 300);
    await page.waitForTimeout(250);
    const afterGrid = await page.evaluate(() => ({ pan: { ...window.JWPLCHMIEditor.getPan() }, top: document.querySelector('#iconDropdownMenu [data-icons-grid]').scrollTop }));
    check(afterGrid.top > 0 && afterGrid.pan.x === pan0.x && afterGrid.pan.y === pan0.y, `la rueda desplaza la grilla y no el lienzo (scrollTop ${afterGrid.top})`);
    const cbox = await page.locator('#canvasViewport').boundingBox();
    await page.mouse.move(cbox.x + cbox.width - 40, cbox.y + cbox.height - 40);
    await page.mouse.wheel(0, 300);
    await page.waitForTimeout(150);
    const pan1 = await page.evaluate(() => ({ ...window.JWPLCHMIEditor.getPan() }));
    check(pan1.x === pan0.x && pan1.y === pan0.y, 'con el menú abierto la rueda sobre el lienzo no lo mueve');
    await page.setViewportSize({ width: 1600, height: 1000 });
    check(await page.locator(`${L} [data-icon-id]`).count() === total, `«Todos» muestra los ${total} iconos`);
    await page.click(`${L} [data-icons-cat="valvulas"]`);
    const valves = await page.locator(`${L} [data-icon-id]`).evaluateAll((els) => els.map((e) => e.dataset.iconId));
    check(valves.length >= 3 && valves.every((id) => id.startsWith('valvulas/')), `categoría Válvulas (${valves.length})`);
    await page.fill(`${L} [data-icons-search]`, 'motor');
    const found = await page.locator(`${L} [data-icon-id]`).evaluateAll((els) => els.map((e) => e.dataset.iconId));
    check(found.includes('motores/motor') && found.includes('valvulas/valvula_motorizada'), 'buscar «motor» cruza categorías');
    await page.fill(`${L} [data-icons-search]`, '');
    await page.click(`${L} [data-icons-cat="motores"]`);
    await page.click(`${L} [data-icons-pick-size="48"]`);
    check(await page.locator(L).isVisible(), 'categorías, búsqueda y tamaño no cierran el menú');
    await page.click(`${L} [data-icon-id="motores/motor"]`);
    await page.waitForTimeout(200);
    check(!(await page.locator(L).isVisible()), 'agregar un icono cierra el menú');
    let list = await icons();
    check(list.length === 1 && list[0].iconId === 'motores/motor' && list[0].iconSize === 48, 'clic agrega un ICON del tamaño elegido (48 px)');
    check(list[0].iconVarType === 'uint8', 'icono de 3 estados empieza con uint8_t');
    const visible = await page.evaluate(() => ({
      icon: !document.getElementById('iconInspectorSection').hidden,
      field: document.getElementById('textFieldControlsSection').hidden
    }));
    check(visible.icon && visible.field, 'se muestra el inspector ICON (y no el de campos)');
    await openMenu();
    await page.keyboard.press('Escape');
    check(!(await page.locator(L).isVisible()), 'Escape cierra el menú');
    await openMenu();
    await page.click(`${L} [data-icon-id="motores/motor"]`);
    await page.waitForTimeout(200);
    const overlap = await page.evaluate(() => {
      const [a, b] = window.JWPLCHMIEditor.getAllFields().filter((f) => f.type === 'ICON').map((f) => window.JWPLCHMIEditor.getFieldBounds(f));
      return !(a.maxX < b.minX || b.maxX < a.minX || a.maxY < b.minY || b.maxY < a.minY);
    });
    check(!overlap, 'el segundo icono va a un hueco libre');

    // Arrastrar al lienzo.
    await openMenu();
    await page.click(`${L} [data-icons-cat="pilotos"]`);
    await page.click(`${L} [data-icons-pick-size="24"]`);
    const box = await page.locator('#displayCanvas').boundingBox();
    await page.dragAndDrop(`${L} [data-icon-id="pilotos/piloto"]`, '#displayCanvas', {
      targetPosition: { x: box.width * 200 / 320, y: box.height * 100 / 170 },
      // El menú tapa el punto de destino; al arrastrar deja pasar el puntero
      // (pointer-events: none), cosa que la comprobación previa de Playwright no ve.
      force: true
    });
    await page.waitForTimeout(200);
    list = await icons();
    const dropped = list.find((f) => f.iconId === 'pilotos/piloto');
    check(Boolean(dropped) && Math.abs(dropped.x - 200) <= 2 && Math.abs(dropped.y - 100) <= 2, `arrastrar suelta el icono donde se suelta (${dropped?.x}, ${dropped?.y})`);

    console.log('INSPECTOR');
    await page.click(`${S} [data-icon-sub="content"]`);
    await page.click(`${S} [data-icon-state="2"]`);
    let sel = await page.evaluate(() => ({ ...window.JWPLCHMIEditor.getSelectedField() }));
    check(sel.iconState === 2, 'clic en un estado lo muestra en el lienzo');
    await page.fill(`${S} [data-icon-prop="label"]`, 'P1');
    await page.locator(`${S} [data-icon-prop="label"]`).dispatchEvent('change');
    await page.click(`${S} [data-icon-set="layout"][data-value="INLINE"]`);
    sel = await page.evaluate(() => ({ ...window.JWPLCHMIEditor.getSelectedField() }));
    check(sel.label === 'P1' && sel.layout === 'INLINE', 'etiqueta y posición');
    await page.click(`${S} [data-icon-sub="shape"]`);
    await page.click(`${S} [data-icon-set="transparentBackground"][data-value="1"]`);
    sel = await page.evaluate(() => ({ ...window.JWPLCHMIEditor.getSelectedField() }));
    check(sel.transparentBackground === true, 'Fondo: «Sin fondo»');
    check(/JWPLC_UIColors\([^;]*?\),\n {8}true\)/.test(await header()), 'sin fondo: el header pasa true a JWPLC_UIIconField');
    // header() deja el lienzo sin selección: se vuelve a elegir el icono.
    await page.evaluate((key) => { const E = window.JWPLCHMIEditor; E.setSelectedFieldKeys([key]); E.render(); window.JWPLCHMIInspectorState?.sync(); window.JWPLCHMIIcons?.sync(); }, sel.key);
    await page.waitForTimeout(150);
    await page.click(`${S} [data-icon-main="design"]`);
    await page.click(`${S} [data-icon-sub="shape"]`);
    await page.click(`${S} [data-icon-set="transparentBackground"][data-value="0"]`);
    await page.click(`${S} [data-icon-size="48"]`);
    sel = await page.evaluate(() => ({ ...window.JWPLCHMIEditor.getSelectedField(), w: window.JWPLCHMIEditor.getFieldBounds(window.JWPLCHMIEditor.getSelectedField()).width }));
    check(sel.iconSize === 48 && sel.w > 48, `tamaño 48 px (recuadro ${sel.w} px con etiqueta)`);
    await page.click(`${S} [data-icon-main="data"]`);
    await page.click(`${S} [data-icon-set="iconVarType"][data-value="bool"]`);
    check(await page.locator(`${S} [data-icon-warn]`).isVisible(), 'aviso: bool con 4 estados sólo alcanza 2');
    await page.click(`${S} [data-icon-set="iconVarType"][data-value="uint8"]`);
    check(!(await page.locator(`${S} [data-icon-warn]`).isVisible()), 'con uint8_t no hay aviso');
    await page.keyboard.press('Control+z');
    await page.waitForTimeout(150);
    sel = await page.evaluate(() => ({ ...window.JWPLCHMIEditor.getSelectedField() }));
    check(sel.iconVarType === 'bool', 'deshacer vuelve al tipo anterior');

    const text = await header();
    check(/JWPLC_UIIconField\(\n {8}FIELD_ICON_\d+,/.test(text) && text.includes('#ifndef JWPLC_UI_HAS_ICON'), 'header con JWPLC_UIIconField y guarda de versión');
    check(text.includes('JWPLC_Display.setIconState(') && /uint8_t \w+ = 0; \/\/ ICON: 0=\S+/.test(text), 'variable uint8_t y setIconState en jwplcUIUpdate()');
    check((text.match(/static const JWPLC_UIIcon HMI_ICON_MOTORES_MOTOR_48\(/g) || []).length === 1, 'dos ICON del mismo icono y tamaño comparten un solo JWPLC_UIIcon');

    console.log('GUARDAR / ABRIR');
    const saved = await page.evaluate(() => window.JWPLCHMIProject.serialize());
    check(saved.iconAssets && Object.keys(saved.iconAssets).includes('motores/motor@48'), 'el .jwhmi guarda iconAssets de los iconos usados');
    // Un icono que ya no está en el catálogo se abre desde su copia.
    const foreign = JSON.parse(JSON.stringify(saved));
    const asset = foreign.iconAssets['motores/motor@48'];
    foreign.iconAssets = { 'propio/bomba_vieja@48': { ...asset, name: 'Bomba vieja', category: 'propio' } };
    foreign.fields = [ICON({ name: 'Vieja', id: 'ICON_VIEJA', variable: 'vieja', iconId: 'propio/bomba_vieja', iconSize: 48, x: 10, y: 10, iconState: 1 })];
    const reopened = await page.evaluate(async (data) => {
      await window.JWPLCHMIProject.apply(data);
      await new Promise((r) => setTimeout(r, 300));
      const f = window.JWPLCHMIEditor.getAllFields().find((x) => x.type === 'ICON');
      return { w: window.JWPLCHMIEditor.getFieldBounds(f).width, again: Object.keys(window.JWPLCHMIProject.serialize().iconAssets || {}) };
    }, foreign);
    check(reopened.w === 48 + 3 + 3 && reopened.again.includes('propio/bomba_vieja@48'), 'icono fuera del catálogo: se dibuja y se vuelve a guardar');
    check((await header()).includes('static const JWPLC_UIIcon HMI_ICON_PROPIO_BOMBA_VIEJA_48('), 'y se genera desde la copia');

    console.log('PARIDAD LIENZO (LIVE) / FIRMWARE');
    const parity = await page.evaluate(async ([fields, rect]) => {
      const P = window.JWPLCHMIProject;
      const s = P.serialize();
      s.pages = [{ id: 0, name: 'Principal' }];
      s.fields = fields.concat([rect]);
      s.navigationBar = null;
      delete s.iconAssets;
      await P.apply(s);
      await new Promise((r) => setTimeout(r, 300));
      const E = window.JWPLCHMIEditor;
      E.setSelectedFieldKeys([]);
      E.render();
      await new Promise((r) => setTimeout(r, 200));
      const canvas = document.getElementById('previewCanvas');
      const rgba = canvas.getContext('2d').getImageData(0, 0, 320, 170).data;
      const frame = [];
      for (let i = 0; i < 320 * 170; i += 1) frame.push(((rgba[i * 4] >> 3) << 11) | ((rgba[i * 4 + 1] >> 2) << 5) | (rgba[i * 4 + 2] >> 3));
      return { frame, states: E.getAllFields().filter((f) => f.type === 'ICON').map((f) => f.iconState) };
    }, [PARITY, PARITY_RECT]);
    const parityHeader = await header();
    fs.writeFileSync(path.join(OUT, 'JWPLC_HMI_Generated.h'), parityHeader);
    fs.writeFileSync(path.join(OUT, 'iconos_paridad.jwhmi'), JSON.stringify(await page.evaluate(() => window.JWPLCHMIProject.serialize()), null, 2));
    if (!HAS_GPP) {
      console.log('  SKIP sin g++: no se dibuja el header con el firmware');
    } else {
      const { buildSim } = require('./icon-host-sim.cjs');
      const { fontHeader } = require('./host-sim-lib.cjs');
      const gen = path.join(OUT, 'gen');
      fs.mkdirSync(gen, { recursive: true });
      fs.writeFileSync(path.join(gen, 'font_data.h'), fontHeader());
      fs.copyFileSync(path.join(OUT, 'JWPLC_HMI_Generated.h'), path.join(gen, 'JWPLC_HMI_Generated.h'));
      const exe = path.join(OUT, process.platform === 'win32' ? 'header_sim.exe' : 'header_sim');
      buildSim(path.join(__dirname, 'icon-host', 'header_sim.cpp'), exe, [gen]);
      check(true, 'el header generado compila con -Wall -Werror contra el JWPLC_UI.cpp real');
      const raw = path.join(OUT, 'firmware.rgb565');
      execFileSync(exe, [raw, ...parity.states.map(String)]);
      const buf = fs.readFileSync(raw);
      let diff = 0;
      let first = -1;
      for (let i = 0; i < 320 * 170; i += 1) {
        // RECT: el lienzo incluye la columna x2 y la fila y2 y el codegen no
        // (fillRect de x2 - x): diferencia previa a los iconos, fuera de esta prueba.
        const x = i % 320;
        const y = Math.floor(i / 320);
        const rectEdge = (x === PARITY_RECT.x2 && y >= PARITY_RECT.y && y <= PARITY_RECT.y2) || (y === PARITY_RECT.y2 && x >= PARITY_RECT.x && x <= PARITY_RECT.x2);
        if (rectEdge) continue;
        if (buf.readUInt16LE(i * 2) !== parity.frame[i]) { diff += 1; if (first < 0) first = i; }
      }
      check(diff === 0, `lienzo == framebuffer del firmware${diff ? ` (${diff} px distintos, primero en x=${first % 320} y=${Math.floor(first / 320)})` : ' (8 iconos de 24 a 64 px, 2 sin fondo sobre un RECT; sin el borde x2/y2 del RECT)'}`);
    }

    console.log('LÍMITE DE CAMPOS');
    const limited = await page.evaluate(async () => {
      const P = window.JWPLCHMIProject;
      const s = P.serialize();
      s.fields = Array.from({ length: 32 }, (_, i) => ({ type: 'TEXT', name: `T${i}`, id: `T_${i}`, variable: `t${i}`, capacity: 2, x: (i % 8) * 38, y: Math.floor(i / 8) * 40, label: '', unit: '', valueSize: 1, labelSize: 1, frame: false, layout: 'INLINE', align: 'LEFT', page: 0, labelColor: 0xFFFF, valueColor: 0xFFFF, backgroundColor: 0, frameColor: 0xFFFF }));
      delete s.iconAssets;
      await P.apply(s);
      await new Promise((r) => setTimeout(r, 300));
      return { added: Boolean(window.JWPLCHMIEditor.addIconField('motores/motor', 32)), count: window.JWPLCHMIEditor.getAllFields().length };
    });
    check(!limited.added && limited.count === 32, 'con 32 campos no se agrega otro ICON (cuenta como campo)');

    check(errors.length === 0, `sin errores de página${errors.length ? `: ${errors[0]}` : ''}`);
  } finally {
    await browser.close();
    server.close();
  }

  console.log(`\nSalida en ${OUT}`);
  if (failures) {
    console.log(`${failures} verificaciones fallaron.`);
    process.exit(1);
  }
  console.log('Todas las verificaciones pasaron.');
}

main().catch((error) => { console.error(error); process.exit(1); });
