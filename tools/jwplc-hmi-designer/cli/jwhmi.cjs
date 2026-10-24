#!/usr/bin/env node
// jwhmi: crea, valida y genera proyectos del JWPLC HMI Designer sin la UI.
//
// Pensada para agentes (y para quien prefiera la terminal). Guía completa:
// tools/jwplc-hmi-designer/agent/GUIA_AGENTES.md
//
//   node tools/jwplc-hmi-designer/cli/jwhmi.cjs <comando> [opciones]
//
// Comandos:
//   new <archivo.json> [--name N]       proyecto de partida (forma corta)
//   describe [TIPO]                     propiedades de un tipo (o lista de tipos)
//   icons [categoría|texto]             catálogo de iconos para ICON (id, estados, tamaños)
//   normalize <proyecto> [-o salida]    forma corta -> .jwhmi completo (como lo guarda el Designer)
//   validate <proyecto> [--fast]        errores y avisos (--fast: sin Chromium, sin geometría)
//   generate <proyecto> [-o header.h]   JWPLC_HMI_Generated.h (stdout si no hay -o)
//   preview <proyecto> [-o carpeta] [--scale 2]   PNG de cada página como en la TFT
//   check <proyecto>                    validate + generate + compilación en host (g++) del sketch base
//   build <proyecto> --sketch <carpeta> [--create-ino] [--preview]
//                                       escribe en el sketch lo mismo que «Escribir en sketch»:
//                                       JWPLC_HMI_Generated.h, <sketch>.jwhmi y JWPLC_HMI_LEEME.md
//   schema                              regenera schema/jwhmi.schema.json y agent/REFERENCIA_JWHMI.md
//
// Opciones comunes: --json (salida para máquinas), --strict (los avisos cuentan como error).
// Código de salida: 0 bien, 1 errores de validación o compilación, 2 uso incorrecto o fallo interno.
'use strict';

const fs = require('fs');
const path = require('path');
const spec = require('./lib/spec.cjs');
const { normalizeProject } = require('./lib/normalize.cjs');
const { validateProject } = require('./lib/validate.cjs');

const DESIGNER = path.resolve(__dirname, '..');
const SCHEMA_FILE = path.join(DESIGNER, 'schema', 'jwhmi.schema.json');
const REFERENCE_FILE = path.join(DESIGNER, 'agent', 'REFERENCIA_JWHMI.md');
const HEADER_NAME = 'JWPLC_HMI_Generated.h';

// --- Argumentos ----------------------------------------------------------------

function parseArgs(argv) {
  const args = { _: [] };
  for (let i = 0; i < argv.length; i += 1) {
    const a = argv[i];
    if (a === '-o' || a === '--out') args.out = argv[++i];
    else if (a.startsWith('--')) {
      const [key, inline] = a.slice(2).split('=');
      const takesValue = ['sketch', 'scale', 'name'].includes(key);
      args[key] = takesValue ? (inline !== undefined ? inline : argv[++i]) : (inline !== undefined ? inline : true);
    } else args._.push(a);
  }
  return args;
}

class UsageError extends Error {}

function readProject(file) {
  if (!file) throw new UsageError('Falta el archivo del proyecto.');
  let text;
  try { text = fs.readFileSync(file, 'utf8'); } catch (_) { throw new UsageError(`No se puede leer ${file}.`); }
  try { return JSON.parse(text.replace(/^﻿/, '')); } catch (error) { throw new UsageError(`${file} no es JSON válido: ${error.message}`); }
}

// --- Salida --------------------------------------------------------------------

function printReport(report, args, extra = {}) {
  const ok = report.errors.length === 0 && !(args.strict && report.warnings.length);
  if (args.json) {
    process.stdout.write(`${JSON.stringify({ ok, errors: report.errors, warnings: report.warnings, ...extra }, null, 2)}\n`);
    return ok;
  }
  report.errors.forEach((e) => console.error(`ERROR  ${e.path}: ${e.message}`));
  report.warnings.forEach((w) => console.error(`AVISO  ${w.path}: ${w.message}`));
  console.error(`${ok ? 'OK' : 'FALLA'}: ${report.errors.length} error(es), ${report.warnings.length} aviso(s).`);
  return ok;
}

function log(args, message) {
  if (!args.json) console.error(message);
}

// --- Pasos comunes ---------------------------------------------------------------

async function withDesigner(fn) {
  const { DesignerSession } = require('./lib/designer.cjs');
  const session = await DesignerSession.open();
  try { return await fn(session); } finally { await session.close(); }
}

// normaliza + valida (node) + carga en el Designer y suma geometría e identificadores.
async function prepare(file, args, session) {
  const project = normalizeProject(readProject(file));
  const report = validateProject(project);
  if (session && report.errors.length === 0) {
    const { geometryWarnings } = require('./lib/designer.cjs');
    await session.load(project);
    const inspection = await session.inspect();
    inspection.issues.forEach((i) => report.errors.push({ path: 'fields', message: `${i.property === 'id' ? 'ID' : 'variable'} C++ duplicado: ${i.canonical}` }));
    inspection.issues = [];
    report.warnings.push(...geometryWarnings(inspection, project));
    if (session.pageErrors.length) report.warnings.push(...session.pageErrors.map((m) => ({ path: 'designer', message: `error de JavaScript en el Designer: ${m}` })));
  }
  return { project, report };
}

function failed(report, args) {
  return report.errors.length > 0 || Boolean(args.strict && report.warnings.length);
}

// --- Comandos ------------------------------------------------------------------

const STARTER = (name) => ({
  $schema: '../schema/jwhmi.schema.json',
  projectName: name,
  display: { start: 'HMI' },
  pages: [
    {
      name: 'Principal',
      fields: [
        { type: 'RECT', x: 0, y: 0, x2: 319, y2: 22, fill: true, fillColor: 'NAVY', frameColor: 'NAVY' },
        { type: 'RAW_TEXT', text: name, x: 6, y: 4, font: 'SANS_BOLD_9', textColor: 'WHITE' },
        { type: 'VALUE', id: 'FIELD_TEMP', variable: 'temperatura', label: 'Temp', unit: 'C', x: 6, y: 40, integerDigits: 3, decimalDigits: 1 },
        { type: 'BOOL', id: 'FIELD_BOMBA', variable: 'bomba', label: 'Bomba', x: 6, y: 80, falseText: 'PARADA', trueText: 'MARCHA' },
        { type: 'BAR', id: 'FIELD_NIVEL', variable: 'nivel', barLabel: 'Nivel', x: 170, y: 40, barWidth: 140 }
      ]
    }
  ]
});

const COMMANDS = {
  async new(args) {
    const file = args._[0];
    if (!file) throw new UsageError('Uso: new <archivo.json> [--name Nombre]');
    if (fs.existsSync(file) && !args.force) throw new UsageError(`${file} ya existe (usa --force para reemplazarlo).`);
    const name = String(args.name || path.basename(file).replace(/\.(jwhmi|json)$/i, '') || 'HMI').slice(0, spec.LIMITS.nameLength);
    const starter = STARTER(name);
    starter.$schema = path.relative(path.dirname(path.resolve(file)), SCHEMA_FILE).split(path.sep).join('/');
    fs.writeFileSync(file, `${JSON.stringify(starter, null, 2)}\n`);
    log(args, `Creado ${file}. Siguiente: validate ${file}`);
    return 0;
  },

  async icons(args) {
    const { catalog, listIcons } = require('./lib/icons.cjs');
    const query = args._[0] ? String(args._[0]).toLowerCase() : '';
    const list = listIcons().filter((i) => !query || i.category === query || `${i.id} ${i.name}`.toLowerCase().includes(query));
    if (args.json) {
      process.stdout.write(`${JSON.stringify({ categories: catalog.categories, icons: list }, null, 2)}\n`);
      return 0;
    }
    for (const cat of catalog.categories) {
      const items = list.filter((i) => i.category === cat.id);
      if (!items.length) continue;
      console.log(`${cat.name} (${cat.id})`);
      items.forEach((i) => console.log(`  ${i.id.padEnd(26)} ${i.sizes.join('/').padEnd(11)} ${i.stateNames.map((n, k) => `${k}=${n}`).join(' · ')}`));
    }
    if (!list.length) console.log(`Sin iconos para «${query}». Categorías: ${catalog.categories.map((c) => c.id).join(', ')}`);
    return 0;
  },

  async describe(args) {
    const type = args._[0] ? String(args._[0]).toUpperCase() : null;
    const tables = { PROJECT: spec.PROJECT, PAGE: spec.PAGE, DISPLAY: spec.DISPLAY, MENU_NODE: spec.MENU_NODE, PIXELMAP: spec.PIXELMAP };
    if (!type) {
      const list = Object.entries(spec.TYPES).map(([t, d]) => ({ type: t, desc: d.desc }));
      if (args.json) process.stdout.write(`${JSON.stringify({ types: list, sections: Object.keys(tables) }, null, 2)}\n`);
      else {
        list.forEach((t) => console.log(`${t.type.padEnd(9)} ${t.desc}`));
        console.log(`\nTambién: ${Object.keys(tables).join(', ')}. Uso: describe <TIPO>`);
      }
      return 0;
    }
    const props = spec.TYPES[type]?.props || tables[type];
    if (!props) throw new UsageError(`Tipo desconocido: ${type}. Tipos: ${Object.keys(spec.TYPES).join(', ')}, ${Object.keys(tables).join(', ')}`);
    const visible = Object.entries(props).filter(([, d]) => !d.internal || args.all);
    if (args.json) {
      process.stdout.write(`${JSON.stringify({ type, desc: spec.TYPES[type]?.desc, props: Object.fromEntries(visible) }, null, 2)}\n`);
      return 0;
    }
    if (spec.TYPES[type]) console.log(`${type}: ${spec.TYPES[type].desc}\n`);
    visible.forEach(([key, d]) => {
      const values = d.enum ? ` [${d.enum.map((v) => JSON.stringify(v)).join('|')}]` : (d.min !== undefined ? ` [${d.min}..${d.max}]` : '');
      const dflt = d.default !== undefined ? d.default : d.doc;
      console.log(`  ${(key + (d.required ? '*' : '')).padEnd(18)} ${d.kind}${values}${dflt !== undefined && dflt !== null ? ` = ${JSON.stringify(dflt)}` : ''}\n      ${d.desc}`);
    });
    console.log('\n* obligatoria');
    return 0;
  },

  async normalize(args) {
    const project = normalizeProject(readProject(args._[0]));
    const text = `${JSON.stringify(project, null, 2)}\n`;
    if (args.out) fs.writeFileSync(args.out, text);
    else process.stdout.write(text);
    const report = validateProject(project);
    if (!args.json) printReport(report, args);
    return failed(report, args) ? 1 : 0;
  },

  async validate(args) {
    const file = args._[0];
    const run = async (session) => prepare(file, args, session);
    const { report } = args.fast ? await run(null) : await withDesigner(run);
    return printReport(report, args) ? 0 : 1;
  },

  async generate(args) {
    return withDesigner(async (session) => {
      const { report } = await prepare(args._[0], args, session);
      if (failed(report, args)) { printReport(report, args); return 1; }
      const header = await session.header();
      if (args.out) fs.writeFileSync(args.out, header);
      else if (!args.json) process.stdout.write(header);
      if (args.json || args.out) printReport(report, args, args.out ? { files: [args.out] } : { header });
      else printReport({ errors: [], warnings: report.warnings }, { ...args, json: false });
      return 0;
    });
  },

  async preview(args) {
    const outDir = args.out || path.join(path.dirname(path.resolve(args._[0] || '.')), 'preview');
    return withDesigner(async (session) => {
      const { report } = await prepare(args._[0], args, session);
      if (failed(report, args)) { printReport(report, args); return 1; }
      const files = await writePreviews(session, outDir, Number(args.scale) || 2);
      log(args, files.map((f) => `PNG ${f}`).join('\n'));
      printReport(report, args, { files });
      return 0;
    });
  },

  async check(args) {
    const { hostCheck } = require('./lib/compile.cjs');
    return withDesigner(async (session) => {
      const { report } = await prepare(args._[0], args, session);
      if (failed(report, args)) { printReport(report, args); return 1; }
      const header = await session.header();
      const result = hostCheck(header);
      if (result.skipped) report.warnings.push({ path: 'check', message: `compilación omitida: ${result.skipped}` });
      else {
        if (!result.compiled) report.errors.push({ path: 'check', message: `el sketch base no compila contra el header:\n${result.log}` });
        if (result.missing.length) report.errors.push({ path: 'check', message: `llamadas que no existen en las librerías: ${result.missing.join(', ')}` });
        if (result.ok) log(args, 'Compilación en host (g++ -fsyntax-only): OK. La compilación ESP32 real y la prueba en la placa siguen pendientes.');
      }
      return printReport(report, args, { compiled: result.skipped ? null : result.compiled }) ? 0 : 1;
    });
  },

  async build(args) {
    if (!args.sketch || args.sketch === true) throw new UsageError('Uso: build <proyecto> --sketch <carpeta del sketch> [--create-ino] [--preview]');
    const sketchDir = path.resolve(args.sketch);
    const sketchName = path.basename(sketchDir);
    const inoPath = path.join(sketchDir, `${sketchName}.ino`);
    if (!fs.existsSync(sketchDir)) {
      if (!args['create-ino']) throw new UsageError(`No existe la carpeta ${sketchDir} (usa --create-ino para crear un sketch nuevo).`);
      fs.mkdirSync(sketchDir, { recursive: true });
    }
    if (!fs.existsSync(inoPath) && !args['create-ino']) {
      throw new UsageError(`${sketchDir} no tiene ${sketchName}.ino (la carpeta y el .ino deben llamarse igual). Usa --create-ino para crearlo con el sketch base.`);
    }
    return withDesigner(async (session) => {
      const { project, report } = await prepare(args._[0], args, session);
      if (failed(report, args)) { printReport(report, args); return 1; }
      const header = await session.header();
      const saved = await session.serialize();
      const guide = require(path.join(DESIGNER, 'poc', 'designer-sketch-guide.js'));
      const files = [];
      const write = (name, text) => { const p = path.join(sketchDir, name); fs.writeFileSync(p, text); files.push(p); };
      write(HEADER_NAME, header);
      write(`${sketchName}.jwhmi`, `${JSON.stringify({ ...saved, projectName: project.projectName }, null, 2)}\n`);
      const guidePath = path.join(sketchDir, guide.GUIDE_NAME);
      // Como el Designer: sólo reemplaza una guía suya.
      if (!fs.existsSync(guidePath) || fs.readFileSync(guidePath, 'utf8').startsWith(guide.GUIDE_MARK)) {
        write(guide.GUIDE_NAME, guide.build({ header, sketchName }));
      } else report.warnings.push({ path: 'build', message: `${guide.GUIDE_NAME} existe y no es del Designer: no se reemplazó` });
      if (!fs.existsSync(inoPath)) {
        // Sólo con --create-ino y si no existe: el .ino del usuario nunca se escribe.
        write(`${sketchName}.ino`, `${guide.baseSketch(header)}\n`);
      }
      if (args.preview) files.push(...await writePreviews(session, path.join(sketchDir, 'hmi_preview'), Number(args.scale) || 2));
      log(args, files.map((f) => `ESCRITO ${f}`).join('\n'));
      return printReport(report, args, { files }) ? 0 : 1;
    });
  },

  async schema(args) {
    const { buildSchema, buildReference } = require('./lib/schema.cjs');
    fs.mkdirSync(path.dirname(SCHEMA_FILE), { recursive: true });
    fs.mkdirSync(path.dirname(REFERENCE_FILE), { recursive: true });
    fs.writeFileSync(SCHEMA_FILE, `${JSON.stringify(buildSchema(), null, 2)}\n`);
    fs.writeFileSync(REFERENCE_FILE, buildReference());
    log(args, `ESCRITO ${path.relative(process.cwd(), SCHEMA_FILE)}\nESCRITO ${path.relative(process.cwd(), REFERENCE_FILE)}`);
    return 0;
  }
};

async function writePreviews(session, outDir, scale) {
  fs.mkdirSync(outDir, { recursive: true });
  const shots = await session.previews(scale);
  return shots.map((s) => {
    const name = `pagina_${s.id}_${String(s.name).normalize('NFD').replace(/[̀-ͯ]/g, '').replace(/[^A-Za-z0-9]+/g, '_').replace(/^_|_$/g, '') || 'sin_nombre'}.png`;
    const file = path.join(outDir, name);
    fs.writeFileSync(file, s.png);
    return file;
  });
}

async function main() {
  const [command, ...rest] = process.argv.slice(2);
  const args = parseArgs(rest);
  if (!command || command === 'help' || command === '--help' || !COMMANDS[command]) {
    const header = fs.readFileSync(__filename, 'utf8').split('\n').slice(1, 26).map((l) => l.replace(/^\/\/ ?/, '')).join('\n');
    console.log(header);
    return command && command !== 'help' && command !== '--help' ? 2 : 0;
  }
  return COMMANDS[command](args);
}

main().then((code) => process.exit(code), (error) => {
  if (error instanceof UsageError) console.error(error.message);
  else console.error(error && error.stack ? error.stack : error);
  process.exit(2);
});
