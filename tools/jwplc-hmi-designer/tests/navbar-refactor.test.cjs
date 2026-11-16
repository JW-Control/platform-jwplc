#!/usr/bin/env node
// Tests unitarios del rediseño de NAV_BAR (sub-pestañas, presets limpios,
// tipografía unificada y paridad pintor ↔ codegen C++).
'use strict';

const fs = require('fs');
const path = require('path');
const vm = require('vm');
const assert = require('assert');

const ROOT = path.resolve(__dirname, '..');
const POC = path.join(ROOT, 'poc');

const spec = require(path.join(ROOT, 'cli', 'lib', 'spec.cjs'));
const normalize = require(path.join(ROOT, 'cli', 'lib', 'normalize.cjs'));
const validate = require(path.join(ROOT, 'cli', 'lib', 'validate.cjs'));

let passed = 0;
let failed = 0;

function test(name, fn) {
  try {
    fn();
    console.log(`  PASS ${name}`);
    passed += 1;
  } catch (err) {
    console.log(`  FAIL ${name}: ${err.message}`);
    failed += 1;
  }
}

function makeMockDocument() {
  const noop = () => {};
  const ctx2d = {
    save: noop, restore: noop, fillRect: noop, strokeRect: noop,
    beginPath: noop, roundRect: noop, rect: noop, fill: noop, stroke: noop,
    fillText: noop, clearRect: noop, translate: noop, scale: noop,
    imageSmoothingEnabled: false
  };
  const makeEl = (id = '') => ({
    id,
    hidden: false,
    disabled: false,
    value: '',
    textContent: '',
    innerHTML: '',
    style: {},
    dataset: {},
    width: 320,
    height: 170,
    children: [],
    classList: { toggle: noop, add: noop, remove: noop, contains: () => false },
    setAttribute: noop,
    addEventListener: noop,
    querySelector: () => null,
    querySelectorAll: () => [],
    appendChild: noop,
    insertBefore: noop,
    after: noop,
    closest: () => null,
    contains: () => false,
    parentNode: null,
    getContext: () => ctx2d
  });
  const elements = new Map();
  const getEl = (id) => {
    if (!elements.has(id)) elements.set(id, makeEl(id));
    return elements.get(id);
  };
  return {
    activeElement: null,
    head: makeEl('head'),
    body: makeEl('body'),
    getElementById: (id) => getEl(id),
    querySelector: (sel) => (sel === '.page-tabs' || sel === '.page-tabs .gate' ? getEl(sel) : null),
    querySelectorAll: () => [],
    createElement: (tag) => makeEl(tag),
    _elements: elements
  };
}

function loadNavDesignerEnv(initialCfg = {}) {
  let navCfg = {
    enabled: true,
    preset: 'industrial-color',
    position: 'bottom',
    height: 22,
    font: 'GLCD',
    textScale: 1,
    showIndicator: true,
    showArrows: true,
    ...initialCfg
  };
  let navField = {
    key: 'NAV_BAR',
    type: 'NAV_BAR',
    preset: navCfg.preset,
    position: navCfg.position,
    height: navCfg.height,
    font: navCfg.font,
    textScale: navCfg.textScale,
    fontSize: navCfg.fontSize,
    labelFormat: navCfg.labelFormat,
    bgMode: navCfg.bgMode,
    borderMode: navCfg.borderMode,
    radius: navCfg.radius,
    gap: navCfg.gap,
    hidden: false
  };
  const pages = [
    { id: 0, name: 'Principal', shortName: 'MAIN', color: 0x07FF },
    { id: 1, name: 'Control', shortName: 'CTRL', color: 0x07E0 },
    { id: 2, name: 'Alarmas', shortName: 'ALRM', color: 0xF800 }
  ];

  const doc = makeMockDocument();
  const baseHeader = [
    '// Código generado por JWPLC HMI Designer',
    '#pragma once',
    '#include <JWPLC.h>',
    '',
    'enum HMIFieldId : uint8_t',
    '{',
    '    FIELD_COUNT',
    '};',
    '',
    '// Variables HMI',
    '',
    '// Definición declarativa',
    '// Sincronización gráfica autogenerada (JWPLC HMI)',
    'void jwplcUISetup() {}'
  ].join('\n');

  const context = {
    console,
    Math,
    Number,
    String,
    Boolean,
    Array,
    Object,
    Map,
    Set,
    Uint8Array,
    atob: (s) => Buffer.from(s, 'base64').toString('binary'),
    setTimeout,
    clearTimeout,
    document: doc,
    window: {
      addEventListener: () => {},
      dispatchEvent: () => {},
      JWPLCHMIEditor: {
        getAllFields: () => [navField],
        getPages: () => pages,
        getActivePage: () => 0,
        getNavBarConfig: () => ({ ...navCfg, enabled: !navField.hidden }),
        setNavBarConfig: (cfg) => { navCfg = { ...navCfg, ...cfg }; },
        render: () => {},
        commitHistory: () => {}
      }
    }
  };
  context.window.window = context.window;
  context.self = context.window;
  vm.createContext(context);

  for (const file of [
    'designer-cp437.js',
    'designer-fonts-data.js',
    'designer-fonts.js',
    'gfx-classic-font.js',
    'designer-pages.js',
    'designer-codegen-core.js',
    'designer-codegen.js'
  ]) {
    vm.runInContext(fs.readFileSync(path.join(POC, file), 'utf8'), context, { filename: file });
  }

  return {
    pagesApi: context.window.JWPLCHMIPages,
    codegenApi: context.window.JWPLCHMICodegen,
    fontsApi: context.window.JWPLCDesignerFonts,
    generateHeader: () => {
      doc.getElementById('codeOutput').textContent = baseHeader;
      context.window.JWPLCHMICodegen.refresh();
      return doc.getElementById('codeOutput').textContent;
    },
    setField: (patch) => {
      navField = { ...navField, ...patch };
      navCfg = { ...navCfg, ...patch };
    }
  };
}

console.log('NAV_BAR REFACTOR');

test('spec y normalize: 5 presets limpios y migración de legados + font/textScale', () => {
  const presets = spec.TYPES.NAV_BAR.props.preset.enum;
  assert.deepStrictEqual(presets, ['industrial-color', 'dark-pill', 'segmented-bar', 'compact-dots', 'counter']);
  assert.ok(spec.TYPES.NAV_BAR.props.font, 'existe prop font en NAV_BAR');
  assert.ok(spec.TYPES.NAV_BAR.props.textScale, 'existe prop textScale en NAV_BAR');

  const proj = {
    projectName: 'NavLegacy',
    pages: [{ name: 'Inicio' }, { name: 'Ajustes' }],
    fields: [
      { type: 'NAV_BAR', preset: 'numeric-pills', fontSize: 'micro', position: 'bottom' }
    ]
  };
  const project = normalize.normalizeProject(proj);
  const nav = project.fields.find((f) => f.type === 'NAV_BAR');
  assert.strictEqual(nav.preset, 'dark-pill');
  assert.strictEqual(nav.labelFormat, 'number');
  assert.strictEqual(nav.font, 'MICRO');

  const projEdge = normalize.normalizeProject({
    projectName: 'NavEdge',
    pages: [{ name: 'A' }, { name: 'B' }],
    fields: [{ type: 'NAV_BAR', preset: 'edge-tabs' }]
  }).fields[0];
  assert.strictEqual(projEdge.preset, 'segmented-bar');

  const report = validate.validateProject(project);
  assert.strictEqual(report.errors.length, 0);
});

test('UI HTML/CSS: sub-tabs Propuesta 2, selector unificado de fuente y tokens sin hex hardcodeados', () => {
  const html = fs.readFileSync(path.join(POC, 'index.html'), 'utf8');
  const start = html.indexOf('id="navBarInspectorSection"');
  const end = html.indexOf('id="textFieldControlsSection"');
  const navHtml = html.slice(start, end);

  for (const subtab of ['shape', 'content', 'appearance', 'placement']) {
    assert.ok(navHtml.includes(`data-navbar-subtab="${subtab}"`), `subtab ${subtab}`);
  }
  assert.ok(navHtml.includes('id="navBarFontCtrl"'), 'contenedor unificado navBarFontCtrl');
  assert.ok(!navHtml.includes('data-navbar-preset="numeric-pills"'), 'sin numeric-pills repetitivo');
  assert.ok(!navHtml.includes('data-navbar-preset="edge-tabs"'), 'sin edge-tabs repetitivo');
  assert.ok(!navHtml.includes('data-navbar-fontsize='), 'sin botones antiguos data-navbar-fontsize');
  assert.ok(!/#(?:09131a|1e3342|6a8292|7890a8|1a2a36)\b/i.test(navHtml), 'sin hex de UI oscuros hardcodeados en #navBarInspectorSection');
});

test('designer-pages.js: migración de presets, resolución de fuentes y ops diferenciados en segmented-bar/compact-dots', () => {
  const env = loadNavDesignerEnv();
  const { pagesApi } = env;

  assert.deepEqual(
    pagesApi.normalizeNavPresetAndLabels({ preset: 'numeric-pills' }),
    { preset: 'dark-pill', labelFormat: 'number' }
  );
  assert.deepEqual(
    pagesApi.normalizeNavPresetAndLabels({ preset: 'edge-tabs' }),
    { preset: 'segmented-bar', labelFormat: 'short' }
  );

  assert.deepEqual(pagesApi.resolveNavFontSpec({ fontSize: 'micro' }), { font: 'MICRO', textScale: 1, fontSize: 'micro' });
  assert.deepEqual(pagesApi.resolveNavFontSpec({ fontSize: 'medium' }), { font: 'GLCD', textScale: 1, fontSize: 'normal' });
  assert.deepEqual(pagesApi.resolveNavFontSpec({ font: 'SANS_9', textScale: 3 }), { font: 'SANS_9', textScale: 1, fontSize: 'normal' });
  assert.deepEqual(pagesApi.resolveNavFontSpec({ font: 'GLCD', textScale: 2 }), { font: 'GLCD', textScale: 2, fontSize: 'normal' });

  // Segmented-bar positioned debe mostrar franja #52c9ff y relleno #1f2e42
  env.setField({ preset: 'segmented-bar', font: 'GLCD', textScale: 1 });
  const segPos = pagesApi.recordNavBar(0, 'positioned');
  assert.ok(segPos.some((op) => op.op === 'fillRound' && op.col === '#1f2e42'), 'segmented-bar active fill #1f2e42');
  assert.ok(segPos.some((op) => op.op === 'fillRect' && op.col === '#52c9ff' && op.h === 2), 'segmented-bar franja #52c9ff');
  const segSel = pagesApi.recordNavBar(0, 'selected');
  const segSelActive = segSel.slice(
    segSel.findIndex((op) => op.op === 'mark' && op.active && op.kind === 'start') + 1,
    segSel.findIndex((op) => op.op === 'mark' && op.active && op.kind === 'end')
  );
  assert.ok(segSelActive.some((op) => op.op === 'fillRound' && op.col === '#ffffff'), 'segmented-bar selected fondo blanco');
  assert.strictEqual(segSelActive.filter((op) => op.op === 'strokeRound').length, 2, 'segmented-bar selected doble marco');

  // Compact-dots positioned vs selected diferenciados
  env.setField({ preset: 'compact-dots' });
  const dotPos = pagesApi.recordNavBar(0, 'positioned');
  const dotSel = pagesApi.recordNavBar(0, 'selected');
  assert.ok(dotPos.some((op) => op.op === 'fillRound' && op.col === '#00c8ff'), 'dots positioned cian');
  assert.ok(dotSel.some((op) => op.op === 'fillRound' && op.col === '#ffffff'), 'dots selected blanco');
  assert.ok(dotSel.some((op) => op.op === 'strokeRound' && op.col === '#00c8ff'), 'dots selected halo cian');
});

test('designer-codegen.js: GLCD 1× sin bloat y fuentes universales (MICRO, GLCD 2×, SANS_9) en C++', () => {
  const env = loadNavDesignerEnv({ preset: 'industrial-color', font: 'GLCD', textScale: 1 });
  const baseCpp = env.generateHeader();
  assert.ok(/static void jwplcDrawPageNavTab/.test(baseCpp));
  assert.ok(!/JWPLC_HMI_TFT_FONTS/.test(baseCpp), 'GLCD 1x por defecto no agrega guardas FreeFonts innecesarias');

  env.setField({ font: 'MICRO', textScale: 1, fontSize: 'micro' });
  const microCpp = env.generateHeader();
  assert.ok(/jwplcHMIDrawMicroText\(tft,/.test(microCpp), 'MICRO usa jwplcHMIDrawMicroText');

  env.setField({ font: 'GLCD', textScale: 2, fontSize: 'normal' });
  const glcd2Cpp = env.generateHeader();
  assert.ok(/tft\.setTextSize\(2\);/.test(glcd2Cpp), 'GLCD 2x aplica setTextSize(2)');

  env.setField({ font: 'SANS_9', textScale: 1, height: 24 });
  const sansCpp = env.generateHeader();
  assert.ok(/#if JWPLC_HMI_TFT_FONTS/.test(sansCpp), 'SANS_9 emite bloque JWPLC_HMI_TFT_FONTS');
  assert.ok(/tft\.setFont\(JWPLC_FONT_SANS_9\);/.test(sansCpp), 'SANS_9 emite JWPLC_FONT_SANS_9');
  assert.ok(/tft\.drawString\(/.test(sansCpp), 'SANS_9 emite drawString');
  assert.ok(/tft\.setFont\(JWPLC_FONT_GLCD\);/.test(sansCpp), 'SANS_9 restaura JWPLC_FONT_GLCD');
});

console.log(`\nTotal: ${passed} passed, ${failed} failed`);
process.exit(failed ? 1 : 0);
