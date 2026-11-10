#!/usr/bin/env node
'use strict';

const fs = require('fs');
const path = require('path');
const vm = require('vm');

const POC = path.resolve(__dirname, '..', 'poc');

let failures = 0;
function check(condition, message) {
  console.log(`  ${condition ? 'PASS' : 'FAIL'} ${message}`);
  if (!condition) failures += 1;
}

console.log('SHAPES PARITY & BEHAVIOR TESTS');

// Load environment to test designer-codegen.js and app.js helpers
function createTestEnv() {
  const elements = {};
  function makeEl(id) {
    return {
      id,
      value: '',
      textContent: '',
      hidden: false,
      style: {},
      dataset: {},
      classList: { toggle() {}, add() {}, remove() {}, contains: () => false },
      addEventListener() {},
      querySelectorAll: () => [],
      querySelector: () => null,
      appendChild() {},
      getBoundingClientRect: () => ({ left: 0, top: 0, width: 320, height: 170 })
    };
  }

  const domIds = [
    'codeOutput', 'contractTab', 'statusTab', 'generateButton', 'fieldId', 'fieldVariable',
    'shapeInspectorSection', 'shapeInspectorIcon', 'shapeInspectorName', 'shapeInspectorHeaderTitle',
    'shapeX', 'shapeY', 'shapeW', 'shapeH', 'shapeCornerCard', 'shapeCornerRadius', 'shapeCornerMaxHint',
    'shapeSidesWrap2', 'shapeSides2', 'shapeRotation', 'shapeBorderRow', 'shapeBorderLabel',
    'shapeBorderToggleWrap', 'shapeBorderEnabled', 'shapeBorderSize', 'shapeFillRow', 'shapeFillEnabled',
    'shapeTechCodePreview', 'shapeSubTabs', 'shapePaneForm', 'shapePaneAppear', 'shapePaneLoc', 'shapePaneTech'
  ];
  domIds.forEach((id) => { elements[id] = makeEl(id); });

  const ctx = {
    window: {},
    console,
    Math,
    Number,
    String,
    Boolean,
    Array,
    Object,
    Set,
    Map,
    addEventListener() {},
    removeEventListener() {},
    document: {
      getElementById: (id) => elements[id] || makeEl(id),
      querySelectorAll: () => [],
      querySelector: () => null,
      addEventListener() {},
      removeEventListener() {},
      activeElement: null
    }
  };
  ctx.window = ctx;
  ctx.self = ctx;
  vm.createContext(ctx);

  // Load cp437 and codegen files
  for (const file of ['designer-cp437.js', 'designer-codegen.js']) {
    const code = fs.readFileSync(path.join(POC, file), 'utf8');
    vm.runInContext(code, ctx, { filename: file });
  }

  return ctx;
}

// -------------------------------------------------------------
// Test 1: Rotated Ellipse in Codegen
// -------------------------------------------------------------
console.log('1. Codegen para ELLIPSE ortogonal vs rotada');
{
  const ctx = createTestEnv();
  const codegen = ctx.window.JWPLCHMICodegen;

  // Mock editor for vertex calculation
  ctx.window.JWPLCHMIEditor = {
    getShapeVertices: (f) => {
      // 8 points polygon for mock transformed vertices of an ellipse
      return [
        { x: 50, y: 20 }, { x: 70, y: 30 }, { x: 80, y: 50 }, { x: 70, y: 70 },
        { x: 50, y: 80 }, { x: 30, y: 70 }, { x: 20, y: 50 }, { x: 30, y: 30 }
      ];
    }
  };

  const orthogonalEllipse = {
    type: 'ELLIPSE',
    x: 20, y: 30, x2: 80, y2: 70,
    rotation: 0,
    frameColor: 0xFFFF,
    fill: true,
    fillColor: 0x07E0,
    borderEnabled: true,
    size: 1
  };

  const snippetOrtho = codegen.staticDrawSnippet(orthogonalEllipse);
  check(snippetOrtho.includes('jwplcHMIFillEllipse') && snippetOrtho.includes('jwplcHMIDrawEllipse'),
    'Elipse ortogonal (rot=0) genera jwplcHMIFillEllipse y jwplcHMIDrawEllipse');

  const circle = {
    type: 'ELLIPSE',
    x: 20, y: 20, x2: 60, y2: 60,
    rotation: 0,
    frameColor: 0xFFFF,
    fill: true,
    fillColor: 0xF800,
    borderEnabled: true,
    size: 1
  };
  const snippetCircle = codegen.staticDrawSnippet(circle);
  check(snippetCircle.includes('tft.fillCircle') && snippetCircle.includes('tft.drawCircle'),
    'Elipse circular ortogonal (rx == ry) genera fillCircle y drawCircle');

  const rotatedEllipse = {
    type: 'ELLIPSE',
    x: 20, y: 30, x2: 80, y2: 70,
    rotation: 45,
    frameColor: 0xFFFF,
    fill: true,
    fillColor: 0x07E0,
    borderEnabled: true,
    size: 1
  };

  const snippetRot = codegen.staticDrawSnippet(rotatedEllipse);
  check(!snippetRot.includes('jwplcHMIDrawEllipse') && snippetRot.includes('jwplcHMIFillTriangle') && snippetRot.includes('tft.drawLine'),
    'Elipse rotada (rot=45) se descompone en jwplcHMIFillTriangle y tft.drawLine');
}

// -------------------------------------------------------------
// Test 2: Snippet técnico en app.js para elipses ortogonales asimétricas
// -------------------------------------------------------------
console.log('2. Snippet técnico de elipse asimétrica en app.js');
{
  // Test direct regex or function in app.js
  const appJs = fs.readFileSync(path.join(POC, 'app.js'), 'utf8');
  // Check that buildShapeCppSnippet does not unconditionally use fillCircle with max(rx, ry)
  const ellipseSnippetMatchesCircle = /if\s*\(field\.type === 'ELLIPSE'[\s\S]*?const rad = Math\.max\(rx, ry\);/.test(appJs);
  check(!ellipseSnippetMatchesCircle,
    'buildShapeCppSnippet no usa fillCircle con radio maximo para elipses asimetricas');
}

// -------------------------------------------------------------
// Test 3: Handles de LINE con grosor size > 1
// -------------------------------------------------------------
console.log('3. Handles de selección para LINE con grosor');
{
  const appJs = fs.readFileSync(path.join(POC, 'app.js'), 'utf8');

  // Verify that inside getSelectionHandles, the LINE block uses vertices directly without getFieldBounds
  const lineHandlesMatch = appJs.match(/if\s*\(f\.type === 'LINE'\)\s*\{([\s\S]*?id:\s*'p1'[\s\S]*?)\}/);
  const lineHandlesCode = lineHandlesMatch ? lineHandlesMatch[1] : '';
  const lineUsesFieldBounds = lineHandlesCode.includes('getFieldBounds');
  const lineUsesVertices = lineHandlesCode.includes('getShapeTransformedVertices');
  check(!lineUsesFieldBounds && lineUsesVertices,
    'getSelectionHandles usa getShapeTransformedVertices y no getFieldBounds(f) para posicionar handles');
}

if (failures > 0) {
  console.error(`\nFALLARON ${failures} PRUEBAS.`);
  process.exit(1);
} else {
  console.log('\nTodas las pruebas de figuras pasaron correctamente.');
}
