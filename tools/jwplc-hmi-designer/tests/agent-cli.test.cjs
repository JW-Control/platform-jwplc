#!/usr/bin/env node
// CLI para agentes (cli/jwhmi.cjs): proyectos .jwhmi sin la UI.
//
// 1. Los valores por defecto de cli/lib/spec.cjs son los que crea el Designer.
// 2. schema/jwhmi.schema.json y agent/REFERENCIA_JWHMI.md están al día con la spec.
// 3. Normalización: forma corta (páginas como texto, campos anidados, colores
//    con nombre o #RRGGBB, nodos de MENU con children) -> proyecto completo.
// 4. El validador detecta los errores típicos y sugiere la corrección.
// 5. Paridad: cada fixture normalizado genera el mismo header que el golden.
// 6. Ejemplos de examples/: sin errores ni avisos, el header compila (g++) y lo
//    que guarda el Designer vuelve a validar sin avisos.
// 7. `build` escribe header, .jwhmi y guía en un sketch y nunca pisa el .ino.
//
// Requiere: Playwright con Chromium (g++ opcional).
'use strict';

const fs = require('fs');
const os = require('os');
const path = require('path');
const { spawnSync } = require('child_process');

const DESIGNER = path.resolve(__dirname, '..');
const CLI = path.join(DESIGNER, 'cli', 'jwhmi.cjs');
const spec = require('../cli/lib/spec.cjs');
const { normalizeProject, parseColor } = require('../cli/lib/normalize.cjs');
const { validateProject } = require('../cli/lib/validate.cjs');
const { buildSchema, buildReference } = require('../cli/lib/schema.cjs');
const { DesignerSession, geometryWarnings } = require('../cli/lib/designer.cjs');
const { hostCheck } = require('../cli/lib/compile.cjs');
const FIXTURES = require('./fixtures.cjs');

let failures = 0;
function check(condition, message) {
  console.log(`  ${condition ? 'PASS' : 'FAIL'} ${message}`);
  if (!condition) failures += 1;
}
const has = (report, re, kind = 'errors') => report[kind].some((e) => re.test(`${e.path}: ${e.message}`));

function cli(args, options = {}) {
  return spawnSync(process.execPath, [CLI, ...args], { encoding: 'utf8', ...options });
}

async function main() {
  const session = await DesignerSession.open();
  try {
    console.log('1. Valores por defecto == Designer');
    // Propiedades de contenido/identidad: el Designer pone ejemplos, la CLI valores neutros.
    const EXEMPT = new Set(['key', 'name', 'id', 'variable', 'label', 'unit', 'preview', 'x', 'y', 'page', 'layerOrder', 'text', 'textColor', 'backgroundColor', 'height', 'iconId', 'iconVarType']);
    const created = await session.page.evaluate(async () => {
      const E = window.JWPLCHMIEditor;
      const r = {};
      E.addTextField(); r.TEXT = { ...E.getSelectedField() };
      E.addValueField(); r.VALUE = { ...E.getSelectedField() };
      window.JWPLCHMIBool.addBoolField(); await new Promise((x) => setTimeout(x, 80)); r.BOOL = { ...E.getSelectedField() };
      window.JWPLCHMIBar.addBarField(); await new Promise((x) => setTimeout(x, 80)); r.BAR = { ...E.getSelectedField() };
      r.ICON = { ...E.addIconField('pilotos/piloto', 32) };
      r.RAW_TEXT = { ...E.addRawTextField() };
      r.MENU = window.JWPLCHMIMenu.createDefault('m', 1, []);
      r.DISPLAY = window.JWPLCHMIIdle.defaults();
      return r;
    });
    for (const type of ['TEXT', 'VALUE', 'BOOL', 'BAR', 'ICON', 'RAW_TEXT', 'MENU']) {
      const props = spec.TYPES[type].props;
      const diffs = Object.entries(created[type])
        .filter(([key]) => !EXEMPT.has(key) && !['nodes', 'colors'].includes(key))
        .filter(([key, value]) => !props[key] || (props[key].default !== undefined && JSON.stringify(props[key].default) !== JSON.stringify(value)))
        .map(([key, value]) => `${key}: Designer ${JSON.stringify(value)} / spec ${props[key] ? JSON.stringify(props[key].default) : '(no está)'}`);
      check(diffs.length === 0, `${type}: cada propiedad del Designer está en la spec con el mismo valor por defecto${diffs.length ? `\n      ${diffs.join('\n      ')}` : ''}`);
    }
    check(JSON.stringify(created.MENU.colors) === JSON.stringify(spec.MENU_COLORS), 'MENU: colores por defecto');
    const displayDefaults = Object.fromEntries(Object.entries(spec.DISPLAY).filter(([, d]) => d.default !== undefined).map(([k, d]) => [k, d.default]));
    check(JSON.stringify(created.DISPLAY) === JSON.stringify(displayDefaults), 'display (IDLE): valores por defecto');

    console.log('2. Esquema y referencia al día');
    const schemaFile = path.join(DESIGNER, 'schema', 'jwhmi.schema.json');
    const refFile = path.join(DESIGNER, 'agent', 'REFERENCIA_JWHMI.md');
    check(fs.existsSync(schemaFile) && fs.readFileSync(schemaFile, 'utf8') === `${JSON.stringify(buildSchema(), null, 2)}\n`, 'schema/jwhmi.schema.json == spec (si falla: node cli/jwhmi.cjs schema)');
    check(fs.existsSync(refFile) && fs.readFileSync(refFile, 'utf8') === buildReference(), 'agent/REFERENCIA_JWHMI.md == spec');

    console.log('3. Normalización de la forma corta');
    check(parseColor('#FF0000') === 0xF800 && parseColor('TFT_RED') === 0xF800 && parseColor('0x07e0') === 0x07E0 && parseColor('orange') === 0xFD20, 'colores: #RRGGBB, TFT_*, 0x.., nombre');
    const short = normalizeProject({
      pages: ['Uno', { name: 'Dos', fields: [{ type: 'value', x: 4, y: 4 }] }],
      fields: [
        { type: 'TEXT', x: 4, y: 4, labelColor: 'YELLOW' },
        { type: 'MENU', x: 0, y: 0, nodes: [{ type: 'SUBMENU', label: 'A', children: [{ type: 'EDIT_NUM', label: 'B', variable: 'b', valueType: 'int32' }] }] }
      ]
    }, { savedAt: 'x' });
    check(short.format === 'JWPLC_HMI_PROJECT' && short.version === 1 && short.pages.map((p) => p.id).join() === '0,1', 'format/version y páginas como texto');
    const v = short.fields.find((f) => f.type === 'VALUE');
    check(v && v.page === 1 && v.id === 'FIELD_VALUE_3' && v.variable === 'value_3' && v.integerDigits === 3, 'campo anidado en la página: page, id, variable y valores por defecto');
    check(short.fields[0].labelColor === 0xFFE0, 'color con nombre convertido');
    const menu = short.fields.find((f) => f.type === 'MENU');
    check(menu.nodes.length === 2 && menu.nodes[1].parent === menu.nodes[0].key && menu.nodes[1].decimals === 0, 'MENU: children aplanados con parent');
    check(short.fields[0].layerOrder > short.fields[1].layerOrder, 'layerOrder por orden del arreglo (el último queda encima)');
    check(validateProject(short).errors.length === 0, 'la forma corta valida sin errores');
    const ic = normalizeProject({ pages: ['P'], fields: [
      { type: 'icon', iconId: 'pilotos/piloto', x: 4, y: 4, iconSize: 44 },
      { type: 'ICON', iconId: 'alarmas/alarma', x: 60, y: 4 }
    ] }).fields;
    check(ic[0].iconSize === 48 && ic[0].iconVarType === 'uint8' && ic[0].variable === 'icon_1' && ic[0].layout === 'STACKED', 'ICON: tamaño más cercano, uint8 con 4 estados, variable y layout por defecto');
    check(ic[1].iconSize === 32 && ic[1].iconVarType === 'bool', 'ICON de 2 estados: bool y 32 px');

    console.log('4. Errores típicos');
    const bad = validateProject(normalizeProject({
      pages: ['P'],
      fields: [
        { type: 'TEXT', id: 'FIELD_A', x: 4, y: 4, align: 'MIDDLE', layout: 'INLNE', labelColour: 'RED' },
        { type: 'VALUE', id: 'FIELD_A', variable: 'a', x: 4, y: 40, page: 3 },
        { type: 'BOOL', id: 'FIELD_B', variable: 'a', x: 400, y: 4, valueColor: 'rojo' },
        { type: 'CIRCLE', x: 1, y: 1 },
        { type: 'MENU', x: 0, y: 0, nodes: [{ type: 'EDIT_BOOL', label: 'X', variable: 'x', valueType: 'float' }] },
        { type: 'ICON', id: 'ICON_A', x: 4, y: 100, iconId: 'motor' },
        { type: 'ICON', id: 'ICON_B', x: 60, y: 100, iconId: 'pilotos/piloto', iconVarType: 'bool', iconState: 5 },
        { type: 'IMAGE', x: 100, y: 100 }
      ]
    }));
    check(has(bad, /align: valor "MIDDLE" no válido.*\(¿CENTER\?\)/), 'enum inválido con sugerencia por sinónimo (MIDDLE -> CENTER)');
    check(has(bad, /layout: valor "INLNE" no válido.*\(¿INLINE\?\)/), 'enum mal escrito con sugerencia');
    check(has(bad, /labelColour.*¿quisiste decir «labelColor»/, 'warnings'), 'propiedad mal escrita: aviso con sugerencia');
    check(has(bad, /el id FIELD_A ya lo usa/), 'id duplicado');
    check(has(bad, /la página 3 no existe/), 'página inexistente');
    check(has(bad, /la variable a ya la usa fields\[1\]/), 'variable repetida entre campos (como validateIdentifiers del Designer)');
    check(has(bad, /\.x: máximo 319/), 'coordenada fuera de rango');
    check(has(bad, /color no válido "rojo"/), 'color inválido');
    check(has(bad, /tipo "CIRCLE" desconocido/), 'tipo desconocido');
    check(has(bad, /tipo float no válido para EDIT_BOOL/), 'MENU: reglas del Designer');
    check(has(bad, /ICON_A\)\.iconId: icono "motor" no está en el catálogo \(¿motores\/motor\?\)/), 'ICON: icono desconocido con sugerencia por nombre');
    check(has(bad, /ICON_B\)\.iconVarType:.*usa "uint8"/, 'warnings') && has(bad, /ICON_B\)\.iconState/, 'warnings'), 'ICON: bool con 4 estados y estado fuera de rango (avisos)');
    check(has(bad, /tipo "IMAGE" desconocido.*\(¿ICON\?\)/), 'ICON: sinónimo IMAGE -> ICON');

    const geo = normalizeProject({ pages: ['P'], fields: [
      { type: 'VALUE', id: 'FIELD_X', variable: 'x', x: 280, y: 4, valueSize: 3 },
      { type: 'TEXT', id: 'FIELD_Y', variable: 'y', x: 150, y: 4, capacity: 20 }
    ] });
    await session.load(geo);
    const gw = geometryWarnings(await session.inspect(), geo);
    check(gw.some((w) => /FIELD_X.*sale de la pantalla/.test(`${w.path} ${w.message}`)), 'geometría: campo que sale de la pantalla');
    check(gw.some((w) => /se pisa con/.test(w.message)), 'geometría: campos que se pisan');

    console.log('5. Paridad con los golden');
    for (const [name, project] of Object.entries(FIXTURES)) {
      await session.load(normalizeProject(project));
      const header = await session.header();
      const golden = fs.readFileSync(path.join(__dirname, 'golden', `${name}.h`), 'utf8').replace(/\r\n/g, '\n');
      check(header === (golden.endsWith('\n') ? golden : `${golden}\n`), `${name}: header de la CLI == golden`);
    }

    console.log('6. Ejemplos');
    const examples = fs.readdirSync(path.join(DESIGNER, 'examples')).filter((f) => f.endsWith('.json')).sort();
    check(examples.length >= 6, `${examples.length} ejemplos`);
    for (const file of examples) {
      const project = normalizeProject(JSON.parse(fs.readFileSync(path.join(DESIGNER, 'examples', file), 'utf8')));
      const report = validateProject(project);
      await session.load(project);
      const inspection = await session.inspect();
      report.warnings.push(...geometryWarnings(inspection, project));
      check(report.errors.length === 0 && report.warnings.length === 0, `${file}: sin errores ni avisos${report.errors.concat(report.warnings).map((e) => `\n      ${e.path}: ${e.message}`).join('')}`);
      const header = await session.header();
      const result = hostCheck(header);
      if (result.skipped) console.log(`  (${file}: compilación omitida, ${result.skipped})`);
      else check(result.ok, `${file}: el sketch base compila contra el header${result.ok ? '' : `\n${result.log}\n${result.missing.join(', ')}`}`);
      const saved = normalizeProject(await session.serialize());
      const again = validateProject(saved);
      check(again.errors.length === 0 && again.warnings.length === 0, `${file}: lo que guarda el Designer vuelve a validar sin avisos${again.errors.concat(again.warnings).map((e) => `\n      ${e.path}: ${e.message}`).join('')}`);
    }
  } finally {
    await session.close();
  }

  console.log('7. CLI de punta a punta');
  const tmp = fs.mkdtempSync(path.join(os.tmpdir(), 'jwhmi-cli-'));
  const sketch = path.join(tmp, 'Bombeo');
  const example = path.join(DESIGNER, 'examples', '05_estacion_bombeo.json');
  let run = cli(['build', example, '--sketch', sketch, '--create-ino', '--json']);
  let out = run.status === 0 ? JSON.parse(run.stdout) : null;
  check(run.status === 0 && out.ok, `build --create-ino termina bien${run.status === 0 ? '' : `\n${run.stderr}${run.stdout}`}`);
  const names = ['JWPLC_HMI_Generated.h', 'Bombeo.jwhmi', 'JWPLC_HMI_LEEME.md', 'Bombeo.ino'];
  check(names.every((n) => fs.existsSync(path.join(sketch, n))), `escribe ${names.join(', ')}`);
  fs.writeFileSync(path.join(sketch, 'Bombeo.ino'), '// mío\n');
  run = cli(['build', example, '--sketch', sketch, '--create-ino']);
  check(run.status === 0 && fs.readFileSync(path.join(sketch, 'Bombeo.ino'), 'utf8') === '// mío\n', 'un segundo build no toca el .ino del usuario');
  run = cli(['generate', path.join(sketch, 'Bombeo.jwhmi')]);
  check(run.status === 0 && run.stdout === fs.readFileSync(path.join(sketch, 'JWPLC_HMI_Generated.h'), 'utf8'), 'el .jwhmi escrito regenera el mismo header');
  const broken = path.join(tmp, 'roto.json');
  fs.writeFileSync(broken, JSON.stringify({ pages: ['P'], fields: [{ type: 'TEXT', x: 4, y: 4, layout: 'ROW' }] }));
  run = cli(['validate', broken, '--fast', '--json']);
  out = JSON.parse(run.stdout || '{}');
  check(run.status === 1 && out.ok === false && out.errors.some((e) => /layout/.test(e.path)), 'validate devuelve 1 y el error en JSON');
  run = cli(['preview', example, '-o', path.join(tmp, 'png')]);
  const pngs = fs.existsSync(path.join(tmp, 'png')) ? fs.readdirSync(path.join(tmp, 'png')) : [];
  check(run.status === 0 && pngs.length === 3 && pngs.every((f) => fs.readFileSync(path.join(tmp, 'png', f)).slice(1, 4).toString() === 'PNG'), `preview: un PNG por página (${pngs.join(', ')})`);
  run = cli(['new', path.join(tmp, 'nuevo.json')]);
  check(run.status === 0 && cli(['validate', path.join(tmp, 'nuevo.json'), '--fast']).status === 0, 'new crea un proyecto que valida');
  fs.rmSync(tmp, { recursive: true, force: true });

  console.log(failures ? `\n${failures} verificación(es) fallaron.` : '\nTodas las verificaciones pasaron.');
  process.exit(failures ? 1 : 0);
}

main().catch((error) => { console.error(error); process.exit(1); });
