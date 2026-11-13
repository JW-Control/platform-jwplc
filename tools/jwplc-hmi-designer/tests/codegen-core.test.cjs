#!/usr/bin/env node
// Tests del codegen puro (A15-5), sin navegador:
//   - designer-codegen-core.js (bloques C++ de TEXT/VALUE/BOOL/BAR);
//   - designer-menu.js en un contexto vm sin DOM (validación, generate(), pintor).
// Si hay g++, compila el header armado con el núcleo contra los headers reales
// y compara el pintor del MENU con el firmware simulado en host.
//
// Uso (desde la raíz del repo):
//   node tools/jwplc-hmi-designer/tests/codegen-core.test.cjs
'use strict';

const fs = require('fs');
const os = require('os');
const path = require('path');
const vm = require('vm');
const { execFileSync } = require('child_process');

const POC = path.resolve(__dirname, '..', 'poc');
const LIBS = path.resolve(__dirname, '..', '..', '..', 'JWPLC', '2.1.0', 'libraries');
const STUB = path.join(__dirname, 'host-stub');
const FIXTURES = require('./fixtures.cjs');
const core = require(path.join(POC, 'designer-codegen-core.js'));

function loadMenuModule() {
  const context = { window: {}, console };
  context.self = context.window; // designer-cp437.js (UMD) se registra en self
  vm.createContext(context);
  for (const file of ['designer-cp437.js', 'designer-fonts.js', 'gfx-classic-font.js', 'designer-menu.js']) {
    vm.runInContext(fs.readFileSync(path.join(POC, file), 'utf8'), context, { filename: file });
  }
  return context.window.JWPLCHMIMenu;
}

function loadEventLogModule() {
  const context = { window: {}, console };
  context.self = context.window;
  vm.createContext(context);
  for (const file of ['designer-cp437.js', 'designer-fonts.js', 'designer-event-log.js']) {
    vm.runInContext(fs.readFileSync(path.join(POC, file), 'utf8'), context, { filename: file });
  }
  return context.window.JWPLCHMIEventLog;
}

const failures = [];
function check(condition, message) {
  if (condition) console.log(`  PASS ${message}`);
  else { console.log(`  FAIL ${message}`); failures.push(message); }
}

function hasGpp() {
  try { execFileSync('g++', ['--version'], { stdio: 'ignore' }); return true; } catch (_) { return false; }
}

const COLORS = { labelColor: 0xFFFF, valueColor: 0x07FF, backgroundColor: 0x0000, frameColor: 0xFFFF };

console.log('NÚCLEO');
const value = { ...COLORS, type: 'VALUE', x: 4, y: 40, label: 'Temp', unit: 'C', integerDigits: 3, decimalDigits: 1, signedValue: true, leadingZeros: false, valueSize: 2, labelSize: 1, frame: false, layout: 'INLINE', align: 'RIGHT', page: 0 };
check(core.valueFieldBlock(value, 'FIELD_TEMP') ===
  '    JWPLC_UIValueField(\n        FIELD_TEMP,\n        JWPLC_UIRect(4, 40),\n        JWPLC_UIText("Temp", "C"),\n        JWPLC_UIValueFormat(\n            3,\n            1,\n            true,\n            false),\n        JWPLC_UIValueStyle(\n            2,\n            1,\n            false,\n            JWPLC_UI_LAYOUT_INLINE,\n            JWPLC_UI_ALIGN_RIGHT),\n        0,\n        JWPLC_UIColors(\n            0xFFFF,\n            0x07FF,\n            0x0000,\n            0xFFFF))',
  'VALUE: bloque exacto');
check(/JWPLC_UIValueFormat\(\n            7,\n            0,\n            false,\n            false,\n            ','\)/.test(core.valueFieldBlock({ ...value, integerDigits: 7, decimalDigits: 0, signedValue: false, thousandsSep: ',' }, 'F')),
  'VALUE: separador de miles como 5.º argumento');
check(/JWPLC_UIDurationFormat\(\n            JWPLC_UI_DURATION_HH_MM_SS,\n            JWPLC_UI_DURATION_MILLIS,\n            3,\n            false\)/.test(core.valueFieldBlock({ ...value, valueKind: 'DURATION', durationPattern: 'HH_MM_SS', durationUnit: 'MS', integerDigits: 3, signedValue: false }, 'F')),
  'VALUE: duración con JWPLC_UIDurationFormat');
check(core.variableDeclaration({ type: 'VALUE', valueKind: 'DURATION', signedValue: false }, 't') === 'uint32_t t = 0;' &&
  core.variableDeclaration({ type: 'VALUE', valueKind: 'DURATION', signedValue: true }, 't') === 'int32_t t = 0;' &&
  core.variableDeclaration({ type: 'VALUE', thousandsSep: ',', decimalDigits: 0 }, 'n') === 'uint32_t n = 0;' &&
  core.variableDeclaration({ type: 'VALUE', thousandsSep: ',', decimalDigits: 1 }, 'n') === 'float n = 0.0f;' &&
  core.variableDeclaration({ type: 'VALUE', decimalDigits: 0 }, 'n') === 'float n = 0.0f;',
  'VALUE: duración y contadores con miles usan enteros de 32 bits; el resto float');
const clockField = { ...value, valueKind: 'DATETIME', dateTimePattern: 'DD_MM_YYYY_HH_MM' };
check(/JWPLC_UIDateTimeFormat\(\n            JWPLC_UI_DATETIME_DD_MM_YYYY_HH_MM\)/.test(core.valueFieldBlock(clockField, 'F')) &&
  core.variableDeclaration(clockField, 'h') === '' && core.setterCall(clockField, 'F', 'h') === '' && core.isClockField(clockField),
  'VALUE fecha/hora: JWPLC_UIDateTimeFormat, sin variable ni setter');
check(core.hex565('TRANSPARENT') === '0x0000' && core.hex565(0x1F) === '0x001F', 'hex565: TRANSPARENT -> 0x0000');
check(/JWPLC_UIText\("Di \\"A\\"", nullptr, 8\)/.test(core.textFieldBlock({ ...COLORS, type: 'TEXT', x: 0, y: 0, label: 'Di "A"', unit: '', capacity: 8, valueSize: 1, labelSize: 1, frame: false, layout: 'INLINE', align: 'LEFT', page: 0 }, 'F')),
  'TEXT: escapa comillas y usa nullptr sin unidad');
check(core.cppFloat(5) === '5.0f' && core.cppFloat(0.25) === '0.25f' && core.cppFloat('x') === '0.0f', 'cppFloat');
check(core.variableDeclaration({ type: 'TEXT', capacity: 12 }, 'msg') === 'char msg[25] = {}; // 12 caracteres (UTF-8: cada tilde ocupa 2 bytes)', 'TEXT reserva 2 bytes por carácter (UTF-8) + 1');
check(core.setterCall({ type: 'BAR' }, 'F', 'v') === 'JWPLC_Display.setBar(F, v);', 'setter de BAR');
const bar = core.barFieldBlock({ ...COLORS, type: 'BAR', x: 1, y: 2, barWidth: 500, barMin: 0, barMax: 10, labelSize: 1, frame: true, layout: 'INLINE', page: 3, showLabel: false, barUnit: '', trackColor: 0x18C3 }, 'B');
check(/JWPLC_UIRect\(1, 2, 320, JWPLC_UI_AUTO\)/.test(bar) && /JWPLC_UIText\(nullptr, nullptr\)/.test(bar) && /0x18C3/.test(bar), 'BAR: ancho acotado a 320, sin etiqueta, pista como fondo');
const vertBar = core.barFieldBlock({
  ...COLORS,
  type: 'BAR',
  barOrientation: 'VERTICAL',
  barStyle: 'BLOCKS',
  x: 10,
  y: 20,
  barWidth: 16,
  barHeight: 80,
  barMin: 0,
  barMax: 100,
  page: 0
}, 'B_VERT');
check(/JWPLC_UIRect\(10, 20\)/.test(vertBar) &&
  /JWPLC_UIBarConfig\(\s*JWPLC_UI_BAR_VERTICAL,\s*JWPLC_UI_BAR_STYLE_BLOCKS,\s*0,\s*0,\s*false,\s*0xFFFF,\s*1,\s*16,\s*80\)/.test(vertBar),
  'BAR VERTICAL: grosor y largo en JWPLC_UIBarConfig, caja calculada por el firmware');
const vertBarWithText = core.barFieldBlock({
  ...COLORS,
  type: 'BAR',
  barOrientation: 'VERTICAL',
  barStyle: 'SOLID',
  showLabel: true,
  barLabel: 'Tanque 1',
  barUnit: 'm3',
  x: 15,
  y: 10,
  barWidth: 20,
  barHeight: 70,
  barMin: 0,
  barMax: 50,
  page: 0
}, 'B_TANK');
check(/JWPLC_UIRect\(15, 10\)/.test(vertBarWithText) &&
  /JWPLC_UIText\("Tanque 1", "m3"\)/.test(vertBarWithText) &&
  /JWPLC_UIBarConfig\(\s*JWPLC_UI_BAR_VERTICAL,\s*JWPLC_UI_BAR_STYLE_SOLID,\s*0,\s*0,\s*false,\s*0xFFFF,\s*1,\s*20,\s*70\)/.test(vertBarWithText),
  'BAR VERTICAL con etiqueta y unidad: emite texto y dimensiones en config');
const thickBar = core.barFieldBlock({ ...COLORS, type: 'BAR', x: 1, y: 2, barWidth: 110, barHeight: 30, barMin: 0, barMax: 100, page: 0 }, 'B_THICK');
check(/JWPLC_UIRect\(1, 2, 110, JWPLC_UI_AUTO\)/.test(thickBar) &&
  /JWPLC_UIBarConfig\(\s*JWPLC_UI_BAR_HORIZONTAL,\s*JWPLC_UI_BAR_STYLE_SOLID,\s*0,\s*0,\s*false,\s*0xFFFF,\s*1,\s*30,\s*0\)/.test(thickBar),
  'BAR horizontal: el grosor distinto de 12 px llega a JWPLC_UIBarConfig');
check(!/JWPLC_UIBarConfig/.test(core.barFieldBlock({ ...COLORS, type: 'BAR', x: 1, y: 2, barWidth: 110, barHeight: 12, barMin: 0, barMax: 100, page: 0 }, 'B_STD')),
  'BAR horizontal de 12 px: sin JWPLC_UIBarConfig');
const blackEndBar = core.barFieldBlock({ ...COLORS, type: 'BAR', x: 1, y: 2, barWidth: 110, barFillMode: 'GRADIENT', valueColor: 0x07FF, valueColorEnd: 0, barMin: 0, barMax: 100, page: 0 }, 'B_BLACK');
check(/JWPLC_UI_BAR_STYLE_SOLID,\s*0x0001\)/.test(blackEndBar), 'BAR GRADIENT: el negro final sale como 0x0001 (0 significa sólido)');
const blackBorderBar = core.barFieldBlock({ ...COLORS, type: 'BAR', x: 1, y: 2, barWidth: 110, barBorderEnabled: true, barBorderColor: 0, barBorderWidth: 2, barMin: 0, barMax: 100, page: 0 }, 'B_BORDER');
check(/true,\s*0x0000,\s*2\)/.test(blackBorderBar), 'BAR: el borde negro se respeta');
const gradBar = core.barFieldBlock({
  ...COLORS,
  type: 'BAR',
  barOrientation: 'HORIZONTAL',
  barFillMode: 'GRADIENT',
  valueColor: 0x07FF,
  valueColorEnd: 0xF800,
  x: 5,
  y: 15,
  barWidth: 100,
  barMin: 0,
  barMax: 50,
  page: 0
}, 'B_GRAD');
check(/JWPLC_UIBarConfig\(\s*JWPLC_UI_BAR_HORIZONTAL,\s*JWPLC_UI_BAR_STYLE_SOLID,\s*0xF800\)/.test(gradBar),
  'BAR GRADIENT: emite JWPLC_UIBarConfig con colorEnd 0xF800');
const roundedBar = core.barFieldBlock({
  ...COLORS,
  type: 'BAR',
  barOrientation: 'HORIZONTAL',
  barStyle: 'SOLID',
  barCornerRadius: 3,
  barBorderEnabled: true,
  barBorderColor: 0x001F,
  barBorderWidth: 2,
  x: 0,
  y: 0,
  barWidth: 100,
  barMin: 0,
  barMax: 100,
  page: 0
}, 'B_ROUND');
check(/JWPLC_UIBarConfig\(\s*JWPLC_UI_BAR_HORIZONTAL,\s*JWPLC_UI_BAR_STYLE_SOLID,\s*0,\s*3,\s*true,\s*0x001F,\s*2\)/.test(roundedBar),
  'BAR ROUNDED: emite cornerRadius y border en JWPLC_UIBarConfig');
const circular = core.barFieldBlock({
  ...COLORS,
  type: 'BAR',
  barPreset: 'CIRCULAR',
  x: 10,
  y: 20,
  circleRadius: 30,
  circleSweep: 270,
  circleThickness: 6,
  circleStartAngle: 'TOP',
  circleDirection: 'CW',
  circleStyle: 'CONTINUOUS',
  circleCenterMode: 'PERCENT',
  circleTextSize: 1,
  circleTextColor: 0xFFFF,
  circleTrackColor: 0x2104,
  barMin: 0,
  barMax: 100,
  barLabel: 'Tanque',
  barUnit: '%',
  showLabel: true,
  page: 1,
  frame: false
}, 'C');
check(/JWPLC_UICircularField\(/.test(circular) &&
  /JWPLC_UIRect\(10, 20, 60, 60\)/.test(circular) &&
  /JWPLC_UICircularStyle\(\s*270,\s*6,\s*1/.test(circular),
  'CIRCULAR: genera JWPLC_UICircularField con JWPLC_UICircularStyle');

const circularBlackText = core.barFieldBlock({
  ...COLORS,
  type: 'BAR',
  barPreset: 'CIRCULAR',
  x: 10,
  y: 20,
  circleRadius: 30,
  circleCenterMode: 'PERCENT',
  circleTextColor: 0x0000,
  barMin: 0,
  barMax: 100
}, 'CB');
check(/0x0001/.test(circularBlackText), 'CIRCULAR: circleTextColor negro explícito (0x0000) sale como 0x0001');

const barJsSrc = fs.readFileSync(path.join(POC, 'designer-bar.js'), 'utf8');
check(barJsSrc.includes("circleText: 'circleTextColor'") && barJsSrc.includes("'Texto central'"),
  'CIRCULAR inspector: ranura de color Texto central conectada a circleTextColor');

const valCustomFont = core.valueFieldBlock({
  ...value,
  labelFont: 'FONT_16',
  valueFont: 'FONT_7SEG_48'
}, 'FIELD_CUSTOM_VAL');
check(/JWPLC_UIValueStyle\(\s*2,\s*1,\s*false,\s*JWPLC_UI_LAYOUT_INLINE,\s*JWPLC_UI_ALIGN_RIGHT,\s*&JWPLC_FONT_16,\s*&JWPLC_FONT_7SEG_48\)/.test(valCustomFont),
  'VALUE: emite &JWPLC_FONT_16 y &JWPLC_FONT_7SEG_48 en JWPLC_UIValueStyle');

const valOnlyValueFont = core.valueFieldBlock({
  ...value,
  valueFont: 'FONT_7SEG_48'
}, 'FIELD_ONLY_VAL_FONT');
check(/JWPLC_UIValueStyle\(\s*2,\s*1,\s*false,\s*JWPLC_UI_LAYOUT_INLINE,\s*JWPLC_UI_ALIGN_RIGHT,\s*nullptr,\s*&JWPLC_FONT_7SEG_48\)/.test(valOnlyValueFont),
  'VALUE: con solo valueFont emite nullptr para labelFont');

const txtCustomFont = core.textFieldBlock({
  ...COLORS, type: 'TEXT', x: 0, y: 0, label: 'Msg', unit: '', capacity: 8,
  valueSize: 1, labelSize: 1, frame: false, layout: 'INLINE', align: 'LEFT', page: 0,
  labelFont: 'FONT_26', valueFont: 'FONT_16'
}, 'FIELD_CUSTOM_TXT');
check(/JWPLC_UITextFieldStyle\(\s*1,\s*1,\s*false,\s*JWPLC_UI_LAYOUT_INLINE,\s*JWPLC_UI_ALIGN_LEFT,\s*&JWPLC_FONT_26,\s*&JWPLC_FONT_16\)/.test(txtCustomFont),
  'TEXT: emite &JWPLC_FONT_26 y &JWPLC_FONT_16 en JWPLC_UITextFieldStyle');

const boolCustomFont = core.boolFieldBlock({
  ...COLORS, type: 'BOOL', x: 0, y: 0, label: 'Estado', unit: '', falseText: 'OFF', trueText: 'ON',
  valueSize: 2, labelSize: 1, frame: false, layout: 'INLINE', align: 'CENTER', page: 0,
  labelFont: 'FONT_16', valueFont: 'FONT_26'
}, 'FIELD_CUSTOM_BOOL');
check(/JWPLC_UIBoolStyle\(\s*2,\s*1,\s*false,\s*JWPLC_UI_LAYOUT_INLINE,\s*JWPLC_UI_ALIGN_CENTER,\s*&JWPLC_FONT_16,\s*&JWPLC_FONT_26\)/.test(boolCustomFont),
  'BOOL: emite &JWPLC_FONT_16 y &JWPLC_FONT_26 en JWPLC_UIBoolStyle');

const iconsCore = require(path.join(POC, 'designer-icons-core.js'));
const iconField = {
  ...COLORS, type: 'ICON', x: 10, y: 15, label: 'Bomba 1', labelSize: 1, frame: true,
  layout: 'STACKED', page: 0
};
const iconBlockNative = iconsCore.iconFieldBlock(iconField, 'FIELD_ICON_BOMBA', iconsCore.iconCallFor('bombas/bomba', 32), (s) => s);
check(iconBlockNative.includes('JWPLC_Icons.bomba(32)') && !iconBlockNative.includes('&JWPLC_Icons'), 'ICON: genera llamada de biblioteca JWPLC_Icons.bomba(32) sin ampersand');
const iconBlockFallback = iconsCore.iconFieldBlock(iconField, 'FIELD_ICON_CUSTOM', 'CUSTOM_ICON_SYMBOL', (s) => s);
check(iconBlockFallback.includes('&CUSTOM_ICON_SYMBOL'), 'ICON: símbolo custom mantiene & como fallback');

console.log('MENU (sin DOM)');
const menuApi = loadMenuModule();
check(Boolean(menuApi?.generate), 'designer-menu.js carga sin DOM');
const fields = FIXTURES.menu.fields;
const menus = fields.filter((f) => f.type === 'MENU').map((m) => JSON.parse(JSON.stringify(m)));
const others = fields.filter((f) => f.type !== 'MENU');
const out = menuApi.generate(menus, { fields: others, pageSymbol: (id) => ['PAGE_PRINCIPAL', 'PAGE_AJUSTES'][id] });
check(out.errors.length === 0, `fixture sin errores${out.errors.length ? `: ${out.errors.join(' | ')}` : ''}`);
check(!/float temperatura/.test(out.variables), 'no redeclara una variable que ya declara un campo');
check((out.variables.match(/bool bomba/g) || []).length === 1, 'una variable usada por dos nodos se declara una vez');
check(/MENU_AJUSTES_BOMBA_2 = 5,/.test(out.declarations), 'símbolos de nodo únicos aunque se repita el texto');
check(/JWPLC_UIMenuEditNum\(MENU_AJUSTES_AVANZADO, "Retardo", &retardo, 0, 600, 5, "s"\)/.test(out.declarations), 'EDIT_NUM int32 con padre simbólico');
check(/PAGE_AJUSTES,/.test(out.declarations), 'usa el símbolo de página');
const accented = JSON.parse(JSON.stringify(menus));
accented[0].nodes[0].label = 'Presión';
accented[0].nodes[0].unit = '°C';
const accOut = menuApi.generate(accented, { fields: others, pageSymbol: (id) => ['PAGE_PRINCIPAL', 'PAGE_AJUSTES'][id] });
check(/"Presi\\242n"/.test(accOut.declarations) && /"\\367C"/.test(accOut.declarations) && !accOut.warnings.some((w) => /fuente/.test(w)),
  'MENU: tildes y ° en cp437 (octal), sin aviso');
accented[0].nodes[0].label = 'Ángulo ☃';
check(menuApi.validate(accented, others).warnings.some((w) => /«Á ☃» no existe en la fuente/.test(w)), 'MENU: avisa sólo lo que no tiene glifo');

const bad = (mutator) => {
  const copy = JSON.parse(JSON.stringify(menus));
  mutator(copy[0]);
  return menuApi.validate(copy, others).errors;
};
check(bad((m) => {
  m.nodes.push({ key: 's2', parent: 'a', type: 'SUBMENU', label: 'X' }, { key: 's3', parent: 's2', type: 'SUBMENU', label: 'Y' }, { key: 'z', parent: 's3', type: 'VIEW', label: 'Z', variable: 'z', valueType: 'float' });
}).some((e) => /profundidad/.test(e)), 'rechaza profundidad 4');
check(bad((m) => { m.nodes[0].valueType = 'bool'; }).some((e) => /temperatura ya es float/.test(e)), 'detecta tipo distinto al del campo');
check(bad((m) => { m.nodes[1].valueType = 'int32'; m.nodes[1].step = 0.5; }).some((e) => /enteros/.test(e)), 'int32 exige mínimo/máximo/paso enteros');
check(bad((m) => { m.nodes[1].variable = ''; }).some((e) => /falta la variable/.test(e)), 'exige variable');
const twoOnPage = JSON.parse(JSON.stringify(menus));
twoOnPage.push({ ...JSON.parse(JSON.stringify(menus[0])), key: 'menu-2', id: 'MENU_2' });
check(menuApi.validate(twoOnPage, others).errors.some((e) => /uno por página/.test(e)), 'un MENU por página');
check(/#error "MENU: /.test(menuApi.generate(twoOnPage, { fields: others }).declarations), 'los errores se emiten como #error');

const L = menuApi.layout({ textSize: 2, frame: true, title: 'X', height: 100 });
check(L.rowH === 20 && L.headerH === 20 && L.rows === 3, 'layout: 2× -> filas de 20 px, 3 visibles en 100 px');
const menuWithFont = { ...menus[0], id: 'MENU_FONT', font: 'FONT_16' };
const genMenuFont = menuApi.generate([menuWithFont]);
check(/&JWPLC_FONT_16\)/.test(genMenuFont.declarations), 'MENU: emite &JWPLC_FONT_16 en JWPLC_UIMenuField');

console.log('EVENT_LOG (sin DOM)');
const evtApi = loadEventLogModule();
check(Boolean(evtApi?.generate), 'designer-event-log.js carga sin DOM');
const sampleEvt = evtApi.createDefault(2);
check(sampleEvt.type === 'EVENT_LOG' && sampleEvt.page === 2, 'createDefault crea EVENT_LOG en la página indicada');
sampleEvt.descriptors = [
  { code: 100, label: 'Arranque sistema', severity: 'INFO', category: 'SYSTEM' },
  { code: 201, label: 'Falla de bomba 1', severity: 'CRIT', category: 'ALARM' }
];
const genEvt = evtApi.generate([sampleEvt]);
check(/JWPLC_UIEventDescriptor HMI_EVENT_DESCRIPTORS/.test(genEvt.declarations), 'genera tabla de descriptores PROGMEM');
check(/JWPLC_UIEventLog HMI_EVENT_LOG/.test(genEvt.declarations), 'genera estructura JWPLC_UIEventLog');
check(/JWPLC_Display\.setEventLog\(HMI_EVENT_LOG\);/.test(genEvt.setup), 'genera setup JWPLC_Display.setEventLog');
const patched = evtApi.patchHeader('void jwplcHMISetup()\n{\n    JWPLCUI::begin();\n}\n', [sampleEvt]);
check(/HMI_EVENT_LOG/.test(patched) && /JWPLC_Display\.setEventLog/.test(patched), 'patchHeader inyecta declaraciones y setup de EVENT_LOG');
const evtWithFont = { ...sampleEvt, headerFont: 'FONT_16', rowFont: 'FONT_26' };
const genEvtFont = evtApi.generate([evtWithFont]);
check(/\/\* headerFont \*\/ &JWPLC_FONT_16/.test(genEvtFont.declarations) &&
  /\/\* rowFont \*\/ &JWPLC_FONT_26/.test(genEvtFont.declarations),
  'EVENT_LOG: emite &JWPLC_FONT_16 y &JWPLC_FONT_26');

if (hasGpp()) {
  console.log('COMPILACIÓN HOST (g++)');
  const dir = fs.mkdtempSync(path.join(os.tmpdir(), 'jwplc-core-'));
  const header = [
    '#pragma once', '#include <JWPLC_Display.h>',
    'enum : uint8_t { PAGE_PRINCIPAL = 0, PAGE_AJUSTES = 1 };',
    'enum : uint8_t { FIELD_TEMP = 1 };',
    'float temperatura = 0.0f;',
    out.variables,
    `static const JWPLC_UIField HMI_FIELDS[] =\n{\n${core.valueFieldBlock({ ...others[0], page: 0 }, 'FIELD_TEMP')}\n};`,
    out.declarations,
    `void jwplcHMISetup()\n{\n${out.setup}    JWPLC_Display.setFields(HMI_FIELDS, 1);\n}`
  ].join('\n\n');
  fs.writeFileSync(path.join(dir, 'gen.h'), header);
  fs.writeFileSync(path.join(dir, 'main.cpp'), '#include "gen.h"\nint main() { jwplcHMISetup(); return 0; }\n');
  let ok = true;
  let log = '';
  try {
    execFileSync('g++', ['-std=gnu++17', '-fsyntax-only', '-Wall', `-I${dir}`, `-I${STUB}`,
      `-I${path.join(LIBS, 'JWPLC_Display', 'src')}`, `-I${path.join(LIBS, 'JWPLC_TFT', 'src')}`, path.join(dir, 'main.cpp')], { stdio: 'pipe' });
  } catch (error) {
    ok = false;
    log = String(error.stderr || error.message).split('\n').slice(0, 6).join('\n');
  }
  check(ok, `núcleo + MENU compilan contra los headers reales${ok ? '' : `\n${log}`}`);

  console.log('PARIDAD PINTOR / FIRMWARE');
  const simDir = path.join(dir, 'sim');
  try {
    execFileSync(process.execPath, [path.join(__dirname, 'menu-host-sim.cjs'), simDir], { stdio: 'pipe' });
  } catch (_) { /* las referencias que existan se comparan igual */ }
  const gate = {
    ...JSON.parse(JSON.stringify(menus[0])), x: 10, y: 20, page: 0,
    nodes: [
      { key: 't', parent: null, type: 'VIEW', label: 'Temperatura', variable: 'temperatura', valueType: 'float', initial: 21.5, decimals: 1, unit: 'C' },
      { key: 'c', parent: null, type: 'EDIT_NUM', label: 'Consigna', variable: 'consigna', valueType: 'float', initial: 45, decimals: 1, unit: 'C' },
      { key: 'b', parent: null, type: 'EDIT_BOOL', label: 'Bomba', variable: 'bomba', valueType: 'bool', initial: 0, falseText: 'PARADA', trueText: 'MARCHA' },
      { key: 'a', parent: null, type: 'SUBMENU', label: 'Avanzado' }
    ]
  };
  const ref = path.join(simDir, 'menu_root.rgb565');
  if (fs.existsSync(ref)) {
    const raw = fs.readFileSync(ref);
    const fb = menuApi.renderToArray(gate, { parentKey: null, selected: 0, depth: 1, editing: false, focused: true });
    let diff = 0;
    for (let i = 0; i < fb.length; i++) if (fb[i] !== raw.readUInt16LE(i * 2)) diff++;
    check(diff === 0, `pintor del MENU (node) == firmware en host${diff ? ` (${diff} px distintos)` : ''}`);
  } else {
    check(false, 'falta la referencia menu_root.rgb565 de menu-host-sim');
  }
} else {
  console.log('  SKIP g++ no disponible: sin compilación ni paridad');
}

if (failures.length) {
  console.log(`\n${failures.length} verificaciones fallaron.`);
  process.exit(1);
}
console.log('\nTodas las verificaciones pasaron.');
